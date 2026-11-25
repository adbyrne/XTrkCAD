/** \file prereleasetest.c
 * Unit tests for the names that keep a pre-release apart from the release
 * (prerelease.c, Development Issues #94)
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
#include <cmocka.h>

#include <glib.h>

// compiled into the test, so it's tested on every platform
#include "../prerelease.c"

static void
CheckEnvVar(const char *appName, const char *expected)
{
	gchar *name = wlibLibDirEnvVar(appName);
	assert_string_equal(name, expected);
	g_free(name);
}

// A release, a development build, one with the hash, a pre-release
static void
LibDirEnvVar(void **state)
{
	(void)state;
	CheckEnvVar("xtrkcad", "XTRKCADLIB");
	CheckEnvVar("xtrkcad-5.4.0", "XTRKCAD_5_4_0LIB");
	CheckEnvVar("xtrkcad-5.4.0.8b67f185", "XTRKCAD_5_4_0_8B67F185LIB");
	CheckEnvVar("xtrkcad-5.4.0Beta1", "XTRKCAD_5_4_0BETA1LIB");
}

// Anything but a letter or digit becomes '_'
static void
LibDirEnvVarCharacters(void **state)
{
	(void)state;
	CheckEnvVar("xtrkcad-5.4.0+rc~1 x", "XTRKCAD_5_4_0_RC_1_XLIB");
}

// A pre-release uses the shared name, whatever it's installed under
static void
WorkDirName(void **state)
{
	(void)state;
	assert_string_equal(wlibWorkDirName("xtrkcad", NULL), "xtrkcad");
	assert_string_equal(wlibWorkDirName("xtrkcad-5.4.0", ""), "xtrkcad-5.4.0");
	assert_string_equal(wlibWorkDirName("xtrkcad-5.4.0Beta1",
	                                    "xtrkcad-prerelease"),
	                    "xtrkcad-prerelease");
	assert_string_equal(wlibWorkDirName("xtrkcad-5.4.0RC1",
	                                    "xtrkcad-prerelease"),
	                    "xtrkcad-prerelease");
}

int
main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(LibDirEnvVar),
		cmocka_unit_test(LibDirEnvVarCharacters),
		cmocka_unit_test(WorkDirName),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
