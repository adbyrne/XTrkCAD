/** \file opendocument.c
 * open a document using the file association
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2018 Martin Fischer
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

#include "gtkint.h"
#include "i18n.h"



#if defined (_WIN32)

#include <windows.h>
#include <shellapi.h>

#else

#include "dynstring.h"

#if defined(__APPLE__) && defined(__MACH__)
#define DEFAULTOPENCOMMAND "open"
#else
#define DEFAULTOPENCOMMAND "xdg-open"
#endif

#endif


/**
 * Invoke the system's default application to open a file.
 *
 * \param topic IN URI of document
 *
 * \return 0 on success, error code on failure
 */

/**  \todo Test UNIX case, change to glib string functions */

unsigned wOpenFileExternal(char * filename)
{
	g_assert(filename != NULL);
	g_assert(strlen(filename));

#ifdef _WIN32
	HINSTANCE handle;
	unsigned int result;

	handle = ShellExecute(NULL, "open", filename, NULL, NULL, SW_SHOWNORMAL);

	if (handle > (HINSTANCE)
	    32) { /** magic Windows number, > 32 is a handle in case of success */
		result = 0;
	} else {
		result = (unsigned)(long long)handle;
	}

#else
	unsigned int result = 0;
	GError *error = NULL;

	gtk_show_uri_on_window(NULL, filename, GDK_CURRENT_TIME, &error);
	if(error) {
		result = error->code;
	}

#endif
	return result;
}
