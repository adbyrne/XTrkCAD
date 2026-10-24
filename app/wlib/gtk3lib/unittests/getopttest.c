/** \file getopttest.c
 * Unit tests for the getopt() replacement used on Windows (win32/getopt.c)
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

// The replacement is compiled into the test, so it's tested on every
// platform. It reads options starting with OPTCHAR: '/' on Windows, '-'
// elsewhere.
#include "../win32/getopt.c"

// misc.c's option string
#define OPTSTRING "vl:d:c:mVT::"

// More calls than any test needs: a getopt() that never returns EOF fails
// the test instead of hanging it.
#define MAXCALLS 10

static char optVersion[] = { OPTCHAR, 'V', '\0' };
static char optFlags[] = { OPTCHAR, 'v', 'm', '\0' };
static char optLog[] = { OPTCHAR, 'l', '\0' };
static char optTests[] = { OPTCHAR, 'T', 'd', 'm', 'i', 'n', 't', 'r', 'o', '\0' };
static char optDebug[] = { OPTCHAR, 'd', 'l', 'o', 'g', '=', '3', '\0' };

/**
 * Parse argv to the end. Records each option character in found and returns
 * the number of options, or -1 when getopt() doesn't finish.
 */
static int
ParseAll(int argc, char *argv[], char *found, char **lastarg)
{
	int n = 0;

	optind = 1;
	for (int i = 0; i < MAXCALLS; i++) {
		int c = getopt(argc, argv, OPTSTRING);
		if (c == EOF) {
			found[n] = '\0';
			return n;
		}
		found[n++] = (char)c;
		*lastarg = optarg;
	}
	return -1;
}

static void
SingleOption(void **state)
{
	(void)state;
	char *argv[] = { "xtrkcad", optVersion, NULL };
	char found[MAXCALLS + 1];
	char *arg = NULL;

	assert_int_equal(ParseAll(2, argv, found, &arg), 1);
	assert_string_equal(found, "V");
	assert_int_equal(optind, 2);
}

static void
CombinedFlags(void **state)
{
	(void)state;
	char *argv[] = { "xtrkcad", optFlags, NULL };
	char found[MAXCALLS + 1];
	char *arg = NULL;

	assert_int_equal(ParseAll(2, argv, found, &arg), 2);
	assert_string_equal(found, "vm");
	assert_int_equal(optind, 2);
}

static void
SeparateArgument(void **state)
{
	(void)state;
	char *argv[] = { "xtrkcad", optLog, "xtclog.txt", NULL };
	char found[MAXCALLS + 1];
	char *arg = NULL;

	assert_int_equal(ParseAll(3, argv, found, &arg), 1);
	assert_string_equal(found, "l");
	assert_string_equal(arg, "xtclog.txt");
	assert_int_equal(optind, 3);
}

// Development Issues #45: an attached argument returned the same option
// forever.
static void
AttachedArgument(void **state)
{
	(void)state;
	char *argv[] = { "xtrkcad", optTests, NULL };
	char found[MAXCALLS + 1];
	char *arg = NULL;

	assert_int_equal(ParseAll(2, argv, found, &arg), 1);
	assert_string_equal(found, "T");
	assert_string_equal(arg, "dmintro");
	assert_int_equal(optind, 2);
}

static void
AttachedArgumentThenFile(void **state)
{
	(void)state;
	char *argv[] = { "xtrkcad", optDebug, "layout.xtc", NULL };
	char found[MAXCALLS + 1];
	char *arg = NULL;

	assert_int_equal(ParseAll(3, argv, found, &arg), 1);
	assert_string_equal(found, "d");
	assert_string_equal(arg, "log=3");
	// the file name is left for the caller
	assert_int_equal(optind, 2);
	assert_string_equal(argv[optind], "layout.xtc");
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(SingleOption),
		cmocka_unit_test(CombinedFlags),
		cmocka_unit_test(SeparateArgument),
		cmocka_unit_test(AttachedArgument),
		cmocka_unit_test(AttachedArgumentThenFile),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
