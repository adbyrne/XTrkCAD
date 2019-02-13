/** \file filenoteui.c
 * View for the file note
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2018 Martin Fischer
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

#include <string.h>
#include <stdbool.h>

#include "custom.h"
#include "dynstring.h"
#include "file2uri.h"
#include "i18n.h"
#include "misc.h"
#include "note.h"
#include "param.h"
#include "paths.h"
#include "track.h"
#include "wlib.h"

extern BOOL_T inDescribeCmd;

#define MYMIN(x, y) (((x) < (y)) ? (x) : (y))

#define DOCUMENTFILEPATTERN "*"
#define DOCUMENTPATHKEY "document"

static struct noteFileData noteFileData;
static struct wFilSel_t * documentFile_fs;

static void NoteFileOpen(void * junk);
static void DocumentFileBrowse(void * junk);

static paramFloatRange_t r_1000_1000 = { -1000.0, 1000.0, 80 };


static char *toggleLabels[] = { N_("Copy to archive"), NULL };
static paramData_t fileEditPLs[] = {
#define I_ORIGX (0)
    /*0*/ { PD_FLOAT, &noteFileData.pos.x, "origx", PDO_DIM, &r_1000_1000, N_("Position X") },
#define I_ORIGY (1)
    /*1*/ { PD_FLOAT, &noteFileData.pos.y, "origy", PDO_DIM, &r_1000_1000, N_("Position Y") },
#define I_LAYER (2)
    /*2*/ { PD_DROPLIST, &noteFileData.layer, "layer", 0, (void*)150, "Layer", 0 },
#define I_TITLE (3)
    /*3*/ { PD_STRING, noteFileData.title, "title", PDO_NOPREF | PDO_STRINGLIMITLENGTH, (void*)200, N_("Title"), 0, 0, sizeof(noteFileData.title)-1 },
#define I_OPEN (4)
	{ PD_BUTTON, (void*)NoteFileOpen, "openfile", PDO_DLGHORZ, NULL, N_("Open...") },
#define I_PATH (5)
	{ PD_STRING, noteFileData.path, "filename", PDO_NOPSHUPD,   (void*)200, N_("Document"), 0, (void *)0L },
#define I_BROWSE (6)
	{ PD_BUTTON, (void*)DocumentFileBrowse, "browse", PDO_DLGHORZ, NULL, N_("Browse ...") },
#define I_ARCHIVE (7)
	{ PD_TOGGLE, &noteFileData.inArchive, "archive", 0, toggleLabels, NULL },

};

static paramGroup_t fileEditPG = { "fileEdit", 0, fileEditPLs, sizeof fileEditPLs / sizeof fileEditPLs[0] };
static wWin_p fileEditW;

/**
 *
 */
struct noteFileData *
GetNoteFileData(void)
{
    return (&noteFileData);
}

/**
 * Simplistic checking of filename syntax validity
 *
 * \param testString
 * \return
 */
static bool
IsValidFile(char *testString)
{
	if (!strlen(testString)) {	// empty string is valid
		return (TRUE);
	}
#ifdef WINDOWS
	if (testString[1] == ':' && testString[strlen(testString)] != '/') {
		return(TRUE);
	}
#else
	if (testString[0] == '/' && testString[strlen(testString)] != '/') {
		return(TRUE);
	}
#endif

	return (FALSE);
}

bool
IsFileNote(track_p trk)
{
    struct extraDataNote * xx = (struct extraDataNote *)GetTrkExtraData(trk);
    char path[PATHMAXIMUMLENGTH];

    SplitNoteUri(xx->text, path, PATHMAXIMUMLENGTH, NULL, 0);

	return(IsValidFile(path));
}


int LoadDocumentFile(
	int files,
	char ** fileName,
	void * data)
{
	if (IsValidFile(*fileName)) {
		wControlActive(fileEditPLs[I_OPEN].control, TRUE);
		ParamDialogOkActive(&fileEditPG, TRUE);
		strlcpy(noteFileData.path, *fileName, PATHMAXIMUMLENGTH - 1);
		ParamLoadControl(&fileEditPG, I_PATH);
	}
	return(0);
}

/************************************************************
 * Run File Select for the Document File
 */
static void DocumentFileBrowse(void * junk)
{
	documentFile_fs = wFilSelCreate(mainW, FS_LOAD, 0, _("Add Document"), DOCUMENTFILEPATTERN, LoadDocumentFile, NULL);

	wFilSelect(documentFile_fs, GetCurrentPath(DOCUMENTPATHKEY));
	return;
}

static void NoteFileOpen(void * junk)
{
    ParamLoadControl(&fileEditPG, I_PATH);
    wOpenFileExternal(noteFileData.path);
}

static void
FileDlgUpdate(
    paramGroup_p pg,
    int inx,
    void * valueP)
{
    switch (inx) {
    case I_PATH:
        if (IsValidFile(noteFileData.path)) {
            wControlActive(fileEditPLs[I_OPEN].control, TRUE);
            ParamDialogOkActive(&fileEditPG, TRUE);
        } else {
            wControlActive(fileEditPLs[I_OPEN].control, FALSE);
            ParamDialogOkActive(&fileEditPG, FALSE);
        }

        break;
    default:
        //UpdateFile(noteFileData.trk, inx, NULL, FALSE);
		break;
    }
}

/*!
 * Handle OK button: make sure the entered filename is syntactically valid, update
 * the layout and close the dialog
 *
 * \param junk
 */

static void
FileEditOK(void *junk)
{
    UpdateFile(noteFileData.trk, OK_FILE, NULL, FALSE);
    wHide(fileEditW);
	ResetIfNotSticky();
}


void CreateEditFileDialog(track_p trk, char * windowTitle, char * defaultTitle, char * defaultPath)
{
    struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);

    if (!inDescribeCmd) {
        return;
    }

    if (!fileEditW) {
        ParamRegister(&fileEditPG);
        fileEditW = ParamCreateDialog(&fileEditPG,
                                      "",
                                      _("Done"), FileEditOK,
                                      NULL, TRUE, NULL,
                                      F_BLOCK,
                                      FileDlgUpdate);
    }

    wWinSetTitle(fileEditPG.win, MakeWindowTitle(windowTitle));

    noteFileData.pos = xx->pos;
    noteFileData.trk = trk;
	if (defaultTitle) {
		strlcpy(noteFileData.title, defaultTitle, sizeof(noteFileData.title) - 1);
	} else {
		*noteFileData.title = '\0';
	}

	if (defaultPath) {
		strlcpy(noteFileData.path, defaultPath, sizeof(noteFileData.path) - 1);
	} else {
		*noteFileData.path = '\0';
	}

	ParamLoadControls(&fileEditPG);
    FillLayerList((wList_p)fileEditPLs[I_LAYER].control);
    wShow(fileEditW);
}

/**
 * Activate note if double clicked
 * \param trk the note
 */

void ActivateFileNote(track_p trk)
{
    struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);
	char *path;

	path = MyMalloc(strlen(xx->text));

    SplitNoteUri(xx->text, noteFileData.path, PATHMAXIMUMLENGTH, NULL, 0);
	URI2File(path, PATHMAXIMUMLENGTH, noteFileData.path);

    NoteFileOpen(NULL);
}

/**
 * Describe and enable editing of an existing link note
 *
 * \param trk the existing, valid note
 * \param str the field to put a text version of the note so it will appear on the status line
 * \param len the lenght of the field
 */

void DescribeFileNote(track_p trk, char * str, CSIZE_T len)
{
    struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);
    DynString statusLine;
    char path[PATHMAXIMUMLENGTH];
    char title[TITLEMAXIMUMLENGTH];
	char *decodedPath;

    DynStringMalloc(&statusLine, 80);

    SplitNoteUri(xx->text, path, PATHMAXIMUMLENGTH,  title, TITLEMAXIMUMLENGTH);
	decodedPath = MyMalloc(strlen(path)+1);
	URI2File(path, strlen(path) + 1, decodedPath);

    DynStringPrintf(&statusLine, _("Document: %-.80s (%s)"), title, decodedPath);
    strcpy(str, DynStringToCStr(&statusLine));
    DynStringFree(&statusLine);
	
	CreateEditFileDialog(trk, _("Describe attachment"), title, decodedPath );
	
	MyFree(decodedPath);
}

/**
 * Take a new note track element and initialize it. It will be
 * initialized with defaults and can then be edited by the user.
 *
 * \param the newly created trk
 */

void NewFileNoteUI(track_p trk)
{
    CreateEditFileDialog(trk, 
						_("Attach document"),
                        NULL, NULL );
}
