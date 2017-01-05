/** \file DynStringTest.c
* Unit tests for the dxfformat module
*/

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>

#include <dynstring.h>
#include <dxfformat.h>

char *sProdNameUpper = "XTRKCAD";

static void BasicFormatting(void **state)
{
	DynString string;
	(void)state;

	DynStringMalloc(&string, 0);

	DxfLayerName(&string, sProdNameUpper, 0);
	assert_string_equal(DynStringToCStr(&string), DXF_INDENT "8\nXTRKCAD0\n");

	DxfLayerName(&string, sProdNameUpper, 99);
	assert_string_equal(DynStringToCStr(&string), DXF_INDENT "8\nXTRKCAD99\n");

	DxfFormatPosition(&string, 20, 1);
	assert_string_equal(DynStringToCStr(&string), DXF_INDENT "20\n1.000000\n");

	DxfFormatPosition(&string, 20, 1.23456789);
	assert_string_equal(DynStringToCStr(&string), DXF_INDENT "20\n1.234568\n");

	DxfFormatPosition(&string, 20, 1.23456712);
	assert_string_equal(DynStringToCStr(&string), DXF_INDENT "20\n1.234567\n");
}

static void LineCommand(void **state)
{
	DynString string;
	(void)state;

	DynStringMalloc(&string, 0);

	DxfLineCommand( &string, 0, 1.0, 2.0, 1.1, 2.2, 1);
	assert_string_equal(DynStringToCStr(&string),
		DXF_INDENT "0\nLINE\n  8\nXTRKCAD0\n  10\n1.000000\n  20\n2.000000\n  11\n1.100000\n  21\n2.200000\n  6\nDASHED\n");

	DynStringFree(&string);
}


static void CircleCommand(void **state)
{
	DynString string;
	(void)state;

	DynStringMalloc(&string, 0);

	DxfCircleCommand(&string, 0, 1.0, 2.0, 1.1, 1);
	assert_string_equal(DynStringToCStr(&string),
		DXF_INDENT "0\nCIRCLE\n  10\n1.000000\n  20\n2.000000\n  40\n1.100000\n  8\nXTRKCAD0\n  6\nDASHED\n");

	DynStringFree(&string);
}


static void ArcCommand(void **state)
{
	DynString string;
	(void)state;

	DynStringMalloc(&string, 0);

	DxfArcCommand(&string, 0, 1.0, 2.0, 1.1, 10.0, 180.0, 1);
	assert_string_equal(DynStringToCStr(&string),
		DXF_INDENT "0\nARC\n  10\n1.000000\n  20\n2.000000\n  40\n1.100000\n  50\n10.000000\n  51\n190.000000\n  8\nXTRKCAD0\n  6\nDASHED\n");

	DynStringFree(&string);
}

#define TESTSTRING  "This is a dxf test string"

static void TextCommand(void **state)
{
	DynString string;
	(void)state;

	DynStringMalloc(&string, 0);

	DxfTextCommand(&string, 0, 10.0, 12.0, 144.0, TESTSTRING);

	assert_string_equal(DynStringToCStr(&string),
		DXF_INDENT "0\nTEXT\n  1\n" TESTSTRING "\n  10\n10.000000\n  20\n12.000000\n  40\n2.000000\n  8\nXTRKCAD0\n");

	DynStringFree(&string);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
		cmocka_unit_test(BasicFormatting),
		cmocka_unit_test(LineCommand),
		cmocka_unit_test(CircleCommand),
		cmocka_unit_test(ArcCommand),
		cmocka_unit_test(TextCommand)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}