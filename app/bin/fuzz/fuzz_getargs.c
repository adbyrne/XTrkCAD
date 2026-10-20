/** \file fuzz_getargs.c
 * libFuzzer harness for GetArgs() (fileio.c), the format-string parser used
 * throughout the .xtc/.xtr/.xtp file readers to pull typed fields out of
 * untrusted file lines.
 *
 * GetArgs() itself is largely self-contained, but its error path calls
 * InputError() -> wNoticeWithIcon(), a *blocking modal GTK dialog* -- fatal
 * for a fuzzer, since the majority of random inputs are malformed and would
 * hit that path on the first iteration. There is no existing stub/mock
 * layer in this codebase for wlib UI calls (checked app/bin/unittest/ --
 * its "stub" comments are for geometry helper functions, not UI), so this
 * harness provides minimal non-blocking stubs for the small set of symbols
 * GetArgs()/InputError() actually need (discovered empirically via
 * `-ffunction-sections -fdata-sections` + `--gc-sections`, which lets the
 * linker discard the rest of fileio.c's ~100-symbol dependency graph that
 * the full file would otherwise pull in).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "misc.h"
#include "draw.h"
#include "fileio.h"
#include "wlib.h"

/* ---- Minimal non-blocking stubs for GetArgs()/InputError()'s dependencies ---- */

char message[STR_HUGE_SIZE];
drawCmd_t mainD;

/* Normally allocated in fileio.c (declared extern in fileio.h); this
 * harness links getargs.c directly, without the rest of fileio.c. */
FILE *paramFile = NULL;
char *paramFileName = NULL;
wIndex_t paramLineNum = 0;
char paramLine[STR_HUGE_SIZE];

int wNoticeWithIcon(int type, const char *msg, const char *yes, const char *no)
{
	(void)type;
	(void)msg;
	(void)yes;
	(void)no;
	return 0;
}

char *ConvertFromEscapedText(const char *text)
{
	return strdup(text ? text : "");
}

const char *AbortMessage(const char *fmt, ...)
{
	(void)fmt;
	return "";
}

void AbortProg(const char *cond, const char *file, int line, const char *msg)
{
	/* Real AbortProg() shows a dialog and may abort()/exit() the process.
	 * A fuzz input reaching a "can't happen" condition is not itself a
	 * memory-safety bug -- must not terminate here, or every such input
	 * would register as a false "crash" to libFuzzer. */
	(void)cond;
	(void)file;
	(void)line;
	(void)msg;
}

/* ---- Target ---- */

extern BOOL_T GetArgs(char *line, const char *format, ...);

/* Representative format strings, taken verbatim from real call sites (not
 * invented), covering the numeric/coordinate, quoted-string, and (as of the
 * bounds-check fix) bounded-string format codes.
 *
 * The 's' format code used to lack any internal bounds check on the
 * destination it writes to, and an earlier version of this harness that
 * exercised it found a crash within seconds. Fixed with a mandatory inline
 * width in the format spec (e.g. "s9" for a 10-byte buffer, mirroring
 * scanf's "%9s") applied at every real call site -- variant 2 below
 * exercises that fixed path directly against a fixed-size buffer matching
 * the real call sites' declared size, so an out-of-bounds write here would
 * be a genuine regression.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size < 1) {
		return 0;
	}

	int variant = data[0] % 3;
	const uint8_t *lineData = data + 1;
	size_t lineSize = size - 1;

	char *line = malloc(lineSize + 1);
	if (!line) {
		return 0;
	}
	memcpy(line, lineData, lineSize);
	line[lineSize] = '\0';

	/* GetArgs() may return FALSE partway through the format string,
	 * leaving trailing output args (including the char* ones that own
	 * heap allocations) untouched -- NULL-initialize so the free() calls
	 * below are always safe regardless of how far parsing got. */
	switch (variant) {
	case 0: {
		long l0 = 0, l1 = 0;
		FLOAT_T w0 = 0, f0 = 0, f1 = 0;
		coOrd p0 = {0, 0}, p1 = {0, 0};
		GetArgs(line, "lwpfpfl", &l0, &w0, &p0, &f0, &p1, &f1, &l1);
		break;
	}
	case 1: {
		char *q0 = NULL, *c0 = NULL;
		GetArgs(line, "qc", &q0, &c0);
		free(q0);
		break;
	}
	case 2: {
		/* Matches the real call sites' "char scale[10]" + "s9". */
		char s0[10];
		GetArgs(line, "s9", s0);
		break;
	}
	default:
		// This fuzz harness links standalone against only getargs.c (see the file
		// header comment), not lprintf.c, so LOG()/LogFindIndex() aren't available
		// here. variant = data[0] % 3 makes this branch provably unreachable anyway.
		break;
	}

	free(line);
	return 0;
}
