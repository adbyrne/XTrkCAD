/** \file clcc.c
 * LCC controls
 */

/* -*- C -*- ****************************************************************
 *
 *  System        :
 *  Module        :
 *  Object Name   : $RCSfile$
 *  Revision      : $Revision$
 *  Date          : $Date$
 *  Author        : $Author$
 *  Created By    : Philip Cameron
 *  Created       : Thu Mar 11 18:34:45 2021
 *  Last Modified :
 *
 *  Description
 *    Support for LCC - Layout Command Control.
 *    Provides events and consumer/producer mechanism
 *
 *  Notes
 *    See: https://www.nmra.org/lcc
 *         https://openlcb.org/
 *    At some point this should interface with a transport mechanism
 *    and permit monitoring and control of a layout.
 *
 *  History
 *
 ****************************************************************************
 *
 *    Copyright (C) 2021  Philip Cameron
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *
 ****************************************************************************
 *
 * When in OpenLCBmode the following event behavior is observed.
 *
 * Lcc uses events, 8 byte unique numbers, to manage operation. When something
 * happens an event is produced and anyone waiting for the thing to happen
 * consumes the event. This just requires that both the consumers and producer
 * use the same number.
 *
 * Nodes are typically signal controllers, blocks occupancy sensors, and
 * switch motor controllers. Each has a collection of events that it produces
 * when state changes and events that it consumes that may cause state changes.
 * The events are grouped together so that one event in the group is active
 * at a time. This is a "redio buton" behavior. The group is organized into
 * an event block.
 *
 * All on the nodes are on a network and each can broadcast events to all the
 * other nodes. Nodes that are interested in the event consume it. The rest
 * of the nodes just ignore it.
 *
 * An Lcc event is a unique 8 byte (long long) number. The event must be
 * unique among all events. The event is stored as a text string and as a
 * long long.
 *
 * The acutal details of an event are not important. However,
 * some structure is helpful in debugging the control system and for that
 * reason the event is encoded as follows:
 *
 * EventID - 8 bytes  xx.xx.xx.xx.xx.xx.xx.xx
 * Each device, node, has a predefined 6 byte ID the remaining 2 bytes are a
 * locally created event number. From the standard:
 *
 * manfID    manf SN   Node evt
 * xx.xx.xx  xx.xx.xx  xx.xx
 *
 * e.g.,
 * Tower-LCC
 * 02.01.57.00.01.69
 *
 * From the --  Layout Command Control - Unique Identifiers - S-9.7.0.3
 * byte 1,2 - 02.01 - Manufacturer Specific
 * byte 3 - 0d - Do-It-Yourself
 * byte 4,5,6 - Self assigned
 *
 * The xtrkcad created events are "02.01.0D" - DIY. The created numbers can be
 * edited to any valid format event.
 *
 * manfID: bytes 1, 2, 3 - "02.01.0D"
 * manfSN: byte 4 is for the node type: 1 - block T_BLOCK,
 *                                      2 - signal T_SIGNAL,
 *                                      3 - switchmotor T_SWITCHMOTOR
 *         byte 5 and 6 are the index of the track segment.
 * nodeEvt: sequence of events 1, 2, 3, ...
 *
 * ---
 * At some point this will be expanded to monitor real hardware on the layout
 * and show what is going on on the layout directly on the xtrkcad window.
 * The manfID is for DIY internal self contined layouts "02.01.0D"
 * ---
 *
 * Each node has a set of events that it can produce. It also may have
 * a set of events that it can consume. For example, a signal will be
 * interested in whether or not the next block is occupied. So the
 * signal will consume occupancy events from the block. When the block
 * becomes occupied it produces the "occupied" event. The signal along
 * with any other device that is interested, consumes the event and
 * changes it state and possibly generates an event for the change.
 *
 * The signal knows which block it is interested in but it doesn't know
 * the event number that is will consume. Initialization done when
 * entering train mode uses the lcc init mechanism to get the
 * actual event numbers for the block.
 *
 * For each node, the events that it can produce are collected into
 * a struct. During initialization the struct is provided for nodes that
 * need to consume events that are in it. See "struct evtBlk".
 * The event block keeps track of the most recently received or produced
 * events. This is modeled on a "radio buttor"/
 *
 *
 * ----
 * In all cases any events read from a .xtc file are used as is. Any missing
 * events are created as needed.
 ****************************************************************************/

static const char rcsid[] = "@(#) : $Id$";


#include "compound.h"
#include "cselect.h"
#include "cundo.h"
#include "custom.h"
#include "fileio.h"
#include "layout.h"
#include "param.h"
#include "track.h"
#include "trackx.h"
#include "common-ui.h"
#include "clcc.h"
#ifdef UTFCONVERT
#include "include/utf8convert.h"
#endif // UTFCONVERT


static int log_lcc = 0;

// generate and verify LCC events.
EXPORT BOOL_T OpenLCBmode = TRUE;

event_t evt_xtrkcad_root = EVT_XTRKCAD_ROOT;

 /* set up all of the control elements */
EXPORT void SetupControlElements( void )
{
	// Go through all "Blocks" and refresh the segments in the block
        UpdateBlockTrack();

	// setup turnouts
        SetupTurnouts(); // call SetupEpPaths on each turnout
}


/* node assignment */

static event_t makeNodeId( event_t root, event_t base, int seq )
{
	return root | base | (seq++) << 16;
}


/***         block            ***/

/* Create a new initialized event block */

EXPORT evtBlk_p NewBlockEvt( track_p block, char *name )
{
	evtBlk_p evtBlk;
	event_t base;
	int nodeId = 0;

	if ( block && GetTrkType( block ) != T_BLOCK ) return NULL;
	if ( block ) nodeId = GetTrkIndex( block );
	base = makeNodeId( EVT_XTRKCAD_ROOT,  EVT_BLOCK_BASE, nodeId );

	evtBlk = MyMalloc( sizeof (evtBlk_t) + sizeof( evtId_t ) * 3 );

	evtBlk->owner = block;
	if ( name )
		evtBlk->ownerName = MyStrdup ( name );
	evtBlk->numEvt = evtBlk->state = 2;

	evtBlk->eId[0].name = MyStrdup( "unoccupied" );
	evtBlk->eId[1].name = MyStrdup( "occupied" );
	if ( GetLayoutOpenLCBmode() ) {
		evtBlk->eId[0].evt = base | 1;
		evtBlk->eId[0].script = EventToAscii( evtBlk->eId[0].evt );
		evtBlk->eId[1].evt = base | 2;
		evtBlk->eId[1].script = EventToAscii( evtBlk->eId[1].evt );
	} else {
		evtBlk->eId[0].script = MyStrdup( "" );
		evtBlk->eId[1].script = MyStrdup( "" );
	}

	DisplayEvtBlk( "BLOCK", evtBlk );

	return evtBlk;
}

/***         signal           ***/

// Signals, by default, have a simplified set of Aspects, Off, Red,
// Yellow, Green. There is an event for each aspect.
// When numEvts is not 0 the space is allocated but not initialized.
EXPORT evtBlk_p NewSignalEvt( track_p signal, char * name, int numEvts )
{
	evtBlk_p evtBlk;
	event_t base;
	int i;

	base = makeNodeId( EVT_XTRKCAD_ROOT,  EVT_SIG_BASE, GetTrkIndex( signal ) );

	if ( numEvts == 0 ) i = 3;
	else i = numEvts - 1;
	evtBlk = MyMalloc( sizeof (evtBlk_t) + sizeof( evtId_t ) * i );

	evtBlk->owner = signal;
	evtBlk->ownerName = MyStrdup ( name );
	evtBlk->numEvt = i + 1;

	if ( numEvts == 0  && GetLayoutOpenLCBmode() ) {
		evtBlk->eId[0].evt = base | 1;
		evtBlk->eId[0].name = MyStrdup( "OFF" );
		evtBlk->eId[0].script = EventToAscii( evtBlk->eId[0].evt );
		evtBlk->eId[1].evt = base | 2;
		evtBlk->eId[1].name = MyStrdup( "RED" );
		evtBlk->eId[1].script = EventToAscii( evtBlk->eId[1].evt );
		evtBlk->eId[2].evt = base | 3;
		evtBlk->eId[2].name = MyStrdup( "YELLOW" );
		evtBlk->eId[2].script = EventToAscii( evtBlk->eId[2].evt );
		evtBlk->eId[3].evt = base | 4;
		evtBlk->eId[3].name = MyStrdup( "GREEN" );
		evtBlk->eId[3].script = EventToAscii( evtBlk->eId[3].evt );

		evtBlk->state = 1;
	} else {
		for ( i = 0; i < evtBlk->numEvt; i++ ) {
			evtBlk->eId[i].name = MyStrdup( "" );
			evtBlk->eId[i].script = MyStrdup( "" );
		}
		evtBlk->state = 0;
	}


	DisplayEvtBlk( "SIG", evtBlk );

	return evtBlk;
}

EXPORT int GetAspect( evtBlk_p eb )
{
	return eb->state == eb->numEvt?0:eb->state;

}

EXPORT void SetAspect( evtBlk_p eb, int aspect )
{
	// if aspect != state, produce active
	eb->state = aspect;
}

/***         switch motor     ***/

// The switch motor controls the position of the turnout. There is one
// event for each position the turnout can enter. These are organized into
// an event block. In addition the switch position can be locked. Locked and
// unlocked are organized into another event block. Lock is produced whenever the
// turnout becomes occupied.
// num != 0 when reading the .xtc file. The space is allocated for "num" positions.
// Otherwise, the position names are taken from the turnout paths and events are defaulted.
EXPORT evtBlk_p NewSwmotorPosEvt( track_p swmotor, track_p turnout, char *name, int num )
{
	evtBlk_p evtBlk;
	event_t base;
	int pos, numPos = num;

	if ( GetTrkType( swmotor ) != T_SWITCHMOTOR || GetTrkType( turnout ) != T_TURNOUT ||
		GetTrkEndPtCnt( turnout ) < 3 ) return NULL;
	base = makeNodeId( EVT_XTRKCAD_ROOT,  EVT_SWMOTOR_BASE, GetTrkIndex( swmotor ) );

	if ( numPos == 0 ) numPos = GetNumPos( turnout );
	evtBlk = MyMalloc( sizeof (evtBlk_t) + sizeof( evtId_t ) * numPos );

	evtBlk->state = evtBlk->numEvt = numPos;
	evtBlk->owner = swmotor;
	evtBlk->ownerName = MyMalloc ( strlen(name) + 3 );
	strcpy( evtBlk->ownerName, name );
	strcat( evtBlk->ownerName, "-P" );

	evtBlk->state = GetPosNames( turnout, evtBlk->eId );

	if ( GetLayoutOpenLCBmode() ) {

		for ( pos = 0; pos < numPos; pos++ ) {
			evtBlk->eId[pos].evt = base | pos + 3;
			evtBlk->eId[pos].script = EventToAscii( evtBlk->eId[pos].evt );
		}
	} else {
		for ( pos = 0; pos < numPos; pos++ ) {
			evtBlk->eId[pos].script = MyStrdup( "" );
		}
	}

	DisplayEvtBlk( "SWMOT-P", evtBlk );

	return evtBlk;
}

EXPORT evtBlk_p NewSwmotorLockEvt( track_p swmotor, char * name )
{
	evtBlk_p evtBlk;
	event_t base;

	if ( GetTrkType( swmotor ) != T_SWITCHMOTOR && GetTrkType( swmotor ) != T_TURNOUT ) return NULL;
	base = makeNodeId( EVT_XTRKCAD_ROOT,  EVT_SWMOTOR_BASE, GetTrkIndex( swmotor ) );

	evtBlk = MyMalloc( sizeof (evtBlk_t) + sizeof( evtId_t ) * 1 );

	evtBlk->owner = swmotor;
	evtBlk->numEvt = 2;
	evtBlk->state = 0;

	evtBlk->ownerName = MyMalloc ( strlen(name) + 3 );
	strcpy( evtBlk->ownerName, name );
	strcat( evtBlk->ownerName, "-L" );

	evtBlk->eId[0].name = MyStrdup( "unlocked" );
	evtBlk->eId[1].name = MyStrdup( "locked" );
	if ( GetLayoutOpenLCBmode() ) {
		evtBlk->eId[0].evt = base | 1;
		evtBlk->eId[0].script = EventToAscii( evtBlk->eId[0].evt );
		evtBlk->eId[1].evt = base | 2;
		evtBlk->eId[1].script = EventToAscii( evtBlk->eId[1].evt );
	} else {
		evtBlk->eId[0].script = MyStrdup( "" );
		evtBlk->eId[1].script = MyStrdup( "" );
	}

	DisplayEvtBlk( "SWMOT-L", evtBlk );

	return evtBlk;
}

/**** utility functions ****/

EXPORT void UpdateEvent( evtId_t *eId, char *name, char *script )
{
	if ( ! eId ) return;
	if ( name ) {
		if ( eId->name ) MyFree( eId->name );
		eId->name = MyStrdup( name );
	}
	if ( script ) {
		if ( eId->script ) MyFree( eId->script );
		eId->script = MyStrdup( script );
		if ( GetLayoutOpenLCBmode() ) {
			if ( ! VerifyAsciiEvent( script ) )
				NoticeMessage( MSG_LCC_FORMAT_ERROR, _("Ok"), NULL);
			eId->evt = AsciiToEvent( script );
		}
	}
}

EXPORT void UpdateOwnerName( evtBlk_p eb, char *name )
{
	if ( ! eb ) return;
	if ( name ) {
		if ( eb->ownerName ) MyFree( eb->ownerName );
		eb->ownerName = MyStrdup( name );
	}
}

EXPORT void DeleteEvtBlk( evtBlk_p eb )
{
	int i;

	if ( ! eb ) return;
	if ( eb->ownerName ) MyFree( eb->ownerName );
	for ( i = 0; i < eb->numEvt; i++ ) {
		if ( eb->eId[i].name ) MyFree( eb->eId[i].name );
		if ( eb->eId[i].script ) MyFree( eb->eId[i].script );
	}
	MyFree( eb );
}


EXPORT void DisplayEvtBlk( char *msg, evtBlk_p eb )
{
	int i;

	if ( ! eb ) return;

	LOG(log_lcc, 1, ("DisplayEvtBlk %s: T%d %s -- %s\n", msg,
	      	eb->owner?GetTrkIndex(eb->owner):0,eb->ownerName?eb->ownerName:"",
		eb->owner?GetTrkTypeName(eb->owner):0))

	for( i = 0; i < eb->numEvt; i++ ) {
		LOG(log_lcc, 1, ("DisplayEvtBlk T%d  -- %s %s %s\n",
			eb->owner?GetTrkIndex(eb->owner):0, i == eb->state?">>":"  ",
			eb->eId[i].script?eb->eId[i].script:"",
			eb->eId[i].name?eb->eId[i].name:""))
	}
}

// convert a long long event into ascii xx.xx.xx.xx.xx.xx.xx.xx
// returns the MyMalloc buffer, must be freed by caller
EXPORT char * EventToAscii( event_t evt )
{
	event_t *evtP = &evt;
	char buf[STR_SHORT_SIZE];
	char * aEvt = (char *)evtP, * out;

	sprintf(buf, "%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x",
		0xff&aEvt[7],0xff&aEvt[6],0xff&aEvt[5],0xff&aEvt[4],
		0xff&aEvt[3],0xff&aEvt[2],0xff&aEvt[1],0xff&aEvt[0]);

	return MyStrdup( buf );
}

// convert an event in ascii xx.xx.xx.xx.xx.xx.xx.xx to long long
EXPORT event_t AsciiToEvent( char * aEvt )
{
	event_t evt;
	unsigned long tmp;
	char buf[100], * op, *ip;
	int cnt = 0;

	ip = aEvt;
	op = buf;
	memset( buf, 0, sizeof(buf));

	while ( *ip && cnt < 12 ) {
		if ( *ip != '.' ) *op++ = *ip++;
		else ip++;
		cnt++;
	}

	sscanf( buf, "%x", &tmp );
	evt = (tmp & 0xffffffffUL)<<32;

	op = buf;
	cnt = 0;
	while ( *ip && cnt < 12 ) {
		if ( *ip != '.' ) *op++ = *ip++;
		else ip++;
		cnt++;
	}

	sscanf( buf, "%x", &tmp );
	evt |= (tmp & 0xffffffffUL);

	return evt;
}

// ascii event must be in this format xx.xx.xx.xx.xx.xx.xx.xx
EXPORT BOOL_T VerifyAsciiEvent( const char * ci )
{
	int cnt = 1;
	char *c, *cAlloc;

	if ( ! GetLayoutOpenLCBmode() ) TRUE;

	cAlloc = c = MyStrdup( ci );

	while ( *c && cnt < 30 ) {
		if ( *c == '.' ) {
		       if ( ( cnt % 3 ) != 0 ) {
				return FALSE;
		       }
		} else if (!((*c>='0'&&*c<='9')||(*c>='a'&&*c<='f')||(*c>='A'&&*c<='F'))) {
			return FALSE;
		}
		c++;
		cnt++;
	}
	if ( cnt != 24 ) return FALSE;

	MyFree( cAlloc );

	return TRUE;
}

EXPORT void LccTest( void )
{
#if 0
	char *buf, evtBuf[STR_SHORT_SIZE];
	event_t evt;
	BOOL_T rtn;

	return;
	LOG(log_lcc, 1, ("LccTest() \n"))

	buf = EventToAscii( evtBuf, EVT_EMERGENCY_OFF );
	LOG(log_lcc, 1, ("EVT_EMERGENCY_OFF %s\n", buf ))

	evt = AsciiToEvent( buf );
	LOG(log_lcc, 1, ("EVT_EMERGENCY_OFF %08x%08x\n", (evt>>32)&0xffffffff, evt&0xffffffff ))

	rtn = VerifyAsciiEvent( buf );
	LOG(log_lcc, 1, ("verify %s - %d\n", buf, rtn))
	rtn = VerifyAsciiEvent( "df.ce.ab.01.23.45.67.89" );
	LOG(log_lcc, 1, ("verify df.ce.ab.01.23.45.67.89 - %d\n", rtn))
	rtn = VerifyAsciiEvent( "DF.CE.AB.01.23.45.67.89" );
	LOG(log_lcc, 1, ("verify DF.CE.AB.01.23.45.67.89 - %d\n", rtn))
	rtn = VerifyAsciiEvent( "DF.CE.AB.01.23.45.67" );
	LOG(log_lcc, 1, ("verify DF.CE.AB.01.23.45.67 - %d\n", rtn))
	rtn = VerifyAsciiEvent( "DFC.E.AB.01.23.45.67.89" );
	LOG(log_lcc, 1, ("verify DFC.E.AB.01.23.45.67.89 - %d\n", rtn))
	rtn = VerifyAsciiEvent( "df.ce.ab.01.23.45.67.89." );
	LOG(log_lcc, 1, ("verify df.ce.ab.01.23.45.67.89. - %d\n", rtn))
	rtn = VerifyAsciiEvent( "df.ce.xx.01.23.45.67.89" );
	LOG(log_lcc, 1, ("verify df.ce.xx.01.23.45.67.89 - %d\n", rtn))
#endif
}

EXPORT void InitLcc ( void )
{
    log_lcc = LogFindIndex ( "lcc" );
}
