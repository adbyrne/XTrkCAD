/**
 * \file   fetchdata.c	Get data from the form and store into respective value variable
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

#include <wlib.h>

#include "formprivate.h"
#include <form.h>


static BOOL_T ShouldParseAsDistance(int option, const char* label,
                                    const char* value)
{
	// No PDO_DIM Flag -> no distance
	if ((option & PDO_DIM) == 0) {
		return FALSE;
	}

	// Special case: linewidth has negative value -> no distance
	if (label && strcmp(label, N_("linewidth")) == 0 && value && value[0] == '-') {
		return FALSE;
	}

	return TRUE;
}

void
FormFetchData(paramGroup_p pg)
{
	FLOAT_T floatV;
	const char* stringV;
	paramData_p p;
	BOOL_T valid;
	BOOL_T bDistance;
	BOOL_T inRange;

	for (p = pg->paramPtr; p < &pg->paramPtr[pg->paramCnt]; p++) {
		if ((p->option & PDO_DLGIGNORE) != 0) {
			continue;
		}

		if (p->control == NULL || p->valueP == NULL) {
			continue;
		}

		switch (p->type) {
			long longV;

		case PD_LONG:
			longV = atol(wEntryGetValue(p->control));

			if (p->winData) {
				inRange = (longV <= ((paramIntegerRange_t*)p->winData)->high) &&
				          (longV >= ((paramIntegerRange_t*)p->winData)->low);
			} else {
				inRange = TRUE;
			}

			if (inRange) {
				*(long*)p->valueP = longV;
			}

			break;

		case PD_RADIO:
			*(long*)p->valueP = wRadioGetValue(p->control);
			break;

		case PD_TOGGLE:
			*(long*)p->valueP = wToggleGetValue(p->control);
			break;

		case PD_LIST:
		case PD_DROPLIST:
		case PD_COMBOLIST:
			*(wIndex_t*)p->valueP = wListGetIndex(p->control);
			break;

		case PD_COLORLIST:
			*(wDrawColor*)p->valueP = wColorSelectButtonGetColor(p->control);
			break;

		case PD_FLOAT:
			stringV = wEntryGetValue(p->control);
			bDistance = ShouldParseAsDistance(p->option, p->nameStr, stringV );

			floatV = FormDecodeNumber(stringV, &valid, p->option, bDistance);

			if (valid && (p->option & PDO_ANGLE)) {
				floatV = NormalizeAngle((angleSystem == ANGLE_POLAR) ? floatV : -floatV);
			}

			if (p->winData) {
				inRange = (floatV <= ((paramFloatRange_t*)p->winData)->high) &&
				          (floatV >= ((paramFloatRange_t*)p->winData)->low);
			} else {
				inRange = TRUE;
			}

			if (valid && inRange) {
				*(FLOAT_T*)p->valueP = floatV;
			}
			break;

		case PD_STRING:
			stringV = wEntryGetValue(p->control);
			strncpy((char*)p->valueP, stringV, p->max_string - 1);
			((char*)p->valueP)[p->max_string - 1] = '\0';
			break;

		case PD_MESSAGE:
		case PD_BUTTON:
		case PD_DRAW:
		case PD_TEXT:
		case PD_MENU:
		case PD_MENUITEM:
		case PD_BITMAP:
		case PD_SCALE:
		case PD_NOTEBOOK:
		case PD_TAG:
		case PD_EXPANDER:
			break;
		}
	}
}
