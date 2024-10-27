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
#include <param.h>
#include <form.h>
#include "formprivate.h"


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
	FLOAT_T tmpR;
	char* valS;

	if ((p->option & PDO_DLGIGNORE) != 0) {
		p->bInvalid = FALSE;
		return;
	}
	if (p->control == NULL || p->valueP == NULL) {
		return;
	}
	switch (p->type) {
	case PD_LONG:
		wEntrySetValue((wEntry_p)p->control, FormatLong(*(long*)p->valueP));
		//if (!ParamIntegerRangeCheck(p, *(long*)p->valueP)) {
		//	return;
		//}
		p->oldD.l = *(long*)p->valueP;
		break;
	case PD_RADIO:
		wRadioSetValue((wChoice_p)p->control, *(long*)p->valueP);
		p->oldD.l = *(long*)p->valueP;
		break;
	case PD_TOGGLE:
		wToggleSetValue((wChoice_p)p->control, *(long*)p->valueP);
		p->oldD.l = *(long*)p->valueP;
		break;
	case PD_LIST:
	case PD_DROPLIST:
	case PD_COMBOLIST:
		wListSetIndex((wList_p)p->control, *(wIndex_t*)p->valueP);
		p->oldD.l = *(wIndex_t*)p->valueP;
		break;
	case PD_COLORLIST:
		LoadColorButton(p, *(wDrawColor*)p->valueP);
		break;
	case PD_FLOAT:
		tmpR = *(FLOAT_T*)p->valueP;
		if (p->option & PDO_DIM) {
			if (p->option & PDO_SMALLDIM) {
				valS = FormatSmallDistance(tmpR);
			}
			else {
				valS = FormatDistance(tmpR);
			}
		}
		else {
			if (p->option & PDO_ANGLE) {
				tmpR = NormalizeAngle((angleSystem == ANGLE_POLAR) ? tmpR : -tmpR);
			}
			valS = FormatFloat(tmpR);
		}
		wEntrySetValue((wEntry_p)p->control, valS);
		//if (!ParamFloatRangeCheck(p, tmpR)) {
		//	break;
		//}
		p->oldD.f = tmpR;
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
			wEntrySetValue((wEntry_p)p->control, (char*)p->oldD.s);
		}
		else {
			p->oldD.s = MyStrdup((char*)p->valueP);
			wEntrySetValue((wEntry_p)p->control, (char*)p->valueP);
		}
		if ((p->option & PDO_NOTBLANK) && strlen(p->oldD.s) == 0) {
			ParamHilite(p->group->win, p->control, TRUE);
			p->bInvalid = TRUE;
		}
		else {
			p->bInvalid = FALSE;
		}
		break;
	case PD_MESSAGE:
		wMessageSetValue((wMessage_p)p->control, _((char*)p->valueP));
		break;
	case PD_TEXT:
		wTextClear((wText_p)p->control);
		wTextAppend((wText_p)p->control, (char*)p->valueP);
		break;
	case PD_BUTTON:
	case PD_DRAW:
	case PD_MENU:
	case PD_MENUITEM:
	case PD_BITMAP:
		break;
	}
}


/** Load all the controls in a parameter group.
 * \param IN pointer to parameter group to be loaded
 */

void FormLoadControls(paramGroup_p pg)
{
	LOG(log_dialogs, 1, ("FormLoadControls( %s )\n", pg->nameStr));

	for (int inx = 0; inx < pg->paramCnt; inx++) {
		FormLoadSingleControl(pg, inx);
	}
}
