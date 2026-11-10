/** \file careditlogic.c
 * Car Edit Logic
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "carsprivate.h"


/* A combo's match-state is only ever RS_NEW for genuinely typed text -- an
 * unmatched index with an empty string means "untouched", not "new", and
 * must not open a nested create panel or light up a "-new" hint label. */
resState_e
CarDlgResState( wIndex_t inx, const char *str )
{
	if ( inx >= 0 ) { return RS_RESOLVED; }
	return ( str[0] != '\0' ) ? RS_NEW : RS_UNRESOLVED;
}

carDateResult_e
CarValidateDate(const char *str, long *result)
{
	const char *cp;
	long valL = 0;
	carDateResult_e resCode = CAR_DATE_OK;

	for (cp = str; *cp && isspace(*(unsigned char *)cp); cp++)
		;
	if (*cp) {
		char *cq;

		valL = strtol(cp, &cq, 10);
		if (cq == NULL || *cq != '\0') {
			resCode = CAR_DATE_NOT_NUMERIC;
		} else {
			if (strlen(cp) != 8 || valL == 0) {
				resCode = CAR_DATE_WRONG_LENGTH;
			} else if (valL < 19000101 || valL > 21991231) {
				resCode = CAR_DATE_OUT_OF_RANGE;
			} else {
				long d;
				long m;
				d = valL % 100;
				m = (valL / 100) % 100;
				if (m < 1 || m > 12) {
					resCode = CAR_DATE_BAD_MONTH;
				} else if (d < 1 || d > 31) {
					resCode = CAR_DATE_BAD_DAY;
				}
			}
		}
	} else {
		resCode = CAR_DATE_EMPTY;
	}

	*result =valL;
	return resCode;
}

wBool_t CarValidatePrice(const char *str, FLOAT_T *price)
{
	wBool_t ok = TRUE;
	char *cp;

	*price = strtod(str, &cp);
	if (cp == NULL || *cp != '\0') {
		ok = FALSE;
	}
	return ok;
}

void
SetNextPartno(char* partnoStr)
{
	long number;
	char* cp;
	errno = 0;

	number = strtol(partnoStr, &cp, 10);

	partnoStr[0] = '\0';

	if (errno != ERANGE) {
		if (cp && *cp == 0 && number > 0) {
			sprintf(partnoStr, "%ld", number + 1);
		}
	}
}

/* carDlgDim is scale-relative (catalog model/car units); a prototype's dim
 * is real-world, so scale back up by the current scale ratio -- the inverse
 * of CarDlgLoadDimsFromProto's divide. */
carDim_t SeedProtoDimsFromCatalog( carDim_t catalogDim, wIndex_t scaleInx )
{
	DIST_T ratio = GetScaleRatio( scaleInx );
	carDim_t protoDim = catalogDim;

	protoDim.carLength *= ratio;
	protoDim.carWidth *= ratio;
	protoDim.truckCenter *= ratio;
	protoDim.truckCenterOffset *= ratio;
	protoDim.coupledLength *= ratio;
	return protoDim;
}

/* careditlogic.c
 *
 * Returns a bit field: one bit per dimension field, set if that field is involved in
 * ANY violated condition — whether field-local (value <= 0)
 * or relational (relationship to another field). Relational violations
 * set the bits for BOTH fields involved, as neither is ‘to blame’ on its own —
 * the user sees the conflicting pair and decides which one to adjust.
 * result == 0 means: all measurements are valid. */


unsigned
CarValidateDims( const carDim_t *dims )
{
	unsigned result = 0;

	/* field-local */
	if ( dims->carLength   <= 0.0 ) { result |= CAR_DIMS_LENGTH_BAD; }
	if ( dims->carWidth    <= 0.0 ) { result |= CAR_DIMS_WIDTH_BAD; }
	if ( dims->truckCenter <= 0.0 ) { result |= CAR_DIMS_TRKCENTER_BAD; }
	if ( dims->truckCenterOffset < 0.0 ) { result |= CAR_DIMS_TRKOFFSET_BAD; }
	if ( dims->coupledLength <= 0.0 ) { result |= CAR_DIMS_CPLDLEN_BAD; }

	/* relational: length > width */
	if ( dims->carLength <= dims->carWidth ) {
		result |= CAR_DIMS_LENGTH_BAD | CAR_DIMS_WIDTH_BAD;
	}
	/* truck center < length */
	if ( dims->truckCenter >= dims->carLength ) {
		result |= CAR_DIMS_TRKCENTER_BAD | CAR_DIMS_LENGTH_BAD;
	}
	/* coupled length > lngth */
	if ( dims->coupledLength <= dims->carLength ) {
		result |= CAR_DIMS_CPLDLEN_BAD | CAR_DIMS_LENGTH_BAD;
	}

	return result;
}

unsigned CarValidateIdentity(
        carDlgLevel_e level, resState_e resCatalog, resState_e resProto,
        const char *manufStr, const char *protoStr, const char *partnoStr )
{
	unsigned result = 0;

	if ( (level == LVL_PROTO || level == LVL_MODEL) && *protoStr == '\0' ) {
		result |= CAR_IDENT_PROTO_EMPTY;
	}

	if ( (level == LVL_CAR || level == LVL_MODEL) && *manufStr == '\0' ) {
		result |= CAR_IDENT_MANUF_EMPTY;
	}

	if ( (level == LVL_CAR || level == LVL_MODEL) && *partnoStr == '\0' ) {
		result |= CAR_IDENT_PARTNO_EMPTY;
	}

	if ( level == LVL_CAR && resCatalog == RS_UNRESOLVED ) {
		result |= CAR_IDENT_CATALOG_UNRESOLVED;
	}

	if ( level == LVL_CAR && resProto == RS_UNRESOLVED ) {
		result |= CAR_IDENT_PROTO_UNRESOLVED;
	}

	return result;
}

/**
 * One line of a car's notes for the Car Inventory list. The list draws a
 * multi-line value as a tall row, so notes with trailing blank lines left
 * large gaps (Development Issues #76). Leading blank lines are skipped,
 * tabs become spaces (a tab would start a new column), trailing spaces are
 * dropped, and " ..." marks further text when it fits.
 *
 * \param notes  the car's notes, may be NULL
 * \param out    receives the text, always terminated
 * \param outLen size of out, at least 1
 */
void CarNotesListText( const char *notes, char *out, size_t outLen )
{
	static const char more[] = " ...";
	size_t len = 0;

	out[0] = '\0';
	if ( notes == NULL ) {
		return;
	}
	while ( *notes && isspace( (unsigned char)*notes ) ) {
		notes++;
	}
	while ( *notes && *notes != '\n' && *notes != '\r' && len + 1 < outLen ) {
		out[len++] = ( *notes == '\t' ) ? ' ' : *notes;
		notes++;
	}
	while ( len > 0 && out[len - 1] == ' ' ) {
		len--;
	}
	out[len] = '\0';
	while ( *notes && *notes != '\n' && *notes != '\r' ) {
		notes++;
	}
	while ( *notes && isspace( (unsigned char)*notes ) ) {
		notes++;
	}
	if ( *notes && len + sizeof more <= outLen ) {
		memcpy( out + len, more, sizeof more );
	}
}
