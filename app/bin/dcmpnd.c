/* \file dcmpnd.c
 * Compound tracks: Turnouts and Structures
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "compound.h"
#include "cundo.h"
#include "custom.h"
#include "fileio.h"
#include "param.h"
#include "include/paramfile.h"
#include "shrtpath.h"
#include "track.h"
#include "trkendpt.h"
#include "common-ui.h"

/*****************************************************************************
 *
 * Refresh Compound
 *
 */

static BOOL_T CheckCompoundEndPoint(
        track_p trk,
        EPINX_T trkEp,
        turnoutInfo_t * to,
        EPINX_T toEp,
        BOOL_T flip )
{

	struct extraDataCompound_t *xx = GET_EXTRA_DATA(trk, T_TURNOUT,
	                                 extraDataCompound_t);
	coOrd pos;
	DIST_T d;
	ANGLE_T a, a2;
	pos = GetTrkEndPos( trk, trkEp );
	Rotate( &pos, xx->orig, -xx->angle );
	pos.x -= xx->orig.x;
	pos.y -= xx->orig.y;
	if ( flip ) {
		pos.y = - pos.y;
	}
	d = FindDistance( pos, GetEndPtPos(EndPtIndex(to->endPt,toEp)));
	if ( d > connectDistance ) {
		sprintf( message,
		         _("End-Point #%d of the selected and actual turnouts are not close"), toEp );
		return FALSE;
	}
	a = GetTrkEndAngle( trk, trkEp );
	a2 = GetEndPtAngle(EndPtIndex(to->endPt,toEp));
	if ( flip ) {
		a2 = 180.0 - a2;
	}
	a = NormalizeAngle( a - xx->angle - a2 + connectAngle/2.0 );
	if ( a > connectAngle ) {
		sprintf( message,
		         _("End-Point #%d of the selected and actual turnouts are not aligned"), toEp );
		return FALSE;
	}
	return TRUE;
}


int refreshCompoundCnt;
static BOOL_T RefreshCompound1(
        track_p trk,
        turnoutInfo_t * to )
{
	EPINX_T ep, epCnt;
	BOOL_T ok;
	BOOL_T flip = FALSE;

	epCnt = GetTrkEndPtCnt(trk);
	if ( epCnt != to->endCnt ) {
		strcpy( message,
		        _("The selected Turnout had a differing number of End-Points") );
		return FALSE;
	}
	ok = TRUE;
	for ( ep=0; ep<epCnt; ep++ )
		if (!CheckCompoundEndPoint( trk, ep, to, ep, FALSE )) {
			ok = FALSE;
			break;
		}
	if ( !ok ) {
		if ( ep > 0 && epCnt == 2 &&
		     CheckCompoundEndPoint( trk, 1, to, 1, TRUE ) ) {
			flip = TRUE;
			ok = TRUE;
		} else if ( ep > 0 && epCnt == 3 &&
		            CheckCompoundEndPoint( trk, 1, to, 2, FALSE ) &&
		            CheckCompoundEndPoint( trk, 2, to, 1, FALSE ) ) {
			ok = TRUE;
		} else if ( ep > 0 && epCnt == 4 &&
		            CheckCompoundEndPoint( trk, 1, to, 3, FALSE ) &&
		            CheckCompoundEndPoint( trk, 2, to, 2, FALSE ) &&
		            CheckCompoundEndPoint( trk, 3, to, 1, FALSE ) ) {
			ok = TRUE;
		} else {
			return FALSE;
		}
	}
	UndoModify( trk );
	struct extraDataCompound_t *xx = GET_EXTRA_DATA(trk, T_NOTRACK,
	                                 extraDataCompound_t);
	FreeFilledDraw( xx->segCnt, xx->segs );
	MyFree( xx->segs );
	xx->segCnt = to->segCnt;
	xx->segs = (trkSeg_p)MyMalloc( xx->segCnt * sizeof *(trkSeg_p)0 );
	memcpy( xx->segs, to->segs, xx->segCnt * sizeof *(trkSeg_p)0 );
	xx->pathOverRide = to->pathOverRide;
	xx->pathNoCombine = to->pathNoCombine;
	SetPaths( trk, GetParamPaths(to) );
	if ( flip ) {
		FlipSegs( xx->segCnt, xx->segs, zero, 90.0 );
	}
	ClrTrkBits( trk, TB_SELECTED );
	refreshCompoundCnt++;
	CloneFilledDraw( xx->segCnt, xx->segs, FALSE );
	return TRUE;
}


typedef struct {
	char * name;
	turnoutInfo_t * to;
} refreshSpecial_t;
static dynArr_t refreshSpecial_da;
#define refreshSpecial(N) DYNARR_N( refreshSpecial_t, refreshSpecial_da, N )
static wIndex_t refreshSpecialInx;
static BOOL_T refreshReturnVal;
static void RefreshSkip( void * junk );
static paramListData_t refreshSpecialListData = { 30, 600, 0, NULL, NULL };
static paramData_t refreshSpecialPLs[] = {
#define REFRESH_M1		(0)
	{ PD_MESSAGE, NULL, NULL, 0/*PDO_DLGRESIZEW*/, I2VP(380) },
#define REFRESH_M2		(1)
	{ PD_MESSAGE, NULL, NULL, 0/*PDO_DLGRESIZEW*/, I2VP(380) },
#define REFRESH_S		(2)
	{ PD_MESSAGE, NULL, NULL, 0/*PDO_DLGRESIZEW*/, I2VP(380) },
#define REFRESH_L		(3)
	{ PD_LIST, &refreshSpecialInx, "list", PDO_LISTINDEX|PDO_NOPREF|PDO_DLGRESIZE, &refreshSpecialListData, NULL, BO_READONLY },
	{ PD_BUTTON, RefreshSkip, "skip", PDO_DLGCMDBUTTON, NULL, N_("Skip") }
};
static paramGroup_t refreshSpecialPG = { "refreshSpecial", 0, refreshSpecialPLs, COUNT( refreshSpecialPLs ) };
static void RefreshSpecialOk(
        void * junk )
{
	wHide( refreshSpecialPG.win );
}
static void RefreshSpecialCancel(
        wWin_p win )
{
	refreshSpecialInx = -1;
	refreshReturnVal = FALSE;
	wHide( refreshSpecialPG.win );
}
static void RefreshSkip(
        void * junk )
{
	refreshSpecialInx = -1;
	wHide( refreshSpecialPG.win );
}

EXPORT BOOL_T RefreshCompound(
        track_p trk,
        BOOL_T junk )
{
	TRKTYP_T trkType;
	struct extraDataCompound_t *xx;
	int inx;
	turnoutInfo_t *to;
	SCALEINX_T scale;

	if ( trk == NULL ) {
		InfoMessage( _("%d Track(s) refreshed"), refreshCompoundCnt );
		refreshCompoundCnt = 0;
		for ( inx=0; inx<refreshSpecial_da.cnt; inx++ )
			if ( refreshSpecial(inx).name != NULL &&
			     refreshSpecial(inx).to == NULL ) {
				refreshSpecial(inx).name = NULL;
			}
		return FALSE;
	}
	trkType = GetTrkType(trk);
	scale = GetTrkScale(trk);
	if ( trkType != T_TURNOUT && trkType != T_STRUCTURE ) {
		ClrTrkBits( trk, TB_SELECTED );
		return TRUE;
	}
	xx = GET_EXTRA_DATA(trk, trkType, extraDataCompound_t);
	refreshReturnVal = TRUE;
	for ( inx=0; inx<refreshSpecial_da.cnt; inx++ ) {
		if ( refreshSpecial(inx).name != NULL &&
		     strcasecmp( xx->title, refreshSpecial(inx).name ) == 0 ) {
			to = refreshSpecial(inx).to;
			if ( to == NULL ) {
				return TRUE;
			}
			if ( IsParamValid(to->paramFileIndex) &&
			     to->segCnt > 0 &&
			     (FIT_NONE != CompatibleScale( GetTrkEndPtCnt(trk)>0?FIT_TURNOUT:FIT_STRUCTURE,
			                                   to->scaleInx, scale ) )) {
				if ( RefreshCompound1( trk, refreshSpecial(inx).to ) ) {
					if ( strcasecmp( xx->title, to->title ) != 0 ) {
						MyFree( xx->title );
						xx->title = MyStrdup( to->title );
					}
					return TRUE;
				}
			}
		}
	}
	if ( ( to = FindCompound( FIND_TURNOUT|FIND_STRUCT, NULL, xx->title ) ) != NULL
	     &&
	     RefreshCompound1( trk, to ) ) {
		return TRUE;
	}
	if ( refreshSpecialPG.win == NULL ) {
		ParamRegister( &refreshSpecialPG );
		ParamCreateDialog( &refreshSpecialPG,
		                   MakeWindowTitle(_("Refresh Turnout/Structure")), _("Ok"), RefreshSpecialOk,
		                   RefreshSpecialCancel, TRUE, NULL, F_BLOCK|F_RESIZE|F_RECALLSIZE, NULL );
	}
	ParamLoadMessage( &refreshSpecialPG, REFRESH_M1,
	                  _("Choose a Turnout/Structure to replace:") );
	ParamLoadMessage( &refreshSpecialPG, REFRESH_M2, "" );
	refreshSpecialInx = -1;
	wListClear( (wList_p)refreshSpecialPLs[REFRESH_L].control );
	if ( GetTrkEndPtCnt(trk) > 0 ) {
		to = TurnoutAdd( listLabels, scale,
		                 (wList_p)refreshSpecialPLs[REFRESH_L].control, NULL, GetTrkEndPtCnt(trk) );
	} else {
		to = StructAdd( listLabels, scale,
		                (wList_p)refreshSpecialPLs[REFRESH_L].control, NULL );
	}
	if ( to == NULL ) {
		NoticeMessage( MSG_NO_TURNOUTS_AVAILABLE, _("Ok"), NULL,
		               GetTrkEndPtCnt(trk)>0 ? _("Turnouts") : _("Structures") );
		return FALSE;
	}
	FormatCompoundTitle( listLabels, xx->title );
	ParamLoadMessage( &refreshSpecialPG, REFRESH_S, message );
	while (1) {
		wListSetIndex( (wList_p)refreshSpecialPLs[REFRESH_L].control, -1 );
		wShow( refreshSpecialPG.win );
		if ( refreshSpecialInx < 0 ) {
			if ( refreshReturnVal ) {
				DYNARR_APPEND( refreshSpecial_t, refreshSpecial_da, 10 );
				refreshSpecial(refreshSpecial_da.cnt-1).to = NULL;
				refreshSpecial(refreshSpecial_da.cnt-1).name = MyStrdup( xx->title );
			}
			return refreshReturnVal;
		}
		to = (turnoutInfo_t*)wListGetItemContext( (wList_p)
		                refreshSpecialPLs[REFRESH_L].control, refreshSpecialInx );
		if ( to != NULL &&
		     RefreshCompound1( trk, to ) ) {
			DYNARR_APPEND( refreshSpecial_t, refreshSpecial_da, 10 );
			refreshSpecial(refreshSpecial_da.cnt-1).to = to;
			refreshSpecial(refreshSpecial_da.cnt-1).name = MyStrdup( xx->title );
			if ( strcasecmp( xx->title, to->title ) != 0 ) {
				MyFree( xx->title );
				xx->title = MyStrdup( to->title );
			}
			return TRUE;
		}
		ParamLoadMessage( &refreshSpecialPG, REFRESH_M1, message );
		ParamLoadMessage( &refreshSpecialPG, REFRESH_M2,
		                  _("Choose another Turnout/Structure to replace:") );
	}
}

/*****************************************************************************
 *
 * Custom Management Support
 *
 */

static char renameManuf[STR_SIZE];
static char renameDesc[STR_SIZE];
static char renamePartno[STR_SIZE];
static turnoutInfo_t * renameTo;

static paramData_t renamePLs[] = {
	/*0*/ { PD_STRING, renameManuf, "manuf", PDO_NOPREF | PDO_NOTBLANK, I2VP(350), N_("Manufacturer"), 0, 0, sizeof(renameManuf)},
	/*1*/ { PD_STRING, renameDesc, "desc", PDO_NOPREF | PDO_NOTBLANK, I2VP(230), N_("Description"), 0, 0, sizeof(renameDesc)},
	/*2*/ { PD_STRING, renamePartno, "partno", PDO_NOPREF|PDO_DLGHORZ|PDO_DLGIGNORELABELWIDTH | PDO_NOTBLANK, I2VP(100), N_("#"), 0, 0, sizeof(renamePartno)}
};
static paramGroup_t renamePG = { "rename", 0, renamePLs, COUNT( renamePLs ) };


EXPORT BOOL_T CompoundCustomSave(
        FILE * f )
{
	int inx;
	turnoutInfo_t * to;
	BOOL_T rc = TRUE;

	for ( inx=0; inx<turnoutInfo_da.cnt; inx++ ) {
		to = turnoutInfo(inx);
		if (to->paramFileIndex == PARAM_CUSTOM && to->segCnt > 0) {
			rc &= fprintf( f, "TURNOUT %s \"%s\"\n", GetScaleName(to->scaleInx),
			               PutTitle(to->title) )>0;
			if ( to->customInfo ) {
				rc &= fprintf( f, "\tU %s\n",to->customInfo )>0;
			}
			rc &= WriteCompoundPathsEndPtsSegs( f, GetParamPaths( to ), to->segCnt,
			                                    to->segs,
			                                    to->endCnt, to->endPt );
		}
	}
	for ( inx=0; inx<structureInfo_da.cnt; inx++ ) {
		to = structureInfo(inx);
		if (to->paramFileIndex == PARAM_CUSTOM && to->segCnt > 0) {
			rc &= fprintf( f, "STRUCTURE %s \"%s\"\n", GetScaleName(to->scaleInx),
			               PutTitle(to->title) )>0;
			if ( to->customInfo ) {
				rc &= fprintf( f, "\tU %s\n",to->customInfo )>0;
			}
			rc &= WriteSegs( f, to->segCnt, to->segs );
		}
	}
	return rc;
}


static void RenameOk( void * junk )
{
	sprintf( message, "%s\t%s\t%s", renameManuf, renameDesc, renamePartno );
	if ( renameTo->title ) {
		MyFree( renameTo->title );
	}
	renameTo->title = MyStrdup( message );
	wHide( renamePG.win );
	DoChangeNotification( CHANGE_PARAMS );
}


static int CompoundCustMgmProc(
        int cmd,
        void * data )
{
	turnoutInfo_t * to = (turnoutInfo_t*)data;
	turnoutInfo_t * to2=NULL;
	int inx;
	char * mP, *pP, *nP;
	int mL, pL, nL;
	BOOL_T rc = TRUE;

	switch ( cmd ) {
	case CUSTMGM_DO_COPYTO:
		if ( to->segCnt <= 0 ) {
			return TRUE;
		}
		if ( to->endCnt ) {
			rc &= fprintf( customMgmF, "TURNOUT %s \"%s\"\n", GetScaleName(to->scaleInx),
			               PutTitle(to->title) )>0;
			if ( to->customInfo ) {
				rc &= fprintf( customMgmF, "\tU %s\n",to->customInfo )>0;
			}
			rc &= WriteCompoundPathsEndPtsSegs( customMgmF,
			                                    GetParamPaths( to ), to->segCnt, to->segs,
			                                    to->endCnt, to->endPt );
		} else {
			rc &= fprintf( customMgmF, "STRUCTURE %s \"%s\"\n", GetScaleName(to->scaleInx),
			               PutTitle(to->title) )>0;
			if ( to->customInfo ) {
				rc &= fprintf( customMgmF, "\tU %s\n",to->customInfo )>0;
			}
			rc &= WriteSegs( customMgmF, to->segCnt, to->segs );
		}
		return rc;
	case CUSTMGM_CAN_EDIT:
		return (to->endCnt != 0 && to->customInfo != NULL);
	case CUSTMGM_DO_EDIT:
		if ( to->endCnt == 0 || to->customInfo==NULL )	{
			renameTo = to;
			ParseCompoundTitle( to->title, &mP, &mL, &pP, &pL, &nP, &nL );
			strncpy( renameManuf, mP, mL ); renameManuf[mL] = 0;
			strncpy( renameDesc, pP, pL ); renameDesc[pL] = 0;
			strncpy( renamePartno, nP, nL ); renamePartno[nL] = 0;
			if ( !renamePG.win ) {
				ParamRegister( &renamePG );
				ParamCreateDialog( &renamePG, MakeWindowTitle(_("Rename Object")), _("Ok"),
				                   RenameOk, wHide, TRUE, NULL, F_BLOCK, NULL );
			}
			ParamLoadControls( &renamePG );
			wShow( renamePG.win );
		} else {
			for (inx=0; inx<turnoutInfo_da.cnt && to!=turnoutInfo(inx); inx++);
			if ( inx > 0 &&
			     turnoutInfo(inx-1)->customInfo &&
			     strcmp( to->customInfo, turnoutInfo(inx-1)->customInfo ) == 0 ) {
				to2 = to;
				to = turnoutInfo(inx-1);
			} else if ( inx < turnoutInfo_da.cnt-1 &&
			            turnoutInfo(inx+1)->customInfo &&
			            strcmp( to->customInfo, turnoutInfo(inx+1)->customInfo ) == 0 ) {
				to2 = turnoutInfo(inx+1);
			}
			EditCustomTurnout( to, to2 );
		}
		return TRUE;
	case CUSTMGM_CAN_DELETE:
		return TRUE;
	case CUSTMGM_DO_DELETE:
		to->segCnt = 0;
		return TRUE;
	case CUSTMGM_GET_TITLE:
		ParseCompoundTitle( to->title, &mP, &mL, &pP, &pL, &nP, &nL );
		sprintf( message, "\t%.*s\t%s\t%.*s\t%.*s", mL, mP, GetScaleName(to->scaleInx),
		         nL, nP, pL, pP );
		return TRUE;
	}
	return FALSE;
}


#include "bitmaps/turnout.xpm3"
#include "bitmaps/building.xpm3"

EXPORT void CompoundCustMgmLoad( void )
{
	int inx;
	turnoutInfo_t * to;
	static wIcon_p turnoutI = NULL;
	static wIcon_p structI = NULL;

	if ( turnoutI == NULL ) {
		turnoutI = wIconCreatePixMap( turnout_xpm3[0] );
	}
	if ( structI == NULL ) {
		structI = wIconCreatePixMap( building_xpm3[0] );
	}

	for ( inx=0; inx<turnoutInfo_da.cnt; inx++ ) {
		to = turnoutInfo(inx);
		if (to->paramFileIndex == PARAM_CUSTOM && to->segCnt > 0) {
			CustMgmLoad( turnoutI, CompoundCustMgmProc, to );
		}
	}
	for ( inx=0; inx<structureInfo_da.cnt; inx++ ) {
		to = structureInfo(inx);
		if (to->paramFileIndex == PARAM_CUSTOM && to->segCnt > 0) {
			CustMgmLoad( structI, CompoundCustMgmProc, to );
		}
	}
}

/*****************************************************************************
 *
 * Utitlies
 *
 */

wIndex_t FindListItemByContext(
        wList_p listP,
        void * context )
{
	if ( listP == NULL ) {
		return -1;
	}
	if ( context == NULL ) {
		return -1;
	}
	for ( wIndex_t inx = 0; inx < wListGetCount( listP ); ++inx ) {
		void * itemContext = wListGetItemContext( listP, inx );
		if ( itemContext != NULL ) {
			if ( itemContext == context ) {
				return inx;
			}
		}
	}
	return -1;
}


