/** \file paths.c
* Path and file name handling functions.
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#ifdef WINDOWS
#include <windows.h>
#if _MSC_VER >=1400
#define strdup _strdup
#endif
#endif

#include <wlib.h>
#include <dynstring.h>
#include "track.h"
#include "common.h"
#include "utility.h"
#include "misc.h"
#include "i18n.h"
#include "uthash.h"
#include "paths.h"

struct pathTable {
	char	  type[ PATH_TYPE_SIZE]; 	   	/**< type of path */
	DynString path;							/**< path */
	UT_hash_handle hh; 						/**< makes this structure hashable */
};

static struct pathTable *paths;

static void AddPath(const char *type, char*path);
static struct pathTable *FindPath(const char *type);

#define PATHS_SECTION "paths"

/**
* Find the path for a given type in the hash table
*
* \param type IN the searched type
* \param entry OUT the table entry
* \return
*/

static struct pathTable *
FindPath(const char *type)
{
	struct pathTable *entry;
	HASH_FIND_STR(paths, type, entry);
	return(entry);
}

/**
 * Add a path to the table. If it already exists, the value ist updated.
 *
 * \param type IN type of path
 * \param path  IN path 
 */
static void
AddPath(const char *type, char*path)
{
	struct pathTable *tableEntry;

	tableEntry = FindPath(type);
	if (tableEntry) {
		DynStringClear(&(tableEntry->path));
	}
	else {
		tableEntry = malloc(sizeof(struct pathTable));
		DynStringMalloc(&tableEntry->path, 16);
		strcpy(tableEntry->type, type);
		HASH_ADD_STR(paths, type, tableEntry);
	}

	DynStringCatCStr(&(tableEntry->path), path);
}

/**
 * Update the current directory for a specific filetype. Get the directory part from the current file. 
 * XTrackCAD keeps seprate directories for different filetypes.(for trackplans, bitmaps and DXF files). 
 * The paths are stored in a hash table using the file type as the hash.   
 *
 * \param pathType IN file type
 * \param fileName IN fully qualified filename
 * \return 
 *
 */

void SetCurrentPath(
	const char * pathType,
	const char * fileName )
{
	char *path;
	char *copy;

	assert( fileName != NULL );
	assert( pathType != NULL );

	copy = strdup( fileName );
	path = strrchr(copy, FILE_SEP_CHAR[0] );
	if ( path ) 
	{
		*path = '\0';
		AddPath(pathType, copy);
		wPrefSetString(PATHS_SECTION, pathType, copy);
	}    
	free( copy );
}

/**
 * Get the current path for a given file type. Following options are searched:
 * 1. the hash array
 * 2. the preferences file for this type
 * 3. the older general setting (for backwards compatibility)
 * 4. the user's home directory as default
 * Search is finished when one of the options returns a valid path
 *
 * \param IN pathType the type
 * \return pointer to path of NULL if not existing
 */

char *GetCurrentPath(
	const char *pathType)
{
	struct pathTable *currentPath;
	const char *path;

	assert(pathType != NULL);

	currentPath = FindPath(pathType);

	if (currentPath) {
		return(DynStringToCStr(&(currentPath->path)));
	}
	
	path = wPrefGetString(PATHS_SECTION, pathType);
	if (!path)
		path = wPrefGetString("file", "directory");
	if (!path)
		path = wGetUserHomeDir();
	AddPath(pathType, (char *)path);

	return((char *)path);
}

/**
* Find the filename/extension piece in a fully qualified path
*
* \param path IN the full path
* \return pointer to the filename part
*/

char *FindFilename(char *path)
{
	char *name;
	name = strrchr(path, FILE_SEP_CHAR[0]);
	if (name) {
		name++;
	}
	else {
		name = path;
	}
	return(name);
}

/**
* Make a full path definition from directorys and filenames. The individual pieces are
* concatinated. Where necessary a path delimiter is added. A pointer to the resulting 
* string is returned. This memory should be free'd when no longer needed. 
* Windows: to construct an absolute path, a leading backslash has to be included after
* the drive delimiter ':' or at the beginning of the first directory name.
* 
* \param str OUT pointer to the complete path
* \param ... IN one or more parts of the path
*/

void
MakeFullpath(char **str, ...)
{
	va_list valist;
	const char *part;
	char *separator = FILE_SEP_CHAR;
	char lastchar = '\0';
	DynString path;

	DynStringMalloc(&path, 0);
	va_start(valist, str);

	while ( part = va_arg(valist, const char *)) {
		if(part[0] !=separator[0] && lastchar && lastchar != separator[0] && lastchar != ':')
			DynStringNCatCStr(&path, 1, separator);
		DynStringCatCStr(&path, part );
		lastchar = part[strlen(part) - 1];	
	}
	*str = strdup(DynStringToCStr(&path));
	DynStringFree(&path);
}
