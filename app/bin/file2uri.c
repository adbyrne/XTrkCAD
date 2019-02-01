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
  *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
  */
  
#include <string.h>
#include "wlib.h"

#define FILEPROTOCOL ("file:///")
static char *reservedChars = "?#[]@!$&'()*+,;= ";

unsigned 
File2URI(char *fileName, unsigned resultLen, char *resultBuffer)
{
	char *currentSource = fileName;
	char *currentDest; 

	if (resultLen >= strlen(FILEPROTOCOL) + 1) {
		strcpy(resultBuffer, FILEPROTOCOL);
	}

	currentDest = resultBuffer + strlen(resultBuffer);
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
	return(strlen(resultBuffer));
}

unsigned 
URI2File(char *encodedFileName, unsigned resultLen, char *resultBuffer)
{
	char *currentSource = encodedFileName;
	char *currentDest = resultBuffer;

	if (strncmp(encodedFileName, FILEPROTOCOL, sizeof( FILEPROTOCOL )-1)) {
		return(0);
	}
	currentSource = encodedFileName + sizeof(FILEPROTOCOL) - 1;
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
	return(strlen(resultBuffer));
}
