/** \file textpositiontest.c
* Unit tests for wTextSetPosition(), text.c
*
* Uses an offscreen window, so no window appears on the desktop. Like the other
* wMain() based tests this still needs a display connection.
*/

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include <cmocka.h>

#include <gtk/gtk.h>

#include "wlib.h"
#include "../gtkint.h"

long dontHideCursor;

#define LINECOUNT (500)

typedef struct {
	struct control control;
	GtkWidget * window;
	GtkWidget * scroll;
} fixture_t;

/**
 * Let GTK finish pending layout and scrolling
 */
static void RunPendingEvents(void)
{
	gint64 end = g_get_monotonic_time() + 200 * G_TIME_SPAN_MILLISECOND;

	while (g_get_monotonic_time() < end) {
		while (gtk_events_pending()) {
			gtk_main_iteration();
		}
		g_usleep(5 * G_TIME_SPAN_MILLISECOND);
	}
}

static int Setup(void **state)
{
	fixture_t * f = g_new0(fixture_t, 1);

	f->window = gtk_offscreen_window_new();
	f->scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(f->scroll, 300, 200);
	f->control.attributes.text.text = gtk_text_view_new();
	gtk_container_add(GTK_CONTAINER(f->scroll), f->control.attributes.text.text);
	gtk_container_add(GTK_CONTAINER(f->window), f->scroll);
	*state = f;
	return 0;
}

static int Teardown(void **state)
{
	fixture_t * f = *state;

	gtk_widget_destroy(f->window);
	g_free(f);
	return 0;
}

static double ScrollOffset(fixture_t * f)
{
	return gtk_adjustment_get_value(
	               gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(f->scroll)));
}

static double ScrollBottom(fixture_t * f)
{
	GtkAdjustment * adj =
	        gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(f->scroll));

	return gtk_adjustment_get_upper(adj) - gtk_adjustment_get_page_size(adj);
}

static int CursorOffset(fixture_t * f)
{
	GtkTextBuffer * tb = gtk_text_view_get_buffer(
	                             GTK_TEXT_VIEW(f->control.attributes.text.text));
	GtkTextIter iter;

	gtk_text_buffer_get_iter_at_mark(tb, &iter, gtk_text_buffer_get_insert(tb));
	return gtk_text_iter_get_offset(&iter);
}

static void Fill(fixture_t * f)
{
	for (int i = 0; i < LINECOUNT; i++) {
		wTextAppend(&f->control, "a line of the parts list\n");
	}
}

/**
 * The parts list dialog fills a hidden control and then returns to the top
 * before showing it. Every append scrolls to the end, so without a working
 * wTextSetPosition() the list opens scrolled to the bottom.
 */
static void TopBeforeShowOpensAtTop(void **state)
{
	fixture_t * f = *state;

	Fill(f);
	wTextSetPosition(&f->control, 0);
	gtk_widget_show_all(f->window);
	RunPendingEvents();

	assert_true(ScrollOffset(f) < 1.0);
	assert_int_equal(CursorOffset(f), 0);
}

/**
 * Same on an already visible control
 */
static void TopWhileVisibleScrollsToTop(void **state)
{
	fixture_t * f = *state;

	gtk_widget_show_all(f->window);
	RunPendingEvents();
	Fill(f);
	RunPendingEvents();
	// the appends scrolled all the way to the end (Development Issues #65)
	assert_true(ScrollOffset(f) >= ScrollBottom(f) - 1.0);

	wTextSetPosition(&f->control, 0);
	RunPendingEvents();

	assert_true(ScrollOffset(f) < 1.0);
	assert_int_equal(CursorOffset(f), 0);
}

/**
 * A position past the end of the text goes to the end of the text
 */
static void PositionPastEndGoesToEnd(void **state)
{
	fixture_t * f = *state;
	GtkTextBuffer * tb = gtk_text_view_get_buffer(
	                             GTK_TEXT_VIEW(f->control.attributes.text.text));

	Fill(f);
	wTextSetPosition(&f->control, 0);
	wTextSetPosition(&f->control, G_MAXINT);

	assert_int_equal(CursorOffset(f), gtk_text_buffer_get_char_count(tb));
}

/**
 * Moving the position is not an edit of the text
 */
static void PositionDoesNotMarkModified(void **state)
{
	fixture_t * f = *state;

	Fill(f);
	assert_false(wTextGetModified(&f->control));

	wTextSetPosition(&f->control, 0);

	assert_false(wTextGetModified(&f->control));
}

void wPrintVersion(void)
{
}

wControl_p wMain(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(TopBeforeShowOpensAtTop, Setup, Teardown),
		cmocka_unit_test_setup_teardown(TopWhileVisibleScrollsToTop, Setup, Teardown),
		cmocka_unit_test_setup_teardown(PositionPastEndGoesToEnd, Setup, Teardown),
		cmocka_unit_test_setup_teardown(PositionDoesNotMarkModified, Setup, Teardown),
	};

	wInitAppName("textpositiontest");

	// see mrulisttest.c: wMain()'s return value does not reach the process
	// exit code, so report the result directly
	exit(cmocka_run_group_tests(tests, NULL, NULL));
}
