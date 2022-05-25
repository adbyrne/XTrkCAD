/** \file trkendpt.h
 *
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


#ifndef TRKENDPT_H
#define TRKENDPT_H

#include "common.h"

void TempEndPtsReset( void );
void TempEndPtsSet( EPINX_T );
EPINX_T TempEndPtsCount( void );
trkEndPt_p TempEndPt( EPINX_T );
trkEndPt_p TempEndPtsAppend( void );
trkEndPt_p TempEndPtsCopy();

CSIZE_T EndPtSize( EPINX_T );
coOrd GetEndPtPos( trkEndPt_p );
ANGLE_T GetEndPtAngle( trkEndPt_p );
void SetEndPt( trkEndPt_p, coOrd, ANGLE_T );
track_p GetEndPtTrack( trkEndPt_p );
EPINX_T GetEndPtEndPt( trkEndPt_p );
void SetEndPtTrack( trkEndPt_p, track_p);
void SetEndPtEndPt( trkEndPt_p, EPINX_T );
trkEndPt_p EndPtIndex( trkEndPt_p, EPINX_T );

void SwapEndPts( trkEndPt_p, EPINX_T, EPINX_T );
#endif
