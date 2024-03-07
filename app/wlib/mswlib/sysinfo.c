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

#include <process.h>
#include <stdio.h>
#include <stdlib.h>

#include <Windows.h>
#include <fileapi.h>
#include <shlobj.h>
#include <Shlwapi.h>

#include <wlib.h>
#include "mswint.h"

#ifdef WINDOWS
#define itoa(a,b,c) _itoa(a,b,c)
#define getpid() _getpid()
#endif

static char buffer[MAX_PATH + 1];

/**
 * Return the path to a temporary directory. The directory is not created.
 * The result is put into a buffer and is only valid immediately after the call.
 * 
 * \return pointer to fully qualified directory path
 */

char *
wGetTempPath()
{
	unsigned retChars;

	retChars = GetTempPath(MAX_PATH + 1, buffer);

	if (retChars <= MAX_PATH + 1) {
		char str[20];
		strcat(buffer, "xtc");
		itoa(getpid(), str, 10);
		strcat(buffer, str);
	}

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
	pPipe = _popen("ver", "r");

	while (fgets(buffer, sizeof(buffer), pPipe))
		;

	if (buffer[strlen(buffer) -1]  == '\n')
		buffer[strlen(buffer) -1 ] = '\0';
	_pclose(pPipe);

	return(buffer);
}

/**
 * Get the name of the configuration file.
 *
 * \return pointer to the filename.
 *
 */

char *
wGetProfileFilename()
{
	return(mswProfileFile);
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
	DWORD bufferSize = sizeof(buffer);

	GetUserName(buffer, &bufferSize);

	return(buffer);
}

/** Get the user's profile directory. Other than on UNIX Windows differentiates
 * between the home directory and and the profile directory. 
 *
 * \return    pointer to the user's profile directory
 */

const char* wGetUserHomeRootDir(void)
{
	if (SHGetSpecialFolderPath(NULL, mswTmpBuff, CSIDL_PROFILE, 0) == 0) {
		wNoticeEx(NT_ERROR, "Cannot get user's profile directory", "Exit", NULL);
		wExit(0);
		return(NULL);
	}
	else {
		return(mswTmpBuff);
	}
}
