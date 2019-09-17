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
  *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
  */
  
#include <string.h>
#include <stdbool.h>

#include "custom.h"
#include "dynstring.h"
#include "i18n.h"
#include "misc.h"
#include "note.h"
#include "param.h"
#include "shortentext.h"
#include "track.h" 
#include "wlib.h"

static struct extraDataNote	noteDataInUI;

static paramTextData_t noteTextData = { 300, 150 };
static paramFloatRange_t r_1000_1000 = { -1000.0, 1000.0, 80 };
static paramData_t textEditPLs[] = {
#define I_ORIGX (0)
	/*0*/ { PD_FLOAT, &noteDataInUI.pos.x, "origx", PDO_DIM, &r_1000_1000, N_("Position X") },
#define I_ORIGY (1)
	/*1*/ { PD_FLOAT, &noteDataInUI.pos.y, "origy", PDO_DIM, &r_1000_1000, N_("Position Y") },
#define I_LAYER (2)
	/*2*/ { PD_DROPLIST, &noteDataInUI.layer, "layer", 0, (void*)150, "Layer", 0 },
#define I_TEXT (3)
	/*3*/ { PD_TEXT, NULL, "text", PDO_NOPREF, &noteTextData, N_("Note") }
};

static paramGroup_t textEditPG = { "textEdit", 0, textEditPLs, sizeof textEditPLs / sizeof textEditPLs[0] };
static wWin_p textEditW;

#define textEntry	((wText_p)textEditPLs[I_TEXT].control)

extern BOOL_T inDescribeCmd;

/**
 * Return the current text 
 * 
 */
static void GetNoteTextData()
{
	int len;

	if (noteDataInUI.noteData.text ) {
		MyFree(noteDataInUI.noteData.text);
	}
	len = wTextGetSize(textEntry);
	noteDataInUI.noteData.text = (char*)MyMalloc(len + 2);
	wTextGetText(textEntry, noteDataInUI.noteData.text, len);
	return;
}

/**
 * Check validity of entered text
 * 
 * \return always TRUE for testing
 */
bool
IsValidText()
{
	return(TRUE);
}


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
		UpdateText(&noteDataInUI, OR_NOTE, FALSE);
		break;
	case I_LAYER:
		UpdateText(&noteDataInUI, LY_NOTE, FALSE);
		break;
	case I_TEXT:
		/** TODO: this is never called, why doesn't text update trigger this callback? */
		GetNoteTextData();
		if (IsValidText(noteDataInUI.noteData.text)) {
			ParamDialogOkActive(&textEditPG, TRUE);
		} else {
			ParamDialogOkActive(&textEditPG, FALSE);
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
TextEditCancel(void *junk)
{
	if (inDescribeCmd) {
		UpdateText(&noteDataInUI, CANCEL_NOTE, FALSE);
	}
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
	GetNoteTextData();
	UpdateText(&noteDataInUI, OK_TEXT, FALSE);
	wHide(textEditW);
	ResetIfNotSticky();
	FileIsChanged();
}



/**
 * Create the edit dialog for text notes.
 * 
 * \param trk IN selected note
 * \param title IN dialog title
 */
static void
CreateEditTextNote(track_p trk, char *title)
{
	struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);

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

	// initialize the dialog fields
	noteDataInUI.pos = xx->pos;
	noteDataInUI.layer = xx->layer;
	noteDataInUI.trk = trk;

	wTextClear(textEntry);
	wTextAppend(textEntry, xx->noteData.text );
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
	struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);
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
	MyFree(noteText);

	if (inDescribeCmd) {
		NoteStateSave(trk);

		CreateEditTextNote(trk, _("Update comment"));
	}
}

/**
 * Show the UI for entering new text notes
 * 
 * \param xx Note object data
 */

void NewTextNoteUI(track_p trk) {
	struct extraDataNote * xx = (struct extraDataNote *)GetTrkExtraData(trk);
	char *tmpPtrText = _("Replace this text with your note");

	xx->noteData.text = MyStrdup(tmpPtrText);

	CreateEditTextNote(trk, _("Create Text Note"));
}

