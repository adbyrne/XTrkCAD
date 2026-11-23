/** \file convertinputtest.c
 * Unit tests for wlibConvertInput() (util.c), which turns text for GTK into
 * UTF-8 (Development Issues #92).
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
#include <stdlib.h>
#include <string.h>
#include <cmocka.h>

#include <gtk/gtk.h>
#include "../gtkint.h"

// Linking xtrkcad-wlib brings its main(), which starts GTK and calls
// wMain() below, so the test needs a display (label gtk-display).
long dontHideCursor;

// Byte strings, so this file stays ASCII

static void
Utf8Unchanged(void **state)
{
	const char text[] = "M\xC3\xBCller \xE2\x82\xAC";	// u-umlaut, euro

	(void)state;
	// UTF-8 comes back as it is, not copied
	assert_ptr_equal(wlibConvertInput(text), text);
	assert_ptr_equal(wlibConvertInput("ascii"), "ascii");
}

static void
Cp1252Converted(void **state)
{
	(void)state;
	// u-umlaut is the same in cp1252 and Latin-1
	assert_string_equal(wlibConvertInput("M\xFCller"), "M\xC3\xBCller");
	// 0x80-0x9F differ: euro sign and quotes in cp1252, controls in Latin-1
	assert_string_equal(wlibConvertInput("\x80"), "\xE2\x82\xAC");
	assert_string_equal(wlibConvertInput("\x84" "a\x93"),
	                    "\xE2\x80\x9E" "a\xE2\x80\x9C");
}

static void
UndefinedBytes(void **state)
{
	const char *out;

	(void)state;
	// 0x81 isn't defined in cp1252: the whole text is read as Latin-1
	out = wlibConvertInput("\x81\xE4");
	assert_true(g_utf8_validate(out, -1, NULL));
	assert_string_equal(out, "\xC2\x81\xC3\xA4");
}

static void
ResultStaysUntilNextCall(void **state)
{
	char *first;

	(void)state;
	first = g_strdup(wlibConvertInput("\xE4"));
	assert_string_equal(first, "\xC3\xA4");
	assert_string_equal(wlibConvertInput("\xF6"), "\xC3\xB6");
	g_free(first);
}

/* wlib's main() calls this for the version option */
void wPrintVersion(void)
{
}

wControl_p wMain(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(Utf8Unchanged),
		cmocka_unit_test(Cp1252Converted),
		cmocka_unit_test(UndefinedBytes),
		cmocka_unit_test(ResultStaysUntilNextCall),
	};

	(void)argc;
	(void)argv;
	// main() discards wMain()'s result, so exit with cmocka's (preftest.c)
	exit(cmocka_run_group_tests(tests, NULL, NULL));
}
