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
#include "track.h"
#include "wlib.h"

extern BOOL_T inDescribeCmd;

#define MINURLLENGTH 5 /* 2 chars domain name, dot, 2 chars TLD */

static char *validProtocols[] = { "http://", "https://" };

static struct noteLinkData noteLinkData;

static void NoteLinkOpen(void * junk);

static paramFloatRange_t r_1000_1000 = { -1000.0, 1000.0, 80 };
static paramData_t linkEditPLs[] = {
#define I_ORIGX (0)
	/*0*/ { PD_FLOAT, &noteLinkData.pos.x, "origx", PDO_DIM, &r_1000_1000, N_("Position X") },
#define I_ORIGY (1)
	/*1*/ { PD_FLOAT, &noteLinkData.pos.y, "origy", PDO_DIM, &r_1000_1000, N_("Position Y") },
#define I_LAYER (2)
	/*2*/ { PD_DROPLIST, &noteLinkData.layer, "layer", 0, (void*)150, "Layer", 0 },
#define I_URL (3)
	/*3*/ { PD_STRING, noteLinkData.url, "name", PDO_NOPREF | PDO_STRINGLIMITLENGTH, (void*)200, N_("URL"), 0, 0, sizeof(noteLinkData.url) },
#define I_OPEN (4)
	/*4*/ { PD_BUTTON, (void*)NoteLinkOpen, "openlink", PDO_DLGHORZ, NULL, N_("Open...") },
};

static paramGroup_t linkEditPG = { "linkEdit", 0, linkEditPLs, sizeof linkEditPLs / sizeof linkEditPLs[0] };
static wWin_p linkEditW;

/**
 * 
 */
struct noteLinkData *
GetNoteLinkData(void)
{
	return(&noteLinkData);
}


bool 
IsLinkNote(track_p trk)
{
	struct extraDataNote * xx = (struct extraDataNote *)GetTrkExtraData(trk);

	for (int i = 0; i < sizeof(validProtocols) / sizeof(char *); i++) {
		if (!strncmp(validProtocols[i], xx->text, strlen(validProtocols[i]))) {
			return(TRUE);
		}
	}
	return(FALSE);
}

static void 
CompleteURL(char *text, size_t length )
{
	DynString url;

	DynStringMalloc(&url, strlen(text));
	for (int i = 0; i < sizeof(validProtocols) / sizeof(char *); i++) {
		if (!strncmp(validProtocols[i], noteLinkData.url, strlen(validProtocols[i]))) {
			DynStringCatCStr(&url, noteLinkData.url);
			break;
		}
	}

	if (!DynStringSize(&url)) {
		DynStringCatCStrs(&url, validProtocols[0], noteLinkData.url, NULL);
	}

	// return the URL
	if (length > DynStringSize(&url)) {
		strcpy(text, DynStringToCStr(&url));
	}
	DynStringFree(&url);
	return;
}

/**
 * Simplistic checking of URL syntax validililty
 * 
 * \param testString
 * \return 
 */
static bool
IsValidURL(char *testString)
{
	if (strlen(testString) < MINURLLENGTH)	// hava minimum length
		return(FALSE);
	if (!strchr(testString, '.'))	// needs at least one dot, the delimiter between domain name and TLD	
		return(FALSE);

	return(TRUE);
}


static void NoteLinkOpen(void * junk)
{
	CompleteURL(noteLinkData.url, URLMAXIMUMLENGTH);
	ParamLoadControl(&linkEditPG, I_URL);
	wOpenFileExternal(noteLinkData.url);
}

static void 
LinkDlgUpdate(
	paramGroup_p pg,
	int inx,
	void * valueP)
{
	switch (inx) {
	case I_URL:
		if (IsValidURL(noteLinkData.url)) {
			wControlActive(linkEditPLs[I_OPEN].control, TRUE);
			ParamDialogOkActive(&linkEditPG, TRUE);
		}
		else {
			wControlActive(linkEditPLs[I_OPEN].control, FALSE);
			ParamDialogOkActive(&linkEditPG, FALSE);
		}
		break;
	default:
		UpdateLink(noteLinkData.trk, inx, NULL, FALSE);
	}
}

/*!
 * Handle OK button: make sure the entered URL is syntactically valid, update
 * the layout and close the dialog
 * 
 * \param junk
 */
 
static void
LinkEditOK(void *junk)
{
	CompleteURL(noteLinkData.url, URLMAXIMUMLENGTH);
	UpdateLink(noteLinkData.trk, OK_LINK, NULL, FALSE);
	wHide(linkEditW);
}



void CreateEditLinkDialog(track_p trk, char *title, char *defaultURL)
{
	struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);

	if (!inDescribeCmd) {
		return;
	}

	if (!linkEditW) {
		ParamRegister(&linkEditPG);
		linkEditW = ParamCreateDialog(&linkEditPG,
			"",
			_("Done"), LinkEditOK,
			NULL, TRUE, NULL,
			F_BLOCK,
			LinkDlgUpdate);
	}

	wWinSetTitle(linkEditPG.win, MakeWindowTitle(title));
	noteLinkData.pos = xx->pos;
	noteLinkData.trk = trk;
	strcpy(noteLinkData.url, defaultURL);
	ParamLoadControls(&linkEditPG);
	FillLayerList((wList_p)linkEditPLs[I_LAYER].control);
	wShow(linkEditW);
}

/**
 * Activate note if double clicked
 * \param trk the note
 */

void ActivateLinkNote(track_p trk) {

	struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);
	strlcpy(noteLinkData.url, xx->text, sizeof noteLinkData.url);
	NoteLinkOpen(NULL);

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
	strlcpy(str,xx->text,len);  /* Set info line */
	CreateEditLinkDialog( trk, _("Describe link"), xx->text);
}

/**
 * Take a new note track element and initialize it. It will be 
 * initialized with defaults and can then be edited by the user.  
 * 
 * \param the newly created trk
 */

void NewLinkNoteUI(track_p trk)
{
	CreateEditLinkDialog( trk, _("Create link"), "http://www.xtrkcad.org/");
}
