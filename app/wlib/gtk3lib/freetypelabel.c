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

#include <wlib.h>
#include "gtkint.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo.h>
#include <cairo-ft.h>

#include <ft2build.h>
#include FT_FREETYPE_H

static FT_Library library;
static char* currentFontFile;
static GBytes *fontBytes = NULL;

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


static GdkPixbuf*
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

	return(gdk_pixbuf_get_from_surface(surface, 0, 0, (int)extents.width,
	                                   (int)extents.height));
}

static GdkPixbuf *
CreatePixbufFromFTLabel(const char* text, wDrawColor color)
{
	GdkPixbuf* bits;

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
 * Load a font from file using freetype. This allows to use fonts that are not installed by the
 * operating system.
 *
 * \param filename path to font
 * \return font size or 0.0 on error
 *
 */

double
wFTLabelLoadFontFromResource(const char* filename)
{
	int error;

	if (!library) {
		InitializeFreeType();
	}

	GError *gerror = NULL;

	fontBytes =
	        g_resources_lookup_data(filename, G_RESOURCE_LOOKUP_FLAGS_NONE, &gerror);

	if (!fontBytes) {
		printf("Could not load font from resource: %s\n", gerror->message);
		g_abort();
	}

	gsize dataSize;
	const guchar *data = g_bytes_get_data(fontBytes, &dataSize);

	error = FT_New_Memory_Face(library,
	                           (const FT_Byte *)data,
	                           (FT_Long)dataSize,
	                           0,
	                           &ftFontFace);

	if (error == FT_Err_Unknown_File_Format) {
		printf("Font format is unsupported!\n");
		g_abort();
	} else if (error) {
		printf("Font file could not be loaded from memory! [return "
		       "code=%X]\n",
		       error);
		g_abort();
	}

	fontSize = GetFaceHeight(ftFontFace);
	return (fontSize);
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
		icon->text = g_strdup(text);
		icon->gtkIconType = ICON_PIXBUF_FROM_TEXT;
	}

	return(icon);
}

/**
 * Change the color of a label
 *
 * \param icon		the icon
 * \param color		the new color
 */

void
wlibFTLabelChangeColor(wIcon_p icon, wDrawColor color)
{
	icon->bits = CreatePixbufFromFTLabel(icon->text, color);
	icon->color = color;
}


