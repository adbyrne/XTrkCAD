/** \file trknote.c
 * Track notes "postits"
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis, 2018 Martin Fischer
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

#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "cundo.h"
#include "custom.h"
#include "dynstring.h"
#include "fileio.h"
#include "i18n.h"
#include "misc.h"
#include "note.h"
#include "param.h"
#include "track.h"
#include "utility.h"

extern BOOL_T inDescribeCmd;
extern descData_t noteDesc[];

static TRKTYP_T T_NOTE = -1;

static wDrawBitMap_p note_bm, link_bm, document_bm;

typedef struct {
	char **xpm;
	int OP;
	char * shortName;
	char * cmdName;
	char * helpKey;
	long acclKey;
} trknoteData_t;

#include "bitmaps/sticky-note-text.xpm"
#include "bitmaps/sticky-note-chain.xpm"
#include "bitmaps/sticky-note-clip.xpm"

static trknoteData_t noteTypes[] = {
	{ sticky_note_text_bits, OP_NOTETEXT, N_("Note"), N_("Comment"), "cmdTextNote", 0L },
	{ sticky_note_chain_bits, OP_NOTELINK, N_("Link"), N_("Weblink"), "cmdLinkNote", 0L },
	{ sticky_note_clip_bits, OP_NOTEFILE, N_("Document"), N_("Document"), "cmdFileNote", 0L },
};

static long curNoteType;

static unsigned layerSave;
static 	coOrd posSave;

#define NOTETYPESCOUNT (sizeof(noteTypes)/sizeof(trknoteData_t))


/*****************************************************************************
 * NOTE OBJECT
 */

static track_p NewNote(wIndex_t index, coOrd p, enum noteCommands command )
{
    track_p t;
    struct extraDataNote * xx;
    t = NewTrack(index, T_NOTE, 0, sizeof *xx);
    xx = (struct extraDataNote *)GetTrkExtraData(t);
    xx->pos = p;
	xx->op = command;
    SetBoundingBox(t, p, p);
    return t;
}

/**
 * Draw the icon for a note into the drawing area
 *
 * \param t IN note
 * \param d IN drawing environment
 * \param color IN color for ico
 */

static void DrawNote(track_p t, drawCmd_p d, wDrawColor color)
{
    struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(t);
    coOrd p[4];

    if (d->scale >= 16) {
        return;
    }
	if ((d->funcs->options & wDrawOptTemp)) {
		//while the icon is moved, draw a square
		DIST_T dist;
		dist = 0.1*d->scale;
		p[0].x = p[1].x = xx->pos.x - dist;
		p[2].x = p[3].x = xx->pos.x + dist;
		p[1].y = p[2].y = xx->pos.y - dist;
		p[3].y = p[0].y = xx->pos.y + dist;
		DrawLine(d, p[0], p[1], 0, color);
		DrawLine(d, p[1], p[2], 0, color);
		DrawLine(d, p[2], p[3], 0, color);
		DrawLine(d, p[3], p[0], 0, color);
	} else {
		// draw a bitmap for static object
		wDrawBitMap_p bm;

		if (xx->op == OP_NOTELINK ||(inDescribeCmd && curNoteType == OP_NOTELINK)) {
			bm = link_bm;
		} else {
			if (xx->op == OP_NOTEFILE || (inDescribeCmd && curNoteType == OP_NOTEFILE)) {
				bm = document_bm;
			} else {
				bm = note_bm;
			}
		}
    	DrawBitMap(d, xx->pos, bm, color);
    }
}

static DIST_T DistanceNote(track_p t, coOrd * p)
{
    struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(t);
    DIST_T d;
    d = FindDistance(*p, xx->pos);

    if (d < 3.0*(mainD.scale/12.0)) {
        return d;
    }

    return 100000.0;
}

static void DeleteNote(track_p t)
{
	struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(t);

	switch (xx->op) {
	case OP_NOTETEXT:
		if (xx->noteData.text) {
			MyFree(xx->noteData.text);
		}
		break;
	case OP_NOTEFILE:
		if (xx->noteData.fileData.path) {
			MyFree(xx->noteData.fileData.path);
		}
		if (xx->noteData.fileData.title) {
			MyFree(xx->noteData.fileData.title);
		}
		break;
	case OP_NOTELINK:
		if (xx->noteData.linkData.title) {
			MyFree(xx->noteData.linkData.title);
		}
		if (xx->noteData.linkData.url) {
			MyFree(xx->noteData.linkData.url);
		}
		break;
	default:
		break;
	}
}

void
NoteStateSave(track_p trk)
{
	struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);
	layerSave = GetTrkLayer(trk);
	posSave = xx->pos;
}

/**
* Handle Cancel button: restore old values for layer and position
*/

void
CommonCancelNote(track_p trk)
{
	if (inDescribeCmd) {
		struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);
		xx->layer = layerSave;
		xx->pos = posSave;
		SetBoundingBox(trk, xx->pos, xx->pos);
	}
}

static void 
CommonUpdateNote(track_p trk, int inx, struct extraDataNote *noteData )
{
	struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);

	switch (inx) {
	case OR_NOTE:
		xx->pos = noteData->pos;
		SetBoundingBox(trk, xx->pos, xx->pos);
		break;
	case LY_NOTE:
		SetTrkLayer(trk, noteData->layer);
		break;
	case CANCEL_NOTE:
		CommonCancelNote(trk);
		break;
	}
}


void UpdateFile(struct extraDataNote *noteUIData, int inx,  BOOL_T needUndoStart)
{
	track_p trk = noteUIData->trk;
	struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);
	
	switch (inx) {
	case OR_NOTE:
	case LY_NOTE:
	case CANCEL_NOTE:
		CommonUpdateNote(trk, inx, noteUIData);
		break;
	case OK_FILE:
	{
		DeleteNote(trk);
		xx->noteData.fileData.path = MyStrdup(noteUIData->noteData.fileData.path);
		xx->noteData.fileData.title = MyStrdup(noteUIData->noteData.fileData.title);
		//result = malloc( maximumSize );
		//resultSize = File2URI(noteFileData->path, maximumSize, result);
		//xx->text = (char*)MyMalloc(resultSize + strlen(noteFileData->title) + 2);
		//sprintf(xx->text, "%s %s", result, noteFileData->title);
		//if (noteFileData->inArchive) {
		//	CopyFile(noteFileData->path, archiveDirectory);

		//}
		//free(result);
	}
		break;
	
	default:
		break;
	}
}

void UpdateLink(struct extraDataNote *noteUIData, int inx, BOOL_T needUndoStart)
{
	track_p trk = noteUIData->trk;
	struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);

	switch (inx) {
	case OR_NOTE:
	case LY_NOTE:
	case CANCEL_NOTE:
		CommonUpdateNote(trk, inx, noteUIData);
		break;

	case OK_LINK:
		DeleteNote(trk);
		xx->noteData.linkData.title = MyStrdup(noteUIData->noteData.linkData.title);
		xx->noteData.linkData.url = MyStrdup(noteUIData->noteData.linkData.url);
		break;
	default:
		break;
	}
}

void UpdateText(struct extraDataNote *noteUIData, int inx, BOOL_T needUndoStart)
{
	track_p trk = noteUIData->trk;
	struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);

	switch (inx) {
	case OR_NOTE:
	case LY_NOTE:
	case CANCEL_NOTE:
		CommonUpdateNote(trk, inx, noteUIData);
		break;

	case OK_TEXT:
		DeleteNote(trk);
		xx->noteData.text = MyStrdup(noteUIData->noteData.text);
		break;
	default:
		break;
	}
	changed = TRUE;
}

/**
 * Get the delimited marker for the current note. Markers start and end with
 * a delimiter. The marker itself is a single digit number. For plain text notes
 * no marker is used for backwards compatibility
 *
 * \param command IN the note's command code
 * \return a pointer to the marker string. 
 */

static char *
GetNoteMarker(enum noteCommands command )
{
	static char marker[2 * sizeof(DELIMITER) + 3];

	switch (command) {
	case OP_NOTEFILE:
	case OP_NOTELINK:
		sprintf(marker, DELIMITER "%d" DELIMITER, command);
		break;
	case OP_NOTETEXT:
	default:
		*marker = '\0';
		break;
	}
	return(marker);
}

/**
 * Write the note to file. Handles the complete syntax for a note statement
 * 
 * \param t IN pointer to the note track element
 * \param f IN file handle for writing
 * \return TRUE for success
 */
  
static BOOL_T WriteNote(track_p t, FILE * f)
{
    struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(t);
    BOOL_T rc = TRUE;
	DynString noteText;
	char *marker;

	DynStringMalloc(&noteText, 16);
	marker = GetNoteMarker(xx->op);

	switch (xx->op) {
	case OP_NOTETEXT:
		DynStringPrintf(&noteText, "%s", xx->noteData.text);
		break;
	case OP_NOTELINK:
		DynStringPrintf(&noteText, "%s %s", xx->noteData.linkData.url, xx->noteData.linkData.title);
		break;
	case OP_NOTEFILE:
		DynStringPrintf(&noteText, "\"%s\" %s", xx->noteData.fileData.path, xx->noteData.fileData.title);
		break;
	default:
		break;
	}

	rc &= fprintf(f, "NOTE %d %u 0 0 %0.6f %0.6f 0 %d\n", GetTrkIndex(t),
		GetTrkLayer(t),
		xx->pos.x, xx->pos.y, (int)(DynStringSize( &noteText ) + strlen(marker))) > 0;

	if (*marker) {
		rc &= fprintf(f, "%s", marker) > 0;
	}

	rc &= fprintf(f, "%s", DynStringToCStr(&noteText)) > 0;
    rc &= fprintf(f, "\n    END\n") > 0;
    DynStringFree(&noteText);
	
	return rc;
}

/**
 * Read a track note aka postit
 *
 * \param line
 */

static void
ReadTrackNote(char *line)
{
    track_p t;
    int size;
    char * cp;
    struct extraDataNote *xx;
    wIndex_t index;
    wIndex_t layer;
    coOrd pos;
    DIST_T elev;
	char *noteText;
	enum noteCommands noteType;

    if (!GetArgs(line + 5, paramVersion < 3 ? "XXpYd" : paramVersion < 9 ?
                 "dL00pYd" : "dL00pfd",
                 &index, &layer, &pos, &elev, &size)) {
        return;
    }

	noteText = MyMalloc(size + 1);

	fread(noteText, sizeof(char), size, paramFile);
	noteType = OP_NOTETEXT;

	if( !strncmp(noteText, DELIMITER, strlen( DELIMITER )) && 
		!strncmp(noteText + strlen(DELIMITER) + 1, DELIMITER, strlen(DELIMITER)) &&
			noteText[strlen(DELIMITER)] - '0' > 0 &&
			noteText[strlen(DELIMITER)] - '0' <= OP_NOTEFILE)
	{
		noteType = noteText[strlen(DELIMITER)] - '0';
	}

    t = NewNote(index, pos, noteType);
    SetTrkLayer(t, layer);
	   
    xx = (struct extraDataNote *)GetTrkExtraData(t);

	switch (noteType) {
	case OP_NOTETEXT:
		xx->noteData.text = MyStrdup(noteText);
		break;
	case OP_NOTELINK:
	{
		char *ptr;
		ptr = strtok(noteText, " ");
		xx->noteData.linkData.url = MyStrdup(ptr + 2 * strlen(DELIMITER) + 1);
		xx->noteData.linkData.title = MyStrdup(noteText + strlen(ptr) + 1);
		break;
	}
	case OP_NOTEFILE:
	{
		char *ptr;
		ptr = strtok(noteText + 2 * strlen(DELIMITER) + 1, "\"");
		xx->noteData.fileData.path = MyStrdup(ptr);
		xx->noteData.fileData.title = MyStrdup(ptr + strlen(ptr) + 2 );
		xx->noteData.fileData.inArchive = FALSE;
		break;
	}
	}
	
	fgetc(paramFile);
	
	cp = GetNextLine();
	
	while ( *cp && isspace(*cp ) ) cp++;
	if (strncmp(cp, "END", 3)) {
		InputError(_("Expected END statement not found!"),
				TRUE );
		exit(1);
	}
    MyFree(noteText);
}

/**
 * Handle reading of NOTE
 *
 * \param line IN complete line with NOTE statement
 */

static void
ReadNote(char * line)
{
    if (strncmp(line, "NOTE MAIN", 9) == 0) {
        ReadMainNote(line);
    } else {
        ReadTrackNote(line);
    }
}

static void MoveNote(track_p trk, coOrd orig)
{
    struct extraDataNote * xx = (struct extraDataNote *)GetTrkExtraData(trk);
    xx->pos.x += orig.x;
    xx->pos.y += orig.y;
    SetBoundingBox(trk, xx->pos, xx->pos);
}


static void RotateNote(track_p trk, coOrd orig, ANGLE_T angle)
{
    struct extraDataNote * xx = (struct extraDataNote *)GetTrkExtraData(trk);
    Rotate(&xx->pos, orig, angle);
    SetBoundingBox(trk, xx->pos, xx->pos);
}

static void RescaleNote(track_p trk, FLOAT_T ratio)
{
    struct extraDataNote * xx = (struct extraDataNote *)GetTrkExtraData(trk);
    xx->pos.x *= ratio;
    xx->pos.y *= ratio;
}

static void DescribeNote(track_p trk, char * str, CSIZE_T len)
{
	if (IsLinkNote(trk)) {
		DescribeLinkNote(trk, str, len);
	}
	else {
		if (IsFileNote(trk)) {
			DescribeFileNote(trk, str, len);
		} else {
			DescribeTextNote(trk, str, len);
		}
	}
}

static void ActivateNote(track_p trk) {
	if (IsLinkNote(trk) ) {
		ActivateLinkNote(trk);
	}
	if (IsFileNote(trk)) {
		ActivateFileNote(trk);
	}
}

static BOOL_T QueryNote( track_p trk, int query )
{
	switch ( query ) {
	case Q_IS_ACTIVATEABLE:;
		if (IsFileNote(trk)) return TRUE;
		if (IsLinkNote(trk)) return TRUE;
		break;
	default:
		return FALSE;
	}
	return FALSE;
}

static trackCmd_t noteCmds = {
    "NOTE",
    DrawNote,
    DistanceNote,
    DescribeNote,
    DeleteNote,
    WriteNote,
    ReadNote,
    MoveNote,
    RotateNote,
    RescaleNote,
    NULL,		/* audit */
    NULL,		/* getAngle */
    NULL,		/* split */
    NULL,		/* traverse */
    NULL,		/* enumerate */
    NULL,		/* redraw */
	NULL,       /*trim*/
	NULL,       /*merge*/
	NULL,       /*modify*/
	NULL,       /*getLength*/
	NULL,       /*getTrackParams*/
	NULL,       /*moveEndPt*/
	QueryNote,  /*query*/
	NULL,       /*ungroup*/
	NULL,       /*flip*/
	NULL,       /*drawPositionIndicator*/
	NULL,       /*advancePositionIndicator*/
	NULL,       /*checkTraverse*/
	NULL,	    /*makeParallel*/
	NULL,       /*drawDesc*/
	NULL,       /*rebuildSegs*/
	NULL,       /*replayData*/
	NULL,       /*storeData*/
	ActivateNote
};

/*****************************************************************************
 * NOTE COMMAND
 */



static STATUS_T CmdNote(wAction_t action, coOrd pos)
{
    static coOrd oldPos;
    static int state_on = FALSE;
    track_p trk;

    switch (action) {
    case C_START:
        InfoMessage(_("Place a note on the layout"));
		curNoteType = (long)commandContext;
        return C_CONTINUE;

    case C_DOWN:
        state_on = TRUE;
        oldPos = pos;
        return C_CONTINUE;

    case C_MOVE:
        oldPos = pos;
        return C_CONTINUE;

    case C_UP:
        UndoStart(_("New Note"), "New Note");
        state_on = FALSE;
        trk = NewNote(-1, pos, curNoteType );
		inDescribeCmd = TRUE;
        DrawNewTrack(trk);

		switch (curNoteType)
		{
		case OP_NOTETEXT:
			NewTextNoteUI(trk);
			break;
		case OP_NOTELINK:
			NewLinkNoteUI(trk);
			break;
		case OP_NOTEFILE:
			NewFileNoteUI(trk);
			break;
		}

		inDescribeCmd = FALSE;

		return C_CONTINUE;

    case C_REDRAW:
    	if (state_on) {
			switch (curNoteType) {
			case OP_NOTETEXT:
				DrawBitMap(&tempD, oldPos, note_bm, normalColor);
				break;
			case OP_NOTELINK:
				DrawBitMap(&tempD, oldPos, link_bm, normalColor);
				break;
			}
    	}
        return C_CONTINUE;

    case C_CANCEL:
        DescribeCancel();
        state_on = FALSE;
        return C_CONTINUE;
    }

    return C_INFO;
}

#include "bitmaps/note.xbm"
#include "bitmaps/link.xbm"
#include "bitmaps/clip.xbm"
#include "bitmaps/cnote.xpm"

void InitTrkNote(wMenu_p menu)
{
    note_bm = wDrawBitMapCreate(mainD.d, note_width, note_width, 8, 8, note_bits);
    link_bm = wDrawBitMapCreate(mainD.d, note_width, note_width, 8, 8, link_bits);
	document_bm = wDrawBitMapCreate(mainD.d, note_width, note_width, 8, 8, clip_bits);

	ButtonGroupBegin(_("Notes"), "cmdNoteCmd", _("Add notes"));
	for (int i = 0; i < NOTETYPESCOUNT; i++) {
		trknoteData_t *nt;
		wIcon_p icon;

		nt = noteTypes + i;
		icon = wIconCreatePixMap(nt->xpm);
		AddMenuButton(menu, CmdNote, nt->helpKey, _(nt->cmdName), icon, LEVEL0_50, IC_STICKY | IC_POPUP2, nt->acclKey, (void *)(intptr_t)nt->OP);
	}
	ButtonGroupEnd();

	T_NOTE = InitObject(&noteCmds);
}
