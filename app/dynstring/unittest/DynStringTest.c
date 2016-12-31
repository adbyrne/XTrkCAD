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
    DynString *string;
    DynString string2 = NaS;
    (void)state;
    string = DynstrMalloc(0);
    DynstrPrintf(string, "%d", 1);
    assert_string_equal(DynstrToCStr(string), "1");
    DynstrFree(string);
}

static void CopyString(void **state)
{
    DynString *string;
    DynString *string2;
    (void)state;
    string = DynstrMalloc(0);
    DynstrCatCStr(string, TEXT1);
    string2 = DynstrDupStr(string);
    assert_int_equal(DynstrSize(string2), strlen(TEXT1));
    assert_string_equal(DynstrToCStr(string2), TEXT1);
    DynstrFree(string2);
    string2 = DynstrMalloc(0);
    DynstrCatCStr(string2, TEXT2);
    DynstrCatStr(string, string2);
    assert_int_equal(DynstrSize(string), strlen(TEXT1) + strlen(TEXT2));
    assert_string_equal(DynstrToCStr(string), TEXT1 TEXT2);
}

static void VarStringCount(void **state)
{
    DynString *string;
    (void)state;
    string = DynstrMalloc(0);
    DynstrCatCStrs(string, TEXT1, TEXT2, TEXT3, NULL);
    assert_int_equal(DynstrSize(string),
                     strlen(TEXT1) + strlen(TEXT2) + strlen(TEXT3));
    assert_string_equal(DynstrToCStr(string), TEXT1 TEXT2 TEXT3);
    DynstrFree(string);
}

static void MultipleStrings(void **state)
{
    DynString *string;
    (void)state;
    string = DynstrMalloc(0);
    DynstrCatCStr(string, TEXT1);
    DynstrCatCStr(string, TEXT2);
    assert_int_equal(DynstrSize(string), strlen(TEXT1)+strlen(TEXT2));
    assert_string_equal(DynstrToCStr(string), TEXT1 TEXT2);
    DynstrFree(string);
}

static void SingleString(void **state)
{
    DynString *string;
    (void)state;
    string = DynstrMalloc(0);
    DynstrCatCStr(string, TEXT1);
    assert_int_equal(DynstrSize(string), strlen(TEXT1));
    assert_string_equal(DynstrToCStr(string), TEXT1);
    DynstrFree(string);
}

static void SimpleInitialization(void **state)
{
    DynString *string;
    (void)state;
    string = DynstrMalloc(0);
    assert_non_null((void *)string);
    assert_false(isnas(string));
    assert_int_equal(DynstrSize(string), 0);
    DynstrFree(string);
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