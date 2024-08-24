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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "custom.h"
#include "dynstring.h"
#include "file2uri.h"
#include "misc.h"
#include "note.h"
#include "param.h"
#include "paths.h"
#include "include/stringxtc.h"
#include "track.h"
#include "cundo.h"

#define MYMIN(x, y) (((x) < (y)) ? (x) : (y))

#define DOCUMENTFILEPATTERN "All Files (*.*)|*.*"
#define DOCUMENTPATHKEY "document"

struct {
	coOrd pos;
	int layer;
	track_p trk;
	char title[TITLEMAXIMUMLENGTH];
	char path[PATHMAXIMUMLENGTH];
} fileNoteData;

static struct wFilSel_t * documentFile_fs;

static void NoteFileOpenExternal(void * junk);
static void NoteFileBrowse(void * junk);
static void FileDlgUpdate(
        paramGroup_p pg,
        int inx,
        void * valueP);

static paramFloatRange_t noRangeCheck = { 0.0, 0.0, 80, PDO_NORANGECHECK_HIGH | PDO_NORANGECHECK_LOW };

// static char *toggleLabels[] = { N_("Copy to archive"), NULL };
static paramData_t fileNotePLs[] = {
#define I_ORIGX (0)
	/*0*/ { PD_FLOAT, &fileNoteData.pos.x, "origx", PDO_DIM|PDO_NOPREF, &noRangeCheck, N_("Position X") },
#define I_ORIGY (1)
	/*1*/ { PD_FLOAT, &fileNoteData.pos.y, "origy", PDO_DIM|PDO_NOPREF, &noRangeCheck, N_("Position Y") },
#define I_LAYER (2)
	/*2*/ { PD_DROPLIST, &fileNoteData.layer, "layer", PDO_NOPREF, I2VP(150), "Layer", 0 },
#define I_TITLE (3)
	/*3*/ { PD_STRING, &fileNoteData.title, "title", PDO_NOPREF | PDO_NOTBLANK, I2VP(200), N_("Title"), 0, 0, sizeof fileNoteData.title },
#define I_PATH (4)
	{ PD_STRING, &fileNoteData.path, "filename", PDO_NOPREF | PDO_NOTBLANK,   I2VP(200), N_("Document"), BO_READONLY, I2VP(0L), sizeof fileNoteData.path },
#define I_BROWSE (5)
	{ PD_BUTTON, NoteFileBrowse, "browse", 0L, NULL, N_("Select...") },
#define I_OPEN (6)
	{ PD_BUTTON, NoteFileOpenExternal, "openfile", PDO_DLGHORZ, NULL, N_("Open...") },
//#define I_ARCHIVE (7)
//	{ PD_TOGGLE, &noteFileData.inArchive, "archive", 0, toggleLabels, NULL },

};

static paramGroup_t fileNotePG = { "fileNote", 0, fileNotePLs, COUNT( fileNotePLs ) };
static wWin_p fileNoteW;

BOOL_T IsFileNote(track_p trk)
{
	struct extraDataNote_t * xx = GET_EXTRA_DATA( trk, T_NOTE, extraDataNote_t );

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
	wControlActive(fileNotePLs[I_OPEN].control, TRUE);
	ParamDialogOkActive(&fileNotePG, TRUE);
	strscpy(fileNoteData.path, *fileName, PATHMAXIMUMLENGTH );
	ParamLoadControl(&fileNotePG, I_PATH);

	return(0);
}

/**
 * Select the file to attach
 *
 * \param junk unused
 */
static void NoteFileBrowse(void * junk)
{
	documentFile_fs = wFilSelCreate(mainW, FS_LOAD, 0, _("Add Document"),
	                                DOCUMENTFILEPATTERN, LoadDocumentFile, NULL);

	wFilSelect(documentFile_fs, GetCurrentPath(DOCUMENTPATHKEY));

	wControlActive(fileNotePLs[I_OPEN].control,
	               (strlen(fileNoteData.path) ? TRUE : FALSE));
	FileDlgUpdate( &fileNotePG, I_PATH, NULL );
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
		wNoticeEx(NT_ERROR, _("The file doesn't exist or cannot be read!"), _("Cancel"),
		          NULL);
		if (fileNoteW) {
			wControlActive(fileNotePLs[I_OPEN].control, FALSE);
		}
	}
}

static void
NoteFileOpenExternal(void * junk)
{
	NoteFileOpen(fileNoteData.path);
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
		// TODO: redraw bitmap at new location
		//UpdateFile(&noteDataInUI, OR_NOTE, FALSE);
		break;
	case I_PATH:
		if (!IsFileValid(fileNoteData.path)) {
			paramData_p p = &fileNotePLs[I_PATH];
			p->bInvalid = TRUE;
			wWinPix_t h = wControlGetHeight(p->control);
			wControlSetBalloon( p->control, 0, h, "Document path is invalid" );
			ParamHilite( p->group->win, p->control, TRUE );
		}
		break;
	default:
		break;
	}
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
	track_p trk = fileNoteData.trk;
	if ( trk == NULL ) {
		// new file note
		trk = NewNote( -1, fileNoteData.pos, OP_NOTEFILE );
	} else {
		if ( ! descUndoStarted ) {
			UndoStart( _("Update File Note"), "Update File Note" );
			descUndoStarted = TRUE;
		}
		UndoModify( trk );
	}
	struct extraDataNote_t * xx = GET_EXTRA_DATA( trk, T_NOTE, extraDataNote_t );
	xx->pos = fileNoteData.pos;
	SetTrkLayer( trk, fileNoteData.layer );
	UndoDeferFree( xx->noteData.fileData.title );
	UndoDeferFree( xx->noteData.fileData.path );
	xx->noteData.fileData.title = MyStrdup( fileNoteData.title );
	xx->noteData.fileData.path = MyStrdup( fileNoteData.path );
	SetBoundingBox( trk, xx->pos, xx->pos );
	DrawNewTrack( trk );
	wHide(fileNoteW);
	ResetIfNotSticky();
	SetFileChanged();
}

/**
 * Show the attachment edit dialog. Create if non-existant
 *
 * \param trk IN track element to edit
 * \param windowTitle IN title for the edit dialog window
 */

static void CreateEditFileDialog(char * windowTitle)
{

	if (!fileNoteW) {
		ParamRegister(&fileNotePG);
		fileNoteW = ParamCreateDialog(&fileNotePG,
		                              "",
		                              _("Done"), FileEditOK,
		                              ParamCancel_Current, TRUE, NULL,
		                              F_BLOCK,
		                              FileDlgUpdate);
	}

	wWinSetTitle(fileNotePG.win, MakeWindowTitle(windowTitle));

	FillLayerList((wList_p)fileNotePLs[I_LAYER].control);
	ParamLoadControls(&fileNotePG);
	wControlActive(fileNotePLs[I_OPEN].control,
	               (IsFileValid(fileNoteData.path)?TRUE:FALSE));
	descTitle = windowTitle;

	wShow(fileNoteW);
}

/**
 * Activate note if double clicked
 * \param trk the note
 */

void ActivateFileNote(track_p trk)
{
	struct extraDataNote_t *xx = GET_EXTRA_DATA( trk, T_NOTE, extraDataNote_t );

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
	struct extraDataNote_t *xx = GET_EXTRA_DATA( trk, T_NOTE, extraDataNote_t );
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
	if ( ! inDescribeCmd ) {
		return;
	}


	fileNoteData.pos = xx->pos;
	fileNoteData.layer = GetTrkLayer( trk );
	fileNoteData.trk = trk;
	strscpy( fileNoteData.title, xx->noteData.fileData.title,
	         sizeof fileNoteData.title );
	strscpy( fileNoteData.path, xx->noteData.fileData.path,
	         sizeof fileNoteData.path );

	CreateEditFileDialog(_("Update Document"));
}

/**
 * Take a new note track element and initialize it. It will be
 * initialized with defaults and can then be edited by the user.
 *
 * \param the newly created trk
 */

void NewFileNoteUI(coOrd pos)
{
	char *tmpPtrText = _("Describe the file");

	fileNoteData.pos = pos;
	fileNoteData.layer = curLayer;
	fileNoteData.trk = NULL;
	strscpy( fileNoteData.title, tmpPtrText, sizeof fileNoteData.title );
	strscpy( fileNoteData.path, "", sizeof fileNoteData.path );

	CreateEditFileDialog( _("Attach Document"));
}
