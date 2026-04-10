/**
 * \file   formmacro.c
 * \brief  Macro record and replay
 *
 * \author Martin Fischer
 */

/*  XTrackCad - Model Railroad CAD
 *  Copyright (C) 2005, 2025 Dave Bullis
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

#include <stdio.h>
#include <stdarg.h>
#include <wlib.h>

#include <form.h>
#include "formprivate.h"

static FILE* macroFile;

void
FormStartRecord(FILE* fileHandle)
{
	macroFile = fileHandle;
	if (macroFile == NULL) {
		return;
	}

	for ( paramGroup_cp * ppg = DialogGroupIter( NULL ); ppg;
	      ppg = DialogGroupIter( ppg ) ) {

		if ((*ppg)->options&PGO_RECORD) {
			FormGroupRecord( (*ppg) );
		}
	}
}


void FormGroupRecord(
        paramGroup_cp pg)
{
	paramData_p p;
	long rgb;

	if (macroFile == NULL) {
		return;
	}
	for (p = pg->paramPtr; p < &pg->paramPtr[pg->paramCnt]; p++) {
		if ((p->option & PDO_NORECORD) != 0 || p->valueP == NULL
		    || p->nameStr == NULL) {
			continue;
		}
		if ((p->option & PDO_DLGIGNORE) != 0) {
			continue;
		}
		switch (p->type) {
		case PD_LONG:
		case PD_RADIO:
		case PD_TOGGLE:
			fprintf(macroFile, "PARAMETER %s %s %ld\n", pg->nameStr, p->nameStr,
			        *(long*)p->valueP);
			break;
		case PD_LIST:
		case PD_DROPLIST:
		case PD_COMBOLIST:
			if (p->control) {
				wListGetValues(p->control, message, sizeof message, NULL, NULL);
			} else {
				message[0] = '\0';
			}
			fprintf(macroFile, "PARAMETER %s %s %d %s\n", pg->nameStr, p->nameStr,
			        *(wIndex_t*)p->valueP, message);
			break;
		case PD_COLORLIST:
			rgb = wDrawGetRGB(*(wDrawColor*)p->valueP);
			fprintf(macroFile, "PARAMETER %s %s %ld\n",
			        pg->nameStr, p->nameStr, rgb);
			break;
		case PD_FLOAT:
			fprintf(macroFile, "PARAMETER %s %s %0.3f\n", pg->nameStr, p->nameStr,
			        *(FLOAT_T*)p->valueP);
			break;
		case PD_STRING:
			fprintf(macroFile, "PARAMETER %s %s %s\n", pg->nameStr, p->nameStr,
			        (char*)p->valueP);
			break;
		case PD_MESSAGE:
		case PD_BUTTON:
		case PD_DRAW:
		case PD_TEXT:
		case PD_MENU:
		case PD_MENUITEM:
		case PD_BITMAP:
		default:
			break;
		}
	}
	if (pg->nameStr) {
		fprintf(macroFile, "PARAMETER GROUP %s\n", pg->nameStr);
	}
	fflush(macroFile);
}


void
FormMacroRecord(char* format, ...)
{
	va_list args;

	if (macroFile) {

		va_start(args, format);
		vfprintf(macroFile, format, args);
		va_end(args);

		fflush(macroFile);
	}
}
