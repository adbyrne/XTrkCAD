/**
 * \file   transform_pts.c
 * \brief
 *
 * \author Martin Fischer
 */

/*  XTrackCad - Model Railroad CAD
 *  Copyright (C) 2005, 2025 Dave Bullis
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

#include "transform_pts.h"
#include <common.h>
#include "utility.h"

void RotatePts(
        int cnt,
        coOrd* pts,
        coOrd orig,
        ANGLE_T angle)
{
	int inx;
	for (inx = 0; inx < cnt; inx++) {
		Rotate(&pts[inx], orig, angle);
	}
}


void RescalePts(
        int cnt,
        coOrd* pts,
        FLOAT_T scale_x,
        FLOAT_T scale_y)
{
	int inx;
	for (inx = 0; inx < cnt; inx++) {
		pts[inx].x *= scale_x;
		pts[inx].y *= scale_y;
	}
}
