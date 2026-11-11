/* Recorder for the link stubs. Deliberately free of wlib.h/gtkint.h so that
 * stubs_basicdraw.c stays independent of the wlib prototypes. */
#ifndef DRAWCAIRO_TEST_STUBS_H
#define DRAWCAIRO_TEST_STUBS_H

#include <glib.h>

#define REC_MAX_AREAS 64

typedef struct {
	int x, y, w, h;
} inval_rect_t;

typedef struct {
	/* invalidations */
	int n_area;                 /* gtk_widget_queue_draw_area calls (all, also beyond REC_MAX_AREAS) */
	inval_rect_t areas[REC_MAX_AREAS];
	int n_all;                  /* gtk_widget_queue_draw (whole widget) */
	int n_region;               /* gtk_widget_queue_draw_region */
	/* size queries */
	int alloc_w, alloc_h;       /* values returned by gtk_widget_get_allocation */
	int n_alloc_calls;
	int n_null_widget;          /* gtk_widget_get_allocation(NULL, ...) -> bug 4 */
	int n_control_get_size;     /* wlibControlGetSize */
	int n_size_request;
	/* DIRECTCAIRO branch */
	int n_basic_line, n_basic_arc, n_basic_string;
	int n_basic_fillrect, n_basic_fillpoly, n_basic_fillcircle;
} recorder_t;

extern recorder_t rec;

void rec_reset(void);

/* Non-NULL placeholder for wControl_p.widget. Must never be dereferenced. */
void *test_sentinel_widget(void);

#endif
