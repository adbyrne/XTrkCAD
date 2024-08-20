/** \file paths.c
 * Find relevant directories Win32 implementation
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis, 2023 Martin Fischer
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


#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <glib.h>
#include <glib/gstdio.h>

#ifdef WIN32
#include <Shlobj.h>
#include <KnownFolders.h>
#include <Shlwapi.h>
#endif // WIN32


#include "wlib.h"
#include "../gtkint.h"
#include "i18n.h"

static char *appLibDir;
static char *appWorkDir;
static char *userHomeDir;

/*
 *******************************************************************************
 *
 * Get Dir Names
 *
 *******************************************************************************
 */

/**
 * Find the directory where configuration files, help, demos etc are installed.
 *
 * Windows: the directory is expected to be at ..\share\xtrkcad
 * 
 * Unix: The search order is:
 *  1. Directory specified by the XTRKCADLIB environment variable
 *  2. relative path ../xtrkcad from the installation directory
 *  3. /usr/share/xtrkcad
 *  4. /usr/local/share/xtrkcad
 *  if no directory can be found, the program terminates
 *
 *  \return pointer to directory name
 */

const char* 
wGetAppLibDir(void)
{
	gchar* moduleDir;
	GString* libDir;

	if (appLibDir != NULL) {
		return appLibDir;
	}

	moduleDir = g_win32_get_package_installation_directory_of_module(NULL);
	libDir = g_string_new(moduleDir);
	g_string_append(libDir, "\\..\\share\\xtrkcad");
	appLibDir = g_canonicalize_filename(libDir->str, NULL);
	g_free(moduleDir);
	g_string_free(libDir, TRUE);

	return(appLibDir);
}

/**
 * Gets the working directory for the application. At least the INI file is stored here.
 * The working directory can be specified manually by creating a file called xtrkcad0.ini
 * in the application lib dir (the directory where the .EXE is located).
 *
 * [workdir]
 *		path=somepath
 *
 * when somepath is set to the keyword "installdir", the install directory for the EXE is
 * used.
 *
 * If no xtrkcad0.ini could be found, the user settings directory (appdata) is used.
 *
 */
const char* wGetAppWorkDir(void)
{
	char* cp;
	int rc;
	gchar* tempIniFile = NULL;
	gchar* tempDir = NULL;

	if (appWorkDir) {
		return appWorkDir;
	}
	tempDir = g_malloc(MAX_PATH);
	tempIniFile = g_strdup_printf( "%s\\xtrkcad0.ini", wGetAppLibDir());

	rc = GetPrivateProfileString("workdir", "path", "", tempDir,
		MAX_PATH, tempIniFile);

	g_free(tempIniFile);
	tempIniFile = NULL;

	if (rc != 0) {
		if (g_ascii_strncasecmp(tempDir, "installdir", strlen(tempDir)) == 0) {
			g_strlcpy(tempDir, appLibDir, MAX_PATH);
		}
		else {
			cp = &tempDir[strlen(tempDir) - 1];
			while (cp > tempDir && *cp == '\\') { *cp-- = 0; }
		}
		appWorkDir = tempDir;
		return appWorkDir;
	}

	if(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, tempDir) != S_OK) {
		wNoticeWithIcon(NT_ERROR, "Cannot get user's profile directory", "Exit", NULL);
		wExit(0);
	}
	else {
		g_snprintf(tempDir, MAX_PATH, "%s\\%s", tempDir,	wlibGetAppName());
		appWorkDir = tempDir;

		if (!g_file_test(appWorkDir, G_FILE_TEST_IS_DIR )) {
			if (g_mkdir(appWorkDir, 0x666)) {
				wNoticeWithIcon(NT_ERROR, "Cannot create user's profile directory", "Exit", NULL);
				wExit(0);
			}
		}
	}

	return appWorkDir;
}

/**
 * Get the user's home directory. 
 *
 * \return    pointer to the user's home directory
 */

const char *wGetUserHomeDir( void )
{
	const char *homeDir;

	if( userHomeDir ) {
		return userHomeDir;
	}

	homeDir = g_get_home_dir();
 	if (homeDir == NULL) {
		wNoticeWithIcon( NT_ERROR, _("HOME is not set"), _("Exit"), NULL);
		wExit(0);
	} else {
		userHomeDir = g_strdup( homeDir );
	}

	return userHomeDir;
}
