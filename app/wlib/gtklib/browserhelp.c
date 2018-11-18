/** \file browserhelp.c
 * use the default browser based help
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2015 Martin Fischer
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

#define DEFAULTBROWSERCOMMAND "xdg-open"

#define  HELPERRORTEXT 			"Help Error - help information can not be found.\n" \
								"The help information you requested cannot be shown.\n" \
								"Usually this is an installation problem, Make sure that a default browser" \
                                " is configured and that XTrackCAD and the included " \
								"HTML files are installed properly and can be found via the XTRKCADLIB environment " \
								"variable.\n Also make sure that the user has sufficient access rights to read these" \
 								"files."
/**
 * Create a fully qualified url from a topic
 *
 * \param helpUrl OUT pointer to url, free by caller
 * \param topic IN the help topic
 */

static void
TopicToUrl(char **helpUrl, const char *topic)
{
    DynString url;
    DynStringMalloc(&url, 16);

    // build up the url line
    DynStringCatCStrs(&url,
                      "file://",
                      wGetAppLibDir(),
                      "/html/",
                      topic,
                      ".html",
                      NULL);

    *helpUrl = strdup(DynStringToCStr(&url));
    DynStringFree(&url);
}
/**
 * Invoke the system's default browser to display help for <topic>. First the
 * system's standard xdg-open command is attempted. If that is not available, the
 * version included with the XTrackCAD installation is executed.
 *
 * \param topic IN topic string
 */

void wHelp(const char * topic)
{
    int rc;
    char *url;
    char *currentPath;

    assert(topic != NULL);
    assert(strlen(topic));

    TopicToUrl(&url, topic);

	rc = wOpenFileExternal(url);

    if (!rc) {
        wNotice(HELPERRORTEXT, _("Cancel"), NULL);
    }

    free(url);
}
