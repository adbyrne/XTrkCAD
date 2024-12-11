/**
 * \file   defaultvalues.c
 * \brief  Save / Restore default values for dialog items
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

#define HASDEFAULT(p) (p->valueP && !(p->option&PDO_NOPREF))

static void
DefaultFromListIndex(const char *section, const char *namePrimary, const char *sectionAlt, const char *nameAlt,  paramData_t*p)
{
	long value;

	if (!wPrefGetInteger(section, namePrimary, &value, *(long *)p->valueP)) {
		wPrefGetInteger(sectionAlt, nameAlt, &value, value);
	}
	if (p->control) {
		wListSetIndex(p->control, (wIndex_t)value);
	}
	*(long *)p->valueP = value;
}

static void
DefaultFromListValue(const char* section, const char* namePrimary, const char* sectionAlt, const char* nameAlt, paramData_t* p)
{
	char* cp;

	cp = wPrefGetString(prefSect, namePrimary);
	if (!cp) {
		cp = wPrefGetString(sectionAlt, nameAlt);
	}
	if (p->control && cp) {
		*(wIndex_t*)p->valueP = wListFindValue(p->control, cp);
	}
}

void
FormSetDefaultValues(paramGroup_p pg)
{
	for (int i = 0; i < (pg->paramCnt); i++) {
		paramData_t* p = (pg->paramPtr)+i;

		if (HASDEFAULT(p)) {
			char prefNamePrimary[STR_SHORT_SIZE];
			const char* prefSectAlternative, * prefNameAlternative;
			char* cp;

			snprintf(prefNamePrimary, sizeof(prefNamePrimary), "%s-%s", pg->nameStr, p->nameStr);

			prefSectAlternative = prefSect;
			prefNameAlternative = prefNamePrimary;
			if ((p->option & PDO_MISC)) { /** \todo PDO_MISC and PGO_PREFDRAWGROUP are unused */
				prefSectAlternative = "misc";
				prefNameAlternative = p->nameStr;
			}
			else if ((p->option & PDO_DRAW)) {
				prefSectAlternative = "draw";
				prefNameAlternative = p->nameStr;
			}
			else if ((p->option & PDO_FILE)) {
				prefSectAlternative = "file";
				prefNameAlternative = p->nameStr;
			}
			else if ((pg->options & PGO_PREFGROUP)) {
				prefSectAlternative = pg->nameStr;
				prefNameAlternative = p->nameStr;
			}
			else if ((pg->options & PGO_PREFMISC)) {
				prefSectAlternative = "misc";
				prefNameAlternative = p->nameStr;
			}
			else if ((pg->options & PGO_PREFMISCGROUP)) {
				prefSectAlternative = "misc";
			}
			else if ((pg->options & PGO_PREFDRAWGROUP)) {
				prefSectAlternative = "draw";
			}

			cp = strchr(p->nameStr, '\t');
			if (cp) {
				LogPrintf("UNSUPPORTED: Parameter type has tab in name %s\n", p->nameStr);
			//	/* *cp++ = 0; */
			//	prefSectAlternative = cp;
			//	cp = strchr(cp, '\t');
			//	if (cp) {
			//		/* *cp++ = 0; */
			//		prefNameAlternative = cp;
			//	}
			}

			switch (p->type) {
			case PD_RADIO:
			case PD_TOGGLE:
				if (!wPrefGetInteger(pg->nameStr, p->nameStr, p->valueP, *(long*)p->valueP)) {
					wPrefGetInteger(prefSectAlternative, prefNameAlternative, p->valueP, *(long*)p->valueP);
				}
				break;
			case PD_LIST:
			case PD_COMBOLIST:
				if ((p->option & PDO_LISTINDEX)) {
					DefaultFromListIndex(prefSect, prefNamePrimary, prefSectAlternative, prefNameAlternative, p);
				}
				else {
					DefaultFromListValue(prefSect, prefNamePrimary, prefSectAlternative, prefNameAlternative, p);
				}
				break;
			case PD_COLORLIST:
				if (!wPrefGetInteger(prefSect, prefNamePrimary, p->valueP, *(long*)p->valueP)) {
					wPrefGetInteger(prefSectAlternative, prefNameAlternative, p->valueP, *(long*)p->valueP);
				}
				break;
			case PD_LONG:
			case PD_SCALE:
				if (!wPrefGetInteger(prefSect, prefNamePrimary, p->valueP, *(long*)p->valueP)) {
					wPrefGetInteger(prefSectAlternative, prefNameAlternative, p->valueP, *(long*)p->valueP);
				}
				break;
			case PD_FLOAT:
				if (!wPrefGetFloat(prefSect, prefNamePrimary, (FLOAT_T*)p->valueP, *(FLOAT_T*)p->valueP)) {
					wPrefGetFloat(prefSectAlternative, prefNameAlternative, (FLOAT_T*)p->valueP, *(FLOAT_T*)p->valueP);
				}
				break;
			case PD_STRING:
				cp = wPrefGetString(prefSect, prefNamePrimary);
				if (!cp) {
					wPrefGetString(prefSectAlternative, prefNameAlternative);
				}
				if (cp) {
					strcpy(p->valueP, cp);
				}
				else {
					((char*)p->valueP)[0] = '\0';
				}
				break;
			case PD_MESSAGE:
			case PD_BUTTON:
			case PD_DRAW:
			case PD_TEXT:
			case PD_MENU:
			case PD_MENUITEM:
			case PD_BITMAP:
				break;
			}
		}
	}
}

void
FormSaveDefaultValues(paramGroup_p pg)
{

	for (int i = 0; i < (pg->paramCnt); i++) {
		paramData_p p = (pg->paramPtr) + i;
		char prefNamePrimary[STR_SHORT_SIZE];

		if (p->valueP == NULL || p->nameStr == NULL ) {
			continue;
		}
		if ((p->option & PDO_DLGIGNORE)|| (p->option & PDO_NOPREF)) {
			continue;
		}
		snprintf(prefNamePrimary, sizeof(prefNamePrimary), "%s-%s", pg->nameStr, p->nameStr);

		switch (p->type) {
		case PD_LONG:
		case PD_RADIO:
		case PD_TOGGLE:
		case PD_COLORLIST:
			wPrefSetInteger(pg->nameStr, p->nameStr, *(long*)p->valueP);
			break;
		case PD_LIST:
			//listDataP = (paramListData_t*)p->winData;
			//if (p->control && listDataP->colCnt > 0) {
			//	if (maxColCnt < listDataP->colCnt) {
			//		if (maxColCnt == 0) {
			//			colWidths = (wWinPix_t*)MyMalloc(listDataP->colCnt * sizeof * colWidths);
			//		}
			//		else {
			//			colWidths = (wWinPix_t*)MyRealloc(colWidths,
			//				listDataP->colCnt * sizeof * colWidths);
			//		}
			//		maxColCnt = listDataP->colCnt;
			//	}
			//	len = wListGetColumnWidths((wList_p)p->control, listDataP->colCnt, colWidths);
			//	cp = message;
			//	for (col = 0; col < len; col++) {
			//		sprintf(cp, "%ld ", colWidths[col]);
			//		cp += strlen(cp);
			//	}
			//	*cp = '\0';
			//	len = strlen(prefNamePrimary);
			//	strcpy(prefNamePrimary + len, "-columnwidths");
			//	wPrefSetString(prefSect, prefNamePrimary, message);
			//	prefNamePrimary[len] = '\0';
			//}
		case PD_DROPLIST:
		case PD_COMBOLIST:
			//if ((p->option & PDO_LISTINDEX)) {
			//	wPrefSetInteger(prefSect, prefNamePrimary, *(wIndex_t*)p->valueP);
			//}
			//else {
			//	if (p->control) {
			//		wListGetValues((wList_p)p->control, message, sizeof message, NULL, NULL);
			//		wPrefSetString(prefSect, prefNamePrimary, message);
			//	}
			//}
			break;
		case PD_FLOAT:
		case PD_SCALE:
			wPrefSetFloat(pg->nameStr, p->nameStr, *(FLOAT_T*)p->valueP);
			break;
		case PD_STRING:
			wPrefSetString(pg->nameStr, p->nameStr, (char*)p->valueP);
			break;
		case PD_MESSAGE:
		case PD_BUTTON:
		case PD_DRAW:
		case PD_TEXT:
		case PD_MENU:
		case PD_MENUITEM:
		case PD_BITMAP:
			break;
		}
	}
	wPrefFlush(NULL);
}

