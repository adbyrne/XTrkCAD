/**
 * \file utf8convert.c
 *
 * UTF8 conversion functions
 */

/*  XTrackCad - Model Railroad CAD
 *  Copyright (C) 2020 Martin Fischer
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

#include "common.h"
#include "include/utf8convert.h"

/**
 * Convert to UTF-8. The string must by a dynamically allocated storage block
 * allocated with MyMalloc(). The functions returns a pointer to the converted
 * string. If no conversion was necessary the returned string is identical to
 * the parameter. Otherwise a new buffer is allocated and returned.
 *
 * \param [in] string If non-null, the string.
 *
 * \returns a pointer to a MyMalloc'ed string in UTF-8 format.
 */

char *
Convert2UTF8( char *string )
{
	if (RequiresConvToUTF8(string)) {
		// A code page's byte is at most three bytes of UTF-8
		size_t cnt = strlen(string) * 3 + 1;
		char *out = MyMalloc(cnt);
		if (wSystemToUTF8(string, out, (unsigned int)cnt)) {
			MyFree(string);
			return(out);
		}
		// Not in the system code page: leave it as it was
		MyFree(out);
	}
	return(string);
}

/**
 * Convert a string from UTF-8 to the system code page in place. The string
 * is left as it was if it's ASCII or not UTF-8, if a character isn't in the
 * code page, or if the result would be longer than the string (possible in
 * some multi-byte code pages).
 *
 * \param [in,out] in the string to be converted
 */

void
ConvertUTF8ToSystem(char *in)
{
	if (RequiresConvToUTF8(in) && wIsUTF8(in)) {
		size_t len = strlen(in);
		size_t cnt = len * 2 + 1;
		char *out = MyMalloc(cnt);
		if (wUTF8ToSystem(in, out, (unsigned int)cnt) && strlen(out) <= len) {
			strcpy(in, out);
		}
		MyFree(out);
	}
}

/**
 * Requires convert to UTF-8 If at least one character is >127 the string
 * has to be converted.
 *
 * \param [in] string the string, or NULL (a missing manifest entry).
 *
 * \returns True if conversion is required, false if not.
 */

bool
RequiresConvToUTF8(char *string)
{
	if (string == NULL) {
		return(false);
	}
	while (*string) {
		if (*string++ & 0x80) {
			return(true);
		}
	}
	return(false);
}


