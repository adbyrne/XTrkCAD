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
  *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
  */

#include <string.h>

#include <wlib.h>
#include "misc.h"
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
		unsigned cnt = strlen(string) * 2 + 2;
		unsigned char *out = MyMalloc(cnt);
		wSystemToUTF8(string, out, cnt);
		MyFree(string);
		return(out);
	} else {
		return(string);
	}
}

/**
 * Convert a string from UTF-8 to system codepage in place. As the length of
 * the result most be equal or smaller than the input this is a safe
 * approach.
 *
 * \param [in,out] in the string to be converted
 */

void
ConvertUTF8ToSystem(unsigned char *in)
{
	if (wIsUTF8(in)) {
		unsigned cnt = strlen(in) * 2 + 1;
		unsigned char *out = MyMalloc(cnt);
		wUTF8ToSystem(in, out, cnt);
		strcpy(in, out);
		MyFree(out);
	}
}

/**
 * Requires convert to UTF-8 If at least one character is >127 the string
 * has to be converted.
 *
 * \param [in9 string the string.
 *
 * \returns True if conversion is required, false if not.
 */

bool
RequiresConvToUTF8(char *string)
{
	while (*string) {
		if (*string++ & 0x7F) {
			return(true);
		}
	}
	return(false);
}


