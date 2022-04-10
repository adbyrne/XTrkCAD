/** \file ctie.c
 * TIE
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

//#include "cselect.h"
//#include "custom.h"
//#include "fileio.h"
#include "layout.h"
//#include "param.h"
//#include "paths.h"
#include "track.h"
//#include "include/paramfile.h"
#include "common-ui.h"

static int log_tieList;

/****************************************************************************
*
* TIE DATA
*
*/

/**
* @brief Default tie data for a scale in tieLength, tieWidth, tieSpacing
*/
EXPORT void GetDefaultTieData( SCALEINX_T inx, tieData_p tieData ) 
{
	SCALEDESCINX_T scaleInx;
	GAUGEINX_T gaugeInx;
	GetScaleGauge( inx, &scaleInx, &gaugeInx );
			
	tieData->length = (96.0-54.0) / GetScaleRatio(inx) + GetScaleTrackGauge(inx);
	tieData->width = 16.0 / GetScaleRatio(inx);
	tieData->spacing = 2 * (tieData->width);
}
