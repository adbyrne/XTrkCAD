/**
 * \file   locale.c
 * \brief  Locale handling
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

#include "common.h"
#include <locale.h>

/**
 * SetCLocale is called before reading/writing any data files (.xtc, .xti, .xtq, .cus...).
 * SetUserLocale is called after.
 * Calls can be nested: C, C, User, User
 */

static char* sUserLocale = NULL;	// current user locale
static long lCLocale = 0;		// locale state: > 0 C locale, <= 0 user locale
static long nCLocale = 0;		// total # of setlocals calls
static int log_locale = 0;		// logging


EXPORT void SetCLocale()
{
	if (sUserLocale == NULL) {
		sUserLocale = MyStrdup(setlocale(LC_ALL, NULL));
	}
	if (lCLocale == 0) {
		LOG(log_locale, 1, ("Set C Locale: %ld\n", ++nCLocale));
		setlocale(LC_ALL, "C");
#ifdef LC_MESSAGES
		setlocale(LC_MESSAGES, "");
#endif
	}
	lCLocale++;
	if (lCLocale > 1) {
		LOG(log_locale, 3, ("SetClocale - C! %ld\n", nCLocale));
	} else if (lCLocale < 1) {
		LOG(log_locale, 2, ("SetClocale - User! %ld\n", nCLocale));
	}
}

EXPORT void SetUserLocale()
{
	if (lCLocale == 1) {
		LOG(log_locale, 1, ("Set %s Locale: %ld\n", sUserLocale, ++nCLocale));
		setlocale(LC_ALL, sUserLocale);
	}
	lCLocale--;
	if (lCLocale < 0) {
		LOG(log_locale, 2, ("SetUserLocale - User! %ld\n", nCLocale));
	} else if (lCLocale > 0) {
		LOG(log_locale, 3, ("SetUserLocale - C! %ld\n", nCLocale));
	}
}

void
LocaleInit()
{
	log_locale = LogFindIndex("locale");
}

