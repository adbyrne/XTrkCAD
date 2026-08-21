/** \file carregistrytest.c
 * Unit tests for the car/loco catalog registries (Phase H, rolling-stock
 * dialog rework, see logseq implementation-plan-rolling-stock-dialog.md §9).
 *
 * Unlike carstest.c's copy-verbatim approach, this target links the real
 * cars/carproto.c, cars/carpart.c, cars/listelem.c, cars/tabstring.c, and
 * cars/transform_pts.c directly, so CarProtoNew/CarProtoFind/CarProtoLookup
 * and CarPartNew/CarPartFind (the auto-create-on-commit logic the whole
 * rework depends on) are exercised as real, not copied, code.
 *
 * CarProtoRead/CarProtoWrite/CarPartRead/CarPartWrite/CustMgmProc/Init
 * (the other functions in these two files) pull in real file I/O
 * (GetArgs/PutTitle/SetCLocale/AddParam), careditdlg.c (CarDlgUpdProto/
 * CarDlgUpdPart), and scale.c -- out of scope for this pass (see the plan
 * doc). This target is compiled with -ffunction-sections and linked with
 * --gc-sections so the linker *can* drop those never-called functions'
 * sections. That pruning is a linker/compiler-version-dependent optimization,
 * though, not a guarantee (confirmed: passes with local gcc 16/ld 2.46, fails
 * with CI's Ubuntu toolchain, leaving SetCLocale/PutTitle/WriteSegs/
 * SetUserLocale/GetScaleName/customMgmF/CarDlgUpdProto/CarDlgUpdPart/
 * carDlgUpdateProtoPtr/carDlgUpdatePartPtr undefined at link time) -- so the
 * symbols those never-called functions reference are also stubbed below,
 * same as the rest of this section, to make the link deterministic
 * regardless of toolchain.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <math.h>
#include <cmocka.h>

#include "common.h"
#include "cars/carsprivate.h"
#include "cars/listelem.h"
#include "fileio.h"   /* PARAM_CUSTOM/PARAM_LAYOUT/PARAM_DEMO */

#define NEARLY_EQ(a, b) (fabs((a) - (b)) < 1e-6)

/* -----------------------------------------------------------------------
 * Stubs -- symbols referenced from CarProtoNew/CarProtoFind/CarProtoLookup/
 * CarPartNew/CarPartFind (and their in-file helpers) but not exercised at
 * runtime by these tests. None of these bodies are ever actually invoked
 * by the test cases below; they only need to exist for the linker.
 * ----------------------------------------------------------------------- */

void *MyMalloc(size_t n) { return malloc(n); }
void *MyRealloc(void *p, size_t n) { return realloc(p, n); }
void MyFree(void *p) { free(p); }
void *memdup(void *p, size_t n)
{
	void *out;
	if (n == 0) { return NULL; }
	out = malloc(n);
	if (out && p) { memcpy(out, p, n); }
	return out;
}
char *MyStrdup(const char *s) { return s ? strdup(s) : NULL; }

void CloneFilledDraw(wIndex_t cnt, trkSeg_p segs, BOOL_T changeColor)
{
	(void)cnt; (void)segs; (void)changeColor;
}

void FreeFilledDraw(wIndex_t cnt, trkSeg_p segs)
{
	(void)cnt; (void)segs;
}

void GetSegBounds(coOrd orig, ANGLE_T angle, wIndex_t cnt, trkSeg_p segs,
                  coOrd *lo, coOrd *hi)
{
	(void)orig; (void)angle; (void)cnt; (void)segs;
	lo->x = lo->y = hi->x = hi->y = 0.0;
}

void LogPrintf(const char *fmt, ...) { (void)fmt; }
dynArr_t logTable_da;
char message[STR_HUGE_SIZE];
coOrd zero;
wDrawColor drawColorBlue;

void AbortProg(const char *scond, const char *file, int line, const char *msg)
{
	(void)scond; (void)file; (void)line; (void)msg;
	abort();
}

/* utility.c:53 -- Cmp_part's min(cmp_key->partnoL, part_elem->partnoL) */
double min(double a, double b) { return a < b ? a : b; }

/* Referenced only from CarProto/CarPartWrite/CustMgmProc/Init -- never
 * called by the test cases below; see the file header comment. */
void SetCLocale(void) {}
void SetUserLocale(void) {}
char *PutTitle(char *cp) { return cp; }
BOOL_T WriteSegs(FILE *f, wIndex_t segCnt, trkSeg_p segs)
{
	(void)f; (void)segCnt; (void)segs;
	return TRUE;
}
char *GetScaleName(SCALEINX_T scaleInx) { (void)scaleInx; return NULL; }
FILE *customMgmF;
carPart_p carDlgUpdatePartPtr;
carProto_p carDlgUpdateProtoPtr;
void CarDlgUpdPart(void) {}
void CarDlgUpdProto(void) {}

/* -----------------------------------------------------------------------
 * Fixture reset -- empties carProto_da/carPartParent_da/roadnameMap_da (and
 * frees the owned strings/segments/nested parts_da) between tests, per the
 * plan's §10.1.3 teardown convention.
 * ----------------------------------------------------------------------- */

extern dynArr_t roadnameMap_da;

static void ResetCarRegistries(void)
{
	int i, j;

	for (i = 0; i < carProto_da.cnt; i++) {
		carProto_p p = carProto(i);
		if (p->desc) { MyFree(p->desc); }
		if (p->segPtr) { MyFree(p->segPtr); }
		MyFree(p);
	}
	DYNARR_FREE(carProto_p, carProto_da);

	for (i = 0; i < carPartParent_da.cnt; i++) {
		carPartParent_p parentP = carPartParent(i);
		for (j = 0; j < parentP->parts_da.cnt; j++) {
			carPart_p partP = carPart(parentP, j);
			if (partP->title) { MyFree(partP->title); }
			MyFree(partP);
		}
		DYNARR_FREE(carPart_p, parentP->parts_da);
		if (parentP->manuf) { MyFree(parentP->manuf); }
		if (parentP->proto) { MyFree(parentP->proto); }
		MyFree(parentP);
	}
	DYNARR_FREE(carPartParent_p, carPartParent_da);

	for (i = 0; i < roadnameMap_da.cnt; i++) {
		roadnameMap_p r = DYNARR_N(roadnameMap_p, roadnameMap_da, i);
		if (r->roadname) { MyFree(r->roadname); }
		if (r->repmark) { MyFree(r->repmark); }
		MyFree(r);
	}
	DYNARR_FREE(roadnameMap_p, roadnameMap_da);

	carProtoListChanged = FALSE;
	carPartListChanged = FALSE;
	roadnameMapChanged = FALSE;
}

static int Teardown(void **state)
{
	(void)state;
	ResetCarRegistries();
	return 0;
}

/* -----------------------------------------------------------------------
 * CarProtoFind / CarProtoNew
 * ----------------------------------------------------------------------- */

static void CarProtoFindTests(void **state)
{
	(void)state;
	carDim_t dim = { 40.0, 9.0, 28.0, 0.0, 44.0 };
	carProto_p proto = CarProtoNew(NULL, PARAM_CUSTOM, "Gondola", 0, 30100,
	                                &dim, 0, NULL);

	assert_non_null(proto);
	assert_ptr_equal(CarProtoFind("Gondola"), proto);
	assert_ptr_equal(CarProtoFind("GONDOLA"), proto);
	assert_null(CarProtoFind("Tank Car"));
}

static void CarProtoNewCreatesTests(void **state)
{
	(void)state;
	carDim_t dim = { 40.0, 9.0, 28.0, 0.0, 44.0 };
	carProto_p proto = CarProtoNew(NULL, PARAM_CUSTOM, "40' Boxcar", 7, 30100,
	                                &dim, 0, NULL);

	assert_non_null(proto);
	assert_string_equal(proto->desc, "40' Boxcar");
	assert_int_equal(proto->options, 7);
	assert_int_equal(proto->type, 30100);
	assert_true(NEARLY_EQ(proto->dim.carLength, 40.0));
	assert_true(NEARLY_EQ(proto->dim.carWidth, 9.0));
}

static void CarProtoNewCustomProtectedFromReloadTests(void **state)
{
	(void)state;
	carDim_t customDim = { 40.0, 9.0, 28.0, 0.0, 44.0 };
	carDim_t stockDim = { 50.0, 10.0, 30.0, 0.0, 54.0 };

	carProto_p proto = CarProtoNew(NULL, PARAM_CUSTOM, "Shared Boxcar", 1,
	                                30100, &customDim, 0, NULL);
	assert_non_null(proto);
	assert_int_equal(proto->paramFileIndex, PARAM_CUSTOM);

	/* A stock-file reload of the same desc must not clobber the custom
	 * entry -- this is the exact guard the auto-create-on-commit path
	 * relies on to never overwrite a user's customization. */
	carProto_p reloaded = CarProtoNew(NULL, PARAM_LAYOUT, "Shared Boxcar", 99,
	                                   99999, &stockDim, 0, NULL);
	assert_ptr_equal(reloaded, proto);
	assert_int_equal(proto->paramFileIndex, PARAM_CUSTOM);
	assert_int_equal(proto->options, 1);
	assert_int_equal(proto->type, 30100);
	assert_true(NEARLY_EQ(proto->dim.carLength, 40.0));
}

static void CarProtoNewInPlaceEditPreservesIdentityTests(void **state)
{
	(void)state;
	carDim_t dim1 = { 40.0, 9.0, 28.0, 0.0, 44.0 };
	carDim_t dim2 = { 45.0, 9.5, 30.0, 0.0, 48.0 };

	carProto_p proto = CarProtoNew(NULL, PARAM_CUSTOM, "Editable Boxcar", 0,
	                                30100, &dim1, 0, NULL);
	carProto_p edited = CarProtoNew(NULL, PARAM_CUSTOM, "Editable Boxcar", 5,
	                                 30200, &dim2, 0, NULL);

	assert_ptr_equal(edited, proto);
	assert_int_equal(proto->options, 5);
	assert_int_equal(proto->type, 30200);
	assert_true(NEARLY_EQ(proto->dim.carLength, 45.0));
}

static void CarProtoLookupCreateMissingTests(void **state)
{
	(void)state;
	assert_null(CarProtoLookup("Ghost Proto", FALSE, FALSE, 40.0, 9.0));

	carProto_p proto = CarProtoLookup("Ghost Proto", TRUE, FALSE, 40.0, 9.0);
	assert_non_null(proto);
	assert_true(NEARLY_EQ(proto->dim.carLength, 40.0));
	assert_true(NEARLY_EQ(proto->dim.carWidth, 9.0));

	/* second lookup finds the just-created entry -- no duplicate, and the
	 * (unused-this-time) length/width args don't overwrite it */
	carProto_p again = CarProtoLookup("Ghost Proto", TRUE, FALSE, 999.0, 999.0);
	assert_ptr_equal(again, proto);
	assert_true(NEARLY_EQ(proto->dim.carLength, 40.0));
}

/* -----------------------------------------------------------------------
 * CarPartNew / CarPartFind
 * ----------------------------------------------------------------------- */

static void CarPartNewRejectsUnknownOrCustomManufTests(void **state)
{
	(void)state;
	carDim_t dim = { 40.0, 9.0, 28.0, 0.0, 44.0 };
	char titleUnknown[] = "Unknown\tBoxcar\tDesc\t123\t\t\t";
	char titleCustom[] = "Custom\tBoxcar\tDesc\t123\t\t\t";
	char titleNoProto[] = "Athearn\t\tDesc\t123\t\t\t";
	char titleNoPart[] = "Athearn\tBoxcar\tDesc\t\t\t\t";

	/* session-10 dead-end guard: these manufacturer/field combinations must
	 * never silently create a catalog part. */
	assert_null(CarPartNew(NULL, PARAM_CUSTOM, 0, titleUnknown, 0, 30100, &dim,
	                       0));
	assert_null(CarPartNew(NULL, PARAM_CUSTOM, 0, titleCustom, 0, 30100, &dim,
	                       0));
	assert_null(CarPartNew(NULL, PARAM_CUSTOM, 0, titleNoProto, 0, 30100, &dim,
	                       0));
	assert_null(CarPartNew(NULL, PARAM_CUSTOM, 0, titleNoPart, 0, 30100, &dim,
	                       0));
}

static void CarPartNewCreatesTests(void **state)
{
	(void)state;
	carDim_t dim = { 40.0, 9.0, 28.0, 0.0, 44.0 };
	char title[] = "Athearn\tBoxcar\t40' PS-1\t12345\tATSF\tATSF\t1234";

	carPart_p part = CarPartNew(NULL, PARAM_CUSTOM, 5, title, 7, 30100, &dim,
	                            0);

	assert_non_null(part);
	assert_int_equal(part->partnoL, 5);
	assert_memory_equal(part->partnoP, "12345", 5);
	assert_non_null(part->parent);
	assert_string_equal(part->parent->manuf, "Athearn");
	assert_string_equal(part->parent->proto, "Boxcar");
	assert_int_equal(part->parent->scale, 5);
}

static void CarPartFindTests(void **state)
{
	(void)state;
	carDim_t dim = { 40.0, 9.0, 28.0, 0.0, 44.0 };
	char title[] = "Athearn\tBoxcar\t40' PS-1\t12345\tATSF\tATSF\t1234";
	carPart_p part = CarPartNew(NULL, PARAM_CUSTOM, 5, title, 0, 30100, &dim,
	                            0);

	assert_ptr_equal(CarPartFind("Athearn", 7, "12345", 5, 5), part);
	assert_ptr_equal(CarPartFind("athearn", 7, "12345", 5, 5), part);
	assert_null(CarPartFind("Athearn", 7, "12345", 5, 6));   /* scale differs */
	assert_null(CarPartFind("Athearn", 7, "99999", 5, 5));   /* part# differs */
}

static void CarPartNewInPlaceEditPreservesIdentityTests(void **state)
{
	(void)state;
	carDim_t dim1 = { 40.0, 9.0, 28.0, 0.0, 44.0 };
	carDim_t dim2 = { 45.0, 9.5, 30.0, 0.0, 48.0 };
	char title1[] = "Athearn\tBoxcar\t40' PS-1\t12345\tATSF\tATSF\t1234";
	char title2[] = "Athearn\tBoxcar\tRenamed\t12345\tATSF\tATSF\t1234";

	carPart_p part = CarPartNew(NULL, PARAM_CUSTOM, 5, title1, 0, 30100,
	                            &dim1, 0);
	/* same parent (manuf/proto/scale) and same part# -> edited in place,
	 * per the mutate-not-replace fix so callers holding partP stay valid */
	carPart_p edited = CarPartNew(part, PARAM_CUSTOM, 5, title2, 3, 30200,
	                              &dim2, 0);

	assert_ptr_equal(edited, part);
	assert_int_equal(part->options, 3);
	assert_int_equal(part->type, 30200);
	assert_true(NEARLY_EQ(part->dim.carLength, 45.0));
}

static void CarPartNewMovedRelocatesTests(void **state)
{
	(void)state;
	carDim_t dim1 = { 40.0, 9.0, 28.0, 0.0, 44.0 };
	carDim_t dim2 = { 40.0, 9.0, 28.0, 0.0, 44.0 };
	char title1[] = "Athearn\tBoxcar\tDesc\t12345\t\t\t";
	char title2[] = "Kadee\tBoxcar\tDesc\t12345\t\t\t";

	carPart_p part = CarPartNew(NULL, PARAM_CUSTOM, 5, title1, 0, 30100,
	                            &dim1, 0);
	/* different manufacturer -> different carPartParent_t: unlink from the
	 * old parent and relocate, rather than editing in place */
	carPart_p moved = CarPartNew(part, PARAM_CUSTOM, 5, title2, 0, 30100,
	                             &dim2, 0);

	assert_non_null(moved);
	assert_string_equal(moved->parent->manuf, "Kadee");
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_teardown(CarProtoFindTests, Teardown),
		cmocka_unit_test_teardown(CarProtoNewCreatesTests, Teardown),
		cmocka_unit_test_teardown(CarProtoNewCustomProtectedFromReloadTests,
		                          Teardown),
		cmocka_unit_test_teardown(CarProtoNewInPlaceEditPreservesIdentityTests,
		                          Teardown),
		cmocka_unit_test_teardown(CarProtoLookupCreateMissingTests, Teardown),
		cmocka_unit_test_teardown(CarPartNewRejectsUnknownOrCustomManufTests,
		                          Teardown),
		cmocka_unit_test_teardown(CarPartNewCreatesTests, Teardown),
		cmocka_unit_test_teardown(CarPartFindTests, Teardown),
		cmocka_unit_test_teardown(CarPartNewInPlaceEditPreservesIdentityTests,
		                          Teardown),
		cmocka_unit_test_teardown(CarPartNewMovedRelocatesTests, Teardown),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
