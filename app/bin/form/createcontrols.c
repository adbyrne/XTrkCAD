/**
 * \file   createcontrols.c
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
#include <dynstring.h>


static void ColorSelectPush(void* dp, wDrawColor dc)
{
	paramData_p p = (paramData_p)dp;
	long rgb = wDrawGetRGB(dc);
	while (dc == drawColorPreviewSelected || dc == drawColorPreviewUnselected) {
		// The user picked a special color, tweak it
		rgb -= 1; // Make it very close but different
		if ((rgb & 0xFF) == 0)
			// Ran out of room - bail
		{
			break;
		}
		dc = wDrawFindColor(rgb);
	}
	//if (recordParamF && (p->option & PDO_NORECORD) == 0 && p->group->nameStr
	//	&& p->nameStr) {
	//	fprintf(recordParamF, "PARAMETER %s %s %ld\n", p->group->nameStr, p->nameStr,
	//		wDrawGetRGB(dc));
	//	fflush(recordParamF);
	//}
	if ((p->option & PDO_NOPSHUPD) == 0 && p->valueP) {
		*(wDrawColor*)(p->valueP) = dc;
	}
	if ((p->option & PDO_NOPSHACT) == 0 && p->group->changeProc) {
		p->group->changeProc(p->group, (int)(p - p->group->paramPtr), &dc);
	}
}



static void ButtonPush(void* dp)
{
	paramData_p p = (paramData_p)dp;
	//if (recordParamF && (p->option & PDO_NORECORD) == 0 && p->group->nameStr
	//	&& p->nameStr) {
	//	fprintf(recordParamF, "PARAMETER %s %s\n", p->group->nameStr, p->nameStr);
	//	fflush(recordParamF);
	//}
	if ((p->option & PDO_NOPSHACT) == 0) {
		if (p->valueP) {
			((wButtonCallBack_p)(p->valueP))(p->context);
		}
		else if (p->group->changeProc) {
			p->group->changeProc(p->group, (int)(p - p->group->paramPtr), NULL);
		}
	}
}


static void ChoicePush(long valL, void* dp)
{
	paramData_p p = (paramData_p)dp;

	//if (recordParamF && (p->option & PDO_NORECORD) == 0 && p->group->nameStr
	//	&& p->nameStr) {
	//	fprintf(recordParamF, "PARAMETER %s %s %ld\n", p->group->nameStr, p->nameStr,
	//		valL);
	//	fflush(recordParamF);
	//}
	if ((p->option & PDO_NOPSHUPD) == 0 && p->valueP) {
		*((long*)(p->valueP)) = valL;
	}
	if ((p->option & PDO_NOPSHACT) == 0 && p->group->changeProc) {
		p->group->changeProc(p->group, (int)(p - p->group->paramPtr), &valL);
	}
}

static void ListPush(wIndex_t inx, const char* val, wIndex_t op,
	void* dp, void* itemContext)
{
	paramData_p p = (paramData_p)dp;
	long valL;

	switch (p->type) {
	case PD_LIST:
	case PD_DROPLIST:
	case PD_COMBOLIST:
		//if (recordParamF && (p->option & PDO_NORECORD) == 0 && p->group->nameStr
		//	&& p->nameStr) {
		//	fprintf(recordParamF, "PARAMETER %s %s %d %s\n", p->group->nameStr, p->nameStr,
		//		inx,
		//		val);
		//	fflush(recordParamF);
		//}
		if ((p->option & PDO_NOPSHUPD) == 0 && p->valueP) {
			*(wIndex_t*)(p->valueP) = inx;
		}
		if ((p->option & PDO_NOPSHACT) == 0 && p->group->changeProc) {
			valL = inx;
			p->group->changeProc(p->group, (int)(p - p->group->paramPtr), &valL);
		}
		break;

	default:
		;
	}
}



static void IntegerPush(const char* val, void* dp)
{
	paramData_p p = (paramData_p)dp;
	long valL;
	char* cp;
	const char* value;

	if (strlen(val) == 1 && val[strlen(val) - 1] == '\n') {
		value = wEntryGetValue(p->control);
		p->enter_pressed = TRUE;
	}
	else {
		value = val;
		p->enter_pressed = FALSE;
	}

	valL = strtol(value, &cp, 10);
	for (; isspace((unsigned char)*cp); cp++);
	if (*cp != '\0') {
		//wWinPix_t h = wControlGetHeight(p->control);
		//wControlSetBalloon(p->control, 0, -h * 3 / 4, _("Invalid Number"));
		p->bInvalid = TRUE;
		// LOG(log_paraminput, 1, (" -> InvalidNumber\n"));
		ParamHilite(p->group->win, p->control, p->bInvalid);
		return;
	}
	if (!FormIntegerRangeCheck(p, valL)) {
		return;
	}
	wControlSetBalloon(p->control, 0, 0, NULL);
	p->bInvalid = FALSE;

	//if (recordParamF && (p->option & PDO_NORECORD) == 0 && p->group->nameStr
	//	&& p->nameStr) {
	//	fprintf(recordParamF, "PARAMETER %s %s %ld\n", p->group->nameStr, p->nameStr,
	//		valL);
	//	fflush(recordParamF);
	//}
	if ((p->option & PDO_NOPSHUPD) == 0 && p->valueP) {
		*((long*)(p->valueP)) = valL;
	}
	if ((p->option & PDO_NOPSHACT) == 0 && p->group->changeProc) {
		p->group->changeProc(p->group, (int)(p - p->group->paramPtr), &valL);
	}
	ParamHilite(p->group->win, p->control, p->bInvalid);

}

static void FloatPush(const char* val, void * dp)
{
	paramData_p p = (paramData_p)dp;
	const char* value;
	FLOAT_T valF;
	BOOL_T valid;

	if (strlen(val) == 1 && val[strlen(val) - 1] == '\n') {
		value = wEntryGetValue(p->control);
		p->enter_pressed = TRUE;
	}
	else {
		value = val;
		p->enter_pressed = FALSE;
	}

	if (p->option & PDO_DIM) {
		valF = DecodeDistance((wEntry_p)p->control, &valid);
	}
	else {
		valF = DecodeFloat((wEntry_p)p->control, &valid);
		if (p->option & PDO_ANGLE) {
			valF = NormalizeAngle((angleSystem == ANGLE_POLAR) ? valF : -valF);
		}
	}
	if (!valid) {
		//wWinPix_t h = wControlGetHeight(p->control);
		//wControlSetBalloon(p->control, 0, -h * 3 / 4, decodeErrorStr);
		p->bInvalid = TRUE;
		wControlHilite(p->control, p->bInvalid);
		return;
	}
	if (!FormFloatRangeCheck(p, valF)) {
		return;
	}
	wControlSetBalloon(p->control, 0, 0, NULL);
	p->bInvalid = FALSE;

	//if (recordParamF && (p->option & PDO_NORECORD) == 0 && p->group->nameStr
	//	&& p->nameStr) {
	//	fprintf(recordParamF, "PARAMETER %s %s %0.6f\n", p->group->nameStr, p->nameStr,
	//		valF);
	//	fflush(recordParamF);
	//}
	if ((p->option & PDO_NOPSHUPD) == 0 && p->valueP) {
		*((FLOAT_T*)(p->valueP)) = valF;
	}
	if ((p->option & PDO_NOPSHACT) == 0 && p->group->changeProc && strlen(value)) {
		p->group->changeProc(p->group, (int)(p - p->group->paramPtr), &valF);
	}
	wControlHilite( p->control, p->bInvalid);
}


static bool StringPush(const char* val, void* dp)
{
	paramData_p p = (paramData_p)dp;
	const char* value;
	wBool_t result = FALSE;

	//	wBool_t bInvalid = p->bInvalid;
	//if (recordParamF && (p->option & PDO_NORECORD) == 0 && p->group->nameStr
	//	&& p->nameStr) {
	//	fprintf(recordParamF, "PARAMETER %s %s %s\n", p->group->nameStr, p->nameStr,
	//		val);
	//	fflush(recordParamF);
	//}
	if (strlen(val) == 1 && val[strlen(val) - 1] == '\n') {
		value = wEntryGetValue(p->control);
		p->enter_pressed = TRUE;
	}
	else {
		value = val;
		p->enter_pressed = FALSE;
	}
	LOG(log_form, 1, ("ParamStringPush( %s: Enter:%d Val:%s )\n",
		p->nameStr, p->enter_pressed, value));
	//if (((!paramPlayback) && p->option & PDO_NOTBLANK) && value[0] == '\0') {
	//	p->bInvalid = TRUE;
	//	wControlSetBalloon(p->control, 0, 0, NULL);
	//	wWinPix_t h = wControlGetHeight(p->control);
	//	wControlSetBalloon(p->control, 0, -h * 3 / 4, _("String cannot be blank"));
	//	ParamHilite(p->group->win, p->control, TRUE);
	//	return;
	//}
	//wControlSetBalloon(p->control, 0, 0, NULL);
	//p->bInvalid = FALSE;
	//ParamHilite(p->group->win, p->control, FALSE);

	//if ((p->option & PDO_NOPSHUPD) == 0 && p->valueP) {
	//	strncpy((char*)p->valueP, value, p->max_string - 1);
	//	((char*)p->valueP)[p->max_string - 1] = '\0';
	//	if (strlen(value) > p->max_string - 1) {
	//		p->bInvalid = TRUE;
	//		wControlSetBalloon(p->control, 0, 0, NULL);
	//		wWinPix_t h = wControlGetHeight(p->control);
	//		sprintf(message, _("String is too long, Max length is %u"), p->max_string - 1);
	//		wControlSetBalloon(p->control, 0, -h * 3 / 4, message);
	//		ParamHilite(p->group->win, p->control, TRUE);
	//	}
	//}
	if ((p->option & PDO_NOPSHACT) == 0 && p->group->changeProc)
		// CAST_AWAY_CONST: param 3 should be const but its a big change
	{
		result = p->group->changeProc(p->group, (int)(p - p->group->paramPtr),
			CAST_AWAY_CONST value);
	}

	return(result);
}


static void 
FormListPush(wIndex_t inx, const char* val, wIndex_t op,
	void* dp, void* itemContext)
{
	paramData_p p = (paramData_p)dp;
	long valL;

	switch (p->type) {
	case PD_LIST:
	case PD_DROPLIST:
	case PD_COMBOLIST:
		//if (recordParamF && (p->option & PDO_NORECORD) == 0 && p->group->nameStr
		//	&& p->nameStr) {
		//	fprintf(recordParamF, "PARAMETER %s %s %d %s\n", p->group->nameStr, p->nameStr,
		//		inx,
		//		val);
		//	fflush(recordParamF);
		//}
		if ((p->option & PDO_NOPSHUPD) == 0 && p->valueP) {
			*(wIndex_t*)(p->valueP) = inx;
		}
		if ((p->option & PDO_NOPSHACT) == 0 && p->group->changeProc) {
			valL = inx;
			p->group->changeProc(p->group, (int)(p - p->group->paramPtr), &valL);
		}
		break;

	default:
		;
	}
}

static void 
ScalePush(FLOAT_T value, void* dp)
{
	paramData_p p = (paramData_p)dp;

	if ((p->option & PDO_NOPSHUPD) == 0 && p->valueP) {
		*((FLOAT_T *)(p->valueP)) = value;
	}
	if ((p->option & PDO_NOPSHACT) == 0 && p->group->changeProc) {
		p->group->changeProc(p->group, (int)(p - p->group->paramPtr), &value);
	}

}

void
CreateControlText(paramData_p pd, wControl_p parent, char* helpStr)
{
	const paramTextData_t* textDataP;

	textDataP = pd->winData;
	pd->control = (wControl_p)wTextCreate(parent, 0, 0, helpStr, NULL,
		pd->winOption, 0, 0);
	if (pd->winOption & BO_READONLY ) {
		wTextSetReadonly(pd->control, TRUE);
	}
}

#define LISTDEFAULTWIDTH 10

static int
GetDefaultColumnFormat(paramListData_t *listData, wWinPix_t* widths, wBool_t* justification)
{
	for (int column = 0; column < listData->colCnt; column++) {
		justification[column] = listData->colWidths[column] < 0;
		widths[column] = labs(listData->colWidths[column]);
	}
	return(listData->colCnt);
}

static int 
GetUserColumnWidths(paramData_p paramData, int columns, wWinPix_t* widths)
{
	wWinPix_t colWidth;
	DynString preference;
	char* cp;

	DynStringMalloc(&preference, 20);
	DynStringPrintf(&preference, "%s-%s-%s", paramData->group->nameStr, paramData->nameStr, "columnwidths");

	cp = wPrefGetString(prefSect, DynStringToCStr(&preference));
	if (cp != NULL) {
		for (int column = 0; column < columns; column++) {
			char* cq;
			colWidth = (wWinPix_t)strtol(cp, &cq, 10);
			if (cp == cq) {
				break;
			}
			widths[column] = colWidth;
			cp = cq;
		}
	}

	DynStringFree(&preference);

	return(columns);
}

static wControl_p
CreateFormattedList(wControl_p parent, paramData_p paramDataList, const char *helpString, unsigned x, unsigned y)
{
	wWinPix_t *columnWidths = NULL;
	wBool_t* columnJustification = NULL;

//	static wBool_t maxColCnt = 0;

	paramListData_t* listDataP = (paramListData_t*)paramDataList->winData;

	if (listDataP->colCnt > 1) {
		int columns = listDataP->colCnt;
		columnWidths = (wWinPix_t*)MyMalloc(columns * sizeof(*columnWidths));
		columnJustification = (wBool_t*)MyMalloc(columns * sizeof(*columnJustification));

		GetDefaultColumnFormat(listDataP, columnWidths, columnJustification);
		GetUserColumnWidths(paramDataList, columns, columnWidths );
	}

	paramDataList->control = (wControl_p)wListCreate(parent, x, y, helpString, 
		_(paramDataList->winLabel),
		paramDataList->winOption, 
		listDataP->number, 
		listDataP->width, 
		listDataP->colCnt,
		columnWidths,
		columnJustification,
		listDataP->colTitles, NULL, ListPush, paramDataList);

	if (listDataP->colCnt > 1) {
		MyFree(columnWidths);
		MyFree(columnJustification);
	}

	return(paramDataList->control);
}

static void DrawRedraw(wControl_p d, void* dp, wWinPix_t w, wWinPix_t h)
{
	paramData_p p = (paramData_p)dp;
	paramDrawData_t* ddp = (paramDrawData_t*)p->winData;
	if (ddp->redraw) {
		ddp->redraw(d, p->context, w, h);
	}
}


static void DrawAction(wControl_p d, void* dp, wAction_t a, wDrawPix_t w,
	wDrawPix_t h)
{
	paramData_p p = (paramData_p)dp;
	paramDrawData_t* ddp = (paramDrawData_t*)p->winData;
	coOrd pos;
	ddp->d->Pix2CoOrd(ddp->d, w, h, &pos);
	//if (recordParamF && (p->option & PDO_NORECORD) == 0 && p->group->nameStr
	//	&& p->nameStr) {
	//	fprintf(recordParamF, "PARAMETER %s %s %d %0.3f %0.3f\n", p->group->nameStr,
	//		p->nameStr, a, pos.x, pos.y);
	//	fflush(recordParamF);
	//}
	if ((p->option & PDO_NOPSHACT) == 0 && ddp->action) {
		ddp->action(a, pos);
	}
}

static 
void CreateDrawingArea( wControl_p parent, char* helpStr, paramData_p pd )
{
	const paramDrawData_t* drawDataP = pd->winData;

	pd->control = wDrawCreate(parent, 0, 0, helpStr, pd->winOption,
		drawDataP->width, drawDataP->height, pd, DrawRedraw, DrawAction);

	if (drawDataP->d) {
		drawDataP->d->d = pd->control;
		drawDataP->d->dpi = wDrawGetDPI(drawDataP->d->d);
	}
}

static int CreateControl(
	paramData_p pd,
	char* helpStr,
	unsigned x,
	unsigned y)
{
	const paramFloatRange_t* floatRangeP;
	const paramIntegerRange_t* integerRangeP;
	paramListData_t* listDataP;

	const struct wIcon_t* iconP;

	wControl_p win;
	wWinPix_t w;

	static wMenu_p menu = NULL;

	if ((win = pd->group->win) == NULL) {
		win = mainW;
	}


	switch (pd->type) {
	case PD_FLOAT:
		floatRangeP = pd->winData;
		w = floatRangeP->width ? floatRangeP->width : 10;
		pd->control = wEntryCreate(win, x, y, helpStr, _(pd->winLabel),
			pd->winOption, w, NULL, 0, FloatPush, pd);
		break;
	case PD_LONG:
		integerRangeP = pd->winData;
		w = integerRangeP->width ? integerRangeP->width : 10;
		pd->control = wEntryCreate(win, x, y, helpStr, _(pd->winLabel),
			pd->winOption, w, NULL, 0, IntegerPush, pd);
		break;
	case PD_STRING:
		w = pd->winData ? (wWinPix_t)VP2L(pd->winData) : (wWinPix_t)250;
		pd->control = wEntryCreate(win, x, y, helpStr, _(pd->winLabel),
			pd->winOption, w, (pd->option & PDO_NOPSHUPD) ? NULL : pd->valueP, 0, StringPush,
			pd);
		break;
	case PD_RADIO:
		pd->control = wRadioCreate(win, x, y, helpStr, _(pd->winLabel),
			pd->winOption, pd->winData, NULL, ChoicePush, pd);
		break;
	case PD_TOGGLE:
		pd->control = wToggleCreate(win, x, y, helpStr, _(pd->winLabel),
			pd->winOption, pd->winData, NULL, ChoicePush, pd);
		break;
	case PD_LIST:
		pd->control = CreateFormattedList(win, pd, helpStr, x, y);
		break;
	case PD_DROPLIST:
		w = pd->winData ? (wWinPix_t)VP2L(pd->winData) : (wWinPix_t)LISTDEFAULTWIDTH;
		//pd->control = (wControl_p)wDropListCreate(win, xx, yy, helpStr,
		//	_(pd->winLabel), pd->winOption, 10, w, NULL, ParamListPush, pd);
		break;
	case PD_COMBOLIST:
		listDataP = (paramListData_t*)pd->winData;
		w = pd->winData ? (wWinPix_t)VP2L(pd->winData) : (wWinPix_t)LISTDEFAULTWIDTH;
		pd->control = (wControl_p)wComboBoxCreate(win, x, y, helpStr,
			_(pd->winLabel), pd->winOption, 10, w, NULL,
			FormListPush, pd);
		//listDataP->height = wControlGetHeight(pd->control);
		break;
	case PD_COLORLIST:
		pd->control = (wControl_p)wColorSelectButtonCreate(win, -1, -1, helpStr,
			_(pd->winLabel), pd->winOption, 0, NULL, ColorSelectPush, pd);
		break;
	case PD_MESSAGE:
		pd->control = (wControl_p)wMessageCreateEx(win, 0, 0, helpStr, 0,
			pd->valueP ? _(pd->valueP) : " ", pd->winOption);
		break;
	case PD_BUTTON:
		pd->control = (wControl_p)wButtonCreate(win, x, y, helpStr, _(pd->winLabel),
			pd->winOption, 0, ButtonPush, pd);
		break;
	case PD_MENU:
		menu = wMenuCreate(win, x, y, helpStr, _(pd->winLabel), pd->winOption);
		pd->control = menu;
		break;
	case PD_MENUITEM:
		pd->control = wMenuPushCreate(menu, helpStr, _(pd->winLabel), 0,
			ParamMenuPush, pd);
		break;
	case PD_DRAW:
		CreateDrawingArea(win, helpStr, pd );
		break;
	case PD_TEXT:
		CreateControlText(pd, win, helpStr);
		break;
	case PD_BITMAP:
		iconP = pd->winData;
		pd->control = (wControl_p)wBitmapCreate(win, x, y, pd->winOption, iconP);
		break;
	case PD_SCALE:
		pd->control = wScaleCreate(win, helpStr, pd->valueP, ScalePush, pd);
		break;
	default:
		CHECK(FALSE);
	}
	pd->bShown = TRUE;

}

void CreateControls(paramGroup_p group)
{
	DynString helpString;
	unsigned xPos = 0;
	unsigned yPos = 1;

	DynStringMalloc(&helpString, 80);

	for (int inx = 0; inx < (group->paramCnt); inx++) {
		paramData_t* pd = (group->paramPtr) + inx;

		if ((pd->option & PDO_DLGIGNORE) != 0) {
			continue;
		}

		LOG(log_form, 2, ("%2d: %s\n", inx, pd->nameStr));

		DynStringPrintf(&helpString, "%s-%s", group->nameStr, pd->nameStr);

		if (group->options & PGO_FULLDIALOGFROMBUILDER) {
			CreateControl(pd, pd->nameStr, -1, -1);
		}
		else
		{
			CreateControl(pd, pd->nameStr, xPos, yPos);
			yPos++;
		}

		DynStringClear(&helpString);

	}

	DynStringFree(&helpString);
}


