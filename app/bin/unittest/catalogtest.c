/** \file catalog.c
* Unit tests for part catalog management
*/

#include <malloc.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>

#include "../include/partcatalog.h"

TrackLibrary *trackLib;


// some dummy functions to suppress linking problems
wGetUserHomeDir()
{

}

wPrefSetString()
{

}

wPrefGetString()
{

}


static void
CreateLib(void **state)
{
	(void)state;
	CreateLibrary(&trackLib);
	assert_non_null((void *)trackLib);
}

static void ScanEmptyDir(void **state)
{
	(void)state;
	bool success;

	success = GetTrackFiles(trackLib, "//");
	assert_false(success);
}

static void ScanTestFiles(void **state)
{
	(void)state;
	bool success;

	success = GetTrackFiles(trackLib, ".");
	assert_true(success);
}

static void CreateIndex(void **state)
{
	(void)state;
	unsigned int words = CreateLibraryIndex(trackLib);
	assert_true(words > 0);
}

static void SearchNothing(void **state)
{
	(void)state;
	SearchResult *result = NULL;
	int files = SearchLibrary(trackLib, "djfhdkljhf", &result);
	assert_true(files == 0);
	FreeResults(&result);
}

static void SearchSingle(void **state)
{
	(void)state;
	SearchResult *result = NULL;
	int files = SearchLibrary(trackLib, "peco", &result );
	assert_true(files==1);
	FreeResults(&result);
}

static void SearchMultiple(void **state)
{
	(void)state;
	SearchResult *result = NULL;
	int files = SearchLibrary(trackLib, "atlas", &result);
	assert_true(files == 2);
	FreeResults(&result);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
		cmocka_unit_test(CreateLib),
		cmocka_unit_test(ScanEmptyDir),
		cmocka_unit_test(ScanTestFiles),
		cmocka_unit_test(CreateIndex),
		cmocka_unit_test(SearchNothing),
		cmocka_unit_test(SearchSingle),
		cmocka_unit_test(SearchMultiple),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}