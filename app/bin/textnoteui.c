/** \file textnoteui.c
 * View for the text note
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
#include "misc.h"
#include "note.h"
#include "param.h"
#include "shortentext.h"
#include "track.h"

struct {
	coOrd pos;
	int layer;
	track_p trk;
} textNoteData;

static paramTextData_t noteTextData = { 300, 150 };
static paramFloatRange_t r_1000_1000 = { -1000.0, 1000.0, 80 };
static paramData_t textEditPLs[] = {
#define I_ORIGX (0)
	/*0*/ { PD_FLOAT, &textNoteData.pos.x, "origx", PDO_DIM|PDO_NOPREF, &r_1000_1000, N_("Position X") },
#define I_ORIGY (1)
	/*1*/ { PD_FLOAT, &textNoteData.pos.y, "origy", PDO_DIM|PDO_NOPREF, &r_1000_1000, N_("Position Y") },
#define I_LAYER (2)
	/*2*/ { PD_DROPLIST, &textNoteData.layer, "layer", PDO_NOPREF, I2VP(150), "Layer", 0 },
#define I_TEXT (3)
	/*3*/ { PD_TEXT, NULL, "text", PDO_NOPREF, &noteTextData, N_("Note") }
};

static paramGroup_t textEditPG = { "textEdit", 0, textEditPLs, COUNT( textEditPLs ) };
static wWin_p textEditW;

#define textEntry	((wText_p)textEditPLs[I_TEXT].control)


/**
 * Callback for text note dialog
 *
 * \param pg IN unused
 * \param inx IN index into dialog template
 * \param valueP IN unused
 */
static void
TextDlgUpdate(
        paramGroup_p pg,
        int inx,
        void * valueP)
{
	switch (inx) {
	case I_ORIGX:
	case I_ORIGY:
		// TODO: Redraw bitmap at new location
		break;
	default:
		break;
	}
}

/**
* Handle Cancel button: restore old values for layer and position
*/

static void
TextEditCancel( wWin_p junk )
{
	ResetIfNotSticky();
	wHide(textEditW);
}

/**
 * Handle OK button: make sure the entered URL is syntactically valid, update
 * the layout and close the dialog
 *
 * \param junk
 */

static void
TextEditOK(void *junk)
{
	track_p trk = textNoteData.trk;
	if ( trk == NULL ) {
		// new note
		trk = NewNote( -1, textNoteData.pos, OP_NOTETEXT );
	}
	struct extraDataNote_t * xx = GET_EXTRA_DATA( trk, T_NOTE, extraDataNote_t );
	xx->pos = textNoteData.pos;
	SetTrkLayer( trk, textNoteData.layer );

	int len = wTextGetSize(textEntry);
	MyFree( xx->noteData.text );
	xx->noteData.text = (char*)MyMalloc(len + 2);
	wTextGetText(textEntry, xx->noteData.text, len);

	SetBoundingBox( trk, xx->pos, xx->pos );
	DrawNewTrack( trk );
	wHide(textEditW);
	ResetIfNotSticky();
	SetFileChanged();
}



/**
 * Create the edit dialog for text notes.
 *
 * \param trk IN selected note
 * \param title IN dialog title
 */
static void
CreateEditTextNote(char *title, char * textData )
{
	// create the dialog if necessary
	if (!textEditW) {
		ParamRegister(&textEditPG);
		textEditW = ParamCreateDialog(&textEditPG,
		                              "",
		                              _("Done"), TextEditOK,
		                              TextEditCancel, TRUE, NULL,
		                              F_BLOCK,
		                              TextDlgUpdate);
	}

	wWinSetTitle(textEditPG.win, MakeWindowTitle(title));

	wTextClear(textEntry);
	wTextAppend(textEntry, textData );
	wTextSetReadonly(textEntry, FALSE);
	FillLayerList((wList_p)textEditPLs[I_LAYER].control);
	ParamLoadControls(&textEditPG);

	// and show the dialog
	wShow(textEditW);
}

/**
 * Show details in statusbar. If running in Describe mode, the describe dialog is opened for editing the note
 *
 * \param trk IN the selected track (note)
 * \param str IN the buffer for the description string
 * \param len IN length of string buffer str
*/

void DescribeTextNote(track_p trk, char * str, CSIZE_T len)
{
	struct extraDataNote_t *xx = GET_EXTRA_DATA( trk, T_NOTE, extraDataNote_t );
	char *noteText;
	DynString statusLine;

	noteText = MyMalloc(strlen(xx->noteData.text) + 1);
	RemoveFormatChars(xx->noteData.text, noteText);
	EllipsizeString(noteText, NULL, 80);
	DynStringMalloc(&statusLine, 100);

	DynStringPrintf(&statusLine,
	                _("Note: Layer=%d %-.80s"),
	                GetTrkLayer(trk)+1,
	                noteText );
	strcpy(str, DynStringToCStr(&statusLine));

	DynStringFree(&statusLine);
	if ( ! inDescribeCmd ) {
		return;
	}
	textNoteData.pos = xx->pos;
	textNoteData.layer = GetTrkLayer( trk );
	textNoteData.trk = trk;

	CreateEditTextNote(_("Update comment"), xx->noteData.text );
}

/**
 * Show the UI for entering new text notes
 *
 * \param xx Note object data
 */

void NewTextNoteUI(coOrd pos )
{
	char *tmpPtrText = _("Replace this text with your note");

	textNoteData.pos = pos;
	textNoteData.layer = curLayer;
	textNoteData.trk = NULL;

	CreateEditTextNote(_("Create Text Note"), tmpPtrText );
}

