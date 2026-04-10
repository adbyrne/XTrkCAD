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
//#include <param.h>
#include <form.h>
#include "formprivate.h"
#include <dynstring.h>

#define DO_MACRO_RECORD(p) (((p)->option & PDO_NORECORD) == 0 && (p)->group->nameStr && (p)->nameStr)

void FormMenuPush(void* dp)
{
	paramData_p p = (paramData_p)dp;

	CHECK(dp != NULL);

	const char* groupNameStr = p->group ? p->group->nameStr : "menu";
	if (((p)->option & PDO_NORECORD) == 0) {
		FormMacroRecord("PARAMETER %s %s\n", groupNameStr, p->nameStr);
	}
	if ((p->option & PDO_NOPSHACT) == 0 && p->valueP) {
		((wMenuCallBack_p)(p->valueP))(p->context);
	}
}

static void ColorSelectPush(void* dp, wDrawColor dc)
{
	paramData_p p = (paramData_p)dp;
	long rgb = wDrawGetRGB(dc);

	// make sure that the special colors for select/unselect aren't used elsewhere
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

	if (DO_MACRO_RECORD(p)) {
		FormMacroRecord( "PARAMETER %s %s %ld\n", p->group->nameStr, p->nameStr,
		                 wDrawGetRGB(dc));
	}

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

	if (DO_MACRO_RECORD(p)) {
		FormMacroRecord("PARAMETER %s %s\n", p->group->nameStr, p->nameStr);
	}

	if ((p->option & PDO_NOPSHACT) == 0) {
		if (p->valueP) {
			((wButtonCallBack_p)(p->valueP))(p->context);
		} else if (p->group->changeProc) {
			p->group->changeProc(p->group, (int)(p - p->group->paramPtr), NULL);
		}
	}
}


static void ChoicePush(long valL, void* dp)
{
	paramData_p p = (paramData_p)dp;

	if (DO_MACRO_RECORD(p)) {
		FormMacroRecord("PARAMETER %s %s\n",
		                p->group->nameStr, p->nameStr, valL);
	}

	if ((p->option & PDO_NOPSHUPD) == 0 && p->valueP) {
		*((long*)(p->valueP)) = valL;
	}
	if ((p->option & PDO_NOPSHACT) == 0 && p->group->changeProc) {
		p->group->changeProc(p->group, (int)(p - p->group->paramPtr), &valL);
	}
}

/**  val is apparently unused */

static void ListPush(wIndex_t inx, const char* val, wIndex_t op,
                     void* dp, void* itemContext)
{
	paramData_p p = (paramData_p)dp;
	long valL;

	switch (p->type) {
	case PD_LIST:
	case PD_DROPLIST:
	case PD_COMBOLIST:
		if (DO_MACRO_RECORD(p)) {
			FormMacroRecord("PARAMETER %s %s %d %s\n", p->group->nameStr, p->nameStr, inx,
			                val);
		}

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



static void IntegerPush(const char* value, void* dp)
{
	paramData_p p = (paramData_p)dp;
	long valL;

	// const char* value;

	// \todo Pressing Enter key is handled here, but when does that happen?, see following
	// functions as well. Function currently gets called on focus loss
	//if (strlen(val) == 1 && val[strlen(val) - 1] == '\n') {
	//	value = wEntryGetValue(p->control);
	//	p->enter_pressed = TRUE;
	//}
	//else {
	//	value = val;
	//	p->enter_pressed = FALSE;
	//}

	valL = FormIntegerGetValue(p, value);
	if(p->bInvalid) {
		// LOG(log_form, 1, (" -> InvalidNumber\n"));
		return;
	}

	if (!FormIntegerRangeCheck(p, valL)) {
		return;
	}

	p->bInvalid = FALSE;
	wControlHilite(p->control, p->bInvalid);

	if (DO_MACRO_RECORD(p)) {
		FormMacroRecord("PARAMETER %s %s %ld\n", p->group->nameStr, p->nameStr, valL);
	}

	if ((p->option & PDO_NOPSHUPD) == 0 && p->valueP) {
		*((long*)(p->valueP)) = valL;
	}
	if ((p->option & PDO_NOPSHACT) == 0 && p->group->changeProc) {
		p->group->changeProc(p->group, (int)(p - p->group->paramPtr), &valL);
	}
}

static void FloatPush(const char* value, void * dp)
{
	paramData_p p = (paramData_p)dp;
	FLOAT_T valF;

	//if (strlen(val) == 1 && val[strlen(val) - 1] == '\n') {
	//	value = wEntryGetValue(p->control);
	//	p->enter_pressed = TRUE;
	//}
	//else {
	//	value = val;
	//	p->enter_pressed = FALSE;
	//}

	valF = FormFloatGetValue(p, value);
	if(p->bInvalid) {
		return;
	}

	if (!FormFloatRangeCheck(p, valF)) {
		return;
	}
	p->bInvalid = FALSE;

	if (DO_MACRO_RECORD(p)) {
		FormMacroRecord("PARAMETER %s %s %f\n", p->group->nameStr, p->nameStr, valF);
	}

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
	// const char* value;
	wBool_t result = FALSE;

	if (DO_MACRO_RECORD(p)) {
		FormMacroRecord("PARAMETER %s %s %s\n", p->group->nameStr, p->nameStr, val);
	}

	//if (strlen(val) == 1 && val[strlen(val) - 1] == '\n') {
	//	value = wEntryGetValue(p->control);
	//	p->enter_pressed = TRUE;
	//}
	//else {
	//	value = val;
	//	p->enter_pressed = FALSE;
	//}

	LOG(log_form, 1, ("StringPush( %s: Enter:%d Val:%s )\n",
	                  p->nameStr, p->enter_pressed, val));

	// Cast away const - validation functions modify the string (strip whitespace)
	if(!FormStringCheckValue(p, (char*)val)) {
		return(TRUE);
	}

	FormStringGetValue(p, (char*)val);
	p->bInvalid = FALSE;
	wControlHilite(p->control, p->bInvalid);

	if ((p->option & PDO_NOPSHACT) == 0 && p->group->changeProc)
		// CAST_AWAY_CONST: param 3 should be const but its a big change
	{
		result = p->group->changeProc(p->group, (int)(p - p->group->paramPtr),
		                              CAST_AWAY_CONST val);
	}

	return(result);
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

static void
MenuPush(void *dp)
{
	paramData_p p = (paramData_p)dp;

	if(p->valueP) {
		((void (*)(void *))p->valueP)(p->context);
	}
}

void
CreateControlText(paramData_p pd, wControl_p parent, unsigned xPos,
                  unsigned yPos, char* helpStr)
{
	const paramTextData_t* textDataP;

	textDataP = pd->winData;
	pd->control = (wControl_p)wTextCreate(parent, xPos, yPos, helpStr, NULL,
	                                      pd->winOption, textDataP->width*10, textDataP->height*16);
	if (pd->winOption & BO_READONLY ) {
		wTextSetReadonly(pd->control, TRUE);
	}
}

#define LISTDEFAULTWIDTH 10

static int
GetDefaultColumnFormat(paramListData_t *listData, wWinPix_t* widths,
                       wBool_t* justification)
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
	DynStringPrintf(&preference, "%s-%s-%s", paramData->group->nameStr,
	                paramData->nameStr, "columnwidths");

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


/**
 * If list box is created from builder file configuration is done in that file.
 *
 * \param parent
 * \param paramDataList
 * \param helpString
 * \param x
 * \param y
 * \return
 */

static wControl_p
GetFormattedList(wControl_p parent, paramData_p paramDataList,
                 const char* helpString, unsigned x, unsigned y)
{
	paramDataList->control = (wControl_p)wListCreate(parent, x, y, helpString,
	                         NULL,
	                         paramDataList->winOption,
	                         0, 0, 0, NULL, NULL, NULL, NULL,
	                         ListPush, paramDataList);

	return(paramDataList->control);
}


static wControl_p
CreateFormattedList(wControl_p parent, paramData_p paramDataList,
                    const char *helpString, unsigned x, unsigned y)
{
	wWinPix_t *columnWidths = NULL;
	wBool_t* columnJustification = NULL;

//	static wBool_t maxColCnt = 0;
//	if (paramDataList->winOption && BL_NODATASTORE) {


	paramListData_t* listDataP = (paramListData_t*)paramDataList->winData;

	if (listDataP->colCnt > 1) {
		int columns = listDataP->colCnt;
		columnWidths = (wWinPix_t*)MyMalloc(columns * sizeof(*columnWidths));
		columnJustification = (wBool_t*)MyMalloc(columns * sizeof(
		                              *columnJustification));

		GetDefaultColumnFormat(listDataP, columnWidths, columnJustification);
		GetUserColumnWidths(paramDataList, columns, columnWidths);
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
	if ((p->option & PDO_NORECORD) == 0 && p->group->nameStr&& p->nameStr) {
		FormMacroRecord( "PARAMETER %s %s %d %0.3f %0.3f\n", p->group->nameStr,
		                 p->nameStr, a, pos.x, pos.y);
	}
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

static void
CreateControl(paramData_p pd, char* helpStr,	unsigned x,	unsigned y)
{
	const paramFloatRange_t* floatRangeP;
	const paramIntegerRange_t* integerRangeP;

	const struct wIcon_t* iconP;

	wControl_p win;
	wWinPix_t width;

	static wMenu_p menu = NULL;

	if ((win = pd->group->win) == NULL) {
		win = mainW;
	}


	switch (pd->type) {
	case PD_FLOAT:
		floatRangeP = pd->winData;
		width = floatRangeP->width ? floatRangeP->width : 10;
		pd->control = wEntryCreate(win, x, y, helpStr, _(pd->winLabel),
		                           pd->winOption, width, NULL, 0, FloatPush, pd);
		break;
	case PD_LONG:
		integerRangeP = pd->winData;
		width = integerRangeP->width ? integerRangeP->width : 10;
		pd->control = wEntryCreate(win, x, y, helpStr, _(pd->winLabel),
		                           pd->winOption, width, NULL, 0, IntegerPush, pd);
		break;
	case PD_STRING:
		width = pd->winData ? (wWinPix_t)VP2L(pd->winData) : (wWinPix_t)250;
		pd->control = wEntryCreate(win, x, y, helpStr, _(pd->winLabel),
		                           pd->winOption, width, (pd->option & PDO_NOPSHUPD) ? NULL : pd->valueP, 0,
		                           StringPush,
		                           pd);
		break;
	case PD_RADIO:
		pd->control = wRadioCreate(win, x, y, helpStr, _(pd->winLabel),
		                           pd->winOption, pd->winData, pd->valueP, ChoicePush, pd);
		break;
	case PD_TOGGLE:
		pd->control = wToggleCreate(win, x, y, helpStr, _(pd->winLabel),
		                            pd->winOption, pd->winData, pd->valueP, ChoicePush, pd);
		break;
	case PD_LIST:
		if (pd->group->options & PGO_FULLDIALOGFROMBUILDER) {
			pd->control = GetFormattedList(win, pd, helpStr, x, y);
		} else {
			pd->control = CreateFormattedList(win, pd, helpStr, x, y);
		}
		break;
	case PD_DROPLIST:
		width = pd->winData ? (wWinPix_t)VP2L(pd->winData) : (wWinPix_t)
		        LISTDEFAULTWIDTH;
		//pd->control = (wControl_p)wDropListCreate(win, xx, yy, helpStr,
		//	_(pd->winLabel), pd->winOption, 10, w, NULL, ParamListPush, pd);
		break;
	case PD_COMBOLIST:
		width = pd->winData ? (wWinPix_t)VP2L(pd->winData) : (wWinPix_t)
		        LISTDEFAULTWIDTH;
		pd->control = (wControl_p)wComboBoxCreate(win, x, y, helpStr,
		              _(pd->winLabel), pd->winOption, 10, width, NULL,
		              ListPush, pd);
		//listDataP->height = wControlGetHeight(pd->control);
		break;
	case PD_COLORLIST:
		pd->control = (wControl_p)wColorSelectButtonCreate(win, x, y, helpStr,
		              _(pd->winLabel), pd->winOption, 1, NULL, ColorSelectPush, pd);
		break;
	case PD_MESSAGE:
		pd->control = (wControl_p)wMessageCreateEx(win,
		              x+((pd->option & PDO_DLGNEWCOLUMN) != 0), y, helpStr, 1,
		              pd->valueP ? _(pd->valueP) : " ", pd->winOption);
		break;
	case PD_BUTTON:
		pd->control = (wControl_p)wButtonCreate(win, x, y, helpStr, _(pd->winLabel),
		                                        pd->winOption, 0, ButtonPush, pd);
		break;
	case PD_MENU:
		menu = wMenuCreate(win, x, y, helpStr, _(pd->winLabel), pd->winOption);
		//pd->control = menu;
		break;
	case PD_MENUITEM:
		pd->control = wMenuPushCreate(menu, helpStr, _(pd->winLabel), 0,
		                              MenuPush, pd);
		break;
	case PD_DRAW:
		CreateDrawingArea(win, helpStr, pd );
		break;
	case PD_TEXT:
		CreateControlText(pd, win, x + ((pd->option & PDO_DLGNEWCOLUMN) != 0), y,
		                  helpStr);
		break;
	case PD_BITMAP:
		iconP = pd->winData;
		pd->control = wBitmapViewCreate(win, x, y, pd->winOption, (wIcon_p)iconP);
		break;
	case PD_SCALE:
		pd->control = wScaleCreate(win, helpStr, pd->valueP, ScalePush, pd);
		break;
	case PD_NOTEBOOK:
		pd->control = wNotebookCreate(win, helpStr, 0, 0L);
		break;
	case PD_TAG:
		pd->control = wTagCreate(win, helpStr, _(pd->winLabel), pd->valueP,
		                         pd->context);
		break;
	default:
		CHECK(FALSE);
	}
	pd->bShown = TRUE;

}

void FormCreateControls(paramGroup_p group)
{
	DynString helpString;
	unsigned xPos = 1;
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
		} else {
			CreateControl(pd, pd->nameStr, xPos, yPos);
			if (!(pd->option & PDO_SAMEROW)) {
				yPos++;
			}
		}

		DynStringClear(&helpString);

	}
	DynStringFree(&helpString);
}


