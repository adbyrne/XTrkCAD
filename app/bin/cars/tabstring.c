
/**
 * \file   tabstring.c
 * \brief  Handle tab separated strings
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

/*
 *  Utilities
 */

#include <string.h>

#include "common.h"
#include "tabstring.h"

void TabStringExtract(
        char* string,
        int count,
        tabString_t* tabs)
{
	int inx;
	char* next = string;

	for (inx = 0; inx < count; inx++) {
		tabs[inx].ptr = string;
		if (next) {
			next = strchr(string, '\t');
		}
		if (next) {
			tabs[inx].len = (unsigned long)(next - string);
			string = next + 1;
		} else {
			if (string) {
				tabs[inx].len = (unsigned long)strlen(string);
			} else {
				tabs[inx].len = 0;
			}
			string += tabs[inx].len;
		}
	}
	if (tabs[T_MANUF].len == 0) {
		tabs[T_MANUF].len = strlen(_("Unknown"));
		tabs[T_MANUF].ptr = _("Unknown");
	}
}


char* TabStringDup(
        const tabString_t* tab)
{
	char* ret;
	ret = MyMalloc(tab->len + 1);
	memcpy(ret, tab->ptr, tab->len);
	ret[tab->len] = '\0';
	return ret;
}


char* TabStringCpy(
        char* dst,
        const tabString_t* tab)
{
	memcpy(dst, tab->ptr, tab->len);
	dst[tab->len] = '\0';
	return dst + tab->len;
}


int TabStringCmp(
        const char* src,
        const tabString_t* tab)
{
	int srclen = (int)strlen(src);
	int len = srclen;
	int rc;
	if (len > tab->len) {
		len = tab->len;
	}
	rc = strncasecmp(src, tab->ptr, len);
	if (rc != 0 || srclen == tab->len) {
		return rc;
	} else if (srclen > tab->len) {
		return 1;
	} else {
		return -1;
	}
}

/**
 * \brief Converts a tabString to a long integer without modifying the source string.
 * \param tab The tabString to convert.
 * \return The converted long value, or 0 if the tab is empty or invalid.
 */

long TabGetLong(
        const tabString_t* tab)
{
	long val = 0;
	// Buffer large enough for a 64-bit integer string plus sign.
	char buf[21];

	if (tab->len > 0 && tab->len < (int)sizeof(buf)) {
		memcpy(buf, tab->ptr, tab->len);
		buf[tab->len] = '\0';
		val = atol(buf);
	}
	return val;
}

/**
 * Converts a tabString to a FLOAT_T without modifying the source string.
 * \param tab The tabString to convert.
 * \return The converted float value, or 0.0 if the tab is empty or invalid.
 */

FLOAT_T TabGetFloat(
        const tabString_t* tab)
{
	if (!tab || tab->len == 0) {
		return 0.0;
	}
	// A buffer for floating point numbers. 40 chars should be sufficient.
	char buf[40];
	if (tab->len >= (int)sizeof(buf)) {
		// String too long, handle as error.
		return 0.0;
	}
	memcpy(buf, tab->ptr, tab->len);
	buf[tab->len] = '\0';
	return (FLOAT_T)atof(buf);
}
