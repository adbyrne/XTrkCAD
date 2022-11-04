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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "custom.h"
#include "dynstring.h"
#include "misc.h"
#include "note.h"
#include "param.h"
#include "include/stringxtc.h"
#include "track.h"
#include "validator.h"

#define DEFAULTLINKURL "http://www.xtrkcad.org/"
#define DEFAULTLINKTITLE "The XTrackCAD Homepage"
struct {
	coOrd pos;
	int layer;
	track_p trk;
	char title[TITLEMAXIMUMLENGTH];
	char url[URLMAXIMUMLENGTH];
	BOOL_T busy;
} linkNoteData;

static void NoteLinkBrowse(void *junk);
static void NoteLinkOpen(char *url );

static paramFloatRange_t r_1000_1000 = { -1000.0, 1000.0, 80 };
static paramData_t linkEditPLs[] = {
#define I_ORIGX (0)
    /*0*/ { PD_FLOAT, &linkNoteData.pos.x, "origx", PDO_DIM|PDO_NOPREF, &r_1000_1000, N_("Position X") },
#define I_ORIGY (1)
    /*1*/ { PD_FLOAT, &linkNoteData.pos.y, "origy", PDO_DIM|PDO_NOPREF, &r_1000_1000, N_("Position Y") },
#define I_LAYER (2)
    /*2*/ { PD_DROPLIST, &linkNoteData.layer, "layer", PDO_NOPREF, I2VP(150), "Layer", 0 },
#define I_TITLE (3)
    /*3*/ { PD_STRING, &linkNoteData.title, "title", PDO_NOPREF | PDO_NOTBLANK, I2VP(200), N_("Title"), 0, 0, sizeof(linkNoteData.title ) },
#define I_URL (4)
    /*4*/ { PD_STRING, &linkNoteData.url, "name", PDO_NOPREF | PDO_NOTBLANK, I2VP(200), N_("URL"), 0, 0, sizeof(linkNoteData.url ) },
#define I_OPEN (5)
	/*5*/{ PD_BUTTON, NoteLinkBrowse, "openlink", PDO_DLGHORZ, NULL, N_("Open...") },
};

static paramGroup_t linkEditPG = { "linkEdit", 0, linkEditPLs, COUNT( linkEditPLs ) };
static wWin_p linkEditW;

BOOL_T
IsLinkNote(track_p trk)
{
    struct extraDataNote_t * xx = GET_EXTRA_DATA( trk, T_NOTE, extraDataNote_t );

	return(xx->op == OP_NOTELINK);
}


/**
 * Callback for Open URL button
 *
 * \param junk IN ignored
 */
static void NoteLinkBrowse(void *junk)
{
	NoteLinkOpen(linkNoteData.url);
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
#ifdef LATER
		if (strlen(linkNoteData.url) > URLMAXIMUMLENGTH) {
			DynString message;

			DynStringMalloc(&message, 80);
			DynStringPrintf(&message, _("The entered URL is too long. The maximum allowed length is %d. Please edit the entered value."), URLMAXIMUMLENGTH);
			wNoticeEx(NT_ERROR,
				DynStringToCStr(&message),
				_("Re-edit"),
				NULL);
			DynStringFree(&message);
		}

        if (IsValidURL(linkNoteData.url) && 
			(strlen(linkNoteData.url) <= URLMAXIMUMLENGTH))
		{
            wControlActive(linkEditPLs[I_OPEN].control, TRUE);
            ParamDialogOkActive(&linkEditPG, TRUE);
        } else {
            wControlActive(linkEditPLs[I_OPEN].control, FALSE);
            ParamDialogOkActive(&linkEditPG, FALSE);
        }
#endif
		if ( ! IsValidURL( linkNoteData.url ) ) {
			printf( "URL %s is invalid\n", linkNoteData.url );
			paramData_p p = &linkEditPLs[I_URL];
			p->bInvalid = TRUE;
			wWinPix_t h = wControlGetHeight(p->control);
			wControlSetBalloon( p->control, 0, -h*3/4, "URL is invalid" );
			ParamHilite( p->group->win, p->control, TRUE );
		}
	        break;
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
LinkEditCancel( wWin_p junk)
{
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
	track_p trk = linkNoteData.trk;
	if ( trk == NULL ) {
		// new note 
		trk = NewNote( -1, linkNoteData.pos, OP_NOTELINK );
	}
	struct extraDataNote_t * xx = GET_EXTRA_DATA( trk, T_NOTE, extraDataNote_t );
	xx->pos = linkNoteData.pos;
	SetTrkLayer( trk, linkNoteData.layer );
	MyFree( xx->noteData.linkData.title );
	xx->noteData.linkData.title = MyStrdup( linkNoteData.title );
	MyFree( xx->noteData.linkData.url );
	xx->noteData.linkData.url = MyStrdup( linkNoteData.url );
	SetBoundingBox( trk, xx->pos, xx->pos );
	DrawNewTrack( trk );
	wHide(linkEditW);
	ResetIfNotSticky();
	SetFileChanged();
}


static void 
CreateEditLinkDialog(char *title)
{

	// create the dialog if necessary
    if (!linkEditW) {
        ParamRegister(&linkEditPG);
        linkEditW = ParamCreateDialog(&linkEditPG,
                                      "",
                                      _("Done"), LinkEditOK,
                                      LinkEditCancel, TRUE, NULL,
                                      F_BLOCK,
                                      LinkDlgUpdate);
    }

    wWinSetTitle(linkEditPG.win, MakeWindowTitle(title));

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
    struct extraDataNote_t *xx = GET_EXTRA_DATA( trk, T_NOTE, extraDataNote_t );
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
    struct extraDataNote_t *xx = GET_EXTRA_DATA( trk, T_NOTE, extraDataNote_t );
    DynString statusLine;

    DynStringMalloc(&statusLine, 80);
    DynStringPrintf(&statusLine, 
					"Link: Layer=%d %-.80s (%s)", 
					GetTrkLayer(trk)+1,
					xx->noteData.linkData.title, 
					xx->noteData.linkData.url);
	strncpy(str, DynStringToCStr(&statusLine), len-1);
	str[len-1] = '\0';
    DynStringFree(&statusLine);
	if ( ! inDescribeCmd )
		return;

	linkNoteData.pos = xx->pos;
	linkNoteData.layer = GetTrkLayer( trk );
	linkNoteData.trk = trk;
	strscpy( linkNoteData.url, xx->noteData.linkData.url, sizeof linkNoteData.url );
	strscpy( linkNoteData.title, xx->noteData.linkData.title, sizeof linkNoteData.title );

	CreateEditLinkDialog(_("Update link"));
}

/**
 * Take a new note track element and initialize it. It will be
 * initialized with defaults and can then be edited by the user.
 *
 * \param the newly created trk
 */

void NewLinkNoteUI( coOrd pos )
{
	linkNoteData.pos = pos;
	linkNoteData.layer = curLayer;
	linkNoteData.trk = NULL;
	strscpy( linkNoteData.url, DEFAULTLINKURL, sizeof( linkNoteData.url ) );
	strscpy( linkNoteData.title, DEFAULTLINKTITLE, sizeof( linkNoteData.title ) );

	CreateEditLinkDialog(_("Create link"));
}
