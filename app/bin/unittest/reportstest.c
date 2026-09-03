/** \file reportstest.c
* Unit tests for reports.c's pure row-formatting function.
*/

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include <cmocka.h>

#include <string.h>

#include <dynstring.h>
#include "../include/reports.h"

static void test_empty_list(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 16);

	ReportsFormatUnconnectedList(&out, NULL, 0);

	assert_string_equal(DynStringToCStr(&out), "");
	DynStringFree(&out);
}

static void test_single_row(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 16);
	reportsEndPt_t list[1] = {
		{ 1, { 0.0, 0.0 }, 180.0 }
	};

	ReportsFormatUnconnectedList(&out, list, 1);

	assert_string_equal(DynStringToCStr(&out),
	                    "     1 |    0.000 |    0.000 | 180.000\n");
	DynStringFree(&out);
}

/* Matches the phase-1 .xtr fixture (dmreportuncon.xtr): three tracks, two
 * joined to each other (endpoints not listed here), four open endpoints
 * total -- one each on tracks 1/2, both on track 3 (which has no
 * connections at all). Track 3's two rows, in endpoint order, confirm
 * multiple open endpoints on the same track id are listed individually,
 * not rolled up (the turntable-parity decision from the phase-1 plan). */
static void test_fixture_shaped_list(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 64);
	reportsEndPt_t list[4] = {
		{ 1, {  0.0, 0.0 }, 180.0 },
		{ 2, {  8.0, 0.0 },   0.0 },
		{ 3, {  0.0, 4.0 }, 180.0 },
		{ 3, {  4.0, 4.0 },   0.0 },
	};

	ReportsFormatUnconnectedList(&out, list, 4);

	assert_string_equal(DynStringToCStr(&out),
	                    "     1 |    0.000 |    0.000 | 180.000\n"
	                    "     2 |    8.000 |    0.000 |   0.000\n"
	                    "     3 |    0.000 |    4.000 | 180.000\n"
	                    "     3 |    4.000 |    4.000 |   0.000\n");
	DynStringFree(&out);
}

static void test_negative_and_fractional_values(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 16);
	reportsEndPt_t list[1] = {
		{ 42, { -1.5, 200.286947 }, 55.067831 }
	};

	ReportsFormatUnconnectedList(&out, list, 1);

	assert_string_equal(DynStringToCStr(&out),
	                    "    42 |   -1.500 |  200.287 |  55.068\n");
	DynStringFree(&out);
}

static void test_appends_not_overwrites(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 16);
	DynStringCatCStr(&out, "existing content\n");
	reportsEndPt_t list[1] = {
		{ 1, { 0.0, 0.0 }, 0.0 }
	};

	ReportsFormatUnconnectedList(&out, list, 1);

	assert_string_equal(DynStringToCStr(&out),
	                    "existing content\n"
	                    "     1 |    0.000 |    0.000 |   0.000\n");
	DynStringFree(&out);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_empty_list),
		cmocka_unit_test(test_single_row),
		cmocka_unit_test(test_fixture_shaped_list),
		cmocka_unit_test(test_negative_and_fractional_values),
		cmocka_unit_test(test_appends_not_overwrites),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
