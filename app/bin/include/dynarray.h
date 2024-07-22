/** \file dynarray.h
 * Macros for dynamic arrays
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

#ifndef DYNARRAY_H
#define DYNARRAY_H

  // DYNARRAY

typedef struct {
	int cnt;
	int max;
	void* ptr;
} dynArr_t;

#define CHECK_SIZE(T,DA)

/**
 * Append INCR mambers to DA
 * INCR is > 1 if we plan to add more members soon
 * Increments .cnt for the next member
 * Note: new members may not be empty
 */
#define DYNARR_APPEND(T,DA,INCR) \
		{ if ((DA).cnt >= (DA).max) { \
			(DA).max += (INCR); \
			CHECK_SIZE((T),(DA)) \
			(DA).ptr = MyRealloc( (DA).ptr, (DA).max * sizeof *(T*)NULL ); \
			if ( (DA).ptr == NULL ) \
				abort(); \
		} \
		(DA).cnt++; }

 /**
  * Return Last member of DA
  */
#define DYNARR_LAST(T,DA) \
		(((T*)(DA).ptr)[(DA).cnt-1])
  /**
   * Return N't member of DA
   */
#define DYNARR_N(T,DA,N) \
		(((T*)(DA).ptr)[N])
   /**
	* Logically empty the DA
	* .max and .ptr are untouched
	*/
#define DYNARR_RESET(T,DA) \
		(DA).cnt=0
	/**
	 * Set number of members to N
	 * If extending (.cnt > .max ), new values will be 0'd, otherwise not
	 */
#define DYNARR_SET(T,DA,N) \
		{ if ((DA).max < (N)) { \
			(DA).max = (N); \
			CHECK_SIZE((T),(DA)) \
			(DA).ptr = MyRealloc( (DA).ptr, (DA).max * sizeof *(T*)NULL ); \
			if ( (DA).ptr == NULL ) \
				abort(); \
		} \
		(DA).cnt = (N); }
	 /**
	  * Initializes DA to empty when .ptr might be garbage (ie local vars)
	  * All fields are cleared
	  */
#define DYNARR_INIT(T,DA) \
		{ (DA).ptr = NULL; \
		(DA).max = 0; \
		(DA).cnt = 0; \
		}
	  /**
	   * Initializes DA to empty and frees .ptr
	   */
#define DYNARR_FREE(T,DA) \
		{ if ((DA).ptr) { \
			MyFree( (DA).ptr); \
			(DA).ptr = NULL; \
		} \
		(DA).max = 0; \
		(DA).cnt = 0; }
	   /**
		* Removes N'th member from DA
		* (Not used)
		*/
#define DYNARR_REMOVE(T,DA,N) \
		{ \
		 { if ((DA).cnt-1 > (N)) { \
				for (int i=(N);i<(DA).cnt-1;i++) { \
				(((T*)(DA).ptr)[i])= (((T*)(DA).ptr)[i+1]); \
				} \
			} \
		 } \
		if ((DA).cnt>=(N)) (DA).cnt--; \
		}

#endif