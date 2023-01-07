/** \file cnote.c
 * Main layout note
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis
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

#include "custom.h"
#include "dynstring.h"
#include "fileio.h"
#include "misc.h"
#include "param.h"
#include "include/utf8convert.h"

static char * mainText = NULL;
static wWin_p noteW;

static paramTextData_t noteTextData = { 300, 150 };
static paramData_t notePLs[] = {
#define I_NOTETEXT		(0)
#define noteT			((wText_p)notePLs[I_NOTETEXT].control)
	{	PD_TEXT, NULL, "text", PDO_DLGRESIZE, &noteTextData }
};
static paramGroup_t notePG = { "note", 0, notePLs, COUNT( notePLs ) };


void ClearNote(void)
{
	if (mainText) {
		MyFree(mainText);
		mainText = NULL;
	}
}

static void NoteOk(void * unused)
{
	if (wTextGetModified(noteT)) {
		int len;
		ClearNote();
		len = wTextGetSize(noteT);
		mainText = (char*)MyMalloc(len+2);
		wTextGetText(noteT, mainText, len);
	}

	wHide(noteW);
}


void DoNote(void * unused)
{
	if (noteW == NULL) {
		noteW = ParamCreateDialog(&notePG, MakeWindowTitle(_("Note")), _("Ok"), NoteOk,
		                          wHide, FALSE, NULL, F_NOTTRANSIENT|F_RESIZE, NULL);
	}

	wTextClear(noteT);
	wTextAppend(noteT, mainText?mainText:
	            _("Replace this text with your layout notes"));
	wTextSetReadonly(noteT, FALSE);
	wShow(noteW);
}


BOOL_T WriteMainNote(FILE* f)
{
	BOOL_T rc = TRUE;
	char *noteText = mainText;

	if (noteText && *noteText) {
#ifdef UTFCONVERT
		char *out = NULL;
		if (RequiresConvToUTF8(mainText)) {
			size_t cnt = strlen(mainText) * 2 + 1;
			out = MyMalloc(cnt);
			wSystemToUTF8(mainText, out, (unsigned int)cnt);
			noteText = out;
		}
#endif // UTFCONVERT


		char * sText = ConvertToEscapedText( noteText );
		rc &= fprintf(f, "NOTE MAIN 0 0 0 0 0 \"%s\"\n", sText )>0;
		MyFree( sText );

#ifdef UTFCONVERT
		if (out) {
			MyFree(out);
		}
#endif // UTFCONVERT
	}
	return rc;
}

/**
 * Read the layout main note
 *
 * \param line complete NOTE statement
 */

BOOL_T ReadMainNote(char *line)
{
	long size;
	char * sNote = NULL;

	if (!GetArgs(line + 9,
	             paramVersion < 3 ? "l" :
	             paramVersion < VERSION_INLINENOTE ? "0000l":
	             "0000lq", &size, &sNote)) {
		return FALSE;
	}

	if (mainText) {
		MyFree(mainText);
	}

	if ( paramVersion < VERSION_INLINENOTE ) {
		mainText = ReadMultilineText();
	} else {
		mainText = sNote;
	}
	return TRUE;
}

void InitCmdNote()
{
	ParamRegister(&notePG);
}
