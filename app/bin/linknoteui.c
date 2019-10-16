/** \file linknoteui.c
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
#include "stringxtc.h"
#include "track.h"
#include "validator.h"
#include "wlib.h"

extern BOOL_T inDescribeCmd;

#define DEFAULTLINKURL "http://www.xtrkcad.org/"
#define DEFAULTLINKTITLE "The XTrackCAD Homepage"

static struct extraDataNote noteDataInUI;

static void NoteLinkBrowse(void *junk);
static void NoteLinkOpen(char *url );

static paramFloatRange_t r_1000_1000 = { -1000.0, 1000.0, 80 };
static paramData_t linkEditPLs[] = {
#define I_ORIGX (0)
    /*0*/ { PD_FLOAT, &noteDataInUI.pos.x, "origx", PDO_DIM, &r_1000_1000, N_("Position X") },
#define I_ORIGY (1)
    /*1*/ { PD_FLOAT, &noteDataInUI.pos.y, "origy", PDO_DIM, &r_1000_1000, N_("Position Y") },
#define I_LAYER (2)
    /*2*/ { PD_DROPLIST, &noteDataInUI.layer, "layer", 0, (void*)150, "Layer", 0 },
#define I_TITLE (3)
    /*3*/ { PD_STRING, NULL, "title", PDO_NOPREF | PDO_STRINGLIMITLENGTH, (void*)200, N_("Title"), 0, 0, TITLEMAXIMUMLENGTH-1 },
#define I_URL (4)
    /*4*/ { PD_STRING, NULL, "name", PDO_NOPREF | PDO_STRINGLIMITLENGTH, (void*)200, N_("URL"), 0, 0, URLMAXIMUMLENGTH-1 },
#define I_OPEN (5)
	/*5*/{ PD_BUTTON, (void*)NoteLinkBrowse, "openlink", PDO_DLGHORZ, NULL, N_("Open...") },
};

static paramGroup_t linkEditPG = { "linkEdit", 0, linkEditPLs, sizeof linkEditPLs / sizeof linkEditPLs[0] };
static wWin_p linkEditW;

BOOL_T
IsLinkNote(track_p trk)
{
    struct extraDataNote * xx = (struct extraDataNote *)GetTrkExtraData(trk);

	return(xx->op == OP_NOTELINK);
}


/**
 * Callback for Open URL button
 *
 * \param junk IN ignored
 */
static void NoteLinkBrowse(void *junk)
{
	NoteLinkOpen(noteDataInUI.noteData.linkData.url);
}

/**
 * Open the URL in the external default browser
 *
 * \param url IN url to open
 */
static void NoteLinkOpen(char *url)
{
    wOpenFileExternal(url);
}

static void
LinkDlgUpdate(
    paramGroup_p pg,
    int inx,
    void * valueP)
{
    switch (inx) {
    case I_URL:
        if (strlen(noteDataInUI.noteData.linkData.url) == 0 || IsValidURL(noteDataInUI.noteData.linkData.url)) {
            wControlActive(linkEditPLs[I_OPEN].control, TRUE);
            ParamDialogOkActive(&linkEditPG, TRUE);
        } else {
            wControlActive(linkEditPLs[I_OPEN].control, FALSE);
            ParamDialogOkActive(&linkEditPG, FALSE);
        }
        break;
	case I_ORIGX:
	case I_ORIGY:
		UpdateLink(&noteDataInUI, OR_NOTE, FALSE);
		break;
	case I_LAYER:
		UpdateLink(&noteDataInUI, LY_NOTE, FALSE);
		break;
	default:
		break;
    }
}

/**
* Handle Cancel button: restore old values for layer and position
*/

static void
LinkEditCancel( wWin_p junk)
{
	if (inDescribeCmd) {
		UpdateFile(&noteDataInUI, CANCEL_NOTE, FALSE);
	}
	ResetIfNotSticky();
	wHide(linkEditW);
}

/**
 * Handle OK button: make sure the entered URL is syntactically valid, update
 * the layout and close the dialog
 *
 * \param junk
 */

static void
LinkEditOK(void *junk)
{
    UpdateLink(&noteDataInUI, OK_LINK, FALSE);
    wHide(linkEditW);
	ResetIfNotSticky();
	FileIsChanged();
}


static void 
CreateEditLinkDialog(track_p trk, char *title)
{
    struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);

	// create the dialog if necessary
    if (!linkEditW) {
		noteDataInUI.noteData.linkData.url = MyMalloc(URLMAXIMUMLENGTH);
		noteDataInUI.noteData.linkData.title = MyMalloc(TITLEMAXIMUMLENGTH);
		linkEditPLs[I_TITLE].valueP = noteDataInUI.noteData.linkData.title;
		linkEditPLs[I_URL].valueP = noteDataInUI.noteData.linkData.url;
        ParamRegister(&linkEditPG);
        linkEditW = ParamCreateDialog(&linkEditPG,
                                      "",
                                      _("Done"), LinkEditOK,
                                      LinkEditCancel, TRUE, NULL,
                                      F_BLOCK,
                                      LinkDlgUpdate);
    }

    wWinSetTitle(linkEditPG.win, MakeWindowTitle(title));

	// initialize the dialog fields
    noteDataInUI.pos = xx->pos;
	noteDataInUI.layer = xx->layer;
    noteDataInUI.trk = trk;
	strscpy(noteDataInUI.noteData.linkData.url, xx->noteData.linkData.url,URLMAXIMUMLENGTH );
	strscpy(noteDataInUI.noteData.linkData.title, xx->noteData.linkData.title, TITLEMAXIMUMLENGTH );
	
	FillLayerList((wList_p)linkEditPLs[I_LAYER].control);
	ParamLoadControls(&linkEditPG);
        
	// and show the dialog
	wShow(linkEditW);
}

/**
 * Activate note if double clicked
 * \param trk the note
 */

void ActivateLinkNote(track_p trk)
{
    struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);
	NoteLinkOpen(xx->noteData.linkData.url);
}


/**
 * Describe and enable editing of an existing link note
 *
 * \param trk the existing, valid note
 * \param str the field to put a text version of the note so it will appear on the status line
 * \param len the lenght of the field
 */

void DescribeLinkNote(track_p trk, char * str, CSIZE_T len)
{
    struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);
    DynString statusLine;

    DynStringMalloc(&statusLine, 80);
    DynStringPrintf(&statusLine, 
					"Link: Layer=%d %-.80s (%s)", 
					GetTrkLayer(trk)+1,
					xx->noteData.linkData.title, 
					xx->noteData.linkData.url);
	strcpy(str, DynStringToCStr(&statusLine));
    DynStringFree(&statusLine);

	if (inDescribeCmd) {
		NoteStateSave(trk);

		CreateEditLinkDialog(trk, _("Update link"));
	}
}

/**
 * Take a new note track element and initialize it. It will be
 * initialized with defaults and can then be edited by the user.
 *
 * \param the newly created trk
 */

void NewLinkNoteUI(track_p trk)
{
	struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);

	xx->noteData.linkData.url = MyStrdup( DEFAULTLINKURL );
	xx->noteData.linkData.title = MyStrdup( DEFAULTLINKTITLE );

	CreateEditLinkDialog(trk, _("Create link"));
}
