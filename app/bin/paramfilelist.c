/** \file paramfilelist.c
 * Handling of the list of parameter files
 */

/*  XTrackCad - Model Railroad CAD
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
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compound.h"
#include "ctrain.h"
#include "custom.h"
#include "dynstring.h"
#include "fileio.h"
#include "i18n.h"
#include "layout.h"
#include "messages.h"
#include "misc2.h"
#include "paths.h"
#include "include/paramfile.h"
#include "include/paramfilelist.h"


dynArr_t paramFileInfo_da;

int curParamFileIndex = PARAM_DEMO;

static int log_params;

static char * customPath;
static char * customPathBak;

#define FAVORITESECTION "Parameter File Favorites"
#define FAVORITETOTALS "Total"
#define FAVORITEKEY "Favorite%d"
#define	FAVORITEDELETED "Deleted%d"

int GetParamFileCount()
{
    return (paramFileInfo_da.cnt);
}




/**
 * Update the configuration file in case the name of a parameter file has changed.
 * The function reads a list of new parameter filenames and gets the contents
 * description for each file. If that contents is in use, i.e. is a loaded parameter
 * file, the setting in the config file is updated to the new filename.
 * First line of update file has the date of the update file.
 * * Following lines have filenames, one per line.
 *
 * \return FALSE if update not possible or not necessary, TRUE if update successful
 */

static BOOL_T UpdateParamFiles(void)
{
    char fileName[STR_LONG_SIZE], *fileNameP;
    const char * cp;
    FILE * updateF;
    long updateTime;
    long lastTime;

    MakeFullpath(&fileNameP, libDir, "xtrkcad.upd", NULL);
    updateF = fopen(fileNameP, "r");
    free(fileNameP);
    if (updateF == NULL) {
        return FALSE;
    }
    if (fgets(message, sizeof message, updateF) == NULL) {
        NoticeMessage("short file: xtrkcad.upd", _("Ok"), NULL);
        fclose(updateF);
        return FALSE;
    }
    wPrefGetInteger("file", "updatetime", &lastTime, 0);
    updateTime = atol(message);
    if (lastTime >= updateTime) {
        fclose(updateF);
        return FALSE;
    }

    while ((fgets(fileName, STR_LONG_SIZE, updateF)) != NULL) {
        FILE * paramF;
        char * contents;

        Stripcr(fileName);
        InfoMessage(_("Updating %s"), fileName);
        MakeFullpath(&fileNameP, libDir, "params", fileName, NULL);
        paramF = fopen(fileNameP, "r");
        if (paramF == NULL) {
            NoticeMessage(MSG_PRMFIL_OPEN_NEW, _("Ok"), NULL, fileNameP);
            free(fileNameP);
            continue;
        }
        contents = NULL;
        while ((fgets(message, sizeof message, paramF)) != NULL) {
            if (strncmp(message, "CONTENTS", 8) == 0) {
                Stripcr(message);
                contents = message + 9;
                break;
            }
        }
        fclose(paramF);
        if (contents == NULL) {
            NoticeMessage(MSG_PRMFIL_NO_CONTENTS, _("Ok"), NULL, fileNameP);
            free(fileNameP);
            continue;
        }
        cp = wPrefGetString("Parameter File Map", contents);
        wPrefSetString("Parameter File Map", contents, fileNameP);
        if (cp != NULL && *cp != '\0') {
            /* been there, done that */
            free(fileNameP);
            continue;
        }

        free(fileNameP);
    }
    fclose(updateF);
    wPrefSetInteger("file", "updatetime", updateTime);
    return TRUE;
}

/**
 * Read the list of parameter files from configuration and load the files.
 *
 */
void LoadParamFileList(void)
{
    int fileNo;
    BOOL_T updated = FALSE;
    long *favoriteList = NULL;
    long favorites;
    int nextFavorite = 0;

    updated = UpdateParamFiles();

    wPrefGetIntegerBasic(FAVORITESECTION, FAVORITETOTALS, &favorites, 0);
    if (favorites) {
        DynString topic;
        favoriteList = MyMalloc(sizeof(long)*favorites);
        if (!favoriteList) {
            AbortProg("Couldn't allocate memory for favorite list!\n");
        }

        DynStringMalloc(&topic, 16);
        for (int i = 0; i < favorites; i++) {
            DynStringPrintf(&topic, FAVORITEKEY, i);
            wPrefGetIntegerBasic(FAVORITESECTION, DynStringToCStr(&topic), &favoriteList[i],
                                 0);
        }
        DynStringFree(&topic);
    }

    for (fileNo = 1; ; fileNo++) {
        char *fileName;
        const char * contents;
        enum paramFileState structState = PARAMFILE_UNLOADED;


        sprintf(message, "File%d", fileNo);
        contents = wPrefGetString("Parameter File Names", message);
        if (contents == NULL || *contents == '\0') {
            break;
        }
        InfoMessage("Parameters for %s", contents);
        fileName = wPrefGetString("Parameter File Map", contents);
        if (fileName == NULL || *fileName == '\0') {
            NoticeMessage(MSG_PRMFIL_NO_MAP, _("Ok"), NULL, contents);
            continue;
        }
        char * share;

       // Rewire to the latest system level
#if defined(WINDOWS)
#define SHAREPARAMS "\\share\\xtrkcad\\params\\"
#else
#define SHAREPARAMS "/share/xtrkcad/params/"
#endif
        if ((share= strstr(fileName,SHAREPARAMS))) {
        	share += strlen(SHAREPARAMS);
        	MakeFullpath(&fileName, wGetAppLibDir(), "params", share, NULL);
        	wPrefSetString("Parameter File Map", contents, fileName);
        }

        ReadParamFile(fileName);

        if (curContents == NULL) {
            curContents = curSubContents = MyStrdup(contents);
        }
        paramFileInfo(curParamFileIndex).contents = curContents;
        if (favoriteList && fileNo == favoriteList[nextFavorite]) {
            DynString topic;
            long deleted;
            DynStringMalloc(&topic, 16);
            DynStringPrintf(&topic, FAVORITEDELETED, fileNo);

            wPrefGetIntegerBasic(FAVORITESECTION, DynStringToCStr(&topic), &deleted, 0L);
            paramFileInfo(curParamFileIndex).favorite = TRUE;
            paramFileInfo(curParamFileIndex).deleted = deleted;
            if (nextFavorite < favorites - 1) {
                nextFavorite++;
            }
            DynStringFree(&topic);
        }

    }
    curParamFileIndex = PARAM_CUSTOM;
    if (updated) {
        SaveParamFileList();
    }

    MyFree(favoriteList);
}

/**
 * Save the currently selected parameter files. Parameter files that have been unloaded
 * are not saved.
 *
 */

void SaveParamFileList(void)
{
    int fileInx;
    int fileNo;
    int favorites;
    char * contents, *cp;

    for (fileInx = 0, fileNo = 1, favorites = 0; fileInx < paramFileInfo_da.cnt;
            fileInx++) {
        if (paramFileInfo(fileInx).valid && (!paramFileInfo(fileInx).deleted ||
                                             paramFileInfo(fileInx).favorite)) {
            sprintf(message, "File%d", fileNo);
            contents = paramFileInfo(fileInx).contents;
            for (cp = contents; *cp; cp++) {
                if (*cp == '=' || *cp == '\'' || *cp == '"' || *cp == ':' || *cp == '.') {
                    *cp = ' ';
                }
            }
            wPrefSetString("Parameter File Names", message, contents);
            wPrefSetString("Parameter File Map", contents, paramFileInfo(fileInx).name);
            if (paramFileInfo(fileInx).favorite) {
                sprintf(message, FAVORITEKEY, favorites);
                wPrefSetInteger(FAVORITESECTION, message, fileNo);
                sprintf(message, FAVORITEDELETED, fileNo);
                wPrefSetInteger(FAVORITESECTION, message, paramFileInfo(fileInx).deleted);
                favorites++;
            }
            fileNo++;
        }
    }
    sprintf(message, "File%d", fileNo);
    wPrefSetString("Parameter File Names", message, "");
    wPrefSetInteger(FAVORITESECTION, FAVORITETOTALS, favorites);
}

void
UpdateParamFileList(void)
{
    for (size_t i = 0; i < (unsigned)paramFileInfo_da.cnt; i++) {
        SetParamFileState(i);
    }
}

/**
 * Load the selected parameter files. This is a callback executed when the file selection dialog
 * is closed.
 * Steps:
 * - the parameters are read from file
 * - check is performed to see whether the content is already present, if yes the previously
 * loaded content is invalidated
 * - loaded parameter file is added to list of parameter files
 * - if a parameter file dialog exists the list is updated. It is either rewritten in
 * in case of an invalidated file or the new file is appended
 * - the settings are updated
 * These steps are repeated for every file in list
 *
 * \param files IN the number of filenames in the fileName array
 * \param fileName IN an array of fully qualified filenames
 * \param data IN ignored
 * \return  TRUE on success, FALSE on error
 */

int LoadParamFile(
    int files,
    char ** fileName,
    void * data)
{
    wIndex_t inx;
    int i = 0;

    assert(fileName != NULL);
    assert(files > 0);

    for (i = 0; i < files; i++) {
        enum paramFileState structState = PARAMFILE_UNLOADED;
        int newIndex;

        curContents = curSubContents = NULL;

        newIndex = ReadParamFile(fileName[i]);

        // in case the contents is already present, make invalid
        for (inx = 0; inx < newIndex; inx++) {
            if (paramFileInfo(inx).valid &&
                    strcmp(paramFileInfo(inx).contents, curContents) == 0) {
                paramFileInfo(inx).valid = FALSE;
                break;
            }
        }

        wPrefSetString("Parameter File Map", curContents,
                       paramFileInfo(curParamFileIndex).name);
    }
    //Only set the ParamFileDir if not the system directory
    if (!strstr(fileName[i-1],"/share/xtrkcad/params/"))
    	SetParamFileDir(fileName[i - 1]);
    curParamFileIndex = PARAM_CUSTOM;
    DoChangeNotification(CHANGE_PARAMS);
    return TRUE;
}

static void ReadCustom(void)
{
    FILE * f;
    MakeFullpath(&customPath, workingDir, sCustomF, NULL);
    customPathBak = MyStrdup(customPath);
    customPathBak[ strlen(customPathBak)-1 ] = '1';
    f = fopen(customPath, "r");
    if (f != NULL) {
        fclose(f);
        curParamFileIndex = PARAM_CUSTOM;
        ReadParams(0, workingDir, sCustomF);
    }
}


/*
 * Open the file and then set the locale to "C". Old locale will be copied to
 * oldLocale. After the required file I/O is done, the caller must call
 * CloseCustom() with the same locale value that was returned in oldLocale by
 * this function.
 */

FILE * OpenCustom(char *mode)
{
    FILE * ret = NULL;

    if (inPlayback) {
        return NULL;
    }
    if (*mode == 'w') {
        rename(customPath, customPathBak);
    }
    if (customPath) {
        ret = fopen(customPath, mode);
        if (ret == NULL) {
            NoticeMessage(MSG_OPEN_FAIL, _("Continue"), NULL, _("Custom"), customPath,
                          strerror(errno));
        }
    }

    return ret;
}
/**
 * Update and open the parameter files dialog box
 *
 * \param junk
 */

static void DoParamFileListDialog(void *junk)
{
    DoParamFiles(junk);
    ParamFileListLoad(paramFileInfo_da.cnt, &paramFileInfo_da);

}

addButtonCallBack_t ParamFilesInit(void)
{
	RegisterChangeNotification(ParamFilesChange);
    return &DoParamFileListDialog;
}

/**
 * Get the initial parameter files. The Xtrkcad.xtq file containing scale and
 * demo definitions is read.
 * 
 * \return FALSE on error, TRUE otherwise
 */
BOOL_T ParamFileListInit(void)
{
    log_params = LogFindIndex("params");

	// get the default definitions
    if (ReadParams(lParamKey, libDir, sParamQF) == FALSE) {
        return FALSE;
    }

    curParamFileIndex = PARAM_CUSTOM;

    if (lParamKey == 0) {
        LoadParamFileList();
        ReadCustom();
    }

    return TRUE;

}

/**
 * Deletes all parameter types described by index
 *
 * \param  index Zero-based index of the.
 */

static void
DeleteAllParamTypes(int index)
{

	DeleteTurnoutParams(index);
	DeleteCarProto(index);
	DeleteCarPart(index);
	DeleteStructures(index);
}


/**
 * Unload parameter file: all parameter definitions from this file are deleted
 * from memory. Strings allocated to store the filename and contents 
 * description are free'd as well. 
 * In order to keep the overall data structures consistent, the file info
 * structure is not removed from the array but flagged as garbage
 *
 * \param  fileIndex Zero-based index of the file.
 *
 * \returns True if it succeeds, false if it fails.
 */

bool
UnloadParamFile(wIndex_t fileIndex)
{
    paramFileInfo_p paramFileI = &paramFileInfo(fileIndex);
	
	DeleteAllParamTypes(fileIndex);
	
    MyFree(paramFileI->name);
    MyFree(paramFileI->contents);

    paramFileI->valid = FALSE;

    for (int i = 0; i < paramFileInfo_da.cnt; i++) {
        LOG1(log_params, ("UnloadParamFiles: = %s: %d\n", paramFileInfo(i).contents,
                          paramFileInfo(i).trackState))
    }

    return (true);
}

/**
 * Reload parameter file
 *
 * \param  index Zero-based index of the paramFileInfo struct.
 *
 * \returns True if it succeeds, false if it fails.
 */

bool
ReloadParamFile(wIndex_t index)
{
	paramFileInfo_p paramFileI = &paramFileInfo(index);
	
	DeleteAllParamTypes(index);
	MyFree(paramFileI->contents);

	ReloadDeletedParamFile(index);

	return(true);
}
