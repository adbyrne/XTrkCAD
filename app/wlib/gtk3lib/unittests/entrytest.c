/** \file entrytest.c
* Unit tests for wEntrySetValue(), entry.c
*
* A value set by the program must reach a focused entry unless the user has
* typed in it since it got focus. Otherwise an entry that was just tabbed
* into keeps its old text and writes it back on focus-out.
*
* The signal handlers are static, so the source file is included directly and
* xtrkcad-wlib is not linked; the few wlib helpers entry.c calls are stubbed
* below. Focus changes are sent with gtk_widget_send_focus_change(), which
* sets the widget's has-focus state like a real focus change. Uses an
* offscreen window, but GTK still needs a display connection.
*/

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include <cmocka.h>

#include "../entry.c"

/* stubs for the wlib functions entry.c uses */

wControl_p wlibControlNew(wType_e type, wControl_p parent, const char *name,
                          void *context)
{
	wControl_p b = g_new0(struct control, 1);

	b->type = type;
	b->parent = parent;
	b->context = context;
	return b;
}

void wlibBasicGridAttach(wControl_p parent, GtkWidget *widget, unsigned xPos,
                         unsigned yPos, unsigned colSpan, unsigned rowSpan)
{
}

void wlibAddLabel(wControl_p b, wWinPix_t x, wWinPix_t y,
                  const char *labelStr)
{
}

void wlibAddTooltip(GtkWidget *widget, const char *dialog,
                    const char *dialogitem)
{
}

GtkWidget *wlibWidgetFromIdWarn(wControl_p win, const char *id)
{
	return NULL;
}

typedef struct {
	struct control control;
	GtkWidget *window;
} fixture_t;

static int Setup(void **state)
{
	fixture_t *f = g_new0(fixture_t, 1);

	f->window = gtk_offscreen_window_new();
	f->control.type = B_TEXT;
	f->control.widget = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(f->window), f->control.widget);
	gtk_widget_show_all(f->window);
	entryConnectSignals(&f->control);
	wEntrySetValue(&f->control, "initial");
	*state = f;
	return 0;
}

static int Teardown(void **state)
{
	fixture_t *f = *state;

	gtk_widget_destroy(f->window);
	g_free(f);
	return 0;
}

/**
 * Give the entry focus or take it away
 */
static void SetFocus(fixture_t *f, gboolean in)
{
	GdkEvent *event = gdk_event_new(GDK_FOCUS_CHANGE);

	event->focus_change.window =
	        g_object_ref(gtk_widget_get_window(f->control.widget));
	event->focus_change.send_event = TRUE;
	event->focus_change.in = in;
	gtk_widget_send_focus_change(f->control.widget, event);
	gdk_event_free(event);
}

/**
 * Change the text the way typing does: not through wEntrySetValue()
 */
static void UserTypes(fixture_t *f, const char *text)
{
	gtk_entry_set_text(GTK_ENTRY(f->control.widget), text);
}

static const char *Text(fixture_t *f)
{
	return gtk_entry_get_text(GTK_ENTRY(f->control.widget));
}

static void SetWithoutFocus(void **state)
{
	fixture_t *f = *state;

	assert_false(gtk_widget_has_focus(f->control.widget));
	wEntrySetValue(&f->control, "new");
	assert_string_equal(Text(f), "new");
}

static void SetFocusedNotEdited(void **state)
{
	fixture_t *f = *state;

	SetFocus(f, TRUE);
	assert_true(gtk_widget_has_focus(f->control.widget));
	wEntrySetValue(&f->control, "new");
	assert_string_equal(Text(f), "new");
}

static void SetFocusedEdited(void **state)
{
	fixture_t *f = *state;

	SetFocus(f, TRUE);
	UserTypes(f, "typed");
	wEntrySetValue(&f->control, "new");
	assert_string_equal(Text(f), "typed");
}

static void NewFocusStartsNewEdit(void **state)
{
	fixture_t *f = *state;

	SetFocus(f, TRUE);
	UserTypes(f, "typed");
	SetFocus(f, FALSE);
	SetFocus(f, TRUE);
	wEntrySetValue(&f->control, "new");
	assert_string_equal(Text(f), "new");
}

static void ProgramSetIsNoEdit(void **state)
{
	fixture_t *f = *state;

	SetFocus(f, TRUE);
	wEntrySetValue(&f->control, "first");
	wEntrySetValue(&f->control, "second");
	assert_string_equal(Text(f), "second");
}

int main(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(SetWithoutFocus, Setup, Teardown),
		cmocka_unit_test_setup_teardown(SetFocusedNotEdited, Setup, Teardown),
		cmocka_unit_test_setup_teardown(SetFocusedEdited, Setup, Teardown),
		cmocka_unit_test_setup_teardown(NewFocusStartsNewEdit, Setup, Teardown),
		cmocka_unit_test_setup_teardown(ProgramSetIsNoEdit, Setup, Teardown),
	};

	gtk_init(&argc, &argv);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
