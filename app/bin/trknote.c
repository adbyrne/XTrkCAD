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

#include "cundo.h"
#include "custom.h"
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

static wDrawBitMap_p note_bm, link_bm;

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

enum noteCommands {
	OP_NOTETEXT,
	OP_NOTELINK
} noteOperations;

static trknoteData_t noteTypes[] = {
	{ sticky_note_text_bits, OP_NOTETEXT, N_("Note"), N_("Text"), "cmdTextNote", 0L },
	{ sticky_note_chain_bits, OP_NOTELINK, N_("Link"), N_("Weblink"), "cmdLinkNote", 0L },
};

static long curNoteType;

#define NOTETYPESCOUNT (sizeof(noteTypes)/sizeof(trknoteData_t))

/*****************************************************************************
 * NOTE OBJECT
 */

static track_p NewNote(wIndex_t index, coOrd p, long size)
{
    track_p t;
    struct extraDataNote * xx;
    t = NewTrack(index, T_NOTE, 0, sizeof *xx);
    xx = (struct extraDataNote *)GetTrkExtraData(t);
    xx->pos = p;
    xx->text = (char*)MyMalloc((int)size + 2);
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

		if (IsLinkNote(t)) {
			bm = link_bm;
		} else {
			bm = note_bm;
		}
    	DrawBitMap(d, xx->pos, bm, color);
    }
}

static DIST_T DistanceNote(track_p t, coOrd * p)
{
    struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(t);
    DIST_T d;
    d = FindDistance(*p, xx->pos);

    if (d < 1.0) {
        return d;
    }

    return 100000.0;
}


void UpdateNote(track_p trk, int inx, descData_p descUpd,
	BOOL_T needUndoStart)
{
	struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);
	struct noteTextData *noteData = GetNoteTextData();
	size_t len;

	switch (inx) {
	case OR_TEXT:

		xx->pos = noteData->pos;
		SetBoundingBox(trk, xx->pos, xx->pos);
		MainRedraw();
		break;

	case LY_TEXT:
		SetTrkLayer(trk, noteData->layer);
		MainRedraw();
		break;

	case TX_TEXT:
		if (needUndoStart) {
			UndoStart(_("Change Track"), "Change Track");
		}

		UndoModify(trk);
		MyFree(xx->text);
		len = strlen(noteData->text);
		xx->text = (char*)MyMalloc(len + 2);
		strcpy( xx->text, noteData->text );
		MainRedraw();
		break;
	case -1:
		if (wTextGetModified((wText_p)noteDesc[TX_TEXT].control0)) {
			int len;

			if (needUndoStart) {
				UndoStart(_("Change Track"), "Change Track");
			}

			UndoModify(trk);
			MyFree(xx->text);
			len = wTextGetSize((wText_p)noteDesc[TX_TEXT].control0);
			xx->text = (char*)MyMalloc(len + 2);
			wTextGetText((wText_p)noteDesc[TX_TEXT].control0, xx->text, len);

			if (xx->text[len - 1] != '\n') {
				xx->text[len++] = '\n';
			}

			xx->text[len] = '\0';
		}
		MainRedraw();
	default:
		break;
	}
}

void UpdateLink(track_p trk, int inx, descData_p descUpd,
	BOOL_T needUndoStart)
{
	struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(trk);
	struct noteLinkData *noteLinkData = GetNoteLinkData();
	int len = strlen(noteLinkData->url);

	switch (inx) {
	case OR_LINK:

		xx->pos = noteLinkData->pos;
		SetBoundingBox(trk, xx->pos, xx->pos);
		MainRedraw();
		break;

	case LY_LINK:
		SetTrkLayer(trk, noteLinkData->layer);
		MainRedraw();
		break;

	case OK_LINK:
		if (xx->text) {
			MyFree(xx->text);
		}
		xx->text = (char*)MyMalloc(strlen(noteLinkData->url) + strlen(noteLinkData->title) + 2);
		sprintf(xx->text, "%s %s", noteLinkData->url, noteLinkData->title);
		break;
	default:
		break;
	}
}

static void DeleteNote(track_p t)
{
    struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(t);

    if (xx->text) {
        MyFree(xx->text);
    }
}

static BOOL_T WriteNote(track_p t, FILE * f)
{
    struct extraDataNote *xx = (struct extraDataNote *)GetTrkExtraData(t);
    int len = strlen(xx->text);
    BOOL_T rc = TRUE;

    rc &= fprintf(f, "NOTE %d %u 0 0 %0.6f %0.6f 0 %d\n", GetTrkIndex(t),
                  GetTrkLayer(t),
                  xx->pos.x, xx->pos.y, len) > 0;
    rc &= fprintf(f, "%s", xx->text ) > 0;
    rc &= fprintf(f, "\n    END\n") > 0;
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
    size_t size;
    char * cp;
    struct extraDataNote *xx;
    wIndex_t index;
    wIndex_t layer;
    coOrd pos;
    DIST_T elev;

    if (!GetArgs(line + 5, paramVersion < 3 ? "XXpYd" : paramVersion < 9 ?
                 "dL00pYd" : "dL00pfd",
                 &index, &layer, &pos, &elev, &size)) {
        return;
    }

    t = NewNote(index, pos, size + 2);
    SetTrkLayer(t, layer);

    cp = ReadMultilineText(size);

    xx = (struct extraDataNote *)GetTrkExtraData(t);
    strcpy(xx->text, cp);
    MyFree(cp);
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
		DescribeTextNote(trk, str, len);
	}
}

static void ActivateNote(track_p trk) {
	if (IsLinkNote(trk)) {
		ActivateLinkNote(trk);
	}
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
	NULL,       /*query*/
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

    const char* tmpPtrText;

    switch (action) {
    case C_START:
        InfoMessage(_("Place a note on the layout"));
		curNoteType = (long)commandContext;
        return C_CONTINUE;

    case C_DOWN:
        state_on = TRUE;
        oldPos = pos;
        MainRedraw();
        return C_CONTINUE;

    case C_MOVE:
        oldPos = pos;
        MainRedraw();
        return C_CONTINUE;

    case C_UP:
        UndoStart(_("New Note"), "New Note");
        state_on = FALSE;
        MainRedraw();
        trk = NewNote(-1, pos, 2);
        DrawNewTrack(trk);
		inDescribeCmd = TRUE;
		switch (curNoteType)
		{
		case OP_NOTETEXT:
			NewTextNoteUI(trk);
			break;
		case OP_NOTELINK:
			NewLinkNoteUI(trk);
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
        MainRedraw();
        return C_CONTINUE;
    }

    return C_INFO;
}

#include "bitmaps/note.xbm"
#include "bitmaps/link.xbm"
#include "bitmaps/cnote.xpm"

void InitTrkNote(wMenu_p menu)
{
    note_bm = wDrawBitMapCreate(mainD.d, note_width, note_width, 8, 8, note_bits);
    link_bm = wDrawBitMapCreate(mainD.d, note_width, note_width, 8, 8, link_bits);

	ButtonGroupBegin(_("Note"), "cmdNoteCmd", _("Select note command"));
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
