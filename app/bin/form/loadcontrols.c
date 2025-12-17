/**
 * \file   loadcontrol.c
 * \brief
 */

/*  XTrackCad - Model Railroad CAD
 *  Copyright (C) 2005, 2024 Dave Bullis
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

#include <wlib.h>
#include <form.h>
#include "formprivate.h"

static void
LoadFloatEntry(paramData_p entry, FLOAT_T value)
{
	char* formattedString;
	if (entry->option & PDO_DIM) {
		if (entry->option & PDO_SMALLDIM) {
			formattedString = FormatSmallDistance(value);
		} else {
			formattedString = FormatDistance(value);
		}
	} else {
		if (entry->option & PDO_ANGLE) {
			value = NormalizeAngle((angleSystem == ANGLE_POLAR) ? value : -value);
		}
		formattedString = FormatFloat(value);
	}
	wEntrySetValue(entry->control, formattedString);
}

static void
LoadColorButton( paramData_p colorButton, wDrawColor color)
{
	wColorSelectButtonSetColor(colorButton->control, color);
	colorButton->oldD.dc = color;
}

void FormLoadSingleControl(
        paramGroup_p pg,
        int inx)
{
	paramData_p p = &pg->paramPtr[inx];

	if ((p->option & PDO_DLGIGNORE) != 0) {
		p->bInvalid = FALSE;
		return;
	}
	if (p->control == NULL || p->valueP == NULL) {
		return;
	}

	p->bInvalid = FALSE;
	wControlHilite(p->control, FALSE );

	switch (p->type) {
	case PD_LONG:
		wEntrySetValue(p->control, FormatLong(*(long*)p->valueP));
		//if (!ParamIntegerRangeCheck(p, *(long*)p->valueP)) {
		//	return;
		//}
		p->oldD.l = *(long*)p->valueP;
		break;
	case PD_RADIO:
		wRadioSetValue(p->control, *(long*)p->valueP);
		p->oldD.l = *(long*)p->valueP;
		break;
	case PD_TOGGLE:
		wToggleSetValue(p->control, *(long*)p->valueP);
		p->oldD.l = *(long*)p->valueP;
		break;
	case PD_LIST:
	case PD_DROPLIST:
	case PD_COMBOLIST:
		wListSetIndex(p->control, *(wIndex_t*)p->valueP);
		p->oldD.l = *(wIndex_t*)p->valueP;
		break;
	case PD_COLORLIST:
		LoadColorButton(p, *(wDrawColor*)p->valueP);
		break;
	case PD_FLOAT:
		LoadFloatEntry(p, *(FLOAT_T*)p->valueP);
		if (FormFloatRangeCheck(p, *(FLOAT_T*)p->valueP)) {
			p->oldD.f = *(FLOAT_T*)p->valueP;
		}
		break;
	case PD_STRING:
		if (p->oldD.s) {
			MyFree(p->oldD.s);
		}
		CHECK(p->max_string > 0);
		if (p->max_string) {
			p->oldD.s = MyMalloc(p->max_string);
			strncpy(p->oldD.s, (char*)p->valueP, p->max_string - 1);
			*(p->oldD.s + (uint32_t)p->max_string - 1) = '\0';
			wEntrySetValue(p->control, (char*)p->oldD.s);
		} else {
			p->oldD.s = MyStrdup((char*)p->valueP);
			wEntrySetValue(p->control, (char*)p->valueP);
		}
		//if ((p->option & PDO_NOTBLANK) && strlen(p->oldD.s) == 0) {
		//	wControlHilite( p->control, TRUE);
		//	p->bInvalid = TRUE;
		//}
		//else {
		//	p->bInvalid = FALSE;
		//}
		break;
	case PD_MESSAGE:
		wMessageSetValue(p->control, _((char*)p->valueP));
		break;
	case PD_TEXT:
		wTextClear(p->control);
		wTextAppend(p->control, (char*)p->valueP);
		break;
	case PD_SCALE:
		wScaleSetValue(p->control, *(FLOAT_T *)p->valueP);
		break;
	case PD_BUTTON:
	case PD_DRAW:
	case PD_MENU:
	case PD_MENUITEM:
	case PD_BITMAP:
	case PD_NOTEBOOK:
	case PD_TAG:
		break;
	}
}


void FormLoadMessage(
        paramGroup_p pg,
        int inx,
        char* message)
{
	paramData_p p = &pg->paramPtr[inx];
	if (p->control) {
		if (p->type == PD_MESSAGE) {
			wMessageSetValue(p->control, message);
		} else if (p->type == PD_STRING) {
			wEntrySetValue(p->control, message);
		} else {
			CHECKMSG(FALSE, ("p->tytpe %d", (int)p->type));
		}
	}
}

/** Load all the controls in a parameter group.
 * \param IN pointer to parameter group to be loaded
 */

void FormLoadControls(paramGroup_p pg)
{
	LOG(log_form, 1, ("FormLoadControls( %s )\n", pg->nameStr));

	for (int inx = 0; inx < pg->paramCnt; inx++) {
		FormLoadSingleControl(pg, inx);
	}
}
