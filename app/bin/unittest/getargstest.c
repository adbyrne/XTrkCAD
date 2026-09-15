/** \file getargstest.c
 * Unit tests for GetArgs()'s 'q' quoted-field format code (getargs.c),
 * round-tripped against the real ConvertToEscapedText()/ConvertFromEscapedText()
 * (misc.c) it's paired with on every NOTE read/write.
 *
 * JSON Note (OP_NOTEJSON) stores its raw JSON text through this exact
 * mechanism -- the escaping round-trip (CSV-style "" doubling for embedded
 * quotes, backslash-escaping for \n/\t/\\) was previously only confirmed by
 * reading both sides of the code, never exercised by a real test. A JSON
 * body is exactly the kind of text (many embedded quotes) this round-trip
 * needs to get right.
 */

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>

#include "common.h"
#include "../getargs.c"

/* ---- Minimal stubs for GetArgs()'s dependencies, matching
 * app/bin/fuzz/fuzz_getargs.c's already-established minimal stub set for
 * this exact extraction (getargs.c linked standalone, without the rest of
 * fileio.c/misc.c's ~100-symbol dependency graph) -- see that file's header
 * comment for why this is the right shape. Unlike the fuzz harness, this
 * test links the REAL ConvertToEscapedText()/ConvertFromEscapedText()
 * (misc.c, compiled as a separate source in this target -- see CMakeLists)
 * instead of stubbing them, since round-trip correctness (not just
 * crash-safety) is the point here. misc.c's own LOG()/LogFindIndex() calls
 * (its ConvertToEscapedText() logs one line per "unexpected" -- i.e. any
 * non-\n/\t/\\/\" -- character) are stubbed rather than linking the real
 * lprintf.c, since this test doesn't care about logging output and
 * lprintf.c pulls in custom.h/fileio.h/paths.h/common-ui.h's own heavier
 * dependency graph for no benefit here. */

/* message/AbortMessage/AbortProg are real-defined in misc.c (linked in
 * below) -- do NOT stub them here too, or the linker sees duplicate
 * definitions. Only stub what misc.c/getargs.c need that ISN'T already
 * provided by misc.c itself. */
drawCmd_t mainD;

FILE *paramFile = NULL;
char *paramFileName = NULL;
wIndex_t paramLineNum = 0;
char paramLine[STR_HUGE_SIZE];

/* misc.h's logTable(N) macro expands to DYNARR_N(logTable_t, logTable_da, N)
 * -- referenced (though never actually dereferenced at runtime here, since
 * LogFindIndex() below always returns -1) by the LOG() macro's condition
 * `(DBINX) > 0 && logTable((DBINX)).level >= (DBLVL)`; needs a real, linkable
 * definition regardless, since that's a runtime short-circuit, not a
 * compile-time-eliminable one. */
dynArr_t logTable_da;

int LogFindIndex(const char *name)
{
	(void)name;
	return -1;
}

void LogPrintf(const char *format, ...)
{
	(void)format;
}

int wNoticeWithIcon(int type, const char *msg, const char *yes, const char *no)
{
	(void)type;
	(void)msg;
	(void)yes;
	(void)no;
	return 0;
}

/* Referenced by misc.c's real AbortProg() (its crash-recovery
 * "try to save before terminating" path) -- never actually called in these
 * tests (no CHECK() failure is expected in the happy-path round trips
 * below), just needs to be linkable. */
void DoSaveAs(void *doAfterSaveVP)
{
	(void)doAfterSaveVP;
}

/* ---------------------------------------------------------------------
 * Round-trip: ConvertToEscapedText() (write side) -> GetArgs("qc", ...)
 * (read side, exercises both the quote-doubling loop in getargs.c and the
 * real ConvertFromEscapedText() backslash-unescape it delegates to).
 * ------------------------------------------------------------------- */

static void RoundTripPlainText(void **state)
{
	(void)state;
	const char *original = "hello world";
	char *escaped = ConvertToEscapedText(original);
	char line[512];
	snprintf(line, sizeof line, "\"%s\"", escaped);
	MyFree(escaped);

	char *decoded = NULL;
	char *cp = NULL;
	assert_true(GetArgs(line, "qc", &decoded, &cp));
	assert_string_equal(decoded, original);
	MyFree(decoded);
}

static void RoundTripEmbeddedQuotes(void **state)
{
	(void)state;
	/* The exact shape a JSON object produces: many embedded double quotes. */
	const char *original = "{\"kind\": \"station\", \"id\": \"WP\"}";
	char *escaped = ConvertToEscapedText(original);
	char line[512];
	snprintf(line, sizeof line, "\"%s\"", escaped);
	MyFree(escaped);

	char *decoded = NULL;
	char *cp = NULL;
	assert_true(GetArgs(line, "qc", &decoded, &cp));
	assert_string_equal(decoded, original);
	MyFree(decoded);
}

static void RoundTripBackslashesAndControlChars(void **state)
{
	(void)state;
	/* A JSON string value can itself contain an escaped backslash or
	 * newline (two literal ASCII chars in the JSON text, e.g. \n meaning
	 * backslash+n) -- these are ordinary characters as far as XTrkCAD's
	 * own escaping is concerned, but a literal raw newline/tab/backslash
	 * can also appear if a JSON Note's text were pretty-printed (cJSON_Print
	 * inserts real newlines). Cover both. */
	const char *original = "line one\nline two\ttabbed\\backslash\"quoted\"";
	char *escaped = ConvertToEscapedText(original);
	char line[512];
	snprintf(line, sizeof line, "\"%s\"", escaped);
	MyFree(escaped);

	char *decoded = NULL;
	char *cp = NULL;
	assert_true(GetArgs(line, "qc", &decoded, &cp));
	assert_string_equal(decoded, original);
	MyFree(decoded);
}

static void RoundTripEmptyText(void **state)
{
	(void)state;
	const char *original = "";
	char *escaped = ConvertToEscapedText(original);
	char line[512];
	snprintf(line, sizeof line, "\"%s\"", escaped);
	MyFree(escaped);

	char *decoded = NULL;
	char *cp = NULL;
	assert_true(GetArgs(line, "qc", &decoded, &cp));
	assert_string_equal(decoded, original);
	MyFree(decoded);
}

/* ---------------------------------------------------------------------
 * Regression guard for the MCP-side bug this round-trip is paired with
 * (mcp/src/xtrkcad_mcp/parser.py's naive str.find('"') scanning, fixed
 * alongside this test): confirms the *native* format two adjacent
 * quoted fields (op 1/2's url+title, or path+title) still parse correctly
 * when the first field contains embedded quotes -- this is the exact shape
 * a hand-crafted multi-field NOTE line takes, and a bug in field-boundary
 * detection would show up here as the second field's content leaking in.
 * ------------------------------------------------------------------- */

static void RoundTripTwoQuotedFieldsWithEmbeddedQuotes(void **state)
{
	(void)state;
	/* Matches trknote.c's ReadTrackNote() OP_NOTELINK/OP_NOTEFILE shape
	 * exactly: two separate "qc" calls chained via the cursor the first
	 * call's 'c' format code advances -- not a single combined format. */
	const char *first = "{\"a\":1}";
	const char *second = "a title";
	char *escapedFirst = ConvertToEscapedText(first);
	char *escapedSecond = ConvertToEscapedText(second);
	char line[512];
	snprintf(line, sizeof line, "\"%s\" \"%s\"", escapedFirst, escapedSecond);
	MyFree(escapedFirst);
	MyFree(escapedSecond);

	char *decodedFirst = NULL;
	char *decodedSecond = NULL;
	char *cp = NULL;
	assert_true(GetArgs(line, "qc", &decodedFirst, &cp));
	assert_string_equal(decodedFirst, first);
	assert_true(GetArgs(cp, "qc", &decodedSecond, &cp));
	assert_string_equal(decodedSecond, second);
	MyFree(decodedFirst);
	MyFree(decodedSecond);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(RoundTripPlainText),
		cmocka_unit_test(RoundTripEmbeddedQuotes),
		cmocka_unit_test(RoundTripBackslashesAndControlChars),
		cmocka_unit_test(RoundTripEmptyText),
		cmocka_unit_test(RoundTripTwoQuotedFieldsWithEmbeddedQuotes),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
