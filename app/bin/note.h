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
  *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
  */

#ifndef HAVE_NOTE_H
#define HAVE_NOTE_H
#include <stdbool.h>
#include "track.h"

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
struct extraDataNote {
	coOrd pos;					/**< position */
	unsigned int layer;
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
};

//struct noteTextData {
//	coOrd pos;
//	unsigned int layer;
//	char *text;
//	track_p trk;
//};

struct noteLinkData {
	coOrd pos;
	unsigned int layer;
	char title[TITLEMAXIMUMLENGTH];
	char url[URLMAXIMUMLENGTH];
	track_p trk;
};

struct noteFileData {
	coOrd pos;
	unsigned int layer;
	char title[TITLEMAXIMUMLENGTH];
	char path[PATHMAXIMUMLENGTH];
	track_p trk;
	BOOL_T inArchive;
};

enum { OR_NOTE, LY_NOTE, TX_TEXT, OK_TEXT,  TITLE_LINK, TX_LINK, OK_LINK, TITLE_FILE, OK_FILE, CANCEL_NOTE };

/* linknoteui.c */
void NewLinkNoteUI(track_p trk);
BOOL_T IsLinkNote(track_p trk);
void DescribeLinkNote(track_p trk, char * str, CSIZE_T len);
void ActivateLinkNote(track_p trk);

/* filenozeui.c */
void NewFileNoteUI(track_p trk);
BOOL_T IsFileNote(track_p trk);
void DescribeFileNote(track_p trk, char * str, CSIZE_T len);
void ActivateFileNote(track_p trk);

/* textnoteui.c */
void NewTextNoteUI(track_p trk);
void DescribeTextNote(track_p trk, char * str, CSIZE_T len);

/* trknote.c */
void NoteStateSave(track_p trk);

void UpdateFile(struct extraDataNote *noteUIData, int inx, BOOL_T needUndoStart);
void UpdateText(struct extraDataNote *noteUIData, int inx, BOOL_T needUndoStart);
void UpdateLink(struct extraDataNote *noteUIData, int inx, BOOL_T needUndoStart);
#endif // !HAVE_NOTE_H
