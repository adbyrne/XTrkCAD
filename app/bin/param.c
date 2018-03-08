/** \file param.c
 * Handle all the dialog box creation stuff.
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#ifndef WINDOWS
#include <unistd.h>
#include <dirent.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifdef WINDOWS
#include <io.h>
#include <windows.h>
#define R_OK (02)
#define access _access
#else
#include <sys/stat.h>
#include <errno.h>
#endif
#include <stdarg.h>
#include <locale.h>
#include <wlib.h>

#include "common.h"
#include "compound.h"
#include "custom.h"
#include "fileio.h"
#include "i18n.h"
#include "messages.h"
#include "misc.h"
#include "param.h"
#include "track.h"
#include "utility.h"


/* Bogus reg vars */
EXPORT int paramLevel = 1;
EXPORT int paramLen;
EXPORT unsigned long paramKey;
EXPORT char paramId[100];
EXPORT BOOL_T paramTogglePlaybackHilite;

EXPORT char *PREFSECT = "DialogItem";

static int paramCheckErrorCount = 0;
static BOOL_T paramCheckShowErrors = FALSE;

static int log_paramLayout;


/*****************************************************************************
 *
 *
 *
 */

static char * getNumberError;
static char decodeErrorStr[STR_SHORT_SIZE];

static int GetDigitStr( char ** cpp, long * numP, int * lenP )
{
	char *cp=*cpp, *cq;
	int len;
	*numP = 0;
	if ( cp == NULL ) {
		getNumberError = N_("Unexpected End Of String");
		return FALSE;
	}
	while ( isspace((unsigned char)*cp) ) cp++;
	*numP = strtol( cp, &cq, 10 );
	if ( cp==cq ) {
		*cpp = cp;
		getNumberError = N_("Expected digit");
		return FALSE;
	}
	len = cq-cp;
	if ( lenP )
		*lenP = len;
	if ( len > 9 ) {
		getNumberError = N_("Overflow");
		return FALSE;
	}
	while ( isspace((unsigned char)*cq) ) cq++;
	*cpp = cq;
	return TRUE;
}

static int GetNumberStr( char ** cpp, FLOAT_T * numP, BOOL_T * hasFract )
{
	long n0=0, f1, f2;
	int l1;
	char * cp = NULL;
	struct lconv *lc;

	while ( isspace((unsigned char)**cpp) ) (*cpp)++;

	/* Find out the decimal separator of the current locale */
	lc = localeconv();

	if ( **cpp != lc->decimal_point[0]
			&& !GetDigitStr( cpp, &n0, NULL ) )
		return FALSE;
	if ( **cpp == lc->decimal_point[0] ) {
		(*cpp)++;
		if ( !isdigit((unsigned char)**cpp) ) {
			*hasFract = FALSE;
			*numP = (FLOAT_T)n0;
			return TRUE;
		}
		if ( !GetDigitStr( cpp, &f1, &l1 ) ) return FALSE;
		for ( f2=1; l1>0; l1-- ) f2 *= 10;
		*numP = ((FLOAT_T)n0)+((FLOAT_T)f1)/((FLOAT_T)f2);
		*hasFract = TRUE;
		return TRUE;  /* 999.999 */
	}
	if ( isdigit( (unsigned char)**cpp ) ) {
		cp = *cpp;
		if ( !GetDigitStr( cpp, &f1, NULL ) ) return FALSE;
	} else {
		f1 = n0;
		n0 = 0;
	}
	if ( **cpp == '/' ) {
		(*cpp)++;
		if ( !GetDigitStr( cpp, &f2, &l1 ) ) return FALSE;
		if ( f2 == 0 ) {
			(*cpp) -= l1;
			getNumberError = N_("Divide by 0");
			return FALSE; /* div by 0 */
		}
		*numP = ((FLOAT_T)n0)+((FLOAT_T)f1)/((FLOAT_T)f2);
		*hasFract = TRUE;
	} else {
		if ( cp != NULL ) {
			*cpp = cp;
			getNumberError = N_("Expected /");
			return FALSE; /* 999 999 ?? */
		} else {
			*hasFract = FALSE;
			*numP = f1;
		}
	}
	return TRUE;
}
//extern wIndex_t distanceFormatInx;   // distanceFormatInx

static BOOL_T GetDistance(char ** cpp, FLOAT_T * distP)
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
        if (units==UNITS_METRIC) {
            n1 = n1/2.54;

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
        n1 *= 100.0/2.54;
        (*cpp) += 1;
    } else if ((*cpp)[0] == '"') {
        (*cpp) += 1;
    } else if (tolower((unsigned char)(*cpp)[0]) == 'i' &&
               tolower((unsigned char)(*cpp)[1]) == 'n') {
        (*cpp) += 2;
    } else {
        getNumberError = N_("Invalid Units Indicator");
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
        getNumberError = N_("Expected End Of String");
        return FALSE;
    }

    if (neg) {
        n1 = -n1;
    }

    *distP = n1;
    return TRUE;
}


EXPORT FLOAT_T DecodeFloat(
		wString_p strCtrl,
		BOOL_T * validP )
{
	FLOAT_T valF;
	const char *cp0, *cp1;
    char *cp2;
	cp0 = cp1 = wStringGetValue( strCtrl );
	while (isspace((unsigned char)*cp1)) cp1++;
	if ( *cp1 ) {
		valF = strtod( cp1, &cp2 );
		if ( *cp2 != 0 ) {
			/*wStringSetHilight( strCtrl, cp2-cp0, -1 );*/
			sprintf( decodeErrorStr, _("Invalid Number") );
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


FLOAT_T DecodeDistance(
    wString_p strCtrl,
    BOOL_T * validP)
{
    FLOAT_T valF;
    char *cp0, *cp1, *cpN, c1;
    cp0 = cp1 = cpN = CAST_AWAY_CONST wStringGetValue(strCtrl);
    cpN += strlen(cpN)-1;

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

        if (cpN) {
            wStringSetValue(strCtrl, FormatDistance(valF));
        }
    } else {
        sprintf(decodeErrorStr, "%s @ %s", _(getNumberError),
                *cp1?cp1:_("End Of String"));
        valF =	0.0;
    }

    return valF;
}


#define N_STRING (10)
static char formatStrings[N_STRING][40];
static int formatStringInx;

EXPORT char * FormatLong(
		long valL )
{
	if ( ++formatStringInx >= N_STRING )
		formatStringInx = 0;
	sprintf( formatStrings[formatStringInx], "%ld", valL );
	return formatStrings[formatStringInx];
}


EXPORT char * FormatFloat(
		FLOAT_T valF )
{
	if ( ++formatStringInx >= N_STRING )
		formatStringInx = 0;
	sprintf( formatStrings[formatStringInx], "%0.3f", valF );
	return formatStrings[formatStringInx];
}


static void FormatFraction(
		char ** cpp,
		BOOL_T printZero,
		int digits,
		BOOL_T rational,
		FLOAT_T valF,
		char * unitFmt )
{
	char * cp = *cpp;
	long integ;
	long f1, f2;
	char * space = "";

	if ( !rational ) {
		sprintf( cp, "%0.*f", digits, valF );
		cp += strlen(cp);
	} else {
		integ = (long)floor(valF);
		valF -= (FLOAT_T)integ;
		for ( f2=1; digits>0; digits--,f2*=2 );
		f1 = (long)floor( (valF*(FLOAT_T)f2) + 0.5 );
		if ( f1 >= f2 ) {
			f1 -= f2;
			integ++;
		}
		if ( integ != 0 || !printZero ) {
			sprintf( cp, "%ld", integ );
			cp += strlen(cp);
			printZero = FALSE;
			space = " ";
		}
		if ( f2 > 1 && f1 != 0 ) {
			while ( (f1&1) == 0 ) { f1 /= 2; f2 /= 2; }
			sprintf( cp, "%s%ld/%ld", space, f1, f2 );
			cp += strlen(cp);
		} else if ( printZero ) {
			*cp++ = '0';
			*cp = '\0';
		}
	}
	if ( cp != *cpp ) {
		strcpy( cp, unitFmt );
		cp += strlen(cp);
		*cpp = cp;
	}
}


EXPORT char * FormatDistanceEx(
		FLOAT_T valF,
		long distanceFormat )
{
	char * cp;
	int digits;
	long feet;
	char * metricInd;

	if ( ++formatStringInx >= N_STRING )
		formatStringInx = 0;
	cp = formatStrings[formatStringInx];
	digits = (int)(distanceFormat&DISTFMT_DECS);
	valF = PutDim(valF);
	if ( valF < 0 ) {
		*cp++ = '-';
		valF = -valF;
	}
	if ( (distanceFormat&DISTFMT_FMT) == DISTFMT_FMT_NONE ) {
		FormatFraction( &cp, FALSE, digits, (distanceFormat&DISTFMT_FRACT) == DISTFMT_FRACT_FRC, valF, "" );
		return formatStrings[formatStringInx];
	} else if ( units == UNITS_ENGLISH ) {
		feet = (long)(floor)(valF/12.0);
		valF -= feet*12.0;
		if ( feet != 0 ) {
			sprintf( cp, "%ld%s", feet, (distanceFormat&DISTFMT_FMT)==DISTFMT_FMT_SHRT?"' ":"ft " );
			cp += strlen(cp);
		}
		if ( feet==0 || valF != 0 ) {
		FormatFraction( &cp, feet==0, digits, (distanceFormat&DISTFMT_FRACT) == DISTFMT_FRACT_FRC, valF,
				(distanceFormat&DISTFMT_FMT)==DISTFMT_FMT_SHRT?"\"":"in" );
		}
	} else {
		if ( (distanceFormat&DISTFMT_FMT)==DISTFMT_FMT_M ) {
			valF = valF/100.0;
			metricInd = "m";
		} else if ( (distanceFormat&DISTFMT_FMT)==DISTFMT_FMT_MM ) {
			valF = valF*10.0;
			metricInd = "mm";
		} else {
			metricInd = "cm";
		}
		FormatFraction( &cp, FALSE, digits, (distanceFormat&DISTFMT_FRACT) == DISTFMT_FRACT_FRC, valF, metricInd );
	}
	return formatStrings[formatStringInx];
}


EXPORT char * FormatDistance(
		FLOAT_T valF )
{
	return FormatDistanceEx( valF, GetDistanceFormat() );
}

EXPORT char * FormatSmallDistance(
		FLOAT_T valF )
{
	long format = GetDistanceFormat();
	format &= ~(DISTFMT_FRACT_FRC|DISTFMT_DECS);
	format |= 3;
	return FormatDistanceEx( valF, format );
}

/*****************************************************************************
 *
 *
 *
 */

EXPORT void ParamControlActive(
		paramGroup_p pg,
		int inx,
		BOOL_T active )
{
	paramData_p p = &pg->paramPtr[inx];
	if ( p->control )
		wControlActive( p->control, active );
}


EXPORT void ParamLoadMessage(
		paramGroup_p pg,
		int inx,
		char * message )
{
	paramData_p p = &pg->paramPtr[inx];
	if ( p->control ) {
		if ( p->type == PD_MESSAGE )
			wMessageSetValue( (wMessage_p)p->control, message );
		else if ( p->type == PD_STRING )
			wStringSetValue( (wString_p)p->control, message );
		else
			AbortProg( "paramLoadMessage: not a PD_MESSAGE or PD_STRING" );
	}
}


EXPORT void ParamLoadControl(
		paramGroup_p pg,
		int inx )
{
	paramData_p p = &pg->paramPtr[inx];
	FLOAT_T tmpR;
	char * valS;

		if ( (p->option&PDO_DLGIGNORE) != 0 )
			return;
		if (p->control == NULL || p->valueP == NULL)
			return;
		switch ( p->type ) {
		case PD_LONG:
			wStringSetValue( (wString_p)p->control, FormatLong( *(long*)p->valueP ) );
			p->oldD.l = *(long*)p->valueP;
			break;
		case PD_RADIO:
			wRadioSetValue( (wChoice_p)p->control, *(long*)p->valueP );
			p->oldD.l = *(long*)p->valueP;
			break;
		case PD_TOGGLE:
			wToggleSetValue( (wChoice_p)p->control, *(long*)p->valueP );
			p->oldD.l = *(long*)p->valueP;
			break;
		case PD_LIST:
		case PD_DROPLIST:
		case PD_COMBOLIST:
			wListSetIndex( (wList_p)p->control, *(wIndex_t*)p->valueP );
			p->oldD.l = *(wIndex_t*)p->valueP;
			break;
		case PD_COLORLIST:
			wColorSelectButtonSetColor( (wButton_p)p->control, *(wDrawColor*)p->valueP );
			p->oldD.dc = *(wDrawColor*)p->valueP;
			break;
		case PD_FLOAT:
			tmpR = *(FLOAT_T*)p->valueP;
			if (p->option&PDO_DIM) {
				if (p->option&PDO_SMALLDIM)
					valS = FormatSmallDistance( tmpR );
				else
					valS = FormatDistance( tmpR );
			} else {
				if (p->option&PDO_ANGLE)
					tmpR = NormalizeAngle( (angleSystem==ANGLE_POLAR)?tmpR:-tmpR );
				valS = FormatFloat( tmpR );
			}
			wStringSetValue( (wString_p)p->control, valS );
			p->oldD.f = tmpR;
			break;
		case PD_STRING:
			if (p->oldD.s)
				MyFree( p->oldD.s );
			if (p->context) {
				p->oldD.s = MyMalloc((unsigned int)p->context);
				strncpy(p->oldD.s, (char*)p->valueP, (unsigned int)p->context);
				*(p->oldD.s + (unsigned int)p->context - 1) = '\0';
				wStringSetValue((wString_p)p->control, (char*)p->oldD.s);
			}
			else {
				p->oldD.s = MyStrdup((char *)p->valueP);
				wStringSetValue((wString_p)p->control, (char*)p->valueP);
			}
			break;
		case PD_MESSAGE:
			wMessageSetValue( (wMessage_p)p->control, _((char*)p->valueP) );
			break;
		case PD_TEXT:
			wTextClear( (wText_p)p->control );
			wTextAppend( (wText_p)p->control, (char*)p->valueP );
			break;
		case PD_BUTTON:
		case PD_DRAW:
		case PD_MENU:
		case PD_MENUITEM:
		case PD_BITMAP:
			break;
		}
}


/** Load all the controls in a parameter group.
* \param IN pointer to parameter group to be loaded
*/
EXPORT void ParamLoadControls(
		paramGroup_p pg )
{
	int inx;
	for ( inx=0; inx<pg->paramCnt; inx++ )
		ParamLoadControl( pg, inx );
}


EXPORT long ParamUpdate(
		paramGroup_p pg )
{
	long longV;
	FLOAT_T floatV;
	const char * stringV;
	wDrawColor dc;
	long change = 0;
	int inx;
	paramData_p p;
	BOOL_T valid;

	for ( p=pg->paramPtr,inx=0; p<&pg->paramPtr[pg->paramCnt]; p++,inx++ ) {
		if ( (p->option&PDO_DLGIGNORE) != 0 )
			continue;
		if ( p->control == NULL )
			continue;
		switch ( p->type ) {
		case PD_LONG:
			stringV = wStringGetValue( (wString_p)p->control );
			longV = atol( stringV );
			if (longV != p->oldD.l) {
				p->oldD.l = longV;
				if ( /*(p->option&PDO_NOUPDUPD)==0 &&*/ p->valueP)
					*(long*)p->valueP = longV;
				if ( (p->option&PDO_NOUPDACT)==0 && pg->changeProc)
					 pg->changeProc( pg, inx, &longV );
				change |= (1L<<inx);
			}
			break;
		case PD_RADIO:
			longV = wRadioGetValue( (wChoice_p)p->control );
			if (longV != p->oldD.l) {
				p->oldD.l = longV;
				if ( /*(p->option&PDO_NOUPDUPD)==0 &&*/ p->valueP)
					*(long*)p->valueP = longV;
				if ( (p->option&PDO_NOUPDACT)==0 && pg->changeProc)
					 pg->changeProc( pg, inx, &longV );
				change |= (1L<<inx);
			}
			break;
		case PD_TOGGLE:
			longV = wToggleGetValue( (wChoice_p)p->control );
			if (longV != p->oldD.l) {
				p->oldD.l = longV;
				if ( /*(p->option&PDO_NOUPDUPD)==0 &&*/ p->valueP)
					*(long*)p->valueP = longV;
				if ( (p->option&PDO_NOUPDACT)==0 && pg->changeProc)
					 pg->changeProc( pg, inx, &longV );
				change |= (1L<<inx);
			}
			break;
		case PD_LIST:
		case PD_DROPLIST:
		case PD_COMBOLIST:
			longV = wListGetIndex( (wList_p)p->control );
			if (longV != p->oldD.l) {
				p->oldD.l = longV;
				if ( /*(p->option&PDO_NOUPDUPD)==0 &&*/ p->valueP)
					*(wIndex_t*)p->valueP = (wIndex_t)longV;
				if ( (p->option&PDO_NOUPDACT)==0 && pg->changeProc)
					 pg->changeProc( pg, inx, &longV );
				change |= (1L<<inx);
			}
			break;
		case PD_COLORLIST:
			dc = wColorSelectButtonGetColor( (wButton_p)p->control );
			if (dc != p->oldD.dc) {
				p->oldD.dc = dc;
				if ( /*(p->option&PDO_NOUPDUPD)==0 &&*/ p->valueP)
					*(wDrawColor*)p->valueP = dc;
				if ( (p->option&PDO_NOUPDACT)==0 && pg->changeProc) {
					pg->changeProc( pg, inx, &longV ); /* COLORNOP */
				}
				change |= (1L<<inx);
			}
			break;
		case PD_FLOAT:
			if (p->option & PDO_DIM) {
				floatV = DecodeDistance( (wString_p)p->control, &valid );
			} else {
				floatV = DecodeFloat( (wString_p)p->control, &valid );
				if (valid && (p->option & PDO_ANGLE) )
					floatV = NormalizeAngle( (angleSystem==ANGLE_POLAR)?floatV:-floatV );
			}
			if ( !valid )
				break;
			if (floatV != p->oldD.f) {
				p->oldD.f = floatV;
				if ( /*(p->option&PDO_NOUPDUPD)==0 &&*/ p->valueP)
					*(FLOAT_T*)p->valueP = floatV;
				if ( (p->option&PDO_NOUPDACT)==0 && pg->changeProc)
					 pg->changeProc( pg, inx, &floatV );
				change |= (1L<<inx);
			}
			break;
		case PD_STRING:
			stringV = wStringGetValue( (wString_p)p->control );
			if ( strcmp( stringV, p->oldD.s ) != 0 ) {
				if (p->oldD.s)
					MyFree( p->oldD.s );
				p->oldD.s = MyStrdup( stringV );
				if ( /*(p->option&PDO_NOUPDUPD)==0 &&*/ p->valueP) {
					if (p->context) {
						strncpy((char*)p->valueP, stringV, (unsigned int)p->context);
						((char *)p->valueP)[(unsigned int)p->context - 1] = '\0';
						if (strlen(stringV) > (unsigned int)p->context) {
							NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED, _("Ok"), NULL, (unsigned int)p->context);
						}

					}
					else {
						strcpy((char*)p->valueP, stringV);
					}
				}

				if ( (p->option&PDO_NOUPDACT)==0 && pg->changeProc)
					 pg->changeProc( pg, inx, CAST_AWAY_CONST stringV );
				change |= (1L<<inx);
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
#ifdef PGPROC
	if (pg->proc)
		pg->proc( PGACT_UPDATE, change );
#endif
	return change;
}


void ParamLoadData(
    paramGroup_p pg)
{
    FLOAT_T floatV;
    const char * stringV;
    paramData_p p;
    BOOL_T valid;
    BOOL_T inRange;

    for (p=pg->paramPtr; p<&pg->paramPtr[pg->paramCnt]; p++) {
        if ((p->option&PDO_DLGIGNORE) != 0) {
            continue;
        }

        if (p->control == NULL || p->valueP == NULL) {
            continue;
        }

        switch (p->type) {
            long longV;

        case PD_LONG:
            longV = atol(wStringGetValue((wString_p)p->control));

            if (p->winData) {
                inRange = (longV <= ((paramIntegerRange_t *)p->winData)->high) &&
                          (longV >= ((paramIntegerRange_t *)p->winData)->low);
            } else {
                inRange = TRUE;
            }

            if (inRange) {
                *(long*)p->valueP = longV;
            }

            break;

        case PD_RADIO:
            *(long*)p->valueP = wRadioGetValue((wChoice_p)p->control);
            break;

        case PD_TOGGLE:
            *(long*)p->valueP = wToggleGetValue((wChoice_p)p->control);
            break;

        case PD_LIST:
        case PD_DROPLIST:
        case PD_COMBOLIST:
            *(wIndex_t*)p->valueP = wListGetIndex((wList_p)p->control);
            break;

        case PD_COLORLIST:
            *(wDrawColor*)p->valueP = wColorSelectButtonGetColor((wButton_p)p->control);
            break;

        case PD_FLOAT:
            if (p->option & PDO_DIM) {
                floatV = DecodeDistance((wString_p)p->control, &valid);
            } else {
                floatV = DecodeFloat((wString_p)p->control, &valid);

                if (valid && (p->option & PDO_ANGLE)) {
                    floatV = NormalizeAngle((angleSystem==ANGLE_POLAR)?floatV:-floatV);
                }
            }

            if (p->winData) {
                inRange = (floatV <= ((paramFloatRange_t *)p->winData)->high) &&
                          (floatV >= ((paramFloatRange_t *)p->winData)->low);
            } else {
                inRange = TRUE;
            }

            if (valid && inRange) {
                *(FLOAT_T*)p->valueP = floatV;
            }

            break;

        case PD_STRING:
            stringV = wStringGetValue((wString_p)p->control);
            strcpy((char*)p->valueP, stringV);
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


static long ParamIntRestore(
		paramGroup_p pg,
		int class )
{
	long change = 0;
	int inx;
	paramData_p p;
	FLOAT_T valR;
	char * valS;
	paramOldData_t * oldP;

	for ( p=pg->paramPtr,inx=0; p<&pg->paramPtr[pg->paramCnt]; p++,inx++ ) {
		oldP = (class==0)?&p->oldD:&p->demoD;
		if ( (p->option&PDO_DLGIGNORE) != 0 )
			continue;
		if (p->valueP == NULL)
			continue;
		switch ( p->type ) {
		case PD_LONG:
			if ( *(long*)p->valueP != oldP->l ) {
				/*if ((p->option&PDO_NORSTUPD)==0)*/
					*(long*)p->valueP = oldP->l;
				if (p->control) {
					wStringSetValue( (wString_p)p->control, FormatLong( oldP->l ) );
				}
				change |= (1L<<inx);
			}
			break;
		case PD_RADIO:
			if ( *(long*)p->valueP != oldP->l ) {
				/*if ((p->option&PDO_NORSTUPD)==0)*/
					*(long*)p->valueP = oldP->l;
				if (p->control)
					wRadioSetValue( (wChoice_p)p->control, oldP->l );
				change |= (1L<<inx);
			}
			break;
		case PD_TOGGLE:
			if ( *(long*)p->valueP != oldP->l ) {
				/*if ((p->option&PDO_NORSTUPD)==0)*/
					*(long*)p->valueP = oldP->l;
				if (p->control)
					wToggleSetValue( (wChoice_p)p->control, oldP->l );
				change |= (1L<<inx);
			}
			break;
		case PD_LIST:
		case PD_DROPLIST:
		case PD_COMBOLIST:
			if ( *(wIndex_t*)p->valueP != (wIndex_t)oldP->l ) {
				/*if ((p->option&PDO_NORSTUPD)==0)*/
					*(wIndex_t*)p->valueP = (wIndex_t)oldP->l;
				if (p->control)
					wListSetIndex( (wList_p)p->control, (wIndex_t)oldP->l );
				change |= (1L<<inx);
			}
			break;
		case PD_COLORLIST:
			if ( *(wDrawColor*)p->valueP != oldP->dc ) {
				/*if ((p->option&PDO_NORSTUPD)==0)*/
					*(wDrawColor*)p->valueP = oldP->dc;
				if (p->control)
					wColorSelectButtonSetColor( (wButton_p)p->control, oldP->dc ); /* COLORNOP */
				change |= (1L<<inx);
			}
			break;
		case PD_FLOAT:
			if ( *(FLOAT_T*)p->valueP != oldP->f ) {
				/*if ((p->option&PDO_NORSTUPD)==0)*/
					*(FLOAT_T*)p->valueP = oldP->f;
				if (p->control) {
					valR = oldP->f;
					if (p->option & PDO_DIM) {
						if (p->option & PDO_SMALLDIM)
							valS = FormatSmallDistance( valR );
						else
							valS = FormatDistance( valR );
					} else {
						if (p->option & PDO_ANGLE)
							valR = NormalizeAngle( (angleSystem==ANGLE_POLAR)?valR:-valR );
						valS = FormatFloat( valR );
					}
					wStringSetValue( (wString_p)p->control, valS );
				}
				change |= (1L<<inx);
			}
			break;
		case PD_STRING:
			if ( oldP->s && strcmp((char*)p->valueP,oldP->s) != 0 ) {
				/*if ((p->option&PDO_NORSTUPD)==0)*/
					strcpy( (char*)p->valueP, oldP->s );
				if (p->control)
					wStringSetValue( (wString_p)p->control, oldP->s );
				change |= (1L<<inx);
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
#ifdef PGPROC
	if (pg->proc)
		pg->proc( PGACT_RESTORE, change );
#endif
	return change;
}


static void ParamIntSave(
		paramGroup_p pg,
		int class )
{
	paramData_p p;
	paramOldData_t * oldP;

	for ( p=pg->paramPtr; p<&pg->paramPtr[pg->paramCnt]; p++ ) {
		oldP = (class==0)?&p->oldD:&p->demoD;
		if (p->valueP) {
			switch (p->type) {
			case PD_LONG:
			case PD_RADIO:
			case PD_TOGGLE:
				oldP->l = *(long*)p->valueP;
				break;
			case PD_LIST:
			case PD_DROPLIST:
			case PD_COMBOLIST:
				oldP->l = *(wIndex_t*)p->valueP;
				break;
			case PD_COLORLIST:
				oldP->dc = *(wDrawColor*)p->valueP;
				break;
			case PD_FLOAT:
				oldP->f = *(FLOAT_T*)p->valueP;
				break;
			case PD_STRING:
				if (oldP->s)
					MyFree(oldP->s);
				oldP->s = MyStrdup( (char*)p->valueP );
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

#ifdef LATER
static void ParamSave( paramGroup_p pg )
{
	ParamIntSave( pg, 0 );
}

static long ParamRestore( paramGroup_p pg )
{
	return ParamIntRestore( pg, 0 );
}
#endif

/****************************************************************************
 *
 *
 *
 */

static dynArr_t paramGroups_da;
#define paramGroups(N) DYNARR_N( paramGroup_p, paramGroups_da, N )



EXPORT void ParamRegister( paramGroup_p pg )
{
	paramData_p p;
	const char * cp;
	WDOUBLE_T tmpR;
	long valL;
	long rgb;
	char prefName1[STR_SHORT_SIZE];
	const char *prefSect2, *prefName2;

	DYNARR_APPEND( paramGroup_p, paramGroups_da, 10 );
	paramGroups(paramGroups_da.cnt-1) = pg;
	for ( p=pg->paramPtr; p<&pg->paramPtr[pg->paramCnt]; p++ ) {
		p->group = pg;
		if ( p->nameStr == NULL )
			continue;
		sprintf( prefName1, "%s-%s", pg->nameStr, p->nameStr );
		if ( p->type != PD_MENUITEM ) {
			(void)GetBalloonHelpStr( prefName1 );
		}
		if (p->valueP == NULL || (p->option&PDO_NOPREF) != 0)
			continue;
		prefSect2 = PREFSECT;
		prefName2 = prefName1;
		if ( (p->option&PDO_MISC) ) {
			prefSect2 = "misc";
			prefName2 = p->nameStr;
		} else if ( (p->option&PDO_DRAW) ) {
			prefSect2 = "draw";
			prefName2 = p->nameStr;
		} else if ( (p->option&PDO_FILE) ) {
			prefSect2 = "file";
			prefName2 = p->nameStr;
		} else if ( (pg->options&PGO_PREFGROUP) ) {
			prefSect2 = pg->nameStr;
			prefName2 = p->nameStr;
		} else if ( (pg->options&PGO_PREFMISC) ) {
			prefSect2 = "misc";
			prefName2 = p->nameStr;
		} else if ( (pg->options&PGO_PREFMISCGROUP) ) {
			prefSect2 = "misc";
		} else if ( (pg->options&PGO_PREFDRAWGROUP) ) {
			prefSect2 = "draw";
		}
		cp = strchr( p->nameStr, '\t' );
		if ( cp ) {
			/* *cp++ = 0; */
			prefSect2 = cp;
			cp = strchr( cp, '\t' );
			if ( cp ) {
				/* *cp++ = 0; */
				prefName2 = cp;
			}
		}
		switch (p->type) {
		case PD_LONG:
		case PD_RADIO:
		case PD_TOGGLE:
			if ( !wPrefGetInteger( PREFSECT, prefName1, p->valueP, *(long*)p->valueP ))
				wPrefGetInteger( prefSect2, prefName2, p->valueP, *(long*)p->valueP );
			break;
		case PD_LIST:
		case PD_DROPLIST:
		case PD_COMBOLIST:
			if ( (p->option&PDO_LISTINDEX) ) {
				if (!wPrefGetInteger( PREFSECT, prefName1, &valL, *(wIndex_t*)p->valueP ))
					wPrefGetInteger( prefSect2, prefName2, &valL, valL );
				if ( p->control )
					wListSetIndex( (wList_p)p->control, (wIndex_t)valL );
				*(wIndex_t*)p->valueP = (wIndex_t)valL;
			} else {
				if (!p->control)
					break;
				cp = wPrefGetString( PREFSECT, prefName1 );
				if ( !cp )
					cp = wPrefGetString( prefSect2, prefName2 );
				if ( !cp )
					break;
				*(wIndex_t*)p->valueP = wListFindValue( (wList_p)p->control, cp );
			}
			break;
		case PD_COLORLIST:
			rgb = wDrawGetRGB( *(wDrawColor*)p->valueP );
			if (!wPrefGetInteger( PREFSECT, prefName1, &rgb, rgb ))
				wPrefGetInteger( prefSect2, prefName2, &rgb, rgb );
			*(wDrawColor*)p->valueP = wDrawFindColor( rgb );
			break;
		case PD_FLOAT:
			if (!wPrefGetFloat( PREFSECT, prefName1, &tmpR, *(FLOAT_T*)p->valueP ))
				wPrefGetFloat( prefSect2, prefName2, &tmpR, *(FLOAT_T*)p->valueP );
			*(FLOAT_T*)p->valueP = tmpR;
			break;
		case PD_STRING:
			cp = wPrefGetString( PREFSECT, prefName1 );
			if (!cp)
				wPrefGetString( prefSect2, prefName2 );
			if (cp)
				strcpy( p->valueP, cp );
			else
				((char*)p->valueP)[0] = '\0';
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




EXPORT void ParamUpdatePrefs( void )
{
	int inx;
	paramGroup_p pg;
	paramData_p p;
	long rgb;
	char prefName[STR_SHORT_SIZE];
	int len;
	int col;
	char * cp;
	static wPos_t * colWidths;
	static int maxColCnt = 0;
	paramListData_t * listDataP;

	for ( inx=0; inx<paramGroups_da.cnt; inx++ ) {
	  pg = paramGroups(inx);
	  for ( p=pg->paramPtr; p<&pg->paramPtr[pg->paramCnt]; p++ ) {
		if (p->valueP == NULL || p->nameStr == NULL || (p->option&PDO_NOPREF)!=0 )
			continue;
		if ( (p->option&PDO_DLGIGNORE) != 0 )
			continue;
		sprintf( prefName, "%s-%s", pg->nameStr, p->nameStr );
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
					if ( maxColCnt == 0 )
						colWidths = (wPos_t*)MyMalloc( listDataP->colCnt * sizeof * colWidths );
					else
						colWidths = (wPos_t*)MyRealloc( colWidths, listDataP->colCnt * sizeof * colWidths );
					maxColCnt = listDataP->colCnt;
				}
				len = wListGetColumnWidths( (wList_p)p->control, listDataP->colCnt, colWidths );
				cp = message;
				for ( col=0; col<len; col++ ) {
					sprintf( cp, "%d ", colWidths[col] );
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
			break;
		}
	  }
	}
}

EXPORT void ParamGroupRecord(
		paramGroup_p pg )
{
	paramData_p p;
	long rgb;

	if (recordF == NULL)
		return;
	for ( p=pg->paramPtr; p<&pg->paramPtr[pg->paramCnt]; p++ ) {
		if ( (p->option&PDO_NORECORD) != 0 || p->valueP == NULL || p->nameStr == NULL )
			continue;
		if ( (p->option&PDO_DLGIGNORE) != 0 )
			continue;
		switch ( p->type ) {
		case PD_LONG:
		case PD_RADIO:
		case PD_TOGGLE:
			fprintf( recordF, "PARAMETER %s %s %ld\n", pg->nameStr, p->nameStr, *(long*)p->valueP );
			break;
		case PD_LIST:
		case PD_DROPLIST:
		case PD_COMBOLIST:
			if (p->control)
				wListGetValues( (wList_p)p->control, message, sizeof message, NULL, NULL );
			else
				message[0] = '\0';
			fprintf( recordF, "PARAMETER %s %s %d %s\n", pg->nameStr, p->nameStr, *(wIndex_t*)p->valueP, message );
			break;
		case PD_COLORLIST:
			rgb = wDrawGetRGB( *(wDrawColor*)p->valueP );
			fprintf( recordF, "PARAMETER %s %s %ld\n",
				pg->nameStr, p->nameStr, rgb );
			break;
		case PD_FLOAT:
			fprintf( recordF, "PARAMETER %s %s %0.3f\n", pg->nameStr, p->nameStr, *(FLOAT_T*)p->valueP );
			break;
		case PD_STRING:
			fprintf( recordF, "PARAMETER %s %s %s\n", pg->nameStr, p->nameStr, (char*)p->valueP );
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
	if (pg->nameStr)
		fprintf( recordF, "PARAMETER GROUP %s\n", pg->nameStr );
	fflush( recordF );
}


EXPORT void ParamStartRecord( void )
{
	int inx;
	paramGroup_p pg;

	if (recordF == NULL)
		return;
	for ( inx=0; inx<paramGroups_da.cnt; inx++ ) {
		pg = paramGroups(inx);
		if (pg->options&PGO_RECORD) {
			ParamGroupRecord( pg );
		}
	}
}


EXPORT void ParamRestoreAll( void )
{
	int inx;
	paramGroup_p pg;

	for ( inx=0; inx<paramGroups_da.cnt; inx++ ) {
		pg = paramGroups(inx);
		ParamIntRestore( pg, 1 );
	}
	if ( paramCheckErrorCount > 0 ) {
		NoticeMessage( "PARAMCHECK: %d errors", "Ok", NULL, paramCheckErrorCount );
	}
}


EXPORT void ParamSaveAll( void )
{
	int inx;

	for ( inx=0; inx<paramGroups_da.cnt; inx++ ) {
		ParamIntSave( paramGroups(inx), 1 );
		paramGroups(inx)->action = 0;
	}
	paramCheckErrorCount = 0;
}


static void ParamButtonPush( void * dp )
{
	paramData_p p = (paramData_p)dp;
	if (recordF && (p->option&PDO_NORECORD)==0 && p->group->nameStr && p->nameStr) {
		fprintf( recordF, "PARAMETER %s %s\n", p->group->nameStr, p->nameStr );
		fflush( recordF );
	}
	if ( (p->option&PDO_NOPSHACT)==0 ) {
		if ( p->valueP )
			((wButtonCallBack_p)(p->valueP))( p->context );
		else if ( p->group->changeProc)
			 p->group->changeProc( p->group, p-p->group->paramPtr, NULL);
	}
}


static void ParamChoicePush( long valL, void * dp )
{
	paramData_p p = (paramData_p)dp;

	if (recordF && (p->option&PDO_NORECORD)==0 && p->group->nameStr && p->nameStr) {
		fprintf( recordF, "PARAMETER %s %s %ld\n", p->group->nameStr, p->nameStr, valL );
		fflush( recordF );
	}
	if ( (p->option&PDO_NOPSHUPD)==0 && p->valueP)
		*((long*)(p->valueP)) = valL;
	if ( (p->option&PDO_NOPSHACT)==0 && p->group->changeProc)
		p->group->changeProc( p->group, p-p->group->paramPtr, &valL);
}


static void ParamIntegerPush( const char * val, void * dp )
{
	paramData_p p = (paramData_p)dp;
	long valL;
	char * cp;
	paramIntegerRange_t * irangeP;

	while ( isspace((unsigned char)*val)) val++;
	valL = strtol( val, &cp, 10 );

	//wControlSetBalloon( p->control, 0, -5, NULL );
	if ( val == cp ) {
		wControlSetBalloon( p->control, 0, -5, _("Invalid Number") );
		return;
	}
	irangeP = (paramIntegerRange_t*)p->winData;
	if ( ( (irangeP->rangechecks&PDO_NORANGECHECK_HIGH) == 0 && valL > irangeP->high ) ||
		 ( (irangeP->rangechecks&PDO_NORANGECHECK_LOW) == 0 && valL < irangeP->low ) ) {
		if ( (irangeP->rangechecks&(PDO_NORANGECHECK_HIGH|PDO_NORANGECHECK_LOW)) == PDO_NORANGECHECK_HIGH )
			sprintf( message, _("Enter a value > %ld"), irangeP->low );
		else if ( (irangeP->rangechecks&(PDO_NORANGECHECK_HIGH|PDO_NORANGECHECK_LOW)) == PDO_NORANGECHECK_LOW )
			 sprintf( message, _("Enter a value < %ld"), irangeP->high );
		else
			 sprintf( message, _("Enter a value between %ld and %ld"), irangeP->low, irangeP->high );
		wControlSetBalloon( p->control, 0, -5, message );
		return;
	}
	wControlSetBalloon( p->control, 0, -5, NULL );

	if (recordF && (p->option&PDO_NORECORD)==0 && p->group->nameStr && p->nameStr) {
		fprintf( recordF, "PARAMETER %s %s %ld\n", p->group->nameStr, p->nameStr, valL );
		fflush( recordF );
	}
	if ( (p->option&PDO_NOPSHUPD)==0 && p->valueP)
		*((long*)(p->valueP)) = valL;
	if ( (p->option&PDO_NOPSHACT)==0 && p->group->changeProc)
		p->group->changeProc( p->group, p-p->group->paramPtr, &valL);
}

/**
 * Checks the entered value in a float field. Accepts data entered in the different
 * formats for dimensions. Compares the value against limits if specified in that
 * entry field description.
 *
 * \param val IN the vale to check
 * \param dp  IN the field description
 */

static void ParamFloatPush( const char * val, void * dp )
{
	paramData_p p = (paramData_p)dp;
	FLOAT_T valF;
	BOOL_T valid;
	paramFloatRange_t * frangeP;

	if (p->option & PDO_DIM) {
		valF = DecodeDistance( (wString_p)p->control, &valid );
	} else {
		valF = DecodeFloat( (wString_p)p->control, &valid );
		if (p->option & PDO_ANGLE)
			valF = NormalizeAngle( (angleSystem==ANGLE_POLAR)?valF:-valF );
	}
	// wControlSetBalloon( p->control, 0, -5, NULL );
	if ( !valid ) {
		wControlSetBalloon( p->control, 0, -5, decodeErrorStr );
		return;
	}
	frangeP = (paramFloatRange_t*)p->winData;
	if ( ( (frangeP->rangechecks&PDO_NORANGECHECK_HIGH) == 0 && valF > frangeP->high ) ||
		 ( (frangeP->rangechecks&PDO_NORANGECHECK_LOW) == 0 && valF < frangeP->low ) ) {
		if ( (frangeP->rangechecks&(PDO_NORANGECHECK_HIGH|PDO_NORANGECHECK_LOW)) == PDO_NORANGECHECK_HIGH )
			sprintf( message, _("Enter a value > %s"),
				(p->option&PDO_DIM)?FormatDistance(frangeP->low):FormatFloat(frangeP->low) );
		else if ( (frangeP->rangechecks&(PDO_NORANGECHECK_HIGH|PDO_NORANGECHECK_LOW)) == PDO_NORANGECHECK_LOW )
			 sprintf( message, _("Enter a value < %s"),
				(p->option&PDO_DIM)?FormatDistance(frangeP->high):FormatFloat(frangeP->high) );
		else
			 sprintf( message, _("Enter a value between %s and %s"),
				(p->option&PDO_DIM)?FormatDistance(frangeP->low):FormatFloat(frangeP->low),
				(p->option&PDO_DIM)?FormatDistance(frangeP->high):FormatFloat(frangeP->high) );
		wControlSetBalloon( p->control, 0, -5, message );
		return;
	}
	wControlSetBalloon( p->control, 0, -5, NULL );

	if (recordF && (p->option&PDO_NORECORD)==0 && p->group->nameStr && p->nameStr) {
		fprintf( recordF, "PARAMETER %s %s %0.6f\n", p->group->nameStr, p->nameStr, valF );
		fflush( recordF );
	}
	if ( (p->option&PDO_NOPSHUPD)==0 && p->valueP)
		*((FLOAT_T*)(p->valueP)) = valF;
	if ( (p->option&PDO_NOPSHACT)==0 && p->group->changeProc && strlen( val ))
		p->group->changeProc( p->group, p-p->group->paramPtr, &valF );
}


static void ParamStringPush( const char * val, void * dp )
{
	paramData_p p = (paramData_p)dp;
	if (recordF && (p->option&PDO_NORECORD)==0 && p->group->nameStr && p->nameStr) {
		fprintf( recordF, "PARAMETER %s %s %s\n", p->group->nameStr, p->nameStr, val );
		fflush( recordF );
	}
	if ( (p->option&PDO_NOPSHUPD)==0 && p->valueP)
		strcpy( (char*)p->valueP, val );
	if ( (p->option&PDO_NOPSHACT)==0 && p->group->changeProc)
		p->group->changeProc( p->group, p-p->group->paramPtr, CAST_AWAY_CONST val );
}


static void ParamListPush( wIndex_t inx, const char * val, wIndex_t op, void * dp, void * itemContext )
{
	paramData_p p = (paramData_p)dp;
	long valL;

	switch (p->type) {
	case PD_LIST:
	case PD_DROPLIST:
	case PD_COMBOLIST:
		if (recordF && (p->option&PDO_NORECORD)==0 && p->group->nameStr && p->nameStr) {
			fprintf( recordF, "PARAMETER %s %s %d %s\n", p->group->nameStr, p->nameStr, inx, val );
			fflush( recordF );
		}
		if ( (p->option&PDO_NOPSHUPD)==0 && p->valueP)
			*(wIndex_t*)(p->valueP) = inx;
		if ( (p->option&PDO_NOPSHACT)==0 && p->group->changeProc ) {
			valL = inx;
			p->group->changeProc( p->group, p-p->group->paramPtr, &valL );
		}
		break;

	default:
		;
	}
}


EXPORT void ParamMenuPush( void * dp )
{
	paramData_p p = (paramData_p)dp;
	if (recordF && (p->option&PDO_NORECORD)==0 && p->group->nameStr && p->nameStr) {
		fprintf( recordF, "PARAMETER %s %s\n", p->group->nameStr, p->nameStr );
		fflush( recordF );
	}
	if ( (p->option&PDO_NOPSHACT)==0 && p->valueP )
		((wMenuCallBack_p)(p->valueP))( p->context );
}


static void ParamColorSelectPush( void * dp, wDrawColor dc )
{
	paramData_p p = (paramData_p)dp;
	if (recordF && (p->option&PDO_NORECORD)==0 && p->group->nameStr && p->nameStr) {
		fprintf( recordF, "PARAMETER %s %s %ld\n", p->group->nameStr, p->nameStr, wDrawGetRGB(dc) );
		fflush( recordF );
	}
	if ( (p->option&PDO_NOPSHUPD)==0 && p->valueP)
		*(wDrawColor*)(p->valueP) = dc;
	if ( (p->option&PDO_NOPSHACT)==0 && p->group->changeProc )
		p->group->changeProc( p->group, p-p->group->paramPtr, &dc );
}


static void ParamDrawRedraw( wDraw_p d, void * dp, wPos_t w, wPos_t h )
{
	paramData_p p = (paramData_p)dp;
	paramDrawData_t * ddp = (paramDrawData_t*)p->winData;
	if ( ddp->redraw )
		ddp->redraw( d, p->context, w, h );
}


static void ParamDrawAction( wDraw_p d, void * dp, wAction_t a, wPos_t w, wPos_t h )
{
	paramData_p p = (paramData_p)dp;
	paramDrawData_t * ddp = (paramDrawData_t*)p->winData;
	coOrd pos;
	ddp->d->Pix2CoOrd( ddp->d, w, h, &pos );
	if ( recordF && (p->option&PDO_NORECORD)==0 && p->group->nameStr && p->nameStr) {
		fprintf( recordF, "PARAMETER %s %s %d %0.3f %0.3f\n", p->group->nameStr, p->nameStr, a, pos.x, pos.y );
		fflush( recordF );
	}
	if ( (p->option&PDO_NOPSHACT)== 0 && ddp->action )
		ddp->action( a, pos );
}


static void ParamButtonOk(
		paramGroup_p group )
{
	if ( recordF && group->nameStr )
		fprintf( recordF, "PARAMETER %s %s\n", group->nameStr, "ok" ); {
		fflush( recordF );
	}
	if ( group->okProc )
		group->okProc( group->okProc==(paramActionOkProc)wHide?((void*)group->win):group );
}


static void ParamButtonCancel(
		paramGroup_p group )
{
	if ( recordF && group->nameStr ) {
		fprintf( recordF, "PARAMETER %s %s\n", group->nameStr, "cancel" );
		fflush( recordF );
	}
	if ( group->cancelProc )
		group->cancelProc( group->win );
}


#ifdef LATER
EXPORT void ParamChange( paramData_p p )
{
	FLOAT_T tmpR;

	if (p->valueP==NULL)
		return;

	switch (p->type) {
	case PD_LONG:
		if (recordF && (p->option&PDO_NORECORD)==0 && p->group->nameStr && p->nameStr)
			fprintf( recordF, "PARAMETER %s %s %ld\n", p->group->nameStr, p->nameStr, *(long*)p->valueP );
#ifdef LATER
		if ( p->control && (p->option&PDO_NOCONTUPD) == 0 ) {
			wStringSetValue( (wString_p)p->control, FormatLong( *(long*)p->valueP ) );
		}
#endif
		break;
	case PD_RADIO:
		if (recordF && (p->option&PDO_NORECORD)==0 && p->group->nameStr && p->nameStr)
			fprintf( recordF, "PARAMETER %s %s %ld\n", p->group->nameStr, p->nameStr, *(long*)p->valueP );
#ifdef LATER
		if ( p->control && (p->option&PDO_NOCONTUPD) == 0 )
			wRadioSetValue( (wChoice_p)p->control, *(long*)p->valueP );
#endif
		break;
	case PD_TOGGLE:
		if (recordF && (p->option&PDO_NORECORD)==0 && p->group->nameStr && p->nameStr)
			fprintf( recordF, "PARAMETER %s %s %ld\n", p->group->nameStr, p->nameStr, *(long*)p->valueP );
#ifdef LATER
		if ( p->control && (p->option&PDO_NOCONTUPD) == 0 )
			wToggleSetValue( (wChoice_p)p->control, *(long*)p->valueP );
#endif
		break;
	case PD_LIST:
	case PD_DROPLIST:
	case PD_COMBOLIST:
		if (recordF && (p->option&PDO_NORECORD)==0 && p->group->nameStr && p->nameStr) {
			fprintf( recordF, "PARAMETER %s %s %d %s\n", p->group->nameStr, p->nameStr, *(wIndex_t*)p->valueP, "???" );
		}
#ifdef LATER
		if ( p->control && (p->option&PDO_NOCONTUPD) == 0 )
			wListSetIndex( (wList_p)p->control, *(wIndex_t*)p->valueP );
#endif
		break;
	case PD_COLORLIST:
		if (recordF && (p->option&PDO_NORECORD)==0 && p->group->nameStr && p->nameStr)
			fprintf( recordF, "PARAMETER %s %s %ld\n", p->group->nameStr, p->nameStr, rgb );
		break;
	case PD_FLOAT:
		tmpR = *(FLOAT_T*)p->valueP;
		if (recordF && (p->option&PDO_NORECORD)==0 && p->group->nameStr && p->nameStr)
			fprintf( recordF, "PARAMETER %s %s %0.6f\n", p->group->nameStr, p->nameStr, tmpR );
#ifdef LATER
		if ( p->control && (p->option&PDO_NOCONTUPD) == 0 ) {
			if (p->option&PDO_DIM)
#endif
			if (p->option&PDO_ANGLE)
				tmpR = NormalizeAngle( (angleSystem==ANGLE_POLAR)?tmpR:-tmpR );
			wStringSetValue( (wString_p)p->control, tmpR );
		}
		break;
	case PD_STRING:
		if (recordF && (p->option&PDO_NORECORD)==0 && p->group->nameStr && p->nameStr)
			fprintf( recordF, "PARAMETER %s %s %s\n", p->group->nameStr, p->nameStr, (char*)p->valueP );
#ifdef LATER
		if ( p->control && (p->option&PDO_NOCONTUPD) == 0 )
			wStringSetValue( (wString_p)p->control, (char*)p->valueP );
#endif
		break;
	case PD_MESSAGE:
	case PD_BUTTON:
	case PD_DRAW:
	case PD_TEXT:
	case PD_MENU:
	case PD_MENUITEM:
		break;
	}
}
#endif


EXPORT int paramHiliteFast = FALSE;
EXPORT void ParamHilite(
		wWin_p win,
		wControl_p control,
		BOOL_T hilite )
{
	if ( win != NULL && wWinIsVisible(win) == FALSE ) return;
	if ( control == NULL ) return;
	if ( !paramTogglePlaybackHilite ) return;
	if ( hilite ) {
		wControlHilite( control, TRUE );
		wFlush();
		if ( !paramHiliteFast )
			wPause(500);
	} else {
		if ( !paramHiliteFast )
			wPause(500);
		wControlHilite( control, FALSE );
		wFlush();
	}
}


static void ParamPlayback( char * line )
{
	paramGroup_p pg;
	paramData_p p;
	long valL;
	FLOAT_T valF, valF1;
	int len, len1, len2;
	wIndex_t inx;
	void * listContext, * itemContext;
	long rgb;
	wDrawColor dc;
	wButton_p button;
	paramDrawData_t * ddp;
	wAction_t a;
	coOrd pos;
	char * valS;
	char *oldLocale = NULL;

	if ( strncmp( line, "GROUP ", 6 ) == 0 ) {
#ifdef PGPROC
		for ( inx=0; inx<paramGroups_da.cnt; inx++ ) {
			pg = paramGroups(inx);
			if ( pg->name && strncmp( line+6, pg->name, strlen( pg->name ) ) == 0 ) {
				if ( pg->proc ) {
					pg->proc( PGACT_PARAM, pg->action );
				}
				pg->action = 0;
			}
		}
#endif
		return;
	}

	for ( inx=0; inx<paramGroups_da.cnt; inx++ ) {
	  pg = paramGroups(inx);
	  if ( pg->nameStr == NULL )
		  continue;
	  len1 = strlen( pg->nameStr );
	  if ( strncmp( pg->nameStr, line, len1 ) != 0 ||
		   line[len1] != ' ' )
		  continue;
	  for ( p=pg->paramPtr,inx=0; inx<pg->paramCnt; p++,inx++ ) {
		if ( p->nameStr == NULL )
			continue;
		len2 = strlen( p->nameStr );
		if ( strncmp(p->nameStr, line+len1+1, len2) != 0 ||
			 (line[len1+1+len2] != ' ' && line[len1+1+len2] != '\0') )
			continue;
		len = len1 + 1 + len2 + 1;
		if ( p->type != PD_DRAW && p->type != PD_MESSAGE && p->type != PD_MENU && p->type != PD_MENUITEM )
			ParamHilite( p->group->win, p->control, TRUE );
		switch (p->type) {
			case PD_BUTTON:
				if (p->valueP)
					 ((wButtonCallBack_p)(p->valueP))( p->context );
				if (playbackTimer == 0 && p->control) {
					wButtonSetBusy( (wButton_p)p->control, TRUE );
					wFlush();
					wPause( 500 );
					wButtonSetBusy( (wButton_p)p->control, FALSE );
					wFlush();
				}
				break;
			case PD_LONG:
				valL = atol( line+len );
				if (p->valueP)
					*(long*)p->valueP = valL;
				if (p->control) {
					wStringSetValue( (wString_p)p->control, FormatLong( valL ) );
					wFlush();
				}
				if (pg->changeProc)
					pg->changeProc( pg, inx, &valL );
				break;
			case PD_RADIO:
				valL = atol( line+len );
				if (p->valueP)
					*(long*)p->valueP = valL;
				if (p->control) {
					wRadioSetValue( (wChoice_p)p->control, valL );
					wFlush();
				}
				if (pg->changeProc)
					pg->changeProc( pg, inx, &valL );
				break;
			case PD_TOGGLE:
				valL = atol( line+len );
				if (p->valueP)
					*(long*)p->valueP = valL;
				if (p->control) {
					wToggleSetValue( (wChoice_p)p->control, valL );
					wFlush();
				}
				if (pg->changeProc)
					pg->changeProc( pg, inx, &valL );
				break;
			case PD_LIST:
			case PD_DROPLIST:
			case PD_COMBOLIST:
				line += len;
				valL = strtol( line, &valS, 10 );
				if ( valS )
					valS++;
				else
					valS = "";
				if ( p->control != NULL ) {
					if ( (p->option&PDO_LISTINDEX) == 0 ) {
						if ( valL < 0 ) {
							wListSetValue( (wList_p)p->control, valS );
						} else {
							valL = wListFindValue( (wList_p)p->control, valS );
							if (valL < 0) {
								NoticeMessage( MSG_PLAYBACK_LISTENTRY, _("Ok"), NULL, line );
								break;
							}
							wListSetIndex( (wList_p)p->control, (wIndex_t)valL );
						}
					} else {
						wListSetIndex( (wList_p)p->control, (wIndex_t)valL );
					}
					wFlush();
					wListGetValues( (wList_p)p->control, message, sizeof message, &listContext, &itemContext );
				} else if ( (p->option&PDO_LISTINDEX) == 0 ) {
					break;
				}
				if (p->valueP)
					*(wIndex_t*)p->valueP = (wIndex_t)valL;
				if (pg->changeProc) {
					pg->changeProc( pg, inx, &valL );
				}
				break;
			case PD_COLORLIST:
				line += len;
				rgb = atol( line );
				dc = wDrawFindColor( rgb );
				if ( p->control)
					wColorSelectButtonSetColor( (wButton_p)p->control, dc );
				if (p->valueP)
					*(wDrawColor*)p->valueP = dc;
				if (pg->changeProc) {
					/* COLORNOP */
					pg->changeProc( pg, inx, &valL );
				}
				break;
			case PD_FLOAT:
				oldLocale = SaveLocale("C");
				valF = valF1 = atof( line+len );
				RestoreLocale(oldLocale);
				if (p->valueP)
					*(FLOAT_T*)p->valueP = valF;
				if (p->option&PDO_DIM) {
					if ( p->option&PDO_SMALLDIM )
						valS = FormatSmallDistance( valF );
					else
						valS = FormatDistance( valF );
				} else {
					if (p->option&PDO_ANGLE)
						valF1 = NormalizeAngle( (angleSystem==ANGLE_POLAR)?valF1:-valF1 );
					valS = FormatFloat( valF );
				}
				if (p->control) {
					wStringSetValue( (wString_p)p->control, valS );
					wFlush();
				}
				if (pg->changeProc)
					pg->changeProc( pg, inx, &valF );
				break;
			case PD_STRING:
				line += len;
				while ( *line == ' ' ) line++;
				Stripcr( line );
				if (p->valueP)
					strcpy( (char*)p->valueP, line );
				if (p->control) {
					wStringSetValue( (wString_p)p->control, line );
					wFlush();
				}
				if (pg->changeProc)
					pg->changeProc( pg, inx, line );
				break;
			case PD_DRAW:
				ddp = (paramDrawData_t*)p->winData;
				if ( ddp->action == NULL )
					break;
				a = (wAction_t)strtol( line+len, &line, 10 );
				pos.x = strtod( line, &line );
				pos.y = strtod( line, NULL );
				PlaybackMouse( ddp->action, ddp->d, a, pos, drawColorBlack );
				break;
			case PD_MESSAGE:
			case PD_TEXT:
			case PD_MENU:
			case PD_BITMAP:
				break;
			case PD_MENUITEM:
				if (p->valueP) {
					if ( (p->option&IC_PLAYBACK_PUSH) != 0 )
						PlaybackButtonMouse( (wIndex_t)(long)p->context );
					((wButtonCallBack_p)(p->valueP))( p->context );
				}
				break;
			}
		if ( p->type != PD_DRAW && p->type != PD_MESSAGE && p->type != PD_MENU && p->type != PD_MENUITEM )
			ParamHilite( p->group->win, p->control, FALSE );
#ifdef HUH
		pg->action |= p->change;
#endif
		return;
	}
	button = NULL;
	if ( strcmp("ok", line+len1+1) == 0 ) {
		ParamHilite( pg->win, (wControl_p)pg->okB, TRUE );
		if ( pg->okProc )
			pg->okProc( pg );
		button = pg->okB;
	} else if ( strcmp("cancel", line+len1+1) == 0 ) {
		ParamHilite( pg->win, (wControl_p)pg->cancelB, TRUE );
		if ( pg->cancelProc )
			pg->cancelProc( pg->win );
		button = pg->cancelB;
	}
	if ( playbackTimer == 0 && button ) {
		wButtonSetBusy( button, TRUE );
		wFlush();
		wPause( 500 );
		wButtonSetBusy( button, FALSE );
		wFlush();
	}
	ParamHilite( pg->win, (wControl_p)button, FALSE );
	if ( !button )
		NoticeMessage( "Unknown PARAM: %s", _("Ok"), NULL, line );
	return;
  }
  NoticeMessage( "Unknown PARAM: %s", _("Ok"), NULL, line );
}


static void ParamCheck( char * line )
{
	paramGroup_p pg;
	paramData_p p;
	long valL;
	FLOAT_T valF, diffF;
	int len, len1, len2;
	wIndex_t inx;
	void * listContext, * itemContext;
	char * valS;
	char * expVal=NULL, * actVal=NULL;
	char expNum[20], actNum[20];
	BOOL_T hasError = FALSE;
	FILE * f;

	for ( inx=0; inx<paramGroups_da.cnt; inx++ ) {
	  pg = paramGroups(inx);
	  if ( pg->nameStr == NULL )
		  continue;
	  len1 = strlen( pg->nameStr );
	  if ( strncmp( pg->nameStr, line, len1 ) != 0 ||
		   line[len1] != ' ' )
		  continue;
	  for ( p=pg->paramPtr,inx=0; inx<pg->paramCnt; p++,inx++ ) {
		if ( p->nameStr == NULL )
			continue;
		len2 = strlen( p->nameStr );
		if ( strncmp(p->nameStr, line+len1+1, len2) != 0 ||
			 (line[len1+1+len2] != ' ' && line[len1+1+len2] != '\0') )
			continue;
		if ( p->valueP == NULL )
			return;
		len = len1 + 1 + len2 + 1;
		switch (p->type) {
			case PD_BUTTON:
				break;
			case PD_LONG:
			case PD_RADIO:
			case PD_TOGGLE:
				valL = atol( line+len );
				if ( *(long*)p->valueP != valL ) {
					sprintf( expNum, "%ld", valL );
					sprintf( actNum, "%ld", *(long*)p->valueP );
					expVal = expNum;
					actVal = actNum;
					hasError = TRUE;
				}
				break;
			case PD_LIST:
			case PD_DROPLIST:
			case PD_COMBOLIST:
				line += len;
				if ( p->control == NULL )
					break;
				valL = strtol( line, &valS, 10 );
				if ( valS ) {
					if ( valS[0] == ' ' )
						valS++;
				} else {
					valS = "";
				}
				if ( (p->option&PDO_LISTINDEX) != 0 ) {
					if ( *(long*)p->valueP != valL ) {
						sprintf( expNum, "%ld", valL );
						sprintf( actNum, "%d", *(wIndex_t*)p->valueP );
						expVal = expNum;
						actVal = actNum;
						hasError = TRUE;
					}
				} else {
					wListGetValues( (wList_p)p->control, message, sizeof message, &listContext, &itemContext );
					if ( strcasecmp( message, valS ) != 0 ) {
						expVal = valS;
						actVal = message;
						hasError = TRUE;
					}
				}
				break;
			case PD_COLORLIST:
				break;
			case PD_FLOAT:
				valF = atof( line+len );
				diffF = fabs( *(FLOAT_T*)p->valueP - valF );
				if ( diffF > 0.001 ) {
					sprintf( expNum, "%0.3f", valF );
					sprintf( actNum, "%0.3f", *(FLOAT_T*)p->valueP );
					expVal = expNum;
					actVal = actNum;
					hasError = TRUE;
				}
				break;
			case PD_STRING:
				line += len;
				while ( *line == ' ' ) line++;
				valS = CAST_AWAY_CONST wStringGetValue( (wString_p)p->control );
				if ( strcasecmp( line, (char*)p->valueP ) != 0 ) {
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
		if ( hasError ) {
			f = fopen( "error.log", "a" );
			if ( f==NULL ) {
				NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, "PARAMCHECK LOG", "error.log", strerror(errno) );
			} else {
				fprintf( f, "CHECK: %s:%d: %s-%s: exp: %s, act=%s\n",
						paramFileName, paramLineNum, pg->nameStr, p->nameStr, expVal, actVal );
				fclose( f );
			}
			if ( paramCheckShowErrors )
				NoticeMessage( "CHECK: %d: %s-%s: exp: %s, act=%s", _("Ok"), NULL, paramLineNum, pg->nameStr, p->nameStr, expVal, actVal );
			paramCheckErrorCount++;
		}
		return;
	 }
  }
  NoticeMessage( "Unknown PARAMCHECK: %s", _("Ok"), NULL, line );
}

/*
 *
 */



static void ParamCreateControl(
		paramData_p pd,
		char * helpStr,
		wPos_t xx,
		wPos_t yy )
{
	paramFloatRange_t * floatRangeP;
	paramIntegerRange_t * integerRangeP;
	paramDrawData_t * drawDataP;
	paramTextData_t * textDataP;
	paramListData_t * listDataP;
	wIcon_p iconP;

	wWin_p win;
	wPos_t w;
	wPos_t colWidth;
	static wPos_t *colWidths;
	static wBool_t *colRightJust;
	static wBool_t maxColCnt = 0;
	int col;
	const char *cp;
    char *cq;
	static wMenu_p menu = NULL;

	if ( ( win = pd->group->win ) == NULL )
		win = mainW;


		switch (pd->type) {
		case PD_FLOAT:
			floatRangeP = pd->winData;
			w = floatRangeP->width?floatRangeP->width:100;
			pd->control = (wControl_p)wStringCreate( win, xx, yy, helpStr, _(pd->winLabel), pd->winOption, w, NULL, 0, ParamFloatPush, pd );
			break;
		case PD_LONG:
			integerRangeP = pd->winData;
			w = integerRangeP->width?integerRangeP->width:100;
			pd->control = (wControl_p)wStringCreate( win, xx, yy, helpStr, _(pd->winLabel), pd->winOption, w, NULL, 0, ParamIntegerPush, pd );
			break;
		case PD_STRING:
			w = pd->winData?(wPos_t)(long)pd->winData:(wPos_t)250;
			pd->control = (wControl_p)wStringCreate( win, xx, yy, helpStr, _(pd->winLabel), pd->winOption, w, (pd->option&PDO_NOPSHUPD)?NULL:pd->valueP, 0, ParamStringPush, pd );
			break;
		case PD_RADIO:
			pd->control = (wControl_p)wRadioCreate( win, xx, yy, helpStr, _(pd->winLabel), pd->winOption, pd->winData, NULL, ParamChoicePush, pd );
			break;
		case PD_TOGGLE:
			pd->control = (wControl_p)wToggleCreate( win, xx, yy, helpStr, _(pd->winLabel), pd->winOption, pd->winData, NULL, ParamChoicePush, pd );
			break;
		case PD_LIST:
			listDataP = (paramListData_t*)pd->winData;
			if ( listDataP->colCnt > 1 ) {
				if ( maxColCnt < listDataP->colCnt ) {
					if ( maxColCnt == 0 ) {
						colWidths = (wPos_t*)MyMalloc( listDataP->colCnt * sizeof *colWidths );
						colRightJust = (wBool_t*)MyMalloc( listDataP->colCnt * sizeof *colRightJust );
					} else {
						colWidths = (wPos_t*)MyRealloc( colWidths, listDataP->colCnt * sizeof *colWidths );
						colRightJust = (wBool_t*)MyRealloc( colRightJust, listDataP->colCnt * sizeof *colRightJust );
					}
					maxColCnt = listDataP->colCnt;
				}
				for ( col=0; col<listDataP->colCnt; col++ ) {
					colRightJust[col] = listDataP->colWidths[col]<0;
					colWidths[col] = abs(listDataP->colWidths[col]);
				}
				sprintf( message, "%s-%s-%s", pd->group->nameStr, pd->nameStr, "columnwidths" );
				cp = wPrefGetString( PREFSECT, message );
				if ( cp != NULL ) {
				for ( col=0; col<listDataP->colCnt; col++ ) {
					colWidth = (wPos_t)strtol( cp, &cq, 10 );
					if ( cp == cq )
						break;
					colWidths[col] = colWidth;
					cp = cq;
					}
				}
			}
			pd->control = (wControl_p)wListCreate( win, xx, yy, helpStr, _(pd->winLabel),
				pd->winOption, listDataP->number, listDataP->width, listDataP->colCnt,
				(listDataP->colCnt>1?colWidths:NULL),
				(listDataP->colCnt>1?colRightJust:NULL),
				listDataP->colTitles, NULL, ParamListPush, pd );
			listDataP->height = wControlGetHeight( pd->control );
			break;
		case PD_DROPLIST:
			w = pd->winData?(wPos_t)(long)pd->winData:(wPos_t)100;
			pd->control = (wControl_p)wDropListCreate( win, xx, yy, helpStr, _(pd->winLabel), pd->winOption, 10, w, NULL, ParamListPush, pd );
			break;
		case PD_COMBOLIST:
			listDataP = (paramListData_t*)pd->winData;
			pd->control = (wControl_p)wComboListCreate( win, xx, yy, helpStr, _(pd->winLabel), pd->winOption, listDataP->number, listDataP->width, NULL, ParamListPush, pd );
			listDataP->height = wControlGetHeight( pd->control );
			break;
		case PD_COLORLIST:
			pd->control = (wControl_p)wColorSelectButtonCreate( win, xx, yy, helpStr, _(pd->winLabel), pd->winOption, 0, NULL, ParamColorSelectPush, pd );
            break;
		case PD_MESSAGE:
			if ( pd->winData != 0 )
				w = (wPos_t)(long)pd->winData;
			else if (pd->valueP)
				w = wLabelWidth( _(pd->valueP) );
			else
				w = 150;
			pd->control = (wControl_p)wMessageCreateEx( win, xx, yy, _(pd->winLabel), w, pd->valueP?_(pd->valueP):" ", pd->winOption );
			break;
		case PD_BUTTON:
			pd->control = (wControl_p)wButtonCreate( win, xx, yy, helpStr, _(pd->winLabel), pd->winOption, 0, ParamButtonPush, pd );
			break;
		case PD_MENU:
			menu = wMenuCreate( win, xx, yy, helpStr, _(pd->winLabel), pd->winOption );
			pd->control = (wControl_p)menu;
			break;
		case PD_MENUITEM:
			pd->control = (wControl_p)wMenuPushCreate( menu, helpStr, _(pd->winLabel), 0, ParamMenuPush, pd );
			break;
		case PD_DRAW:
			drawDataP = pd->winData;
			pd->control = (wControl_p)wDrawCreate( win, xx, yy, helpStr, pd->winOption, drawDataP->width, drawDataP->height, pd, ParamDrawRedraw, ParamDrawAction );
			if ( drawDataP->d ) {
				drawDataP->d->d = (wDraw_p)pd->control;
				drawDataP->d->dpi = wDrawGetDPI( drawDataP->d->d );
			}
			break;
		case PD_TEXT:
			textDataP = pd->winData;
			pd->control = (wControl_p)wTextCreate( win, xx, yy, helpStr, NULL, pd->winOption, textDataP->width, textDataP->height );
			if ( (pd->winOption&BO_READONLY) == 0 )
				wTextSetReadonly( (wText_p)pd->control, FALSE );
			break;
		case PD_BITMAP:
			iconP = pd->winData;
			pd->control = (wControl_p)wBitmapCreate( win, xx, yy, pd->winOption, iconP );
			break;
		default:
			AbortProg( "paramCreatePG" );
		}

}


static void ParamPositionControl(
		paramData_p pd,
		char * helpStr,
		wPos_t xx,
		wPos_t yy )
{
	paramDrawData_t * drawDataP;
	paramTextData_t * textDataP;
	paramListData_t * listDataP;
	wPos_t winW, winH, ctlW, ctlH;

	if ( pd->type != PD_MENUITEM )
		wControlSetPos( pd->control, xx, yy );
	if ( pd->option&PDO_DLGRESIZE ) {
		wWinGetSize( pd->group->win, &winW, &winH );
		switch (pd->type) {
		case PD_LIST:
		case PD_COMBOLIST:
		case PD_DROPLIST:
			if ( pd->type == PD_DROPLIST ) {
				ctlW = pd->winData?(wPos_t)(long)pd->winData:(wPos_t)100;
				ctlH = wControlGetHeight( pd->control );
			} else {
				listDataP = (paramListData_t*)pd->winData;
				ctlW = listDataP->width;
				ctlH = listDataP->height;
			}
			if ( (pd->option&PDO_DLGRESIZE) == 0 )
				break;
			if ( (pd->option&PDO_DLGRESIZEW) != 0 )
				ctlW = winW - (pd->group->origW-ctlW);
			if ( (pd->option&PDO_DLGRESIZEH) != 0 )
				ctlH = winH - (pd->group->origH-ctlH);
			wListSetSize( (wList_p)pd->control, ctlW, ctlH );
			break;
		case PD_DRAW:
			drawDataP = pd->winData;
			if ( (pd->option&PDO_DLGRESIZEW) )
				ctlW = winW - (pd->group->origW-drawDataP->width);
			else
				ctlW = wControlGetWidth( pd->control );
			if ( (pd->option&PDO_DLGRESIZEH) )
				ctlH = winH - (pd->group->origH-drawDataP->height);
			else
				ctlH = wControlGetHeight( pd->control );
			wDrawSetSize( (wDraw_p)pd->control, ctlW, ctlH );
			if ( drawDataP->redraw )
				drawDataP->redraw( (wDraw_p)pd->control, pd->context, ctlW, ctlH );
			break;
		case PD_TEXT:
			textDataP = pd->winData;
			ctlW = textDataP->width;
			ctlH = textDataP->height;
			if ( (pd->winOption&BT_CHARUNITS) )
				wTextComputeSize( (wText_p)pd->control, ctlW, ctlH, &ctlW, &ctlH );
			if ( (pd->option&PDO_DLGRESIZEW) )
				ctlW = winW - (pd->group->origW-ctlW);
			else
				ctlW = wControlGetWidth( pd->control );
			if ( (pd->option&PDO_DLGRESIZEH) )
				ctlH = winH - (pd->group->origH-ctlH);
			else
				ctlH = wControlGetHeight( pd->control );
			wTextSetSize( (wText_p)pd->control, ctlW, ctlH );
			break;
		case PD_STRING:
			ctlW = pd->winData?(wPos_t)(long)pd->winData:(wPos_t)250;
			if ( (pd->option&PDO_DLGRESIZEW) ) {
				ctlW = winW - (pd->group->origW-ctlW);
				wStringSetWidth( (wString_p)pd->control, ctlW );
			}
			break;
		case PD_MESSAGE:
			ctlW = pd->winData?(wPos_t)(long)pd->winData:(wPos_t)150;
			if ( (pd->option&PDO_DLGRESIZEW) ) {
				ctlW = winW - (pd->group->origW-ctlW);
				wMessageSetWidth( (wMessage_p)pd->control, ctlW );
			}
			break;
		default:
			AbortProg( "paramPositionControl" );
		}
	}
}


typedef void (*layoutControlsProc)(paramData_p, char *, wPos_t, wPos_t );
static void LayoutControls(
		paramGroup_p group,
		layoutControlsProc proc,
		wPos_t * retW,
		wPos_t * retH )
{
	struct {
		struct { wPos_t x, y; } orig, term;
	} controlK, columnK, windowK;
	wPos_t controlSize_x;
	wPos_t controlSize_y;
	paramData_p pd;
	wPos_t w;
	BOOL_T hasBox;
	wPos_t boxTop;
	wPos_t boxPos[10];
	int boxCnt = 0;
	int box;
	int inx;
	wPos_t labelW[100];
	int lastLabelPos, currLabelPos;
	char helpStr[STR_SHORT_SIZE], * helpStrP;
	BOOL_T inCmdButtons = FALSE;
	wButton_p lastB = NULL;
	BOOL_T areCmdButtons = FALSE;

	strcpy( helpStr, group->nameStr );
	helpStrP = helpStr+strlen(helpStr);
	*helpStrP++ = '-';
	*helpStrP = 0;
	controlK.orig.x = 0;
	hasBox = FALSE;
	memset( boxPos, 0, sizeof boxPos );
	memset( labelW, 0, sizeof labelW );
	lastLabelPos = 0;
	currLabelPos = 0;
	for ( pd=group->paramPtr; pd<&group->paramPtr[group->paramCnt]; pd++,currLabelPos++ ) {
		if ( (pd->option&PDO_DLGIGNORE) != 0 )
			continue;
		if ( (pd->option&PDO_DLGBOXEND) )
			hasBox = TRUE;
		if ( (pd->option&(PDO_DLGRESETMARGIN|PDO_DLGNEWCOLUMN|PDO_DLGCMDBUTTON)) ) {
			for ( inx=lastLabelPos; inx<currLabelPos; inx++ )
				labelW[inx] = controlK.orig.x;
			controlK.orig.x = 0;
			lastLabelPos = currLabelPos;
		}
		if ( pd->winLabel && (pd->option&(PDO_DLGIGNORELABELWIDTH|PDO_DLGHORZ))==0 &&
			 pd->type!=PD_BUTTON &&
			 pd->type!=PD_MENU &&
			 pd->type!=PD_MENUITEM) {
			w = wLabelWidth( _(pd->winLabel) );
			if ( w > controlK.orig.x )
				controlK.orig.x = w;
		}
	}
	for ( inx=lastLabelPos; inx<group->paramCnt; inx++ )
		labelW[inx] = controlK.orig.x;
	for ( inx=0; inx<group->paramCnt; inx++ )
		if ( (group->paramPtr[inx].option&PDO_DLGNOLABELALIGN) != 0 )
			labelW[inx] = 0;

	LOG( log_paramLayout, 1, ("Layout %s B?=%s\n", group->nameStr, hasBox?"T":"F" ) )

	windowK.orig.x =  DlgSepLeft + (hasBox?DlgSepFrmLeft:0);
	windowK.orig.y = DlgSepTop + (hasBox?DlgSepFrmTop:0);
	windowK.term = windowK.orig;
	controlK = columnK = windowK;
	controlK.orig.x += labelW[0];

	for ( pd = group->paramPtr,inx=0; pd<&group->paramPtr[group->paramCnt]; pd++,inx++ ) {
		LOG( log_paramLayout, 1, ("%2d: Col %dx%d..%dx%d Ctl %dx%d..%dx%d\n", inx,
			columnK.orig.x, columnK.orig.y, columnK.term.x, columnK.term.y,
			controlK.orig.x, controlK.orig.y, controlK.term.x, controlK.term.y ) )
		if ( (pd->option&PDO_DLGIGNORE) != 0 )
			goto SkipControl;
		if ( pd->type == PD_MENUITEM ) {
			proc( pd, helpStr, 0, 0 );
			continue;
		}
		/*
		 * Set control orig
		 */
		if ( (pd->option&PDO_DLGNEWCOLUMN) ) {
			columnK.orig.x = columnK.term.x;
			columnK.orig.x += ((pd->option&PDO_DLGWIDE)?10:DlgSepNarrow);
			columnK.term.y = columnK.orig.y;
			controlK.orig.x = columnK.orig.x + labelW[inx];
			controlK.orig.y = columnK.orig.y;
		} else if ( (pd->option&PDO_DLGHORZ) ) {
			controlK.orig.x = controlK.term.x;
			if ( (pd->option&PDO_DLGWIDE) )
				controlK.orig.x += 10;
			else if ( (pd->option&PDO_DLGNARROW)== 0)
				controlK.orig.x += 3;
			if ( pd->winLabel && ( pd->type!=PD_BUTTON ) )
				controlK.orig.x += wLabelWidth( _(pd->winLabel) );
		} else if ( inx != 0 ) {
			controlK.orig.x = columnK.orig.x + labelW[inx];
			controlK.orig.y = controlK.term.y;
			if ( (pd->option&PDO_DLGWIDE) )
				controlK.orig.y += 10;
			else if ( (pd->option&PDO_DLGNARROW)== 0)
				controlK.orig.y += 3;
		}
		if ( (pd->option&PDO_DLGSETY) ) {
			columnK.term.x = controlK.orig.x;
			columnK.orig.y = controlK.orig.y;
		}
		/*
		 * Custom layout and create/postion control
		 */
		if (group->layoutProc)
			group->layoutProc( pd, inx, columnK.orig.x+labelW[inx], &controlK.orig.x, &controlK.orig.y );
		if ( pd->nameStr )
			strcpy( helpStrP, pd->nameStr );
		proc( pd, helpStr, controlK.orig.x, controlK.orig.y );
		/*
		 * Set control term
		 */
		controlSize_x = wControlGetWidth( pd->control );
		controlSize_y = wControlGetHeight( pd->control );
		controlK.term.x = controlK.orig.x+controlSize_x;
		if ( (pd->option&PDO_DLGHORZ)==0 ||
			 controlK.term.y < controlK.orig.y+controlSize_y )
			controlK.term.y = controlK.orig.y+controlSize_y;
		if ( retW && pd->nameStr ) {
			char * cp;
			strcpy( message, pd->nameStr );
			for ( cp=message; *cp; cp++ ) if ( *cp == '-' ) *cp = '_';
		}
		/*
		 * Set column term
		 */
		if ( (pd->option&PDO_DLGIGNOREX) == 0 ) {
			if ( (pd->option&PDO_DLGUNDERCMDBUTT) == 0 ) {
				if ( columnK.term.x < controlK.term.x )
					columnK.term.x = controlK.term.x;
			} else {
				if ( columnK.term.x < controlK.term.x-90 )
					columnK.term.x = controlK.term.x-90;
			}
		}
		if ( columnK.term.y < controlK.term.y )
			columnK.term.y = controlK.term.y;
		if ( hasBox )
			if ( boxPos[boxCnt] < columnK.term.y+2 )
				boxPos[boxCnt] = columnK.term.y+2;
		if ( (pd->option&PDO_DLGBOXEND) ) {
			columnK.term.y += 8;
			boxCnt++;
			controlK.term.y = columnK.term.y;
		}
		/*
		 * Set window term
		 */
		if ( windowK.term.x < columnK.term.x )
			windowK.term.x = columnK.term.x;
		if ( windowK.term.y < columnK.term.y )
			windowK.term.y = columnK.term.y;
		if ( (pd[1].option&PDO_DLGCMDBUTTON) )
			areCmdButtons = TRUE;
SkipControl:
		if ( (!inCmdButtons) &&
			 (pd==&group->paramPtr[group->paramCnt-1] || (pd[1].option&PDO_DLGCMDBUTTON)) ) {
			columnK.orig.x = columnK.term.x + DlgSepMid;
			if ( boxCnt ) {
				boxTop = DlgSepTop;
				if ( group->boxs == NULL ) {
					group->boxs = (wBox_p*)MyMalloc( boxCnt * sizeof *(wBox_p*)0 );
					for ( box=0; box<boxCnt; box++ ) {
						group->boxs[box] = wBoxCreate( group->win, DlgSepLeft, boxTop, NULL, wBoxBelow, columnK.term.x, boxPos[box]-boxTop );
						boxTop = boxPos[box] + 4;
					}
				} else {
					for ( box=0; box<boxCnt; box++ ) {
						wControlSetPos( (wControl_p)group->boxs[box], DlgSepLeft, boxTop );
						wBoxSetSize( group->boxs[box], columnK.term.x, boxPos[box]-boxTop );
						boxTop = boxPos[box] + 4;
					}
				}
				columnK.orig.x += DlgSepFrmRight;
			}
			columnK.orig.y = columnK.term.y = DlgSepTop;
			controlK = columnK;
			if ( group->okB ) {
				wControlSetPos( (wControl_p)(lastB=group->okB), columnK.orig.x, columnK.orig.y );
				controlK.term.y += wControlGetHeight((wControl_p)group->okB);
				columnK.term.y = controlK.term.y + 3;
			}
			inCmdButtons = TRUE;
		}
		LOG( log_paramLayout, 1, ("    Col %dx%d..%dx%d Ctl %dx%d..%dx%d\n",
				columnK.orig.x, columnK.orig.y, columnK.term.x, columnK.term.y,
				controlK.orig.x, controlK.orig.y, controlK.term.x, controlK.term.y ) )
		if ( windowK.term.x < columnK.term.x )
			windowK.term.x = columnK.term.x;
		if ( windowK.term.y < columnK.term.y )
			windowK.term.y = columnK.term.y;
	}
	if ( group->cancelB ) {
		if ( areCmdButtons )
			columnK.term.y += 10;
		else if ( group->okB )
			columnK.term.y += 3;
		wControlSetPos( (wControl_p)(lastB=group->cancelB), columnK.orig.x, columnK.term.y );
		columnK.term.y += wControlGetHeight((wControl_p)group->cancelB);
	}
	if ( group->helpB ) {
		columnK.term.y += 10;
		wControlSetPos( (wControl_p)(lastB=group->helpB), columnK.orig.x, columnK.term.y );
		columnK.term.y += wControlGetHeight((wControl_p)group->helpB);
	}
	if ( lastB ) {
		controlK.term.x = controlK.orig.x + wControlGetWidth((wControl_p)lastB);
		if ( columnK.term.x < controlK.term.x )
			columnK.term.x = controlK.term.x;
	}
	if ( windowK.term.x < columnK.term.x )
		windowK.term.x = columnK.term.x;
	if ( windowK.term.y < columnK.term.y )
		windowK.term.y = columnK.term.y;

	if ( retW )
		*retW = windowK.term.x;
	if ( retH )
		*retH = windowK.term.y;
}


static void ParamDlgProc(
		wWin_p win,
		winProcEvent e,
		void * data )
{
	paramGroup_p pg = (paramGroup_p)data;
	switch (e) {
	case wClose_e:
		if ( pg->changeProc )
			pg->changeProc( pg, -1, NULL );
		if ( (pg->options&PGO_NODEFAULTPROC) == 0 )
			DefaultProc( win, wClose_e, data );
		break;
	case wResize_e:
		LayoutControls( pg, ParamPositionControl, NULL, NULL );
		break;
	default:
		break;
	}
}

/**
 * Create a dialog box from data definition. 
 *
 * \param IN group	data definition for the dialog
 * \param IN title  title of the new dialog
 * \param IN okLabel text for the affirmative button
 * \param IN okProc	subroutine to call when ok is pressed
 * \param IN cancelProc if not NULL a subroutine for Cancel event. If NULL no cancel button is created
 * \param IN needHelpButton if TRUE a help button is created
 * \param IN layoutProc ???
 * \param IN winOption ???
 * \param IN changeProc ???
 */

wWin_p ParamCreateDialog(
		paramGroup_p group,
		char * title,
		char * okLabel,
		paramActionOkProc okProc,
		paramActionCancelProc cancelProc,
		BOOL_T needHelpButton,
		paramLayoutProc layoutProc,
		long winOption,
		paramChangeProc changeProc )
{
	char helpStr[STR_SHORT_SIZE];
	wPos_t w0, h0;
	char * cancelLabel = (winOption&PD_F_ALT_CANCELLABEL?_("Close"):_("Cancel"));

	winOption &= ~PD_F_ALT_CANCELLABEL;
	group->okProc = okProc;
	group->cancelProc = cancelProc;
	group->layoutProc = layoutProc;
	group->changeProc = changeProc;
	if ( (winOption&F_CENTER) == 0 )
		winOption |= F_RECALLPOS;
	if ( (winOption&F_RESIZE) != 0 )
		winOption |= F_RECALLSIZE;

	group->win = wWinPopupCreate( mainW, DlgSepRight, DlgSepFrmBottom, helpStr, title, group->nameStr, F_AUTOSIZE|winOption, ParamDlgProc, group );

	if ( okLabel && okProc ) {
		sprintf( helpStr, "%s-ok", group->nameStr );
		group->okB = wButtonCreate( group->win, 0, 0, helpStr, okLabel, BB_DEFAULT, 0, (wButtonCallBack_p)ParamButtonOk, group );
	}
	if ( group->cancelProc ) {
		group->cancelB = wButtonCreate( group->win, 0, 0, NULL, cancelLabel, BB_CANCEL, 0, (wButtonCallBack_p)ParamButtonCancel, group );
	}
	if ( needHelpButton ) {
		sprintf( helpStr, "cmd%s", group->nameStr );
		helpStr[3] = toupper((unsigned char)helpStr[3]);
		group->helpB = wButtonCreate( group->win, 0, 0, NULL, _("Help"), BB_HELP, 0, (wButtonCallBack_p)wHelp, MyStrdup(helpStr) );
	}

	LayoutControls( group, ParamCreateControl, &group->origW, &group->origH );

	group->origW += DlgSepRight;
	group->origH += DlgSepBottom;
	wWinGetSize( group->win, &w0, &h0 );
	if ( (winOption&F_RESIZE) ) {
		if ( group->origW != w0 ||
			 group->origH != h0 ) {
			LayoutControls( group, ParamPositionControl, NULL, NULL );
		}
	} else {
		w0 = max(group->origW, w0);
		h0 = max(group->origH, h0);
		wWinSetSize( group->win, w0, h0 );
	}

	return group->win;
}


/** Resize dialog window for the contained fields.
* \param IN OUT Prameter Group
*
*/
EXPORT void ParamLayoutDialog(
		paramGroup_p pg )
{
	wPos_t w, h;
	LayoutControls( pg, ParamPositionControl, &w, &h );
	w += DlgSepRight;
	h += DlgSepBottom;
	if ( w != pg->origW || h != pg->origH ) {
		wWinSetSize( pg->win, w, h );
		pg->origW = w;
		pg->origH = h;
	}
}


EXPORT void ParamDialogOkActive(
		paramGroup_p pg,
		int active )
{
	if ( pg->okB )
		wControlActive( (wControl_p)pg->okB, active );
}


EXPORT void ParamCreateControls(
		paramGroup_p pg,
		paramChangeProc changeProc )
{
	paramData_p pd;
	char helpStr[STR_SHORT_SIZE], * helpStrP;
	strcpy( helpStr, pg->nameStr );
	helpStrP = helpStr+strlen(helpStr);
	*helpStrP++ = '-';
	for ( pd=pg->paramPtr; pd<&pg->paramPtr[pg->paramCnt]; pd++ ) {
		pd->group = pg;
		strcpy( helpStrP, pd->nameStr );
		ParamCreateControl( pd, helpStr, 0, 0 );
		if ( pd->type != PD_MENUITEM && pd->control )
			wControlShow( pd->control, FALSE );
	}
	pg->changeProc = changeProc;
}


EXPORT void ParamInit( void )
{
	AddPlaybackProc( "PARAMETER", ParamPlayback, NULL );
	AddPlaybackProc( "PARAMCHECK", ParamCheck, NULL );
	log_paramLayout = LogFindIndex( "paramlayout" );
}
