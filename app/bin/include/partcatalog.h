/** \file partcatalog.h
* Manage the catalog of track parameter files
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

#ifndef HAVE_TRACKCATALOG_H
#define HAVE_TRACKCATALOG_H

#include <stdbool.h>

#define MAXFILESPERCONTENT	10					/**< count of files with the same content header */
#define ESTIMATED_CONTENTS_WORDS 10				/**< average count of words in CONTENTS header */

struct sCatalogEntry {
    struct sCatalogEntry *next;
    unsigned files;								/**< current count of files */
    char *fullFileName[MAXFILESPERCONTENT];		/**< fully qualified file name */
    char *contents;								/**< content field of parameter file */
};

typedef struct sCatalogEntry CatalogEntry;

struct sIndexEntry {
    CatalogEntry *value;						/**< catalog entry having the key word in contents */
    char *keyWord;								/**< keyword */
};

typedef struct sIndexEntry IndexEntry;

struct sTrackLibrary {
    CatalogEntry *catalog;						/**< list of files cataloged */
    IndexEntry *index;							/**< Index for lookup */
    unsigned wordCount;							/**< How many words indexed */
    unsigned trackTypeCount;					/**< */
};

typedef struct sTrackLibrary
		TrackLibrary;							/**< core data structure for the catalog */

CatalogEntry *InitCatalog(void);
TrackLibrary *InitLibrary(void);
TrackLibrary *CreateLibrary(char *directory);
void DeleteLibrary(TrackLibrary *tracklib);
bool GetTrackFiles(TrackLibrary *trackLib, char *directory);
int GetParameterFileInfo(int files, char ** fileName, void * data);
unsigned CreateLibraryIndex(TrackLibrary *trackLib);
unsigned SearchLibrary(TrackLibrary *library, char *searchExpression, CatalogEntry *resultEntries);
unsigned CountCatalogEntries(CatalogEntry *listHeader);
void EmptyCatalog(CatalogEntry *listHeader);
unsigned SearchLibrary(TrackLibrary *library, char *searchExpression, CatalogEntry *resultEntries);
bool FilterKeyword(char *word);
#endif // !HAVE_TRACKCATALOG_H
