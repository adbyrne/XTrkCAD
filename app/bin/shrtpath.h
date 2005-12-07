/* $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/shrtpath.h,v 1.1 2005-12-07 15:46:54 rc-flyer Exp $ */

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

typedef enum {
		SPTC_MATCH,				/* trk:ep is end of path? */
		SPTC_MATCHANY,			/* any EP matches? */
		SPTC_IGNNXTTRK,			/* don't traverse via trk:ep? */
		SPTC_ADD_TRK,			/* trk:ep is next on current path */
		SPTC_TERMINATE,			/* stop processing after current path? */
		SPTC_VALID				/* trk:ep is still valid? */
		 } SPTF_CMD;

typedef int (*shortestPathFunc_p)( SPTF_CMD cmd, track_p, EPINX_T, EPINX_T, DIST_T, void * );
int FindShortestPath( track_p, EPINX_T, BOOL_T, shortestPathFunc_p, void * );

extern int log_shortPath;
