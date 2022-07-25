/** \file scale.h
 * 
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

#ifndef SCALE_H
#define SCALE_H

#include "common.h"

extern DIST_T curScaleRatio;
extern char * curScaleName;
DIST_T GetScaleTrackGauge( SCALEINX_T );
DIST_T GetScaleRatio( SCALEINX_T );
char * GetScaleName( SCALEINX_T );
SCALEINX_T GetScaleInx( SCALEDESCINX_T scaleInx, GAUGEINX_T gaugeInx );

void GetScaleEasementValues( DIST_T *, DIST_T * );
DIST_T GetScaleMinRadius( SCALEINX_T );
void ValidateTieData( tieData_p );
tieData_t GetScaleTieData( SCALEINX_T );
SCALEINX_T LookupScale( const char * );
BOOL_T GetScaleGauge( SCALEINX_T scaleInx, SCALEDESCINX_T *scaleDescInx, GAUGEINX_T *gaugeInx);
void SetScaleGauge(SCALEDESCINX_T desc, GAUGEINX_T gauge);
BOOL_T DoSetScale( char * );

void ScaleLengthIncrement( SCALEINX_T, DIST_T );
void ScaleLengthEnd( void );
void LoadScaleList( wList_p );
void LoadGaugeList( wList_p, SCALEDESCINX_T );

typedef enum {FIT_STRUCTURE, FIT_TURNOUT, FIT_CAR} SCALE_FIT_TYPE_T;
typedef enum {FIT_NONE, FIT_COMPATIBLE, FIT_EXACT} SCALE_FIT_T;
SCALE_FIT_T CompatibleScale( SCALE_FIT_TYPE_T, SCALEINX_T, SCALEINX_T );

SCALE_FIT_T FindScaleCompatible(SCALE_FIT_TYPE_T type, char * scale1, char * scale2);

BOOL_T DoAllSetScaleDesc( void );

void DoRescale( void *unused );

EXPORT void ScaleInit( void );

#endif
