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

#define URLMAXIMUMLENGTH (2048)
#define PATHMAXIMUMLENGTH (2048)
#define TITLEMAXIMUMLENGTH (81)

struct extraDataNote {
	coOrd pos;
	char * text;
};

struct noteTextData {
	coOrd pos;
	unsigned int layer;
	char *text;
};

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
	bool inArchive;
};

enum { OR_TEXT, LY_TEXT, TX_TEXT };
enum { OR_LINK, LY_LINK, TITLE_LINK, TX_LINK, OK_LINK };
enum { OR_FILE, LY_FILE, TITLE_FILE, OK_FILE };

/* linknoteui.c */
void NewLinkNoteUI(track_p trk);
struct noteLinkData *GetNoteLinkData(void);
bool IsLinkNote(track_p trk);
void DescribeLinkNote(track_p trk, char * str, CSIZE_T len);
void ActivateLinkNote(track_p trk);

/* filenozeui.c */
void NewFileNoteUI(track_p trk);
struct noteFileData *GetNoteFileData(void);
bool IsFileNote(track_p trk);
void DescribeFileNote(track_p trk, char * str, CSIZE_T len);
void ActivateFileNote(track_p trk);

/* textnoteui.c */
void NewTextNoteUI(track_p trk);
struct noteTextData *GetNoteTextData(void);
void DescribeTextNote(track_p trk, char * str, CSIZE_T len);

/* trknote.c */
void UpdateNote(track_p trk, int inx, descData_p descUpd, BOOL_T needUndoStart);
void UpdateLink(track_p trk, int inx, descData_p descUpd, BOOL_T needUndoStart);
void UpdateFile(track_p trk, int inx, descData_p descUpd, BOOL_T needUndoStart);
void SplitNoteUri(char *text, char *uri, size_t uriMaxLength, char *title, size_t titleMaxLength);
#endif // !HAVE_NOTE_H
