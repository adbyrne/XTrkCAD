/** \file clcc.h
 * Definition of LCC components
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2021 Philip Cameron
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef HAVE_CLCC_H
#define HAVE_CLCC_H

#include "common.h"

//Automatically Routed
#define EVT_EMERGENCY_OFF        0x010000000000ffffULL
#define EVT_EMERGENCY_CLEAR      0x010000000000fffeULL
#define EVT_EMERGENCY_STOP       0x010000000000fffdULL
#define EVT_EMERGENCY_STOP_CLEAR 0x010000000000fffcULL

// http://registry.openlcb.org/uniqueidranges
// Reserved - uninitialized event
#define EVT_NULL                 0ULL

// Reserved - DCC-DIY -  bytes 0, 1, 2 
// bytes 3, 4, 5 are Self-assigned
// This works as long as events are not shared with others
#define EVT_XTRKCAD_ROOT         0x02010D0000000000ULL
// EVT_BLOCK_BASE
#define EVT_BLOCK_BASE           0x0000000100000000ULL
// EVT_SIG_BASE
#define EVT_SIG_BASE             0x0000000200000000ULL
// EVT_SWMOTOR_BASE
#define EVT_SWMOTOR_BASE         0x0000000300000000ULL

typedef unsigned long long event_t;

typedef struct evtId {
	char *name;	// name of the event
	char *script;	// script
	event_t evt;	// lcc event
} evtId_t;

typedef struct evtBlk {
	track_p owner;	// a sig, blk or swmotor
	char *ownerName;// name of the owner
	int state;      // index of latest received event 
	int numEvt;	// number of events in this block
	evtId_t eId[1];	// vector of events (must be last item)
} evtBlk_t, *evtBlk_p;

// This is the default and can be changes at will
extern event_t evt_xtrkcad_root;
extern BOOL_T OpenLCBmode;

#define ASP_OFF 0
#define ASP_RED 1
#define ASP_YELLOW 2
#define ASP_GREEN 3

/* clcc.c */
void UpdateEvent( evtId_t *eId, char *name, char *script );
void UpdateOwnerName( evtBlk_p eb, char *name );
evtBlk_p NewBlockEvt( track_p block, char *name );
evtBlk_p NewSignalEvt( track_p signal, char *name, int numEvts );
int GetSignalHead( track_p sig );
int GetAspect( evtBlk_p eb );
void SetAspect( evtBlk_p eb, int aspect );
evtBlk_p NewSwmotorPosEvt( track_p swmotor, track_p turnout, char *name, int num );
evtBlk_p NewSwmotorLockEvt( track_p swmotor, char *name );

void DeleteEvtBlk( evtBlk_p eb );
void DisplayEvtBlk( char *msg, evtBlk_p eb );
char * EventToAscii( event_t evt );
event_t AsciiToEvent( char * aEvt );
BOOL_T VerifyAsciiEvent( const char * evt );

/* cturnout.c */
int GetNumPos( track_p trk );
long GetPosNames( track_p trk, evtId_t * item );

#endif // !HAVE_CLCC_H
