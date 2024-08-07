/**
 * \file   drawbitmap.c
 * \brief  Possibly obsolete functions to create and save bitmaps
 *
 * \author Martin Fischer
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

#include <wlib.h>

  /*******************************************************************************
   *
   * BitMaps
   *
  *******************************************************************************/

  /**
  * Export as bitmap file.
  *
  * \param d IN the drawing area ?
  * \param fileName IN  fully qualified filename for the bitmap file.
  * \return    TRUE on success, FALSE on error
  */

wBool_t wBitMapWriteFile(wDraw_p d, const char* fileName)
{
	cairo_status_t status;

	status = cairo_surface_write_to_png(d->surface, fileName);

	if (status != CAIRO_STATUS_SUCCESS) {
		wNoticeEx(NT_ERROR, "WriteBitMap: surface_write_to_png failed", "Ok", NULL);
		return FALSE;
	}
	return TRUE;
}

wDraw_p wBitMapCreate(wWinPix_t w, wWinPix_t h, int arg)
{
	wDraw_p bd;

	bd = (wDraw_p)wlibAlloc(gtkMainW, B_DRAW, 0, 0, NULL, sizeof * bd, NULL);

	bd->lastColor = -1;

	double dpi;

	wPrefGetFloat(PREFSECTION, DPISET, &dpi, 96.0);

	bd->dpi = dpi;
	bd->maxW = bd->w = w;
	bd->maxH = bd->h = h;
	bd->clip_set = FALSE;
	bd->scale_adjust = 1.0;
	bd->scale_text = 1.0;

	if (arg & EXPORTBITMAP) {
		bd->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
		if (bd->surface == NULL) {
			wNoticeEx(NT_ERROR, "image_ surface_create failed", "Ok", NULL);
			return NULL;
		}
		bd->cr = cairo_create(bd->surface);
		if (bd->cr == NULL) {
			wNoticeEx(NT_ERROR, "image_ surface_create failed", "Ok", NULL);
			return NULL;
		}
		bd->drawDestination = EXPORTBITMAP;

		// correct origin of coordinates top left -> bottom left
		cairo_translate(bd->cr, 0, h);
		cairo_scale(bd->cr, 1.0, -1.0);

		wlibBasicClear(bd);
	}
	else {
		bd->pixbuf = gdk_pixbuf_get_from_window(gtk_widget_get_window(GTK_WIDGET(
			gtkMainW->gtkwin)), 0, 0, w, h);
		if (bd->pixbuf == NULL) {
			wNoticeEx(NT_ERROR, "CreateBitMap: pixmap_new failed", "Ok", NULL);
			return FALSE;
		}
		bd->drawDestination = 0;
		wDrawClear(bd);

	}
	return bd;
}


wBool_t wBitMapDelete(wDraw_p d)
{
	if (d->drawDestination == EXPORTBITMAP) {
		cairo_destroy(d->cr);
		d->cr = NULL;
		cairo_surface_destroy(d->surface);
		d->surface = NULL;
	}
	else {
		g_object_unref(d->pixbuf);
		d->pixbuf = NULL;
	}
	d->clip_set = FALSE;
	return TRUE;
}

