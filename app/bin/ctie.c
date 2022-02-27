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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
EXPORT void defaultTieData( SCALEINX_T inx, DIST_T *tieLength, DIST_T *tieWidth, DIST_T *tieSpacing ) 
{
	SCALEDESCINX_T scaleInx;
	GAUGEINX_T gaugeInx;
	GetScaleGauge( inx, &scaleInx, &gaugeInx );
			
	*tieLength = (96.0-54.0) / GetScaleRatio(inx) + GetScaleTrackGauge(inx);
	*tieWidth = 16.0 / GetScaleRatio(inx);
	*tieSpacing = 2 * (*tieWidth);
}

EXPORT tieData_t GetTieData( track_cp trk )
{
	tieData_t td;
	return td;
}

