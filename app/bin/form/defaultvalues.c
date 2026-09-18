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

#include <limits.h>
#include <wlib.h>

#include <form.h>
#include <dynstring.h>
#include "formprivate.h"

#define HASDEFAULT(p) ((p)->nameStr && (p)->valueP && !((p)->option&PDO_NOPREF))

static void
DefaultFromListIndex(const char *section, const char *namePrimary,
                     const char *sectionAlt, const char *nameAlt,  paramData_t*p)
{
	long value;

	if (!wPrefGetInteger(section, namePrimary, &value, *(wIndex_t *)p->valueP)) {
		wPrefGetInteger(sectionAlt, nameAlt, &value, value);
	}
	if (p->control) {
		wListSetIndex(p->control, (wIndex_t)value);
	}
	*(wIndex_t *)p->valueP = (wIndex_t)value;
}

static void
DefaultFromListValue(const char* section, const char* namePrimary,
                     const char* sectionAlt, const char* nameAlt, paramData_t* p)
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

// see https://stackoverflow.com/questions/8257714/how-can-i-convert-an-int-to-a-string-in-c
// for explanation of the following macro
#define INT_DECIMAL_STRING_SIZE(int_type) ((CHAR_BIT*sizeof(int_type)-1)*10/33+3)

static void
FormatWidthsList(unsigned count, wWinPix_t* widths, DynString *output)
{
	DynString formattedWidths;

	DynStringMalloc(&formattedWidths, 20);

	for (unsigned int col = 0; col < count; col++) {
		char buffer[INT_DECIMAL_STRING_SIZE(wWinPix_t) + sizeof(' ')];

		sprintf(buffer, "%ld ", widths[col]);

		DynStringCatCStr(output, buffer);
	}

	return;
}

static void
SaveListColumnWidths(const char *section, const char *key, paramData_p listData)
{
#ifdef TODO_UNUSED
	paramListData_t *listDataP = (paramListData_t*)listData->winData;
#endif

	if (listData->control) {
		DynString columnWidthString;
		wWinPix_t* colWidths = NULL;
		unsigned int count = wListGetColumnCount(listData->control);

		colWidths = (wWinPix_t*)MyMalloc(count * sizeof( wWinPix_t));
		wListGetColumnWidths(listData->control, count, colWidths);

		DynStringMalloc(&columnWidthString, 20);

		FormatWidthsList(count, colWidths, &columnWidthString);
		wPrefSetString(section, key, DynStringToCStr(&columnWidthString));

		DynStringFree(&columnWidthString);
		MyFree(colWidths);
	}
}

static void
CopyCStringtoDynString(DynString* destination, const char* cstring)
{
	DynStringClear(destination);
	DynStringCatCStr(destination, cstring);
}

void
FormLoadDefaultValues(paramGroup_p pg)
{
	for (int i = 0; i < (pg->paramCnt); i++) {
		DynString prefNameAlternative;
		DynStringMalloc(&prefNameAlternative, STR_SHORT_SIZE);

		paramData_t* p = (pg->paramPtr)+i;

		if (HASDEFAULT(p)) {
			const char* prefSectAlternative;
			char* cp;

			DynStringPrintf(&prefNameAlternative, "%s-%s", pg->nameStr, p->nameStr);
			prefSectAlternative = prefSect;

			if ((p->option & PDO_DRAW)) {
				prefSectAlternative = "draw";
				CopyCStringtoDynString(&prefNameAlternative, p->nameStr);
			} else if ((p->option & PDO_FILE)) {
				prefSectAlternative = "file";
				CopyCStringtoDynString(&prefNameAlternative, p->nameStr);
			}

			else if ((pg->options & PGO_PREFMISC)) {
				prefSectAlternative = "misc";
				CopyCStringtoDynString(&prefNameAlternative, p->nameStr);
			} else if ((pg->options & PGO_PREFMISCGROUP)) {
				prefSectAlternative = "misc";
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
					wPrefGetInteger(prefSectAlternative, DynStringToCStr(&prefNameAlternative),
					                p->valueP, *(long*)p->valueP);
				}
				break;
			case PD_LIST:
			/** \todo Load column widths from preferences */
			case PD_COMBOLIST:
				if ((p->option & PDO_LISTINDEX)) {
					DefaultFromListIndex(pg->nameStr, p->nameStr, prefSectAlternative,
					                     DynStringToCStr(&prefNameAlternative), p);
				} else {
					DefaultFromListValue(pg->nameStr, p->nameStr, prefSectAlternative,
					                     DynStringToCStr(&prefNameAlternative), p);
				}
				break;
			case PD_COLORLIST:
			case PD_LONG:
			case PD_SCALE:
				if (!wPrefGetInteger(pg->nameStr, p->nameStr, p->valueP, *(long*)p->valueP)) {
					wPrefGetInteger(prefSectAlternative, DynStringToCStr(&prefNameAlternative),
					                p->valueP, *(long*)p->valueP);
				}
				break;
			case PD_FLOAT:
				if (!wPrefGetFloat(pg->nameStr, p->nameStr, (FLOAT_T*)p->valueP,
				                   *(FLOAT_T*)p->valueP)) {
					wPrefGetFloat(prefSectAlternative, DynStringToCStr(&prefNameAlternative),
					              (FLOAT_T*)p->valueP, *(FLOAT_T*)p->valueP);
				}
				break;
			case PD_STRING:
				cp = wPrefGetString(pg->nameStr, p->nameStr);
				if (!cp) {
					wPrefGetString(prefSectAlternative, DynStringToCStr(&prefNameAlternative));
				}
				if (cp) {
					strcpy(p->valueP, cp);
				} else {
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
			case PD_NOTEBOOK:
			case PD_DROPLIST:
			case PD_TAG:
			case PD_EXPANDER:
				break;
			}
		}
		DynStringFree(&prefNameAlternative);
	}
}

/**
 * Write one paramData_t's current value into the preferences database
 * under the given section/key. Shared by FormSaveDefaultValues (run
 * from the Ok button for one dialog) and FormUpdatePrefs (run for every
 * registered dialog at app-lifecycle checkpoints -- quit, save,
 * save-as, load, open-example); both write into the dialog's own
 * section, so whichever trigger fires first wins and the other is a
 * harmless idempotent re-save.
 */
static void
SaveParamPref(const char *section, const char *prefName, paramData_p p)
{
	char columnWidthsKey[STR_SHORT_SIZE];

	switch (p->type) {
	case PD_LONG:
	case PD_RADIO:
	case PD_TOGGLE:
		wPrefSetInteger(section, prefName, *(long*)p->valueP);
		break;
	case PD_COLORLIST:
		wPrefSetInteger(section, prefName, wDrawGetRGB(*(wDrawColor*)p->valueP));
		break;
	case PD_LIST:
		snprintf(columnWidthsKey, sizeof(columnWidthsKey), "%s-columnwidths",
		         prefName);
		SaveListColumnWidths(section, columnWidthsKey, p);
		__attribute__((fallthrough));
	case PD_DROPLIST:
	case PD_COMBOLIST:
		if ((p->option & PDO_LISTINDEX)) {
			wPrefSetInteger(section, prefName, *(wIndex_t*)p->valueP);
		} else if (p->control) {
			wListGetValues((wList_p)p->control, message, sizeof message, NULL, NULL);
			wPrefSetString(section, prefName, message);
		}
		break;
	case PD_FLOAT:
	case PD_SCALE:
		wPrefSetFloat(section, prefName, *(FLOAT_T*)p->valueP);
		break;
	case PD_STRING:
		wPrefSetString(section, prefName, (char*)p->valueP);
		break;
	case PD_MESSAGE:
	case PD_BUTTON:
	case PD_DRAW:
	case PD_TEXT:
	case PD_MENU:
	case PD_MENUITEM:
	case PD_BITMAP:
	case PD_NOTEBOOK:
	case PD_TAG:
	case PD_EXPANDER:
		break;
	}
}

void
FormSaveDefaultValues(paramGroup_p pg)
{

	for (int i = 0; i < (pg->paramCnt); i++) {
		paramData_p p = (pg->paramPtr) + i;

		if (p->valueP == NULL || p->nameStr == NULL) {
			continue;
		}
		if ((p->option & PDO_DLGIGNORE)|| (p->option & PDO_NOPREF)) {
			continue;
		}

		SaveParamPref(pg->nameStr, p->nameStr, p);
	}
	wPrefFlush(NULL);
}


EXPORT void FormUpdatePrefs( void )
{
	paramData_p p;

	for ( paramGroup_cp * ppg = DialogGroupIter(NULL); ppg;
	      ppg = DialogGroupIter(ppg) ) {
		if ((*ppg)->nameStr == NULL) { continue; }
		for ( p=(*ppg)->paramPtr; p<&(*ppg)->paramPtr[(*ppg)->paramCnt]; p++ ) {
			if (p->valueP == NULL || p->nameStr == NULL || (p->option&PDO_NOPREF)!=0 ) {
				continue;
			}
			if ( (p->option&PDO_DLGIGNORE) != 0 ) {
				continue;
			}
			SaveParamPref((*ppg)->nameStr, p->nameStr, p);
		}
	}
}

EXPORT void FormResetInvalid(
        wControl_p win )
{
	for ( paramGroup_cp * ppg = DialogGroupIter(NULL); ppg;
	      ppg = DialogGroupIter(ppg) ) {
		if ( (*ppg)->win == win ) {
//			LOG( log_paraminput, 1, ( "Reset Invalid: %s\n", (*ppg)->nameStr ) );
			for ( paramData_p p = &(*ppg)->paramPtr[0];
			      p < &(*ppg)->paramPtr[(*ppg)->paramCnt]; p++ ) {
//				if ( p->bInvalid )
//					LOG( log_paraminput, 1, ( "  %s Invalid\n", p->nameStr ) );
				FormHilite( win, p->control, FALSE );
//				wControlSetBalloon( p->control, 0, 0, NULL );
				p->bInvalid = FALSE;
			}
			break;
		}
	}
}


