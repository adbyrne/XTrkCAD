/** \file layout.c
 * Layout data 
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2017 Martin Fischer 
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
#include <dynstring.h>
#include "paths.h"

struct sDataLayout {
	DynString	fullFileName;
};

static struct sDataLayout thisLayout;

/**
* Update the full file name.
*
* \param filename IN the new filename
*/

void
SetLayoutFullPath(const char *fileName)
{
	if (isnas(&thisLayout.fullFileName)) {
		DynStringMalloc(&thisLayout.fullFileName, strlen(fileName) + 1);
	}
	else {
		DynStringClear(&thisLayout.fullFileName);
	}

	DynStringCatCStr(&thisLayout.fullFileName, fileName);
}

/**
* Return the full filename.
*
* \return    pointer to the full filename, should not be modified or freed
*/

char *
GetLayoutFullPath()
{
	return(DynStringToCStr(&thisLayout.fullFileName));
}

/**
* Return the filename part of the full path
*
* \return    pointer to the filename part, NULL is no filename is set
*/

char *
GetLayoutFilename()
{
	char *string = DynStringToCStr(&thisLayout.fullFileName);

	if (string)
		return(FindName(string));
	else
		return(NULL);
}