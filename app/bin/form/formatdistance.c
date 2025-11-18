/**
 * \file   formdistance.c
 * \brief  Distance format functions, create and interpret
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

#include <form.h>
#include "formprivate.h"


static char* parseErrorMessage;

static int GetDigitStr(char** cpp, long* numP, int* lenP)
{
	char* cp = *cpp, * cq;
	size_t len;
	*numP = 0;
	if (cp == NULL) {
		parseErrorMessage = N_("Unexpected End Of String");
		return FALSE;
	}
	while (isspace((unsigned char)*cp)) { cp++; }
	*numP = strtol(cp, &cq, 10);
	if (cp == cq) {
		*cpp = cp;
		parseErrorMessage = N_("Expected digit");
		return FALSE;
	}
	len = cq - cp;
	if (lenP) {
		*lenP = (int)len;
	}
	if (len > 9) {
		parseErrorMessage = N_("Overflow");
		return FALSE;
	}
	while (isspace((unsigned char)*cq)) { cq++; }
	*cpp = cq;
	return TRUE;
}

static int GetNumberStr(char** cpp, FLOAT_T* numP, BOOL_T* hasFract)
{
	long n0 = 0, f1, f2;
	int l1;
	char* cp = NULL;
	struct lconv* lc;

	while (isspace((unsigned char)**cpp)) { (*cpp)++; }

	/* Find out the decimal separator of the current locale */
	lc = localeconv();

	if (**cpp != lc->decimal_point[0]
	    && !GetDigitStr(cpp, &n0, NULL)) {
		return FALSE;
	}
	if (**cpp == lc->decimal_point[0]) {
		(*cpp)++;
		if (!isdigit((unsigned char)**cpp)) {
			*hasFract = FALSE;
			*numP = (FLOAT_T)n0;
			return TRUE;
		}
		if (!GetDigitStr(cpp, &f1, &l1)) { return FALSE; }
		for (f2 = 1; l1 > 0; l1--) { f2 *= 10; }
		*numP = ((FLOAT_T)n0) + ((FLOAT_T)f1) / ((FLOAT_T)f2);
		*hasFract = TRUE;
		return TRUE;  /* 999.999 */
	}
	if (isdigit((unsigned char)**cpp)) {
		cp = *cpp;
		if (!GetDigitStr(cpp, &f1, NULL)) { return FALSE; }
	} else {
		f1 = n0;
		n0 = 0;
	}
	if (**cpp == '/') {
		(*cpp)++;
		if (!GetDigitStr(cpp, &f2, &l1)) { return FALSE; }
		if (f2 == 0) {
			(*cpp) -= l1;
			parseErrorMessage = N_("Divide by 0");
			return FALSE; /* div by 0 */
		}
		*numP = ((FLOAT_T)n0) + ((FLOAT_T)f1) / ((FLOAT_T)f2);
		*hasFract = TRUE;
	} else {
		if (cp != NULL) {
			*cpp = cp;
			parseErrorMessage = N_("Expected /");
			return FALSE; /* 999 999 ?? */
		} else {
			*hasFract = FALSE;
			*numP = f1;
		}
	}
	return TRUE;
}

static BOOL_T GetDistance(char** cpp, FLOAT_T* distP)
{
	FLOAT_T n1, n2;
	BOOL_T neg = FALSE;
	BOOL_T hasFract;
	BOOL_T expectInch = FALSE;
	long distanceFormat;

	while (isspace((unsigned char)**cpp)) {
		(*cpp)++;
	}

	if ((*cpp)[0] == '\0') {
		*distP = 0.0;
		return TRUE;
	}

	if ((*cpp)[0] == '-') {
		neg = TRUE;
		(*cpp)++;
	}

	if (!GetNumberStr(cpp, &n1, &hasFract)) {
		return FALSE;
	}

	distanceFormat = GetDistanceFormat();

	if ((*cpp)[0] == '\0') {   /* EOL */
		if (units == UNITS_METRIC) {
			n1 = n1 / 2.54;

			if ((distanceFormat & DISTFMT_FMT) == DISTFMT_FMT_MM) {
				n1 /= 10;
			}

			if ((distanceFormat & DISTFMT_FMT) == DISTFMT_FMT_M) {
				n1 *= 100;
			}
		}

		if (neg) {
			n1 = -n1;
		}

		*distP = n1;
		return TRUE;
	}

	if ((*cpp)[0] == '\'') {
		n1 *= 12.0;
		(*cpp) += 1;
		expectInch = !hasFract;
	} else if (tolower((unsigned char)(*cpp)[0]) == 'f' &&
	           tolower((unsigned char)(*cpp)[1]) == 't') {
		n1 *= 12.0;
		(*cpp) += 2;
		expectInch = !hasFract;
	} else if (tolower((unsigned char)(*cpp)[0]) == 'c' &&
	           tolower((unsigned char)(*cpp)[1]) == 'm') {
		n1 /= 2.54;
		(*cpp) += 2;
	} else if (tolower((unsigned char)(*cpp)[0]) == 'm' &&
	           tolower((unsigned char)(*cpp)[1]) == 'm') {
		n1 /= 25.4;
		(*cpp) += 2;
	} else if (tolower((unsigned char)(*cpp)[0]) == 'm') {
		n1 *= 100.0 / 2.54;
		(*cpp) += 1;
	} else if ((*cpp)[0] == '"') {
		(*cpp) += 1;
	} else if (tolower((unsigned char)(*cpp)[0]) == 'i' &&
	           tolower((unsigned char)(*cpp)[1]) == 'n') {
		(*cpp) += 2;
	} else {
		parseErrorMessage = N_("Invalid Units Indicator");
		return FALSE;
	}

	while (isspace((unsigned char)**cpp)) {
		(*cpp)++;
	}

	if (expectInch && isdigit((unsigned char)**cpp)) {
		if (!GetNumberStr(cpp, &n2, &hasFract)) {
			return FALSE;
		}

		n1 += n2;

		if ((*cpp)[0] == '"') {
			(*cpp) += 1;
		} else if (tolower((unsigned char)(*cpp)[0]) == 'i' &&
		           tolower((unsigned char)(*cpp)[1]) == 'n') {
			(*cpp) += 2;
		}

		while (isspace((unsigned char)**cpp)) {
			(*cpp)++;
		}
	}

	if (**cpp) {
		parseErrorMessage = N_("Expected End Of String");
		return FALSE;
	}

	if (neg) {
		n1 = -n1;
	}

	*distP = n1;
	return TRUE;
}

FLOAT_T 
FormDecodeFloat(const char* enteredValue, 	BOOL_T* validP)
{
	FLOAT_T valF;
	const char* cp1;
	char* cp2;
	cp1 = enteredValue;

	while (isspace((unsigned char)*cp1)) { cp1++; }
	if (*cp1) {
		valF = strtod(cp1, &cp2);
		if (*cp2 != 0) {
			parseErrorMessage = _("Invalid Number");
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

FLOAT_T
FormDecodeDistance(	const char *enteredValue, BOOL_T* validP)
{
	FLOAT_T valF = 0.0;
	char* cp1, * cpN, c1;
	// CAST_AWAY_CONST: we temporarily replace *cpN with a NULL and later restore
	cp1 = cpN = CAST_AWAY_CONST enteredValue;
	cpN += strlen(cpN) - 1;

	while (cpN > cp1 && isspace((unsigned char)*cpN)) {
		cpN--;
	}

	c1 = *cpN;

	switch (c1) {
	case '=':
	case 's':
	case 'S':
	case 'p':
	case 'P':
		*cpN = '\0';
		break;

	default:
		cpN = NULL;
	}

	*validP = (GetDistance(&cp1, &valF));

	if (cpN) {
		*cpN = c1;
	}

	if (*validP) {
		if (c1 == 's' || c1 == 'S') {
			valF *= curScaleRatio;
		} else if (c1 == 'p' || c1 == 'P') {
			valF /= curScaleRatio;
		}
	} else {
		//snprintf(decodeErrorStr, sizeof(decodeErrorStr), "%s @ %s", _(parseErrorMessage),
		//	*cp1 ? cp1 : _("End Of String"));
		valF = 0.0;
	}

	return valF;
}


FLOAT_T
FormDecodeNumber(
    const char* enteredValue,
    BOOL_T* validP,
    int option,              // equivalent to pd->option
    BOOL_T bDistance)        // pre-determined distance vs number flag
{
    FLOAT_T valF;
    const char* cp1;
    
    *validP = TRUE;
    cp1 = enteredValue;
    
    // Skip leading whitespace
    while (isspace((unsigned char)*cp1)) { cp1++; }
    
    // Handle empty string
    if (*cp1 == '\0') {
        return 0.0;
    }
    
    // Route to appropriate parser
    if (!bDistance) {
        valF = FormDecodeFloat(cp1, validP);
        if (*validP && (option & PDO_ANGLE)) {
            valF = NormalizeAngle((angleSystem == ANGLE_POLAR) ? valF : -valF);
        }
        return valF;
    }
    
    // Parse as distance with units
    valF = FormDecodeDistance(cp1, validP);
    return valF;
}

char *
FormGetParseError()
{
	return(parseErrorMessage);
}
