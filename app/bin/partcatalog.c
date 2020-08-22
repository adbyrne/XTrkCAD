/** \file partcatalog.c
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

#include <assert.h>
#include <ctype.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <search.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef WINDOWS
    #include "include/dirent.h"
#else
	#include <dirent.h>
#endif
#include "fileio.h"
#include "misc.h"
#include "include/paramfile.h"
#include "include/partcatalog.h"
#include "paths.h"
#include "include/stringxtc.h"
#include "include/utf8convert.h"

#if _MSC_VER > 1300
    #define strnicmp _strnicmp
    #define strdup _strdup
#endif

#define PUNCTUATION "+-*/.,&%=#"



/**
 * Create and initialize the linked list for the catalog entries
 *
 * \return pointer to first element
 */

CatalogEntry *
InitCatalog(void)
{
    CatalogEntry *head;
    CatalogEntry *tail;

    /* allocate two pseudo nodes for beginning and end of list */
    head = (CatalogEntry *)malloc(sizeof(CatalogEntry));
    tail = (CatalogEntry *)malloc(sizeof(CatalogEntry));

    head->next = tail;
    tail->next = tail;



    return (head);
}

/**
 * Create a new CatalogEntry and add it to the linked list. The newly
 * created entry is inserted into the list after the given position
 *
 * \param entry IN insertion point
 * \return pointer to new entry
 */

static CatalogEntry *
InsertIntoCatalogAfter(CatalogEntry *entry)
{
    CatalogEntry *newEntry = (CatalogEntry *)malloc(sizeof(CatalogEntry));
    newEntry->next = entry->next;
    entry->next = newEntry;
    newEntry->files = 0;
    newEntry->contents = NULL;

    return (newEntry);
}

/**
 * Count the elements in the linked list ignoring dummy elements
 *
 * \param listHeader IN the linked list
 * \return the numberof elements
 */

unsigned
CountCatalogEntries(CatalogEntry *listHeader)
{
    CatalogEntry *currentEntry = listHeader->next;
    unsigned count = 0;

    while (currentEntry != currentEntry->next) {
        count++;
        currentEntry = currentEntry->next;
    }
    return (count);
}

/**
 * Empty a catalog. All data nodes including their allocated memory are freed. On
 *
 * \param listHeader IN the list
 */

void
EmptyCatalog(CatalogEntry *listHeader)
{
    CatalogEntry *current = listHeader;

    while (current->next != current->next->next) {
        CatalogEntry *removedElement;
        removedElement = current->next;
        current->next = current->next->next;
        if (removedElement->contents) {
            free(removedElement->contents);
        }
        for (unsigned int i = 0; i < removedElement->files; i++) {
            free(removedElement->fullFileName[i]);
        }
        free(removedElement);
    }
}

/**
 * Find the position in the list and add
 *
 * \param listHeader IN start of list
 * \param contents IN  contents to include
 *
 * \return CatalogEntry if found, NULL otherwise
 */

EXPORT CatalogEntry *
InsertInOrder(CatalogEntry *listHeader, const char *contents)
{
    CatalogEntry *currentEntry = listHeader;

    while (currentEntry->next != currentEntry->next->next) {
    	CatalogEntry *nextEntry = currentEntry->next;
        if (XtcStricmp(nextEntry->contents, contents)>0) {
            return InsertIntoCatalogAfter(currentEntry);
        }
        currentEntry = nextEntry;
    }
    return InsertIntoCatalogAfter(currentEntry);
}
/**
 * Get the existing list element for a content
 *
 * \param listHeader IN start of list
 * \param contents IN  contents to search
 * \param Do we log error messages or not
 *
 * \return CatalogEntry if found, NULL otherwise
 */

static CatalogEntry *
IsExistingContents(CatalogEntry *listHeader, const char *contents, BOOL_T silent)
{
    CatalogEntry *currentEntry = listHeader->next;

    while (currentEntry != currentEntry->next) {
        if (!XtcStricmp(currentEntry->contents, contents)) {
        	if (!silent)
        		printf("%s already exists in %s\n", contents, currentEntry->fullFileName[0]);
            return (currentEntry);
        }
        currentEntry = currentEntry->next;
    }
    return (NULL);
}

/**
 * Store information about a parameter file. The filename is added to the array
 * of files with identical content. If not already present, in addition
 * the content is stored
 *
 * \param entry existing entry to be updated
 * \param path filename to add
 * \param contents contents description
 */

EXPORT void
UpdateCatalogEntry(CatalogEntry *entry, char *path, char *contents)
{
    if (!entry->contents) {
        entry->contents = strdup(contents);
    }

    if (entry->files < MAXFILESPERCONTENT) {
        entry->fullFileName[entry->files++] = strdup(path);
    } else {
		AbortProg("Number of file with same content too large!", NULL);
    }
}

/**
 * Create the list for the catalog entries
 *
 * \return
 */

static CatalogEntry *
CreateCatalog()
{
    CatalogEntry *catalog = InitCatalog();

    return (catalog);
}


static IndexEntry *
CreateIndexTable(unsigned int capacity)
{
    IndexEntry *index = (IndexEntry *)malloc(capacity * sizeof(IndexEntry));

    return (index);
}

/**
 * Scan opened directory for the next parameter file
 *
 * \param dir IN opened directory handle
 * \param dirName IN name of directory
 * \param fileName OUT fully qualified filename
 *
 * \return TRUE if file found, FALSE if not
 */

static bool
GetNextParameterFile(DIR *dir, const char *dirName, char **fileName)
{
    bool done = false;
    bool res = false;

    /*
    * get all files from the directory
    */
    while (!done) {
        struct stat fileState;
        struct dirent *ent;

        ent = readdir(dir);

        if (ent) {
            if (!XtcStricmp(FindFileExtension(ent->d_name), "xtp")) {
                /* create full file name and get the state for that file */
                MakeFullpath(fileName, dirName, ent->d_name, NULL);

                if (stat(*fileName, &fileState) == -1) {
                    fprintf(stderr, "Error getting file state for %s\n", *fileName);
                    continue;
                }

                /* ignore any directories */
                if (!(fileState.st_mode & S_IFDIR)) {
                    done = true;
                    res = true;
                }
            }
        } else {
            done = true;
            res = false;
        }
    }
    return (res);
}

/**
 * Scan a directory for parameter files. For each file found the CONTENTS is
 * read and added to the list *
 *
 * \param insertAfter IN starting point for the list of files
 * \param dirName IN directory to be scanned
 *
 * \return pointer to the last element(?)
 */

static CatalogEntry *
ScanDirectory(CatalogEntry *catalog, const char *dirName)
{
    DIR *d;
    CatalogEntry *newEntry = catalog;

    d = opendir(dirName);
    if (d) {
        char *fileName = NULL;

        while (GetNextParameterFile(d, dirName, &fileName)) {
            CatalogEntry *existingEntry;
            char *contents = GetParameterFileContent(fileName);
            if ((existingEntry = IsExistingContents(catalog, contents,FALSE))) {
                printf("Duplicate CONTENTS record in parameter file %s\n", fileName);
                if (strcmp(existingEntry->fullFileName[existingEntry->files-1],fileName))
                	UpdateCatalogEntry(existingEntry, fileName, contents);
            } else {
                newEntry = InsertInOrder(catalog,contents);
                UpdateCatalogEntry(newEntry, fileName, contents);
            }
            free(contents);
            free(fileName);
            fileName = NULL;
        }
        closedir(d);
    }

    return (newEntry);
}

/**
 * Comparison function for IndexEntries used by qsort()
 *
 * \param entry1 IN
 * \param entry2 IN
 * \return per C runtime conventions
 */

static int
CompareIndex(const void *entry1, const void *entry2)
{
	IndexEntry index1 = *(IndexEntry *)entry1;
	IndexEntry index2 = *(IndexEntry *)entry2;
    return (strcoll(index1.keyWord, index2.keyWord));
}

/*!
 * Filter keywords. Current rules:
 *	- single character string that only consist of a punctuation char
 *
 * \param word IN keyword
 * \return true if any rule applies, false otherwise
 */

bool
FilterKeyword(char *word)
{
	if (strlen(word) == 1 && strpbrk(word, PUNCTUATION )) {
		return(true);
	}
	return(false);
}

/**
 * Create the keyword index from a list of parameter files
 *
 * \param catalog IN list of parameter files
 * \param index IN index table to be filled
 * \param pointer IN/OUT array of words that are indexed
 * \param capacityOfIndex IN total maximum of keywords
 * \return number of indexed keywords
 */
static unsigned
CreateContentsIndex(CatalogEntry *catalog, IndexEntry *index, void** words_array,
                    unsigned capacityOfIndex)
{
    CatalogEntry *currentEntry = catalog->next;
    unsigned totalMemory = 0;
    size_t wordCount = 0;
    char *wordList;
    char *wordListPtr;

    while (currentEntry != currentEntry->next) {
        totalMemory += strlen(currentEntry->contents) + 1;
        currentEntry = currentEntry->next;
    }

    wordList = malloc((totalMemory + 1) * sizeof(char));
    *words_array = (void*)wordList;

    wordListPtr = wordList;
    currentEntry = catalog->next;

    while (currentEntry != currentEntry->next) {
        char *word;
        char *content = strdup(currentEntry->contents);

        word = strtok(content, " \t\n\r");
        while (word && wordCount < capacityOfIndex) {
            strcpy(wordListPtr, word);

            char *p = wordListPtr;
            for (; *p; ++p) {
                *p = tolower(*p);
            }
			if (!FilterKeyword(wordListPtr)) {
				index[wordCount].value = currentEntry;
				index[wordCount].keyWord = wordListPtr;
				wordListPtr += strlen(word) + 1;
				wordCount++;
				if (wordCount >= capacityOfIndex) {
					AbortProg("Too many keywords were used!", NULL);
				}
			}
            word = strtok(NULL, " \t\n\r");
        }
        free(content);
        currentEntry = currentEntry->next;
    }
    *wordListPtr = '\0';

    qsort((void*)index, wordCount, sizeof(IndexEntry), CompareIndex);

    return (wordCount);
}

/**
* A recursive binary search function. It returns location of x in
* given array arr[l..r] is present, otherwise -1
* Taken from http://www.geeksforgeeks.org/binary-search/ and modified
*
* \param arr IN array to search
* \param l IN starting index
* \param r IN highest index in array
* \param key IN key to search
* \return index if found, -1 otherwise
*/

static int SearchInIndex(IndexEntry arr[], int l, int r, char *key)
{
    if (r >= l) {
        int mid = l + (r - l) / 2;
        int res = XtcStricmp(key, arr[mid].keyWord);

        // If the element is present at the middle itself
        if (!res) {
            return mid;
        }

        // If the array size is 1
        if (r == 0) {
            return -1;
        }

        // If element is smaller than mid, then it can only be present
        // in left subarray
        if (res < 0) {
            return SearchInIndex(arr, l, mid - 1, key);
        }

        // Else the element can only be present in right subarray
        return SearchInIndex(arr, mid + 1, r, key);
    }

    // We reach here when element is not present in array
    return -1;
}

/**
 * Inserts a key in arr[] of given capacity.  n is current
 * size of arr[]. This function returns n+1 if insertion
 * is successful, else n.
 * Taken from http ://www.geeksforgeeks.org/search-insert-and-delete-in-a-sorted-array/ and modified
 */

int InsertSorted(CatalogEntry *arr[], int n, CatalogEntry *key, int capacity)
{
    // Cannot insert more elements if n is already
    // more than or equal to capcity
    if (n >= capacity) {
        return n;
    }

    int i;
    for (i = n - 1; (i >= 0 && arr[i] > key); i--) {
        arr[i + 1] = arr[i];
    }

    arr[i + 1] = key;

    return (n + 1);
}

/**
 * Comparison function for CatalogEntries used by qsort()
 *
 * \param entry1 IN
 * \param entry2 IN
 * \return per C runtime conventions
 */

static int
CompareResults(const void *entry1, const void *entry2)
{
	CatalogEntry * index1 = *(CatalogEntry **)entry1;
	CatalogEntry * index2 = *(CatalogEntry **)entry2;
    return (strcoll(index1->contents, index2->contents));
}

/**
 * Search the index for a keyword. The index is assumed to be sorted. So after one entry
 * is found, neighboring entries up and down are checked as well. The total result set
 * is placed into an array and returned. This array has to be free'd by the caller.
 *
 * \param index IN	index list
 * \param length IN number of entries index
 * \param search IN search string
 * \param resultCount OUT count of found entries
 * \return array of found catalog entries, NULL if none found
 */

static int findAll = 1;

unsigned int
FindWord(IndexEntry *index, int length, char *search, CatalogEntry ***entries)
{
    CatalogEntry **result;  //Array of pointers to Catalog Entries
    int found;
    int foundElements = 0;
    *entries = NULL;

    //Get all the entries back for generic search or if "generic find"
    if (findAll || !search || (search[0] == '*') || (search[0] == '\0')) {
    	result = malloc((length) * sizeof(CatalogEntry *));
    	for (int i = 0; i < length; i++) {
			result[i] = index[i].value;
		}
    	*entries = result;
    	return length;
    }

    found = SearchInIndex(index, 0, length, search);

    if (found >= 0) {
        int lower = found;
        int upper = found;
        int i;

        while (lower > 0 && !XtcStricmp(index[lower-1].keyWord, search)) {
            lower--;
        }

        while (upper < length - 1 && !XtcStricmp(index[upper + 1].keyWord, search)) {
            upper++;
        }

        foundElements = 1 + upper - lower;

        result = malloc((foundElements) * sizeof(CatalogEntry *));

        for (i = 0; i < foundElements; i++) {
            result[i] = index[i+lower].value;
        }

        qsort((void*)result, foundElements, sizeof(void *), CompareResults);

        *entries = result;
    }
    return (foundElements);
}

/**
 * Create and initialize the data structure for the track library
 *
 * \param trackLibrary OUT the newly allocated track library
 * \return TRUE on success
 */

TrackLibrary *
InitLibrary(void)
{
    TrackLibrary *trackLib = malloc(sizeof(TrackLibrary));

    if (trackLib) {
        trackLib->catalog = CreateCatalog();
        trackLib->index = NULL;
        trackLib->wordCount = 0;
        trackLib->trackTypeCount = 0;
    }

    return (trackLib);
}

/**
 * Scan directory and all parameter files found to the catalog
 *
 * \param trackLib IN the catalog
 * \param directory IN directory to scan
 * \return number of files found
 */

bool
GetTrackFiles(TrackLibrary *trackLib, char *directory)
{
    ScanDirectory(trackLib->catalog, directory);
    trackLib->trackTypeCount = CountCatalogEntries(trackLib->catalog);

    return (trackLib->trackTypeCount);
}
/**
 * Add a list of parameters files to a catalog. This function is
 * called when the user selects files in the file selector.
 *
 * \param files IN count of files
 * \param fileName IN array of filenames
 * \param data IN pointer to the catalog
 * \return alwqys TRUE
 */

int GetParameterFileInfo(
    int files,
    char ** fileName,
    void * data)
{
    CatalogEntry *catalog = (CatalogEntry *)data;

    assert(fileName != NULL);
    assert(files > 0);
    assert(data != NULL);

    for (int i = 0; i < files; i++) {
        CatalogEntry *newEntry;
        char *contents = GetParameterFileContent(fileName[i]);

        if (!(newEntry = IsExistingContents(catalog, contents,TRUE))) {
            newEntry = InsertIntoCatalogAfter(catalog);
        }
        UpdateCatalogEntry(newEntry, fileName[i], contents);
        free(contents);
    }
    return (TRUE);
}

/**
 * Create the search index from the contents description for the whole catalog.
 * A fixed number of words are added to the index. See ESTIMATED_CONTENTS_WORDS
 *
 * \param trackLib IN the catalog
 * \return the number of words indexed
 */

unsigned
CreateLibraryIndex(TrackLibrary *trackLib)
{
    trackLib->index = CreateIndexTable(trackLib->trackTypeCount *
                                       ESTIMATED_CONTENTS_WORDS);

    trackLib->wordCount = CreateContentsIndex(trackLib->catalog, trackLib->index,
    					  &trackLib->words_array,
                          ESTIMATED_CONTENTS_WORDS * trackLib->trackTypeCount);

    return (trackLib->wordCount);
}

void
DeleteLibraryIndex(TrackLibrary *trackLib)
{
	free(trackLib->index);
	trackLib->index = NULL;

	free(trackLib->words_array);

	trackLib->wordCount = 0;

}


/**
 * Create the library and index of parameter files in a given directory.
 *
 * \param directory IN directory to scan
 * \return  NULL if error or empty directory, else library handle
 */

TrackLibrary *
CreateLibrary(char *directory)
{
    TrackLibrary *library;

    library = InitLibrary();
    if (library) {
        if (!GetTrackFiles(library, directory)) {
            return (NULL);
        }

        CreateLibraryIndex(library);
    }
    return (library);
}

void
DeleteLibrary(TrackLibrary* library)
{
	DeleteLibraryIndex(library);


	free(library);
}

// Case insensitive comparison
char* stristr( const char* haystack, const char* needle )
{
	int c = tolower((unsigned char)*needle);
	    if (c == '\0')
	        return (char *)haystack;
	    for (; *haystack; haystack++) {
	        if (tolower((unsigned char)*haystack) == c) {
	            for (size_t i = 0;;) {
	                if (needle[++i] == '\0')
	                    return (char *)haystack;
	                if (tolower((unsigned char)haystack[i]) != tolower((unsigned char)needle[i]))
	                    break;
	            }
	        }
	    }
	    return NULL;
}

/**
 * Search the library for a keyword string and return the result list
 *
 * First the index is searched for the first word and then each "hit" is matched
 * to the entire search string
 *
 * Null, Blank and "*" match all entries
 *
 *  The list is de-duped of repeat of filenames as the same file might appear in
 *  more than once
 *
 * \param library IN the library
 * \param searchExpression	IN keyword to search for
 * \param resultEntries IN list header for result list
 * \return number of found entries
 */
unsigned
SearchLibrary(TrackLibrary *library, char *searchExpression,
              CatalogEntry *resultEntries)
{
    CatalogEntry **entries;
    CatalogEntry * newEntry = resultEntries;
    unsigned entryCount;

    char * word;

    word = strdup(searchExpression);

    //word = strtok(word," \t");

    if (library->index == NULL || library->wordCount == 0) {
        return (0);
    }
    entryCount = FindWord(library->index, library->wordCount, word,
                          &entries);
    int count= 0;
    if (entryCount) {
        unsigned int i = 0;
        while (i < entryCount) {
        	char * match;
        	//Check if entire String Matches
        	if (!searchExpression || !word || (word[0] == '*') || (word[0] == '\0') ||
        		(match = stristr(entries[i]->contents,searchExpression))) {
				CatalogEntry * existingEntry;
				existingEntry = IsExistingContents(resultEntries, entries[i]->contents, TRUE);
				//Same FileName already in one of the entries?
				BOOL_T found = FALSE;
				if (existingEntry) {
					for (unsigned int j=0;j<existingEntry->files;j++) {
						if (!strcmp(existingEntry->fullFileName[j],entries[i]->fullFileName[entries[i]->files-1])) {
							found=TRUE;
							break;
						}
					}
					if (found == TRUE ) {
						i++;
						continue;
					}
					UpdateCatalogEntry(existingEntry, entries[i]->fullFileName[(entries[i]->files- 1)],
												   entries[i]->contents);
				} else {
					newEntry = InsertInOrder(resultEntries,entries[i]->contents);
					UpdateCatalogEntry(newEntry, entries[i]->fullFileName[(entries[i]->files- 1)],
						   entries[i]->contents);
				}
				count++;
        	}
            i++;
        }
    }
    free(word);
    if (entries)
    	free(entries);    //Clean-up after search
    return (count);
}

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

	fh = fopen(file, "rt");
	if (fh) {
		bool found = false;

		while (!found) {
			char buffer[512];
			if (fgets(buffer, sizeof(buffer), fh)) {
				char *ptr = strtok(buffer, " \t");
				if (!XtcStricmp(ptr, CONTENTSCOMMAND)) {
					/* if found, store the rest of the line and the filename	*/
					ptr = ptr+strlen(CONTENTSCOMMAND)+1;
					ptr = strtok(ptr, "\r\n");
					result = strdup(ptr);
#ifdef WINDOWS
					ConvertUTF8ToSystem(result);
#endif // WINDOWS
					found = true;
				}
			} else {
				fprintf(stderr, "Nothing found in %s\n", file);
				found = true;
			}
		}
		fclose(fh);
	}
	return(result);
}
