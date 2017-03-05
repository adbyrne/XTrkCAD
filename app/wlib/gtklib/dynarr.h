
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


#ifndef DYNARR_H_SEEN
#define DYNARR_H_SEEN

typedef struct {
		int cnt;
		int max;
		void * ptr;
		} dynArr_t;

#define DYNARR_APPEND(T,DA,INCR) \
		{ if ((DA).cnt >= (DA).max) { \
			(DA).max += INCR; \
			(DA).ptr = realloc( (DA).ptr, (DA).max * sizeof *(T*)NULL ); \
			if ( (DA).ptr == NULL ) \
				abort(); \
		} \
		(DA).cnt++; }
#define DYNARR_ADD(T,DA,INCR) DYNARR_APPEND(T,DA,INCR)

#define DYNARR_LAST(T,DA) \
		(((T*)(DA).ptr)[(DA).cnt-1])
#define DYNARR_N(T,DA,N) \
		(((T*)(DA).ptr)[N])
#define DYNARR_RESET(T,DA) \
		(DA).cnt=0
#define DYNARR_SET(T,DA,N) \
		{ if ((DA).max < N) { \
			(DA).max = N; \
			(DA).ptr = realloc( (DA).ptr, (DA).max * sizeof *(T*)NULL ); \
			if ( (DA).ptr == NULL ) \
				abort(); \
		} \
		(DA).cnt = 0; }
#endif /* DYNARR_H_SEEN */
