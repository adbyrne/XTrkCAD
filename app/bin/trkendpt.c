/*
 * $Header: $
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2022 Dave Bullis
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

#include "common.h"
#include "common.h"
#include "common-ui.h"
#include "fileio.h"
#include "param.h"
#include "track.h"
#include "trkendpt.h"
#include "trkendptx.h"
#include "misc.h"
//#include "cbezier.h"
//#include "tbezier.h"
//#include "cjoin.h"
#include "draw.h"


/************************************************************************
 * Basic Access
 */

EXPORT CSIZE_T EndPtSize(
	EPINX_T epCnt )
{
	return epCnt * sizeof *(trkEndPt_p)NULL;
}


EXPORT coOrd GetEndPtPos( trkEndPt_p epp )
{
	return epp->pos;
}


EXPORT ANGLE_T GetEndPtAngle( trkEndPt_p epp )
{
	return epp->angle;
}


EXPORT track_p GetEndPtTrack( trkEndPt_p epp )
{
	return epp->track;
}


EXPORT void SetEndPt( trkEndPt_p epp, coOrd pos, ANGLE_T angle )
{
	epp->pos = pos;
	epp->angle = angle;
}


EXPORT EPINX_T GetEndPtEndPt( trkEndPt_p epp )
{
	return (EPINX_T)(epp->index);
}


EXPORT TRKINX_T GetEndPtIndex( trkEndPt_p epp )
{
	return (TRKINX_T)(epp->index);
}


EXPORT void SetEndPtTrack( trkEndPt_p epp, track_p trk )
{
	epp->track = trk;
	epp->index = -1;
}


EXPORT void SetEndPtEndPt( trkEndPt_p epp, EPINX_T ep )
{
	epp->index = ep;
}


EXPORT trkEndPt_p EndPtIndex( trkEndPt_p epp, EPINX_T ep )
{
	return epp+ep;
}


/************************************************************************
 * Temp End Points
 */

static dynArr_t tempEndPts_da;

EXPORT void TempEndPtsReset( void )
{
	DYNARR_RESET( trkEndPt_t, tempEndPts_da );
}


EXPORT void TempEndPtsSet( EPINX_T ep )
{
	DYNARR_SET( trkEndPt_t, tempEndPts_da, ep );
	memset( tempEndPts_da.ptr, 0, ep * sizeof *(trkEndPt_p)NULL );
}


EXPORT EPINX_T TempEndPtsCount( void )
{
	return tempEndPts_da.cnt;
}


EXPORT trkEndPt_p TempEndPt( EPINX_T ep )
{
	CHECK( ep < tempEndPts_da.cnt );
	return &DYNARR_N( trkEndPt_t, tempEndPts_da, ep );
}


#ifndef MKTURNOUT

EXPORT trkEndPt_p TempEndPtsAppend( void )
{
	DYNARR_APPEND( trkEndPt_t, tempEndPts_da, 10 );
	trkEndPt_p epp = &DYNARR_LAST( trkEndPt_t, tempEndPts_da );
	memset( epp, 0, sizeof *epp );
	return epp;
}


EXPORT void SwapEndPts(
	trkEndPt_p epp,
	EPINX_T ep0,
	EPINX_T ep1 )
{
	trkEndPt_t tempEP;
	tempEP = epp[ep0];
	epp[ep0] = epp[ep1];
	epp[ep1] = tempEP;
}


/************************************************************************
 * Track level access
 */

EXPORT void SetTrkEndPoint( track_p trk, EPINX_T ep, coOrd pos, ANGLE_T angle )
{
	CHECK( ep < GetTrkEndPtCnt(trk) );
	// check  setTrkEndPoint: endPt is not connected
	CHECK( GetEndPtTrack( GetTrkEndPt( trk, ep ) ) == NULL );
	SetEndPt( GetTrkEndPt( trk, ep ), pos, angle );
}


EXPORT void SetTrkEndPointSilent( track_p trk, EPINX_T ep, coOrd pos, ANGLE_T angle )
{
	CHECK( ep < GetTrkEndPtCnt(trk) );
	SetEndPt( GetTrkEndPt( trk, ep ), pos, angle );
}


EXPORT coOrd GetTrkEndPos( track_p trk, EPINX_T ep )
{
	CHECK( ep < GetTrkEndPtCnt(trk) );
	return GetTrkEndPt(trk,ep)->pos;
}


EXPORT ANGLE_T GetTrkEndAngle( track_p trk, EPINX_T ep )
{
	CHECK( ep < GetTrkEndPtCnt(trk) );
	return GetTrkEndPt(trk,ep)->angle;
}


EXPORT track_p GetTrkEndTrk( track_p trk, EPINX_T ep )
{
	CHECK( ep < GetTrkEndPtCnt(trk) );
	return GetTrkEndPt(trk,ep)->track;
}


EXPORT long GetTrkEndOption( track_p trk, EPINX_T ep )
{
	CHECK( ep < GetTrkEndPtCnt(trk) );
	return GetTrkEndPt(trk,ep)->option;
}


EXPORT long SetTrkEndOption( track_p trk, EPINX_T ep, long option )
{
	CHECK( ep < GetTrkEndPtCnt(trk) );
	return GetTrkEndPt(trk,ep)->option = option;
}


/************************************************************************
 * Elevations
 */

EXPORT void SetTrkEndElev( track_p trk, EPINX_T ep, int option, DIST_T height, char * station )
{
	track_p trk1;
	EPINX_T ep1;
	trkEndPt_p epp = GetTrkEndPt( trk, ep );
	epp->elev.option = option;
	epp->elev.cacheSet = FALSE;
	if (EndPtIsDefinedElev(trk,ep)) {
		epp->elev.u.height = height;
	} else if (EndPtIsStationElev(trk,ep)) {
		if (station == NULL)
			station = "";
		epp->elev.u.name = MyStrdup(station);
	}
	if ( (trk1=GetTrkEndTrk(trk, ep)) != NULL ) {
		ep1 = GetEndPtConnectedToMe( trk1, trk );
		if (ep1 >= 0) {
			trkEndPt_p epp1 = GetTrkEndPt( trk1, ep1 );
			epp1->elev.option = option;
			epp1->elev.u.height = height;
			if (EndPtIsDefinedElev(trk1,ep1))
				epp1->elev.u.height = height;
			else if (EndPtIsStationElev(trk,ep))
				epp1->elev.u.name = MyStrdup(station);
		}
	}
}


EXPORT void GetTrkEndElev( track_p trk, EPINX_T e, int *option, DIST_T *height )
{
	trkEndPt_p epp = GetTrkEndPt( trk, e );
	*option = epp->elev.option;
	*height = epp->elev.u.height;
}


EXPORT int GetTrkEndElevUnmaskedMode( track_p trk, EPINX_T e )
{
	trkEndPt_p epp = GetTrkEndPt( trk, e );
	return epp->elev.option;
}


EXPORT int GetTrkEndElevMode( track_p trk, EPINX_T e )
{
	trkEndPt_p epp = GetTrkEndPt( trk, e );
	return epp->elev.option&ELEV_MASK;
}


EXPORT DIST_T GetTrkEndElevHeight( track_p trk, EPINX_T e )
{
	CHECK( EndPtIsDefinedElev(trk,e) );
	trkEndPt_p epp = GetTrkEndPt( trk, e );
	return epp->elev.u.height;
}


EXPORT void ClrEndPtElevCache(
	EPINX_T epCnt, 
	trkEndPt_p epPts )
{
	for ( EPINX_T ep = 0; ep < epCnt; ep++ ) {
		epPts[ep].elev.cacheSet = FALSE;
	}
}


static BOOL_T bCacheElev = TRUE;

EXPORT BOOL_T GetTrkEndElevCachedHeight (track_p trk, EPINX_T e, DIST_T * height, DIST_T * grade)
{
	if ( ! bCacheElev )
		return FALSE;
	trkEndPt_p epp = GetTrkEndPt( trk, e );
	if (epp->elev.cacheSet) {
		*height = epp->elev.cachedElev;
		*grade = epp->elev.cachedGrade;
		return TRUE;
	}
	return FALSE;
}

EXPORT void SetTrkEndElevCachedHeight ( track_p trk, EPINX_T e, DIST_T height, DIST_T grade)
{
	trkEndPt_p epp = GetTrkEndPt( trk, e );
	epp->elev.cachedElev = height;
	epp->elev.cachedGrade = grade;
	epp->elev.cacheSet = TRUE;
}


EXPORT char * GetTrkEndElevStation( track_p trk, EPINX_T e )
{
	CHECK( EndPtIsStationElev(trk,e) );
	trkEndPt_p epp = GetTrkEndPt( trk, e );
	if ( epp->elev.u.name == NULL )
		return "";
	else
		return epp->elev.u.name;
}


EXPORT void DrawEndElev( drawCmd_p d, track_p trk, EPINX_T ep, wDrawColor color )
{
	coOrd pp;
	wFont_p fp;
	elev_t * elev;
	track_p trk1;
	DIST_T elev0, grade;
	ANGLE_T a=0;
	int style = BOX_BOX;
	BOOL_T gradeOk = TRUE;
	char *elevStr;

	if ((labelEnable&LABELENABLE_ENDPT_ELEV)==0)
		return;
	elev = &GetTrkEndPt(trk,ep)->elev;
	if ( (elev->option&ELEV_MASK)==ELEV_NONE ||
		 (elev->option&ELEV_VISIBLE)==0 )
		return;
	if ( (trk1=GetTrkEndTrk(trk,ep)) && GetTrkIndex(trk1)<GetTrkIndex(trk) )
		return;

	fp = wStandardFont( F_HELV, FALSE, FALSE );
	pp = GetTrkEndPos( trk, ep );
	switch ((elev->option&ELEV_MASK)) {
	case ELEV_COMP:
	case ELEV_GRADE:
		if ( color == wDrawColorWhite ) {
			elev0 = grade = elev->u.height;
		} else if ( !ComputeElev( trk, ep, FALSE, &elev0, &grade, FALSE ) ) {
			elev0 = grade = 0;
			gradeOk = FALSE;
		}
		if ((elev->option&ELEV_MASK)==ELEV_COMP) {
			elevStr = FormatDistance(elev0);
			elev->u.height = elev0;
		} else if (gradeOk) {
			sprintf( message, "%0.1f%%", round(fabs(grade*100.0)*10)/10 );
			elevStr = message;
			a = GetTrkEndAngle( trk, ep );
			style = BOX_ARROW_BACKGROUND;
			if (grade <= -0.001)
				a = NormalizeAngle( a+180.0 );
			else if ( grade < 0.001 )
				style = BOX_BOX_BACKGROUND;
			elev->u.height = grade;
		} else {
			elevStr = "????%%";
		}
		break;
	case ELEV_DEF:
		elevStr = FormatDistance( elev->u.height);
		break;
	case ELEV_STATION:
		elevStr = elev->u.name;
		break;
	default:
		return;
	}
	coOrd startLine = pp, endLine = pp;
	pp.x += elev->doff.x;
	pp.y += elev->doff.y;
	if (color==drawColorPreviewSelected) {
		Translate(&endLine,pp,FindAngle(pp,startLine),descriptionFontSize/d->dpi);
		DrawLine( d, startLine, endLine, 0, color );
	}
	DrawBoxedString( style, d, pp, elevStr, fp, (wFontSize_t)descriptionFontSize, color, a );

}


/************************************************************************
 * Descriptions
 */

EXPORT DIST_T EndPtDescriptionDistance(
		coOrd pos,
		track_p trk,
		EPINX_T ep,
		coOrd *dpos,
		BOOL_T show_hidden,
		BOOL_T * hidden)
{
	elev_t *e;
	coOrd pos1;
	track_p trk1;
	*dpos = pos;
	if (hidden) *hidden = FALSE;
	e = &GetTrkEndPt(trk,ep)->elev;
	if ((e->option&ELEV_MASK)==ELEV_NONE)
		return DIST_INF;
	if (((e->option&ELEV_VISIBLE)==0) && !show_hidden)
		return DIST_INF;
	if ((trk1=GetTrkEndTrk(trk,ep)) && GetTrkIndex(trk1)<GetTrkIndex(trk))
		return DIST_INF;
	if ((e->option&ELEV_VISIBLE)==0) {					//Hidden - disregard offset
		if (hidden) *hidden = TRUE;
		return FindDistance( GetTrkEndPos(trk,ep), pos );
	}
	/*REORIGIN( pos1, e->doff, GetTrkEndPos(trk,ep), GetTrkEndAngle(trk,ep) );*/
	pos1 = GetTrkEndPos(trk,ep);
	coOrd tpos = pos1;
	pos1.x += e->doff.x;
	pos1.y += e->doff.y;
	*dpos = pos1;
	if (hidden) *hidden = !(e->option&ELEV_VISIBLE);
	if (FindDistance(tpos,pos)<FindDistance( pos1, pos ))
		return FindDistance(tpos,pos);
	return FindDistance( pos1, pos );
}


EXPORT STATUS_T EndPtDescriptionMove(
		track_p trk,
		EPINX_T ep,
		wAction_t action,
		coOrd pos )
{
	static coOrd p0;
//	static coOrd p1;
	elev_t *e;
	track_p trk1;

	e = &GetTrkEndPt(trk,ep)->elev;
	switch (action) {
	case C_DOWN:
		p0 = GetTrkEndPos(trk,ep);
//		p1 = pos;
		e->option |= ELEV_VISIBLE; //Make sure we make visible
		DrawEndElev( &mainD, trk, ep, wDrawColorWhite );
		/*no break*/
	case C_MOVE:
	case C_UP:
//		p1 = pos;
		e->doff.x = (pos.x-p0.x);
		e->doff.y = (pos.y-p0.y);
		if ((trk1=GetTrkEndTrk(trk,ep))) {
			EPINX_T ep1 = GetEndPtConnectedToMe(trk1,trk);
			trkEndPt_p epp = GetTrkEndPt( trk1, ep1 );
//			e1 = &trk1->endPt[GetEndPtConnectedToMe(trk1,trk)].elev;
			epp->elev.doff = e->doff;
		}
		if ( action == C_UP ) {
			wDrawColor color = GetTrkColor( trk, &mainD );
			DrawEndElev( &mainD, trk, ep, color );
		}
		return action==C_UP?C_TERMINATE:C_CONTINUE;

	case C_REDRAW:
		DrawEndElev( &tempD, trk, ep, drawColorPreviewSelected );
		break;
	}
	return C_CONTINUE;
}


/************************************************************************
 * Read/Write End Points
 */

EXPORT BOOL_T GetEndPtArg(
	char * cp,
	char type,
	BOOL_T bImprovedEnds )
{
	long option;
	long option2;
	trkEndPt_p e = TempEndPtsAppend();
	FLOAT_T ignoreFloat;

	if (type == SEG_CONEP) {
		if ( !GetArgs( cp, "dc", &e->index, &cp ) ) {
							/*??*/return FALSE;
		}
	} else {
		e->index = -1;
	}
	if ( !GetArgs( cp, "pfc",
		&e->pos, &e->angle, &cp) ) {
		/*??*/return FALSE;
	}
	e->elev.option = 0;
	e->elev.u.height = 0.0;
	e->elev.doff = zero;
	e->option = 0;
	if (bImprovedEnds) {				//E4 and T4
		if (!GetArgs( cp, "lpc", &option, &e->elev.doff, &cp )) {
			/*??*/return FALSE;
		}
		switch (option&ELEV_MASK) {
			case ELEV_STATION:
				GetArgs( cp, "qc", &e->elev.u.name, &cp);
				break;
			default:
				GetArgs( cp, "fc", &e->elev.u.height, &cp);   //First height
		}
		DIST_T height2;
		if (!GetArgs( cp, "flLlc", &height2, &option2, &e->elev.option, &e->option, &cp ) ) {
			return FALSE;
		}
		if (option2) e->elev.option |= ELEV_VISIBLE;
		GetArgs(cp, "fc", &ignoreFloat, &cp);
		return TRUE;
	}
	if ( cp != NULL ) {
		if (paramVersion < 7) {
			GetArgs( cp, "dfp", &e->elev.option,  &e->elev.u.height, &e->elev.doff, &cp );
			/*??*/return TRUE;
		}
		GetArgs( cp, "lpc", &option, &e->elev.doff, &cp );
		e->option = option >> 8;
		e->elev.option = (int)(option&0xFF);
		if ( (e->elev.option&ELEV_MASK) != ELEV_NONE ) {
			switch (e->elev.option&ELEV_MASK) {
			case ELEV_DEF:
				GetArgs( cp, "fc", &e->elev.u.height, &cp );
				break;
			case ELEV_STATION:
				GetArgs( cp, "qc", &e->elev.u.name, &cp );
				/*??*/break;
			default:
				;
			}
		}
	}
	return TRUE;
}


EXPORT BOOL_T bWriteEndPtDirectIndex = FALSE;
EXPORT BOOL_T bWriteEndPtExporting = FALSE;


EXPORT BOOL_T WriteEndPt( FILE * f, track_cp trk, EPINX_T ep )
{
	trkEndPt_p endPt = GetTrkEndPt(trk,ep);
	BOOL_T rc = TRUE;
	long option;

	CHECK ( endPt );
	if (bWriteEndPtDirectIndex && endPt->index > 0) {
		rc &= fprintf( f, "\tT4 %ld ", endPt->index )>0;
	} else if (endPt->track == NULL ||
		( bWriteEndPtExporting && !GetTrkSelected(endPt->track) ) ) {
		rc &= fprintf( f, "\tE4 " )>0;
	} else {
		rc &= fprintf( f, "\tT4 %d ", GetTrkIndex(endPt->track) )>0;
	}
	rc &= fprintf( f, "%0.6f %0.6f %0.6f", endPt->pos.x, endPt->pos.y, endPt->angle )>0; 
	option = (endPt->option<<8) | (endPt->elev.option&0xFF);
	if ( option != 0 ) {
		rc &= fprintf( f, " %ld %0.6f %0.6f", option, endPt->elev.doff.x, endPt->elev.doff.y )>0;
		switch ( endPt->elev.option&ELEV_MASK ) {
		case ELEV_DEF:
			rc &= fprintf( f, " %0.6f ", endPt->elev.u.height )>0;
			break;
		case ELEV_STATION:
			rc &= fprintf( f, " \"%s\" ", PutTitle( endPt->elev.u.name ) )>0;
			break;
		default:
			rc &= fprintf( f, " 0.0 ")>0;
		}
	} else {
		rc &= fprintf( f, " 0 0.0 0.0 0.0 ")>0;
	}
	if ((endPt->elev.option&ELEV_MASK) == ELEV_DEF)
		rc &= fprintf( f, "%0.6f ",endPt->elev.u.height)>0;
	else
		rc &= fprintf( f, "0.0 ")>0;
	long elevVisible = (endPt->elev.option&ELEV_VISIBLE)?1:0;
	long elevType = endPt->elev.option&ELEV_MASK;
	long gapType = endPt->option;
	rc &= fprintf( f, "%ld %ld %ld ", elevVisible, elevType, gapType)>0;
	rc &= fprintf( f, "%0.6f ", GetTrkElev( trk ) )>0;
	rc &= fprintf( f, "\n" )>0;
	return rc;
}


/************************************************************************
* Read/Write End Points
*/

wBool_t CompareEndPt(
	char * cp,
	track_p trk1,
	track_p trk2,
	EPINX_T ep )
{
	trkEndPt_p epp1 = GetTrkEndPt( trk1, ep );
	trkEndPt_p epp2 = GetTrkEndPt( trk2, ep );
	REGRESS_CHECK_POS( "Pos", epp1, epp2, pos )
	REGRESS_CHECK_ANGLE( "Angle", epp1, epp2, angle )
	// Actual EP connection
	int inx1 = -1;
	if ( epp1->track )
		inx1 = GetTrkIndex( epp1->track );
	// Expected EP connection.  endPt[inx].track is not resolved
	int inx2 = epp2->index;
	if ( inx1 != inx2 ) {
		sprintf( cp, "Index: Actual` %d, Expected %d\n", inx1, inx2 );
		return FALSE;
	}
	REGRESS_CHECK_INT( "Option", epp1, epp2, option )
	return TRUE;
}
#endif
