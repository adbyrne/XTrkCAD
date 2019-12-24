/** \file readxtp.c
* Get information from parameter files
*/
/*  XTrkCad - Model Railroad CAD
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "include/readxtp.h"

#if _MSC_VER > 1300
#define stricmp _stricmp
#define strnicmp _strnicmp
#define strdup _strdup
#endif

/**
 * Get the contents description from a parameter file. Returned string has to be freed after use.
 * 
 * \param file IN xtpfile
 * \return pointer to found contents or NULL if not present
 */

char *
GetParameterFileContent(char *file)
{
	FILE *fh;
	char *result = NULL;

	fh = fopen( file, "rt" );
	if (fh) {
		bool found = false;

		while (!found) {
			char buffer[512];
			if (fgets(buffer, sizeof(buffer), fh)) {
				char *ptr = strtok(buffer, " \t");

				if (!stricmp(ptr, CONTENTSCOMMAND )) {
					/* if found, store the rest of the line and the filename	*/
					ptr = strtok(NULL, "\t\n");
					result = strdup(ptr);
					found = true;
				}
			}
			else {
				fprintf(stderr, "Nothing found in %s\n", file);
				found = true;
			}
		}
		fclose(fh);
	}
	return(result);
}

