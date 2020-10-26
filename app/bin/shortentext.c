/** \file stringutils.c
 * Some assorted string handling functions
 */

 /*  XTrackCAD - Model Railroad CAD
  *  Copyright (C) 2019 Martin Fischer
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
#include "shortentext.h"

#define WHITESPACES "\n\r\t "
#define ELLIPSIZE "..."

/**
 * Replace all whitespace characters with blanks. Successive occurences are reduced to a single blank.
 * 
 * \param source IN string to convert
 * \param dest IN buffer for converted string, minimum size is the size of the source string
 */
void
RemoveFormatChars( char *source, char *dest )
{
	int lastChar = '\0';

	while (*source) {
		if (strchr(WHITESPACES, *source)) {
			if (lastChar != ' ') {
				*dest++ = ' ';
				lastChar = ' ';
			}
		} else {
			lastChar = *source;
			*dest++ = lastChar;
		}
		source++;
	}
	if (lastChar != ' ') {
		*dest = '\0';
	} else {
		*(dest - 1) = '\0';
	}
}

void
EllipsizeString(char *source, char *dest, size_t length)
{
	size_t position;
	char *resultString = (dest ? dest: source);


	// trivial case: nothing to do if source is shorter and no inplace
	if( strlen(source)  <= length )
	{
		if( dest )
			strcpy(dest, source);
		return;
	}

	strncpy(resultString, source, length);
	resultString[ length ] = '\0';

	position = length - 1;
	while (position) {
		if (resultString[position] == ' ' && position <= (length - sizeof(ELLIPSIZE))) {
			strcpy(resultString + position, ELLIPSIZE);
			break;
		} else {
			position--;
		}
	}

	// no blank in string, replace the last n chars 
	if (!position) {
		strcpy(resultString + (strlen(resultString) - sizeof(ELLIPSIZE) + 1), ELLIPSIZE);
	}
	return;
}