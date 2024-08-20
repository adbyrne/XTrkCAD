/** \file wpref.c
 * Handle loading and saving preferences.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE


#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "wlib.h"
#include "gtkint.h"
#include "dynarr.h"
#include "i18n.h"

#include "xtrkcad-config.h"

extern char wConfigName[];


/*
 *******************************************************************************
 *
 * Preferences
 *
 *******************************************************************************
 */

static bool prefInitted = false;
static GKeyFile *prefs;

/**
 * Read the preferences from an ini file iinto memory
 *
 * \param name name of file, if NULL or empty string, the default file is used
 * \param update force update (ignored)
 * \return
 */

static void readPrefs( char * name, wBool_t update )
{
	gchar *tmp;
	const char * workDir;
	GError *error = NULL;

	prefInitted = TRUE;
	workDir = wGetAppWorkDir();

	if (name && name[0]) {
		tmp = g_strdup( name );
	} else {
		tmp = g_strdup_printf("%s/%s.ini", workDir, wConfigName );
	}

	prefs = g_key_file_new();

	g_key_file_load_from_file(prefs,
	                          tmp,
	                          G_KEY_FILE_KEEP_COMMENTS,
	                          &error);
	if(error) {
		// ignore file does not exist condition, it will be created later
		if(error->code != G_FILE_ERROR_NOENT) {
			wNoticeWithIcon( NT_ERROR, error->message, _("Exit"), NULL);
		}
	}
	g_free( tmp );
}

/**
 * Store a string in the user preferences.
 *
 * \param section IN section in preferences file
 * \param name IN name of parameter
 * \param sval IN value to save
 */

void wPrefSetString(
        const char * section,		/* Section */
        const char * name,		/* Name */
        const char * sval )		/* Value */
{
	if (!prefInitted) {
		readPrefs("", FALSE);
	}

	g_key_file_set_string(prefs,
	                      section,
	                      name,
	                      sval);
}

/**
 * Get a string from the user preferences.
 *
 * \param section IN section in preferences file
 * \param name IN name of parameter
 */

char * wPrefGetStringBasic(
        const char * section,			/* Section */
        const char * name )			/* Name */
{

	if (!prefInitted) {
		readPrefs("", FALSE);
	}

	return g_key_file_get_string (prefs,
	                              section,
	                              name,
	                              NULL);
}

/**
 * Store an integer value in the user preferences.
 *
 * \param section IN section in preferences file
 * \param name IN name of parameter
 * \param lval IN value to save
 */

void wPrefSetInteger(
        const char * section,		/* Section */
        const char * name,		/* Name */
        long lval )		/* Value */
{
	if (!prefInitted) {
		readPrefs("", FALSE);
	}

	g_key_file_set_integer(prefs,
	                       section,
	                       name,
	                       lval);
}

/**
 * Read an integer value from the user preferences.
 *
 * \param section IN section in preferences file
 * \param name IN name of parameter
 * \param res OUT resulting value
 * \param default IN default value
 * \return TRUE if value was found, FALSE if default is returned
 */

wBool_t wPrefGetIntegerBasic(
        const char * section,		/* Section */
        const char * name,		/* Name */
        long * res,		/* Address of result */
        long def )		/* Default value */
{
	GError *error = NULL;
	int result;

	if (!prefInitted) {
		readPrefs("", FALSE);
	}

	result = g_key_file_get_integer (prefs,
	                                 section,
	                                 name,
	                                 &error);

	if(error) {
		*res = def;
		return FALSE;
	} else {
		*res= result;
		return TRUE;
	}
}

/**
 * Save a float value in the preferences file.
 *
 * \param section IN the file section into which the value should be saved
 * \param name IN the name of the preference
 * \param lval IN the value
 */

void wPrefSetFloat(
        const char * section,		/* Section */
        const char * name,		/* Name */
        double lval )		/* Value */
{
	if (!prefInitted) {
		readPrefs("", FALSE);
	}

	g_key_file_set_double(prefs,
	                      section,
	                      name,
	                      lval);
}

/**
 * Read a float from the preferencesd file.
 *
 * \param section IN the file section from which the value should be read
 * \param name IN the name of the preference
 * \param res OUT pointer for the value
 * \param def IN	default value
 * \return TRUE if value was read, FALSE if default value is used
 */


wBool_t wPrefGetFloatBasic(
        const char * section,		/* Section */
        const char * name,		/* Name */
        double * res,		/* Address of result */
        double def )		/* Default value */
{
	GError *error = NULL;
	double result;

	if (!prefInitted) {
		readPrefs("", FALSE);
	}

	result = g_key_file_get_double(prefs,
	                               section,
	                               name,
	                               &error);

	if(error) {
		*res = def;
		return FALSE;
	} else {
		*res= result;
		return TRUE;
	}
}

void wPrefsLoad(char * name)
{
	readPrefs(name,TRUE);
}

/**
 * Save the preferences to a key-value file (ini file)
 *
 * \param name if NULL use default filename, otherwise points to the name and
 * path for the ini file
 */
void
wPrefFlush(	char *name )
{
	GError *error = NULL;
	const char *workDir;
	char *tmp;

	if (!prefInitted) {
		return;
	}

	workDir = wGetAppWorkDir();
	if (name && name[0]) {
		tmp = g_strdup(name);
	} else {
		tmp = g_strdup_printf("%s/%s.ini", workDir, wConfigName );
	}

	g_key_file_save_to_file(prefs,
	                        (const char *)tmp,
	                        &error);

	g_free(tmp);
}

/**
 * Clear the preferences from memory
 */

void
wPrefReset(void )
{
	prefInitted = FALSE;
	g_key_file_free (prefs);
}
