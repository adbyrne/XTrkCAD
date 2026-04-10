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

#define HASDEFAULT(p) (p->nameStr && p->valueP && !(p->option&PDO_NOPREF))

static void
DefaultFromListIndex(const char *section, const char *namePrimary,
                     const char *sectionAlt, const char *nameAlt,  paramData_t*p)
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
SaveListColumnWidths(char *section, paramData_p listData)
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
		wPrefSetString(section, "columnwidths", DynStringToCStr(&columnWidthString));

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
				if (!wPrefGetInteger(pg->nameStr, p->nameStr, p->valueP, *(long*)p->valueP)) {
					wPrefGetInteger(prefSectAlternative, DynStringToCStr(&prefNameAlternative),
					                p->valueP, *(long*)p->valueP);
				}
				break;
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
				break;
			}
		}
		DynStringFree(&prefNameAlternative);
	}
}

void
FormSaveDefaultValues(paramGroup_p pg)
{

	for (int i = 0; i < (pg->paramCnt); i++) {
		paramData_p p = (pg->paramPtr) + i;
		char prefNamePrimary[STR_SHORT_SIZE];

		if (/*p->valueP == NULL || */p->nameStr ==
		                             NULL) { /** \todo check for valueP == NULL */
			continue;
		}
		if ((p->option & PDO_DLGIGNORE)|| (p->option & PDO_NOPREF)) {
			continue;
		}
		snprintf(prefNamePrimary, sizeof(prefNamePrimary), "%s-%s", pg->nameStr,
		         p->nameStr);

		switch (p->type) {
		case PD_LONG:
		case PD_RADIO:
		case PD_TOGGLE:
		case PD_COLORLIST:
			wPrefSetInteger(pg->nameStr, p->nameStr, *(long*)p->valueP);
			break;
		case PD_LIST:
			SaveListColumnWidths(pg->nameStr, p);
		//no break!
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
		case PD_NOTEBOOK:
		case PD_TAG:
			break;
		}
	}
	wPrefFlush(NULL);
}


static const char * PREFSECT = "DialogItem";
EXPORT void FormUpdatePrefs( void )
{
	paramData_p p;
	long rgb;
	char prefName[STR_SHORT_SIZE];
	size_t len;
	int col;
	char * cp;
	static wWinPix_t * colWidths;
	static int maxColCnt = 0;
	paramListData_t * listDataP;

	for ( paramGroup_cp * ppg = DialogGroupIter(NULL); ppg;
	      ppg = DialogGroupIter(ppg) ) {
		//pg = paramGroups(inx);
		if ((*ppg)->nameStr == NULL) { continue; }
		for ( p=(*ppg)->paramPtr; p<&(*ppg)->paramPtr[(*ppg)->paramCnt]; p++ ) {
			if (p->valueP == NULL || p->nameStr == NULL || (p->option&PDO_NOPREF)!=0 ) {
				continue;
			}
			if ( (p->option&PDO_DLGIGNORE) != 0 ) {
				continue;
			}
			snprintf( prefName, sizeof(prefName), "%s-%s", (*ppg)->nameStr, p->nameStr );
			switch ( p->type ) {
			case PD_LONG:
			case PD_RADIO:
			case PD_TOGGLE:
				wPrefSetInteger( PREFSECT, prefName, *(long*)p->valueP );
				break;
			case PD_LIST:
				listDataP = (paramListData_t*)p->winData;
				if ( p->control && listDataP->colCnt > 0 ) {
					if ( maxColCnt < listDataP->colCnt ) {
						if ( maxColCnt == 0 ) {
							colWidths = (wWinPix_t*)MyMalloc( listDataP->colCnt * sizeof * colWidths );
						} else {
							colWidths = (wWinPix_t*)MyRealloc( colWidths,
							                                   listDataP->colCnt * sizeof * colWidths );
						}
						maxColCnt = listDataP->colCnt;
					}
					len = wListGetColumnWidths( (wList_p)p->control, listDataP->colCnt, colWidths );
					cp = message;
					for ( col=0; col<len; col++ ) {
						sprintf( cp, "%ld ", colWidths[col] );
						cp += strlen(cp);
					}
					*cp = '\0';
					len = strlen( prefName );
					strcpy( prefName+len, "-columnwidths" );
					wPrefSetString( PREFSECT, prefName, message );
					prefName[len] = '\0';
				}
			case PD_DROPLIST:
			case PD_COMBOLIST:
				if ( (p->option&PDO_LISTINDEX) ) {
					wPrefSetInteger( PREFSECT, prefName, *(wIndex_t*)p->valueP );
				} else {
					if (p->control) {
						wListGetValues( (wList_p)p->control, message, sizeof message, NULL, NULL );
						wPrefSetString( PREFSECT, prefName, message );
					}
				}
				break;
			case PD_COLORLIST:
				rgb = wDrawGetRGB( *(wDrawColor*)p->valueP );
				wPrefSetInteger( PREFSECT, prefName, rgb );
				break;
			case PD_FLOAT:
				wPrefSetFloat( PREFSECT, prefName, *(FLOAT_T*)p->valueP );
				break;
			case PD_STRING:
				wPrefSetString( PREFSECT, prefName, (char*)p->valueP );
				break;
			case PD_MESSAGE:
			case PD_BUTTON:
			case PD_DRAW:
			case PD_TEXT:
			case PD_MENU:
			case PD_MENUITEM:
			case PD_BITMAP:
			case PD_SCALE:
			case PD_TAG:
			case PD_NOTEBOOK:
				break;
			}
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


