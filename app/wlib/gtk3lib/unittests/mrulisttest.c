/** \file mrulisttest.c
* Unit tests for the MRU (most-recently-used) list, mrulist.c
*/

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include <cmocka.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "wlib.h"
#include "../gtkint.h"

unsigned dontHideCursor;

static int a = 1, b = 2, c = 3, d = 4;

static void NewEntriesGoToHead(void **state)
{
	(void)state;
	MRUList *list = MRUCreate(-1);

	assert_null(MRUTouchEntry(list, "a", &a));
	assert_null(MRUTouchEntry(list, "b", &b));
	assert_null(MRUTouchEntry(list, "c", &c));

	assert_int_equal(MRUGetCount(list), 3);
	assert_ptr_equal(MRUGetNth(list, 0), &c);
	assert_ptr_equal(MRUGetNth(list, 1), &b);
	assert_ptr_equal(MRUGetNth(list, 2), &a);

	MRUDestroy(list);
}

static void TouchExistingMovesToHead(void **state)
{
	(void)state;
	MRUList *list = MRUCreate(-1);

	MRUTouchEntry(list, "a", &a);
	MRUTouchEntry(list, "b", &b);
	MRUTouchEntry(list, "c", &c);
	// order is now: c, b, a

	MRUTouchEntry(list, "a", &a);
	// re-touching a must not grow the list, and must move a to the head
	assert_int_equal(MRUGetCount(list), 3);
	assert_ptr_equal(MRUGetNth(list, 0), &a);
	assert_ptr_equal(MRUGetNth(list, 1), &c);
	assert_ptr_equal(MRUGetNth(list, 2), &b);

	MRUDestroy(list);
}

/**
 * Regression test for the bug where re-touching an already-present label
 * silently discarded the newly-passed-in entry pointer instead of
 * replacing the tracked one -- the caller (recentuse.c's PushListEntry)
 * would create a fresh widget for it, but since MRUTouchEntry never
 * reported the old one back, the caller had no way to know its previous
 * widget was no longer tracked and needed disposing of, leaking an
 * "orphan" menu item that never got reordered or removed again.
 */
static void TouchExistingReplacesUserdataAndReturnsOld(void **state)
{
	(void)state;
	MRUList *list = MRUCreate(-1);

	MRUTouchEntry(list, "a", &a);

	// touching "a" again with a *different* userdata pointer must update
	// the tracked entry to the new pointer, and hand back the old one so
	// the caller can dispose of it -- not silently drop it.
	void *displaced = MRUTouchEntry(list, "a", &b);

	assert_ptr_equal(displaced, &a);
	assert_int_equal(MRUGetCount(list), 1);
	assert_ptr_equal(MRUGetNth(list, 0), &b);

	MRUDestroy(list);
}

static void AppendPreservesInsertionOrder(void **state)
{
	(void)state;
	MRUList *list = MRUCreate(-1);

	assert_null(MRUAppendEntry(list, "a", &a));
	assert_null(MRUAppendEntry(list, "b", &b));
	assert_null(MRUAppendEntry(list, "c", &c));

	assert_int_equal(MRUGetCount(list), 3);
	assert_ptr_equal(MRUGetNth(list, 0), &a);
	assert_ptr_equal(MRUGetNth(list, 1), &b);
	assert_ptr_equal(MRUGetNth(list, 2), &c);

	MRUDestroy(list);
}

static void AppendExistingKeepsPositionButReplacesUserdata(void **state)
{
	(void)state;
	MRUList *list = MRUCreate(-1);

	MRUAppendEntry(list, "a", &a);
	MRUAppendEntry(list, "b", &b);
	MRUAppendEntry(list, "c", &c);

	// re-appending "a" (first position) must not move it, must update its
	// userdata, and must hand back the displaced pointer
	void *displaced = MRUAppendEntry(list, "a", &d);

	assert_ptr_equal(displaced, &a);
	assert_int_equal(MRUGetCount(list), 3);
	assert_ptr_equal(MRUGetNth(list, 0), &d);
	assert_ptr_equal(MRUGetNth(list, 1), &b);
	assert_ptr_equal(MRUGetNth(list, 2), &c);

	MRUDestroy(list);
}

static void CapacityEvictsOldest(void **state)
{
	(void)state;
	MRUList *list = MRUCreate(2);

	assert_null(MRUTouchEntry(list, "a", &a));
	assert_null(MRUTouchEntry(list, "b", &b));
	// list is now full (2 of 2); adding a third evicts the oldest ("a")
	void *evicted = MRUTouchEntry(list, "c", &c);

	assert_ptr_equal(evicted, &a);
	assert_int_equal(MRUGetCount(list), 2);
	assert_ptr_equal(MRUGetNth(list, 0), &c);
	assert_ptr_equal(MRUGetNth(list, 1), &b);

	MRUDestroy(list);
}

static void RemoveEntryByLabel(void **state)
{
	(void)state;
	MRUList *list = MRUCreate(-1);

	MRUTouchEntry(list, "a", &a);
	MRUTouchEntry(list, "b", &b);

	void *removed = MRURemoveEntry(list, "a");
	assert_ptr_equal(removed, &a);
	assert_int_equal(MRUGetCount(list), 1);
	assert_ptr_equal(MRUGetNth(list, 0), &b);

	assert_null(MRURemoveEntry(list, "not-there"));

	MRUDestroy(list);
}

wControl_p wMain(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(NewEntriesGoToHead),
		cmocka_unit_test(TouchExistingMovesToHead),
		cmocka_unit_test(TouchExistingReplacesUserdataAndReturnsOld),
		cmocka_unit_test(AppendPreservesInsertionOrder),
		cmocka_unit_test(AppendExistingKeepsPositionButReplacesUserdata),
		cmocka_unit_test(CapacityEvictsOldest),
		cmocka_unit_test(RemoveEntryByLabel),
	};

	wInitAppName("mrulisttest");

	// wMain()'s return value feeds GTK window management, not the process
	// exit code (app/wlib/gtk3lib/main.c's startup() discards it and the
	// real exit status comes from g_application_run(), unrelated to
	// cmocka's result) -- so a failed assertion here would otherwise never
	// make ctest see a non-zero exit code. Exit directly instead.
	exit(cmocka_run_group_tests(tests, NULL, NULL));
}
