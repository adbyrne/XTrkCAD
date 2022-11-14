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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "common.h"
#include "cundo.h"
#include "custom.h"
#include "dynstring.h"
#include "fileio.h"
#include "misc.h"
#include "note.h"
#include "param.h"
#include "track.h"
#include "include/utf8convert.h"

EXPORT TRKTYP_T T_NOTE = -1;

static wDrawBitMap_p note_bm, link_bm, document_bm;

typedef struct {
	char ***xpm;
	int OP;
	char * shortName;
	char * cmdName;
	char * helpKey;
	long acclKey;
} trknoteData_t;

#include "bitmaps/sticky-note.xpm"
#include "bitmaps/sticky-link.xpm"
#include "bitmaps/sticky-doc.xpm"

static trknoteData_t noteTypes[] = {
	{ sticky_note_xpm, OP_NOTETEXT, N_("Note"), N_("Comment"), "cmdTextNote", 0L },
	{ sticky_link_xpm, OP_NOTELINK, N_("Link"), N_("Weblink"), "cmdLinkNote", 0L },
	{ sticky_doc_xpm, OP_NOTEFILE, N_("Document"), N_("Document"), "cmdFileNote", 0L },
};

static long curNoteType;

static unsigned layerSave;
static 	coOrd posSave;

#define NOTETYPESCOUNT COUNT(noteTypes)


/*****************************************************************************
 * NOTE OBJECT
 */

EXPORT track_p NewNote(wIndex_t index, coOrd p, enum noteCommands command )
{
    track_p t;
    struct extraDataNote_t * xx;
    t = NewTrack(index, T_NOTE, 0, sizeof *xx);
    xx = GET_EXTRA_DATA( t, T_NOTE, extraDataNote_t );
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
    struct extraDataNote_t *xx = GET_EXTRA_DATA( t, T_NOTE, extraDataNote_t );
    coOrd p[5];
    int type[5];


	if ((d->options & DC_SIMPLE) || mainD.scale >= 16) {
		//while the icon is moved, draw a square with a lopped off corner
		//because CmdMove draws all selected object into tempSeg and
		//tempSegDrawFuncs doesn't have a BitMap drawing func
		DIST_T dist;
		dist = 0.8 + 0.1*(mainD.scale-16)/4;
		p[0].x = p[1].x = xx->pos.x - dist;
		p[2].x = p[3].x = p[4].x = xx->pos.x + dist;
		p[1].y = p[2].y = xx->pos.y - dist;
		p[3].y = p[4].y = p[0].y = xx->pos.y + dist;
		p[3].y = p[3].y - (dist/2);
		p[4].x = p[4].x - (dist/2);

		for (int i=0;i<5;i++) {
			type[i] = 0;
		}
		DrawPoly(d, 5, p, type, color, 0, DRAW_CLOSED);
		DrawPoly(d, 5, p, type, drawColorGold, 0, DRAW_FILL);
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
    struct extraDataNote_t *xx = GET_EXTRA_DATA( t, T_NOTE, extraDataNote_t );
    DIST_T d;
    d = FindDistance(*p, xx->pos);

    if (d < 3.0*(mainD.scale/12.0)) {
        return d;
    }

    return DIST_INF;
}

static void DeleteNote(track_p t)
{
	struct extraDataNote_t *xx = GET_EXTRA_DATA( t, T_NOTE, extraDataNote_t );

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


#if 0
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
#endif

/**
 * Write the note to file. Handles the complete syntax for a note statement
 *
 * \param t IN pointer to the note track element
 * \param f IN file handle for writing
 * \return TRUE for success
 */

static BOOL_T WriteNote(track_p t, FILE * f)
{
    struct extraDataNote_t *xx = GET_EXTRA_DATA( t, T_NOTE, extraDataNote_t );
    BOOL_T rc = TRUE;
#ifdef UTFCONVERT
	unsigned strings2convert = 1;
#endif

	rc &= fprintf(f, "NOTE %d %u 0 0 %0.6f %0.6f 0 %d", GetTrkIndex(t),
		GetTrkLayer(t),
		xx->pos.x, xx->pos.y, xx->op )>0;

	char *s[2] = { NULL, NULL };
	switch (xx->op) {
	case OP_NOTETEXT:
		s[0]=ConvertToEscapedText( xx->noteData.text );
		break;
	case OP_NOTELINK:
		s[0]=ConvertToEscapedText( xx->noteData.linkData.url );
		s[1]=ConvertToEscapedText( xx->noteData.linkData.title );
#ifdef UTFCONVERT
		strings2convert = 2;
#endif
		break;
	case OP_NOTEFILE:
		s[0]=ConvertToEscapedText( xx->noteData.fileData.path );
		s[1]=ConvertToEscapedText( xx->noteData.fileData.title );
#ifdef UTFCONVERT
		strings2convert = 2;
#endif
		break;
	default:
		CHECKMSG( FALSE, ( "WriteNote: %d", xx->op ) );
	}
#ifdef UTFCONVERT
	for ( unsigned int inx = 0; inx < strings2convert; inx++ ) {
		if ( RequiresConvToUTF8( s[inx] ) ) {
			wSystemToUTF8 ( s[inx], message, sizeof message );
			MyFree( s[inx] );
			s[inx] = MyStrdup( message );
		}
	}
#endif
	rc &= fprintf( f, " \"%s\"", s[0] )>0;
	MyFree(s[0]);
	if ( s[1] ) {
		rc &= fprintf( f, " \"%s\"", s[1] )>0;
		MyFree( s[1] );
	}
	rc &= fprintf( f, "\n" )>0;
	
	return rc;
}

/**
 * Read a track note aka postit
 *
 * \param line
 */

static BOOL_T
ReadTrackNote(char *line)
{
    track_p t;
    int size;
    char * cp;
    struct extraDataNote_t *xx;
    wIndex_t index;
    wIndex_t layer;
    coOrd pos;
    DIST_T elev;
	char *noteText;
	enum noteCommands noteType;
	char * sText;

    if (!GetArgs(line + 5, paramVersion < 3 ? "XXpYdc" : paramVersion < 9 ?
                 "dL00pYdc" : "dL00pfdc",
                 &index, &layer, &pos, &elev, &size, &cp)) {
        return FALSE;
    }

	if ( paramVersion >= VERSION_INLINENOTE ) {
		noteType = size;
		t = NewNote(index, pos, noteType);
   		SetTrkLayer(t, layer);
	   
   		xx = GET_EXTRA_DATA( t, T_NOTE, extraDataNote_t );
		switch (noteType) {
		case OP_NOTETEXT:
			if ( !GetArgs( cp, "qc", &sText, &cp ) )
				return FALSE;
#ifdef UTFCONVERT
			ConvertUTF8ToSystem( sText );
#endif
			xx->noteData.text = sText;
			break;
		case OP_NOTELINK:
			if ( !GetArgs( cp, "qc", &sText, &cp ) )
				return FALSE;
#ifdef UTFCONVERT
			ConvertUTF8ToSystem( sText );
#endif
			xx->noteData.linkData.url = sText;
			if ( !GetArgs( cp, "qc", &sText, &cp ) )
				return FALSE;
#ifdef UTFCONVERT
			ConvertUTF8ToSystem( sText );
#endif
			xx->noteData.linkData.title = sText;
			break;
		case OP_NOTEFILE:
			if ( !GetArgs( cp, "qc", &sText, &cp ) )
				return FALSE;
#ifdef UTFCONVERT
			ConvertUTF8ToSystem( sText );
#endif
			xx->noteData.fileData.path = sText;
			if ( !GetArgs( cp, "qc", &sText, &cp ) )
				return FALSE;
#ifdef UTFCONVERT
			ConvertUTF8ToSystem( sText );
#endif
			xx->noteData.fileData.title = sText;
			xx->noteData.fileData.inArchive = FALSE;
			break;
		default:
			CHECKMSG( FALSE, ( "ReadNote: %d", noteType ) );
		}
	} else {
	noteText = ReadMultilineText();

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
	   
    xx = GET_EXTRA_DATA( t, T_NOTE, extraDataNote_t );

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
    MyFree(noteText);
    }
	return TRUE;
}

/**
 * Handle reading of NOTE
 *
 * \param line IN complete line with NOTE statement
 */

static BOOL_T
ReadNote(char * line)
{
    if (strncmp(line, "NOTE MAIN", 9) == 0) {
        return ReadMainNote(line);
    } else {
        return ReadTrackNote(line);
    }
}

static void MoveNote(track_p trk, coOrd orig)
{
    struct extraDataNote_t * xx = GET_EXTRA_DATA( trk, T_NOTE, extraDataNote_t );
    xx->pos.x += orig.x;
    xx->pos.y += orig.y;
    SetBoundingBox(trk, xx->pos, xx->pos);
}


static void RotateNote(track_p trk, coOrd orig, ANGLE_T angle)
{
    struct extraDataNote_t * xx = GET_EXTRA_DATA( trk, T_NOTE, extraDataNote_t );
    Rotate(&xx->pos, orig, angle);
    SetBoundingBox(trk, xx->pos, xx->pos);
}

static void RescaleNote(track_p trk, FLOAT_T ratio)
{
    struct extraDataNote_t * xx = GET_EXTRA_DATA( trk, T_NOTE, extraDataNote_t );
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

static wBool_t CompareNote( track_cp trk1, track_cp trk2 )
{
	struct extraDataNote_t *xx1 = GET_EXTRA_DATA( trk1, T_NOTE, extraDataNote_t );
	struct extraDataNote_t *xx2 = GET_EXTRA_DATA( trk2, T_NOTE, extraDataNote_t );
	char * cp = message + strlen(message);
	REGRESS_CHECK_POS( "Pos", xx1, xx2, pos )
	REGRESS_CHECK_INT( "Op", xx1, xx2, op )
	return TRUE;
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
	ActivateNote,
	CompareNote
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
		curNoteType = VP2L(commandContext);
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

		switch (curNoteType)
		{
		case OP_NOTETEXT:
			NewTextNoteUI(pos);
			break;
		case OP_NOTELINK:
			NewLinkNoteUI(pos);
			break;
		case OP_NOTEFILE:
			NewFileNoteUI(pos);
			break;
		}

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
			case OP_NOTEFILE:
				DrawBitMap(&tempD, oldPos, document_bm, normalColor);
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
// RWS not used #include "bitmaps/note.xpm"

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
		icon = wIconCreatePixMap(nt->xpm[iconSize]);
		AddMenuButton(menu, CmdNote, nt->helpKey, _(nt->cmdName), icon, LEVEL0_50, IC_STICKY | IC_POPUP2, nt->acclKey, I2VP(nt->OP));
	}
	ButtonGroupEnd();

	T_NOTE = InitObject(&noteCmds);
}
