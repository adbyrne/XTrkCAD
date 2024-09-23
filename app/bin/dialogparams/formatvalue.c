/**
 * \file   formatvalue.c
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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "param.h"

#define N_STRING (10)
static int formatStringInx;					//Index ahead in case of overwrite
static char formatStrings[N_STRING + 1][80];  //Add safety


char* FormatLong(
	long valL)
{
	if (++formatStringInx >= N_STRING) {
		formatStringInx = 0;
	}
	sprintf(formatStrings[formatStringInx], "%ld", valL);
	return formatStrings[formatStringInx];
}


char* FormatFloat(
	FLOAT_T valF)
{
	if (++formatStringInx >= N_STRING) {
		formatStringInx = 0;
	}
	sprintf(formatStrings[formatStringInx], "%0.3f", valF);
	return formatStrings[formatStringInx];
}


static void FormatFraction(
	char** cpp,
	BOOL_T printZero,
	int digits,
	BOOL_T rational,
	FLOAT_T valF,
	const char* unitFmt)
{
	char* cp = *cpp;
	long integ;
	long f1, f2;
	char* space = "";

	if (!rational) {
		sprintf(cp, "%0.*f", digits, valF);
		cp += strlen(cp);
	}
	else {
		integ = (long)floor(valF);
		valF -= (FLOAT_T)integ;
		for (f2 = 1; digits > 0; digits--, f2 *= 2);
		f1 = (long)floor((valF * (FLOAT_T)f2) + 0.5);
		if (f1 >= f2) {
			f1 -= f2;
			integ++;
		}
		if (integ != 0 || !printZero) {
			sprintf(cp, "%ld", integ);
			cp += strlen(cp);
			printZero = FALSE;
			space = " ";
		}
		if (f2 > 1 && f1 != 0) {
			while ((f1 & 1) == 0) { f1 /= 2; f2 /= 2; }
			sprintf(cp, "%s%ld/%ld", space, f1, f2);
			cp += strlen(cp);
		}
		else if (printZero) {
			*cp++ = '0';
			*cp = '\0';
		}
	}
	if (cp != *cpp) {
		strcpy(cp, unitFmt);
		cp += strlen(cp);
		*cpp = cp;
	}
}


char* FormatDistanceEx(
	FLOAT_T valF,
	long distanceFormat)
{
	char* cp;
	int digits;
	long feet;
	char* metricInd;

	if (++formatStringInx >= N_STRING) {
		formatStringInx = 0;
	}
	cp = formatStrings[formatStringInx];
	digits = (int)(distanceFormat & DISTFMT_DECS);
	valF = PutDim(valF);
	if (valF < 0) {
		*cp++ = '-';
		valF = -valF;
	}
	if ((distanceFormat & DISTFMT_FMT) == DISTFMT_FMT_NONE) {
		FormatFraction(&cp, FALSE, digits,
			(distanceFormat & DISTFMT_FRACT) == DISTFMT_FRACT_FRC, valF, "");
		return formatStrings[formatStringInx];
	}
	else if (units == UNITS_ENGLISH) {
		feet = (long)(floor)(valF / 12.0);
		valF -= feet * 12.0;
		if (feet != 0) {
			sprintf(cp, "%ld%s", feet,
				(distanceFormat & DISTFMT_FMT) == DISTFMT_FMT_SHRT ? "' " : "ft ");
			cp += strlen(cp);
		}
		if (feet == 0 || valF != 0) {
			FormatFraction(&cp, feet == 0, digits,
				(distanceFormat & DISTFMT_FRACT) == DISTFMT_FRACT_FRC, valF,
				(distanceFormat & DISTFMT_FMT) == DISTFMT_FMT_SHRT ? "\"" : "in");
		}
	}
	else {
		if ((distanceFormat & DISTFMT_FMT) == DISTFMT_FMT_M) {
			valF = valF / 100.0;
			metricInd = "m";
		}
		else if ((distanceFormat & DISTFMT_FMT) == DISTFMT_FMT_MM) {
			valF = valF * 10.0;
			metricInd = "mm";
		}
		else {
			metricInd = "cm";
		}
		FormatFraction(&cp, FALSE, digits,
			(distanceFormat & DISTFMT_FRACT) == DISTFMT_FRACT_FRC, valF, metricInd);
	}
	return formatStrings[formatStringInx];
}


char* FormatDistance(
	FLOAT_T valF)
{
	return FormatDistanceEx(valF, GetDistanceFormat());
}

EXPORT char* FormatSmallDistance(
	FLOAT_T valF)
{
	long format = GetDistanceFormat();
	format &= ~(DISTFMT_FRACT_FRC | DISTFMT_DECS);
	format |= 3;
	return FormatDistanceEx(valF, format);
}

