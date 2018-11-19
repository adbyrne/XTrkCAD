/** \file opendocument.c
 * open a document using the systems default application for that doc
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

#include <stdlib.h>
#include <assert.h>

#include "gtkint.h"
#include "i18n.h"

#include "dynstring.h"

#if defined (_WIN32)

#define DEFAULTOPENCOMMAND "start"

#endif

#if defined(__APPLE__) && defined(__MACH__)

#define DEFAULTOPENCOMMAND "open"

#else

#define DEFAULTOPENCOMMAND "xdg-open"

#endif

/**
 * Extend the PATH variable in the environment to include XTrackCAD's
 * script directory.
 *
 * \return pointer to old path
 */

static char *
ExtendPath(void)
{
    char *path = getenv("PATH");
    DynString newPath;
    DynStringMalloc(&newPath, 16);

    // append XTrackCAD's directory to the path as a fallback
    DynStringCatCStrs(&newPath,
                      path,
                      ":",
                      wGetAppLibDir(),
                      NULL);

    setenv("PATH",
           DynStringToCStr(&newPath),
           TRUE);

    DynStringFree(&newPath);

    return (path);
}

/**
 * Invoke the system's default application to open a file. First the
 * system's standard xdg-open command is attempted. If that is not available, the
 * version included with the XTrackCAD installation is executed.
 *
 * \param topic IN URI of document
 */

unsigned wOpenFileExternal(char * filename)
{
    int rc;
    DynString commandLine;
    char *currentPath;

    assert(filename != NULL);
    assert(strlen(filename));

    currentPath = ExtendPath();
    
    DynStringMalloc(&commandLine, 16);
    DynStringCatCStrs(&commandLine,
                      DEFAULTOPENCOMMAND,
                      " ",
                      filename,
                      NULL);

    // the command should be found via the PATH
    rc = system(DynStringToCStr(&commandLine));

    // restore the PATH
    setenv("PATH",
           currentPath,
           TRUE);

    DynStringFree(&commandLine);
    
    return(rc==0);
}
