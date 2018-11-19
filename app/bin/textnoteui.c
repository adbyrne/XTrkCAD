/** \file textnoteui.c
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

#include "i18n.h"
#include "misc.h"
#include "note.h"
#include "param.h"
#include "track.h" 

static struct noteTextData noteData;

static descData_t noteDesc[] = {
    /*OR_TEXT*/	{ DESC_POS, N_("Position"), &noteData.pos },
    /*LY_TEXT*/	{ DESC_LAYER, N_("Layer"), &noteData.layer },
    /*TX_TEXT*/	{ DESC_TEXT, NULL, NULL },
    { DESC_NULL }
};

/**
 * Return the current values entered
 */
struct noteTextData *GetNoteTextData()
{
	if (wTextGetModified((wText_p)noteDesc[TX_TEXT].control0)) {
		int len;

		MyFree(noteData.text);
		len = wTextGetSize((wText_p)noteDesc[TX_TEXT].control0);
		noteData.text = (char*)MyMalloc(len + 2);
		wTextGetText((wText_p)noteDesc[TX_TEXT].control0, noteData.text, len);
	}
	return(&noteData);
}


void DescribeTextNote(track_p trk, char * str, CSIZE_T len)
{
    struct extraDataNote * xx = (struct extraDataNote *)GetTrkExtraData(trk);
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
    noteDesc[TX_TEXT].valueP = xx->text;
    noteDesc[OR_TEXT].mode = 0;
    noteDesc[TX_TEXT].mode = 0;
    noteDesc[LY_TEXT].mode = DESC_NOREDRAW;
    DoDescribe(_("Note"), trk, noteDesc, UpdateNote);
}

/**
 * Show the UI for entering new text notes
 * 
 * \param xx Note object data
 */

void NewTextNoteUI(track_p trk) {
	struct extraDataNote * xx = (struct extraDataNote *)GetTrkExtraData(trk);
	char *tmpPtrText;

	tmpPtrText = _("Replace this text with your note");
	xx->text = (char*)MyMalloc(strlen(tmpPtrText) + 1);
	strcpy(xx->text, tmpPtrText);

	DescribeTextNote(trk, message, sizeof message);
}

