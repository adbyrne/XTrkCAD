/**
 * \file   checkinputs.c
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

#include <ctype.h>

#include <wlib.h>
#include <form.h>
#include "dynstring.h"
#include "messages.h"
#include "xtctypes.h"

#include "formprivate.h"

#define CHECKUPPERLIMIT(rangeCheck) (!(rangeCheck & PDO_NORANGECHECK_HIGH ))
#define CHECKLOWERLIMIT(rangeCheck) (!(rangeCheck & PDO_NORANGECHECK_LOW ))

static void
ShowErrorMessage(paramData_p p, const char *message)
{
	wTooltipSetText(p->control, message);
	wControlHilite(p->control, TRUE);
}

static void
ClearErrorMessage(paramData_p p)
{
	wTooltipSet(p->control, p->group->nameStr, p->nameStr);
	wControlHilite(p->control, FALSE);
}

unsigned long
FormIntegerGetValue(paramData_p data, const char *enteredValue)
{
	char* cp;
	unsigned long value;

	value = strtol(enteredValue, &cp, 10);

	while (isspace((unsigned char)*cp)) {
		cp++;
	}

	if (*cp != '\0') {
		data->bInvalid = TRUE;
		ShowErrorMessage(data, _("Invalid number"));
	} else {
		data->bInvalid = FALSE;
		ClearErrorMessage(data);
	}

	return(value);
}

wBool_t FormIntegerRangeCheck(paramData_p p, long valL)
{
	paramIntegerRange_t* irangeP = (paramIntegerRange_t*)p->winData;
	DynString errorMessage;
	bool tooHigh = false;
	bool tooLow = false;

	if (inPlayback) {
		return TRUE;
	}

	DynStringMalloc(&errorMessage, 80);

	p->bInvalid = FALSE;

	if (CHECKUPPERLIMIT(irangeP->rangechecks) && valL > irangeP->high) {
		p->bInvalid = TRUE;
		tooHigh = true;
	}

	if (CHECKLOWERLIMIT(irangeP->rangechecks) && valL < irangeP->low) {
		p->bInvalid = TRUE;
		tooLow = true;
	}

	if (tooHigh && tooLow) {
		DynStringPrintf(&errorMessage, _("Enter a value between %ld and %ld"),
		                irangeP->low,
		                irangeP->high);
	} else {
		if (tooHigh) {
			DynStringPrintf(&errorMessage, _("Enter a value < %ld"), irangeP->high);
		}
		if (tooLow) {
			DynStringPrintf(&errorMessage, _("Enter a value > %ld"), irangeP->low);
		}
	}

	if (p->bInvalid) {
		ShowErrorMessage(p, DynStringToCStr(&errorMessage));
	} else {
		ClearErrorMessage(p);
	}

	DynStringFree(&errorMessage);
	return(!p->bInvalid);
}

static FLOAT_T FloatGetValue(const char *enteredValue,	BOOL_T* validP)
{
	FLOAT_T valF;
	char* cp2;

	while (isspace((unsigned char)*enteredValue)) { enteredValue++; }
	if (*enteredValue) {
		valF = strtod(enteredValue, &cp2);
		if (*cp2 != 0) {
			*validP = FALSE;
			return 0.0;
		}
		*validP = TRUE;
		return valF;
	} else {
		*validP = TRUE;
		return 0.0;
	}
}


FLOAT_T FormFloatGetValue(paramData_p data, const char *enteredValue)
{
	FLOAT_T value;
	wBool_t valid;

	if (data->option & PDO_DIM) {
		value = FormDecodeDistance(enteredValue, &valid);
		if (!valid) {
			ShowErrorMessage(data, FormGetParseError());
		}
		/**
		 * \todo The original version set the entry field to a properly formated value incl. scale conversion.
		 * This was removed from DecodeDistance for clarity but could be put here if it makes sense
		 */
	} else {
		value = FloatGetValue(enteredValue, &valid);
		if (!valid) {
			ShowErrorMessage(data, _("Invalid Number"));
		} else {
			if (data->option & PDO_ANGLE) {
				value = NormalizeAngle((angleSystem == ANGLE_POLAR) ? value : -value);
			}
		}
	}


	data->bInvalid = !valid;
	return(value);
}


wBool_t FormFloatRangeCheck(paramData_p p, FLOAT_T valF)
{
	paramFloatRange_t* frangeP = (paramFloatRange_t*)p->winData;
	DynString message;
	bool tooHigh = false;
	bool tooLow = false;

	if (inPlayback) {
		return TRUE;
	}
	

	DynStringMalloc(&message, 80);

	p->bInvalid = FALSE;

	if (CHECKUPPERLIMIT(frangeP->rangechecks) && valF > frangeP->high) {
		p->bInvalid = TRUE;
		tooHigh = true;
	}

	if (CHECKLOWERLIMIT(frangeP->rangechecks) && valF < frangeP->low) {
		p->bInvalid = TRUE;
		tooLow = true;
	}

	if (tooHigh && tooLow) {
		DynStringPrintf(&message, _("Enter a value between %s and %s"),
		                (p->option & PDO_DIM) ? FormatDistance(frangeP->low) : FormatFloat(
		                        frangeP->low),
		                (p->option & PDO_DIM) ? FormatDistance(frangeP->high) : FormatFloat(
		                        frangeP->high));
	} else {
		if (tooHigh) {
			DynStringPrintf(&message, _("Enter a value < %s"),
			                (p->option & PDO_DIM) ? FormatDistance(frangeP->high) : FormatFloat(
			                        frangeP->high));
		}
		if (tooLow) {
			DynStringPrintf(&message, _("Enter a value > %s"),
			                (p->option & PDO_DIM) ? FormatDistance(frangeP->low) : FormatFloat(
			                        frangeP->low));
		}
	}

	if (p->bInvalid) {
		wTooltipSetText(p->control, DynStringToCStr(&message));
	} else {
		wTooltipSet(p->control, p->group->nameStr, p->nameStr);
	}

	wControlHilite(p->control, p->bInvalid);
	DynStringFree(&message);
	return(!p->bInvalid);
}

static void strip_whitespace(char* str)
{
	size_t len = strlen(str);
	if (len == 0) { return; }

	char* end = str + len - 1;

	// Move backward while the character is whitespace
	while (end >= str && isspace((unsigned char)*end)) {
		end--;
	}

	// Write the null terminator just after the last non-whitespace character
	*(end + 1) = '\0';
}

/**
 * Check for valid string: .
 *
 * \param data	parameter definition
 * \param value	entered string
 * \return false if invalid, true if ok
 */
wBool_t FormStringCheckValue(paramData_p data, char * value)
{
	DynString message;
	DynStringMalloc(&message, 80);

	data->bInvalid = FALSE;

	strip_whitespace(value);

	
	if ( (!inPlayback) && (data->option & PDO_NOTBLANK) && value[0] == '\0') {
		DynStringPrintf(&message, "%s", _("String cannot be blank"));
		data->bInvalid = TRUE;
	} else {
		if ((data->option & PDO_NOPSHUPD) == 0 && data->valueP) {
			if (strlen(value) > data->max_string - 1) {
				DynStringPrintf(&message, "String is too long, Max length is % u",
				                data->max_string - 1);
				data->bInvalid = TRUE;
			}
		}
	}

	if (data->bInvalid) {
		wTooltipSetText(data->control, DynStringToCStr(&message));
	} else {
		wTooltipSet(data->control, data->group->nameStr, data->nameStr);
	}

	wControlHilite(data->control, data->bInvalid);

	DynStringFree(&message);
	return(!data->bInvalid);
}

void
FormStringGetValue(paramData_p data, char* value)
{
	strncpy((char*)data->valueP, value, data->max_string - 1);
	((char*)data->valueP)[data->max_string - 1] = '\0';
}

wBool_t FormCheckInputs(
        paramGroup_p group,
        wControl_p button)
{
	wBool_t bInvalid = FALSE;
	// Check for invalid entries
	for (int i = 0; i < (group->paramCnt); i++) {
		paramData_p p = (group->paramPtr) + i;
		if(p->control) 
		{
			wControlHilite(p->control, p->bInvalid );

			if (p->bInvalid && p->bShown) {
				LOG(log_form, 1, ("   %s: Invalid\n", p->nameStr));
				bInvalid = TRUE;
			}
		}
	}

	if (bInvalid) {
		// At least 1 invalid entry
		LOG(log_form, 1, ("  Group %s Invalid\n", group->nameStr));
		wTooltipSetText(button,
		                _("Invalid input(s), please correct the hilighted field(s)"));
		wFlush();
		return FALSE;
	}
	return TRUE;
}


long FormUpdate(
	paramGroup_p pg)
{
	long longV;
	FLOAT_T floatV;
	const char* stringV;
	wDrawColor dc;
	long change = 0;
	int inx;
	paramData_p p;
	BOOL_T valid;

	for (p = pg->paramPtr, inx = 0; p < &pg->paramPtr[pg->paramCnt]; p++, inx++) {
		if ((p->option & PDO_DLGIGNORE) != 0) {
			continue;
		}
		if (p->control == NULL) {
			continue;
		}
		if (p->bInvalid == TRUE) {
			break;
		}
		switch (p->type) {
		case PD_LONG:
			stringV = wEntryGetValue(p->control);
			longV = atol(stringV);
			if (!FormIntegerRangeCheck(p, longV)) {
				break;
			}
			if (longV != p->oldD.l) {
				p->oldD.l = longV;
				if (p->valueP) {
					*(long*)p->valueP = longV;
				}
				if ((p->option & PDO_NOUPDACT) == 0 && pg->changeProc) {
					pg->changeProc(pg, inx, &longV);
				}
				change |= (1L << inx);
			}
			break;
		case PD_RADIO:
			longV = wRadioGetValue(p->control);
			if (longV != p->oldD.l) {
				p->oldD.l = longV;
				if (p->valueP) {
					*(long*)p->valueP = longV;
				}
				if ((p->option & PDO_NOUPDACT) == 0 && pg->changeProc) {
					pg->changeProc(pg, inx, &longV);
				}
				change |= (1L << inx);
			}
			break;
		case PD_TOGGLE:
			longV = wToggleGetValue(p->control);
			if (longV != p->oldD.l) {
				p->oldD.l = longV;
				if (p->valueP) {
					*(long*)p->valueP = longV;
				}
				if ((p->option & PDO_NOUPDACT) == 0 && pg->changeProc) {
					pg->changeProc(pg, inx, &longV);
				}
				change |= (1L << inx);
			}
			break;
		case PD_LIST:
		case PD_DROPLIST:
		case PD_COMBOLIST:
			longV = wListGetIndex(p->control);
			if (longV != p->oldD.l) {
				p->oldD.l = longV;
				if (p->valueP) {
					*(wIndex_t*)p->valueP = (wIndex_t)longV;
				}
				if ((p->option & PDO_NOUPDACT) == 0 && pg->changeProc) {
					pg->changeProc(pg, inx, &longV);
				}
				change |= (1L << inx);
			}
			break;
		case PD_COLORLIST:
			dc = wColorSelectButtonGetColor(p->control);
			if (dc != p->oldD.dc) {
				p->oldD.dc = dc;
				if (p->valueP) {
					*(wDrawColor*)p->valueP = dc;
				}
				if ((p->option & PDO_NOUPDACT) == 0 && pg->changeProc) {
					pg->changeProc(pg, inx, &longV); /* COLORNOP */
				}
				change |= (1L << inx);
			}
			break;
		case PD_FLOAT:
			stringV = wEntryGetValue(p->control);
			if (p->option & PDO_DIM) {
				floatV = FormDecodeDistance(stringV, &valid);
			}
			else {
				floatV = FormDecodeFloat(stringV, &valid);
				if (valid && (p->option & PDO_ANGLE)) {
					floatV = NormalizeAngle((angleSystem == ANGLE_POLAR) ? floatV : -floatV);
				}
			}
			if (!valid) {
				break;
			}
			if (!FormFloatRangeCheck(p, floatV)) {
				break;
			}
			if (floatV != p->oldD.f) {
				p->oldD.f = floatV;
				if (p->valueP) {
					*(FLOAT_T*)p->valueP = floatV;
				}
				if ((p->option & PDO_NOUPDACT) == 0 && pg->changeProc) {
					pg->changeProc(pg, inx, &floatV);
				}
				change |= (1L << inx);
			}
			break;
		case PD_STRING:
			stringV = wEntryGetValue(p->control);
			if ((p->option & PDO_NOTBLANK) && stringV[0] == '\0') {
				p->bInvalid = TRUE;
				wTooltipSetText(p->control, _("String cannot be blank"));
        		wControlHilite(p->control, TRUE);
				break;
			}

			p->bInvalid = FALSE;
			wTooltipSet(p->control, p->group->nameStr, p->nameStr); 
			wControlHilite(p->control, FALSE);
			
			if (strcmp(stringV, p->oldD.s) != 0) {
				if (p->oldD.s) {
					MyFree(p->oldD.s);
				}
				p->oldD.s = MyStrdup(stringV);
				if (p->valueP) {
					strncpy((char*)p->valueP, stringV, p->max_string - 1);
					((char*)p->valueP)[p->max_string - 1] = '\0';
					if (strlen(stringV) > p->max_string - 1) {
						NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED, _("Ok"), NULL, p->max_string - 1);
					}
				}

				if ((p->option & PDO_NOUPDACT) == 0 && pg->changeProc)
					// CAST_AWAY_CONST: param 3 should be const but its a big change
				{
					pg->changeProc(pg, inx, CAST_AWAY_CONST stringV);
				}
				change |= (1L << inx);
			}
			break;
		case PD_MESSAGE:
		case PD_BUTTON:
		case PD_DRAW:
		case PD_TEXT:
		case PD_MENU:
		case PD_MENUITEM:
		case PD_BITMAP:
		case PD_TAG:
		case PD_SCALE:
		case PD_NOTEBOOK:
			break;
		}
	}

	return change;
}



