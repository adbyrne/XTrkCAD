/**
 * \file   listelem.c
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

#include <string.h>

#include "common.h"
#include "listelem.h"


int lookupListIndex;

void* LookupListElem(
        dynArr_t* da,
        void* key,
        int (*cmpFunc)(void*, void*),
        int elem_size)
{
	int hi, lo, mid;
	lo = 0;
	hi = da->cnt - 1;
	while (lo <= hi) {
		int rc;
		mid = (lo + hi) / 2;
		rc = cmpFunc(key, DYNARR_N(void*, *da, mid));
		if (rc == 0) {
			lookupListIndex = mid;
			return DYNARR_N(void*, *da, mid);
		}
		if (rc > 0) {
			lo = mid + 1;
		} else {
			hi = mid - 1;
		}
	}
	if (elem_size == 0) {
		lookupListIndex = -1;
		return NULL;
	}
	DYNARR_APPEND(void*, *da, 10);
	for (mid = da->cnt - 1; mid > lo; mid--) {
		DYNARR_N(void*, *da, mid) = DYNARR_N(void*, *da, mid - 1);
	}
	DYNARR_N(void*, *da, lo) = MyMalloc(elem_size);
	memset(DYNARR_N(void*, *da, lo), 0, elem_size);
	lookupListIndex = lo;
	return DYNARR_N(void*, *da, lo);
}

void RemoveListElem(
        dynArr_t* da,
        const void* elem)
{
	int inx;
	for (inx = 0; inx < da->cnt; inx++)
		if (DYNARR_N(void*, *da, inx) == elem) {
			break;
		}
	CHECK(inx < da->cnt);
	for (inx++; inx < da->cnt; inx++) {
		DYNARR_N(void*, *da, inx - 1) = DYNARR_N(void*, *da, inx);
	}
	da->cnt--;
}
