/** \file file2uri.c
 * Conversion for filename to URI and reverse
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
  *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
  */
  
#include "common.h"

static char *reservedChars = "?#[]@!$&'()*+,;= ";

unsigned 
File2URI(char *fileName, unsigned resultLen, char *resultBuffer)
{
	char *currentSource = fileName;
	char *currentDest; 

	currentDest = resultBuffer;

	while (*currentSource && ((unsigned)(currentDest - resultBuffer) < resultLen - 1)) {
		if (*currentSource == FILE_SEP_CHAR[ 0 ]) {
			*currentDest++ = '/';
			currentSource++;
			continue;
		}
		if (strchr(reservedChars, *currentSource))
		{
			sprintf(currentDest, "%%%02x", *currentSource);
			currentSource++;
			currentDest += 3;
		} else {
			*currentDest++ = *currentSource++;
		}

	}
	*currentDest = '\0';
	return((unsigned)strlen(resultBuffer));
}

unsigned 
URI2File(char *encodedFileName, unsigned resultLen, char *resultBuffer)
{
	char *currentSource = encodedFileName;
	char *currentDest = resultBuffer;

	currentSource = encodedFileName;
	while (*currentSource && ((unsigned)(currentDest - resultBuffer) < resultLen - 1)) {
		if (*currentSource == '/') {
			*currentDest++ = FILE_SEP_CHAR[0];
			currentSource++;
			continue;
		}
		if (*currentSource == '%') {
			char hexCode[3];
			memcpy(hexCode, currentSource + 1, 2);
			hexCode[2] = '\0';
			sscanf(hexCode, "%x", (unsigned int*)currentDest);
			currentDest++;
			currentSource += 3;
		} else {
			*currentDest++ = *currentSource++;
		}
	}
	*currentDest = '\0';
	return((unsigned)strlen(resultBuffer));
}
