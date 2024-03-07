/** \file sysinfo.c
 * Collect info about runtime environment
*/

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2024 Martin Fischer
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

#include <stdio.h>
#include <stdlib.h>

#include <wlib.h>
#include "gtkint.h"


static char *buffer;

/**
 * Return the path to a temporary directory. The directory is not created.
 * The result is put into a buffer and is only valid immediately after the call.
 * 
 * \return pointer to fully qualified directory path
 */

char *
wGetTempPath()
{
	gchar const *tempDir = g_get_tmp_dir();
	gchar *path;
	gchar pidString[20];

	g_snprintf(pidString, 20, "xtc%d", getpid());
	path = g_build_path("/", tempDir, pidString, (char *)0);

	if(buffer) {
		g_free(buffer);
	}

	buffer = g_strdup(path);
	g_free(path);

	return(buffer);
}

/**
 * Get the Windows version. This function uses the Windows ver command to 
 * retrieve the OS version. The result is put into a buffer and is only
 * valid immediately after the call.
 * 
 * \return buffer containing the zero terminated string
 * 
 */

char *
wGetOSVersion()
{
	FILE* pPipe;
	size_t bufferSize = 80;

	if(buffer) {
		free(buffer);
		buffer = NULL;
	}

	buffer = malloc(bufferSize);
//	pPipe = _popen("cat /etc/*-release | grep "PRETTY_NAME" | sed 's/PRETTY_NAME=//g'", "r");
	pPipe = popen("uname -sr", "r");

	while (fgets(buffer, bufferSize, pPipe))
		;

	if (buffer[strlen(buffer) -1]  == '\n')
		buffer[strlen(buffer) -1 ] = '\0';
	pclose(pPipe);

	return(buffer);
}



/**
 * Get the name of the current user. The result is put into a buffer and is only
 * valid immediately after the call.
 *
 * \return buffer containing the zero terminated string
 *
 */

char *
wGetUserID()
{
	const gchar *name;

	name = g_get_user_name();

	if(buffer) {
		g_free(buffer);
	}
	buffer = g_strdup( name );
	return(buffer);
}

/** Get the user's profile directory. Other than on UNIX Windows differentiates
 * between the home directory and and the profile directory. 
 *
 * \return    pointer to the user's profile directory
 */

const char* wGetUserHomeRootDir(void)
{
	return(wGetUserHomeDir());
}

const char* wGetPlatformVersion()
{
	if(buffer) {
		g_free(buffer);
	}

	buffer = g_strdup_printf("%d.%d.%d", gtk_major_version, gtk_minor_version, gtk_micro_version );
	return(buffer);
}

