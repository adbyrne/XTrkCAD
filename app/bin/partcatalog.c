/** \file partcatalog.c
* Manage the catalog of track parameter files
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
#include "dynstring.h"
#include "fileio.h"
#include "i18n.h"
#include "include/levenshtein.h"
#include "misc.h"
#include "misc2.h"
#include "include/paramfile.h"
#include "include/partcatalog.h"
#include "paths.h"
#include "include/stringxtc.h"
#include "include/utf8convert.h"
#include "include/utlist.h"
#include "utility.h"

#if _MSC_VER > 1300
    #define strnicmp _strnicmp
    #define stricmp _stricmp
    #define strdup _strdup
#endif

#define PUNCTUATION "+-*/.,&%=#"
#define SEARCHDELIMITER " \t\n\r/"
#define LDISTANCELIMIT (2)

static char *stopwords = {
    "scale",
};

static int log_params;

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
 * Destroys the catalog
 *
 * \param [in] catalog
 */

void
DestroyCatalog(Catalog *catalog)
{
    MyFree(catalog);
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
    CatalogEntry *newEntry = (CatalogEntry *)MyMalloc(sizeof(CatalogEntry));
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

EXPORT void
CatalogDiscard(Catalog *catalog)
{
    CatalogEntry *current = catalog->head;
    CatalogEntry *element;
    CatalogEntry *tmp;

    DL_FOREACH_SAFE(current, element, tmp) {
        DL_DELETE(current, element);
        MyFree(element);
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
CompareEntries(CatalogEntry *a, CatalogEntry *b)
{
    return XtcStricmp(a->contents, b->contents);
}

/**
 * Create a new CatalogEntry and insert it keeping the list sorted
 *
 * \param [in] catalog
 * \param [in] contents to include.
 *
 * \returns CatalogEntry
 */

EXPORT CatalogEntry *
InsertInOrder(Catalog *catalog, const char *contents)
{
    CatalogEntry *newEntry = MyMalloc(sizeof(CatalogEntry));
    newEntry->files = 0;

    newEntry->contents = MyStrdup(contents);

    DL_INSERT_INORDER(catalog->head, newEntry, CompareEntries);

    return newEntry;
}

/**
 * Find an existing list element for a given content
 *
 * \param [in] catalog
 * \param [in] contents contents to search.
 * \param [in] silent   we log error messages or not.
 *
 * \returns CatalogEntry if found, NULL otherwise.
 */

static CatalogEntry *
IsExistingContents(Catalog *catalog, const char *contents, BOOL_T silent)
{
    CatalogEntry *head = catalog->head;
    CatalogEntry *currentEntry;

    DL_FOREACH(head, currentEntry) {
        if (!XtcStricmp(currentEntry->contents, contents)) {
            if (!silent) {
                printf("%s already exists in %s\n", contents, currentEntry->fullFileName[0]);
            }
            return (currentEntry);
        }
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
 * \param fileName OUT fully qualified filename, must be free()'d by caller
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
    if (strlen(word) == 1 && strpbrk(word, PUNCTUATION)) {
        return (true);
    }

    for (int i = 0; i < sizeof(stopwords) / sizeof(char *); i++) {
        if (!XtcStricmp(word, stopwords+i)) {
            return (true);
        }
    }
    return (false);
}

int KeyWordCmp(IndexEntry *a, IndexEntry *b)
{
    return XtcStricmp(a->keyWord, b->keyWord);
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
        if (!XtcStricmp(p+1, "scale")) {
            *p = '\0';
        }
    }

    if (!strncasecmp(word, "h0", 2)) {
        strncpy(word, "ho", 2);
    }

    if (!strncasecmp(word, "00", 2)) {
        strncpy(word, "oo", 2);
    }

    if (word[0] == '0') {
        word[0] = 'o';
    }
}

/**
 * Create the keyword index from a list of parameter files
 *
 * \param [in] library initialized library
 *
 * \returns number of indexed keywords.
 */

static unsigned
CreateKeywordIndex(ParameterLib *library)
{
    CatalogEntry *listOfEntries = library->catalog->head;
    CatalogEntry *curParamFile;
    unsigned totalMemory = 0;
    size_t wordCount = 0;
    char *wordList;
    char *wordListPtr;
    IndexEntry *index = library->index;

    // allocate a  buffer for the complete set of keywords
    DL_FOREACH(listOfEntries, curParamFile) {
        totalMemory += strlen(curParamFile->contents) + 1;
    }
    wordList = MyMalloc((totalMemory + 1) * sizeof(char));

    wordListPtr = wordList;

    DL_FOREACH(listOfEntries, curParamFile) {
        char *word;
        char *content = strdup(curParamFile->contents);

        word = strtok(content, SEARCHDELIMITER);
        while (word) {
            strcpy(wordListPtr, word);

            XtcStrlwr(wordListPtr);
            if (!FilterKeyword(wordListPtr)) {
                IndexEntry *searchEntry = MyMalloc(sizeof(IndexEntry));
                IndexEntry *existingEntry = NULL;
                searchEntry->keyWord = wordListPtr;
                StandardizeSpelling(wordListPtr);

                if (index) {
                    DL_SEARCH(index, existingEntry, searchEntry, KeyWordCmp);
                }
                if (existingEntry) {
                    DYNARR_APPEND(CatalogEntry *, *(existingEntry->references), 5);
                    DYNARR_LAST(CatalogEntry *, *(existingEntry->references)) = curParamFile;
                    MyFree(searchEntry);
                } else {
                    searchEntry->references = calloc(1, sizeof(dynArr_t));
                    DYNARR_APPEND(CatalogEntry *, *(searchEntry->references), 5);
                    DYNARR_LAST(CatalogEntry *, *(searchEntry->references)) = curParamFile;
                    DL_APPEND(index, searchEntry);
                    LOG1(log_params, ("Index Entry: <%s>\n", searchEntry->keyWord))
                }

                wordListPtr += strlen(word) + 1;
                wordCount++;
            }
            word = strtok(NULL, SEARCHDELIMITER);
        }
        free(content);
    }
    *wordListPtr = '\0';

    DL_SORT(index, KeyWordCmp);

    library->index = index;
    library->words = wordList;

    IndexEntry *existingEntry;
    DL_FOREACH(index, existingEntry) {
        LOG1(log_params, ("Index Entry: <%s> Count: %d\n", existingEntry->keyWord,
                          existingEntry->references->cnt));
    }
    return (wordCount);
}

/**
 * Search the index for a keyword. The index is assumed to be sorted. Each
 * keyword has one entry in the index list.
 *
 * \param [in]  index   index list.
 * \param 	    length  number of entries index.
 * \param [in]  search  search string.
 * \param [out] entries array of found entry.
 *
 * \returns TRUE if found, FALSE otherwise.
 */

unsigned int
FindWord(IndexEntry *index, int length, char *search, IndexEntry **entries)
{
    IndexEntry *result = NULL;

    IndexEntry searchWord;
    searchWord.keyWord = search;

    *entries = NULL;

    DL_SEARCH(index, result, &searchWord, KeyWordCmp);
	if (!result) {
		int maxdistance = 1;
		while (maxdistance <= LDISTANCELIMIT && !result ) {
			IndexEntry *current;
			size_t minDistance = LDISTANCELIMIT + 1;
			int maxProbability = 0;
			LOG1(log_params, ("Close match for: <%s> maxdistance: %d\n", search, maxdistance));
				
			DL_FOREACH(index, current)
			{
				size_t ldist = levenshtein(search, current->keyWord);
				LOG1(log_params, ("Distance of: <%s> is %d\n", current->keyWord, ldist));
				if (ldist == maxdistance) {
					if (current->references->cnt > maxProbability) {
						if (!result) {
							result = MyMalloc(sizeof(IndexEntry));
						}
						memcpy(result, current, sizeof(IndexEntry));
						maxProbability = current->references->cnt;
					}
				}
			}

			maxdistance++;
		}
	}
    
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
    ParameterLib *trackLib = MyMalloc(sizeof(ParameterLib));

    if (trackLib) {
        trackLib->catalog = InitCatalog();
        trackLib->index = NULL;
        trackLib->wordCount = 0;
        trackLib->parameterFileCount = 0;
    }

    return (trackLib);
}

/**
 * Destroys the library freeing all associated memory
 *
 * \param [in] library If non-null, the library.
 */

void
DestroyLibrary(ParameterLib *library)
{
    if (library) {
        DestroyCatalog(library->catalog);
        MyFree(library);
    }
}

/**
 * Scan directory and add all parameter files found to the catalog
 *
 * \param trackLib IN the catalog
 * \param directory IN directory to scan
 * \return number of files found
 */

bool
CreateCatalogFromDir(ParameterLib *paramLib, char *directory)
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
 * Discard the complete catalog from a library
 *
 * \param [in] library
 */

void
DiscardCatalog(ParameterLib *library)
{
    CatalogEntry *entry;
    CatalogEntry *temp;

    DL_FOREACH_SAFE(library->catalog->head, entry, temp) {
        MyFree(entry->contents);
        for (unsigned int i = 0; i < entry->files; i++) {
            MyFree(entry->fullFileName[i]);
        }
        DL_DELETE(library->catalog->head, entry);
        MyFree(entry);
    }

}


/**
 * Create the search index from the contents description for the whole
 * catalog.
 *
 * \param [in] parameterLib IN the catalog.
 *
 * \returns the number of words indexed.
 */

unsigned
CreateLibraryIndex(ParameterLib *parameterLib)
{
    parameterLib->index = NULL;

    parameterLib->wordCount = CreateKeywordIndex(parameterLib);

    return (parameterLib->wordCount);
}

/**
 * Discard library index freeing all memory used
 * references were created using MakeFullPath. These were allocated using malloc and
 * not MyMalloc
 *
 * \param [in] trackLib the track library.
 */

void
DiscardLibraryIndex(ParameterLib *trackLib)
{
    IndexEntry *indexEntry;
    IndexEntry *tmp;

    DL_FOREACH_SAFE(trackLib->index, indexEntry, tmp) {
        DYNARR_FREE(CatalogEntry *, *(indexEntry->references));
        free(indexEntry->references);
        DL_DELETE(trackLib->index, indexEntry);
        MyFree(indexEntry);
    }

    MyFree(trackLib->words);
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

    log_params = LogFindIndex("params");

    library = InitLibrary();
    if (library) {
        if (!CreateCatalogFromDir(library, directory)) {
            return (NULL);
        }

        CreateLibraryIndex(library);
    }
    return (library);
}

/**
 * Discard library freeing all memory used
 *
 * \param [in,out] library If non-null, the library.
 */

void
DiscardLibrary(ParameterLib* library)
{
    CatalogEntry *entry = library->catalog->head;
    CatalogEntry *element;
    CatalogEntry *tmp;

    DiscardLibraryIndex(library);

    DL_FOREACH_SAFE(entry, element, tmp) {
        MyFree(element->contents);
        for (unsigned int i = 0; i < element->files; i++) {
            MyFree(element->fullFileName[i]);
        }
        DL_DELETE(entry, element);
        MyFree(element);
    }
    MyFree(library->words);
    MyFree(library);
}

/**
 * Create a statistic for a finished search. The returned string has to be MyFreed() after usage
 *
 * \param [in] result the finished search
 *
 * \returns Null if it fails, else the found statistics.
 */

char *
SearchStatistics(SearchResult *result)
{
    DynString buffer;
    DynString subStats[STATE_COUNT];

    unsigned searched = 0;
    unsigned discarded = 0;
    unsigned notfound = 0;
	unsigned close = 0;

    char *resStat;
    DynStringMalloc(&buffer, 16);

    for (int i = SEARCHED; i < STATE_COUNT; i++) {
        DynStringMalloc(subStats + i, 16);
    }

    DynStringCatCStr(subStats + SEARCHED, _("Found: "));
	DynStringCatCStr(subStats + CLOSE, _("Similar: "));
    DynStringCatCStr(subStats + DISCARDED, _("Ignored: "));
    DynStringCatCStr(subStats + NOTFOUND, _("Not found: "));

    for (unsigned int i = 0; i < result->words; i++) {
        switch (result->kw[i].state) {
        case SEARCHED:
            DynStringPrintf(&buffer, "%s (%d) ", result->kw[i].keyWord,
                            result->kw[i].count);
            searched++;
            break;
        case DISCARDED:
            DynStringPrintf(&buffer, "%s ", result->kw[i].keyWord);
            discarded++;
            break;
        case NOTFOUND:
            DynStringPrintf(&buffer, "%s ", result->kw[i].keyWord);
            notfound++;
            break;
		case CLOSE:
			DynStringPrintf(&buffer, "%s ", result->kw[i].keyWord);
			close++;
			break;
        default:
            break;
        }
        DynStringCatStr(subStats + result->kw[i].state, &buffer);
    }

    DynStringReset(&buffer);
    if (searched) {
        DynStringCatStr(&buffer, subStats + SEARCHED);
    }
	if (close) {
		DynStringCatStr(&buffer, subStats + CLOSE);
	}
    if (notfound) {
        DynStringCatStr(&buffer, subStats + NOTFOUND);
    }
    if (discarded) {
        DynStringCatStr(&buffer, subStats + DISCARDED);
    }

    resStat = MyStrdup(DynStringToCStr(&buffer));
    DynStringFree(&buffer);
	for (int i = SEARCHED; i < STATE_COUNT; i++) {
		DynStringFree(subStats + i);
	}
    return (resStat);
}

/**
 * returns number of words in str.
 *
 * \param [in] str the string.
 *
 * \returns The total number of words.
 */

unsigned countWords(char *str)
{
    int state = FALSE;
    unsigned wc = 0;  // word count

    // Scan all characters one by one
    while (*str) {
        // If next character is a separator, set the
        // state as FALSE
        if (*str == ' ' || *str == '\n' || *str == '\t' || *str == '\r' || *str == '/') {
            state = FALSE;
        }

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

    searchWord = strtok(searchExp, SEARCHDELIMITER);
    while (searchWord) {
        XtcStrlwr(searchWord);
        if (!FilterKeyword(searchWord)) {
            StandardizeSpelling(searchWord);
            results->kw[i].state = SEARCHED;
        } else {
            results->kw[i].state = DISCARDED;
        }
        results->kw[i++].keyWord = MyStrdup(searchWord);
        searchWord = strtok(NULL, SEARCHDELIMITER);
    }
    results->words = words;

    i = 0;
    while (i < words) {
        if (results->kw[i].state == DISCARDED) {
            i++;
            continue;
        }
        FindWord(library->index, library->wordCount, results->kw[i].keyWord, &entries);
        if (entries) {
            results->kw[i].count = entries->references->cnt;
			if (XtcStricmp(results->kw[i].keyWord, entries->keyWord)) {
				results->kw[i].state = CLOSE;
				MyFree(results->kw[i].keyWord);
				results->kw[i].keyWord = MyStrdup(entries->keyWord);
			}

            if (results->subCatalog.head == NULL) {
                // if first keyword -> initialize result set
                for (int j = 0; j < entries->references->cnt; j++) {
                    CatalogEntry *newEntry = MyMalloc(sizeof(CatalogEntry));
                    CatalogEntry *foundEntry = DYNARR_N(CatalogEntry *, *(entries->references), j);
                    newEntry->contents = foundEntry->contents;
                    newEntry->files = foundEntry->files;
                    memcpy(newEntry->fullFileName, foundEntry->fullFileName,
                           sizeof(char *) * MAXFILESPERCONTENT);

                    DL_APPEND(results->subCatalog.head, newEntry);
                }
            } else {
                // follow up keyword, create intersection with current result set
                CatalogEntry *current;
                CatalogEntry *temp;

                DL_FOREACH_SAFE(results->subCatalog.head, current, temp) {
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
            results->kw[i].state = NOTFOUND;
            results->kw[i].count = 0;
        }
        i++;
    }

    DL_COUNT(results->subCatalog.head, element, results->totalFound);
    MyFree(searchExp);
    return (results->totalFound);
}

/**
 * Discard results. The memory allocated with the search is freed
 *
 * \param [in] res If non-null, the results.
 */

void
SearchDiscardResult(SearchResult *res)
{
    if (res) {
        CatalogEntry *current = res->subCatalog.head;
        CatalogEntry *element;
        CatalogEntry *tmp;

        DL_FOREACH_SAFE(current, element, tmp) {
            DL_DELETE(current, element);
            MyFree(element);
        }

        for (unsigned int i = 0; i < res->words; i++) {
            MyFree(res->kw[i].keyWord);
        }
        MyFree(res->kw);
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
    return (result);
}

#ifdef MEMWATCH
/** this is used to test for memory leaks. It should show no leaks from functions in this source file */
RunMemoryTest(char *directory)
{
    ParameterLib *library;
    SearchResult *results;

    mwInit();
    library = InitLibrary();
    if (library) {
        CreateCatalogFromDir(library, directory);
        CreateLibraryIndex(library);
        results = MyMalloc(sizeof(SearchResult));
        SearchLibrary(library, "peco", results);
        SearchDiscardResult(results);
        MyFree(results);
        DiscardLibraryIndex(library);
        DiscardCatalog(library);
    }
    DestroyLibrary(library);
    mwTerm();
}
#endif //MEMWATCH
