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


/**
 * Data structures:
 * 
 * 
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
#include "utility.h"

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
#include "include/utlist.h"

#if _MSC_VER > 1300
    #define strnicmp _strnicmp
    #define strdup _strdup
	#define strlwr _strlwr
#endif

#define PUNCTUATION "+-*/.,&%=#"

static char *stopwords = {
	"scale",
};

/**
 * Create and initialize the linked list for the catalog entries
 *
 * \return pointer to first element
 */

Catalog *
InitCatalog(void)
{
	Catalog *newCatalog = MyMalloc(sizeof(Catalog));
	if (newCatalog) {
		newCatalog->head = NULL;
	}
    return (newCatalog);
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
 * Count the elements in the linked list
 *
 * \param catalog IN 
 * \return the number of elements
 */

unsigned
CountCatalogEntries(Catalog *catalog)
{
	CatalogEntry * entry;
    unsigned count = 0;
	DL_COUNT(catalog->head, entry, count);
    return (count);
}

/**
 * Empty a catalog. All data nodes including their allocated memory are freed. On
 *
 * \param listHeader IN the list
 */

void
EmptyCatalog(Catalog *catalog)
{
    CatalogEntry *current = catalog->head;
	CatalogEntry *element;
	CatalogEntry *tmp;

	DL_FOREACH_SAFE(current, element, tmp)
	{
		DL_DELETE(current, element);
		MyFree(element->contents);
		for (unsigned int i = 0; i < element->files; i++) {
			MyFree(element->fullFileName[i]);
		}
		free(element);
	}
}

/**
 * Compare entries
 *
 * \param [in] a If non-null, a CatalogEntry to compare.
 * \param [in] b If non-null, a CatalogEntry to compare.
 *
 * \returns An int.
 */

static int
CompareEntries( CatalogEntry *a, CatalogEntry *b)
{
	return XtcStricmp(a->contents, b->contents);
}

/**
 * Find the position in the list and add
 *
 * \param listHeader IN start of list
 * \param contents IN  contents to include
 *
 * \return CatalogEntry if found, NULL otherwise
 */

static CatalogEntry *
InsertInOrder(Catalog *catalog, const char *contents)
{
	CatalogEntry *newEntry = MyMalloc(sizeof(CatalogEntry));
	newEntry->contents = MyStrdup(contents);

	DL_INSERT_INORDER(catalog->head, newEntry, CompareEntries);

    return newEntry;
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
IsExistingContents(Catalog *catalog, const char *contents, BOOL_T silent)
{
    CatalogEntry *head = catalog->head;
	CatalogEntry *currentEntry;

	DL_FOREACH(head, currentEntry)
	{
		if (!XtcStricmp(currentEntry->contents, contents)) {
			if (!silent)
				printf("%s already exists in %s\n", contents, currentEntry->fullFileName[0]);
			return (currentEntry);
		}
	};
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

static void
UpdateCatalogEntry(CatalogEntry *entry, char *path, char *contents)
{
    if (!entry->contents) {
        entry->contents = MyStrdup(contents);
    }

    if (entry->files < MAXFILESPERCONTENT) {
        entry->fullFileName[entry->files++] = MyStrdup(path);
    } else {
		AbortProg("Number of files with same content too large!", NULL);
    }
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

	for (int i = 0; i < sizeof(stopwords) / sizeof(char *); i++) {
		if (!strcmp(word, stopwords+i)) {
			return(true);
		}
	}
	return(false);
}

int KeyWordCmp(IndexEntry *a, IndexEntry *b)
{
	return strcmp(a->keyWord, b->keyWord);
}

/**
 * Standardize spelling: remove some typical spelling problems. It is assumed that the word 
 * has already been converted to lower case
 *
 * \param [in,out] word If non-null, the word.
 */

void
StandardizeSpelling(char *word)
{
	char *p = strchr(word, '-');
	// remove the word 'scale' from combinations like N-scale 
	if (p) {
		if (!strcmp(p+1, "scale")) {
			*p = '\0';
		}
	}

	if (!strncmp(word, "h0", 2))
		strncpy(word, "hO", 2);

	if (!strncmp(word, "00", 2))
		strncpy(word, "oo", 2);

	if (word[0] == '0')
		word[0] = 'o';
}

/**
 * Create the keyword index from a list of parameter files
 *
 * \param [in] catalog  list of parameter files.
 * \param [in,out] indexPtr index table to be filled.
 *
 * \returns number of indexed keywords.
 */

static unsigned
CreateContentsIndex(Catalog *catalog, IndexEntry **indexPtr )
{
    CatalogEntry *listOfEntries = catalog->head;
	CatalogEntry *curParamFile;
    unsigned totalMemory = 0;
    size_t wordCount = 0;
    char *wordList;
    char *wordListPtr;
	IndexEntry *index = *indexPtr;

	// allocate a  buffer for the complete set of keywords
	DL_FOREACH(listOfEntries, curParamFile ) {
        totalMemory += strlen(curParamFile->contents) + 1;
    }
	wordList = malloc((totalMemory + 1) * sizeof(char));

    wordListPtr = wordList;
    listOfEntries = catalog->head;

    DL_FOREACH(listOfEntries, curParamFile) {
        char *word;
        char *content = strdup(curParamFile->contents);

        word = strtok(content, " \t\n\r");
        while (word) {
            strcpy(wordListPtr, word);

			strlwr(wordListPtr);
			if (!FilterKeyword(wordListPtr)) {
				IndexEntry *searchEntry = malloc(sizeof( IndexEntry ));
				IndexEntry *existingEntry = NULL;
				searchEntry->keyWord = wordListPtr;

				if (index) {
					DL_SEARCH(index, existingEntry, searchEntry, KeyWordCmp);
				}
				if (existingEntry) {
					DYNARR_APPEND(CatalogEntry *, *(existingEntry->references), 5);
					DYNARR_LAST(CatalogEntry *, *(existingEntry->references)) = curParamFile;
				} else {
					searchEntry->references = calloc(1, sizeof(dynArr_t));
					DYNARR_APPEND(CatalogEntry *, *(searchEntry->references), 5);
					DYNARR_LAST(CatalogEntry *, *(searchEntry->references)) = curParamFile;
					DL_APPEND(index, searchEntry);
				}

				wordListPtr += strlen(word) + 1;
				wordCount++;
			}
            word = strtok(NULL, " \t\n\r");
        }
        free(content);
    }
    *wordListPtr = '\0';
   
	DL_SORT(index, KeyWordCmp);
	
	* indexPtr = index;
    return (wordCount);
}

/**
 * Search the index for a keyword. The index is assumed to be sorted.
 * Each keyword has one entry in the index list. 
 *
 * \param [in]	index	index list.
 * \param 		length  number of entries index.
 * \param [in]	search  search string.
 * \param [out] entries	array of found entry.
 *
 * \returns TRUE if found, FALSE otherwise
 */

unsigned int
FindWord(IndexEntry *index, int length, char *search, IndexEntry **entries)
{
    int foundElements = 0;
	IndexEntry *result = NULL;

	IndexEntry *searchWord = malloc(sizeof(IndexEntry));
	searchWord->keyWord = search;

    *entries = NULL;

	DL_SEARCH(index, result, searchWord, KeyWordCmp);
	*entries = result;
	
    return (result != NULL);
}

/**
 * Create and initialize the data structure for the track library
 *
 * \param trackLibrary OUT the newly allocated track library
 * \return TRUE on success
 */

ParameterLib *
InitLibrary(void)
{
    ParameterLib *trackLib = malloc(sizeof(ParameterLib));

    if (trackLib) {
        trackLib->catalog = InitCatalog();
        trackLib->index = NULL;
        trackLib->wordCount = 0;
        trackLib->parameterFileCount = 0;
    }

    return (trackLib);
}

/**
 * Scan directory and add all parameter files found to the catalog
 *
 * \param trackLib IN the catalog
 * \param directory IN directory to scan
 * \return number of files found
 */

bool
GetParameterFiles(ParameterLib *paramLib, char *directory)
{
	DIR *d;
	Catalog *catalog = paramLib->catalog;

	d = opendir(directory);
	if (d) {
		char *fileName = NULL;

		while (GetNextParameterFile(d, directory, &fileName)) {
			CatalogEntry *existingEntry;

			char *contents = GetParameterFileContent(fileName);

			if ((existingEntry = IsExistingContents(catalog, contents, FALSE))) {
				// [TODO] Find a reasonable way to handle duplicate contents descriptions
				// if (strcmp(existingEntry->fullFileName[existingEntry->files - 1], fileName))
				UpdateCatalogEntry(existingEntry, fileName, contents);
			} else {
				CatalogEntry *newEntry;
				newEntry = InsertInOrder(catalog, contents);
				UpdateCatalogEntry(newEntry, fileName, contents);
			}
			MyFree(contents);
			free(fileName);
			fileName = NULL;
		}
		closedir(d);
	}
    paramLib->parameterFileCount = CountCatalogEntries(paramLib->catalog);

    return (paramLib->parameterFileCount);
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
    Catalog *catalog = (Catalog *)data;

    assert(fileName != NULL);
    assert(files > 0);
    assert(data != NULL);

    for (int i = 0; i < files; i++) {
        CatalogEntry *newEntry;
        char *contents = GetParameterFileContent(fileName[i]);
		///[TODO] try to find out what should happen here
        if (!(newEntry = IsExistingContents(catalog, contents,TRUE))) {
            newEntry = InsertIntoCatalogAfter(catalog->head);
        }
        UpdateCatalogEntry(newEntry, fileName[i], contents);
        free(contents);
    }
    return (TRUE);
}

/**
 * Create the search index from the contents description for the whole
 * catalog. A fixed number of words are added to the index. See
 * ESTIMATED_CONTENTS_WORDS
 *
 * \param [in] parameterLib IN the catalog.
 *
 * \returns the number of words indexed.
 */

unsigned
CreateLibraryIndex(ParameterLib *parameterLib)
{
	parameterLib->index = NULL;

    parameterLib->wordCount = CreateContentsIndex(parameterLib->catalog,
                              &(parameterLib->index ));

    return (parameterLib->wordCount);
}

void
DeleteLibraryIndex(ParameterLib *trackLib)
{
	free(trackLib->index);
	trackLib->index = NULL;

	trackLib->wordCount = 0;

}


/**
 * Create the library and index of parameter files in a given directory.
 *
 * \param directory IN directory to scan
 * \return  NULL if error or empty directory, else library handle
 */

ParameterLib *
CreateLibrary(char *directory)
{
    ParameterLib *library;

    library = InitLibrary();
    if (library) {
        if (!GetParameterFiles(library, directory)) {
            return (NULL);
        }

        CreateLibraryIndex(library);
    }
    return (library);
}

void
DeleteLibrary(ParameterLib* library)
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
 * Intersection of results
 * See GeeksForGeeks https ://www.geeksforgeeks.org/union-and-intersection-of-two-sorted-arrays-2/
 * 1) Use two index variables i and j, initial values i = 0, j = 0
 * 2) If arr1[i] is smaller than arr2[j] then increment i.
 * 3) If arr1[i] is greater than arr2[j] then increment j.
 * 4) If both are same then print any of them and increment both i and j.
 *  
 * \param [in] array1 If non-null, the first result.
 * \param [in] array2 If non-null, the second result.
 *
 * \returns number of elements in result set
 */
 
unsigned
IntersectionOfResults(CatalogEntry *** resultEntries, CatalogEntry **array1, unsigned count1, CatalogEntry **array2, unsigned count2)
{
	unsigned index1 = 0;
	unsigned index2 = 0;
	unsigned maxResultSize = min(count1, count2);
	unsigned matches = 0;
	CatalogEntry **result = MyMalloc(maxResultSize * sizeof(CatalogEntry *));
	int diff;

	while (index1 < count1 && index2 < count2) {
		diff = strcmp(array1[index1]->contents, array2[index2]->contents);

		if (diff < 0) {
			index1++;
			continue;
		}
		if (diff > 0) {
			index2++;
			continue;
		}

		printf("Match found: %s\n", array1[index1]->contents);
		result[matches++] = array1[index1];
		index1++;
		index2++;
	}
	*resultEntries = result;
	return(matches);
}

// returns number of words in str 
unsigned countWords(char *str)
{
	int state = FALSE;
	unsigned wc = 0;  // word count 

	// Scan all characters one by one 
	while (*str) {
		// If next character is a separator, set the  
		// state as FALSE 
		if (*str == ' ' || *str == '\n' || *str == '\t')
			state = FALSE;

		// If next character is not a word separator and  
		// state is OUT, then set the state as IN and  
		// increment word count 
		else if (state == FALSE) {
			state = TRUE;
			++wc;
		}

		// Move to next character 
		++str;
	}

	return wc;
}


/**
 * Search the library for a keyword string and return the result list
 *
 * Each key word exists only once in the index. 
 * 
 * \param library IN the library
 * \param searchExpression	IN keyword to search for
 * \param resultEntries IN list header for result list
 * \return number of found entries
 */
 
unsigned
SearchLibrary(ParameterLib *library, char *searchExpression,
	SearchResult *results)
{
	CatalogEntry *element;
	IndexEntry *entries;			
	unsigned entryCount = 0;
	char *searchWord;
	unsigned words = countWords(searchExpression);
	char *searchExp = MyStrdup(searchExpression);
	unsigned i = 0;

	if (library->index == NULL || library->wordCount == 0) {
		return (0);
	}

	results->kw = MyMalloc(words * sizeof(struct sSingleResult));
	results->subCatalog.head = NULL;

	searchWord = strtok(searchExp, " \t");
	while (searchWord) {
		strlwr(searchWord);
		if (!FilterKeyword(searchWord)) {
			StandardizeSpelling(searchWord);
			results->kw[i].state = SEARCHED;			
		} else {
			results->kw[i].state = DISCARDED;
		}
		results->kw[i++].keyWord = searchWord;
		searchWord = strtok(NULL, " \t");
	}

	i = 0;
	while (i < words) {
		FindWord(library->index, library->wordCount, results->kw[i].keyWord, &entries);
		if(entries) {
			results->kw[i].count = entries->references->cnt;

			if (results->subCatalog.head == NULL) {
				// if first keyword -> initialize result set
				for (int j = 0; j < entries->references->cnt; j++) {
					CatalogEntry *newEntry = MyMalloc(sizeof(CatalogEntry));
					CatalogEntry *foundEntry = DYNARR_N(CatalogEntry *, *(entries->references), j);
					newEntry->contents = foundEntry->contents;
					newEntry->files = foundEntry->files;
					memcpy(newEntry->fullFileName, foundEntry->fullFileName, sizeof(char *) * MAXFILESPERCONTENT);

					DL_APPEND(results->subCatalog.head, newEntry);
				}
			} else {
				// follow up keyword, create intersection with current result set
				CatalogEntry *current;
				CatalogEntry *temp;

				DL_FOREACH_SAFE(results->subCatalog.head, current, temp)
				{
					int found = 0;
					for (int j = 0; j < entries->references->cnt; j++) {
						CatalogEntry *foundEntry = DYNARR_N(CatalogEntry *, *(entries->references), j);

						if (foundEntry->contents == current->contents) {
							found = TRUE;
							break;
						}
					}
					if (!found) {
						DL_DELETE(results->subCatalog.head, current);
					}
				}
			}
		} else {
			// Searches that don't yield a result are ignored
			results->kw[i].count = 0;
		}
		i++;
	}
	
	DL_COUNT(results->subCatalog.head, element, results->totalFound );
	return (results->totalFound);
}

/**
 * Discard results. The memory allocated with the search is freed
 *
 * \param [in] res If non-null, the results.
 */

void
SearchResultDiscard(SearchResult *res)
{
	if (res) {
		CatalogEntry *element;
		MyFree(res->kw);
		DL_DELETE(res->subCatalog.head, element);
	}
}

/**
 * Get the contents description from a parameter file. Returned string has to be MyFree'd after use.
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
					result = MyStrdup(ptr);
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
