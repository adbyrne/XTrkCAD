/** \file prerelease.c
 * Names of the data directory variable and the work directory, which keep a
 * pre-release apart from the release (Development Issues #94)
 *
 * Pure GLib, so unittests/prereleasetest.c can compile it in.
 */

/*  XTrkCad - Model Railroad CAD
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

#include <glib.h>

gchar *wlibLibDirEnvVar(const char *appName);
const char *wlibWorkDirName(const char *appName, const char *prereleaseName);

/**
 * The environment variable that can name the data directory: the name the
 * program is installed under, upper case, then LIB. A shell can only set a
 * variable whose name is letters, digits and '_', so any other character
 * becomes '_'. "xtrkcad" gives XTRKCADLIB, "xtrkcad-5.4.0" XTRKCAD_5_4_0LIB.
 * A pre-release needs nothing more: it's installed under its version
 * (xtrkcad-5.4.0Beta1), so its name is its own.
 *
 * \param appName IN the name the program is installed under
 * \return the name, to be freed with g_free()
 */
gchar *
wlibLibDirEnvVar(const char *appName)
{
	gchar *name = g_strconcat(appName, "LIB", NULL);

	for (gchar *cp = name; *cp; cp++) {
		*cp = g_ascii_isalnum(*cp) ? g_ascii_toupper(*cp) : '_';
	}
	return name;
}

/**
 * The name of the work directory, which holds the settings: the name the
 * program is installed under, but for a pre-release the one name every
 * pre-release shares (XTRKCAD_PRERELEASE_NAME), so its settings carry over
 * from one pre-release to the next and never touch the release's.
 *
 * \param appName IN the name the program is installed under
 * \param prereleaseName IN the shared pre-release name, or NULL or "" for a
 *	  build that isn't a pre-release
 * \return one of the two arguments
 */
const char *
wlibWorkDirName(const char *appName, const char *prereleaseName)
{
	if (prereleaseName != NULL && *prereleaseName != '\0') {
		return prereleaseName;
	}
	return appName;
}
