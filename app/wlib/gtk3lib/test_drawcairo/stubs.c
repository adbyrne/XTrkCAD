/* Link stubs for drawcairo.c: GTK widget functions and wlib dependencies.
 *
 * The GTK functions are defined here (libgtk is not linked). If the linker
 * reports further undefined gtk_* symbols, add them here. */

#include "gtkint.h"
#include "stubs.h"

#include <string.h>

recorder_t rec;

void rec_reset(void)
{
	memset(&rec, 0, sizeof rec);
	rec.alloc_w = 100;
	rec.alloc_h = 100;
}

static long long sentinel_storage[4]; /* aligned, never dereferenced */

void *test_sentinel_widget(void)
{
	return (void *)sentinel_storage;
}

/* ------------------------------------------------------------------ GTK */

void gtk_widget_queue_draw_area(GtkWidget *widget, gint x, gint y,
                                gint width, gint height)
{
	(void)widget;
	if (rec.n_area < REC_MAX_AREAS) {
		rec.areas[rec.n_area].x = x;
		rec.areas[rec.n_area].y = y;
		rec.areas[rec.n_area].w = width;
		rec.areas[rec.n_area].h = height;
	}
	rec.n_area++;
}

void gtk_widget_queue_draw(GtkWidget *widget)
{
	(void)widget;
	rec.n_all++;
}

void gtk_widget_queue_draw_region(GtkWidget *widget,
                                  const cairo_region_t *region)
{
	(void)widget;
	(void)region;
	rec.n_region++;
}

void gtk_widget_get_allocation(GtkWidget *widget, GtkAllocation *allocation)
{
	rec.n_alloc_calls++;
	if (widget == NULL) {
		/* real GTK: g_return_if_fail -> allocation is left untouched (bug 4) */
		rec.n_null_widget++;
		return;
	}
	allocation->x = 0;
	allocation->y = 0;
	allocation->width = rec.alloc_w;
	allocation->height = rec.alloc_h;
}

void gtk_widget_set_size_request(GtkWidget *widget, gint width, gint height)
{
	(void)widget;
	(void)width;
	(void)height;
	rec.n_size_request++;
}

/* ----------------------------------------------------------------- wlib */

wDrawColor wDrawColorWhite = 0xFFFFFF;
wDrawColor wDrawColorBlack = 0x000000;

GdkRGBA wlibGetColor(wDrawColor color, wBool_t normal)
{
	GdkRGBA c = { 0.0, 0.0, 0.0, 1.0 };
	(void)color;
	(void)normal;
	return c;
}

void wlibControlGetSize(wControl_p b)
{
	(void)b;
	rec.n_control_get_size++;
}

PangoLayout *wlibFontCreatePangoLayout(GtkWidget *widget, void *cairo,
                                       wFont_p fp, wFontSize_t fs, const char *s,
                                       wDrawPix_t *width_p, wDrawPix_t *height_p,
                                       wDrawPix_t *ascent_p, wDrawPix_t *descent_p,
                                       wDrawPix_t *baseline_p)
{
	(void)widget; (void)cairo; (void)fp; (void)fs; (void)s;
	*width_p = *height_p = *ascent_p = *descent_p = *baseline_p = 0;
	return NULL; /* wDrawString is not tested at this stage */
}

void wlibFontDestroyPangoLayout(PangoLayout *layout)
{
	(void)layout;
}

int GetLBorder(void) { return 0; }
int GetBBorder(void) { return 0; }

/* ------------------------------------------------------------------ GDK */

/* The real function needs an open display (it creates a surface similar to
 * the default screen's). The tests run without one, so this copies the
 * pixbuf into a plain ARGB32 image surface (premultiplied alpha). */
cairo_surface_t *gdk_cairo_surface_create_from_pixbuf(const GdkPixbuf *pixbuf,
                int scale, GdkWindow *for_window)
{
	int w = gdk_pixbuf_get_width(pixbuf);
	int h = gdk_pixbuf_get_height(pixbuf);
	int n = gdk_pixbuf_get_n_channels(pixbuf);
	int sstride = gdk_pixbuf_get_rowstride(pixbuf);
	const guchar *src = gdk_pixbuf_read_pixels(pixbuf);
	cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	unsigned char *dst;
	int dstride, x, y;

	(void)scale; (void)for_window;
	cairo_surface_flush(s);
	dst = cairo_image_surface_get_data(s);
	dstride = cairo_image_surface_get_stride(s);
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			const guchar *p = src + y * sstride + x * n;
			guint32 a = (n == 4) ? p[3] : 255;
			guint32 v = (a << 24) | ((p[0] * a / 255) << 16) |
			            ((p[1] * a / 255) << 8) | (p[2] * a / 255);
			memcpy(dst + y * dstride + x * 4, &v, 4);
		}
	}
	cairo_surface_mark_dirty(s);
	return s;
}
