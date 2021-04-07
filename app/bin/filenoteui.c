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
#ifdef WINDOWS
	#include <io.h>
	#define access(path,mode) _access(path,mode)
	#define F_OK (0) 
#else
	#include <unistd.h>
#endif
#include "custom.h"
#include "dynstring.h"
#include "file2uri.h"
#include "i18n.h"
#include "misc.h"
#include "note.h"
#include "param.h"
#include "paths.h"
#include "include/stringxtc.h"
#include "track.h"
#include "wlib.h"

extern BOOL_T inDescribeCmd;

#define MYMIN(x, y) (((x) < (y)) ? (x) : (y))

#define DOCUMENTFILEPATTERN "All Files (*.*)|*.*"
#define DOCUMENTPATHKEY "document"

static struct extraDataNote noteDataInUI;
static struct wFilSel_t * documentFile_fs;

static void NoteFileOpenExternal(void * junk);
static void NoteFileBrowse(void * junk);

static paramFloatRange_t r_1000_1000 = { -1000.0, 1000.0, 80 };

// static char *toggleLabels[] = { N_("Copy to archive"), NULL };
static paramData_t fileEditPLs[] = {
#define I_ORIGX (0)
    /*0*/ { PD_FLOAT, &noteDataInUI.pos.x, "origx", PDO_DIM, &r_1000_1000, N_("Position X") },
#define I_ORIGY (1)
    /*1*/ { PD_FLOAT, &noteDataInUI.pos.y, "origy", PDO_DIM, &r_1000_1000, N_("Position Y") },
#define I_LAYER (2)
    /*2*/ { PD_DROPLIST, &noteDataInUI.layer, "layer", 0, (void*)150, "Layer", 0 },
#define I_TITLE (3)
    /*3*/ { PD_STRING, NULL, "title", PDO_NOPREF | PDO_STRINGLIMITLENGTH, (void*)200, N_("Title"), 0, 0, TITLEMAXIMUMLENGTH-1 },
#define I_PATH (4)
	{ PD_STRING, NULL, "filename", PDO_NOPSHUPD,   (void*)200, N_("Document"), BO_READONLY, (void *)0L },
#define I_BROWSE (5)
	{ PD_BUTTON, (void *)NoteFileBrowse, "browse", 0L, NULL, N_("Select...") },
#define I_OPEN (6)
	{ PD_BUTTON, (void*)NoteFileOpenExternal, "openfile", PDO_DLGHORZ, NULL, N_("Open...") },
//#define I_ARCHIVE (7)
//	{ PD_TOGGLE, &noteFileData.inArchive, "archive", 0, toggleLabels, NULL },

};

static paramGroup_t fileEditPG = { "fileEdit", 0, fileEditPLs, sizeof fileEditPLs / sizeof fileEditPLs[0] };
static wWin_p fileEditW;

BOOL_T IsFileNote(track_p trk)
{
    struct extraDataNote * xx = (struct extraDataNote *)GetTrkExtraData(trk);

	return(xx->op == OP_NOTEFILE );
}

/** Check for the file existance
 *
 * \param fileName IN file
 * \return TRUE if exists, FALSE otherwise
 */
BOOL_T IsFileValid(char *fileName)
{
	if (!strlen(fileName)) {
		return(FALSE); 
	} else {
		if (access(fileName, F_OK) == -1) {
			return(FALSE);
		}
	}
	
	return(TRUE);
}

/**
 * Put the selected filename into the dialog
 *
 * \param files IN always 1
 * \param fileName IN name of selected file
 * \param data IN ignored
 * \return always 0
 */
int LoadDocumentFile(
	int files,
	char ** fileName,
	void * data)
{
	wControlActive(fileEditPLs[I_OPEN].control, TRUE);
	ParamDialogOkActive(&fileEditPG, TRUE);
	strscpy(noteDataInUI.noteData.fileData.path, *fileName, PATHMAXIMUMLENGTH );
	ParamLoadControl(&fileEditPG, I_PATH);

	return(0);
}

/**
 * Select the file to attach
 * 
 * \param junk unused
 */
static void NoteFileBrowse(void * junk)
{
	documentFile_fs = wFilSelCreate(mainW, FS_LOAD, 0, _("Add Document"), DOCUMENTFILEPATTERN, LoadDocumentFile, NULL);

	wFilSelect(documentFile_fs, GetCurrentPath(DOCUMENTPATHKEY));

	wControlActive(fileEditPLs[I_OPEN].control,
				   (strlen(noteDataInUI.noteData.fileData.path) ? TRUE : FALSE));

	return;
}

/**
 * Open the file using an external program. Before opening the file existance and permissions are checked.
 * If access is not allowed the Open Button is disabled
 * 
 * \param fileName IN file
 */
 
static void NoteFileOpen(char *fileName)
{
	if (IsFileValid(fileName)) {
		wOpenFileExternal(fileName);
	} else {
		wNoticeEx(NT_ERROR, _("The file doesn't exist or cannot be read!"), _("Cancel"), NULL);
		if (fileEditW) {
			wControlActive(fileEditPLs[I_OPEN].control, FALSE);
		}
	}
}

static void
NoteFileOpenExternal(void * junk)
{
	NoteFileOpen(noteDataInUI.noteData.fileData.path);
}
/**
 * Handle the dialog actions
 */
static void
FileDlgUpdate(
    paramGroup_p pg,
    int inx,
    void * valueP)
{
    switch (inx) {
	case I_ORIGX:
	case I_ORIGY:
		UpdateFile(&noteDataInUI, OR_NOTE, FALSE);
		break;
	case I_LAYER:
		UpdateFile(&noteDataInUI, LY_NOTE, FALSE);
		break;
    case I_PATH:
        if (IsFileValid(noteDataInUI.noteData.fileData.path)) {
            wControlActive(fileEditPLs[I_OPEN].control, TRUE);
        } else {
            wControlActive(fileEditPLs[I_OPEN].control, FALSE);
        }
        break;
    default:
		break;
    }
}


/** 
 * Handle Cancel button: restore old values for layer and position
 */

static void
FileEditCancel( wWin_p junk)
{
	if (inDescribeCmd) {
		UpdateFile(&noteDataInUI, CANCEL_NOTE, FALSE);
	}
	ResetIfNotSticky();
	wHide(fileEditW);
}
/**
 * Handle OK button: make sure the entered filename is syntactically valid, update
 * the layout and close the dialog
 *
 * \param junk
 */

static void
FileEditOK(void *junk)
{
    UpdateFile(&noteDataInUI, OK_FILE, FALSE);
    wHide(fileEditW);
	ResetIfNotSticky();
	FileIsChanged();
}

/**
 * Show the attachment edit dialog. Create if non-existant
 *
 * \param trk IN track element to edit
 * \param windowTitle IN title for the edit dialog window
 */

void CreateEditFileDialog(track_p trk, char * windowTitle)
{
    struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);

    if (!fileEditW) {
		noteDataInUI.noteData.fileData.path = MyMalloc(PATHMAXIMUMLENGTH);
		noteDataInUI.noteData.fileData.title = MyMalloc(TITLEMAXIMUMLENGTH);
		fileEditPLs[I_TITLE].valueP = noteDataInUI.noteData.fileData.title;
		fileEditPLs[I_PATH].valueP = noteDataInUI.noteData.fileData.path;

		ParamRegister(&fileEditPG);
        fileEditW = ParamCreateDialog(&fileEditPG,
                                      "",
                                      _("Done"), FileEditOK,
                                      FileEditCancel, TRUE, NULL,
                                      F_BLOCK,
                                      FileDlgUpdate);
    }

    wWinSetTitle(fileEditPG.win, MakeWindowTitle(windowTitle));

    noteDataInUI.pos = xx->pos;
	noteDataInUI.layer = xx->layer;
    noteDataInUI.trk = trk;
	strscpy(noteDataInUI.noteData.fileData.title, xx->noteData.fileData.title, TITLEMAXIMUMLENGTH);
	strscpy(noteDataInUI.noteData.fileData.path, xx->noteData.fileData.path, PATHMAXIMUMLENGTH);
	FillLayerList((wList_p)fileEditPLs[I_LAYER].control);
	ParamLoadControls(&fileEditPG);
	wControlActive(fileEditPLs[I_OPEN].control, (IsFileValid(noteDataInUI.noteData.fileData.path)?TRUE:FALSE));
	
    wShow(fileEditW);
}

/**
 * Activate note if double clicked
 * \param trk the note
 */

void ActivateFileNote(track_p trk)
{
    struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);

	NoteFileOpen(xx->noteData.fileData.path);
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

    DynStringMalloc(&statusLine, 80);

    DynStringPrintf(&statusLine, 
					_("Document(%d) Layer=%d %-.80s [%s]"),
					GetTrkIndex(trk),
					GetTrkLayer(trk) + 1,
					xx->noteData.fileData.title, 
					xx->noteData.fileData.path);

    strcpy(str, DynStringToCStr(&statusLine));
    DynStringFree(&statusLine);

	if (inDescribeCmd) {
		NoteStateSave(trk);
 
		CreateEditFileDialog(trk, _("Update document"));
	}
}

/**
 * Take a new note track element and initialize it. It will be
 * initialized with defaults and can then be edited by the user.
 *
 * \param the newly created trk
 */

void NewFileNoteUI(track_p trk)
{
	struct extraDataNote * xx = (struct extraDataNote *)GetTrkExtraData(trk);
	char *tmpPtrText = _("Describe the file");

	xx->noteData.fileData.title = MyStrdup(tmpPtrText);
	xx->noteData.fileData.path = MyStrdup("");

    CreateEditFileDialog(trk, 
						_("Attach document"));
}
