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
} linkNoteData;

static void NoteLinkBrowse(void *junk);
static void NoteLinkOpen(char *url );

static paramFloatRange_t noRangeCheck = { 0.0, 0.0, 80, PDO_NORANGECHECK_HIGH | PDO_NORANGECHECK_LOW };
static paramData_t linkNotePLs[] = {
#define I_ORIGX (0)
	/*0*/ { PD_FLOAT, &linkNoteData.pos.x, "origx", PDO_DIM|PDO_NOPREF, &noRangeCheck, N_("Position X") },
#define I_ORIGY (1)
	/*1*/ { PD_FLOAT, &linkNoteData.pos.y, "origy", PDO_DIM|PDO_NOPREF, &noRangeCheck, N_("Position Y") },
#define I_LAYER (2)
	/*2*/ { PD_DROPLIST, &linkNoteData.layer, "layer", PDO_NOPREF, I2VP(150), "Layer", 0 },
#define I_TITLE (3)
	/*3*/ { PD_STRING, &linkNoteData.title, "title", PDO_NOPREF | PDO_NOTBLANK, I2VP(200), N_("Title"), 0, 0, sizeof(linkNoteData.title ) },
#define I_URL (4)
	/*4*/ { PD_STRING, &linkNoteData.url, "name", PDO_NOPREF | PDO_NOTBLANK, I2VP(200), N_("URL"), 0, 0, sizeof(linkNoteData.url ) },
#define I_OPEN (5)
	/*5*/{ PD_BUTTON, NoteLinkBrowse, "openlink", PDO_DLGHORZ, NULL, N_("Open...") },
};

static paramGroup_t linkNotePG = { "linkNote", 0, linkNotePLs, COUNT( linkNotePLs ) };
static wWin_p linkNoteW;

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
		if ( ! IsValidURL( linkNoteData.url ) ) {
			printf( "URL %s is invalid\n", linkNoteData.url );
			paramData_p p = &linkNotePLs[I_URL];
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
	wHide(linkNoteW);
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
	wHide(linkNoteW);
	ResetIfNotSticky();
	SetFileChanged();
}


static void
CreateEditLinkDialog(char *title)
{

	// create the dialog if necessary
	if (!linkNoteW) {
		ParamRegister(&linkNotePG);
		linkNoteW = ParamCreateDialog(&linkNotePG,
		                              "",
		                              _("Done"), LinkEditOK,
		                              LinkEditCancel, TRUE, NULL,
		                              F_BLOCK,
		                              LinkDlgUpdate);
	}

	wWinSetTitle(linkNotePG.win, MakeWindowTitle(title));

	FillLayerList((wList_p)linkNotePLs[I_LAYER].control);
	ParamLoadControls(&linkNotePG);

	// and show the dialog
	wShow(linkNoteW);
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
	                "Weblink (%d) Layer=%d %-.80s (%s)",
	                GetTrkIndex(trk),
	                GetTrkLayer(trk)+1,
	                xx->noteData.linkData.title,
	                xx->noteData.linkData.url);
	strncpy(str, DynStringToCStr(&statusLine), len-1);
	str[len-1] = '\0';
	DynStringFree(&statusLine);
	if ( ! inDescribeCmd ) {
		return;
	}

	linkNoteData.pos = xx->pos;
	linkNoteData.layer = GetTrkLayer( trk );
	linkNoteData.trk = trk;
	strscpy( linkNoteData.url, xx->noteData.linkData.url, sizeof linkNoteData.url );
	strscpy( linkNoteData.title, xx->noteData.linkData.title,
	         sizeof linkNoteData.title );

	CreateEditLinkDialog(_("Update Webink"));
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

	CreateEditLinkDialog(_("Create Weblink"));
}
