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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <string.h>
#include <dynstring.h>
#include "misc.h"
#include "paths.h"

#define PATHS_SECTION "paths"
#define PATH_TYPE_SIZE	10

struct pathList {
	char type[ PATH_TYPE_SIZE ];	/**< type of path */
	DynString path;			/**< path */
	struct pathList *next;		/**< next entry in list */
};

static struct pathList *pathPreferences;

static void AddPathToList( const char *type, char *path );
static struct pathList *FindPathInList( const char *type );

/**
 * Find the path for a given type in the list
 *
 * \param type IN the searched type
 * \return the list entry or NULL if not found
 */
static struct pathList *
FindPathInList(const char *type)
{
	struct pathList *member = pathPreferences;

	while(member) {
		if(!strcmp(member->type, type)) {
			break;
		}
		member = member->next;
	}

	return member;
}

/**
 * Add a path to the list. If the type already exists, its path is replaced.
 *
 * \param type IN type of path
 * \param path IN path
 */
static void
AddPathToList(const char *type, char *path)
{
	struct pathList *newMember;

	if(!(newMember = FindPathInList(type))) {
		newMember = malloc(sizeof(struct pathList));
		CHECK(newMember);

		CHECK(strlen(type) < PATH_TYPE_SIZE);

		strcpy(newMember->type, type);
		DynStringMalloc( &newMember->path, strlen(path) + 1 );

		newMember->next = pathPreferences;
		pathPreferences = newMember;
	} else {
		DynStringClear(&(newMember->path));
	}

	DynStringCatCStr(&newMember->path, path);
}

/**
 * Update the current directory for a specific filetype. Get the directory part from the current file.
 * XTrackCAD keeps separate directories for different filetypes.(for trackplans, bitmaps and DXF files).
 * The paths are stored in a list keyed by the file type.
 *
 * \param pathType IN file type
 * \param fileName IN fully qualified filename
 *
 */

void SetCurrentPath(
        const char * pathType,
        const char * fileName)
{
	char *path;
	char *copy;
	CHECK(fileName != NULL);
	CHECK(pathType != NULL);
	copy = strdup(fileName);
	path = strrchr(copy, FILE_SEP_CHAR[0]);

	if (path) {
		*path = '\0';
		AddPathToList(pathType, copy);
		wPrefSetString(PATHS_SECTION, pathType, copy);
	}

	free(copy);
}

/**
 * Get the current path for a given file type. Following options are searched:
 * 1. the path list
 * 2. the preferences file for this type
 * 3. the older general setting (for backwards compatibility)
 * 4. the user's home directory as default
 * Search is finished when one of the options returns a valid path
 *
 * \param[in] pathType the type
 * \return pointer to path of NULL if not existing
 */

char *GetCurrentPath(
        const char *pathType)
{
	struct pathList *currentPath;
	const char *path;
	CHECK(pathType != NULL);
	currentPath = FindPathInList(pathType);

	if (currentPath) {
		return (DynStringToCStr(&(currentPath->path)));
	}

	path = wPrefGetString(PATHS_SECTION, pathType);

	if (!path) {
		path = wPrefGetString("file", "directory");
	}

	if (!path) {
		path = wGetUserHomeDir();
	}

	AddPathToList(pathType, (char *)path);
	return ((char *)path);
}

/**
 * Convert path to forward slash
 *
 * \param [in,out] string If non-null, the string.
 */

void ConvertPathForward(char *string)
{
	char *ptr = string;
	while ((ptr = strchr(ptr, '\\')) != NULL) {
		ptr[0] = '/';
		ptr++;
	}
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
	} else {
		name = path;
	}

	return (name);
}

/**
 * Find file extension in a filename
 *
 * \param path IN full or partial path
 * \return pointer to the file extension part, empty string if no extension present
 */

char *FindFileExtension(char *path)
{
	char *ext;
	ext = strrchr(path, '.');

	if (ext) {
		ext++;
	} else {
		ext = path + strlen(path);
	}

	return ext;
}

/**
* Make a full path definition from directories and filenames. The individual pieces are
* Concatenated. Where necessary a path delimiter is added. A pointer to the resulting
* string is returned. This memory should be free'd when no longer needed.
* Windows: to construct an absolute path, a leading backslash has to be included after
* the drive delimiter ':' or at the beginning of the first directory name.
*
* \param str OUT pointer to the complete path
* \param ... IN one or more parts of the path, terminate with NULL
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

	while ((part = va_arg(valist, const char *))) {
		if (part[0] !=separator[0] && lastchar && lastchar != separator[0] &&
		    lastchar != ':') {
			DynStringNCatCStr(&path, 1, separator);
		}

		DynStringCatCStr(&path, part);
		lastchar = part[strlen(part) - 1];
	}

	*str = strdup(DynStringToCStr(&path));
	DynStringFree(&path);
	va_end(valist);
}

/**
 * Add the default extension to a filename if it doesn'thave an extension.
 *
 * \param	filename the filename, the extension while be appended. Make sure that the buffer is large enough.
 * \param	default_ext	the extension to add if none is present
 *
 */
void AddDefaultExtension(char *filename, const char *default_ext)
{
	const char *dot = strrchr(filename, '.');
	char *slash = strrchr(filename, '/');
	char *backslash = strrchr(filename, '\\');

	// Find the last path separator (if any)
	const char *last_sep = (slash > backslash) ? slash : backslash;

	// Check if dot exists and comes after the last path separator
	// (to avoid treating directories like "folder.name" as having extensions)
	if (dot == NULL || (last_sep != NULL && dot < last_sep)) {
		// No extension found, add the default
		strcat(filename, default_ext);
	}
}
