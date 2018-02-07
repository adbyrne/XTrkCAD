/** \file cblock.c
 * Implement blocks: a group of trackwork with a single occ. detector
 */
/* Created by Robert Heller on Thu Mar 12 09:43:02 2009
 * ------------------------------------------------------------------
 * Modification History: $Log: not supported by cvs2svn $
 * Modification History: Revision 1.4  2009/09/16 18:32:24  m_fischer
 * Modification History: Remove unused locals
 * Modification History:
 * Modification History: Revision 1.3  2009/09/05 16:40:53  m_fischer
 * Modification History: Make layout control commands a build-time choice
 * Modification History:
 * Modification History: Revision 1.2  2009/07/08 19:13:58  m_fischer
 * Modification History: Make compile under MSVC
 * Modification History:
 * Modification History: Revision 1.1  2009/07/08 18:40:27  m_fischer
 * Modification History: Add switchmotor and block for layout control
 * Modification History:
 * Modification History: Revision 1.1  2002/07/28 14:03:50  heller
 * Modification History: Add it copyright notice headers
 * Modification History:
 * ------------------------------------------------------------------
 * Contents:
 * ------------------------------------------------------------------
 *
 *     Generic Project
 *     Copyright (C) 2005  Robert Heller D/B/A Deepwoods Software
 * 			51 Locke Hill Road
 * 			Wendell, MA 01379-9728
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  T_BLOCK
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/cblock.c,v 1.5 2009-11-23 19:46:16 rheller Exp $
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "compound.h"
#include "cundo.h"
#include "custom.h"
#include "fileio.h"
#include "i18n.h"
#include "messages.h"
#include "param.h"
#include "track.h"
#include "trackx.h"
#include "utility.h"

EXPORT TRKTYP_T T_BLOCK = -1;

static int log_block = 0;

static void NoDrawLine(drawCmd_p d, coOrd p0, coOrd p1, wDrawWidth width,
		       wDrawColor color ) {}
static void NoDrawArc(drawCmd_p d, coOrd p, DIST_T r, ANGLE_T angle0,
		      ANGLE_T angle1, BOOL_T drawCenter, wDrawWidth width,
		      wDrawColor color ) {}
static void NoDrawString( drawCmd_p d, coOrd p, ANGLE_T a, char * s,
			  wFont_p fp, FONTSIZE_T fontSize, wDrawColor color ) {}
static void NoDrawBitMap( drawCmd_p d, coOrd p, wDrawBitMap_p bm,
			  wDrawColor color) {}
static void NoDrawFillPoly( drawCmd_p d, int cnt, coOrd * pts,
			    wDrawColor color ) {}
static void NoDrawFillCircle( drawCmd_p d, coOrd p, DIST_T r,
			      wDrawColor color ) {}

static drawFuncs_t noDrawFuncs = {
	0,
	NoDrawLine,
	NoDrawArc,
	NoDrawString,
	NoDrawBitMap,
	NoDrawFillPoly,
	NoDrawFillCircle };

static drawCmd_t blockD = {
	NULL,
	&noDrawFuncs,
	0,
	1.0,
	0.0,
	{0.0,0.0}, {0.0,0.0},
	Pix2CoOrd, CoOrd2Pix };

static char blockName[STR_SHORT_SIZE];
static char blockScript[STR_LONG_SIZE];
static long blockElementCount;
static track_p first_block;
static track_p last_block;

static paramData_t blockPLs[] = {
/*0*/ { PD_STRING, blockName, "name", PDO_NOPREF, (void*)200, N_("Name") },
/*1*/ { PD_STRING, blockScript, "script", PDO_NOPREF, (void*)350, N_("Script") }
};
static paramGroup_t blockPG = { "block", 0, blockPLs,  sizeof blockPLs/sizeof blockPLs[0] };
static wWin_p blockW;

static char blockEditName[STR_SHORT_SIZE];
static char blockEditScript[STR_LONG_SIZE];
static char blockEditSegs[STR_LONG_SIZE];
static track_p blockEditTrack;

static paramData_t blockEditPLs[] = {
/*0*/ { PD_STRING, blockEditName, "name", PDO_NOPREF, (void*)200, N_("Name") },
/*1*/ { PD_STRING, blockEditScript, "script", PDO_NOPREF, (void*)350, N_("Script") },
/*2*/ { PD_STRING, blockEditSegs, "segments", PDO_NOPREF, (void*)350, N_("Segments"), BO_READONLY }, 
};
static paramGroup_t blockEditPG = { "block", 0, blockEditPLs,  sizeof blockEditPLs/sizeof blockEditPLs[0] };
static wWin_p blockEditW;

typedef struct btrackinfo_t {
    track_p t;
    TRKINX_T i;
} btrackinfo_t, *btrackinfo_p;

static dynArr_t blockTrk_da;
#define blockTrk(N) DYNARR_N( btrackinfo_t , blockTrk_da, N )



typedef struct blockData_t {
    char * name;
    char * script;
    BOOL_T IsHilite;
    track_p next_block;
    wIndex_t numTracks;
    btrackinfo_t trackList;
} blockData_t, *blockData_p;

static blockData_p GetblockData ( track_p trk )
{
	return (blockData_p) GetTrkExtraData(trk);
}

static void DrawBlock (track_p t, drawCmd_p d, wDrawColor color )
{
}

static struct {
	char name[STR_SHORT_SIZE];
	char script[STR_LONG_SIZE];
	FLOAT_T length;
	coOrd endPt[2];
} blockData;

typedef enum { NM, SC, LN, E0, E1 } blockDesc_e;
static descData_t blockDesc[] = {
/*NM*/	{ DESC_STRING, N_("Name"), &blockData.name },
/*SC*/  { DESC_STRING, N_("Script"), &blockData.script },
/*LN*/  { DESC_DIM, N_("Length"), &blockData.length },
/*E0*/	{ DESC_POS, N_("End Pt 1: X,Y"), &blockData.endPt[0] },
/*E1*/	{ DESC_POS, N_("End Pt 2: X,Y"), &blockData.endPt[1] },
	{ DESC_NULL } };

static void UpdateBlock (track_p trk, int inx, descData_p descUpd, BOOL_T needUndoStart )
{
	blockData_p xx = GetblockData(trk);
	const char * thename, *thescript;
	char *newName, *newScript;
	BOOL_T changed, nChanged, sChanged;

	LOG( log_block, 1, ("*** UpdateBlock(): needUndoStart = %d\n",needUndoStart))
	if ( inx == -1 ) {
		nChanged = sChanged = changed = FALSE;
		thename = wStringGetValue( (wString_p)blockDesc[NM].control0 );
		if ( strcmp( thename, xx->name ) != 0 ) {
			nChanged = changed = TRUE;
			newName = MyStrdup(thename);
		}
		thescript = wStringGetValue( (wString_p)blockDesc[SC].control0 );
		if ( strcmp( thescript, xx->script ) != 0 ) {
			sChanged = changed = TRUE;
			newScript = MyStrdup(thescript);
		}
		if ( ! changed ) return;
		if ( needUndoStart )
			UndoStart( _("Change block"), "Change block" );
		UndoModify( trk );
		if (nChanged) {
			MyFree(xx->name);
			xx->name = newName;
		}
		if (sChanged) {
			MyFree(xx->script);
			xx->script = newScript;
		}
		return;
	}
}

static DIST_T DistanceBlock (track_p t, coOrd * p )
{
	blockData_p xx = GetblockData(t);
	DIST_T closest, current;
	int iTrk = 1;
	coOrd pos = *p;
	closest = GetTrkDistance ((&(xx->trackList))[0].t, &pos);
	coOrd best_pos = pos;
	for (; iTrk < xx->numTracks; iTrk++) {
		pos = *p;
		current = GetTrkDistance ((&(xx->trackList))[iTrk].t, &pos);
		if (current < closest) {
			closest = current;
			best_pos = pos;
		}
	}
	*p = best_pos;
	return closest;
}

static void DescribeBlock (track_p trk, char * str, CSIZE_T len )
{
	blockData_p xx = GetblockData(trk);
	wIndex_t tcount = 0;
	track_p lastTrk = NULL;
	long listLabelsOption = listLabels;

	LOG( log_block, 1, ("*** DescribeBlock(): trk is T%d\n",GetTrkIndex(trk)))
	FormatCompoundTitle( listLabelsOption, xx->name );
	if (message[0] == '\0')
		FormatCompoundTitle( listLabelsOption|LABEL_DESCR, xx->name );
	strcpy( str, _(GetTrkTypeName( trk )) );
	str++;
	while (*str) {
		*str = tolower((unsigned char)*str);
		str++;
	}
	sprintf( str, _("(%d): Layer=%d %s"),
		GetTrkIndex(trk), GetTrkLayer(trk)+1, message );
	strncpy(blockData.name,xx->name,STR_SHORT_SIZE-1);
	blockData.name[STR_SHORT_SIZE-1] = '\0';
	strncpy(blockData.script,xx->script,STR_LONG_SIZE-1);
	blockData.script[STR_LONG_SIZE-1] = '\0';
	blockData.length = 0;
	if (xx->numTracks > 0) {
		blockData.endPt[0] = GetTrkEndPos((&(xx->trackList))[0].t,0);
	}
	for (tcount = 0; tcount < xx->numTracks; tcount++) {
                if ((&(xx->trackList))[tcount].t == NULL) continue;
		blockData.length += GetTrkLength((&(xx->trackList))[tcount].t,0,1);
		lastTrk = (&(xx->trackList))[tcount].t;
	}
	if (lastTrk != NULL) blockData.endPt[1] = GetTrkEndPos(lastTrk,1);
	blockDesc[E0].mode =
	blockDesc[E1].mode =
	blockDesc[LN].mode = DESC_RO;
	blockDesc[NM].mode =
	blockDesc[SC].mode = DESC_NOREDRAW;
	DoDescribe(_("Block"), trk, blockDesc, UpdateBlock );

}

static int blockDebug (track_p trk)
{
	wIndex_t iTrack;
	blockData_p xx = GetblockData(trk);
	LOG( log_block, 1, ("*** blockDebug(): trk = %08x\n",trk))
	LOG( log_block, 1, ("*** blockDebug(): Index = %d\n",GetTrkIndex(trk)))
	LOG( log_block, 1, ("*** blockDebug(): name = \"%s\"\n",xx->name))
	LOG( log_block, 1, ("*** blockDebug(): script = \"%s\"\n",xx->script))
	LOG( log_block, 1, ("*** blockDebug(): numTracks = %d\n",xx->numTracks))
	for (iTrack = 0; iTrack < xx->numTracks; iTrack++) {
                if ((&(xx->trackList))[iTrack].t == NULL) continue;
		LOG( log_block, 1, ("*** blockDebug(): trackList[%d] = T%d, ",iTrack,GetTrkIndex((&(xx->trackList))[iTrack].t)))
		LOG( log_block, 1, ("%s\n",GetTrkTypeName((&(xx->trackList))[iTrack].t)))
	}
	return(0);
}

static BOOL_T blockCheckContigiousPath()
{
	EPINX_T ep, epCnt, epN;
	int inx;
	track_p trk, trk1;
	DIST_T dist;
	ANGLE_T angle;
        /*int pathElemStart = 0;*/
	coOrd endPtOrig = zero;
	BOOL_T IsConnectedP;
	trkEndPt_p endPtP;
	DYNARR_RESET( trkEndPt_t, tempEndPts_da );

	for ( inx=0; inx<blockTrk_da.cnt; inx++ ) {
		trk = blockTrk(inx).t;
		epCnt = GetTrkEndPtCnt(trk);
		IsConnectedP = FALSE;
		for ( ep=0; ep<epCnt; ep++ ) {
			trk1 = GetTrkEndTrk(trk,ep);
			if ( trk1 == NULL || !GetTrkSelected(trk1) ) {
				/* boundary EP */
				for ( epN=0; epN<tempEndPts_da.cnt; epN++ ) {
					dist = FindDistance( GetTrkEndPos(trk,ep), tempEndPts(epN).pos );
					angle = NormalizeAngle( GetTrkEndAngle(trk,ep) - tempEndPts(epN).angle + connectAngle/2.0 );
					if ( dist < connectDistance && angle < connectAngle )
						break;
				}
				if ( epN>=tempEndPts_da.cnt ) {
					DYNARR_APPEND( trkEndPt_t, tempEndPts_da, 10 );
					endPtP = &tempEndPts(tempEndPts_da.cnt-1);
					memset( endPtP, 0, sizeof *endPtP );
					endPtP->pos = GetTrkEndPos(trk,ep);
					endPtP->angle = GetTrkEndAngle(trk,ep);
					/*endPtP->track = trk1;*/
					/* These End Points are dummies --
					   we don't want DeleteTrack to look at
					   them. */
					endPtP->track = NULL;
					endPtP->index = (trk1?GetEndPtConnectedToMe(trk1,trk):-1);
					endPtOrig.x += endPtP->pos.x;
					endPtOrig.y += endPtP->pos.y;
				}
			} else {
				IsConnectedP = TRUE;
			}
		}
		if (!IsConnectedP && blockTrk_da.cnt > 1) return FALSE;
	}
	return TRUE;
}

static void DeleteBlock ( track_p t )
{
    track_p trk1;
    blockData_p xx1;

	LOG( log_block, 1, ("*** DeleteBlock(%p)\n",t))
	blockData_p xx = GetblockData(t);
	LOG( log_block, 1, ("*** DeleteBlock(): index is %d\n",GetTrkIndex(t)))
	LOG( log_block, 1, ("*** DeleteBlock(): xx = %p, xx->name = %p, xx->script = %p\n",
                xx,xx->name,xx->script))
	MyFree(xx->name); xx->name = NULL;
	MyFree(xx->script); xx->script = NULL;

	if (first_block == t)
	    first_block = xx->next_block;
	trk1 = first_block;
	while(trk1) {
		xx1 = GetblockData (trk1);
		if (xx1->next_block == t) {
			xx1->next_block = xx->next_block;
			break;
		}
		trk1 = xx1->next_block;
	}
	if (t == last_block)
		last_block = trk1;

}

static BOOL_T WriteBlock ( track_p t, FILE * f )
{
	BOOL_T rc = TRUE;
	wIndex_t iTrack;
	blockData_p xx = GetblockData(t);

	rc &= fprintf(f, "BLOCK %d \"%s\" \"%s\"\n",
		GetTrkIndex(t), xx->name, xx->script)>0;
	for (iTrack = 0; iTrack < xx->numTracks && rc; iTrack++) {
                if ((&(xx->trackList))[iTrack].t == NULL) continue;
		rc &= fprintf(f, "\tTRK %d\n",
				GetTrkIndex((&(xx->trackList))[iTrack].t))>0;
	}
	rc &= fprintf( f, "\tEND\n" )>0;
	return rc;
}

static void ReadBlock ( char * line )
{
	TRKINX_T trkindex;
	wIndex_t index;
	track_p trk, trk1;
	char * cp = NULL;
	blockData_p xx,xx1;
	wIndex_t iTrack;
	EPINX_T ep;
	trkEndPt_p endPtP;
	char *name, *script;

	LOG( log_block, 1, ("*** ReadBlock: line is '%s'\n",line))
	if (!GetArgs(line+6,"dqq",&index,&name,&script)) {
		return;
	}
	DYNARR_RESET( btrackinfo_p , blockTrk_da );
	while ( (cp = GetNextLine()) != NULL ) {
		while (isspace((unsigned char)*cp)) cp++;
		if ( strncmp( cp, "END", 3 ) == 0 ) {
			break;
		}
		if ( *cp == '\n' || *cp == '#' ) {
			continue;
		}
		if ( strncmp( cp, "TRK", 3 ) == 0 ) {
			if (!GetArgs(cp+4,"d",&trkindex)) return;
			/*trk = FindTrack(trkindex);*/
			DYNARR_APPEND( btrackinfo_p *, blockTrk_da, 10 );
			blockTrk(blockTrk_da.cnt-1).i = trkindex;
		}
	}
	/*blockCheckContigiousPath(); save for ResolveBlockTracks */
	trk = NewTrack(index, T_BLOCK, tempEndPts_da.cnt, sizeof(blockData_t)+(sizeof(btrackinfo_t)*(blockTrk_da.cnt-1))+1);
	for ( ep=0; ep<tempEndPts_da.cnt; ep++) {
		endPtP = &tempEndPts(ep);
		SetTrkEndPoint( trk, ep, endPtP->pos, endPtP->angle );
	}
	xx = GetblockData( trk );
	LOG( log_block, 1, ("*** ReadBlock(): trk = %p (%d), xx = %p\n",trk,GetTrkIndex(trk),xx))
	LOG( log_block, 1, ("*** ReadBlock(): name = %p, script = %p\n",name,script))
	xx->name = name;
	xx->script = script;
	xx->IsHilite = FALSE;
	xx->numTracks = blockTrk_da.cnt;
	trk1 = last_block;
	if (!trk1) first_block = trk;
	else {
		xx1 = GetblockData(trk1);
		xx1->next_block = trk;
	}
	xx->next_block = NULL;
	last_block = trk;
	for (iTrack = 0; iTrack < blockTrk_da.cnt; iTrack++) {
		LOG( log_block, 1, ("*** ReadBlock(): copying track T%d\n",GetTrkIndex(blockTrk(iTrack).t)))
		memcpy((void*)&((&(xx->trackList))[iTrack]),(void*)&(blockTrk(iTrack)),sizeof(btrackinfo_t));
	}
	blockDebug(trk);
}

EXPORT void ResolveBlockTrack ( track_p trk )
{
    LOG( log_block, 1, ("*** ResolveBlockTrack(%p)\n",trk))
    blockData_p xx;
    track_p t_trk;
    wIndex_t iTrack;
    if (GetTrkType(trk) != T_BLOCK) return;
    LOG( log_block, 1, ("*** ResolveBlockTrack(%d)\n",GetTrkIndex(trk)))
    xx = GetblockData(trk);
    for (iTrack = 0; iTrack < xx->numTracks; iTrack++) {
        t_trk = FindTrack((&(xx->trackList))[iTrack].i);
        if (t_trk == NULL) {
            NoticeMessage( _("resolveBlockTrack: T%d[%d]: T%d doesn't exist"), _("Continue"), NULL, GetTrkIndex(trk), iTrack, (&(xx->trackList))[iTrack].i );
        }
        (&(xx->trackList))[iTrack].t = t_trk;
        LOG( log_block, 1, ("*** ResolveBlockTrack(): %d (%d): %p\n",iTrack,(&(xx->trackList))[iTrack].i,t_trk))
    }
}

static void MoveBlock (track_p trk, coOrd orig ) {}
static void RotateBlock (track_p trk, coOrd orig, ANGLE_T angle ) {}
static void RescaleBlock (track_p trk, FLOAT_T ratio ) {}

static trackCmd_t blockCmds = {
	"BLOCK",
	DrawBlock,
	DistanceBlock,
	DescribeBlock,
	DeleteBlock,
	WriteBlock,
	ReadBlock,
	MoveBlock,
	RotateBlock,
	RescaleBlock,
	NULL, /* audit */
	NULL, /* getAngle */
	NULL, /* split */
	NULL, /* traverse */
	NULL, /* enumerate */
	NULL, /* redraw */
	NULL, /* trim */
	NULL, /* merge */
	NULL, /* modify */
	NULL, /* getLength */
	NULL, /* getTrkParams */
	NULL, /* moveEndPt */
	NULL, /* query */
	NULL, /* ungroup */
	NULL, /* flip */
	NULL, /* drawPositionIndicator */
	NULL, /* advancePositionIndicator */
	NULL, /* checkTraverse */
	NULL, /* makeParallel */
	NULL  /* drawDesc */
};



static BOOL_T TrackInBlock (track_p trk, track_p blk) {
	wIndex_t iTrack;
	blockData_p xx = GetblockData(blk);
	for (iTrack = 0; iTrack < xx->numTracks; iTrack++) {
		if (trk == (&(xx->trackList))[iTrack].t) return TRUE;
	}
	return FALSE;
}

static track_p FindBlock (track_p trk) {
	track_p a_trk;
	blockData_p xx;
	if (!first_block) return NULL;
	a_trk = first_block;
	while (a_trk) {
		if (!IsTrackDeleted(a_trk)) {
			if (GetTrkType(a_trk) == T_BLOCK &&
					TrackInBlock(trk,a_trk)) return a_trk;
		}
		xx = GetblockData(a_trk);
		a_trk = xx->next_block;
	}
	return NULL;
}

static void BlockOk ( void * junk )
{
	blockData_p xx,xx1;
	track_p trk,trk1;
	wIndex_t iTrack;
	EPINX_T ep;
	trkEndPt_p endPtP;

	LOG( log_block, 1, ("*** BlockOk()\n"))
	DYNARR_RESET( btrackinfo_p *, blockTrk_da );

	ParamUpdate( &blockPG );
	if ( blockName[0]==0 ) {
		NoticeMessage( _("Block must have a name!"), _("Ok"), NULL);
		return;
	}
	wDrawDelayUpdate( mainD.d, TRUE );
	/*
	 * Collect tracks
	 */
	trk = NULL;
	while ( TrackIterate( &trk ) ) {
		if ( GetTrkSelected( trk ) ) {
			if ( IsTrack(trk) ) {
				DYNARR_APPEND( btrackinfo_p *, blockTrk_da, 10 );
				LOG( log_block, 1, ("*** BlockOk(): adding track T%d\n",GetTrkIndex(trk)))
                                blockTrk(blockTrk_da.cnt-1).t = trk;
                                blockTrk(blockTrk_da.cnt-1).i = GetTrkIndex(trk);
			}
		}
	}
	if ( blockTrk_da.cnt>0 ) {
		if ( blockTrk_da.cnt > 128 ) {
			NoticeMessage( MSG_TOOMANYSEGSINGROUP, _("Ok"), NULL );
			wDrawDelayUpdate( mainD.d, FALSE );
			wHide( blockW );
			return;
		}
		/* Need to check that all block elements are connected to each
		   other... */
		if (!blockCheckContigiousPath()) {
			NoticeMessage( _("Block is discontigious!"), _("Ok"), NULL );
			wDrawDelayUpdate( mainD.d, FALSE );
			wHide( blockW );
			return;
		}
		UndoStart( _("Create block"), "Create block" );
		/* Create a block object */
		LOG( log_block, 1, ("*** BlockOk(): %d tracks in block\n",blockTrk_da.cnt))
		trk = NewTrack(0, T_BLOCK, tempEndPts_da.cnt, sizeof(blockData_t)+(sizeof(btrackinfo_t)*(blockTrk_da.cnt-1))+1);
		for ( ep=0; ep<tempEndPts_da.cnt; ep++) {
			endPtP = &tempEndPts(ep);
			SetTrkEndPoint( trk, ep, endPtP->pos, endPtP->angle );
		}

		xx = GetblockData( trk );
		LOG(log_block, 1, ("*** BlockOk(): trk = %p (%d), xx = %p\n", trk, GetTrkIndex(trk), xx))
		xx->name = MyStrdup(blockName);
		xx->script = MyStrdup(blockScript);
        xx->IsHilite = FALSE;
		xx->numTracks = blockTrk_da.cnt;
		trk1 = last_block;
		if (!trk1) {
			first_block = trk;
		}
		else {
			xx1 = GetblockData(trk1);
			xx1->next_block = trk;
		}
		xx->next_block = NULL;
		last_block = trk;
		for (iTrack = 0; iTrack < blockTrk_da.cnt; iTrack++) {
			LOG( log_block, 1, ("*** BlockOk(): copying track T%d\n",GetTrkIndex(blockTrk(iTrack).t)))
			memcpy((void*)&(&(xx->trackList))[iTrack],(void*)&blockTrk(iTrack),sizeof(btrackinfo_t));
		}
		blockDebug(trk);
		UndoEnd();
	}
	wHide( blockW );

}

static void NewBlockDialog()
{
	track_p trk = NULL;

	LOG( log_block, 1, ("*** NewBlockDialog()\n"))
	blockElementCount = 0;

	while ( TrackIterate( &trk ) ) {
		if ( GetTrkSelected( trk ) ) {
			if ( !IsTrack( trk ) ) {
				ErrorMessage( _("Non track object skipped!") );
				continue;
			}
			if ( FindBlock( trk ) != NULL ) {
				ErrorMessage( _("Selected track is already in a block, skipped!") );
				continue;
			}
			blockElementCount++;
		}
	}

	if (blockElementCount == 0) {
		ErrorMessage( MSG_NO_SELECTED_TRK );
		return;
	}
	if ( log_block < 0 ) log_block = LogFindIndex( "block" );
	if ( !blockW ) {
		ParamRegister( &blockPG );
		blockW = ParamCreateDialog (&blockPG, MakeWindowTitle(_("Create Block")), _("Ok"), BlockOk, wHide, TRUE, NULL, F_BLOCK, NULL );
		blockD.dpi = mainD.dpi;
	}
	ParamLoadControls( &blockPG );
	wShow( blockW );
}

static STATUS_T CmdBlockCreate( wAction_t action, coOrd pos )
{
	LOG( log_block, 1, ("*** CmdBlockAction(%08x,{%f,%f})\n",action,pos.x,pos.y))
	switch (action & 0xFF) {
	case C_START:
                LOG( log_block, 1,("*** CmdBlockCreate(): C_START\n"))
		NewBlockDialog();
		return C_TERMINATE;
	default:
		return C_CONTINUE;
	}
}

#if 0
extern BOOL_T inDescribeCmd;

static STATUS_T CmdBlockEdit( wAction_t action, coOrd pos )
{
	track_p trk,btrk;
	char msg[STR_SIZE];

	switch (action) {
	case C_START:
		InfoMessage( _("Select a track") );
		inDescribeCmd = TRUE;
		return C_CONTINUE;
	case C_DOWN:
		if ((trk = OnTrack(&pos, TRUE, TRUE )) == NULL) {
			return C_CONTINUE;
		}
		btrk = FindBlock( trk );
		if ( !btrk ) {
			ErrorMessage( _("Not a block!") );
			return C_CONTINUE;
		}
		DescribeTrack (btrk, msg, sizeof msg );
		InfoMessage( msg );
		return C_CONTINUE;
	case C_REDRAW:
		return C_CONTINUE;
	case C_CANCEL:
		inDescribeCmd = FALSE;
		return C_TERMINATE;
	default:
		return C_CONTINUE;
	}
}

static STATUS_T CmdBlockDelete( wAction_t action, coOrd pos )
{
	track_p trk,btrk;
	blockData_p xx;

	switch (action) {
	case C_START:
		InfoMessage( _("Select a track") );
		return C_CONTINUE;
	case C_DOWN:
		if ((trk = OnTrack(&pos, TRUE, TRUE )) == NULL) {
			return C_CONTINUE;
		}
		btrk = FindBlock( trk );
		if ( !btrk ) {
			ErrorMessage( _("Not a block!") );
			return C_CONTINUE;
		}
		/* Confirm Delete Block */
		xx = GetblockData(btrk);
		if ( NoticeMessage( _("Really delete block %s?"), _("Yes"), _("No"), xx->name) ) {
			UndoStart( _("Delete Block"), "delete" );
			DeleteTrack (btrk, FALSE);
			UndoEnd();
			return C_TERMINATE;
		}
		return C_CONTINUE;
	case C_REDRAW:
		return C_CONTINUE;
	case C_CANCEL:
		return C_TERMINATE;
	default:
		return C_CONTINUE;
	}
}


#define BLOCK_CREATE 0
#define BLOCK_EDIT   1
#define BLOCK_DELETE 2

static STATUS_T CmdBlock (wAction_t action, coOrd pos )
{
	LOG( log_block, 1, ("*** CmdBlock(%08x,{%f,%f})\n",action,pos.x,pos.y))

	switch ((long)commandContext) {
	case BLOCK_CREATE: return CmdBlockCreate(action,pos);
	case BLOCK_EDIT:   return CmdBlockEdit(action,pos);
	case BLOCK_DELETE: return CmdBlockDelete(action,pos);
	default: return C_TERMINATE;
	}
}
#endif

EXPORT void CheckDeleteBlock (track_p t) 
{
    track_p blk,trk1;
    blockData_p xx,xx1;
    if (!IsTrack(t)) return;
    blk = FindBlock(t);
    if (blk == NULL) return;
    NoticeMessage(_("Deleting block %s"),_("Ok"),NULL,xx->name);
    DeleteTrack(blk,FALSE);
}

static void BlockEditOk ( void * junk )
{
    blockData_p xx;
    track_p trk;

    LOG( log_block, 1, ("*** BlockEditOk()\n"))
    ParamUpdate (&blockEditPG );
    if ( blockEditName[0]==0 ) {
        NoticeMessage( _("Block must have a name!"), _("Ok"), NULL);
        return;
    }
    wDrawDelayUpdate( mainD.d, TRUE );
    UndoStart( _("Modify Block"), "Modify Block" );
    trk = blockEditTrack;
    xx = GetblockData( trk );
    xx->name = MyStrdup(blockEditName);
    xx->script = MyStrdup(blockEditScript);
    blockDebug(trk);
    UndoEnd();
    wHide( blockEditW );
}


static void EditBlock (track_p trk)
{
    blockData_p xx = GetblockData(trk);
    wIndex_t iTrack;
    BOOL_T needComma = FALSE;
    char temp[32];
    
    strncpy(blockEditName,xx->name,STR_SHORT_SIZE);
    strncpy(blockEditScript,xx->script,STR_LONG_SIZE);
    blockEditSegs[0] = '\0';
    for (iTrack = 0; iTrack < xx->numTracks ; iTrack++) {
        if ((&(xx->trackList))[iTrack].t == NULL) continue;
        sprintf(temp,"%d",GetTrkIndex((&(xx->trackList))[iTrack].t));
        if (needComma) strcat(blockEditSegs,", ");
        strcat(blockEditSegs,temp);
        needComma = TRUE;
    }
    blockEditTrack = trk;
    if ( !blockEditW ) {
        ParamRegister( &blockEditPG );
        blockEditW = ParamCreateDialog (&blockEditPG, 
                                        MakeWindowTitle(_("Edit block")), 
                                        _("Ok"), BlockEditOk, 
                                        wHide, TRUE, NULL, F_BLOCK, 
                                        NULL );
    }
    ParamLoadControls( &blockEditPG );
    sprintf( message, _("Edit block %d"), GetTrkIndex(trk) );
    wWinSetTitle( blockEditW, message );
    wShow (blockEditW);
}

static coOrd blkhiliteOrig, blkhiliteSize;
static POS_T blkhiliteBorder;
static wDrawColor blkhiliteColor = 0;
static void DrawBlockTrackHilite( void )
{
	wPos_t x, y, w, h;
	if (blkhiliteColor==0)
		blkhiliteColor = wDrawColorGray(87);
	w = (wPos_t)((blkhiliteSize.x/mainD.scale)*mainD.dpi+0.5);
	h = (wPos_t)((blkhiliteSize.y/mainD.scale)*mainD.dpi+0.5);
	mainD.CoOrd2Pix(&mainD,blkhiliteOrig,&x,&y);
	wDrawFilledRectangle( mainD.d, x, y, w, h, blkhiliteColor, wDrawOptTemp );
}


static int BlockMgmProc ( int cmd, void * data )
{
    track_p trk = (track_p) data;
    blockData_p xx = GetblockData(trk);
    wIndex_t iTrack;
    BOOL_T needComma = FALSE;
    char temp[32];
    /*char msg[STR_SIZE];*/
    coOrd tempOrig, tempSize;
    BOOL_T first = TRUE;
    
    switch ( cmd ) {
    case CONTMGM_CAN_EDIT:
        return TRUE;
        break;
    case CONTMGM_DO_EDIT:
        EditBlock (trk);
        /*inDescribeCmd = TRUE;*/
        /*DescribeTrack (trk, msg, sizeof msg );*/
        /*InfoMessage( msg );*/
        return TRUE;
        break;
    case CONTMGM_CAN_DELETE:
        return TRUE;
        break;
    case CONTMGM_DO_DELETE:
        DeleteTrack (trk, FALSE);
        return TRUE;
        break;
    case CONTMGM_DO_HILIGHT:
        if (!xx->IsHilite) {
            blkhiliteBorder = mainD.scale*0.1;
            if ( blkhiliteBorder < trackGauge ) blkhiliteBorder = trackGauge;
            first = TRUE;
            for (iTrack = 0; iTrack < xx->numTracks ; iTrack++) {
                if ((&(xx->trackList))[iTrack].t == NULL) continue;
                GetBoundingBox( (&(xx->trackList))[iTrack].t, &tempSize, &tempOrig );
                if (first) {
                    blkhiliteOrig = tempOrig;
                    blkhiliteSize = tempSize;
                    first = FALSE;
                } else {
                    if (tempSize.x > blkhiliteSize.x)
                        blkhiliteSize.x = tempSize.x;
                    if (tempSize.y > blkhiliteSize.y)
                        blkhiliteSize.y = tempSize.y;
                    if (tempOrig.x < blkhiliteOrig.x)
                        blkhiliteOrig.x = tempOrig.x;
                    if (tempOrig.y < blkhiliteOrig.y)
                        blkhiliteOrig.y = tempOrig.y;
                }
            }
            blkhiliteOrig.x -= blkhiliteBorder;
            blkhiliteOrig.y -= blkhiliteBorder;
            blkhiliteSize.x -= blkhiliteOrig.x-blkhiliteBorder;
            blkhiliteSize.y -= blkhiliteOrig.y-blkhiliteBorder;
            DrawBlockTrackHilite();
            xx->IsHilite = TRUE;
        }
        break;
    case CONTMGM_UN_HILIGHT:
        if (xx->IsHilite) {
            blkhiliteBorder = mainD.scale*0.1;
            if ( blkhiliteBorder < trackGauge ) blkhiliteBorder = trackGauge;
            first = TRUE;
            for (iTrack = 0; iTrack < xx->numTracks ; iTrack++) {
                if ((&(xx->trackList))[iTrack].t == NULL) continue;
                GetBoundingBox( (&(xx->trackList))[iTrack].t, &tempSize, &tempOrig );
                if (first) {
                    blkhiliteOrig = tempOrig;
                    blkhiliteSize = tempSize;
                    first = FALSE;
                } else {
                    if (tempSize.x > blkhiliteSize.x)
                        blkhiliteSize.x = tempSize.x;
                    if (tempSize.y > blkhiliteSize.y)
                        blkhiliteSize.y = tempSize.y;
                    if (tempOrig.x < blkhiliteOrig.x)
                        blkhiliteOrig.x = tempOrig.x;
                    if (tempOrig.y < blkhiliteOrig.y)
                        blkhiliteOrig.y = tempOrig.y;
                }
            }
            blkhiliteOrig.x -= blkhiliteBorder;
            blkhiliteOrig.y -= blkhiliteBorder;
            blkhiliteSize.x -= blkhiliteOrig.x-blkhiliteBorder;
            blkhiliteSize.y -= blkhiliteOrig.y-blkhiliteBorder;
            DrawBlockTrackHilite();
            xx->IsHilite = FALSE;
        }
        break;
    case CONTMGM_GET_TITLE:
        sprintf( message, "\t%s\t", xx->name);
        for (iTrack = 0; iTrack < xx->numTracks ; iTrack++) {
            if ((&(xx->trackList))[iTrack].t == NULL) continue;
            sprintf(temp,"%d",GetTrkIndex((&(xx->trackList))[iTrack].t));
            if (needComma) strcat(message,", ");
            strcat(message,temp);
            needComma = TRUE;
        }
        break;
    }
    return FALSE;
}


//#include "bitmaps/blocknew.xpm"
//#include "bitmaps/blockedit.xpm"
//#include "bitmaps/blockdel.xpm"
#include "bitmaps/block.xpm"

EXPORT void BlockMgmLoad( void )
{
    track_p trk;
    static wIcon_p blockI = NULL;
    
    if ( blockI == NULL) 
        blockI = wIconCreatePixMap( block_xpm );
    
    TRK_ITERATE(trk) {
        if (GetTrkType(trk) != T_BLOCK) continue;
        ContMgmLoad( blockI, BlockMgmProc, (void *)trk );
    }
    
}

EXPORT void InitCmdBlock( wMenu_p menu )
{
	blockName[0] = '\0';
	blockScript[0] = '\0';
        AddMenuButton( menu, CmdBlockCreate, "cmdBlockCreate", _("Block"), 
                       wIconCreatePixMap( block_xpm ), LEVEL0_50, 
                       IC_STICKY|IC_POPUP2, ACCL_BLOCK1, NULL );
	ParamRegister( &blockPG );
}


EXPORT void InitTrkBlock( void )
{
	T_BLOCK = InitObject ( &blockCmds );
	log_block = LogFindIndex ( "block" );
}


