/** \file validator.c
* Validators for misc textformats
*/

/*  XTrackCAD - Model Railroad CAD
*  Copyright (C) 2019 Martin Fischer
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

#include "validator.h"

/**
* Simplistic checking of URL syntax validity. Checks:
* if scheme is present, it has to be terminated by double /
* hostname has at least 5 characters
*
* \param testString
* \return TRUE if valid, FALSE otherwise
*/

enum URLSCANSTATE { STATE_SCHEME, STATE_ENDOFSCHEME, STATE_HIER, STATE_PATH, STATE_ERROR };

#define MINURLLENGTH 5 /* 2 chars domain name, dot, 2 chars TLD */

bool
IsValidURL(const char *testString)
{
	const char *result = testString;
	const char *hostname = testString;
	enum URLSCANSTATE state = STATE_SCHEME;

	if (!*result) {
		return(false);
	}

	while (*result && state != STATE_ERROR) {
		switch (*result) {
		case ':':
			if (state == STATE_SCHEME) {
				if (result == testString) {
					state = STATE_ERROR;
				} else {
					state = STATE_ENDOFSCHEME;
				}
			}
			break;
		case '/':
			if (state == STATE_ENDOFSCHEME) {
				if (*(result + 1) == '/') {
					state = STATE_HIER;
					hostname = result + 2;
					result++;
				} else {
					state = STATE_ERROR;
				}
			} else {
				if (state == STATE_HIER || state == STATE_SCHEME) {
					state = STATE_PATH;
					if (result - hostname < MINURLLENGTH) {
						state = STATE_ERROR;
					}
				}
			}
			break;
		default:
			break;

		}
		result++;
	}

	return (state != STATE_ERROR);
}
