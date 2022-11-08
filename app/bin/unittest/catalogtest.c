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

//#include "../common.h"
#include "../include/partcatalog.h"

ParameterLib *trackLib;
Catalog *catalog;
SearchResult* result;


// some dummy functions to suppress linking problems
const char *
wGetUserHomeDir()
{
	return("");
}

void wPrefSetString(const char* a, const char* b, const char* c)
{

}

char* wPrefGetString(const char* section, const char* name)
{
	return("");
}

void AbortProg(const char* a, const char* b, int c, const char*d)
{

}

void *
MyMalloc(size_t size)
{
	return(malloc(size));
}

void 
MyFree(void* memory)
{
	free(memory);
}

char *
MyStrdup(const char* string)
{
	return(strdup(string));
}

void *
MyRealloc(void *memory, size_t size)
{
	return(realloc(memory, size));
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
	DestroyCatalog(trackLib->catalog);
	success = CreateCatalogFromDir(trackLib, "//");
	assert_false(success);
}

static void ScanTestFiles(void **state)
{
	(void)state;
	bool success;
	DestroyCatalog(trackLib->catalog);
	success = CreateCatalogFromDir(trackLib, ".");
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
	unsigned int files = SearchLibrary(trackLib, "djfhdkljhf", result);
	assert_true(files == 0);
	SearchDiscardResult(result);
}

static void SearchSingle(void **state)
{
	(void)state;
	int files = SearchLibrary(trackLib, "peco", result );
	assert_true(files==1);
	SearchDiscardResult(result);
}

static void SearchMultiple(void **state)
{
	(void)state;
	int files = SearchLibrary(trackLib, "atlas", result);
	assert_true(files == 2);
	SearchDiscardResult(result);
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