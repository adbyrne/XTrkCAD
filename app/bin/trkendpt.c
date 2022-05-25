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


#endif
