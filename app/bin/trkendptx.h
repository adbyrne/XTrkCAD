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


#ifndef TRKENDPTX_H
#define TRKENDPTX_H

#include "common.h"
#include "track.h"

typedef struct {
	int option;
	coOrd doff;
	union {
		DIST_T height;
		char * name;
	} u;
	BOOL_T cacheSet;
	double cachedElev;
	double cachedGrade;
} elev_t;

typedef struct trkEndPt_t {
	coOrd pos;
	ANGLE_T angle;
	track_p track;
	long index;
	elev_t elev;
	long option;
} trkEndPt_t;

#endif
