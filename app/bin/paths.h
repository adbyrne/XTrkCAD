/** \file paths.h
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

#ifndef HAVE_PATHS_H
#define HAVE_PATHS_H

void SetCurrentPath( const char * pathType,	const char * fileName );
char *GetCurrentPath(const char *pathType);
void ConvertPathForward(char *string);
char *FindFilename(char *path);
char *FindFileExtension(char *path);
void MakeFullpath(char **str, ...);
#endif
