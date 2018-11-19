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
	char url[URLMAXIMUMLENGTH];
	track_p trk;
};

enum { OR_TEXT, LY_TEXT, TX_TEXT };
enum { OR_LINK, LY_LINK, TX_LINK, OK_LINK };

/* linknoteui.c */
void NewLinkNoteUI(track_p trk);
struct noteLinkData *GetNoteLinkData(void);
bool IsLinkNote(track_p trk);
void DescribeLinkNote(track_p trk, char * str, CSIZE_T len);
void ActivateLinkNote(track_p trk);

/* textnoteui.c */
void NewTextNoteUI(track_p trk);
struct noteTextData *GetNoteTextData(void);
void DescribeTextNote(track_p trk, char * str, CSIZE_T len);

/* trknote.c */
void UpdateNote(track_p trk, int inx, descData_p descUpd, BOOL_T needUndoStart);
void UpdateLink(track_p trk, int inx, descData_p descUpd, BOOL_T needUndoStart);
#endif // !HAVE_NOTE_H
