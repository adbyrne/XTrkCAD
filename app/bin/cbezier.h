/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/cbezier.h,v 1.1 2005-12-07 15:47:36 rc-flyer Exp $
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


typedef struct {
		coOrd pos[4];
		DIST_T minCurveRadius;
		ANGLE_T a0, a1;
		} BezierData_t;


typedef void (*bezMessageProc)( char *, ... );
STATUS_T CreateBezier( wAction_t, coOrd, BOOL_T, wDrawColor, DIST_T, long, bezMessageProc );
int IsBezier( track_p );
track_p NewBezierTrack( coOrd, DIST_T, ANGLE_T, ANGLE_T, long );
DIST_T BezierDescriptionDistance( coOrd, track_p );
STATUS_T BezierDescriptionMove( track_p, wAction_t, coOrd );
BOOL_T GetBeziereMiddle( track_p, coOrd * );
int DrawControlArm(trkSeg_p, coOrd, coOrd, BOOL_T, BOOL_T, wDrawColor, wDrawColor, wDrawColor );
