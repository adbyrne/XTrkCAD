/** \file trackx.h
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifndef TRACKX_H
#define TRACKX_H

#include "common.h"
#include "track.h"

struct extraData;

typedef struct track_t {
		struct track_t *next;
		TRKINX_T index;
		TRKTYP_T type;
		unsigned int layer;
		signed char scale;
		BOOL_T modified:1;
		BOOL_T deleted:1;
		BOOL_T new:1;
		unsigned int width:2;
		unsigned int elevMode:2;
		unsigned int bits:9;
		EPINX_T endCnt;
		trkEndPt_p endPt;
		struct { float x; float y; } lo, hi;
		struct extraData * extraData;
		CSIZE_T extraSize;
		DIST_T elev;
		} track_t;

extern track_p to_first;
extern track_p * to_last;
#define TRK_ITERATE(TRK)		for (TRK=to_first; TRK!=NULL; TRK=TRK->next) if (!(TRK->deleted)) 
#endif
