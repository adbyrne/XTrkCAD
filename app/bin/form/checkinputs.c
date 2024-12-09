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

#include "formprivate.h"


wBool_t FormIntegerRangeCheck(paramData_p p, long valL)
{
	//if (paramPlayback) {
	//	return TRUE;
	//}
	paramIntegerRange_t* irangeP = (paramIntegerRange_t*)p->winData;

	if (((irangeP->rangechecks & PDO_NORANGECHECK_HIGH) == 0
		&& valL > irangeP->high) ||
		((irangeP->rangechecks & PDO_NORANGECHECK_LOW) == 0 && valL < irangeP->low)) {
		if ((irangeP->rangechecks & (PDO_NORANGECHECK_HIGH | PDO_NORANGECHECK_LOW)) ==
			PDO_NORANGECHECK_HIGH) {
			sprintf(message, _("Enter a value > %ld"), irangeP->low);
		}
		else if ((irangeP->rangechecks & (PDO_NORANGECHECK_HIGH | PDO_NORANGECHECK_LOW))
			== PDO_NORANGECHECK_LOW) {
			sprintf(message, _("Enter a value < %ld"), irangeP->high);
		}
		else {
			sprintf(message, _("Enter a value between %ld and %ld"), irangeP->low,
				irangeP->high);
		}
		//wWinPix_t h = wControlGetHeight(p->control);
		//wControlSetBalloon(p->control, 0, -h * 3 / 4, message);
		p->bInvalid = TRUE;
		wControlHilite(p->control, p->bInvalid);
		return FALSE;
	}
	p->bInvalid = FALSE;
	return TRUE;
}



wBool_t FormFloatRangeCheck(paramData_p p, FLOAT_T valF)
{
	//if (paramPlayback) {
	//	return TRUE;
	//}
	paramFloatRange_t* frangeP = (paramFloatRange_t*)p->winData;
	//	wBool_t bInvalid = p->bInvalid;
	if (((frangeP->rangechecks & PDO_NORANGECHECK_HIGH) == 0
		&& valF > frangeP->high) ||
		((frangeP->rangechecks & PDO_NORANGECHECK_LOW) == 0 && valF < frangeP->low)) {
		if ((frangeP->rangechecks & (PDO_NORANGECHECK_HIGH | PDO_NORANGECHECK_LOW)) ==
			PDO_NORANGECHECK_HIGH)
			sprintf(message, _("Enter a value > %s"),
				(p->option & PDO_DIM) ? FormatDistance(frangeP->low) : FormatFloat(frangeP->low));
		else if ((frangeP->rangechecks & (PDO_NORANGECHECK_HIGH | PDO_NORANGECHECK_LOW)) ==
			PDO_NORANGECHECK_LOW)
			sprintf(message, _("Enter a value < %s"),
				(p->option & PDO_DIM) ? FormatDistance(frangeP->high) : FormatFloat(frangeP->high));
		else
			sprintf(message, _("Enter a value between %s and %s"),
				(p->option & PDO_DIM) ? FormatDistance(frangeP->low) : FormatFloat(frangeP->low),
				(p->option & PDO_DIM) ? FormatDistance(frangeP->high) : FormatFloat(frangeP->high));
		/** \todo Need to find a solution forshowing error messages in dialogs */
		//wWinPix_t h = wControlGetHeight(p->control);
		//wControlSetBalloon(p->control, 0, -h * 3 / 4, message);
		p->bInvalid = TRUE;
		wControlHilite( p->control, p->bInvalid);
		return FALSE;
	}
	p->bInvalid = FALSE;
	return TRUE;
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
		wControlSetBalloonText(button, _("Invalid input(s), please correct the hilighted field(s)"));
		wFlush();
		return FALSE;
	}
	return TRUE;
}

