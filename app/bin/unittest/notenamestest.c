/** \file notenamestest.c
 * Unit tests for notenames.c's pure name-registry logic (Manage Notes,
 * SF #800 phase 3). Written test-first, per feedback_test_first_where_practical.
 *
 * notenames.c deliberately carries no dependency on common.h/dynarray.h/
 * wlib (see its header comment), so -- like layergrouptest.c/dlayergroup.c --
 * this links the real source directly with no stubs.
 *
 * Each test starts with NoteNameResetAll() since the registry is
 * process-global state shared across every test in this file's single
 * cmocka_run_group_tests() run.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include <cmocka.h>

#include <string.h>

#include "../include/notenames.h"

static void test_starts_empty(void **state)
{
	(void) state;
	NoteNameResetAll();

	assert_int_equal(NoteNameCount(), 0);
}

static void test_add_returns_increasing_index(void **state)
{
	(void) state;
	NoteNameResetAll();

	int a = NoteNameAdd("spots");
	int b = NoteNameAdd("meta");

	assert_int_equal(a, 0);
	assert_int_equal(b, 1);
	assert_int_equal(NoteNameCount(), 2);
	assert_string_equal(NoteNameAt(a), "spots");
	assert_string_equal(NoteNameAt(b), "meta");
}

static void test_add_null_or_empty_fails(void **state)
{
	(void) state;
	NoteNameResetAll();

	assert_int_equal(NoteNameAdd(NULL), -1);
	assert_int_equal(NoteNameAdd(""), -1);
	assert_int_equal(NoteNameCount(), 0);
}

static void test_add_duplicate_returns_existing_index(void **state)
{
	(void) state;
	NoteNameResetAll();

	int a = NoteNameAdd("spots");
	int b = NoteNameAdd("spots");

	assert_int_equal(a, b);
	assert_int_equal(NoteNameCount(), 1);
}

static void test_remove_valid_index(void **state)
{
	(void) state;
	NoteNameResetAll();

	NoteNameAdd("spots");
	NoteNameAdd("meta");
	NoteNameAdd("message");

	assert_int_equal(NoteNameRemove(1), 1);
	assert_int_equal(NoteNameCount(), 2);
	assert_string_equal(NoteNameAt(0), "spots");
	assert_string_equal(NoteNameAt(1), "message");
}

static void test_remove_invalid_index_fails(void **state)
{
	(void) state;
	NoteNameResetAll();

	NoteNameAdd("spots");

	assert_int_equal(NoteNameRemove(-1), 0);
	assert_int_equal(NoteNameRemove(1), 0);
	assert_int_equal(NoteNameCount(), 1);
}

static void test_at_invalid_index_returns_null(void **state)
{
	(void) state;
	NoteNameResetAll();

	NoteNameAdd("spots");

	assert_null(NoteNameAt(-1));
	assert_null(NoteNameAt(1));
}

static void test_add_after_remove_reuses_freed_slot_correctly(void **state)
{
	(void) state;
	NoteNameResetAll();

	NoteNameAdd("spots");
	NoteNameAdd("meta");
	NoteNameRemove(0);
	int c = NoteNameAdd("message");

	assert_int_equal(NoteNameCount(), 2);
	assert_string_equal(NoteNameAt(0), "meta");
	assert_int_equal(c, 1);
	assert_string_equal(NoteNameAt(1), "message");
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_starts_empty),
		cmocka_unit_test(test_add_returns_increasing_index),
		cmocka_unit_test(test_add_null_or_empty_fails),
		cmocka_unit_test(test_add_duplicate_returns_existing_index),
		cmocka_unit_test(test_remove_valid_index),
		cmocka_unit_test(test_remove_invalid_index_fails),
		cmocka_unit_test(test_at_invalid_index_returns_null),
		cmocka_unit_test(test_add_after_remove_reuses_freed_slot_correctly),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
