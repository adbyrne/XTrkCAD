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

/* getargs.c's 'q' GetArgs case calls this directly (not through misc.c)
 * when UTFCONVERT is defined -- Windows only (common.h). The real
 * implementation (utf8convert.c) round-trips through wlib's codepage
 * functions, which this test has no need to link just to prove the
 * quote-doubling escape logic under test here; a no-op is behaviorally
 * fine since every fixture string below is already plain ASCII/UTF-8. */
void ConvertUTF8ToSystem(unsigned char *in)
{
	(void)in;
}

/* wMain() itself is excluded from this target's compilation of misc.c
 * (XTRKCAD_TESTBUILD_NO_WMAIN, see ../unittest/CMakeLists.txt) since it's
 * the whole application's entry point and has nothing to do with
 * ConvertToEscapedText()/ConvertFromEscapedText(), the only two functions
 * this test actually needs from misc.c. That alone isn't sufficient on
 * Windows/MSYS2, though: several *other* exported misc.c functions
 * (SetAccelKeys/OfferCheckpoint/InitAudio/wShow/wHide/DoShowWindow/
 * DefaultProc/AccelKeyDispatch/DoClearAfter, all normally only reachable
 * from wMain(), plus their own transitive callees) still linked in and
 * pulled their real dependencies along -- confirmed via the actual MSYS2
 * CI job log that --gc-sections does not reliably discard globally-linked
 * (non-static) functions from misc.c on this platform/toolchain, unlike
 * Linux and macOS. Below are link-only stubs (exact prototypes copied from
 * their real headers, so this compiles against the same declarations
 * misc.c itself sees) for every symbol that surfaced as undefined this
 * way -- none of them are ever actually invoked in these tests, since none
 * of the now-dead misc.c call paths above run. */
wControl_p aboutW = NULL;
wBool_t bReadOnly = FALSE;
wIndex_t changed = 0;
wIndex_t checkPtMark = 0;
wControl_p demoW = NULL;
long enableAudio = 0;
BOOL_T inError = FALSE;
wBool_t inPlayback = FALSE;
wControl_p mapW = NULL;
wControl_p winList_mi = NULL;

void CleanupCheckpointFiles(void) { }
void CleanupTempArchive(void) { }
void ClearTracks(void) { }
int ConfirmReset(BOOL_T b) { (void)b; return 0; }
void DoLayout(void *unused) { (void)unused; }
void DoSave(void *doAfterSaveVP) { (void)doAfterSaveVP; }
void DoZoomDown(const void *modeVP) { (void)modeVP; }
void DoZoomUp(const void *modeVP) { (void)modeVP; }
void EditCopy(void *unused) { (void)unused; }
void EditCut(void *unused) { (void)unused; }
void EditPaste(void *unused) { (void)unused; }
void EnableCommands(void) { }
void FormResetInvalid(wControl_p win) { (void)win; }
void InfoDefaultControls(void) { }
void LayoutBackGroundInit(BOOL_T clear) { (void)clear; }
int LoadCheckpoint(BOOL_T b) { (void)b; return 0; }
void LogClose(void) { }
void MainRedraw(void) { }
void MapWindowShow(int state) { (void)state; }
void MessageListAppend(const char *a, const char *b) { (void)a; (void)b; }
void Reset(void) { }
void ResetLayers(void) { }
void SaveState(void) { }
void SetLayoutFullPath(const char *fileName) { (void)fileName; }
void SetMessage(char *infotext) { (void)infotext; }
void SetWindowTitle(void) { }
void TrySelectDelete(void) { }
void UndoUndo(void *unused) { (void)unused; }
void wAttachAccelKey(wAccelKey_e k, int i, wAccelKeyCallBack_p cb, void *d)
{
	(void)k; (void)i; (void)cb; (void)d;
}
void wBeep(void) { }
void wDoAccelHelp(wAccelKey_e key, void *d) { (void)key; (void)d; }
void wExit(int code) { (void)code; }
const char *wGetAppLibDir(void) { return NULL; }
void wMenuListAdd(wControl_p ml, int index, const char *labelStr,
		   const void *attributes)
{
	(void)ml; (void)index; (void)labelStr; (void)attributes;
}
void wMenuListDelete(wControl_p ml, const char *labelStr)
{
	(void)ml; (void)labelStr;
}
int wNotice(const char *msg, const char *yes, const char *no)
{
	(void)msg; (void)yes; (void)no;
	return 0;
}
int wNotice3(const char *msg, const char *affirmative, const char *cancel,
	     const char *alternate)
{
	(void)msg; (void)affirmative; (void)cancel; (void)alternate;
	return 0;
}
wBool_t wPrefGetInteger(const char *section, const char *name, long *result,
			 long defaultValue)
{
	(void)section; (void)name;
	if (result) {
		*result = defaultValue;
	}
	return FALSE;
}
char *wPrefGetString(const char *section, const char *name)
{
	(void)section; (void)name;
	return NULL;
}
void wSetAudio(bool setting) { (void)setting; }
const char *wWinGetTitle(wControl_p window) { (void)window; return NULL; }
void wWinSetBusy(wControl_p win, wBool_t busy) { (void)win; (void)busy; }
void wWinShow(wControl_p control, wBool_t visibility)
{
	(void)control; (void)visibility;
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
