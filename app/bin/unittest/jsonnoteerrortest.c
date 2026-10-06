/** \file jsonnoteerrortest.c
 * Unit tests for jsonnoteerror.c: locating and classifying a JSON Note parse
 * failure (Martin Fischer's dev-ML #4404 review: "I don't understand why
 * this JSON is invalid"). Written test-first.
 *
 * Each case runs the real cJSON parser, so the error positions are exactly
 * the ones the JSON Note editor sees, not hand-computed guesses.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include <cmocka.h>

#include <string.h>

#include "../include/jsonnoteerror.h"
#include "cJSON.h"

/** Parse \p text the way JsonNoteIsValid() does, require that it fails,
 * and locate the error. */
static void
locate(const char *text, jsonNoteErrorInfo_t *info)
{
	const char *end = NULL;
	cJSON *parsed = cJSON_ParseWithOpts(text, &end, 0);
	assert_null(parsed);
	JsonNoteLocateError(text, end, info);
}

static void test_empty_text(void **state)
{
	(void) state;
	jsonNoteErrorInfo_t info;
	locate("", &info);

	assert_int_equal(info.cause, JSONNOTEERR_EMPTY);
	assert_int_equal(info.line, 1);
	assert_int_equal(info.column, 1);
	assert_string_equal(info.snippet, "");
}

static void test_whitespace_only_is_empty(void **state)
{
	(void) state;
	jsonNoteErrorInfo_t info;
	locate("  \n\t ", &info);

	assert_int_equal(info.cause, JSONNOTEERR_EMPTY);
}

static void test_typographic_double_quotes(void **state)
{
	(void) state;
	jsonNoteErrorInfo_t info;
	/* {"kind": "x"} typed with U+201C/U+201D curly quotes */
	locate("{\xE2\x80\x9Ckind\xE2\x80\x9D: \xE2\x80\x9Cx\xE2\x80\x9D}", &info);

	assert_int_equal(info.cause, JSONNOTEERR_TYPOGRAPHIC_QUOTE);
	assert_int_equal(info.line, 1);
	assert_int_equal(info.column, 2);
	assert_memory_equal(info.snippet, "\xE2\x80\x9Ckind", 7);
}

static void test_german_low_quote(void **state)
{
	(void) state;
	jsonNoteErrorInfo_t info;
	/* {„kind": 1} -- U+201E, as a German keyboard/autocorrect produces */
	locate("{\xE2\x80\x9Ekind\": 1}", &info);

	assert_int_equal(info.cause, JSONNOTEERR_TYPOGRAPHIC_QUOTE);
	assert_int_equal(info.column, 2);
}

static void test_curly_closing_quote_found_elsewhere(void **state)
{
	(void) state;
	jsonNoteErrorInfo_t info;
	/* {"kind<U+201D>: 1} -- only the closing quote is curly, so cJSON
	 * reads it as part of the string and stops somewhere else entirely;
	 * the cause must still be found. */
	locate("{\"kind\xE2\x80\x9D: 1}", &info);

	assert_int_equal(info.cause, JSONNOTEERR_TYPOGRAPHIC_QUOTE);
	/* ...and point at the curly quote itself, not where cJSON stopped */
	assert_int_equal(info.line, 1);
	assert_int_equal(info.column, 7);
	assert_memory_equal(info.snippet, "\xE2\x80\x9D: 1}", 7);
}

static void test_nonbreaking_space(void **state)
{
	(void) state;
	jsonNoteErrorInfo_t info;
	/* {"a":<U+00A0>1} -- copied from a web page or email */
	locate("{\"a\":\xC2\xA0" "1}", &info);

	assert_int_equal(info.cause, JSONNOTEERR_NONBREAKING_SPACE);
	assert_int_equal(info.line, 1);
	assert_int_equal(info.column, 6);
}

static void test_trailing_comma_is_generic(void **state)
{
	(void) state;
	jsonNoteErrorInfo_t info;
	locate("{\"a\": 1,}", &info);

	assert_int_equal(info.cause, JSONNOTEERR_GENERIC);
	assert_int_equal(info.line, 1);
	assert_string_equal(info.snippet, "}");
}

static void test_missing_comma_reports_later_line(void **state)
{
	(void) state;
	jsonNoteErrorInfo_t info;
	locate("{\n  \"a\": 1\n  \"b\": 2\n}", &info);

	assert_int_equal(info.cause, JSONNOTEERR_GENERIC);
	assert_int_equal(info.line, 3);
	assert_int_equal(info.column, 3);
	/* snippet stops at the end of the line */
	assert_string_equal(info.snippet, "\"b\": 2");
}

static void test_unterminated_object(void **state)
{
	(void) state;
	jsonNoteErrorInfo_t info;
	locate("{\"a\": 1", &info);

	assert_int_equal(info.cause, JSONNOTEERR_UNEXPECTED_END);
	assert_string_equal(info.snippet, "");
}

static void test_column_counts_characters_not_bytes(void **state)
{
	(void) state;
	jsonNoteErrorInfo_t info;
	/* {"é": x} -- 'x' is character 7 but byte 8 */
	locate("{\"\xC3\xA9\": x}", &info);

	assert_int_equal(info.cause, JSONNOTEERR_GENERIC);
	assert_int_equal(info.column, 7);
	assert_string_equal(info.snippet, "x}");
}

static void test_snippet_never_splits_a_character(void **state)
{
	(void) state;
	jsonNoteErrorInfo_t info;
	char text[128] = "{";
	for (int i = 0; i < 20; i++) {
		strcat(text, "\xE2\x80\x9C");	/* 3-byte curly quote */
	}
	locate(text, &info);

	/* 23 usable bytes hold 7 whole 3-byte characters (21 bytes), not 7
	 * and two thirds of an eighth. */
	assert_int_equal(strlen(info.snippet), 21);
}

static void test_null_errptr_means_start(void **state)
{
	(void) state;
	jsonNoteErrorInfo_t info;
	JsonNoteLocateError("x", NULL, &info);

	assert_int_equal(info.line, 1);
	assert_int_equal(info.column, 1);
	assert_string_equal(info.snippet, "x");
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_empty_text),
		cmocka_unit_test(test_whitespace_only_is_empty),
		cmocka_unit_test(test_typographic_double_quotes),
		cmocka_unit_test(test_german_low_quote),
		cmocka_unit_test(test_curly_closing_quote_found_elsewhere),
		cmocka_unit_test(test_nonbreaking_space),
		cmocka_unit_test(test_trailing_comma_is_generic),
		cmocka_unit_test(test_missing_comma_reports_later_line),
		cmocka_unit_test(test_unterminated_object),
		cmocka_unit_test(test_column_counts_characters_not_bytes),
		cmocka_unit_test(test_snippet_never_splits_a_character),
		cmocka_unit_test(test_null_errptr_means_start),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
