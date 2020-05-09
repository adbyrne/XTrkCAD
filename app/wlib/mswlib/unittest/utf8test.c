/** \file utf8test.c
* Unit tests for utf 8 conversion routines on Windows
*/

#include <setjmp.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include <wlib.h>

#define SIMPLEASCIITEXT "The quick brown fox jumps over the lazy dog."
#define UMLAUTTEXT "‰ˆ¸ƒ÷‹ﬂ"

static void
ASCIIText(void **state)
{
	char output[100];
	char result[100];
	bool success;
	(void)state;

	success = wSystemToUTF8(SIMPLEASCIITEXT, output, 100);
	assert_true((void *)success);

	success = wUTF8ToSystem(output, result, 100);
	assert_true((void *)success);

	assert_false(strcmp(SIMPLEASCIITEXT, result));
}

static void
Umlauts(void **state)
{
	char output[100];
	char result[100];
	bool success;
	(void)state;

	success = wIsUTF8(UMLAUTTEXT);
	assert_false((void *)success);

	success = wSystemToUTF8(UMLAUTTEXT, output, 100);
	assert_true((void *)success);

	success = wIsUTF8(output);
	assert_true((void *)success);

	success = wUTF8ToSystem(output, result, 100);
	assert_true((void *)success);

	assert_false(strcmp(UMLAUTTEXT, result));
}


int main(void)
{
    const struct CMUnitTest tests[] = {
		cmocka_unit_test(ASCIIText),
		cmocka_unit_test(Umlauts),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}