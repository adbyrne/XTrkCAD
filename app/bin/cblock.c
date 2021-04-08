/** \file cblock.c
 * Implement blocks: a group of trackwork with a single occupancy detector
 *  See app/doc/appendix.but "Block occupancy and the Model RR System's Dispatcher"
 *      app/doc/managem.but "Layout Control Elements Dialog"
 *  Blocks:
 *    - Connect to a turnout or another block at each end
 *      - very long blocks are divided into sub-blocks based on Max Block
 *        Length.
 *    - Have a minimum and possibly maximum length
 *      - min and max length is saved in the layout file
 *      - may be set/changed in options->layout
 *           "Min Block Length" and "Max Block Length"
 *      - if length is changed the blocks are deleted and may be recreated
 *        using the new length. (future)
 *    - When the block length is > 2 * maxBlockLength, the block is divided
 *      into sub-blocks. This allows trains to follow each other down a long
 *      block.
 *    - Have from 1 to 128 track segments
 *    - When any segment of the block is occupied, the whole block
 *      is occupied.
 *   Dynamic Blocks:
 *    - Created when a train enters a turnout and the path through the
 *      turnout(s) lead to another block.
 *    - A path between blocks, when a dynamic block exists, is drawn in green.
 *    - Deleted when the train exits the block.
 *    - When any turnout in the dynamic block is occupied all turnouts are
 *      occupied.
 *    - A Dynamic block is associated with (attached to) one connected block.
 *      Occupancy is reported as part of the attached block.
 *      They do not report occupancy. They do not have a name or script.
 *      If the whole train is in the dynamic block, the block it came from
 *      remains occupied until the train enters the next block.
 *   Note: when a turnout is occupied, it's position can't be changed.
 *
 *   manage->Mange Layout Control Elements "Add Missing" button
 *   automatically creates all (non dynamic) blocks needed by the
 *   layout based on Min and Max Block Lengths.
 *
 *   manage->Mange Layout Control Elements "Delete Blocks" button
 *   automatically deletes all blocks.
 *
 *   Options->Display "Draw Occupied Blocks" may be set to highlight the
 *   occupied blocks on the layout while in train mode.
 */
/* Created by Robert Heller on Thu Mar 12 09:43:02 2009
 * ------------------------------------------------------------------
 * Modification History: $Log: not supported by cvs2svn $
 * Modification History: Revision 1.6  2021/02/02 12:00:00  pecameron
 * Modification History: Add Dynamic Blocks.
 * Modification History: Track block occupancy.
 * Modification History: Prevent occupied turnout from being changed.
 * Modification History: Revision 1.5  2020/04/10 17:22:00  pecameron
 * Modification History: Revised to collect all segments in a block
 * Modification History: Revised to get the length of each block
 * Modification History: Revised to display length in manage->Layout Control Elements
 * Modification History: Revised to set minBlockLen in layout file
 * Modification History: Revised to add Min Block Length to options->preferences
 * Modification History: Revised to store MINBLOCKLENGTH in .xtc file
 * Modification History: Revised to use block eps for pos and angle
 * Modification History: Revised to add AddMissingBlockTrack to make
 * Modification History:            all needed blocks for layout
 * Modification History: Does not yet track block occupancy.
 * Modification History:
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

#include "compound.h"
#include "cundo.h"
#include "custom.h"
#include "fileio.h"
#include "param.h"
#include "misc.h"
#include "track.h"
#include "trackx.h"
#include "common-ui.h"
#include "layout.h"
#include "ctrain.h"

#ifdef UTFCONVERT
#include "include/utf8convert.h"
#endif // UTFCONVERT

static char * attFlags( track_p blk );
static void verifyOccupancy ( BOOL_T rpt, BOOL_T all );
static void verifyTrackOccupancy ( BOOL_T all );

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
static void NoDrawFillPoly( drawCmd_p d, int cnt, coOrd * pts, int * types,
			    wDrawColor color, wDrawWidth width, int fill, int open) {}
static void NoDrawFillCircle( drawCmd_p d, coOrd p, DIST_T r,
			      wDrawColor color ) {}
static void EditBlock( track_p trk );
static void initBlockData( track_p blk );
static BOOL_T CheckDeleteBlock( track_p t );
static void getPathEp( track_p trk, coOrd pos, EPINX_T *ep0, EPINX_T *ep1 );

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
static coOrd description_offset;
static track_p first_block;
static track_p last_block;

static paramData_t blockPLs[] = {
/*0*/ { PD_STRING, blockName, "name", PDO_NOPREF | PDO_STRINGLIMITLENGTH, I2VP(200), N_("Name"), 0, 0, sizeof( blockName )},
/*1*/ { PD_STRING, blockScript, "script", PDO_NOPREF | PDO_STRINGLIMITLENGTH, I2VP(350), N_("Script"), 0, 0, sizeof( blockScript)}
};
static paramGroup_t blockPG = { "block", 0, blockPLs,  sizeof blockPLs/sizeof blockPLs[0] };
static wWin_p blockW;

static char blockEditName[STR_SHORT_SIZE];
static char blockEditScript[STR_LONG_SIZE];
static char blockEditSegs[STR_LONG_SIZE];
static track_p blockEditTrack;

static paramData_t blockEditPLs[] = {
/*0*/ { PD_STRING, blockEditName, "name", PDO_NOPREF | PDO_STRINGLIMITLENGTH, I2VP(200), N_("Name"), 0, 0, sizeof(blockEditName)},
/*1*/ { PD_STRING, blockEditScript, "script", PDO_NOPREF | PDO_STRINGLIMITLENGTH, I2VP(350), N_("Script"), 0, 0, sizeof(blockEditScript)},
/*2*/ { PD_STRING, blockEditSegs, "segments", PDO_NOPREF, I2VP(350), N_("Segments"), BO_READONLY },
};
static paramGroup_t blockEditPG = { "block", 0, blockEditPLs,  sizeof blockEditPLs/sizeof blockEditPLs[0] };
static wWin_p blockEditW;

typedef struct btrackinfo_t {
    track_p t;
    TRKINX_T i;
} btrackinfo_t, *btrackinfo_p;

static dynArr_t blockTrk_da;

#define blockTrk(N) DYNARR_N( btrackinfo_t , blockTrk_da, N )

#define tracklist(N) ((&xx->trackList)[N])
static coOrd blockOrig, blockSize;
static POS_T blockBorder;
static BOOL_T blockUndoStarted;

static DIST_T blockLen;

typedef struct blockData_t {
    extraDataBase_t base;
    char * name;
    char * script;
    BOOL_T IsHilite;
    BOOL_T AutoGenerated;
    track_p next_block;
    wIndex_t numTracks;
    DIST_T blkLength;
    coOrd description_offset;
    char * state;
    btrackinfo_t trackList;
    //Note trackList expands - has to be last...
} blockData_t, *blockData_p;

static blockData_p GetblockData( track_p trk )
{
	if ( ! trk || IsTrackDeleted( trk ) ) return NULL;
	return GET_EXTRA_DATA( trk, T_BLOCK, blockData_t );
}

static BOOL_T blockSide = FALSE;

static void SetBlockBoundingBox( track_p trk )
{
	blockData_p xx = GetblockData(trk);
	coOrd hi, lo, hit,lot;
	int i;

	GetBoundingBox((&(xx->trackList))[0].t,&hi,&lo);
	for ( i = 1; i < xx->numTracks; i++ ) {
		GetBoundingBox((&(xx->trackList))[i].t,&hit,&lot);
		hi.x = max(hi.x,hit.x);
		hi.y = max(hi.y,hit.y);
		lo.x = min(lo.x,lot.x);
		lo.y = min(lo.y,lot.y);
	}
	ComputeRectBoundingBox( trk, hi, lo );
}

static BOOL_T BlockDescriptionPos( track_p trk, coOrd * org1, coOrd * org2, coOrd * pos )
{

	coOrd p0,p1;
	coOrd endPos[2];

	blockData_p b = GetblockData(trk);
	if ( ! drawBlocksMode ) return FALSE;
	if ( (GetTrkBits( trk ) & TB_HIDEDESC) != 0 ) return FALSE;
	if ( GetTrkType( trk ) != T_BLOCK || ( GetTrkBits( trk ) & TB_HIDEDESC ) != 0 )
		return FALSE;
	endPos[0] = (trk)->endPt[0].pos;
	endPos[1] = (trk)->endPt[1].pos;
	p0.x = (endPos[1].x - endPos[0].x)/2+endPos[0].x;
	p0.y = (endPos[1].y - endPos[0].y)/2+endPos[0].y;
	p1.x = p0.x + b->description_offset.x;
	p1.y = p0.y + b->description_offset.y;
	*org1 = endPos[0];
	*org2 = endPos[1];
	*pos = p1;
	return TRUE;
}

void DrawBlockDescription(
		track_p trk,
		drawCmd_p d,
		wDrawColor color,
		BOOL_T side)
{
	coOrd p0,p1,p2;
	wFont_p fp;
	blockData_p b = GetblockData(trk);
	if ( ! BlockDescriptionPos( trk, &p0, &p1, &p2 ) ) return;
	DrawLine(d,p0,p2,0,color);
	DrawLine(d,p1,p2,0,color);

	fp = wStandardFont( F_TIMES, FALSE, FALSE );
	DrawBoxedString( BOX_BACKGROUND, d, p2, b->name, fp,
			(wFontSize_t)descriptionFontSize, color, 0.0 );

}

EXPORT DIST_T BlockDescriptionDistance(coOrd pos,
		track_p trk )
{
	blockData_p b = GetblockData(trk);
	coOrd p0,p1,p2;
	if ( ! BlockDescriptionPos( trk, &p0, &p1, &p2 ) ) return 10000;
	return FindDistance( p2, pos );
}

EXPORT STATUS_T BlockDescriptionMove(
		track_p trk,
		wAction_t action,
		coOrd pos )
{
	blockData_p b = GetblockData(trk);
	if ( ! drawBlocksMode ) return C_CONTINUE;
	static coOrd p00, p0, p1, p2;
	static BOOL_T editMode;
	wDrawColor color;

	switch (action) {
	case C_DOWN:
		editMode = TRUE;
		if ( ! BlockDescriptionPos( trk, &p0, &p1, &p2 ) ) return C_CONTINUE;
		p00.x = p2.x - b->description_offset.x;
		p00.y = p2.y - b->description_offset.y;
		/* no break */
	case C_MOVE:
	case C_UP:
		color = GetTrkColor( trk, &mainD );
		b->description_offset.x = (pos.x-p00.x);
		b->description_offset.y = (pos.y-p00.y);
		p2.x = pos.x;
		p2.y = pos.y;
		if ( action == C_UP ) {
			editMode = FALSE;
		}
		// MainRedraw();

		return action==C_UP?C_TERMINATE:C_CONTINUE;
		break;
	case C_REDRAW:
		if ( editMode ) {
			color = drawColorPurple;
			DrawLine( &tempD, p1, p2, 0, color);
			DrawLine( &tempD, p0, p2, 0, color);
			wFont_p fp;
			fp = wStandardFont( F_TIMES, FALSE, FALSE );
			DrawBoxedString( BOX_BACKGROUND, &tempD, p2, b->name, fp,
				(wFontSize_t)descriptionFontSize, color, 0.0 );
		}
	}
	return C_CONTINUE;
}

static void DrawBlock( track_p t, drawCmd_p d, wDrawColor color )
{
	if ( ! drawBlocksMode ) return;
	DIST_T scale2rail = (d->options&DC_PRINT)?(twoRailScale*2+1):twoRailScale;
	//if (d->scale < scale2rail) return;
	blockSide = t->index%2;
	blockData_p b = GetblockData(t);
	track_p trk, last_trk = NULL;
	int i;

	for ( i=0; i < b->numTracks; i++ ) {
		trk = (&(b->trackList))[i].t;
		EPINX_T ep;
		/* Cope with track facing the other way around */
		if ( last_trk ) {
			EPINX_T ep = GetEndPtConnectedToMe(trk,last_trk);
		} else {
			/* First track */
			if ( GetTrkEndAngle(trk,0) <= 180.0 ) blockSide = 1-blockSide;
			ep = 0;
		}
		DrawTrack(trk,d,color);
		last_trk = trk;
	}

	if ( d->scale <= labelScale )
		DrawBlockDescription(t,d,color,blockSide);
}

static struct {
	char name[STR_SHORT_SIZE];
	char script[STR_LONG_SIZE];
	FLOAT_T length;
	coOrd endPt[2];
} blockData;

typedef enum { NM, SC, LN, E0, E1 } blockDesc_e;
static descData_t blockDesc[] = {
/*NM*/	{ DESC_STRING, N_("Name"), &blockData.name, sizeof(blockData.name) },
/*SC*/  { DESC_STRING, N_("Script"), &blockData.script, sizeof(blockData.script) },
/*LN*/  { DESC_DIM, N_("Length"), &blockData.length },
/*E0*/	{ DESC_POS, N_("End Pt 1: X,Y"), &blockData.endPt[0] },
/*E1*/	{ DESC_POS, N_("End Pt 2: X,Y"), &blockData.endPt[1] },
	{ DESC_NULL } };

static void UpdateBlock( track_p trk, int inx, descData_p descUpd, BOOL_T needUndoStart )
{
	blockData_p xx = GetblockData(trk);
	const char * thename, *thescript;
	char *newName, *newScript;
	BOOL_T changed, nChanged, sChanged;
	size_t max_str;

	LOG( log_block, 1, ("*** UpdateBlock(): needUndoStart = %d\n",needUndoStart))
	if ( inx == -1 ) {
		nChanged = sChanged = changed = FALSE;
		thename = wStringGetValue( (wString_p)blockDesc[NM].control0 );

		if ( ! xx->name || strcmp( thename, xx->name ) != 0 ) {
			nChanged = changed = TRUE;
			max_str = blockDesc[NM].max_string;
			if ( max_str && strlen(thename) > max_str-1 ) {
				newName = MyMalloc(max_str);
				strncpy( newName, thename, max_str - 1 );
				newName[max_str-1] = '\0';
				NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED,
						_("Ok"), NULL, max_str-1);
			} else 	newName = MyStrdup(thename);
		}

		thescript = wStringGetValue( (wString_p)blockDesc[SC].control0 );
		if ( ! xx->script || strcmp( thescript, xx->script ) != 0 ) {
			sChanged = changed = TRUE;
			max_str = blockDesc[SC].max_string;
			if ( max_str && strlen( thescript ) > max_str-1 ) {
				newScript = MyMalloc(max_str);
				strncpy(newScript, thescript, max_str - 1);
				newScript[max_str-1] = '\0';
				NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED,
						_("Ok"), NULL, max_str-1);
			} else newScript = MyStrdup(thescript);
		}
		if ( ! changed ) return;
		if ( needUndoStart ) {
			UndoStart( _("Change block"), "Change block" );
			blockUndoStarted = TRUE;
		}
		UndoModify( trk );
		if ( nChanged ) {
			if ( xx->name ) MyFree( xx->name );
			xx->name = newName;
		}
		if ( sChanged ) {
			if ( xx->script ) MyFree( xx->script );
			xx->script = newScript;
		}
		SetBlockBoundingBox( trk );
		return;
	}
}

static DIST_T DistanceBlock( track_p t, coOrd * p )
{
	blockData_p xx = GetblockData(t);
	DIST_T closest, current;
	int iTrk = 1;
	coOrd pos = *p;
	closest = 99999.0;
	coOrd best_pos = pos;
	for ( iTrk = 0; iTrk < xx->numTracks; iTrk++ ) {
		pos = *p;
		if ( (&(xx->trackList))[iTrk].t == NULL ) continue;
		current = GetTrkDistance( (&(xx->trackList))[iTrk].t, &pos );
		if ( current < closest ) {
			closest = current;
			best_pos = pos;
		}
	}
	*p = best_pos;
	return closest;
}

static void DescribeBlock( track_p trk, char * str, CSIZE_T len )
{
	blockData_p xx = GetblockData(trk);
	wIndex_t tcount = 0;
	track_p lastTrk = NULL;
	long listLabelsOption = listLabels;

	LOG( log_block, 1, ("*** DescribeBlock(): trk is T%d\n",GetTrkIndex(trk)))
	FormatCompoundTitle( listLabelsOption, xx->name );
	if ( message[0] == '\0' )
		FormatCompoundTitle( listLabelsOption|LABEL_DESCR, xx->name );
	strcpy( str, _(GetTrkTypeName( trk )) );
	str++;
	while (*str) {
		*str = tolower((unsigned char)*str);
		str++;
	}
	sprintf( str, _("(%d): Layer=%u %s"),
		GetTrkIndex(trk), GetTrkLayer(trk)+1, message );
	blockData.name[0] = '\0';
	strncat(blockData.name,xx->name,STR_SHORT_SIZE-1);
	blockData.script[0] = '\0';
	strncat(blockData.script,xx->script,STR_LONG_SIZE-1);
	blockData.length = 0;
	BOOL_T first = TRUE;
	for ( tcount = 0; tcount < xx->numTracks; tcount++ ) {
	    if ( (&(xx->trackList))[tcount].t == NULL ) continue;
	    if ( first ) {
	    	blockData.endPt[0] = GetTrkEndPos((&(xx->trackList))[tcount].t,0);
	    	first = FALSE;
	    }
	    blockData.endPt[1] = GetTrkEndPos((&(xx->trackList))[tcount].t,1);
	    blockData.length += GetTrkLength((&(xx->trackList))[tcount].t,0,1);
	    tcount++;
	    break;
	}
	blockDesc[E0].mode =
	blockDesc[E1].mode =
	blockDesc[LN].mode = DESC_RO;
	blockDesc[NM].mode =
	blockDesc[SC].mode = DESC_NOREDRAW;
	DoDescribe(_("Block"), trk, blockDesc, UpdateBlock );

}

static void blockDebug( track_p b_trk )
{
	wIndex_t iTrack;
	EPINX_T epN;
	trkEndPt_p endPtP;
	blockData_p xx;

	xx = GetblockData(b_trk);
	LOG( log_block, 1, ("*** blockDebug(): B%d(%p) name = \"%s\" script = \"%s\"\n",
			GetTrkIndex(b_trk), b_trk, xx->name,xx->script))
	// track segments in the block
	for ( iTrack = 0; iTrack < xx->numTracks; iTrack++ ) {
                if ( (&(xx->trackList))[iTrack].t == NULL ) continue;
		LOG( log_block, 1, ("*** blockDebug(): trackList[%d] = T%d, cnt %d",
				iTrack, GetTrkIndex((&(xx->trackList))[iTrack].t),
				GetTrkEndPtCnt((&(xx->trackList))[iTrack].t)))
		LOG( log_block, 1, (", %s\n",GetTrkTypeName((&(xx->trackList))[iTrack].t)))
	}
	// endpoints for the block - prevTrack is the segment at the end of the block
	for ( epN = 0; epN < GetTrkEndPtCnt(b_trk); epN++ ) {
		endPtP = &(b_trk->endPt[epN]);
		LOG( log_block, 1, ("*** blockDebug(): ep[%d] pos %0.1f %0.1f "
			"angle %0.2f track T%d options 0x%08lX\n",
			epN, endPtP->pos.x, endPtP->pos.y, endPtP->angle,
			endPtP->prevTrack?GetTrkIndex(endPtP->prevTrack):0,
			endPtP->option))
	}
	return;
}

/* Prereq blockTrack_da is set to have all the tracks to check all tracks must be selected */
static BOOL_T blockCheckContigiousPath( BOOL_T selected )
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
	int validEnds = 2;
	DYNARR_RESET( trkEndPt_t, tempEndPts_da );

	for ( inx = 0; inx < blockTrk_da.cnt; inx++ ) {
		trk = blockTrk(inx).t;
		if ( ! trk ) continue;              //Ignore missing tracks
		epCnt = GetTrkEndPtCnt(trk);
		if ( epCnt > 2 ) validEnds += epCnt-2;  //Add extra ends
		for ( ep = 0; ep < epCnt; ep++ ) {
			trk1 = GetTrkEndTrk(trk,ep);
			IsConnectedP = FALSE;
			if ( trk1 == NULL || ( selected && ! GetTrkSelected( trk1 ) ) ) {
				/* boundary EP - is it connected to part of the array? */
				for ( epN = 0; epN < tempEndPts_da.cnt; epN++ ) {
					dist = FindDistance( GetTrkEndPos(trk,ep), tempEndPts(epN).pos );
					angle = NormalizeAngle( GetTrkEndAngle(trk,ep) - tempEndPts(epN).angle + connectAngle/2.0 );
					if ( dist < connectDistance && angle < connectAngle )
						break;
				}
				/* Add to array if not found */
				if ( epN >= tempEndPts_da.cnt ) {
					DYNARR_APPEND( trkEndPt_t, tempEndPts_da, 10 );
					endPtP = &tempEndPts(tempEndPts_da.cnt-1);
					memset( endPtP, 0, sizeof *endPtP );
					endPtP->pos = GetTrkEndPos(trk,ep);
					endPtP->angle = GetTrkEndAngle(trk,ep);
					/* These End Points are dummies --
					   we don't want DeleteTrack to look at
					   them. */
					endPtP->track = NULL;
					endPtP->index = (trk1?GetEndPtConnectedToMe(trk1,trk):-1);
					endPtOrig.x += endPtP->pos.x;
					endPtOrig.y += endPtP->pos.y;
				}
				else {
					endPtP->track = trk1;   //Found this one
				}
			} else {
				if ( trk1 ) IsConnectedP = TRUE;        //Not an end - at least one connection for
			}
		}
	}
	int openEnds = 0;
	for ( epN = 0; epN < tempEndPts_da.cnt; epN++ ) {
		endPtP = &DYNARR_N(trkEndPt_t,tempEndPts_da,epN);
		if ( ! endPtP->track ) openEnds++;   //Not connected end
	}
	if ( openEnds>validEnds ) return FALSE;  //Too many - means isolated track groups
	return TRUE;
}

// called by FreeTrack
static void DeleteBlock( track_p t )
{
    track_p trk1;
    blockData_p xx1, xx;

    if ( ! t || IsTrackDeleted( t ) ) return;
    xx = GetblockData(t);
    MyFree(xx->name); xx->name = NULL;
    MyFree(xx->script); xx->script = NULL;

    if ( first_block == t )
        first_block = xx->next_block;
    trk1 = first_block;
    while(trk1) {
        if ( IsTrackDeleted(trk1) ) break;
        xx1 = GetblockData (trk1);
        if ( xx1->next_block == t ) {
            xx1->next_block = xx->next_block;
            break;
        }
        trk1 = xx1->next_block;
    }
    if ( t == last_block )
        last_block = trk1;
}

static BOOL_T WriteBlock( track_p t, FILE * f )
{
	BOOL_T rc = TRUE;
	wIndex_t iTrack;
	blockData_p xx = GetblockData(t);
	char *blockName = MyStrdup(xx->name);

#ifdef UTFCONVERT
	blockName = Convert2UTF8(blockName);
#endif // UTFCONVERT

	// Skip Dynamic Blocks
	if ( blockName[0] == 0 ) return TRUE;

	xx->AutoGenerated = FALSE;

	rc &= fprintf(f, "BLOCK %d \"%s\" \"%s\"\n",
		GetTrkIndex(t), blockName, xx->script)>0;
	rc &= WriteEndPt( f, t, 0 );
	rc &= WriteEndPt( f, t, 1 );
	for ( iTrack = 0; iTrack < xx->numTracks && rc; iTrack++ ) {
                if ( (&(xx->trackList))[iTrack].t == NULL ) continue;
		rc &= fprintf(f, "\tTRK %d\n",
				GetTrkIndex((&(xx->trackList))[iTrack].t))>0;
	}
	rc &= fprintf( f, "\t%s\n", END_BLOCK )>0;
	MyFree(blockName);
	return rc;
}

// blockName - preloaded with block name
// blockScript - preloaded with block script
// blockLen - preloaded length of block
// blockTrk_da - DYNARR pre loaded with list of track segments
// tempEndPts_da - DYNARR pre loaded with 2 endpoints of block
//                 prevTrack is track at end of block, track is NULL.
static track_p makeBlock( void )
{
	track_p blk;
	trkEndPt_p endPtP;
	EPINX_T ep;

	/*blockCheckContigiousPath(); save for ResolveBlockTracks */
	blk = NewTrack( 0, T_BLOCK, tempEndPts_da.cnt,
		sizeof(blockData_t)+(sizeof(btrackinfo_t)*(blockTrk_da.cnt))+1 );
#if 0
	LOG( log_block, 1, ("*** makeBlock: B%d\n",GetTrkIndex(blk)))
#endif

	initBlockData( blk );
//	blockDebug( blk );

	return blk;
}

static BOOL_T ReadBlock( char * line )
{
	TRKINX_T trkindex;
	wIndex_t index;
	track_p trk, trk1;
	char * cp = NULL;
	blockData_p xx,xx1;
	wIndex_t iTrack;
	EPINX_T ep;
	trkEndPt_p endPtP, e;
	char *name, *script;
	BOOL_T drop_block = FALSE;

	LOG( log_block, 1, ("*** ReadBlock: line is '%s'\n",line))
	if ( ! GetArgs( line+6, "dqq", &index, &name, &script ) ) {
		return FALSE;
	}

#ifdef UTFCONVERT
	ConvertUTF8ToSystem(name);
#endif // UTFCONVERT

	strncpy(blockName, name, STR_SHORT_SIZE);
	strncpy(blockScript, script, STR_LONG_SIZE);
#if 0
	LOG( log_block, 1, ("*** ReadBlock: index %d name %s script %s\n",
		index, blockName, blockScript))
#endif

	if ( cp )
		GetArgs( cp, "p", &description_offset );

	blockLen = 0.0;
	DYNARR_RESET( trkEndPt_t, tempEndPts_da );
	DYNARR_RESET( btrackinfo_t , blockTrk_da );
	while ( (cp = GetNextLine()) != NULL ) {
		while (isspace((unsigned char)*cp)) cp++;
		if ( strncmp( cp, "END", 3 ) == 0 ) {
			break;
		}
		if ( *cp == '\n' || *cp == '#' ) {
			continue;
		}
		if ( strncmp( cp, "T4", 2 ) == 0 ) {
			DYNARR_APPEND( trkEndPt_t, tempEndPts_da, 2 );
			e = &tempEndPts(tempEndPts_da.cnt-1);
			if ( !GetArgs( cp+3, "dc", &trkindex, &cp ) ) return FALSE;
			if ( !GetArgs( cp, "pfc", &e->pos, &e->angle, &cp) ) return FALSE;
			e->prevTrack = FindTrack(trkindex);
#if 0
			LOG( log_block, 1, ("*** ReadBlock: ep%d index %d trk %p\n",
					tempEndPts_da.cnt-1,trkindex,e->prevTrack))
#endif
		}
		if ( strncmp( cp, "TRK", 3 ) == 0 ) {
			if ( ! GetArgs( cp+4, "d", &trkindex ) ) return FALSE;
			trk = FindTrack(trkindex);
#if 0
			LOG( log_block, 1, ("*** ReadBlock: T%d(%p) eps %d\n",
					trkindex, trk, GetTrkEndPtCnt(trk)))
#endif
			if ( trk ) {
				DYNARR_APPEND( btrackinfo_t, blockTrk_da, 129 );
				blockTrk(blockTrk_da.cnt-1).i = trkindex;
				blockTrk(blockTrk_da.cnt-1).t = trk;
			}
			// drop block if not valid
			if ( GetTrkEndPtCnt(trk) != 2 )
				drop_block = TRUE;
			if ( trk->endPt[0].index < 0 || trk->endPt[1].index < 0 )
				drop_block = TRUE;
			blockLen += GetTrkLength( trk, 0, 1 );
		}
	}

#if 0
	LOG( log_block, 1, ("*** ReadBlock: T%d len %0.1f drop %d\n",
			trkindex, blockLen, drop_block))
#endif
	if ( drop_block )
		return TRUE;
	if ( blockLen < GetLayoutMinBlockLength() )
		return TRUE;
	blockLen = 0.0;

	if ( ! blockUndoStarted ) {
		UndoStart( _("Create block"), "Create block" );
		blockUndoStarted = TRUE;
	}

	makeBlock();

	if ( blockUndoStarted ) {
		UndoEnd();
		blockUndoStarted = FALSE;
	}

	return TRUE;
}

static void pushEp( track_p trk, EPINX_T ep )
{
	trkEndPt_p endPtP;

	DYNARR_APPEND( trkEndPt_t, tempEndPts_da, 2 );
	endPtP = &tempEndPts(tempEndPts_da.cnt-1);
	memset( endPtP, 0, sizeof *endPtP );
	endPtP->trackEp = ep;
	endPtP->pos = GetTrkEndPos(trk,ep);
	endPtP->angle = GetTrkEndAngle(trk,ep);
	endPtP->prevTrack = trk;
#if 0
	LOG( log_block, 2, ("*** pushEp(): trk T%d-%d, cnt %d track T%d  angle %0.1f pos %0.3f %0.3f\n",
		GetTrkIndex(trk),ep,tempEndPts_da.cnt,GetTrkIndex(endPtP->prevTrack),endPtP->angle,
		endPtP->pos.x,endPtP->pos.y))
#endif
}

static void pushDa( track_p trk )
{
	// Add the segment to the list
	DYNARR_APPEND( btrackinfo_t, blockTrk_da, 129 );
	blockTrk(blockTrk_da.cnt - 1).t = trk;
	blockTrk(blockTrk_da.cnt - 1).i = GetTrkIndex(trk);
#if 0
	LOG( log_block, 2, ("*** pushDa(): T%d, cnt %d\n",GetTrkIndex(trk),blockTrk_da.cnt))
#endif
}

// Recursively goes from track segment to track segment until
// a turnout or track end is encountered. Save the endPt at
// the turnout.
// When dynamic is set collect turnouts between two blocks.
// Place tracks in DYNARR blockTrk_da
// Place endpoints in tempEndPts_da
// Place total length in blockLen
static void addSegs( track_p here, track_p from, EPINX_T epFrom )
{
	EPINX_T epCnt, epN;
	track_p epTrk;

#if 0
	LOG( log_block, 2, ("*** addSegs(): here T%d from T%d  epFrom %d\n",
		here?GetTrkIndex(here):0,from?GetTrkIndex(from):0,epFrom))
#endif
	if ( ! IsTrack( here ) || GetTrkType( here ) == T_TURNTABLE ) return;

	epCnt = GetTrkEndPtCnt(here);
	if ( here == from ) {
		blockLen = GetTrkLength( here, 0, 1 );
		if ( epCnt == 2 && ((from)->endPt)[0].track != here &&
				((from)->endPt)[1].track != here ) {
#if 0
			LOG( log_block, 2, ("*** addSegs(): adding track T%d\n",GetTrkIndex(here)))
#endif
			pushDa( here );
			addSegs( ((from)->endPt)[0].track, here, epFrom );
			addSegs( ((from)->endPt)[1].track, here, epFrom );
		}
		return;
	}

	// See if this is a turnout or a block
	// Check for the end of the scan. For a block its a turnout, for dynamic
	// its a turnout with toBlock set.
	if ( epCnt > 2 || here->conBlock ) { // The from seg is one end of the block
#if 0
		LOG( log_block, 2, ("*** addSegs(): switch/block at T%d epCnt %d conBlock %p, B%d\n",
			GetTrkIndex(here),epCnt,here->conBlock, here->conBlock?GetTrkIndex(here->conBlock):0))
#endif
		epN = from->endPt[0].track == here?0:1;

		pushEp( from, epN );
		return;
	}

	// "here" is a segment of the block
	// Add it to the length of the block
	if ( epCnt == 2 ) {
		blockLen += GetTrkLength( here, 0, 1 );

		pushDa( here );
#if 0
		LOG( log_block, 2, ("*** addSegs(): adding track T%d\n",GetTrkIndex(here)))
#endif

		for ( epN = 0; epN < epCnt; epN++ ) {
			epTrk = here->endPt[epN].track;
			if ( epTrk && epTrk != from ) break;
		}
		if ( epN < epCnt )
		    addSegs( epTrk, here, epN );
		return;
	}
}

// Get ep in "trk1" connected to "trk2"
// not found -- ep == GetTrkEndPtCnt(trk1)
static EPINX_T getEpInTrk1ToTrk2( track_p trk1, track_p trk2 )
{
	EPINX_T ep;
	track_p epTrk;

	for ( ep = 0; ep < GetTrkEndPtCnt(trk1); ep++ ) {
		epTrk = trk1->endPt[ep].track;
		if ( epTrk && epTrk == trk2 ) break;
	}
	return ep;
}

// Recursively goes from turnout to turnout until
// a turnout with toTrack is encountered. When toBlock is encountered
// save the endPt at the turnout.
// Place tracks in DYNARR blockTrk_da
// Place endpoints in tempEndPts_da
// Place total length in blockLen
static void addTurnouts( track_p here, track_p from, EPINX_T epFrom )
{
	track_p epTrk;
	EPINX_T epCnt, epN, epR, epTo;

	if ( ! IsTrack( here ) || GetTrkType( here ) == T_TURNTABLE ) return;

#if 0
	LOG( log_block, 2, ("*** addTurnouts(): here T%d from T%d-%d\n",
			GetTrkIndex(here),GetTrkIndex(from),epFrom))
#endif
	epCnt = GetTrkEndPtCnt(here);
	if ( here == from ) { // this is a turnout, epTo is the other end of the path from epFrom
		epTo = GetRemoteEp( from, epFrom );
		pushDa( here );
		blockLen = 0.0;
		if ( here->endPt[epTo].toBlock  ) {
			pushEp( here, epTo );
		}
		if ( ! here->endPt[epTo].toTrack ) { // keep going down path...
			epTrk = here->endPt[epTo].track;
			addTurnouts(epTrk, here, epTo);
		}
		if ( here->endPt[epFrom].toBlock ) {
			pushEp( here, epFrom );
		}
		if ( ! here->endPt[epFrom].toTrack ) {
			epTrk = here->endPt[epFrom].track;
			addTurnouts(epTrk, here, epFrom);
		}
#if 0
		LOG( log_block, 2, ("*** addTurnouts(A): exiting\n"))
#endif
		return;
	}

	// Get ep in "here" connected to "from"
	// here[ep].track == from
	// epN = ep
	epN = getEpInTrk1ToTrk2( here, from );

	epR = GetRemoteEp( here, epN );
	if ( epR == epN ) {
#if 0
		LOG( log_block, 2, ("*** addTurnouts(B): endpoint open T%d-%d\n",
			GetTrkIndex(here),epR))
#endif
		return;
	}

	if ( epCnt > 2 && here->endPt[epR].toBlock ) {
		pushEp( here, epR );
		pushDa( here );
#if 0
		LOG( log_block, 2, ("*** addTurnouts(C): end of scan toBlock"))
#endif
		return;
	}
	// At end of scan
	if ( epCnt > 2 && here->endPt[epR].toTrack ) {
#if 0
		LOG( log_block, 2, ("*** addTurnouts(D): end of scan toTrack"))
#endif
		return;
	}

	// "here" is a segment of the block
	// Add it to the length of the block
	if ( epCnt == 2 )
		blockLen += GetTrkLength( here, epR, epN );

	// Add the segment to the list
	pushDa( here );

	epTrk = here->endPt[epR].track;
	addTurnouts( epTrk, here, epN );
#if 0
		LOG( log_block, 2, ("*** addTurnouts(E): exiting\n"))
#endif
}


// Given any track segment in a block
// Get the set of segments that are in the block. Place in blockTrk_da
// Place endpoints in tempEndPts_da
// Place total length in blockLen
// If something is invalid, reset blockTrk_da
// If blocklen is > 2*maxBlocklen make block from tempEndPts_da[0].prevTrack
// for maxBlockLength. If remaining len is < maxBlockLength, add rest of segs
// to block. Adjust parameters as required.
static void GetBlockSegs( track_p s_trk )
{
	DIST_T len;
	track_p lastTrk = s_trk, trk = s_trk;
	trkEndPt_p endPtP;
	EPINX_T ep, lastEp;

#if 0
	LOG( log_block, 2, ("*** GetBlockSegs(): T%d\n", GetTrkIndex(trk)))
#endif

	if ( ! IsTrack( trk ) ) return;

	// Length of block
	blockLen = 0.0;
	// Endpoints of block
	DYNARR_RESET( trkEndPt_t, tempEndPts_da );
	// Track segments in block
	DYNARR_RESET( btrackinfo_t, blockTrk_da );

	// Find all track segments between turnouts/blocks in this block
	// trk is a track segment
	addSegs( trk, trk, 0 );

#if 0
	LOG( log_block, 2, ("*** GetBlockSegs(): T%d  eps %d, tr %d len %0.2f\n",
		GetTrkIndex(trk), tempEndPts_da.cnt, blockTrk_da.cnt,blockLen))
#endif
}

// gather the segs for the sub-block
static void GetSubBlockSegs( track_p trk, EPINX_T ep, DIST_T len )
{
	track_p lastTrk = trk;
	EPINX_T lastEp = GetRemoteEp( trk, ep );
	DIST_T testLen = len;

	if ( ! IsTrack( trk ) ) return;

	// Length of block
	blockLen = 0.0;
	// Endpoints of block
	DYNARR_RESET( trkEndPt_t, tempEndPts_da );
	// Track segments in block
	DYNARR_RESET( btrackinfo_t, blockTrk_da );

	if ( testLen > 2.0 * GetLayoutMaxBlockLength() )
		testLen = GetLayoutMaxBlockLength();

#if 0
	LOG( log_block, 2, ("*** GetSubBlockSegs() T%d-%d  len %0.2f\n",
		GetTrkIndex(trk),ep,len))
#endif
	// make a sub-block
	pushEp( trk, ep );
	ep = GetRemoteEp( trk, ep );
	while ( blockLen <= testLen ) {
#if 0
	LOG( log_block, 2, ("*** GetSubBlockSegs() T%d-%d  blockLen %0.2f testLen %0.2f\n",
		GetTrkIndex(trk),ep,blockLen,testLen))
#endif
		if ( ! trk ) break;
		if ( GetTrkEndPtCnt( trk ) > 2 ) break;
		if ( trk->conBlock ) break;
		pushDa( trk );
		blockLen += GetTrkLength( trk, 0, 1 );
		if ( len - blockLen < GetLayoutMaxBlockLength() )
			testLen = len;
		lastTrk = trk;
		lastEp = ep;
		// on to the next seg...
		trk = lastTrk->endPt[ep].track;
		ep = lastTrk->endPt[ep].trackEp;
		ep = GetRemoteEp( trk, ep );
	}
	pushEp( lastTrk, lastEp );
}

static void GetDynamicSegs( track_p trk, EPINX_T ep )
{
#if 0
	LOG( log_block, 1, ( "*** GetDynamicSegs T%d\n", GetTrkIndex(trk)))
#endif

	// Length of block
	blockLen = 0.0;
	// Endpoints of block
	DYNARR_RESET( trkEndPt_t, tempEndPts_da );
	// Track segments in block
	DYNARR_RESET( btrackinfo_t, blockTrk_da );

	if ( ! IsTrack( trk ) || GetTrkType( trk ) == T_TURNTABLE ) return;

	// Find all turnouts between two blocks
	addTurnouts( trk, trk, ep );
}

// blk[ep].prevTrack (blk[ep].prevTrack[blk[ep].trackEp] is the track and ep at the
// end of the block that connects with the next block.
// trkN[epN} track is end of next block connected to this block
static void getDispTrkEp( track_p blk, EPINX_T ep, EPINX_T *epB, track_p *trkN, EPINX_T *epN )
{
	track_p tmpTrk;
	EPINX_T tmpEp;

	*epB = *epN = 0;
	*trkN = NULL;

        // blk[ep].prevTrack is the track (tmpTrk) at the end of the block
	// blk[ep].trackEP is the ep (tmpEp) to the next block (tmpTrk[tmpEP])
        tmpTrk = blk->endPt[ep].prevTrack;
        tmpEp = blk->endPt[ep].trackEp;
	*epB = tmpEp;
	// *trkN is the track at the end of the next block that points to blk
        *trkN = tmpTrk->endPt[tmpEp].track;
	if ( ! *trkN ) return;
        for ( tmpEp = 0; tmpEp < GetTrkEndPtCnt(tmpTrk); tmpEp++ ) {
            if ( (*trkN)->endPt[tmpEp].track == tmpTrk ) break	;
        }
	*epN = tmpEp;
}

// The needed data is in the static areas
// tempEndPts_da - DYNARR preloaded with endpoints
// blockName - preloaded with block name
// blockScript - preloaded with block script
// blockLen - preloaded length of block
void initBlockData( track_p b_trk )
{
	blockData_p xx, xx1;
	track_p trk, trk0, trk1;
	wIndex_t iTrack;
	EPINX_T ep, ep0, ep1, epB0, epB1;
	trkEndPt_p endPtP;

	// This suppresses displaying the block description
	// SetTrkBits( b_trk, TB_HIDEDESC);

	b_trk->endCnt = tempEndPts_da.cnt;
	for ( ep = 0; ep < tempEndPts_da.cnt; ep++ ) {
		endPtP = &tempEndPts(ep);
		b_trk->endPt[ep].prevTrack = endPtP->prevTrack;
		b_trk->endPt[ep].trackEp = endPtP->trackEp;
		b_trk->endPt[ep].pos = endPtP->pos;
		b_trk->endPt[ep].angle = endPtP->angle;
		b_trk->endPt[ep].option = 0;
	}
	// b_trk-ep[0] connects with this segment at ep0
	getDispTrkEp( b_trk, 0, &epB0, &trk0, &ep0 );
	// b_trk-ep[1] connects with this segment at ep1
	getDispTrkEp( b_trk, 1, &epB1, &trk1, &ep1 );

#if 0
	LOG( log_block, 1, ( "*** initBlockData: T%d-%d -- 0(T%d-%d)-B%d-(T%d-%d)1 -- T%d-%d\n",
		trk0?GetTrkIndex(trk0):0,ep0,
		GetTrkIndex(b_trk->endPt[0].prevTrack),epB0,
		GetTrkIndex(b_trk),
		GetTrkIndex(b_trk->endPt[1].prevTrack),epB1,
		trk1?GetTrkIndex(trk1):0,ep1))
#endif

	if ( blockName[0] != 'D' )
		sprintf( blockName,"B%03d", GetTrkIndex( b_trk ) );
	else
		blockName[0] = 0; // Dynamic blocks
	xx = GetblockData( b_trk );
	xx->name = MyStrdup( blockName );
	blockName[0] = 0;
	xx->script = MyStrdup( blockScript );
	blockScript[0] = 0;
	xx->IsHilite = FALSE;
	xx->AutoGenerated = FALSE;
	xx->description_offset = zero;
	trk1 = last_block;
	if ( ! trk1 || IsTrackDeleted( trk1 ) ) {
		first_block = b_trk;
	}
	else {
		xx1 = GetblockData(trk1);
		xx1->next_block = b_trk;
	}
	xx->next_block = NULL;
	last_block = b_trk;

	xx->numTracks = blockTrk_da.cnt;
	b_trk->occupied = 0;
	blockLen = 0;
#if 0
	LOG( log_block, 1, ( "*** initBlockData: B%d --\"%s\"-- ", GetTrkIndex(b_trk),xx->name) )
#endif
	for ( iTrack = 0; iTrack < blockTrk_da.cnt; iTrack++ ) {
		tracklist(iTrack).i = blockTrk(iTrack).i;
		tracklist(iTrack).t = blockTrk(iTrack).t;
		blockLen += GetTrkLength( blockTrk(iTrack).t, 0, 1 );
		if ( tracklist(iTrack).t ) {
			tracklist(iTrack).t->conBlock = b_trk;
			b_trk->occupied += tracklist(iTrack).t->occupied;
		}
#if 0
		LOG( log_block, 1, ( " T%d", tracklist(iTrack).i) )
#endif
	}
#if 0
	LOG( log_block, 1, ( "  -- blockLen %0.2f\n", blockLen) )
#endif
	xx->blkLength = blockLen;
}

// The type is T_BLOCK and the name is ""
EXPORT BOOL_T IsDynamicBlock( track_p blk )
{
	blockData_p xx;

	if ( ! blk || IsTrackDeleted( blk ) || GetTrkType(blk) != T_BLOCK ) return FALSE;

	xx = GetblockData( blk );
	if ( ! xx->name || xx->name[0] != 0 ) return FALSE;

	return TRUE;
}

// Get the block that trk, pos reference
// blk->endPt[?].{prevTrack, pos} is the track segment and endPt of the block
//
static track_p getNextBlock( track_p trk, coOrd pos )
{
	EPINX_T ep;

	for ( ep = 0; ep < GetTrkEndPtCnt(trk); ep++ ) {
		if ( IsPosClose( trk->endPt[ep].pos, pos ) ) break;
	}
#if 0
	LOG(log_block, 2, ("*** getNextBlock(): T%d (%0.2f %0.2f) ep %d  ",
			GetTrkIndex(trk),pos.x,pos.y,ep))
#endif
	trk = trk->endPt[ep].track;
#if 0
	LOG(log_block, 2, (" --- T%d (%0.2f %0.2f)  B%d\n",
		trk?GetTrkIndex(trk):0,trk?trk->endPt[ep].pos.x:0.0,
		trk?trk->endPt[ep].pos.y:0.0,
		trk&&trk->conBlock?GetTrkIndex(trk->conBlock):0))
#endif
	return trk?trk->conBlock:NULL;
}

// Given a block and a dynamic block, get the other block
// (A dynamic block connects two blocks.)
// end of car is on blk
// used to be in the dynamic block dynBlk
EXPORT track_p GetRemoteBlock( track_p blk, track_p dynBlk )
{
	track_p  blk0, blk1;
	if ( ! blk || ! dynBlk ) return NULL;

	if ( ! IsDynamicBlock( dynBlk ) ) return NULL;

	blk0 = getNextBlock( dynBlk->endPt[0].prevTrack, dynBlk->endPt[0].pos );
	blk1 = getNextBlock( dynBlk->endPt[1].prevTrack, dynBlk->endPt[1].pos );
#if 0
	LOG(log_block, 2, ("*** GetRemoteBlock(): B%d, DB%d B%d - B%d\n",
			GetTrkIndex(blk0),
			GetTrkIndex(blk),
			GetTrkIndex(blk1),GetTrkIndex(blk0 == blk ? blk1 : blk0)))
#endif
	return blk0  == blk ? blk1 : blk0;
}

EXPORT BOOL_T ResolveBlockTrack( track_p b_trk )
{
    blockData_p xx;
    track_p t_trk;
    wIndex_t iTrack;
    EPINX_T ep;
    trkEndPt_p endPtP;
    int rc =0;
    int first = -1;

    if ( IsTrackDeleted( b_trk ) || GetTrkType(b_trk) != T_BLOCK ) return TRUE;
    LOG( log_block, 2, ("*** ResolveBlockTrack(B%d)\n",GetTrkIndex(b_trk)))

    xx = GetblockData( b_trk );
    for ( iTrack = 0; iTrack < xx->numTracks; iTrack++ ) {
        /* For all tracks in the block, set the block pointer, conBlock, on the track.  */
        t_trk = FindTrack( tracklist(iTrack).i );
        if ( t_trk == NULL ) { // track is gone, remove reference
            tracklist(iTrack).i = 0;
            continue;
        }
        if ( ! IsTrack( t_trk ) ) { // t_trk is not a track, remove reference
            tracklist(iTrack).i = 0;
            tracklist(iTrack).t = NULL;
            continue;
        }
        tracklist(iTrack).t = t_trk;
        if ( t_trk->conBlock == b_trk )
            t_trk->conBlock = NULL;
        if ( first < 0 ) first = iTrack;
    }
    if ( first < 0 ) {
        // No segs in block
        xx->numTracks = 0;
//      blockDebug( b_trk );

        DeleteTrack( b_trk, FALSE ); // calls DeleteBlock

//      SetBlockBoundingBox(b_trk);
        return FALSE;
    }
    else {
        // This track is the first in the block
        t_trk = tracklist(first).t;

        // Capture all tracks between turnouts into this block
        // t_trk is a track segment, b_trk is the controlBlock
        GetBlockSegs( t_trk );
        if ( blockTrk_da.cnt == 0 ) {
            DeleteTrack( b_trk, FALSE );

            return FALSE;
        }
        if ( blockLen < GetLayoutMinBlockLength() ) {
            DeleteTrack( b_trk, FALSE );

            return FALSE;
        }

        xx->blkLength = 0.0;
        for ( iTrack = 0; iTrack < blockTrk_da.cnt; iTrack++ ) {
            tracklist(iTrack).i = blockTrk(iTrack).i;
            tracklist(iTrack).t = blockTrk(iTrack).t;
            blockTrk(iTrack).t->conBlock = b_trk;
            xx->blkLength += GetTrkLength( blockTrk(iTrack).t, 0, 1 );
        }
        xx->numTracks = blockTrk_da.cnt;
        for ( ep = 0; ep < tempEndPts_da.cnt; ep++ ) {
            endPtP = &tempEndPts(ep);
            b_trk->endPt[ep].prevTrack = endPtP->prevTrack;
            b_trk->endPt[ep].pos = endPtP->pos;
            b_trk->endPt[ep].angle = endPtP->angle;
            b_trk->endPt[ep].option = 0;
        }
//      blockDebug( b_trk );
//      SetBlockBoundingBox(trk);
    }

    if ( ! blockCheckContigiousPath( FALSE ) ) {
        if ( NoticeMessage( _("resolveBlockTrack(B%d): is not continuous"),
                    _("Continue"), NULL, GetTrkIndex(b_trk)) ) {
            exit(4);
        } else {
            rc = 4;
        }
    }

    return (rc==0);
}

// Go through all "Blocks" and refresh the segments in the block
// Call this before entering train mode
EXPORT void UpdateBlockTrack( void )
{
    track_p trk;

    LOG( log_block, 1, ("*** UpdateBlockTrack -- enter\n"))

    // Set turnout flags (toBlock, toTrack) as needed
    SetTurnoutFlags();

    // Go through blocks and update the segments
    TRK_ITERATE(trk) {
        if ( GetTrkType(trk) == T_BLOCK ) {
            ResolveBlockTrack( trk );
        }
    }
    LOG( log_block, 1, ("*** UpdateBlockTrack -- exit\n"))

}

EXPORT void AddMissingBlockTrack( void )
{
    track_p trk, blk, startTrk;
    blockData_p xx;
    trkEndPt_p endPtP;
    DIST_T totLen;
    EPINX_T startEp;

    LOG( log_block, 1, ("*** AddMissingBlockTrack(1) -- enter\n"))

    SetupTurnouts();

    // Loop through all track segs that are not in blocks
    // Create blocks for them when they are long enough and don't end at a track end.
    TRK_ITERATE(trk) {
        if ( ! IsTrack(trk) ) continue;
#if 0
        LOG( log_block, 2, ("*** AddMissingBlockTrack(2) next seg T%d  EndPtCnt %d",
                GetTrkIndex(trk),GetTrkEndPtCnt(trk)))
        if ( trk->conBlock )
            LOG( log_block, 1, ("        B%d", GetTrkIndex(trk->conBlock)))
        LOG( log_block, 1, ("\n"))
#endif

        // seg already in a block
        if ( GetTrkEndPtCnt(trk) != 2 ) continue;
        if ( trk->endPt[0].index < 0 || trk->endPt[1].index < 0 ) continue;
        if ( trk->conBlock != NULL ) continue;
        GetBlockSegs( trk );
        if ( blockTrk_da.cnt <= 0 ) continue;
        if ( tempEndPts_da.cnt < 2 ) continue;
        endPtP = &tempEndPts(0);
#if 0
        LOG( log_block, 2, ("*** AddMissingBlockTrack(3) T%d  blockLen %0.2f (min %0.2f max %0.2f) segs %d Start T%d\n",
                GetTrkIndex(trk),blockLen,GetLayoutMinBlockLength(),
                GetLayoutMaxBlockLength(),blockTrk_da.cnt,GetTrkIndex(endPtP->prevTrack)))
#endif
        if ( blockLen < GetLayoutMinBlockLength() ) continue;

        startTrk = endPtP->prevTrack;
        startEp = endPtP->trackEp;
        totLen = blockLen;

        while ( totLen > 0.01 ) {
#if 0
            LOG( log_block, 2, ("*** AddMissingBlockTrack(4) T%d  totLen %0.2f Start T%d-%d\n",
                    GetTrkIndex(trk),totLen,GetTrkIndex(startTrk),startEp))
#endif

            // Gather segs for the block
            GetSubBlockSegs( startTrk, startEp, totLen );
#if 0
            LOG( log_block, 2, ("*** AddMissingBlockTrack(5) T%d  blockLen %0.2f eps %d segs %d\n",
                    GetTrkIndex(trk),blockLen,tempEndPts_da.cnt,blockTrk_da.cnt))
#endif
            if ( totLen <= 0.0 || blockLen <= 0.0 ) break;

            // build the block
            if ( ! blockUndoStarted ) {
               UndoStart( _("Create block"), "Create block" );
               blockUndoStarted = TRUE;
            }

            blockScript[0] = 0;
            sprintf( blockName,"B%03d", GetTrkIndex( startTrk ) );
            SetTrkBits( blockTrk(0).t, TB_SELECTED );
            blk = makeBlock();
            ClrTrkBits( blockTrk(0).t, TB_SELECTED );
            if ( blk ) {
                xx = GetblockData(blk);
                xx->AutoGenerated = TRUE;
            }

            if ( blockUndoStarted ) {
                UndoEnd();
                blockUndoStarted = FALSE;
            }

            // On to the next sub block
            totLen -= blockLen;
            endPtP = &tempEndPts(1);
            startTrk = endPtP->prevTrack->endPt[endPtP->trackEp].track;
            startEp = endPtP->prevTrack->endPt[endPtP->trackEp].trackEp;
        }
    }
    verifyOccupancy( FALSE, TRUE );
#if 0
    verifyTrackOccupancy( TRUE );
#endif
    LOG( log_block, 1, ("*** AddMissingBlockTrack(6) -- exit\n"))
    MainRedraw();
}

// Delete all auto generated blocks
EXPORT void DeleteAllBlockTrack( void )
{
    track_p blk;
    blockData_p xx;

    LOG( log_block, 1, ("*** DeleteAllBlockTrack() -- enter\n"))

    // Loop through all track segs
    TRK_ITERATE(blk) {
        if ( IsTrackDeleted(blk) || GetTrkType(blk) == T_BLOCK ) {
            LOG( log_block, 1, ("*** DeleteAllBlockTrack() next block B%d\n",
                GetTrkIndex(blk)))
            xx = GetblockData(blk);
            if ( ! xx->AutoGenerated ) continue;

            CheckDeleteBlock( blk );
        }
    }
}

static void deleteDynamicBlock( track_p trk )
{
	track_p blk = NULL, trkT;

	if ( trk->conBlock ) blk = trk->conBlock;
	if ( ! blk || GetTrkType(blk) != T_BLOCK ) return;
#if 0
	LOG( log_block, 1, ("*** deleteDynamicBlock T%d B%d\n", GetTrkIndex(trk), GetTrkIndex(blk)))
#endif

	// clean up attached ends
	if ( blk->endPt[0].attached ) {
		blk->endPt[0].attached = FALSE;
		trkT = blk->endPt[0].prevTrack->endPt[blk->endPt[0].trackEp].track;
		if (  trkT && trkT->conBlock && trkT->conBlock->occupied > 0 )
			trkT->conBlock->occupied--;
	}
	if ( blk->endPt[1].attached ) {
		blk->endPt[1].attached = FALSE;
		trkT = blk->endPt[1].prevTrack->endPt[blk->endPt[1].trackEp].track;
		if (  trkT && trkT->conBlock && trkT->conBlock->occupied > 0 )
			trkT->conBlock->occupied--;
	}

	CheckDeleteBlock( blk );
#if 0
	verifyOccupancy( FALSE, TRUE );
#endif
}

// Create a Dynamic block that starts at trk[ep]
// The block does not have a name or script
// The two end points mark the end track (prevTrack) and ep (trackEp) of the block.
static track_p  createDynamicBlock( track_p trk, EPINX_T ep )
{
	track_p blk = NULL, blkR, trk0;
	EPINX_T epB0, ep0;

	if ( ! IsTrack( trk ) ) return NULL;
	if ( GetTrkEndPtCnt(trk) <= 2 ) return NULL;  // This is not a turnout
	if ( trk->conBlock ) return NULL; // Dynamic block already exists

#if 0
	LOG( log_block, 1, ("*** createDynamicBlock T%d-%d -- enter\n", GetTrkIndex(trk),ep))
#endif

	// Get the turnouts in the path
	blockScript[0] = 0;
	blockName[0] = 'D'; // Dynamic block gets temp name "D", will ve changed to ""
	blockName[1] = 0;
	GetDynamicSegs( trk, ep ); // trk[ep] is closest to prevBlock
	if ( tempEndPts_da.cnt < 2 ) {
#if 0
		LOG( log_block, 1, ("*** createDynamicBlock T%d epCnt %d - Must end at a block\n",
					GetTrkIndex(trk),tempEndPts_da.cnt))
#endif
		return NULL; // doesn't end at a block
	}

	if ( ! blockUndoStarted ) {
		UndoStart( _("Create block"), "Create block" );
		blockUndoStarted = TRUE;
	}

	blk = makeBlock();


	if ( blockUndoStarted ) {
		UndoEnd();
		blockUndoStarted = FALSE;
	}

#if 0
	LOG( log_block, 1, ("*** createDynamicBlock T%d -- B%d -- exit\n",
		GetTrkIndex(trk),blk?GetTrkIndex(blk):0))
#endif

#if 0
	verifyOccupancy( FALSE, TRUE );
#endif
	return blk;
}

// Delete all dynamic blocks
// When leaving train mode
EXPORT void ClearDynamicBlocks( void )
{
    track_p trk;

    LOG( log_block, 1, ("*** ClearDynamicBlocks -- enter\n"))

    TRK_ITERATE(trk) {
        if ( trk->conBlock && IsDynamicBlock( trk->conBlock ) )
            deleteDynamicBlock( trk );
    }

    LOG( log_block, 1, ("*** ClearDynamicBlocks -- exit\n"))
}


/*
 * BLOCK OCCUPANCY
 */

// Clear all segment and block occupied indicators
// When the indicator is not zero the segment is occupied.
EXPORT void ClearOccupied( void )
{
    track_p trk;
    for (trk=NULL; TrackIterate(&trk);) {
        trk->occupied = 0;
    }
}

EXPORT  BOOL_T IsOccupied ( track_p trk )
{
	if ( ! trk ) return FALSE;
	if ( trk->conBlock && trk->conBlock->occupied > 0 ) return TRUE;
	if ( trk->occupied > 0 ) return TRUE;

	return FALSE;
}



// The segment occupied indicator maintains a count of the number
// of car ends (trucks) on the segment. The block, when present,
// maintains a count for all segments in the block. Static blocks
// are created before entering train mode. Dynamic blocks are created,
// and exist, when turnouts are positioned to connect two static blocks
// and a train occupies any of the turnouts. Since Dynamic blocks
// do not report occupancy, they are attached (counted as part of) an
// abutting block.

//  set attached for a newly created dynamic block
static void setAttached( track_p blkD, track_p car )
{
	track_p blk = car->conBlock;

	// one end of the car is in the new Dynamic block the
	// other isn't (unless this is in setupOccupied)
	if ( blk == blkD ) {
		blk = blkD->endPt[0].prevTrack->endPt[blkD->endPt[0].trackEp].track->conBlock;
		blk->occupied++;
		blkD->endPt[0].attached = TRUE;
	} else {
		blk->occupied++;
		if ( blkD->endPt[0].prevTrack->endPt[blkD->endPt[0].trackEp].track->conBlock == blk )
			blkD->endPt[0].attached = TRUE;
		else
			blkD->endPt[1].attached = TRUE;
	}
}

// When a block's occupancy goes from 0 to 1 a car has entered the block.
// If the car came from a Dynamic block, change attached to the new block.
// trk - track car is on - not in a dynamic block
// prev - track somewhere behand the car - in a dynamic block
static void transferAtached( track_p trk, track_p prev )
{
	track_p blkN = trk->conBlock, blkD = prev->conBlock, blkR0, blkR1, trkT;
	EPINX_T epN = -1, epR = -1;

#if 0
	LOG(log_block, 2, ("transferAtached(1)  trk T%d  prev T%d\n",
		trk?GetTrkIndex(trk):0,prev?GetTrkIndex(prev):0))
	LOG(log_block, 2, ("transferAtached(2)  T%d %sB%d occ %d -- prev T%d %sB%d occ %d\n",
		GetTrkIndex(trk),IsDynamicBlock( blkN )?"Dyn ":"",GetTrkIndex(blkN),blkN->occupied,
		GetTrkIndex(prev),IsDynamicBlock( blkD )?"Dyn ":"",GetTrkIndex(blkD),blkD->occupied))
#endif

	trkT = blkD->endPt[0].prevTrack->endPt[blkD->endPt[0].trackEp].track;
	blkR0 = trkT?trkT->conBlock:NULL;
	trkT = blkD->endPt[1].prevTrack->endPt[blkD->endPt[1].trackEp].track;
	blkR1 = trkT?trkT->conBlock:NULL;
#if 0
	LOG(log_block, 2, ("transferAtached(3)  T%d %sBN%d -- %sBD%d   %sBR%d %sBR%d\n",
		trk?GetTrkIndex(trk):0,IsDynamicBlock(blkN)?"Dyn ":"",blkN?GetTrkIndex(blkN):0,
		IsDynamicBlock( blkD )?"Dyn ":"",blkD?GetTrkIndex(blkD):0,
		IsDynamicBlock( blkR0 )?"Dyn ":"",blkR0?GetTrkIndex(blkR0):0,
		IsDynamicBlock( blkR1 )?"Dyn ":"",blkR1?GetTrkIndex(blkR1):0))
#endif

	if ( blkN == blkR0 ) {
		epN = 0;
		epR = 1;
		blkR0 = blkR1;
	} else if (blkN == blkR1 ) {
		epN = 1;
		epR = 0;
	}
#if 0
	LOG(log_block, 2, ("transferAtached(4)  T%d BN%d occ %d  -- BD%d (epN %d epR %d) att %d %d occ %d"
				"  -- BR%d occ %d\n",
		trk?GetTrkIndex(trk):0,blkN?GetTrkIndex(blkN):0,blkN?blkN->occupied:99,
		blkD?GetTrkIndex(blkD):0,epN,epR,
		blkD?blkD->endPt[epN].attached:9,blkD?blkD->endPt[epR].attached:9,
		blkD?blkD->occupied:99,
		blkR0?GetTrkIndex(blkR0):0,blkR0?blkR0->occupied:99))
#endif
	if ( blkD->endPt[epN].attached ) return;

	// move attached to epR
	if ( blkD->endPt[epR].attached ) {
		blkD->endPt[epR].attached = FALSE;
#if 0
	LOG(log_block, 2, ("transferAtached(5) --move--  T%d B%d  -- BR%d occ %d\n",
		trk?GetTrkIndex(trk):0,blkN?GetTrkIndex(blkN):0,blkR0?GetTrkIndex(blkR0):0,
		blkR0?blkR0->occupied:99))
#endif
		blkR0->occupied--;
		blkD->endPt[epN].attached = TRUE;
		blkN->occupied++;
	}
}

// Add or subtract from the occupied counter and when a
// block is present adjust there as well.
static void adjustOccupied ( track_p trk, int adj )
{
	trk->occupied += adj;
	if (trk->conBlock)
		trk->conBlock->occupied += adj;
#if 0
	LOG(log_block, 1, ("adjustOccupied adj %d : trk T%d occ %d  B%d occ %d\n",
		adj, GetTrkIndex(trk), trk->occupied,
		trk->conBlock?GetTrkIndex(trk->conBlock):0,trk->conBlock?trk->conBlock->occupied:99))
#endif
}

// When entering train mode set up the counters for all present cars.
// Go through the cars, find the segment under each end and increment
// its counter.
EXPORT void SetOccupied( void )
{
    track_p car, blkD, blk, trk;
    coOrd pos;

    LOG(log_block, 1, ("SetOccupied: -- enter\n"))

    ClearOccupied();

    for (car=NULL; TrackIterate(&car);) {
        if ( ! IsTrainCarOnTrk( car ) ) continue;

        // Force occupied to be set
        car->endPt[0].prevTrack = NULL;
        car->endPt[1].prevTrack = NULL;
    }

    UpdateOccupied();

    verifyOccupancy ( FALSE, TRUE );
    verifyTrackOccupancy( TRUE );

    MainRedraw();
}

// all - TRUE -- display all tracks
static void verifyTrackOccupancy ( BOOL_T all )
{
    track_p b_trk, trk, trk0, trk1;
    blockData_p xx;
    EPINX_T segOcc, iTrack, ep0, ep1, epB0, epB1;
    EPINX_T problems = 0, report = 1;

    TRK_ITERATE(trk) {
        if ( ! IsTrack( trk ) || GetTrkIndex(trk) == T_TURNTABLE ) continue;
        if ( ! all && trk->occupied == 0 ) continue;
        LOG(log_block, 1, ("verifyTrackOccupancy: T%d %s occ %d -- B%d occ %d\n",
                GetTrkIndex(trk),GetTrkTypeName(trk),trk->occupied,trk->conBlock?GetTrkIndex(trk->conBlock):0,
                trk->conBlock?trk->conBlock->occupied:99))
    }
}

// rpt - TRUE - display occupied blocks
// all - TRUE - display all blocks
static void verifyOccupancy ( BOOL_T rpt, BOOL_T all )
{
    track_p b_trk, trk, trk0, trk1;
    blockData_p xx;
    EPINX_T segOcc, iTrack, ep0, ep1, epB0, epB1;
    EPINX_T problems = 0, report = 1;

    TRK_ITERATE(b_trk) {
        if ( IsTrackDeleted( b_trk ) || GetTrkType(b_trk) != T_BLOCK ) continue;

        // sum the seg occupancy
        xx = GetblockData( b_trk );
        if ( ( rpt  && b_trk->occupied ) || all ) {
            LOG(log_block, 1, ("verifyOccupancy: T%d %s --%s--\n",
                        GetTrkIndex(b_trk),GetTrkTypeName(b_trk),xx->name))
        }

        for ( segOcc = 0, iTrack = 0; iTrack < xx->numTracks; iTrack++ ) {
            if ( tracklist(iTrack).t && tracklist(iTrack).t->occupied < 0 ) problems++;
            segOcc += tracklist(iTrack).t->occupied;
        }

        // b_trk-ep[0] connects with this segment at ep0
        getDispTrkEp( b_trk, 0, &epB0, &trk0, &ep0 );

        // b_trk-ep[1] connects with this segment at ep1
        getDispTrkEp( b_trk, 1, &epB1, &trk1, &ep1 );

        // compare seg and block occupancy
        if ( ! IsDynamicBlock( b_trk ) && ( b_trk->endPt[0].attached || b_trk->endPt[1].attached ) )
            problems++;
        if ( IsDynamicBlock( b_trk ) && b_trk->endPt[0].attached && b_trk->endPt[1].attached )
            problems++;
        if ( b_trk->occupied > 200 || segOcc > 200 )
            problems++;
        if ( b_trk->occupied < segOcc )
            problems++;
        if ( b_trk->occupied < 0 || segOcc < 0 )
            problems++;

        if ( ( rpt && ( b_trk->occupied || segOcc ) ) || all || problems ) {
            LOG(log_block, 1, ("verifyOccupancy: B%d \"%s\" %socc %d, segs occ %d\n",
                    GetTrkIndex(b_trk), xx->name, attFlags(b_trk), b_trk->occupied, segOcc))
            LOG( log_block, 1, ( "verifyOccupancy:  T%d-%d -- %s0(T%d-%d)-B%d-(T%d-%d)1%s -- T%d-%d\n",
                GetTrkIndex(trk0),ep0,
                b_trk->endPt[0].attached==TRUE?"A":"U",GetTrkIndex(b_trk->endPt[0].prevTrack),epB0,
                GetTrkIndex(b_trk),
                GetTrkIndex(b_trk->endPt[1].prevTrack),epB1,b_trk->endPt[1].attached==TRUE?"A":"U",
                GetTrkIndex(trk1),ep1))
            LOG(log_block, 1, ("verifyOccupancy:  "))
            for ( iTrack = 0; iTrack < xx->numTracks; iTrack++ ) {
                LOG(log_block, 1, (" T%d  %d,",
                        tracklist(iTrack).i, tracklist(iTrack).t->occupied))
            }
            LOG(log_block, 1, ("\n"))
        }
    }
    if ( problems ) {
        LOG(log_block, 1, ("verifyOccupancy: %d problems\n", problems))
        if ( report ) {
            report = 0;
            NoticeMessage( _("Block occupancy error!"), _("Ok"), NULL);
        }
    }
}

static void adjustCarOccupancy( track_p trk0, EPINX_T ep0, track_p trk1, int adj )
{
	track_p trk = trk0, trkT;
	EPINX_T ep = ep0, epT;
	int ctr = 0;

	if ( ! trk0 || ! trk1 || GetTrkEndPtCnt( trk0 ) < 2 || GetTrkEndPtCnt( trk1 ) < 2 ) return;
#if 0
	LOG(log_block, 1, ("adjustCarOccupancy: T%d-%d T%d adj %d \n",
		GetTrkIndex(trk0),ep0,GetTrkIndex(trk1),adj))
#endif

	if ( trk0 == trk1 ) {
		adjustOccupied( trk, adj * 2 );
		return;
	}
	ep = GetRemoteEp( trk, ep );
	while ( trk && trk != trk1 ) {
		adjustOccupied( trk, adj );
		epT = trk->endPt[ep].trackEp;
		trkT = trk->endPt[ep].track;
		trk = trkT;
		ep = GetRemoteEp( trk, epT );
#if 0
	LOG(log_block, 1, ("adjustCarOccupancy: T%d  T%d-%d  trkT%d-%d\n",
		GetTrkIndex(trk0),trk?GetTrkIndex(trk):0,ep,trkT?GetTrkIndex(trkT):0,epT,adj))
#endif
	}
	if ( trk ) adjustOccupied( trk, adj );
}

// Given a track and position, find the end points the position is on.
static void getPathEp( track_p trk, coOrd pos, EPINX_T *ep0, EPINX_T *ep1 )
{
	EPINX_T epCnt, ep, minEp;
	DIST_T minDist, dist;
	track_p blk, trkD, lastD;

	epCnt = GetTrkEndPtCnt(trk);
	if ( epCnt == 2 ) {
		*ep0 = 0;
		*ep1 = 1;
	} else if ( epCnt == 3) {
		*ep0 = 0;
		*ep1 = GetRemoteEp( trk, *ep0 );
	} else {
		// for slip switches and other complex turnouts need to find the path
		// These may appear in Dynamic blocks
		*ep0 = *ep1 = epCnt;
		if ( (blk = trk->conBlock) && IsDynamicBlock( blk ) ) {
			// Start at the block end and look for trk
			trkD = blk->endPt[0].prevTrack;
			*ep0 = blk->endPt[0].trackEp;
			*ep1 = GetRemoteEp( trkD, *ep0 );
			if ( *ep1 == epCnt || *ep0 == epCnt ) return;
#if 0
			LOG(log_block, 2, ("getPathEp() T%d pos %0.2f %0.2f  trkD%d ep0 %d ep1 %d\n",
				trk?GetTrkIndex(trk):0,pos.x,pos.y,trkD?GetTrkIndex(trkD):0,*ep0,*ep1))
#endif
			// scan the Dynamic block from ep0 to trk
			// This gives us the correct path eps for the turnout
			while ( trkD != trk ) {
				*ep0 = trkD->endPt[*ep1].trackEp;
				trkD = trkD->endPt[*ep1].track;
				*ep1 = GetRemoteEp( trkD, *ep0 );
#if 0
			LOG(log_block, 2, ("getPathEp() trkD%d  ep0 %d ep1 %d\n",
				trkD?GetTrkIndex(trkD):0, *ep0, *ep1))
#endif
			}
			return;
		}
		minEp   = 0;
		minDist = FindDistance ( pos, trk->endPt[0].pos );
		for ( ep = 1; ep < GetTrkEndPtCnt(trk); ep++ ) {
			dist = FindDistance ( pos, trk->endPt[ep].pos );
			if ( dist < minDist ) {
				minDist = dist;
				minEp = ep;
			}
		}
		*ep0 = minEp;
		*ep1 = GetRemoteEp( trk, *ep0 );
	}
}

// scan for the length of the car and stop at the end seg
static BOOL_T scanForCar( DIST_T dist, track_p *trk, EPINX_T *ep )
{
	EPINX_T epT;

	if ( ! *trk ) return FALSE;

#if 0
        LOG(log_block, 2, ("scanForCar(A) dist %0.2f T%d-%d\n",
		dist,GetTrkIndex(*trk),*ep))
#endif

	while ( dist > 0.0 ) {
		epT = (*trk)->endPt[(*ep)].trackEp;
		*trk = (*trk)->endPt[(*ep)].track;
		if ( ! *trk ) return FALSE;
		*ep = GetRemoteEp( *trk, epT );
		dist -= FindDistance( (*trk)->endPt[*ep].pos, (*trk)->endPt[epT].pos );
#if 0
        	LOG(log_block, 2, ("scanForCar(B) dist %0.2f T%d-%d\n",
			dist,GetTrkIndex(*trk),*ep))
#endif
	}
	return TRUE;
}

// Return the tracks the front and back of the car is on and the entry
// points away from center of the car.
// Return TRUE when all data is found.
static BOOL_T getCarEnds( track_p car, track_p *trk0, EPINX_T *ep0, track_p *trk1, EPINX_T *ep1 )
{
	track_p trk;
	coOrd pos;
	DIST_T d0, d1, halfLen;

        if ( ! ( trk = TrainCarOnTrk( car, &pos ) ) ) return FALSE;
	if ( GetTrkType(trk) == T_TURNTABLE ) return FALSE;

	// get the end points on the track for the path the center of the car is on
        getPathEp( trk, pos, ep0, ep1 );

	// length of car
	halfLen = FindDistance( car->endPt[0].pos, car->endPt[1].pos ) / 2.0;
	// distance from center of car to each end of the track segment (seg len = d0 + d1)
	d0 = halfLen - FindDistance( trk->endPt[*ep0].pos, pos );
	d1 = halfLen - FindDistance( trk->endPt[*ep1].pos, pos );

#if 0
	LOG(log_block, 2, ("getCarEnds C%d T%d (%d %d) d0 %0.2f d1 %0.2f\n",
		GetTrkIndex(car),GetTrkIndex(trk),*ep0,*ep1,d0,d1))
#endif

	// Scan for half length of car and pass back trk and ep
	*trk1 = *trk0 = trk;
	if ( ! scanForCar( d0, trk0, ep0 ) ) return FALSE;
	if ( ! scanForCar( d1, trk1, ep1 ) ) return FALSE;
#if 0
	LOG(log_block, 2, ("getCarEnds C%d T%d-%d T%d-%d\n",
		GetTrkIndex(car),GetTrkIndex(*trk0),*ep0,GetTrkIndex(*trk1),*ep1))
#endif

	if ( GetTrkType(*trk0) == T_TURNTABLE ) return FALSE;
	if ( GetTrkType(*trk1) == T_TURNTABLE ) return FALSE;

	return TRUE;
}

static char * attFlags( track_p blk )
{
	if ( ! blk ) return "";
	if ( GetTrkType(blk) != T_BLOCK )
		blk = blk->conBlock;
	if ( ! IsDynamicBlock( blk ) ) return "";

	if ( blk->endPt[0].attached && blk->endPt[1].attached ) return "AA ";
	else if ( blk->endPt[0].attached ) return "AU ";
	else if ( blk->endPt[1].attached ) return "UA ";
	else return "UU ";
}

// For each car, increment occupancy for all tracks the car occupies and
// decrement occupancy for all tracks the car previously occupied.
// Create and delete dynamic blocks and transfer attached as needed.
EXPORT void UpdateOccupied( void )
{
    track_p car, car0, car1, prev0, prev1, blkD = NULL;
    EPINX_T ep0, ep1, epPrev0, epPrev1;
    coOrd pos;

//  LOG(log_block, 2, ("\nUpdateOccupied()\n"))

    for (car=NULL; TrackIterate(&car);) {

        // trk@pos is the center of the car
        // eps point away from center of car
        if ( ! getCarEnds( car, &car0, &ep0, &car1, &ep1 ) ) continue;

        prev0 = car->endPt[0].prevTrack;
        epPrev0 = car->endPt[0].trackEp;
        prev1 = car->endPt[1].prevTrack;
        epPrev1 = car->endPt[1].trackEp;
#if 0
        LOG(log_block, 2, ("\n\nUpdateOccupied() C%d car ends CF%d-%d CB%d-%d  PF%d-%d  PB%d-%d\n",
                GetTrkIndex(car),GetTrkIndex(car0),ep0, GetTrkIndex(car1),ep1,
	        prev0?GetTrkIndex(prev0):0,epPrev0,prev1?GetTrkIndex(prev1):0,epPrev1))
        LOG(log_block, 2, ("UpdateOccupied() C%d car ends CF%d occ %d,  B%d %socc %d  -- CB%d occ %d, B%d %socc %d\n",
                GetTrkIndex(car),GetTrkIndex(car0),car0->occupied,
                car0->conBlock?GetTrkIndex(car0->conBlock):0,attFlags(car0),
                car0->conBlock?car0->conBlock->occupied:99,
                GetTrkIndex(car1),car1->occupied,
                car1->conBlock?GetTrkIndex(car1->conBlock):0,attFlags(car1),
                car1->conBlock?car1->conBlock->occupied:99))
#endif

        // Create a Dynamic Block before entering the turnout
        if ( (blkD = createDynamicBlock( car0, ep0 )) ) {
            // Set the attached flag for the new dynamic block
            setAttached( blkD, car1 );
        } else if ( (blkD = createDynamicBlock( car1, ep1 )) ) {
            // Set the attached flag for the new dynamic block
            setAttached( blkD, car0 );
        }

        // Increment counters where car is located
        adjustCarOccupancy( car0, ep0, car1, 1 );
        // Decrement counters where car was located
        adjustCarOccupancy( prev0, epPrev0, prev1, -1 );

        // when an end of a car exits a Dyn block move the attached to the exit endpoint
        if ( prev0 && prev1 ) {
            if ( car1 != prev1 && ! IsDynamicBlock( car1->conBlock ) && IsDynamicBlock( prev1->conBlock ) )
                transferAtached( car1, prev1 );
            else if ( car0 != prev0 && ! IsDynamicBlock( car0->conBlock ) && IsDynamicBlock( prev0->conBlock ) )
                transferAtached( car0, prev0 );
        }

        // Delete a Dynamic Block that is no longer occupied
        if ( prev1 && prev1->conBlock && prev1->conBlock->occupied == 0 && IsDynamicBlock( prev1->conBlock ) )
            deleteDynamicBlock( prev1 );
        if ( prev0 && prev0->conBlock && prev0->conBlock->occupied == 0 && IsDynamicBlock( prev0->conBlock ) )
            deleteDynamicBlock( prev0 );

#if 0
        LOG(log_block, 1, ("UpdateOccupied C%d =O=  F on T%d-%d occ %d prev T%d occ %d  "
                    "B on T%d occ %d prev T%d occ %d\n",
                GetTrkIndex(car),GetTrkIndex(car0),ep0,car0->occupied,
                prev0?GetTrkIndex(prev0):0,prev0?prev0->occupied:99,
                GetTrkIndex(car1),car1->occupied,
                prev1?GetTrkIndex(prev1):0,prev1?prev1->occupied:99))
        LOG(log_block, 1, ("UpdateOccupied C%d     =O=  F on B%d %socc %d prev B%d %socc %d  "
                    "B on B%d %socc %d prev B%d %socc %d\n",
                GetTrkIndex(car),
                car0->conBlock?GetTrkIndex(car0->conBlock):0,attFlags(car0),
                car0->conBlock?car0->conBlock->occupied:99,
                prev0&&prev0->conBlock?GetTrkIndex(prev0->conBlock):0,attFlags(prev0),
                prev0&&prev0->conBlock?prev0->conBlock->occupied:99,
                car1->conBlock?GetTrkIndex(car1->conBlock):0,attFlags(car1),
                car1->conBlock?car1->conBlock->occupied:99,
                prev1&&prev1->conBlock?GetTrkIndex(prev1->conBlock):0,attFlags(prev1),
                prev1&&prev1->conBlock?prev1->conBlock->occupied:99))
#endif
        car->endPt[0].prevTrack = car0;
        car->endPt[0].trackEp = ep0;
        car->endPt[1].prevTrack = car1;
        car->endPt[1].trackEp = ep1;
    }

#if 0
    verifyOccupancy( TRUE, FALSE );
    verifyTrackOccupancy( FALSE );
#endif
    MainRedraw();
#if 0
    LOG(log_block, 3, ("    UpdateOccupied() -- exit \n\n"))
#endif
}


static void MoveBlock (track_p trk, coOrd orig ) {}
static void RotateBlock (track_p trk, coOrd orig, ANGLE_T angle ) {}
static void RescaleBlock (track_p trk, FLOAT_T ratio ) {}

static BOOL_T QueryBlock( track_p trk, int query )
{
	switch ( query ) {
	case Q_HAS_DESC:
		return TRUE;
	default:
		return FALSE;
	}
}


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
	QueryBlock, /* query */
	NULL, /* ungroup */
	NULL, /* flip */
	NULL, /* drawPositionIndicator */
	NULL, /* advancePositionIndicator */
	NULL, /* checkTraverse */
	NULL, /* makeParallel */
	NULL, /* drawDesc */
	NULL, /*rebuild*/
	NULL, /*store*/
	NULL, /*replay*/
	NULL, /*activate*/
	NULL  /*compare*/
};



static BOOL_T TrackInBlock( track_p trk, track_p blk )
{
	wIndex_t iTrack;

//	LOG( log_block, 1, ("*** TrackInBlock() trk %d\n", trk?trk->index:0))
	if ( ! blk || IsTrackDeleted( blk ) ) return FALSE;
	blockData_p xx = GetblockData(blk);
	for ( iTrack = 0; iTrack < xx->numTracks; iTrack++ ) {
//		LOG( log_block, 1, ("*** TrackInBlock() trk %d\n", (&(xx->trackList))[iTrack].i))
		if ( trk->index == (&(xx->trackList))[iTrack].i ) return TRUE;
	}

	return FALSE;
}

static track_p FindBlock( track_p trk )
{
	track_p a_trk;
	blockData_p xx;

	if ( IsTrackDeleted(trk) || GetTrkType(trk) == T_BLOCK ) return trk;
	if ( ! first_block ) return NULL;
	a_trk = first_block;
	while ( a_trk ) {
		if ( ! IsTrackDeleted( a_trk ) ) {
			if ( GetTrkType(a_trk) == T_BLOCK &&
					TrackInBlock(trk,a_trk) ) return a_trk;
		}
		xx = GetblockData(a_trk);
		a_trk = xx->next_block;
	}

	return NULL;
}

static track_p FindBlockByName( char *name )
{
	track_p a_trk;
	blockData_p xx;

	a_trk = first_block;
	while ( a_trk ) {
		xx = GetblockData(a_trk);
		if ( strcmp(xx->name, name) == 0 ) {
			return a_trk;
		}
		a_trk = xx->next_block;
	}

	return NULL;
}

static DIST_T getBlockLen( void )
{
	DIST_T totLen =0.0, len;
	wIndex_t iTrack;

	for ( iTrack = 0; iTrack < blockTrk_da.cnt; iTrack++ ) {
		len = GetTrkLength( blockTrk(iTrack).t, 0, 1 );
		totLen += len;
//		LOG( log_block, 1, ("*** getBlockLen(): track T%d len %0.2f\n",
//				blockTrk(iTrack).i, len))
	}

	return totLen;
}


static void BlockOk( void * junk )
{
	blockData_p xx,xx1;
	track_p trk,trk1;
	wIndex_t iTrack;
	EPINX_T ep;
	DIST_T len;

	wHide( blockPG.win );
	LOG( log_block, 1, ("*** BlockOk()\n"))

	ParamUpdate( &blockPG );
	if ( blockName[0] == 0 ) {
		NoticeMessage( _("Block must have a name!"), _("Ok"), NULL);
		return;
	}
	if ( FindBlockByName (blockName) ) {
		NoticeMessage( _("Block must have a unique name!"), _("Ok"), NULL);
		return;
	}

	//wDrawDelayUpdate( mainD.d, TRUE );

	if ( ! blockUndoStarted ) {
		UndoStart( _("Create block"), "Create block" );
		blockUndoStarted = TRUE;
	}

	/* Create a block object */
	LOG( log_block, 1, ("*** BlockOk(): %d tracks in block\n",blockTrk_da.cnt))
	trk = makeBlock();

	SetBlockBoundingBox( trk );

	if ( blockUndoStarted ) {
		UndoEnd();
		blockUndoStarted = FALSE;
	}
	wHide( blockW );

	if ( blockUndoStarted ) {
		UndoEnd();
		blockUndoStarted = FALSE;
	}

	Reset(); // DescOk
}

static void NewBlockDialog( track_p sel_trk )
{
	track_p trk = NULL;
	track_p blk_trk = NULL;

	LOG( log_block, 1, ("*** NewBlockDialog()\n"))
	if ( ! sel_trk )
		return;

	LOG( log_block, 1, ("*** NewBlockDialog( T%03d )\n", GetTrkIndex(sel_trk)))

	if ( ! IsTrack( sel_trk ) ) {
		LOG( log_block, 1, ("*** NewBlockDialog( %d is not trk type )\n",
				GetTrkType(sel_trk)))
		NoticeMessage( _("Please select a track segment"), _("Ok"), NULL);
		return;
	}

	if ( GetTrkEndPtCnt(sel_trk) != 2 ) {
		LOG( log_block, 1, ("*** NewBlockDialog( turnout )\n"))
		NoticeMessage( _("Please select a track segment with 2 endpoints"),
			_("Ok"), NULL);
		return;
	}

	// Look for sel_trk in existing block
	blockEditName[0] = 0;
	while ( TrackIterate( &trk ) ) {
		if ( GetTrkType(trk) == T_BLOCK && TrackInBlock(sel_trk,trk) ) {
			EditBlock ( trk );
			wShow( blockW );
			return;
		}
	}

	GetBlockSegs( sel_trk );
	if ( tempEndPts_da.cnt != 2 ) {
		LOG( log_block, 1, ("*** NewBlockDialog(): track end\n"))
		NoticeMessage( _("Track is track end"), _("Ok"), NULL);
		return;
	}
	if ( blockLen < GetLayoutMinBlockLength() ) {
		LOG( log_block, 1, ("*** NewBlockDialog(): too short - %0.1f\n", blockLen))
		NoticeMessage( _("Block is too short"), _("Ok"), NULL );
		return;
	}
	if ( blockTrk_da.cnt > 128 ) {
		LOG( log_block, 1, ("*** NewBlockDialog(): too many segments\n"))
		NoticeMessage( _("Block has too many segments"), _("Ok"), NULL );
		return;
	}
	if ( blockTrk_da.cnt <= 0 ) {
		LOG( log_block, 1, ("*** NewBlockDialog(): da.cnt <=0\n"))
		NoticeMessage( _("Block error"), _("Ok"), NULL );
		return;
	}

	if ( ! blockW ) {
		ParamRegister( &blockPG );
		blockW = ParamCreateDialog ( &blockPG, MakeWindowTitle( _("Create Block") ),
			       	_("Ok"), BlockOk, wHide, TRUE, NULL, F_BLOCK, NULL );
		blockD.dpi = mainD.dpi;
	}

	blockScript[0] = 0;
       	sprintf( blockName,"B%03d",blockTrk(0).i );
	ParamLoadControls( &blockPG );
	LOG( log_block, 1, ("*** NewBlockDialog( blockName %s )\n", blockName))
	wShow( blockW );
}

EXPORT void BlockCancel( void )
{
    if ( blockPG.win && wWinIsVisible(blockPG.win) ) {

        wHide(blockPG.win);

        if ( blockUndoStarted ) {
            UndoEnd();
            blockUndoStarted = FALSE;
        }
    }
    wSetCursor(mainD.d,defaultCursor);
}

static STATUS_T CmdBlockCreate( wAction_t action, coOrd pos )
{
	track_p trk = NULL;

	trk = OnTrack(&pos, FALSE, FALSE);

//	LOG( log_block, 1, ("*** CmdBlockAction(%08x,{%f,%f})\n",action,pos.x,pos.y))
	switch (action & 0xFF) {
	case C_START:
                LOG( log_block, 1,("*** CmdBlockCreate(): C_START\n"))
		InfoMessage( _("Select a track") );
		wSetCursor(mainD.d,wCursorCross);
		blockUndoStarted = FALSE;
		trk = NULL;
		return C_CONTINUE;

	case wActionMove:
		return C_CONTINUE;

	case C_DOWN:
           LOG( log_block, 1,("*** CmdBlockCreate(): C_DOWN\n"))
	   if ( (trk = OnTrack(&pos, FALSE, FALSE) ) != NULL) {

		blockBorder = mainD.scale*0.1;

		if ( blockBorder < trackGauge ) {
			blockBorder = trackGauge;
		}
		GetBoundingBox(trk, &blockSize, &blockOrig);
		blockOrig.x -= blockBorder;
		blockOrig.y -= blockBorder;
		blockSize.x -= blockOrig.x-blockBorder;
		blockSize.y -= blockOrig.y-blockBorder;
                LOG( log_block, 1,("*** CmdBlockCreate(): C_DOWN trk %d\n",
				GetTrkIndex(trk), GetTrkType(trk)))
		SetTrkBits( trk, TB_SELECTED );
		NewBlockDialog(trk);
		ClrTrkBits( trk, TB_SELECTED );
	    }
	    return C_CONTINUE;

	case C_REDRAW:
		return C_CONTINUE;
	case C_CANCEL:
		BlockCancel();
		return C_CONTINUE;
	}

	return C_CONTINUE;
}


// Called by DeleteTrack
static BOOL_T CheckDeleteBlock( track_p t )
{
    track_p blk = NULL, blkN;
    blockData_p xx;
    wIndex_t iTrack;

#if 0
    LOG( log_block, 1,("*** CheckDeleteBlock( B%d ) %p\n", GetTrkIndex(t), t))
#endif
    if ( IsTrack(t) ) {
        blk = FindBlock(t);
        if ( blk == NULL ) {
            return FALSE;
        }
    }
    if ( GetTrkType(t) == T_BLOCK )
        blk = t;
    if ( IsTrackDeleted( blk ) ) return FALSE;
    xx = GetblockData(blk);
    if ( ! xx ) return FALSE;

    // Remove the conBlock links
    for ( iTrack = 0; iTrack < xx->numTracks; iTrack++ ) {
        if ( (&(xx->trackList))[iTrack].t ) {
#if 0
            LOG( log_block, 1,("*** CheckDeleteBlock( B%d ) -- T%d con %p\n",
                GetTrkIndex(blk), (&(xx->trackList))[iTrack].i,
                (&(xx->trackList))[iTrack].t->conBlock))
#endif
            (&(xx->trackList))[iTrack].t->conBlock = NULL;
        }
    }

    blk->endPt[0].prevTrack = NULL;
    blk->endPt[1].prevTrack = NULL;
    DeleteTrack (blk, FALSE);
    return TRUE;
}

static void BlockEditOk( void * junk )
{
    blockData_p xx;
    track_p trk;

    LOG( log_block, 1, ("*** BlockEditOk()\n"))
    ParamUpdate (&blockEditPG );
    if ( blockEditName[0] == 0 ) {
        NoticeMessage( _("Block must have a name!"), _("Ok"), NULL);
        return;
    }
    wDrawDelayUpdate( mainD.d, TRUE );
    UndoStart( _("Modify Block"), "Modify Block" );
    trk = blockEditTrack;
    xx = GetblockData( trk );
    xx->name = MyStrdup(blockEditName);
    xx->script = MyStrdup(blockEditScript);
    xx->AutoGenerated = FALSE;
//  blockDebug(trk);
    UndoEnd();
    wHide( blockEditW );
}


static void EditBlock( track_p trk )
{
    blockData_p xx = GetblockData(trk);
    wIndex_t iTrack;
    BOOL_T needComma = FALSE;
    char temp[32];

    strncpy(blockEditName, xx->name, STR_SHORT_SIZE - 1);
    blockEditName[STR_SHORT_SIZE-1] = '\0';
    strncpy(blockEditScript, xx->script, STR_LONG_SIZE - 1);
    blockEditScript[STR_LONG_SIZE-1] = '\0';
    blockEditSegs[0] = '\0';
    for ( iTrack = 0; iTrack < xx->numTracks ; iTrack++ ) {
        if ( (&(xx->trackList))[iTrack].t == NULL ) continue;
        sprintf(temp,"%d",GetTrkIndex((&(xx->trackList))[iTrack].t));
        if ( needComma ) strcat(blockEditSegs,", ");
        strcat(blockEditSegs,temp);
        needComma = TRUE;
    }
    blockEditTrack = trk;
    if ( ! blockEditW ) {
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
	wDrawPix_t x, y, w, h;
	if ( blkhiliteColor == 0 )
		blkhiliteColor = wDrawColorGray(87);
	w = (wDrawPix_t)((blkhiliteSize.x/mainD.scale)*mainD.dpi+0.5);
	h = (wDrawPix_t)((blkhiliteSize.y/mainD.scale)*mainD.dpi+0.5);
	mainD.CoOrd2Pix(&mainD,blkhiliteOrig,&x,&y);
	wDrawFilledRectangle( mainD.d, x, y, w, h, blkhiliteColor,
		wDrawOptTransparent );
}


static int BlockMgmProc( int cmd, void * data )
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
        if ( ! xx->IsHilite ) {
            blkhiliteBorder = mainD.scale*0.1;
            if ( blkhiliteBorder < trackGauge ) blkhiliteBorder = trackGauge;
            first = TRUE;
            for ( iTrack = 0; iTrack < xx->numTracks ; iTrack++ ) {
                if ( (&(xx->trackList))[iTrack].t == NULL ) continue;
                GetBoundingBox( (&(xx->trackList))[iTrack].t, &tempSize, &tempOrig );
                if ( first ) {
                    blkhiliteOrig = tempOrig;
                    blkhiliteSize = tempSize;
                    first = FALSE;
                } else {
                    if ( tempSize.x > blkhiliteSize.x )
                        blkhiliteSize.x = tempSize.x;
                    if ( tempSize.y > blkhiliteSize.y )
                        blkhiliteSize.y = tempSize.y;
                    if ( tempOrig.x < blkhiliteOrig.x )
                        blkhiliteOrig.x = tempOrig.x;
                    if ( tempOrig.y < blkhiliteOrig.y )
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
        if ( xx && xx->IsHilite ) {
            blkhiliteBorder = mainD.scale*0.1;
            if ( blkhiliteBorder < trackGauge ) blkhiliteBorder = trackGauge;
            first = TRUE;
            for ( iTrack = 0; iTrack < xx->numTracks ; iTrack++ ) {
                if ( (&(xx->trackList))[iTrack].t == NULL ) continue;
                GetBoundingBox( (&(xx->trackList))[iTrack].t, &tempSize, &tempOrig );
                if ( first ) {
                    blkhiliteOrig = tempOrig;
                    blkhiliteSize = tempSize;
                    first = FALSE;
                } else {
                    if ( tempSize.x > blkhiliteSize.x )
                        blkhiliteSize.x = tempSize.x;
                    if ( tempSize.y > blkhiliteSize.y )
                        blkhiliteSize.y = tempSize.y;
                    if ( tempOrig.x < blkhiliteOrig.x )
                        blkhiliteOrig.x = tempOrig.x;
                    if ( tempOrig.y < blkhiliteOrig.y )
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
        sprintf( message, "\t%s\t%0.1f\t", xx->name, xx->blkLength);
        for ( iTrack = 0; iTrack < xx->numTracks ; iTrack++ ) {
            if ( (&(xx->trackList))[iTrack].t == NULL ) continue;
            sprintf(temp,"%d",GetTrkIndex((&(xx->trackList))[iTrack].t));
            if ( needComma ) strcat(message,", ");
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

    if ( blockI == NULL )
        blockI = wIconCreatePixMap( block_xpm );

    TRK_ITERATE(trk) {
        if ( GetTrkType(trk) != T_BLOCK ) continue;
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
    blockTrk_da.max = 0;
    blockTrk_da.cnt = 0;
    blockTrk_da.ptr = NULL;
    last_block = NULL;
}

EXPORT void UpdateMinBlockLength( void )
{
	if (GetLayoutMinBlockLength() > 1000.0)
		SetLayoutMinBlockLength( 1000.0 );
	if (GetLayoutMinBlockLength() <= 0.0)
		SetLayoutMinBlockLength( 0.0 );
	if (GetLayoutMinBlockLength() > GetLayoutMaxBlockLength())
		SetLayoutMaxBlockLength( GetLayoutMinBlockLength() );
}

EXPORT void UpdateMaxBlockLength( void )
{
	if (GetLayoutMaxBlockLength() > 1000.0)
		SetLayoutMaxBlockLength( 1000.0 );
	if (GetLayoutMinBlockLength() > GetLayoutMaxBlockLength())
		SetLayoutMaxBlockLength( GetLayoutMinBlockLength() );
}


EXPORT BOOL_T HasBlocks( void )
{
	track_p trk;
	EPINX_T blkCnt = 0;

	TRK_ITERATE(trk) {
		if ( GetTrkType(trk) == T_BLOCK ) return TRUE;
	}
	return FALSE;
}

