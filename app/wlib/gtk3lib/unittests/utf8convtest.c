/** \file utf8convtest.c
 * Unit tests for the Windows code page conversions (win32/utf8conv.c):
 * wIsUTF8(), wSystemToUTF8() and wUTF8ToSystem() (Development Issues #92).
 */

/*  XTrkCad - Model Railroad CAD
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

// The conversions are compiled into the test. They call the Windows API, so
// the test only runs on Windows.
#include "../win32/utf8conv.c"

// Byte strings, so this file stays ASCII
static const char aUmlautUtf8[] = "\xC3\xA4";		// a-umlaut
static const char aUmlautCp1252[] = "\xE4";
static const char euroUtf8[] = "\xE2\x82\xAC";
static const char euroCp1252[] = "\x80";
static const char sharpSUtf8[] = "\xC3\x9F";
static const char sharpSCp1252[] = "\xDF";
static const char lStrokeUtf8[] = "\xC5\x81";		// not in cp1252

/**
 * The conversion cases expect the system code page to be cp1252, as on the
 * CI runners. The test program has no manifest, so the code page isn't
 * UTF-8 here even where the application's is.
 */
static void
SkipUnlessCp1252(void)
{
	if (GetACP() != 1252) {
		print_message("system code page is %u, not 1252: skipped\n", GetACP());
		skip();
	}
}

static void
IsUTF8Valid(void **state)
{
	(void)state;
	assert_true(wIsUTF8(""));
	assert_true(wIsUTF8("Lok 01"));
	assert_true(wIsUTF8(aUmlautUtf8));
	assert_true(wIsUTF8(euroUtf8));
	assert_true(wIsUTF8("\xF4\x8F\xBF\xBF"));	// U+10FFFF
	// Every ASCII byte is UTF-8, control characters and DEL too
	assert_true(wIsUTF8("form\x0C" "feed"));
	assert_true(wIsUTF8("del\x7F"));
	assert_true(wIsUTF8("\x01\x1B\x1F"));
}

static void
IsUTF8Invalid(void **state)
{
	(void)state;
	assert_false(wIsUTF8(NULL));
	assert_false(wIsUTF8(aUmlautCp1252));
	assert_false(wIsUTF8(euroCp1252));
	assert_false(wIsUTF8("\xE2\x82"));		// truncated
	assert_false(wIsUTF8("\xC0\xAF"));		// overlong
	assert_false(wIsUTF8("\xED\xA0\x80"));		// surrogate
	assert_false(wIsUTF8("\xF4\x90\x80\x80"));	// above U+10FFFF
}

static void
SystemToUTF8Converts(void **state)
{
	char out[16];

	(void)state;
	SkipUnlessCp1252();
	assert_true(wSystemToUTF8(aUmlautCp1252, out, sizeof out));
	assert_string_equal(out, aUmlautUtf8);
	assert_true(wSystemToUTF8(euroCp1252, out, sizeof out));
	assert_string_equal(out, euroUtf8);
	assert_true(wSystemToUTF8(sharpSCp1252, out, sizeof out));
	assert_string_equal(out, sharpSUtf8);
	assert_true(wSystemToUTF8("abc", out, sizeof out));
	assert_string_equal(out, "abc");
}

static void
SystemToUTF8BufferSize(void **state)
{
	char in[1001];
	char out[3001];
	char small[3];

	(void)state;
	SkipUnlessCp1252();
	// 1000 euro signs: 3000 bytes and the terminator
	memset(in, euroCp1252[0], 1000);
	in[1000] = '\0';
	assert_true(wSystemToUTF8(in, out, sizeof out));
	assert_int_equal(strlen(out), 3000);
	assert_false(wSystemToUTF8(in, out, sizeof out - 1));

	// Exactly enough room, then one byte too little
	assert_true(wSystemToUTF8(aUmlautCp1252, small, 3));
	assert_string_equal(small, aUmlautUtf8);
	memset(small, 'x', sizeof small);
	assert_false(wSystemToUTF8(aUmlautCp1252, small, 2));
	// A failed conversion leaves an empty string, never one without its end
	assert_int_equal(small[0], '\0');
}

static void
UTF8ToSystemConverts(void **state)
{
	char out[16];

	(void)state;
	SkipUnlessCp1252();
	assert_true(wUTF8ToSystem(aUmlautUtf8, out, sizeof out));
	assert_string_equal(out, aUmlautCp1252);
	assert_true(wUTF8ToSystem(euroUtf8, out, sizeof out));
	assert_string_equal(out, euroCp1252);
	assert_true(wUTF8ToSystem("abc", out, sizeof out));
	assert_string_equal(out, "abc");
}

static void
UTF8ToSystemBufferSize(void **state)
{
	char out[2];

	(void)state;
	SkipUnlessCp1252();
	// One byte and the terminator: exactly enough room
	assert_true(wUTF8ToSystem(aUmlautUtf8, out, 2));
	assert_string_equal(out, aUmlautCp1252);
	memset(out, 'x', sizeof out);
	assert_false(wUTF8ToSystem(aUmlautUtf8, out, 1));
	assert_int_equal(out[0], '\0');
}

static void
UTF8ToSystemReportsLoss(void **state)
{
	char out[16];

	(void)state;
	SkipUnlessCp1252();
	// No L-stroke in cp1252: that's a failure, not an L or a '?'
	assert_false(wUTF8ToSystem(lStrokeUtf8, out, sizeof out));
	assert_false(wUTF8ToSystem("\xC5\x81" "odz", out, sizeof out));
	// Not UTF-8 at all
	assert_false(wUTF8ToSystem(aUmlautCp1252, out, sizeof out));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(IsUTF8Valid),
		cmocka_unit_test(IsUTF8Invalid),
		cmocka_unit_test(SystemToUTF8Converts),
		cmocka_unit_test(SystemToUTF8BufferSize),
		cmocka_unit_test(UTF8ToSystemConverts),
		cmocka_unit_test(UTF8ToSystemBufferSize),
		cmocka_unit_test(UTF8ToSystemReportsLoss),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
