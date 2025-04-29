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

#include <wlib.h>
#include <form.h>
#include "dynstring.h"
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
	}
	else {
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

	//if (paramPlayback) {
	//	return TRUE;
	//}

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
	}
	else {
		if (tooHigh) {
			DynStringPrintf(&errorMessage, _("Enter a value < %ld"), irangeP->high);
		}
		if (tooLow) {
			DynStringPrintf(&errorMessage, _("Enter a value > %ld"), irangeP->low);
		}
	}

	if (p->bInvalid) {
		ShowErrorMessage(p, DynStringToCStr(&errorMessage));
	}
	else {
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
	}
	else {
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
		}
		else {
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

	//if (paramPlayback) {
	//	return TRUE;
	//}

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
			(p->option & PDO_DIM) ? FormatDistance(frangeP->low) : FormatFloat(frangeP->low),
			(p->option & PDO_DIM) ? FormatDistance(frangeP->high) : FormatFloat(frangeP->high));
	}
	else {
		if (tooHigh) {
			DynStringPrintf(&message, _("Enter a value < %s"), 
				(p->option & PDO_DIM) ? FormatDistance(frangeP->high) : FormatFloat(frangeP->high));
		}
		if (tooLow) {
			DynStringPrintf(&message, _("Enter a value > %s"), 
				(p->option & PDO_DIM) ? FormatDistance(frangeP->low) : FormatFloat(frangeP->low));
		}
	}

	if (p->bInvalid) {
		wTooltipSetText(p->control, DynStringToCStr(&message));
	}
	else {
		wTooltipSet(p->control, p->group->nameStr, p->nameStr);
	}

	wControlHilite(p->control, p->bInvalid);
	DynStringFree(&message);
	return(!p->bInvalid);
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

	if ((data->option & PDO_NOTBLANK) && value[0] == '\0') {
		DynStringPrintf(&message, "%s", _("String cannot be blank"));
		data->bInvalid = TRUE;
	}
	else {
		if ((data->option & PDO_NOPSHUPD) == 0 && data->valueP) {
			if (strlen(value) > data->max_string - 1) {
				DynStringPrintf(&message, "String is too long, Max length is % u", data->max_string - 1);
				data->bInvalid = TRUE;
			}
		}
	}

	if (data->bInvalid) {
		wTooltipSetText(data->control, DynStringToCStr(&message));
	}
	else {
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
		wControlHilite(p->control, p->bInvalid );

		if (p->bInvalid && p->bShown) {
			LOG(log_form, 1, ("   %s: Invalid\n", p->nameStr));
			bInvalid = TRUE;
		}
	}

	if (bInvalid) {
		// At least 1 invalid entry
		LOG(log_form, 1, ("  Group %s Invalid\n", group->nameStr));
		wTooltipSetText(button, _("Invalid input(s), please correct the hilighted field(s)"));
		wFlush();
		return FALSE;
	}
	return TRUE;
}


