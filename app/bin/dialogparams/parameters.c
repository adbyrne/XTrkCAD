/**
 * \file   parameterall.c
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

#include "misc.h"
#include "dynarray.h"
#include "param.h"
#include "paramprivate.h"
#include <messages.h>

int log_paramLayout = 0;
int log_paraminput = 0;

dynArr_t paramGroups_da;
BOOL_T paramGroups_init = FALSE;

int paramCheckErrorCount = 0;
static BOOL_T paramCheckShowErrors = FALSE;

static void SimulateButtonClick(wButton_p p);
static bool disablePlaybackDelays;

static void ParamPlayback(char* line)
{
	paramGroup_p pg;
	paramData_p p;
	long valL;
	FLOAT_T valF, valF1;
	size_t len, len1, len2;
	wIndex_t inx;
	void* listContext, * itemContext;
	long rgb;
	wDrawColor dc;
	wButton_p button;
	paramDrawData_t* ddp;
	wAction_t a;
	coOrd pos;
	char* valS;

	if (strncmp(line, "GROUP ", 6) == 0) {
		return;
	}

	for (inx = 0; inx < paramGroups_da.cnt; inx++) {
		pg = paramGroups(inx);
		if (pg->nameStr == NULL) {
			continue;
		}
		len1 = strlen(pg->nameStr);
		if (strncmp(pg->nameStr, line, len1) != 0 ||
			line[len1] != ' ') {
			continue;
		}
		for (p = pg->paramPtr, inx = 0; inx < pg->paramCnt; p++, inx++) {
			if (p->nameStr == NULL) {
				continue;
			}
			len2 = strlen(p->nameStr);
			if (strncmp(p->nameStr, line + len1 + 1, len2) != 0 ||
				(line[len1 + 1 + len2] != ' ' && line[len1 + 1 + len2] != '\0')) {
				continue;
			}
			len = len1 + 1 + len2 + 1;
			if (p->type != PD_DRAW && p->type != PD_MESSAGE && p->type != PD_MENU
				&& p->type != PD_MENUITEM) {
				ParamHilite(p->group->win, p->control, TRUE);
			}
			switch (p->type) {
			case PD_BUTTON:
				if (p->valueP) {
					((wButtonCallBack_p)(p->valueP))(p->context);
				}
				SimulateButtonClick((wButton_p)p->control);
				break;
			case PD_LONG:
				valL = atol(line + len);
				if (p->valueP) {
					*(long*)p->valueP = valL;
				}
				if (p->control) {
					wEntrySetValue((wEntry_p)p->control, FormatLong(valL));
					wFlush();
				}
				if (pg->changeProc) {
					pg->changeProc(pg, inx, &valL);
				}
				break;
			case PD_RADIO:
				valL = atol(line + len);
				if (p->valueP) {
					*(long*)p->valueP = valL;
				}
				if (p->control) {
					wRadioSetValue((wChoice_p)p->control, valL);
					wFlush();
				}
				if (pg->changeProc) {
					pg->changeProc(pg, inx, &valL);
				}
				break;
			case PD_TOGGLE:
				valL = atol(line + len);
				if (p->valueP) {
					*(long*)p->valueP = valL;
				}
				if (p->control) {
					wToggleSetValue((wChoice_p)p->control, valL);
					wFlush();
				}
				if (pg->changeProc) {
					pg->changeProc(pg, inx, &valL);
				}
				break;
			case PD_LIST:
			case PD_COMBOLIST:
				line += len;
				valL = strtol(line, &valS, 10);
				if (valS) {
					valS++;
				}
				else {
					valS = "";
				}
				if (p->control != NULL) {
					if ((p->option & PDO_LISTINDEX) == 0) {
						if (valL < 0) {
							wListSetValue((wList_p)p->control, valS);
						}
						else {
							valL = wListFindValue((wList_p)p->control, valS);
							if (valL < 0) {
								NoticeMessage(MSG_PLAYBACK_LISTENTRY, _("Ok"), NULL, line);
								break;
							}
							wListSetIndex((wList_p)p->control, (wIndex_t)valL);
						}
					}
					else {
						wListSetIndex((wList_p)p->control, (wIndex_t)valL);
					}
					wFlush();
					wListGetValues((wList_p)p->control, message, sizeof message, &listContext,
						&itemContext);
				}
				else if ((p->option & PDO_LISTINDEX) == 0) {
					break;
				}
				if (p->valueP) {
					*(wIndex_t*)p->valueP = (wIndex_t)valL;
				}
				if (pg->changeProc) {
					pg->changeProc(pg, inx, &valL);
				}
				break;
			case PD_COLORLIST:
				line += len;
				rgb = atol(line);
				dc = wDrawFindColor(rgb);
				if (p->control) {
					wColorSelectButtonSetColor((wColorButton_p)p->control, dc);
				}
				if (p->valueP) {
					*(wDrawColor*)p->valueP = dc;
				}
				if (pg->changeProc) {
					/* COLORNOP */
					pg->changeProc(pg, inx, &valL);
				}
				break;
			case PD_FLOAT:
				SetCLocale();
				valF = valF1 = atof(line + len);
				SetUserLocale();
				if (p->valueP) {
					*(FLOAT_T*)p->valueP = valF;
				}
				if (p->option & PDO_DIM) {
					if (p->option & PDO_SMALLDIM) {
						valS = FormatSmallDistance(valF);
					}
					else {
						valS = FormatDistance(valF);
					}
				}
				else {
					if (p->option & PDO_ANGLE) {
						valF1 = NormalizeAngle((angleSystem == ANGLE_POLAR) ? valF1 : -valF1);
					}
					valS = FormatFloat(valF);
				}
				if (p->control) {
					wEntrySetValue((wEntry_p)p->control, valS);
					wFlush();
				}
				if (pg->changeProc) {
					pg->changeProc(pg, inx, &valF);
				}
				break;
			case PD_STRING:
			case PD_TEXT:
				line += len;
				while (*line == ' ') { line++; }
				Stripcr(line);
				if (p->valueP) {
					strcpy((char*)p->valueP, line);
				}
				if (p->control) {
					if (p->type == PD_STRING) {
						wEntrySetValue((wEntry_p)p->control, line);
						p->bInvalid =
							(p->option & PDO_NOTBLANK) &&
							strlen(line) == 0;
					}
					else {
						wTextClear((wText_p)p->control);
						wTextAppend((wText_p)p->control, line);
					}
					wFlush();
				}
				if (pg->changeProc) {
					pg->changeProc(pg, inx, line);
				}
				break;
			case PD_DRAW:
				ddp = (paramDrawData_t*)p->winData;
				if (ddp->action == NULL) {
					break;
				}
				a = (wAction_t)strtol(line + len, &line, 10);
				pos.x = strtod(line, &line);
				pos.y = strtod(line, NULL);
				PlaybackMouse(ddp->action, ddp->d, a, pos, drawColorBlack);
				break;
			case PD_MESSAGE:
			case PD_MENU:
			case PD_BITMAP:
				break;
			case PD_MENUITEM:
				if (p->valueP) {
					if ((p->option & IC_PLAYBACK_PUSH) != 0) {
						PlaybackButtonMouse((wIndex_t)VP2L(p->context));
					}
					((wButtonCallBack_p)(p->valueP))(p->context);
				}
				break;
			}
			if (p->type != PD_DRAW && p->type != PD_MESSAGE && p->type != PD_MENU
				&& p->type != PD_MENUITEM) {
				ParamHilite(p->group->win, p->control, FALSE);
			}
			return;
		}
		button = NULL;
		if (strcmp("ok", line + len1 + 1) == 0) {
			ParamHilite(pg->win, (wControl_p)pg->okB, TRUE);
			if (pg->okProc) {
				pg->okProc(pg);
			}
			button = pg->okB;
		}
		else if (strcmp("cancel", line + len1 + 1) == 0) {
			ParamHilite(pg->win, (wControl_p)pg->cancelB, TRUE);
			if (pg->cancelProc) {
				pg->cancelProc(pg->win);
			}
			button = pg->cancelB;
		}
		SimulateButtonClick(button);
		ParamHilite(pg->win, (wControl_p)button, FALSE);
		if (!button) {
			NoticeMessage("Unknown PARAM: %s", _("Ok"), NULL, line);
		}
		return;
	}
	NoticeMessage("Unknown PARAM: %s", _("Ok"), NULL, line);
}


static void ParamCheck(char* line)
{
	paramGroup_p pg;
	paramData_p p;
	long valL;
	FLOAT_T valF, diffF;
	size_t len, len1, len2;
	wIndex_t inx;
	void* listContext, * itemContext;
	char* valS;
	char* expVal = NULL, * actVal = NULL;
	char expNum[20], actNum[20];
	BOOL_T hasError = FALSE;
	FILE* f;

	for (inx = 0; inx < paramGroups_da.cnt; inx++) {
		pg = paramGroups(inx);
		if (pg->nameStr == NULL) {
			continue;
		}
		len1 = strlen(pg->nameStr);
		if (strncmp(pg->nameStr, line, len1) != 0 ||
			line[len1] != ' ') {
			continue;
		}
		for (p = pg->paramPtr, inx = 0; inx < pg->paramCnt; p++, inx++) {
			if (p->nameStr == NULL) {
				continue;
			}
			len2 = strlen(p->nameStr);
			if (strncmp(p->nameStr, line + len1 + 1, len2) != 0 ||
				(line[len1 + 1 + len2] != ' ' && line[len1 + 1 + len2] != '\0')) {
				continue;
			}
			if (p->valueP == NULL) {
				return;
			}
			len = len1 + 1 + len2 + 1;
			switch (p->type) {
			case PD_BUTTON:
				break;
			case PD_LONG:
			case PD_RADIO:
			case PD_TOGGLE:
				valL = atol(line + len);
				if (*(long*)p->valueP != valL) {
					sprintf(expNum, "%ld", valL);
					sprintf(actNum, "%ld", *(long*)p->valueP);
					expVal = expNum;
					actVal = actNum;
					hasError = TRUE;
				}
				break;
			case PD_LIST:
			case PD_COMBOLIST:
				line += len;
				if (p->control == NULL) {
					break;
				}
				valL = strtol(line, &valS, 10);
				if (valS) {
					if (valS[0] == ' ') {
						valS++;
					}
				}
				else {
					valS = "";
				}
				if ((p->option & PDO_LISTINDEX) != 0) {
					if (*(long*)p->valueP != valL) {
						sprintf(expNum, "%ld", valL);
						sprintf(actNum, "%d", *(wIndex_t*)p->valueP);
						expVal = expNum;
						actVal = actNum;
						hasError = TRUE;
					}
				}
				else {
					wListGetValues((wList_p)p->control, message, sizeof message, &listContext,
						&itemContext);
					if (strcasecmp(message, valS) != 0) {
						expVal = valS;
						actVal = message;
						hasError = TRUE;
					}
				}
				break;
			case PD_COLORLIST:
				break;
			case PD_FLOAT:
				valF = atof(line + len);
				diffF = fabs(*(FLOAT_T*)p->valueP - valF);
				if (diffF > 0.001) {
					sprintf(expNum, "%0.3f", valF);
					sprintf(actNum, "%0.3f", *(FLOAT_T*)p->valueP);
					expVal = expNum;
					actVal = actNum;
					hasError = TRUE;
				}
				break;
			case PD_STRING:
				line += len;
				while (*line == ' ') { line++; }
				wEntryGetValue((wEntry_p)p->control);
				if (strcasecmp(line, (char*)p->valueP) != 0) {
					expVal = line;
					actVal = (char*)p->valueP;
					hasError = TRUE;
				}
				break;
			case PD_DRAW:
			case PD_MESSAGE:
			case PD_TEXT:
			case PD_MENU:
			case PD_MENUITEM:
			case PD_BITMAP:
				break;
			}
			if (hasError) {
				f = fopen("error.log", "a");
				if (f == NULL) {
					NoticeMessage(MSG_OPEN_FAIL, _("Continue"), NULL, "PARAMCHECK LOG",
						"error.log", strerror(errno));
				}
				else {
					/** TODO: check usage - why is file information needed here? */
					//fprintf(f, "CHECK: %s:%d: %s-%s: exp: %s, act=%s\n",
					//	paramFileName, paramLineNum, pg->nameStr, p->nameStr, expVal, actVal);
					fclose(f);
				}
				if (paramCheckShowErrors) {
					//NoticeMessage("CHECK: %d: %s-%s: exp: %s, act=%s", _("Ok"), NULL, paramLineNum,
					//	pg->nameStr, p->nameStr, expVal, actVal);
				}
				paramCheckErrorCount++;
			}
			return;
		}
	}
	NoticeMessage("Unknown PARAMCHECK: %s", _("Ok"), NULL, line);
}


EXPORT void
ParamTurnOffDelays(bool disable)
{
	disablePlaybackDelays = disable;
}

void SimulateButtonClick(wButton_p control)
{
	if (!disablePlaybackDelays && control) {
		wButtonSetBusy(control, TRUE);
		wFlush();
		wPause(500);
		wButtonSetBusy(control, FALSE);
		wFlush();
	}
}

void ParamInit(void)
{
	if (paramGroups_init) { return; }

	AddPlaybackProc("PARAMETER", ParamPlayback, NULL);
	AddPlaybackProc("PARAMCHECK", ParamCheck, NULL);

	log_paramLayout = LogFindIndex("paramlayout");
	log_paraminput = LogFindIndex("paraminput");

	DYNARR_INIT(paramGroup_p, paramGroups_da);

	paramGroups_init = TRUE;
}

