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
	OP_NOTEFILE
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

/* trknote.c */
extern TRKTYP_T T_NOTE;
//void NoteStateSave(track_p trk);
track_p NewNote(wIndex_t index, coOrd p, enum noteCommands command );

#endif // !HAVE_NOTE_H
