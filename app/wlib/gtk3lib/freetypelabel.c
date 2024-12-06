/**
 * \file   freetypelabel.c
 * \brief  Create icons of simple labels using freetype and cairo
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

#include <direct.h>

#include <wlib.h>
#include "gtkint.h"

#include <cairo.h>
#include <cairo-ft.h>

#include <ft2build.h>
#include FT_FREETYPE_H

static FT_Library library;
static char* currentFontFile;
static FT_Face ftFontFace;
static double fontSize;

#define FONTNOTCREATED(font) (!currentFontFile || strcmp(font, currentFontFile))

#define RED_COMPONENT_FROM_RGB(color) (((color & 0xFF0000) >> 16) / 255.0)
#define GREEN_COMPONENT_FROM_RGB(color) (((color & 0xFF00) >> 8) / 255.0)
#define BLUE_COMPONENT_FROM_RGB(color) ((color & 0xFF) / 255.0)

#define ICON_DEFAULTSIZE 50

static void
InitializeFreeType(void)
{
	int error;

	error = FT_Init_FreeType(&library);

	if (error) {
		printf("Freetype Library could not be initialized! [return code=%X]\n", error);
		g_abort();
	}
}

static double
GetFaceHeight(FT_Face face)
{
	double size = 0.0;

	if (face->num_fixed_sizes) {
		size = face->available_sizes[0].height;
	}

	return(size);
}


static char*
RenderToPixbuf(cairo_surface_t* surface, const char* text, wDrawColor color)
{
	cairo_t* cr = cairo_create(surface);
	cairo_text_extents_t extents;
	cairo_font_face_t* cairoFontFace = cairo_ft_font_face_create_for_ft_face(
	                ftFontFace, 0);

	cairo_set_font_face(cr, cairoFontFace);
	cairo_set_font_size(cr, fontSize);
	cairo_set_source_rgb(cr,
	                     RED_COMPONENT_FROM_RGB(color),
	                     GREEN_COMPONENT_FROM_RGB(color),
	                     BLUE_COMPONENT_FROM_RGB(color));

	cairo_text_extents(cr, text, &extents);
	cairo_move_to(cr, 0.0, extents.height);
	cairo_show_text(cr, text);

	cairo_destroy(cr);
	cairo_font_face_destroy(cairoFontFace);

	return((char*)gdk_pixbuf_get_from_surface(surface, 0, 0, (int)extents.width,
	        (int)extents.height));
}

static char*
CreatePixbufFromFTLabel(const char* text, wDrawColor color)
{
	char* bits;

	cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
	                           ICON_DEFAULTSIZE,
	                           ICON_DEFAULTSIZE);

	bits = RenderToPixbuf(surface, text, color);

	cairo_surface_destroy(surface);

	return(bits);
}

/**
 * Load a font from file using freetype. This allows to use fonts that are not installed by the
 * operating system.
 *
 * \param filename path to font
 * \return font size or 0.0 on error
 *
 */

double
wFTLabelLoadFontFromFile(const char* filename)
{
	if (!library) {
		InitializeFreeType();
	}

	if (FONTNOTCREATED(filename)) {
		int error;

		const char* directory = getcwd(NULL, 0);

		error = FT_New_Face(library,
		                    filename,
		                    0,
		                    &ftFontFace);

		if (error == FT_Err_Unknown_File_Format) {
			printf("The font file could be opened and read, but it appears that its font format is unsupported!\n");
			g_abort();
		} else if (error) {
			printf("Font file could not be opened or read! [return code=%X]\n", error);
			g_abort();
		}

		fontSize = GetFaceHeight(ftFontFace);
	}
	return(fontSize);
}

/**
 * Create a label using a previously loaded font.
 *
 * \param text	text of label
 * \param color	color of label
 * \return NULL on error, label otherwise
 */

wIcon_p
wFTLabelCreate(const char* text, wDrawColor color)
{
	struct wIcon_t* icon = (wIcon_p)g_malloc0(sizeof(struct wIcon_t));

	if (icon) {
		icon->bits = CreatePixbufFromFTLabel(text, color);
		icon->color = color;
		icon->text = text;
		icon->gtkIconType = ICON_PIXBUF;
	}

	return(icon);
}

/**
 * Change the color of a label used by a button. The newly colored label is shown in the
 * button
 *
 * \param button	the button which has the label
 * \param newColor	new color for the label
 */

void
wFTLabelChangeColor(wControl_p button, wDrawColor newColor)
{
	const struct button* buttonAttributes = CONTROL_GET_ATTRIBUTES_PTR(button,
	                                        button);
	wIcon_p icon = buttonAttributes->icon;

	if (newColor != icon->color) {
		icon->bits = CreatePixbufFromFTLabel(icon->text, newColor);
		icon->color = newColor;
		wButtonSetIcon(button, icon);
	}
}


