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
CatalogEntry *catalog;


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

AbortProg()
{

}

static void
CreateLib(void **state)
{
	(void)state;
	trackLib = CreateLibrary(".");
	assert_non_null((void *)trackLib);
}

static void ScanEmptyDir(void **state)
{
	(void)state;
	bool success;
	EmptyCatalog(trackLib->catalog);
	success = GetTrackFiles(trackLib, "//");
	assert_false(success);
}

static void ScanTestFiles(void **state)
{
	(void)state;
	bool success;
	EmptyCatalog(trackLib->catalog);
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
	unsigned int files = SearchLibrary(trackLib, "djfhdkljhf", catalog);
	assert_true(files == 0);
	EmptyCatalog(catalog);
}

static void SearchSingle(void **state)
{
	(void)state;
	int files = SearchLibrary(trackLib, "peco", catalog );
	assert_true(files==1);
	EmptyCatalog(catalog);
}

static void SearchMultiple(void **state)
{
	(void)state;
	int files = SearchLibrary(trackLib, "atlas", catalog);
	assert_true(files == 2);
	EmptyCatalog(catalog);
}

static void FilterTest(void **state)
{
	(void)state;
	bool res;

	res = FilterKeyword("test");
	assert_false(res);
	assert_true(FilterKeyword("&"));
	assert_false(FilterKeyword("n"));
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
		cmocka_unit_test(FilterTest),
    };

	catalog = InitCatalog();

    return cmocka_run_group_tests(tests, NULL, NULL);
}