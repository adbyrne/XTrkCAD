/** \file ccurve.h
 * Definitions for curve commands
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

#ifndef HAVE_CCURVE_H
#define HAVE_CCURVE_H

#include "draw.h"
#include "track.h"
#include "wlib.h"
#include "utility.h"

typedef struct {
		curveType_e type;
		coOrd curvePos;
		coOrd pos1;
		DIST_T curveRadius;
		ANGLE_T a0, a1;
		BOOL_T negative;
		} curveData_t;

#define crvCmdFromEP1			(0)
#define crvCmdFromTangent		(1)
#define crvCmdFromCenter		(2)
#define crvCmdFromChord			(3)

#define circleCmdFixedRadius	(0)
#define circleCmdFromTangent	(1)
#define circleCmdFromCenter		(2)

typedef void (*curveMessageProc)( char *, ... );
STATUS_T CreateCurve( wAction_t, coOrd, BOOL_T, wDrawColor, DIST_T, long, curveMessageProc );
int IsCurveCircle( track_p );
void PlotCurve( long, coOrd, coOrd, coOrd, curveData_t *, BOOL_T );
track_p NewCurvedTrack( coOrd, DIST_T, ANGLE_T, ANGLE_T, long );
DIST_T CurveDescriptionDistance( coOrd, track_p );
STATUS_T CurveDescriptionMove( track_p, wAction_t, coOrd );
BOOL_T GetCurveMiddle( track_p , coOrd * );

#endif // !HAVE_CCURVE_H
