/** \file DynStringTest.c
* Unit tests for the dynstring library
*/

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
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
    DynStringFree(&string);
    DynStringFree(&string2);
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

static void ResetString(void **state)
{
    DynString string;
    (void)state;
    DynStringMalloc(&string, 0);
    DynStringCatCStr(&string, TEXT1);
    assert_int_equal(DynStringSize(&string), strlen(TEXT1));
    DynStringReset(&string);
    assert_int_equal(DynStringSize(&string), 0);
    /* buffer still valid — can append again */
    DynStringCatCStr(&string, TEXT2);
    assert_int_equal(DynStringSize(&string), strlen(TEXT2));
    assert_string_equal(DynStringToCStr(&string), TEXT2);
    DynStringFree(&string);
}

static void CopyStr(void **state)
{
    DynString src, dest;
    (void)state;
    DynStringMalloc(&src, 0);
    DynStringMalloc(&dest, 0);
    DynStringCatCStr(&src, TEXT1);
    DynStringCpyStr(&dest, &src);
    assert_int_equal(DynStringSize(&dest), strlen(TEXT1));
    assert_string_equal(DynStringToCStr(&dest), TEXT1);
    /* overwrite dest with longer content */
    DynStringCatCStr(&src, TEXT2);
    DynStringCpyStr(&dest, &src);
    assert_int_equal(DynStringSize(&dest), strlen(TEXT1) + strlen(TEXT2));
    assert_string_equal(DynStringToCStr(&dest), TEXT1 TEXT2);
    DynStringFree(&src);
    DynStringFree(&dest);
}

static void CatStrsDyn(void **state)
{
    DynString s1, s2, s3;
    (void)state;
    DynStringMalloc(&s1, 0);
    DynStringMalloc(&s2, 0);
    DynStringMalloc(&s3, 0);
    DynStringCatCStr(&s1, TEXT1);
    DynStringCatCStr(&s2, TEXT2);
    DynStringCatCStr(&s3, TEXT3);
    DynStringCatStrs(&s1, &s2, &s3, NULL);
    assert_int_equal(DynStringSize(&s1),
                     strlen(TEXT1) + strlen(TEXT2) + strlen(TEXT3));
    assert_string_equal(DynStringToCStr(&s1), TEXT1 TEXT2 TEXT3);
    DynStringFree(&s1);
    DynStringFree(&s2);
    DynStringFree(&s3);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(SimpleInitialization),
        cmocka_unit_test(SingleString),
        cmocka_unit_test(MultipleStrings),
        cmocka_unit_test(VarStringCount),
        cmocka_unit_test(CopyString),
        cmocka_unit_test(PrintfString),
        cmocka_unit_test(ResetString),
        cmocka_unit_test(CopyStr),
        cmocka_unit_test(CatStrsDyn)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}