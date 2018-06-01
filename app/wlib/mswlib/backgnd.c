/** \file backgnd.c
* Layout background image
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

#include <windows.h>

#include <FreeImage.h>

#include "mswint.h"

int 
wDrawSetBackground(wDraw_p bd, char * path, char * error)
{
	FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;

	if (path) {
		// check the file signature and deduce its format
		// (the second argument is currently not used by FreeImage)
		fif = FreeImage_GetFileType(path, 0);
		if (fif == FIF_UNKNOWN) {
			// no signature ?
			// try to guess the file format from the file extension
			fif = FreeImage_GetFIFFromFilename(path);
		}
		// check that the plugin has reading capabilities ...
		if ((fif != FIF_UNKNOWN) && FreeImage_FIFSupportsReading(fif)) {
			// ok, let's load the file
			bd->background = FreeImage_Load(fif, path, 0);
			// unless a bad file format, we are done !
			if (!bd->background) {
				return(-1);
			}
			else {
				return(0);
			}
		}
		else {
			return(-1);
		}
	}
	else {
		if (bd->background) {
			FreeImage_Unload(bd->background);
			bd->background = 0;
		}
		return(1);
	}
}


void 
wDrawShowBackground(wDraw_p bd, wPos_t pos_x, wPos_t pos_y, wPos_t size, wAngle_t angle, int screen) 
{
	
	if (bd->background) {
		double scale;
		FIBITMAP *tmp;

		if (size == 0) {
			scale = 1.0;
		}
		else {
			scale = (double)size / FreeImage_GetWidth(bd->background);
		}

		tmp = FreeImage_RescaleRect(bd->background,
				(int)((double)FreeImage_GetWidth(bd->background) * scale ),
				(int)((double)FreeImage_GetHeight(bd->background) * scale ),
				0,
				0,
				FreeImage_GetWidth(bd->background),
				FreeImage_GetHeight(bd->background),
				FILTER_BILINEAR,
				0);

		SetStretchBltMode(bd->hDc, COLORONCOLOR);
		StretchDIBits(bd->hDc, 
					  pos_x, 
					  pos_y,
					  FreeImage_GetWidth(tmp),
					  FreeImage_GetHeight(tmp),
						0, 0, 
					  FreeImage_GetWidth(tmp), 
					  FreeImage_GetHeight(tmp),
				      FreeImage_GetBits(tmp),
					  FreeImage_GetInfo(tmp), 
					  DIB_RGB_COLORS, SRCCOPY);

		FreeImage_Unload(tmp);
	}
/**
		cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, wDrawColorWhite, 0);
		cairo_save(cairo);
		int pixels_width = gdk_pixbuf_get_width(bd->background);
		int pixels_height = gdk_pixbuf_get_height(bd->background);
		double scale;
		double posx, posy, width, sized;
		posx = (double)pos_x;
		posy = (double)pos_y;
		if (size == 0) {
			scale = 1.0;
		}
		else {
			sized = (double)size;
			width = (double)pixels_width;
			scale = sized / width;
		}
		cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
		double rad = M_PI*(angle / 180);
		posy = (double)bd->h - ((pixels_height*fabs(cos(rad)) + pixels_width*fabs(sin(rad)))*scale) - posy;
		//width = (double)(pixels_width*scale);
		//height = (double)(pixels_height*scale);
		cairo_translate(cairo, posx, posy);
		cairo_scale(cairo, scale, scale);
		cairo_translate(cairo, fabs(pixels_width / 2.0*cos(rad)) + fabs(pixels_height / 2.0*sin(rad)),
			fabs(pixels_width / 2.0*sin(rad)) + fabs(pixels_height / 2.0*cos(rad)));
		cairo_rotate(cairo, M_PI*(angle / 180.0));
		// We need to clip around the image, or cairo will paint garbage data
		cairo_rectangle(cairo, -pixels_width / 2.0, -pixels_height / 2.0, pixels_width, pixels_height);
		cairo_clip(cairo);
		gdk_cairo_set_source_pixbuf(cairo, bd->background, -pixels_width / 2.0, -pixels_height / 2.0);
		cairo_pattern_t *mask = cairo_pattern_create_rgba(1.0, 1.0, 1.0, (100.0 - screen) / 100.0);
		cairo_mask(cairo, mask);
		cairo_restore(cairo);
		gtkDrawDestroyCairoContext(cairo);
**/


}