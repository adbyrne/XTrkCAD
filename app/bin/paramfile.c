/** \file paramfile.c
 * Handling of parameter files
 */

/*  XTrackkCad - Model Railroad CAD
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
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compound.h"
#include "ctrain.h"
#include "custom.h"
#include "fileio.h"
#include "i18n.h"
#include "layout.h"
#include "messages.h"
#include "misc2.h"
#include "paths.h"
#include "include/paramfile.h"
#include "include/paramfilelist.h"
#include "include/utf8convert.h"

#if _MSC_VER >1300
	#define stricmp( a, b ) _stricmp(a, b )
#endif

static long paramCheckSum;

typedef enum paramFileState(*GetCompatibilityFunction)(int index,
        SCALEINX_T scale);

GetCompatibilityFunction GetCompatibility[] = {
    GetTrackCompatibility,
    GetStructureCompatibility,
    GetCarProtoCompatibility,
    GetCarPartCompatibility
};

#define COMPATIBILITYCHECKSCOUNT (sizeof(GetCompatibility)/sizeof(GetCompatibility[0]))

/**
 * Check whether parameter file is still loaded
 *
 * \param fileInx
 * \return TRUE if loaded, FALSE otherwise
 */

wBool_t IsParamValid(
    int fileInx)
{
    if (fileInx == PARAM_DEMO) {
        return (curDemo >= 0);
    } else if (fileInx == PARAM_CUSTOM) {
        return TRUE;
    } else if (fileInx == PARAM_LAYOUT) {
        return TRUE;
    } else if (fileInx >= 0 && fileInx < paramFileInfo_da.cnt) {
        return (!paramFileInfo(fileInx).deleted);
    } else {
        return FALSE;
    }
}

char *GetParamFileDir(void)
{
    return (GetCurrentPath(PARAMETERPATHKEY));
}

void
SetParamFileDir(char *fullPath)
{
    SetCurrentPath(PARAMETERPATHKEY, fullPath);
}

char * GetParamFileName(
    int fileInx)
{
    return paramFileInfo(fileInx).name;
}

char * GetParamFileContents(
    int fileInx)
{
    return paramFileInfo(fileInx).contents;
}

bool IsParamFileDeleted(int inx)
{
    return paramFileInfo(inx).deleted;
}

bool IsParamFileFavorite(int inx)
{
    return paramFileInfo(inx).favorite;
}

void SetParamFileDeleted(int fileInx, bool deleted)
{
    paramFileInfo(fileInx).deleted = deleted;
}

void SetParamFileFavorite(int fileInx, bool favorite)
{
    paramFileInfo(fileInx).favorite = favorite;
}

void ParamCheckSumLine(char * line)
{
    long mult = 1;
    while (*line) {
        paramCheckSum += (((long)(*line++)) & 0xFF)*(mult++);
    }
}

/**
 * Set the compatibility state of a parameter file
 *
 * \param index parameter file number in list
 * \return
 */

void SetParamFileState(int index)
{
    enum paramFileState state = PARAMFILE_NOTUSABLE;
    enum paramFileState newState;
    SCALEINX_T scale = GetLayoutCurScale();

    for (int i = 0; i < COMPATIBILITYCHECKSCOUNT && state < PARAMFILE_FIT &&
            state != PARAMFILE_UNLOADED; i++) {
        newState = (*GetCompatibility[i])(index, scale);
        if (newState > state || newState == PARAMFILE_UNLOADED) {
            state = newState;
        }
    }

    paramFileInfo(index).trackState = state;
}

/**
 * Read a single parameter file and update the parameter file list
 *
 * \param fileName full path for parameter file
 * \return
 */

int
ReadParamFile(const char *fileName)
{
    DYNARR_APPEND(paramFileInfo_t, paramFileInfo_da, 10);
    curParamFileIndex = paramFileInfo_da.cnt - 1;
    paramFileInfo(curParamFileIndex).name = MyStrdup(fileName);
    paramFileInfo(curParamFileIndex).deleted = FALSE;
    paramFileInfo(curParamFileIndex).valid = TRUE;
    paramFileInfo(curParamFileIndex).deletedShadow =
        paramFileInfo(curParamFileIndex).deleted = !ReadParams(0, NULL, fileName);
    paramFileInfo(curParamFileIndex).contents = MyStrdup(curContents);

    SetParamFileState(curParamFileIndex);

    return (curParamFileIndex);
}

/**
 * Paramter file reader and interpreter
 *
 * \param key unused
 * \param dirName prefix for parameter file path
 * \param fileName name of parameter file
 * \return
 */

bool ReadParams(
    long key,
    const char * dirName,
    const char * fileName)
{
    FILE * oldFile;
    char *cp;
    wIndex_t oldLineNum;
    wIndex_t pc;
    long oldCheckSum;
    long checkSum = 0;
    BOOL_T checkSummed;
    paramVersion = -1;
    char *oldLocale = NULL;

    if (dirName) {
        MakeFullpath(&paramFileName, dirName, fileName, NULL);
    } else {
        MakeFullpath(&paramFileName, fileName, NULL);
    }
    paramLineNum = 0;
    curBarScale = -1;
    curContents = strdup(fileName);
    curSubContents = curContents;

    //LOG1( log_paramFile, ("ReadParam( %s )\n", fileName ) )

    oldLocale = SaveLocale("C");

    paramFile = fopen(paramFileName, "r");
    if (paramFile == NULL) {
        /* Reset the locale settings */
        RestoreLocale(oldLocale);

        NoticeMessage(MSG_OPEN_FAIL, _("Continue"), NULL, _("Parameter"), paramFileName,
                      strerror(errno));

        return FALSE;
    }
    paramCheckSum = key;
    paramLineNum = 0;
    checkSummed = FALSE;
    BOOL_T skip = false;
    int skipLines = 0;
    while (paramFile && (fgets(paramLine, 256, paramFile)) != NULL) {
        paramLineNum++;
        Stripcr(paramLine);
        if (strncmp(paramLine, "CHECKSUM ", 9) == 0) {
            checkSum = atol(paramLine + 9);
            checkSummed = TRUE;
            goto nextLine;
        }
        ParamCheckSumLine(paramLine);
        if (paramLine[0] == '#') {
            /* comment */
        } else if (paramLine[0] == 0) {
            /* empty paramLine */
        } else if (strncmp(paramLine, "INCLUDE ", 8) == 0) {
            cp = &paramLine[8];
            while (*cp && isspace((unsigned char)*cp)) {
                cp++;
            }
            if (!*cp) {
                InputError("INCLUDE - no file name", TRUE);

                /* Close file and reset the locale settings */
                if (paramFile) {
                    fclose(paramFile);
                }
                RestoreLocale(oldLocale);

                return FALSE;
            }
            oldFile = paramFile;
            oldLineNum = paramLineNum;
            oldCheckSum = paramCheckSum;
            if ( !ReadParams(key, dirName, cp) ) {
		RestoreLocale( oldLocale );
		return FALSE;
            }
            paramFile = oldFile;
            paramLineNum = oldLineNum;
            paramCheckSum = oldCheckSum;
            if (dirName) {
                MakeFullpath(&paramFileName, dirName, fileName, NULL);
            } else {
                MakeFullpath(&paramFileName, fileName);
            }
            skip = FALSE;
        } else if (strncmp(paramLine, "CONTENTS ", 9) == 0) {
#ifdef WINDOWS
			ConvertUTF8ToSystem(paramLine + 9);
#endif
            curContents = MyStrdup(paramLine + 9);
            curSubContents = curContents;
            skip = FALSE;
        } else if (strncmp(paramLine, "SUBCONTENTS ", 12) == 0) {
#ifdef WINDOWS
			ConvertUTF8ToSystem(paramLine + 12);
#endif // WINDOWS
            curSubContents = MyStrdup(paramLine + 12);
            skip = FALSE;
        } else if (strncmp(paramLine, "PARAM ", 6) == 0) {
        	paramVersion = strtol( paramLine+8, &cp, 10 );
			if (cp)
				while (*cp && isspace((unsigned char)*cp)) cp++;
			if ( paramVersion > iParamVersion ) {
				if (cp && *cp) {
					NoticeMessage( MSG_PARAM_UPGRADE_VERSION1, _("Ok"), NULL, paramVersion, iParamVersion, sProdName, cp );
				} else {
					NoticeMessage( MSG_PARAM_UPGRADE_VERSION2, _("Ok"), NULL, paramVersion, iParamVersion, sProdName );
				}
				break;
			}
			if ( paramVersion < iMinParamVersion ) {
				NoticeMessage( MSG_PARAM_BAD_FILE_VERSION, _("Ok"), NULL, paramVersion, iMinParamVersion, sProdName );
				break;
			}
        } else if (skip && (strncmp(paramLine, "  ",1) == 0)) {
        	//Always skip to next line starting in LeftHandColumn
        	skipLines++;
        	goto nextLine;
        } else {
			for (pc = 0; pc < paramProc_da.cnt; pc++) {
				if (strncmp(paramLine, paramProc(pc).name,
							strlen(paramProc(pc).name)) == 0) {
					skip = FALSE;					//Stop skip so we re-message
					paramProc(pc).proc(paramLine);
					goto nextLine;
				}
			}
			if (!skip) {
				if (InputError(_("Unknown param file line - skip until next good object?"), TRUE)) {   //OK to carry on
					/* SKIP until next main line we recognize */
					skip = TRUE;
					skipLines++;
					goto nextLine;
				} else {
					if (skipLines>0)
						NoticeMessage( MSG_PARAM_LINES_SKIPPED, _("Ok"), NULL, paramFileName, skipLines);
					if ( paramFile ) {
						fclose(paramFile);
						paramFile = NULL;
					}
					if ( paramFileName ) {
						free(paramFileName);
						paramFileName = NULL;
					}
					RestoreLocale(oldLocale);
					return FALSE;
				}
			}
			skipLines++;
        }
nextLine:
        ;
    }
    if (key) {
        if (!checkSummed || checkSum != paramCheckSum) {
            /* Close file and reset the locale settings */
            if (paramFile) {
                fclose(paramFile);
            }
            RestoreLocale(oldLocale);

            NoticeMessage(MSG_PROG_CORRUPTED, _("Ok"), NULL, paramFileName);

            return FALSE;
        }
    }
    if (skipLines>0)
    	NoticeMessage( MSG_PARAM_LINES_SKIPPED, _("Ok"), NULL, paramFileName, skipLines);
    if (paramFile) {
        fclose(paramFile);
    }
    free(paramFileName);
    paramFileName = NULL;
    RestoreLocale(oldLocale);

    return TRUE;
}
