/** \file writebitmap.c
 * Bitmap file creation
 */

/*  XTrackCad - Model Railroad CAD
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <string.h>
#include <gtk/gtk.h>
#include "gtkint.h"

#define PNGFORMAT "png"
#define JPEGFORMAT "jpeg"

/**
 * Get the Extension part of a filename
 *
 * /param fname the filename
 *
 * /return char* point to the extension
 */

static const char *
GetExtension(const char *fname)
{
	const char *end = fname + strlen(fname);

	while (end > fname && *end != '.') {
		--end;
	}
	return( end + 1 );
}

/**
* Export as bitmap file.
*
* \param d IN the drawing area ?
* \param fileName IN  fully qualified filename for the bitmap file.
* \return    TRUE on success, FALSE on error
*/

wBool_t wBitMapWriteFile(wDraw_p d, const char * fileName)
{
	GdkPixbuf *pixbuf;
	GError *error;
	gboolean res;
	const char *fileFormat = GetExtension(fileName);
	char *writeFormat = NULL;

	if(!strcasecmp(fileFormat, PNGFORMAT )) {
		writeFormat = PNGFORMAT;
	}
	if( !strcasecmp(fileFormat, "jpg") ||
	    !strcasecmp(fileFormat, "jpeg")) {
		writeFormat = JPEGFORMAT;
	}

	if(!writeFormat) {
		wNoticeEx(NT_ERROR, "WriteBitMap: invalid file format!", "Ok", NULL);
		return FALSE;
	}

	pixbuf = gdk_pixbuf_get_from_drawable(NULL, (GdkWindow*)d->pixmap, NULL, 0, 0,
	                                      0, 0, d->w, d->h);

	if (!pixbuf) {
		wNoticeEx(NT_ERROR, "WriteBitMap: pixbuf_get failed", "Ok", NULL);
		return FALSE;
	}

	error = NULL;
	res = gdk_pixbuf_save(pixbuf, fileName, writeFormat, &error, NULL);

	if (res == FALSE) {
		wNoticeEx(NT_ERROR, "WriteBitMap: pixbuf_save failed", "Ok", NULL);
		return FALSE;
	}

	g_object_ref_sink(pixbuf);
	g_object_unref(pixbuf);
	return TRUE;
}
