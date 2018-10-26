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

#include <string.h>

#include "cundo.h"
#include "custom.h"
#include "fileio.h"
#include "i18n.h"
#include "misc.h"
#include "param.h"
#include "track.h"
#include "utility.h"

extern BOOL_T inDescribeCmd;

static TRKTYP_T T_NOTE = -1;

static wDrawBitMap_p note_bm;
struct extraData {
    coOrd pos;
    char * text;
};

/*****************************************************************************
 * NOTE OBJECT
 */

static track_p NewNote(wIndex_t index, coOrd p, long size)
{
    track_p t;
    struct extraData * xx;
    t = NewTrack(index, T_NOTE, 0, sizeof *xx);
    xx = GetTrkExtraData(t);
    xx->pos = p;
    xx->text = (char*)MyMalloc((int)size + 2);
    SetBoundingBox(t, p, p);
    return t;
}

static void DrawNote(track_p t, drawCmd_p d, wDrawColor color)
{
    struct extraData *xx = GetTrkExtraData(t);
    coOrd p[4];

    if (d->scale >= 16) {
        return;
    }

    if ((d->funcs->options & wDrawOptTemp) == 0) {
        DrawBitMap(d, xx->pos, note_bm, color);
    } else {
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
    }
}

static DIST_T DistanceNote(track_p t, coOrd * p)
{
    struct extraData *xx = GetTrkExtraData(t);
    DIST_T d;
    d = FindDistance(*p, xx->pos);

    if (d < 1.0) {
        return d;
    }

    return 100000.0;
}


static struct {
    coOrd pos;
    unsigned int layer;
} noteData;
typedef enum { OR, LY, TX } noteDesc_e;
static descData_t noteDesc[] = {
    /*OR*/	{ DESC_POS, N_("Position"), &noteData.pos },
    /*LY*/	{ DESC_LAYER, N_("Layer"), &noteData.layer },
    /*TX*/	{ DESC_TEXT, NULL, NULL },
    { DESC_NULL }
};

static void UpdateNote(track_p trk, int inx, descData_p descUpd,
                       BOOL_T needUndoStart)
{
    struct extraData *xx = GetTrkExtraData(trk);

    switch (inx) {
    case OR:

        xx->pos = noteData.pos;
        SetBoundingBox(trk, xx->pos, xx->pos);
        MainRedraw();
        break;

    case LY:
        SetTrkLayer(trk, noteData.layer);
        MainRedraw();
        break;

    case -1:
        if (wTextGetModified((wText_p)noteDesc[TX].control0)) {
            int len;

            if (needUndoStart) {
                UndoStart(_("Change Track"), "Change Track");
            }

            UndoModify(trk);
            MyFree(xx->text);
            len = wTextGetSize((wText_p)noteDesc[TX].control0);
            xx->text = (char*)MyMalloc(len + 2);
            wTextGetText((wText_p)noteDesc[TX].control0, xx->text, len);

            if (xx->text[len - 1] != '\n') {
                xx->text[len++] = '\n';
            }

            xx->text[len] = '\0';
        }
        MainRedraw();
        break;

    default:
        break;
    }
}


static void DescribeNote(track_p trk, char * str, CSIZE_T len)
{
    struct extraData * xx = GetTrkExtraData(trk);
    strcpy(str, _("Note: "));
    len -= strlen(_("Note: "));
    str += strlen(_("Note: "));
    strncpy(str, xx->text, len);

    for (; *str; str++) {
        if (*str == '\n') {
            *str = ' ';
        }
    }

    noteData.pos = xx->pos;
    noteDesc[TX].valueP = xx->text;
    noteDesc[OR].mode = 0;
    noteDesc[TX].mode = 0;
    noteDesc[LY].mode = DESC_NOREDRAW;
    DoDescribe(_("Note"), trk, noteDesc, UpdateNote);
}

static void DeleteNote(track_p t)
{
    struct extraData *xx = GetTrkExtraData(t);

    if (xx->text) {
        MyFree(xx->text);
    }
}

static BOOL_T WriteNote(track_p t, FILE * f)
{
    struct extraData *xx = GetTrkExtraData(t);
    int len;
    BOOL_T addNL = FALSE;
    BOOL_T rc = TRUE;
    len = strlen(xx->text);

    if (xx->text[len - 1] != '\n') {
        len++;
        addNL = TRUE;
    }

    rc &= fprintf(f, "NOTE %d %u 0 0 %0.6f %0.6f 0 %d\n", GetTrkIndex(t),
                  GetTrkLayer(t),
                  xx->pos.x, xx->pos.y, len) > 0;
    rc &= fprintf(f, "%s%s", xx->text, addNL ? "\n" : "") > 0;
    rc &= fprintf(f, "    END\n") > 0;
    return rc;
}

/**
 * Read a track note aka postit
 * 
 * \param line
 */
 * 
static void
ReadTrackNote(char *line)
{
    track_p t;
    size_t size;
    char * cp;
    struct extraData *xx;
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

    xx = GetTrkExtraData(t);
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
    struct extraData * xx = GetTrkExtraData(trk);
    xx->pos.x += orig.x;
    xx->pos.y += orig.y;
    SetBoundingBox(trk, xx->pos, xx->pos);
}


static void RotateNote(track_p trk, coOrd orig, ANGLE_T angle)
{
    struct extraData * xx = GetTrkExtraData(trk);
    Rotate(&xx->pos, orig, angle);
    SetBoundingBox(trk, xx->pos, xx->pos);
}

static void RescaleNote(track_p trk, FLOAT_T ratio)
{
    struct extraData * xx = GetTrkExtraData(trk);
    xx->pos.x *= ratio;
    xx->pos.y *= ratio;
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
    NULL		/* redraw */
};

/*****************************************************************************
 * NOTE COMMAND
 */



static STATUS_T CmdNote(wAction_t action, coOrd pos)
{
    static coOrd oldPos;
    static int state_on = FALSE;
    track_p trk;
    struct extraData * xx;
    const char* tmpPtrText;

    switch (action) {
    case C_START:
        InfoMessage(_("Place a note on the layout"));
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
        xx = GetTrkExtraData(trk);
        tmpPtrText = _("Replace this text with your note");
        xx->text = (char*)MyMalloc(strlen(tmpPtrText) + 1);
        strcpy(xx->text, tmpPtrText);
        inDescribeCmd = TRUE;
        DescribeNote(trk, message, sizeof message);
        inDescribeCmd = FALSE;
        return C_CONTINUE;

    case C_REDRAW:
        if (state_on) {
            DrawBitMap(&tempD, oldPos, note_bm, normalColor);
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
#include "bitmaps/cnote.xpm"

void InitTrkNote(wMenu_p menu)
{
    note_bm = wDrawBitMapCreate(mainD.d, note_width, note_width, 8, 8, note_bits);
    AddMenuButton(menu, CmdNote, "cmdNote", _("Note"), wIconCreatePixMap(cnote_xpm),
                  LEVEL0_50, IC_POPUP2, ACCL_NOTE, NULL);
    T_NOTE = InitObject(&noteCmds);
}