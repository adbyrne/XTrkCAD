/** \file note.h
 * Common definitions for notes
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

#ifndef HAVE_NOTE_H
#define HAVE_NOTE_H
#include "common.h"

#define URLMAXIMUMLENGTH (512)
#define PATHMAXIMUMLENGTH (2048)
#define TITLEMAXIMUMLENGTH (81)

#define MYMIN(x, y) (((x) < (y)) ? (x) : (y))

#define DELIMITER "--|--"

enum noteCommands {
	OP_NOTETEXT,
	OP_NOTELINK,
	OP_NOTEFILE,
	OP_NOTEJSON
};

/** SF #802 follow-on: the on-canvas marker shape for a note type, one of a
 * user-configurable set (see trknote.c's NoteTypePrefLoad()/DrawNoteShape()).
 * NOTE_SHAPE_SQUARE is the original/default shape (a square with one lopped
 * corner) -- kept exactly as before so an unconfigured install looks
 * unchanged. */
enum noteShape {
	NOTE_SHAPE_SQUARE,
	NOTE_SHAPE_CIRCLE,
	NOTE_SHAPE_DIAMOND,
	NOTE_SHAPE_TRIANGLE,
	NOTE_SHAPE_PENTAGON,
	NOTE_SHAPE_HEXAGON,
	NOTE_SHAPE_OCTAGON,
	NOTE_SHAPE_STAR,
	NOTE_SHAPE_CROSS,
	NOTE_SHAPE_X,
	NOTE_SHAPE_COUNT
};

/** hold the data for the note */
typedef struct extraDataNote_t {
	extraDataBase_t base;
	coOrd pos;					/**< position */
	enum noteCommands op;		/**< note type */
	track_p trk;				/**< track */
	union {
		char * text;			/**< used for text only note */
		struct {
			char *title;
			char *url;
		} linkData;				/**< used for link note */
		struct {
			char *path;
			char *title;
			BOOL_T inArchive;
		} fileData;				/**< used for file note */
	} noteData;
} extraDataNote_t;


/* linknoteui.c */
void NewLinkNoteUI( coOrd );
BOOL_T IsLinkNote(track_p trk);
void DescribeLinkNote(track_p trk, char * str, CSIZE_T len);
void ActivateLinkNote(track_p trk);

/* filenozeui.c */
void NewFileNoteUI( coOrd );
BOOL_T IsFileNote(track_p trk);
void DescribeFileNote(track_p trk, char * str, CSIZE_T len);
void ActivateFileNote(track_p trk);

/* textnoteui.c */
void NewTextNoteUI( coOrd );
void DescribeTextNote(track_p trk, char * str, CSIZE_T len);

/* jsonnoteui.c */
void NewJsonNoteUI( coOrd );
BOOL_T IsJsonNote(track_p trk);
void DescribeJsonNote(track_p trk, char * str, CSIZE_T len);

/* trknote.c */
extern TRKTYP_T T_NOTE;
//void NoteStateSave(track_p trk);
track_p NewNote(wIndex_t index, coOrd p, enum noteCommands command );

/* SF #800 phase 3: MANAGENOTES file-format line read/write, doc comment on
 * the definition in trknote.c */
void ReadNoteNames(char *line);
BOOL_T WriteNoteNames(FILE *f);

/* SF #802 follow-on: per-note-type color/shape properties -- accessors for
 * the Manage Notes dialog's per-type tabs (dmanagenotesui.c), doc comments
 * on the definitions in trknote.c. Persisted as an app preference, call
 * NoteTypePrefSave() after any Set call the user should keep across
 * sessions. */
wDrawColor NoteTypeGetColor(enum noteCommands op);
void NoteTypeSetColor(enum noteCommands op, wDrawColor color);
enum noteShape NoteTypeGetShape(enum noteCommands op);
void NoteTypeSetShape(enum noteCommands op, enum noteShape shape);
void NoteTypePrefSave(void);

#endif // !HAVE_NOTE_H
