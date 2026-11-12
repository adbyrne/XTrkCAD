/* cmocka tests for drawcairo.c
 *
 * Two kinds of tests:
 *   - Characterization tests: pin down the current behavior, always run.
 *   - KNOWN_BUG tests: reproduce a finding from the handoff document.
 *     Skipped by default. With the environment variable DRAWCAIRO_KNOWN_BUGS=1
 *     they run and are expected to FAIL before the fix. After the fix, delete
 *     the KNOWN_BUG line of the test.
 *
 * Coordinates (drawing area 100 x 100): INMAPY(y) = 99 - y.
 * Lines are drawn with a +0.5 offset: width 1 hits exactly one pixel row.
 * The pixel coordinates in the comments were computed by hand. If a
 * characterization test is red on the first run, check the calculation first.
 *
 * Running a subset (glob patterns, ':' separates several patterns):
 *   ./test_drawcairo --list                  list all test names
 *   ./test_drawcairo "*known_bug_2_*"        run only the matching tests
 *   DRAWCAIRO_FILTER="*dirty*" ./test_drawcairo
 * A pattern containing "known_bug" enables DRAWCAIRO_KNOWN_BUGS automatically.
 * A pattern that matches no test is an error (exit code 2).
 *
 * stdarg/stddef/setjmp must be included before cmocka.h. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmocka.h>

#include "gtkint.h"
#include "stubs.h"

#define W 100
#define H 100

#define COL_BLACK 0x000000
#define COL_RED   0xFF0000
#define COL_BLUE  0x0000FF

/* ARGB32, premultiplied, native byte order */
#define PX_BLACK 0xFF000000u
#define PX_RED   0xFFFF0000u

#define KNOWN_BUG(id)                                                         \
	do {                                                                  \
		if (!getenv("DRAWCAIRO_KNOWN_BUGS")) {                        \
			print_message("[skipped: known bug %s]\n", id);       \
			skip();                                               \
		}                                                             \
	} while (0)

/* ---------------------------------------------------------------------
 * Coordinate convention behind all expected pixel positions below.
 * Derived by hand from drawcairo.c, NOT yet confirmed by running the tests.
 *
 *   canvas : W x H = 100 x 100, pixel (column, row), row 0 is the TOP row
 *   app y is measured from the bottom: device_y = INMAPY(y) = (H-1) - y = 99 - y
 *
 *   line at app y          : drawn at device_y + 0.5 with width 1
 *                            -> exactly row (99 - y)
 *   filled rectangle (x, y, w, h):
 *                            top = INMAPY(y) - h = 99 - y - h
 *                            -> rows (99-y-h) .. (98-y), columns x .. x+w-1
 *                            i.e. it lies ABOVE the row of a line drawn at the same y
 *   filled circle (x0, y0, r): center at device (x0, 99 - y0), extent +-r around it
 *   clip (x, y, w, h)      : same rectangle rule as the filled rectangle
 *
 * Probe points are at least 2 px away from any edge, except in tests that are
 * about an edge itself. That keeps antialiasing out of the assertions.
 * ------------------------------------------------------------------- */

/* ------------------------------------------------------------ helpers */

static uint32_t pixel(cairo_surface_t *s, int x, int y)
{
	uint32_t v;
	const unsigned char *d;
	int stride;

	cairo_surface_flush(s);
	d = cairo_image_surface_get_data(s);
	stride = cairo_image_surface_get_stride(s);
	memcpy(&v, d + (size_t)y * stride + (size_t)x * 4, 4);
	return v;
}

static int is_set(cairo_surface_t *s, int x, int y)
{
	return (pixel(s, x, y) >> 24) != 0;
}

static void clear_surface(cairo_surface_t *s)
{
	cairo_t *cr = cairo_create(s);
	cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_flush(s);
}

static unsigned char *snapshot(cairo_surface_t *s, size_t *len)
{
	size_t n;
	unsigned char *copy;

	cairo_surface_flush(s);
	n = (size_t)cairo_image_surface_get_stride(s) *
	    (size_t)cairo_image_surface_get_height(s);
	copy = malloc(n);
	assert_non_null(copy);
	memcpy(copy, cairo_image_surface_get_data(s), n);
	if (len) {
		*len = n;
	}
	return copy;
}

static uint64_t fnv1a(const unsigned char *p, size_t n)
{
	uint64_t h = 1469598103934665603ull;
	size_t i;

	for (i = 0; i < n; i++) {
		h ^= p[i];
		h *= 1099511628211ull;
	}
	return h;
}

/* Is (x,y) inside the union of the recorded invalidations? */
static int covered(int x, int y)
{
	int i, n;

	if (rec.n_all > 0) {
		return 1;
	}
	n = rec.n_area < REC_MAX_AREAS ? rec.n_area : REC_MAX_AREAS;
	for (i = 0; i < n; i++) {
		const inval_rect_t *r = &rec.areas[i];
		if (x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h) {
			return 1;
		}
	}
	return 0;
}

/* Number of changed pixels that were NOT invalidated */
static int uncovered_changes(const unsigned char *before,
                             cairo_surface_t *after)
{
	int x, y, bad = 0;
	int stride = cairo_image_surface_get_stride(after);
	const unsigned char *a;

	cairo_surface_flush(after);
	a = cairo_image_surface_get_data(after);
	for (y = 0; y < H; y++) {
		for (x = 0; x < W; x++) {
			size_t off = (size_t)y * stride + (size_t)x * 4;
			if (memcmp(before + off, a + off, 4) != 0 && !covered(x, y)) {
				bad++;
			}
		}
	}
	return bad;
}

/* ------------------------------------------------------------ fixture */

static struct control *make_control(int with_widget)
{
	struct control *c = calloc(1, sizeof *c);
	struct draw *d;

	assert_non_null(c);
	d = CONTROL_GET_ATTRIBUTES_PTR(c, draw);
	c->type = B_DRAW;
	c->widget = with_widget ? test_sentinel_widget() : NULL;
	d->width = W;
	d->height = H;
	d->dpi = 72.0;
	d->drawDestination = 0; /* not DIRECTCAIRO */
	d->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
	d->temp_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
	return c;
}

static int setup_nowidget(void **state)
{
	rec_reset();
	*state = make_control(0);
	return 0;
}

static int setup_widget(void **state)
{
	rec_reset();
	*state = make_control(1);
	return 0;
}

static int teardown(void **state)
{
	struct control *c = *state;
	struct draw *d = CONTROL_GET_ATTRIBUTES_PTR(c, draw);

	wDrawFinish(c); /* resets the file-static cairoCtx in drawcairo.c */
	g_free(d->clipregion);
	if (d->background) {
		g_object_unref(d->background);
	}
	if (d->pixbufBackup) {
		g_object_unref(d->pixbufBackup);
	}
	cairo_surface_destroy(d->surface);
	cairo_surface_destroy(d->temp_surface);
	free(c);
	return 0;
}

#define CTX()                                                                 \
	struct control *c = *state;                                           \
	struct draw *d = CONTROL_GET_ATTRIBUTES_PTR(c, draw);                 \
	(void)d

/* ------------------------------------------------------------- scenes */

static void scene_basic(struct control *c)
{
	wDrawLine(c, 10, 10, 90, 10, 1, wDrawLineSolid, COL_BLACK, 0);
	wDrawLine(c, 10, 20, 90, 60, 3, wDrawLineDash, COL_RED, 0);
	wDrawArc(c, 50, 50, 20, 0.0, 90.0, 0, 1, wDrawLineSolid, COL_BLUE, 0);
	wDrawFilledCircle(c, 70, 70, 8, COL_RED, 0);
}

/* Bug 6 scene. Both lines lie in device row 99 - 50 = 49.
 * Legacy: the white line is widened from 3 to 4 px ("remove ghosts"),
 * batch: it stays 3 px wide. The black line (5 px) is the same in both paths,
 * so the two results differ in the rows at the edge of the white line. */
static void scene_white(struct control *c)
{
	wDrawLine(c, 10, 50, 90, 50, 5, wDrawLineSolid, COL_BLACK, 0);
	wDrawLine(c, 10, 50, 90, 50, 3, wDrawLineSolid, wDrawColorWhite, 0);
}

/* Draw the same scene in legacy mode and in batch mode: must be pixel-equal. */
static void assert_batch_equals_legacy(struct control *c,
                                       void (*scene)(struct control *))
{
	struct draw *d = CONTROL_GET_ATTRIBUTES_PTR(c, draw);
	size_t lenA, lenB;
	unsigned char *a, *b;

	scene(c);
	a = snapshot(d->surface, &lenA);

	clear_surface(d->surface);
	wDrawStart(c);
	scene(c);
	wDrawFinish(c);
	b = snapshot(d->surface, &lenB);

	assert_int_equal(lenA, lenB);
	assert_memory_equal(a, b, lenA);
	free(a);
	free(b);
}

/* ===================================================================
 * Characterization tests (must be green on the current code)
 * =================================================================== */

/* Harness smoke test: line at y=10 -> device row 89 */
static void test_line_legacy(void **state)
{
	CTX();
	wDrawLine(c, 10, 10, 90, 10, 1, wDrawLineSolid, COL_BLACK, 0);
	/* y = 10 -> device row 99 - 10 = 89. The line is centered at 89.5 with
	 * width 1, so it covers exactly row 89 over columns 10..90: (50, 89) is
	 * fully black, rows 88 and 90 are untouched. */
	assert_int_equal(pixel(d->surface, 50, 89), PX_BLACK);
	assert_false(is_set(d->surface, 50, 88));
	assert_false(is_set(d->surface, 50, 90));
}

/* The legacy path clips (gtkDrawCreateCairoContext).
 * Clip: device rows 49..98, x 0..49 */
static void test_line_clip_legacy(void **state)
{
	CTX();
	wDrawClip(c, 0, 0, 50, 50);
	wDrawLine(c, 10, 10, 90, 10, 1, wDrawLineSolid, COL_BLACK, 0);
	/* clip (0,0,50,50): columns 0..49, rows 99-0-50 = 49 .. 98.
	 * The line lies in row 89, which is inside the clip rows.
	 * Column 30 is inside the clip -> drawn, column 70 is outside -> clipped. */
	assert_true(is_set(d->surface, 30, 89));
	assert_false(is_set(d->surface, 70, 89));
}

static void test_directcairo_routing(void **state)
{
	CTX();
	d->drawDestination = DIRECTCAIRO;
	wDrawLine(c, 10, 10, 90, 10, 1, wDrawLineSolid, COL_BLACK, 0);
	assert_int_equal(rec.n_basic_line, 1);
	/* Row 89 is where drawcairo.c itself would draw this line (see
	 * test_line_legacy). It stays empty because the call was routed to
	 * wBasicDrawLine instead. */
	assert_false(is_set(d->surface, 50, 89));
}

static void test_batch_equals_legacy_basic(void **state)
{
	CTX();
	assert_batch_equals_legacy(c, scene_basic);
}

/* Filled triangle. Device coordinates (20,79) (80,79) (50,19), centroid ~(50,59). */
static const uint64_t GOLDEN_TRIANGLE =
        0xb42211c10b6fc94b; /* 0 = not recorded yet */

static void test_polygon_filled_triangle(void **state)
{
	CTX();
	wDrawPix_t p[3][2] = { { 20, 20 }, { 80, 20 }, { 50, 80 } };
	size_t len;
	unsigned char *snap;
	uint64_t h;

	wDrawPolygon(c, p, NULL, 3, COL_RED, 0, wDrawLineSolid, 0, 1, 0);
	/* Points (20,20) (80,20) (50,80) -> device (20,79) (80,79) (50,19).
	 * At row 59 the left edge runs from (20,79) to (50,19):
	 *   x = 20 + (79-59)/(79-19) * 30 = 30, the right edge is at x = 70.
	 * So (50, 59) is 20 columns inside both edges. (5, 5) is far outside. */
	assert_int_equal(pixel(d->surface, 50, 59), PX_RED);
	assert_false(is_set(d->surface, 5, 5));

	snap = snapshot(d->surface, &len);
	h = fnv1a(snap, len);
	free(snap);
	if (GOLDEN_TRIANGLE == 0) {
		print_message("golden hash (put into GOLDEN_TRIANGLE): 0x%016llx\n",
		              (unsigned long long)h);
	} else {
		assert_true(h == GOLDEN_TRIANGLE);
	}
}

/* Dirty-rect property: every changed pixel lies inside an invalidated region.
 *
 * What the legacy code invalidates (read from drawcairo.c):
 *   line    : x0..x0+|dx| (+1 px), the row of the line, 1 row high
 *             -> covers a horizontal line of width 1 exactly
 *   circle  : (x0-r, 99-(y0+r)), size 2r x 2r
 *             -> covers the filled circle exactly
 *   polygon : bounding box of the device points (+1 px)
 *             -> covers a filled polygon with integer vertices
 * Strokes wider than 1 px and antialiased arcs spill over these rectangles.
 * That is finding N6 and is tested separately. */
static void draw_hline(struct control *c)
{
	wDrawLine(c, 10, 10, 90, 10, 1, wDrawLineSolid, COL_BLACK, 0);
}

static void draw_circle(struct control *c)
{
	wDrawFilledCircle(c, 50, 50, 20, COL_RED, 0);
}

static void draw_triangle(struct control *c)
{
	wDrawPix_t p[3][2] = { { 20, 20 }, { 80, 20 }, { 50, 80 } };
	wDrawPolygon(c, p, NULL, 3, COL_RED, 0, wDrawLineSolid, 0, 1, 0);
}

static void test_dirty_rect_legacy(void **state)
{
	CTX();
	static const struct {
		const char *name;
		void (*fn)(struct control *);
	} cases[] = {
		{ "hline", draw_hline },
		{ "filled circle", draw_circle },
		{ "filled triangle", draw_triangle },
	};
	size_t i;

	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		unsigned char *before;

		clear_surface(d->surface);
		before = snapshot(d->surface, NULL);
		rec_reset();
		cases[i].fn(c);
		if (uncovered_changes(before, d->surface) != 0) {
			print_message("uncovered changes in case: %s\n", cases[i].name);
			free(before);
			fail();
		}
		free(before);
	}
}

static GdkPixbuf *new_pixbuf(void)
{
	GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);

	assert_non_null(pb);
	return pb;
}



/* the target already holds a background and the source has another one.
 * The old target background had a single owner (the target). After the call
 * it must be gone: the weak pointer is cleared when the pixbuf is finalized. */
static void test_releases_old_target_background(void **state)
{
	CTX();
	struct control *other = make_control(0);
	struct draw *od = CONTROL_GET_ATTRIBUTES_PTR(other, draw);
	void *other_state = other;
	GdkPixbuf *src_pb = new_pixbuf();
	GdkPixbuf *old_pb = new_pixbuf();
	GdkPixbuf *weak = old_pb;
	int shares, released;

	d->background = src_pb;         /* the source owns one reference */
	od->background = old_pb;        /* the target is the only owner */
	g_object_add_weak_pointer(G_OBJECT(old_pb), (gpointer *)&weak);

	wDrawCloneBackground(c, other);

	shares = (od->background == src_pb);
	released = (weak == NULL);
	if (!released) {
		/* leaked: clean up by hand so the test itself does not leak */
		g_object_remove_weak_pointer(G_OBJECT(old_pb), (gpointer *)&weak);
		g_object_unref(old_pb);
	}
	teardown(&other_state);

	assert_true(shares);
	assert_true(released);          /* fails today: old pixbuf is still alive */
}

/* the source has no background, so the target's pointer is
 * set to NULL. The old target background must be released here as well. */
static void test_empty_source_releases_old_target(void **state)
{
	CTX();
	struct control *other = make_control(0);
	struct draw *od = CONTROL_GET_ATTRIBUTES_PTR(other, draw);
	void *other_state = other;
	GdkPixbuf *old_pb = new_pixbuf();
	GdkPixbuf *weak = old_pb;
	int target_empty, released;

	d->background = NULL;
	od->background = old_pb;
	g_object_add_weak_pointer(G_OBJECT(old_pb), (gpointer *)&weak);

	wDrawCloneBackground(c, other);

	target_empty = (od->background == NULL);
	released = (weak == NULL);
	if (!released) {
		g_object_remove_weak_pointer(G_OBJECT(old_pb), (gpointer *)&weak);
		g_object_unref(old_pb);
	}
	teardown(&other_state);

	assert_true(target_empty);
	assert_true(released);          /* fails today */
}

/* cloning a control into itself.
 * The control owns one reference. After the call it must still own exactly
 * one (ref_count 1) and the pixbuf must be alive.
 *   today:            g_object_ref without unref -> ref_count 2 (leak)
 *   correct fix:      ref the new one first, then unref the old -> ref_count 1
 *   WRONG fix order:  unref first -> the pixbuf is finalized, then
 *                     g_object_ref touches freed memory. ASan reports it,
 *                     so this test also guards against the wrong order. */
static void test_clone_into_itself(void **state)
{
	CTX();
	GdkPixbuf *pb = new_pixbuf();
	int same, refs;

	d->background = pb;             /* single reference, owned by c */
	wDrawCloneBackground(c, c);

	same = (d->background == pb);
	refs = (int)G_OBJECT(pb)->ref_count;
	if (refs > 1) {
		g_object_unref(pb);     /* drop the leaked reference ourselves */
	}

	assert_true(same);
	assert_int_equal(refs, 1);      /* fails today: it is 2 */
}


/* Characterization: the stored value follows the argument, and nothing is
 * invalidated while the delay is only switched on or stays unchanged. */
static void test_delayupdate_stores_value_and_is_quiet(void **state)
{
	CTX();
	assert_int_equal(d->delayUpdate, 0);

	wDrawDelayUpdate(c, FALSE);      /* off -> off */
	assert_int_equal(d->delayUpdate, 0);
	assert_int_equal(rec.n_all, 0);

	wDrawDelayUpdate(c, TRUE);       /* off -> on */
	assert_int_equal(d->delayUpdate, 1);
	assert_int_equal(rec.n_all, 0);

	wDrawDelayUpdate(c, TRUE);       /* on -> on */
	assert_int_equal(d->delayUpdate, 1);
	assert_int_equal(rec.n_all, 0);

	assert_int_equal(rec.n_area, 0);
}

/* Characterization: that is what the delay is for.
 *  - while delayed, drawing still reaches the surface but nothing is invalidated
 *  - releasing the delay invalidates the whole widget exactly once
 *  - after the release, drawing invalidates its own area again */
static void test_delayupdate_delays_and_releases_invalidation(void **state)
{
	CTX();
	wDrawDelayUpdate(c, TRUE);

	wDrawLine(c, 10, 10, 90, 10, 1, wDrawLineSolid, COL_BLACK, 0);
	assert_true(is_set(d->surface, 50, 89));  /* drawn ... */
	assert_int_equal(rec.n_area, 0);          /* ... but not invalidated */

	rec_reset();                              /* count the release only */
	wDrawDelayUpdate(c, FALSE);
	assert_int_equal(d->delayUpdate, 0);
	assert_int_equal(rec.n_all, 1);           /* one whole-widget invalidation */

	rec_reset();
	wDrawLine(c, 10, 20, 90, 20, 1, wDrawLineSolid, COL_BLACK, 0);
	assert_true(is_set(d->surface, 50, 79));
	assert_int_equal(rec.n_area, 1);          /* delay is really off again */
}

/* Today: n_region == 1 (queue_draw_region), n_alloc_calls == 1 and
 * n_control_get_size == 1 (wDrawGetSize). After the fix: only the single
 * gtk_widget_queue_draw(). Checked in this order so that the first failing
 * assert names the most obvious symptom. */
static void test_release_is_single_invalidation(void **state)
{
	CTX();
	wDrawDelayUpdate(c, TRUE);
	rec_reset();

	wDrawDelayUpdate(c, FALSE);

	assert_int_equal(rec.n_region, 0);            /* redundant region call */
	assert_int_equal(rec.n_alloc_calls, 0);       /* size is not needed */
	assert_int_equal(rec.n_control_get_size, 0);
	assert_int_equal(rec.n_all, 1);               /* the one that stays */
	assert_int_equal(d->delayUpdate, 0);
}

/*
 * Today the release path calls wDrawGetSize (gtk_widget_get_allocation(NULL),
 * bug 4) and passes the NULL widget to both invalidation calls. After the
 * fix nothing is invalidated and nothing touches the widget, but the stored
 * value is still updated: a control without widget must not stay "delayed"
 * forever. */
static void test_release_without_widget(void **state)
{
	CTX();
	wDrawDelayUpdate(c, TRUE);
	rec_reset();

	wDrawDelayUpdate(c, FALSE);

	assert_int_equal(rec.n_null_widget, 0);       /* no get_allocation(NULL) */
	assert_int_equal(rec.n_region, 0);            /* no invalidation of NULL */
	assert_int_equal(rec.n_all, 0);
	assert_int_equal(d->delayUpdate, 0);          /* value still updated */
}



/* Bug 2: opaque wDrawFilledRectangle draws nothing in legacy mode.
 * Rectangle (x=10, y=10, w=20, h=20): top row = 99 - 10 - 20 = 69, so rows
 * 69..88 and columns 10..29. The center (20, 79) is 10 px from every edge. */
static void test_filled_rect_legacy(void **state)
{
	CTX();
	wDrawFilledRectangle(c, 10, 10, 20, 20, COL_RED, 0);
	assert_int_equal(pixel(d->surface, 20, 79), PX_RED);
	/* Rectangle (10,10,20,20): columns 10..29, rows 69..88 (see convention block).
	 * Corners of the rectangle are painted: */
	assert_int_equal(pixel(d->surface, 10, 69), PX_RED);   /* top-left     */
	assert_int_equal(pixel(d->surface, 29, 88), PX_RED);   /* bottom-right */
	/* Just outside, in each direction: */
	assert_false(is_set(d->surface, 9, 79));    /* left  */
	assert_false(is_set(d->surface, 30, 79));   /* right */
	assert_false(is_set(d->surface, 20, 68));   /* above */
	assert_false(is_set(d->surface, 20,
	                    89));   /* below: the row a line at y=10 would use */
}

/* Batch path draws the opaque rectangle already: same geometry as bug 2. */
static void test_filled_rect_batch(void **state)
{
	CTX();
	wDrawStart(c);
	wDrawFilledRectangle(c, 10, 10, 20, 20, COL_RED, 0);
	wDrawFinish(c);
	/* rows 69..88, columns 10..29, center (20, 79) */
	assert_int_equal(pixel(d->surface, 20, 79), PX_RED);
}

/* bd->fill stays TRUE after a batch with a rectangle, so a legacy arc
 * is filled instead of stroked */
static void test_arc_after_rect_batch(void **state)
{
	CTX();
	wDrawStart(c);
	wDrawFilledRectangle(c, 10, 10, 20, 20, COL_RED, 0);
	wDrawFinish(c);
	clear_surface(d->surface);

	wDrawArc(c, 50, 50, 20, 0.0, 360.0, 0, 1, wDrawLineSolid, COL_BLACK, 0);
	/* Arc center (50,50) -> device (50, 49), r = 20, angles 0..360 = full ring.
	 * The center is 20 px from the ring, so it must stay empty when stroked
	 * (the bug fills it). The rightmost ring point is at x = 50 + 20 = 70;
	 * a 1 px line centered on x = 70.0 covers 69.5..70.5, so pixels 69 and 70
	 * are each half covered (alpha > 0). Either one proves the ring exists. */
	assert_false(is_set(d->surface, 50, 49));
	assert_true(is_set(d->surface, 69, 49) || is_set(d->surface, 70, 49));
}


/* Bug 4: wDrawGetSize without a widget */
static void test_getsize_without_widget(void **state)
{
	CTX();
	wWinPix_t w = -12345, h = -12345;
	wDrawGetSize(c, &w, &h);
	assert_int_equal(rec.n_null_widget, 0);
	assert_true(w != -12345 && h != -12345);
}

/* Bug 5: wDrawUnrefBackground without a background */
static int n_critical;

static void count_log(const gchar *domain, GLogLevelFlags level,
                      const gchar *msg, gpointer data)
{
	(void)domain; (void)msg; (void)data;
	if (level & G_LOG_LEVEL_CRITICAL) {
		n_critical++;
	}
}

static void test_unref_without_background(void **state)
{
	CTX();
	GLogFunc old;
	n_critical = 0;
	old = g_log_set_default_handler(count_log, NULL);
	wDrawUnrefBackground(c);
	g_log_set_default_handler(old, NULL);
	assert_int_equal(n_critical, 0);
}

/* 7c: wDrawPolygon with cnt == 0 reads p[0]. Only visible under ASan. */
static void test_polygon_zero_points(void **state)
{
	CTX();
	wDrawPix_t (*p)[2];
	p = malloc(1); /* deliberately too small: p[0] is out of bounds */
	assert_non_null(p);
	wDrawPolygon(c, p, NULL, 0, COL_BLACK, 1, wDrawLineSolid, 0, 0, 0);
	free(p);
}

/* ===================================================================
 * Known-bug tests (red before the fix, only with DRAWCAIRO_KNOWN_BUGS=1)
 * =================================================================== */

/* 7e: wDrawBitMap does not invalidate.
 * A missing resource yields a 1x1 transparent bitmap. */
static void test_known_bug_7e_bitmap_invalidation(void **state)
{
	CTX();
	wDrawBitMap_p bm;
	KNOWN_BUG("7e");
	bm = wDrawBitMapCreate(c, 0, 0, "/xtrkcad-test/", "missing.png");
	assert_non_null(bm); /* intentionally not freed: the type is private */
	rec_reset();
	wDrawBitMap(c, bm, 50, 50, COL_BLACK, 0);
	assert_true(rec.n_area + rec.n_all >= 1);
}

/* 1a: lines ignore the clip in batch mode */
static void test_known_bug_1a_clip_ignored_in_batch(void **state)
{
	CTX();
	KNOWN_BUG("1a");
	wDrawStart(c);
	wDrawClip(c, 0, 0, 50, 50);
	wDrawLine(c, 10, 10, 90, 10, 1, wDrawLineSolid, COL_BLACK, 0);
	wDrawFinish(c);
	/* Same geometry as test_line_clip_legacy: clip = columns 0..49, rows 49..98,
	 * line in row 89. Column 30 is inside, column 70 is outside the clip. */
	assert_true(is_set(d->surface, 30, 89));
	assert_false(is_set(d->surface, 70, 89)); /* outside the clip */
}

/* 1b: the clip applied by a circle sticks to the shared batch context.
 * Line (60,60)-(90,60) -> device row 39, outside the old clip. */
static void test_known_bug_1b_clip_leaks_in_batch(void **state)
{
	CTX();
	KNOWN_BUG("1b");
	wDrawStart(c);
	wDrawClip(c, 0, 0, 50, 50);
	wDrawFilledCircle(c, 25, 25, 5, COL_RED, 0);
	wDrawClipClear(c);
	wDrawLine(c, 60, 60, 90, 60, 1, wDrawLineSolid, COL_BLACK, 0);
	wDrawFinish(c);
	/* Clip (0,0,50,50) = columns 0..49, rows 49..98.
	 * Circle center (25,25) -> device (25, 74), r = 5: inside the clip, so it
	 * is drawn while the clip is active.
	 * Line y = 60 -> row 99 - 60 = 39, columns 60..90: above row 49, i.e.
	 * outside the old clip, so it only shows if the clip was really cleared. */
	assert_true(is_set(d->surface, 25, 74)); /* circle was drawn */
	assert_true(is_set(d->surface, 75, 39)); /* line visible after ClipClear */
}

/* 6: a white line is not widened in batch mode */
static void test_known_bug_6_white_line_batch_vs_legacy(void **state)
{
	CTX();
	KNOWN_BUG("6");
	assert_batch_equals_legacy(c, scene_white);
}

/* N6: invalidation ignores the stroke width */
static void test_known_bug_N6_dirty_rect_stroke_width(void **state)
{
	CTX();
	unsigned char *before;
	KNOWN_BUG("N6");
	before = snapshot(d->surface, NULL);
	rec_reset();
	/* y = 50 -> device row 49, line centered at 49.5 with width 5: it covers
	 * 47.0 .. 52.0 = rows 47..51. The legacy code invalidates only the row of
	 * the line (height |dy| + 1 = 1 -> row 49), so rows 47, 48, 50, 51 are
	 * changed but not invalidated. */
	wDrawLine(c, 10, 50, 90, 50, 5, wDrawLineSolid, COL_BLACK, 0);
	assert_int_equal(uncovered_changes(before, d->surface), 0);
	free(before);
}

/* N4 scene: a wide dashed line, then a transparent filled rectangle.
 * Line y = 90 -> row 9, width 9 -> rows 5..13, far away from the rectangle.
 * Rectangle (30, 20, 40, 40): columns 30..69, rows 99-20-40 = 39 .. 78. */
static void scene_transparent_after_wide(struct control *c)
{
	wDrawLine(c, 10, 90, 90, 90, 9, wDrawLineDash, COL_BLACK, 0);
	wDrawFilledRectangle(c, 30, 20, 40, 40, COL_RED, wDrawOptTransparent);
}

/* N4 (fixed): the transparent wDrawFilledRectangle in batch mode stroked its
 * outline with the width and dash left over from the previous batched segment.
 * Legacy: outline width 1, solid. The left edge lies on x = 30.0, so a 1 px
 * stroke covers 29.5 .. 30.5 and column 27 (3 px outside) stays empty.
 * Batch (before the fix): width 9 covers 25.5 .. 34.5, so column 27 is painted. */
static void test_transparent_rect_batch_width(void **state)
{
	CTX();
	size_t lenA, lenB;
	unsigned char *a, *b;

	scene_transparent_after_wide(c);
	a = snapshot(d->surface, &lenA);
	assert_false(is_set(d->surface, 27, 59));   /* legacy: thin outline */

	clear_surface(d->surface);
	wDrawStart(c);
	scene_transparent_after_wide(c);
	wDrawFinish(c);
	b = snapshot(d->surface, &lenB);
	assert_false(is_set(d->surface, 27, 59));   /* batch: must match */

	assert_int_equal(lenA, lenB);
	assert_memory_equal(a, b, lenA);
	free(a);
	free(b);
}



/* ------------------------------------------------------------------ main */

/* Does `name` match one of the ':'-separated glob patterns?
 * Positive patterns only. Used to detect a filter that selects nothing,
 * which cmocka itself would silently report as "0 tests, all passed". */
static int matches_filter(const char *name, const char *pattern)
{
	/* GLib, not POSIX fnmatch() and strtok_r(), which MinGW lacks */
	gchar **toks = g_strsplit(pattern, ":", -1);
	int hit = 0;

	for (gchar **tok = toks; *tok != NULL; tok++) {
		if (**tok != '\0' && g_pattern_match_simple(*tok, name)) {
			hit = 1;
		}
	}
	g_strfreev(toks);
	return hit;
}

int main(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		/* characterization */
		cmocka_unit_test_setup_teardown(test_filled_rect_batch, setup_nowidget, teardown),
		cmocka_unit_test_setup_teardown(test_filled_rect_legacy, setup_nowidget, teardown),
		cmocka_unit_test_setup_teardown(test_unref_without_background, setup_nowidget, teardown),
		cmocka_unit_test_setup_teardown(test_getsize_without_widget, setup_nowidget, teardown),
		cmocka_unit_test_setup_teardown(test_line_legacy, setup_nowidget, teardown),
		cmocka_unit_test_setup_teardown(test_line_clip_legacy, setup_nowidget, teardown),
		cmocka_unit_test_setup_teardown(test_directcairo_routing, setup_nowidget, teardown),
		cmocka_unit_test_setup_teardown(test_batch_equals_legacy_basic, setup_nowidget, teardown),
		cmocka_unit_test_setup_teardown(test_polygon_filled_triangle, setup_nowidget, teardown),
		cmocka_unit_test_setup_teardown(test_dirty_rect_legacy, setup_widget, teardown),
		cmocka_unit_test_setup_teardown(test_releases_old_target_background, setup_nowidget, teardown),
		cmocka_unit_test_setup_teardown(test_empty_source_releases_old_target, setup_nowidget, teardown),
		cmocka_unit_test_setup_teardown(test_clone_into_itself, setup_nowidget, teardown),
		cmocka_unit_test_setup_teardown(test_delayupdate_stores_value_and_is_quiet, setup_widget, teardown),
		cmocka_unit_test_setup_teardown(test_delayupdate_delays_and_releases_invalidation, setup_widget, teardown),
		cmocka_unit_test_setup_teardown(test_release_is_single_invalidation, setup_widget, teardown),
		cmocka_unit_test_setup_teardown(test_release_without_widget, setup_nowidget, teardown),
		cmocka_unit_test_setup_teardown(test_arc_after_rect_batch, setup_nowidget, teardown),
		cmocka_unit_test_setup_teardown(test_transparent_rect_batch_width, setup_nowidget, teardown),

		/* keep last: under ASan this test aborts the whole process */
		cmocka_unit_test_setup_teardown(test_polygon_zero_points, setup_nowidget, teardown),

		/* known bugs */

		cmocka_unit_test_setup_teardown(test_known_bug_7e_bitmap_invalidation, setup_widget, teardown),
		cmocka_unit_test_setup_teardown(test_known_bug_1a_clip_ignored_in_batch, setup_nowidget, teardown),
		cmocka_unit_test_setup_teardown(test_known_bug_1b_clip_leaks_in_batch, setup_widget, teardown),
		cmocka_unit_test_setup_teardown(test_known_bug_6_white_line_batch_vs_legacy, setup_nowidget, teardown),
		cmocka_unit_test_setup_teardown(test_known_bug_N6_dirty_rect_stroke_width, setup_widget, teardown),


	};
	const size_t n_tests = sizeof tests / sizeof tests[0];
	const char *pattern = NULL;
	size_t i;

	if (argc > 1 && strcmp(argv[1], "--list") == 0) {
		for (i = 0; i < n_tests; i++) {
			printf("%s\n", tests[i].name);
		}
		return 0;
	}

	if (argc > 1) {
		pattern = argv[1];
	} else {
		pattern = getenv("DRAWCAIRO_FILTER");
	}

	if (pattern != NULL && *pattern != '\0') {
		size_t hits = 0;

		for (i = 0; i < n_tests; i++) {
			if (matches_filter(tests[i].name, pattern)) {
				hits++;
			}
		}
		if (hits == 0) {
			fprintf(stderr, "no test matches filter '%s' (try --list)\n", pattern);
			return 2;
		}
		/* explicitly selecting a known-bug test means: run it */
		if (strstr(pattern, "known_bug") != NULL) {
			/* GLib, not POSIX setenv(), which Windows lacks */
			g_setenv("DRAWCAIRO_KNOWN_BUGS", "1", FALSE);
		}
		cmocka_set_test_filter(pattern);
	}

	return cmocka_run_group_tests(tests, NULL, NULL);
}
