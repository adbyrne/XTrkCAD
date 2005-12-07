/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/cjoin.h,v 1.1 2005-12-07 15:47:39 rc-flyer Exp $
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

#define E_NOTREQ		(0)
#define E_REQ			(1)
#define E_ERROR			(2)

typedef struct {
		DIST_T x;
		DIST_T r0, r1;
		DIST_T l0, l1;
		DIST_T d0, d1;
		BOOL_T flip, negate, Scurve;
		} easementData_t;

extern DIST_T easementVal;
extern DIST_T easeR;
extern DIST_T easeL;

STATUS_T ComputeJoint( DIST_T, DIST_T, easementData_t * );
BOOL_T JoinTracks( track_p, EPINX_T, coOrd, track_p, EPINX_T, coOrd, easementData_t * );
void UndoJoint( track_p, EPINX_T, track_p, EPINX_T );
void DrawJointTrack( drawCmd_p, coOrd, ANGLE_T, DIST_T, DIST_T, DIST_T, DIST_T, BOOL_T, BOOL_T, BOOL_T, track_p, EPINX_T, EPINX_T, DIST_T, wDrawColor, long );
DIST_T JointDistance( coOrd *, coOrd, ANGLE_T, DIST_T, DIST_T, DIST_T, DIST_T, BOOL_T, BOOL_T );
coOrd GetJointSegEndPos( coOrd, ANGLE_T, DIST_T, DIST_T, DIST_T, DIST_T, BOOL_T, BOOL_T, BOOL_T, EPINX_T, ANGLE_T * );
