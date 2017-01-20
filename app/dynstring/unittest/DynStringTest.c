/** \file DynStringTest.c
* Unit tests for the dynstring library
*/

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>

#include "../dynstring.h"

#define TEXT1 "Pastry gummi bears candy canes jelly beans macaroon choc"
#define TEXT2 "olate jelly beans. Marshmallow cupcake tart jelly apple pie sesame snaps ju"
#define TEXT3 "jubes. Tootsie roll dessert gummi bears jelly."

static void PrintfString(void **state)
{
    DynString string;

    (void)state;
    DynStringMalloc(&string, 0);
    DynStringPrintf(&string, "%d", 1);
    assert_string_equal(DynStringToCStr(&string), "1");
    DynStringFree(&string);
}

static void CopyString(void **state)
{
    DynString string;
    DynString string2;
    (void)state;
    DynStringMalloc(&string, 0);
    DynStringCatCStr(&string, TEXT1);
    DynStringDupStr(&string2, &string);
    assert_int_equal(DynStringSize(&string2), strlen(TEXT1));
    assert_string_equal(DynStringToCStr(&string2), TEXT1);
    DynStringFree(&string2);
    DynStringMalloc(&string2, 0);
    DynStringCatCStr(&string2, TEXT2);
    DynStringCatStr(&string, &string2);
    assert_int_equal(DynStringSize(&string), strlen(TEXT1) + strlen(TEXT2));
    assert_string_equal(DynStringToCStr(&string), TEXT1 TEXT2);
}

static void VarStringCount(void **state)
{
    DynString string;
    (void)state;
    DynStringMalloc(&string, 0);
    DynStringCatCStrs(&string, TEXT1, TEXT2, TEXT3, NULL);
    assert_int_equal(DynStringSize(&string),
                     strlen(TEXT1) + strlen(TEXT2) + strlen(TEXT3));
    assert_string_equal(DynStringToCStr(&string), TEXT1 TEXT2 TEXT3);
    DynStringFree(&string);
}

static void MultipleStrings(void **state)
{
    DynString string;
    (void)state;
    DynStringMalloc(&string, 0);
    DynStringCatCStr(&string, TEXT1);
    DynStringCatCStr(&string, TEXT2);
    assert_int_equal(DynStringSize(&string), strlen(TEXT1)+strlen(TEXT2));
    assert_string_equal(DynStringToCStr(&string), TEXT1 TEXT2);
    DynStringFree(&string);
}

static void SingleString(void **state)
{
    DynString string;
    (void)state;
    DynStringMalloc(&string, 0);
    DynStringCatCStr(&string, TEXT1);
    assert_int_equal(DynStringSize(&string), strlen(TEXT1));
    assert_string_equal(DynStringToCStr(&string), TEXT1);

	DynStringClear(&string);
	assert_int_equal(DynStringSize(&string), 0);

    DynStringFree(&string);
}

static void SimpleInitialization(void **state)
{
    DynString string;
    (void)state;
    DynStringMalloc(&string, 0);
    assert_non_null((void *)&string);
    assert_false(isnas(&string));
    assert_int_equal(DynStringSize(&string), 0);
    DynStringFree(&string);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(SimpleInitialization),
        cmocka_unit_test(SingleString),
        cmocka_unit_test(MultipleStrings),
        cmocka_unit_test(VarStringCount),
        cmocka_unit_test(CopyString),
        cmocka_unit_test(PrintfString)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}