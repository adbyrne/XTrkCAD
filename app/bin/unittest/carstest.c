/** \file carstest.c
 * Unit tests for pure/self-contained functions in app/bin/cars/ (Phase A of
 * the rolling-stock dialog rework, see logseq implementation-plan-rolling-
 * stock-dialog.md §10.3).
 *
 * cars/tabstring.c and cars/transform_pts.c are standalone (only depend on
 * utility.c's Rotate()) and are compiled in as real sources below. IsLocoType
 * (carproto.c), Cmp_part/Cmp_roadnameMap (carpart.c), and MapCondition/
 * OffsetInchesToFile/OffsetFileToInches (caritem.c) live in files coupled to
 * drawing/track/file-I/O code, so (matching the precedent in beziertest.c)
 * they are copied verbatim here with source-line anchors; update the copies
 * if the originals change.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <math.h>
#include <cmocka.h>

#include "common.h"
#include "cars/tabstring.h"
#include "../utility.c"    /* Rotate() */

void *MyMalloc(size_t n) { return malloc(n); }   /* cars/tabstring.c: TabStringDup */

/* cars/transform_pts.c prototypes (carsprivate.h) */
void RotatePts(int cnt, coOrd *pts, coOrd orig, ANGLE_T angle);
void RescalePts(int cnt, coOrd *pts, FLOAT_T scale_x, FLOAT_T scale_y);
void MovePts(int cnt, coOrd *pts, coOrd orig);

/* -----------------------------------------------------------------------
 * Functions copied from cars/carproto.c, cars/carpart.c, cars/caritem.c
 * ----------------------------------------------------------------------- */

/* carsprivate.h */
#define CAR_TYPE_LOCO_MIN (10000L)
#define CAR_TYPE_LOCO_MAX (19999L)

/* carproto.c:105 */
static BOOL_T IsLocoType(long type)
{
	return type >= CAR_TYPE_LOCO_MIN && type <= CAR_TYPE_LOCO_MAX;
}

/* carpart.c:53 — trimmed to the fields Cmp_part actually reads */
typedef struct {
	int paramFileIndex;
	char *partnoP;
	int partnoL;
} cmp_part_t;

#define PARAM_DEMO   -3
#define PARAM_CUSTOM -2
#define PARAM_LAYOUT -1

static int Cmp_part(cmp_part_t *cmp_key, cmp_part_t *part_elem)
{
	int rc;
	int len;

	len = (cmp_key->partnoL < part_elem->partnoL) ? cmp_key->partnoL :
	      part_elem->partnoL;
	rc = strncasecmp(cmp_key->partnoP, part_elem->partnoP, len + 1);
	if (rc != 0) {
		return rc;
	}
	if (cmp_key->paramFileIndex == part_elem->paramFileIndex) {
		return 0;
	}
	if (cmp_key->paramFileIndex == PARAM_DEMO) {
		return -1;
	}
	if (part_elem->paramFileIndex == PARAM_DEMO) {
		return 1;
	}
	if (cmp_key->paramFileIndex == PARAM_CUSTOM) {
		return -1;
	}
	if (part_elem->paramFileIndex == PARAM_CUSTOM) {
		return 1;
	}
	if (cmp_key->paramFileIndex == PARAM_LAYOUT) {
		return 1;
	}
	if (part_elem->paramFileIndex == PARAM_LAYOUT) {
		return -1;
	}
	if (cmp_key->paramFileIndex > part_elem->paramFileIndex) {
		return -1;
	} else {
		return 1;
	}
}

/* carpart.c:116 */
typedef struct {
	char *name;
	int len;
} cmp_key_t;

typedef struct {
	char *roadname;
	char *repmark;
} roadnameMap_t;

static int Cmp_roadnameMap(cmp_key_t *cmp_key, roadnameMap_t *roadname_elem)
{
	int rc;

	rc = strncasecmp(cmp_key->name, roadname_elem->roadname, cmp_key->len);
	if (rc == 0 && roadname_elem->roadname[cmp_key->len]) {
		return -1;
	}
	return rc;
}

/* caritem.c:47 */
static long MapCondition(long conditionValue)
{
	if (conditionValue < 10) {
		return 0;
	} else if (conditionValue < 30) {
		return 5;
	} else if (conditionValue < 50) {
		return 4;
	} else if (conditionValue < 70) {
		return 3;
	} else if (conditionValue < 90) {
		return 2;
	} else {
		return 1;
	}
}

/* caritem.c: OffsetInchesToFile/OffsetFileToInches */
static long OffsetInchesToFile(double inches)
{
	return (long)(inches * 1000);
}

static double OffsetFileToInches(long fileValue)
{
	return fileValue / 1000.0;
}

/* -----------------------------------------------------------------------
 * IsLocoType / type-band tests (10.3 Phase A item 1)
 * ----------------------------------------------------------------------- */

static void IsLocoTypeSpecCodesTests(void **state)
{
	(void)state;
	assert_true(IsLocoType(10101));   /* Diesel Loco */
	assert_true(IsLocoType(10201));   /* Steam Loco */
	assert_true(IsLocoType(10301));   /* Elect Loco */
	assert_false(IsLocoType(30100));  /* Freight Car */
	assert_false(IsLocoType(50100));  /* Psngr Car */
	assert_false(IsLocoType(70100));  /* M-O-W */
	assert_false(IsLocoType(90100));  /* Other */
}

static void IsLocoTypeBandBoundaryTests(void **state)
{
	(void)state;
	assert_false(IsLocoType(9999));
	assert_true(IsLocoType(10000));
	assert_true(IsLocoType(19999));
	assert_false(IsLocoType(20000));
}

static void IsLocoTypeOldBitBehaviorDiffersTests(void **state)
{
	(void)state;
	/* Regression case: the retired CAR_DESC_IS_LOCO/"type&1" heuristic
	 * disagreed with the type-band rule for an odd non-loco type code. */
	long oddNonLocoType = 50101;
	assert_true((oddNonLocoType & 1) != 0);
	assert_false(IsLocoType(oddNonLocoType));
}

/* -----------------------------------------------------------------------
 * RotatePts / RescalePts / MovePts (10.3 Phase A item 4)
 * ----------------------------------------------------------------------- */

#define NEARLY_EQ(a, b) (fabs((a) - (b)) < 1e-6)

static void RotatePtsTests(void **state)
{
	(void)state;
	coOrd orig = { 0.0, 0.0 };
	coOrd p[1] = { { 1.0, 0.0 } };

	/* Bearing convention (0=North/+Y, 90=East/+X, clockwise): rotating a
	 * point at bearing 90 (East, (1,0)) by +90 deg moves it to bearing
	 * 180 (South, (0,-1)). */
	RotatePts(1, p, orig, 90.0);
	assert_true(NEARLY_EQ(p[0].x, 0.0));
	assert_true(NEARLY_EQ(p[0].y, -1.0));
}

static void RescalePtsTests(void **state)
{
	(void)state;
	coOrd p[2] = { { 2.0, 3.0 }, { -1.0, 4.0 } };

	RescalePts(2, p, 2.0, 0.5);
	assert_true(NEARLY_EQ(p[0].x, 4.0));
	assert_true(NEARLY_EQ(p[0].y, 1.5));
	assert_true(NEARLY_EQ(p[1].x, -2.0));
	assert_true(NEARLY_EQ(p[1].y, 2.0));
}

static void MovePtsTests(void **state)
{
	(void)state;
	coOrd orig = { 1.0, -1.0 };
	coOrd p[2] = { { 0.0, 0.0 }, { 2.0, 2.0 } };

	MovePts(2, p, orig);
	assert_true(NEARLY_EQ(p[0].x, 1.0));
	assert_true(NEARLY_EQ(p[0].y, -1.0));
	assert_true(NEARLY_EQ(p[1].x, 3.0));
	assert_true(NEARLY_EQ(p[1].y, 1.0));
}

/* -----------------------------------------------------------------------
 * TabStringExtract / title composition (10.3 Phase A item 3)
 * ----------------------------------------------------------------------- */

static void TabStringExtractBasicTests(void **state)
{
	(void)state;
	char buf[64];
	tabString_t tabs[7];

	strcpy(buf, "Athearn\tBoxcar\t40' PS-1\t12345\tATSF\tATSF\t1234");
	TabStringExtract(buf, 7, tabs);

	assert_int_equal(tabs[T_MANUF].len, 7);
	assert_memory_equal(tabs[T_MANUF].ptr, "Athearn", 7);
	assert_int_equal(tabs[T_PROTO].len, 6);
	assert_memory_equal(tabs[T_PROTO].ptr, "Boxcar", 6);
	assert_int_equal(tabs[T_NUMBER].len, 4);
	assert_memory_equal(tabs[T_NUMBER].ptr, "1234", 4);
}

static void TabStringExtractEmptyFieldsTests(void **state)
{
	(void)state;
	char buf[64];
	tabString_t tabs[7];

	/* Consecutive tabs => empty PROTO/DESC/PART fields */
	strcpy(buf, "Athearn\t\t\t\tATSF\tATSF\t1234");
	TabStringExtract(buf, 7, tabs);

	assert_int_equal(tabs[T_PROTO].len, 0);
	assert_int_equal(tabs[T_DESC].len, 0);
	assert_int_equal(tabs[T_PART].len, 0);
}

static void TabStringExtractManufDefaultTests(void **state)
{
	(void)state;
	char buf[64];
	tabString_t tabs[7];

	/* Empty manuf field => "Unknown" default is substituted */
	strcpy(buf, "\tBoxcar\tDesc\tPT\tATSF\tATSF\t1234");
	TabStringExtract(buf, 7, tabs);

	assert_string_equal(tabs[T_MANUF].ptr, _("Unknown"));
	assert_int_equal(tabs[T_MANUF].len, (int)strlen(_("Unknown")));
}

static void TabStringExtractShortCountTests(void **state)
{
	(void)state;
	char buf[64];
	tabString_t tabs[7];

	/* No trailing fields at all: only class-name/manuf portion present */
	strcpy(buf, "Athearn");
	TabStringExtract(buf, 7, tabs);

	assert_int_equal(tabs[T_MANUF].len, 7);
	assert_int_equal(tabs[T_PROTO].len, 0);
	assert_int_equal(tabs[T_NUMBER].len, 0);
}

static void TabStringCpyRoundTripTests(void **state)
{
	(void)state;
	char buf[64];
	char out[32];
	tabString_t tabs[7];

	strcpy(buf, "Athearn\tBoxcar\tDesc\tPT\tATSF\tATSF\t1234");
	TabStringExtract(buf, 7, tabs);

	char *end = TabStringCpy(out, &tabs[T_NUMBER]);
	*end = '\0';
	assert_string_equal(out, "1234");
}

static void TabStringDupTests(void **state)
{
	(void)state;
	char buf[64];
	tabString_t tabs[7];

	strcpy(buf, "Athearn\tBoxcar\tDesc\tPT\tATSF\tATSF\t1234");
	TabStringExtract(buf, 7, tabs);

	char *dup = TabStringDup(&tabs[T_PROTO]);
	assert_string_equal(dup, "Boxcar");
	free(dup);
}

static void TabStringCmpTests(void **state)
{
	(void)state;
	char buf[64];
	tabString_t tabs[7];

	strcpy(buf, "Athearn\tBoxcar\tDesc\tPT\tATSF\tATSF\t1234");
	TabStringExtract(buf, 7, tabs);

	assert_int_equal(TabStringCmp("boxcar", &tabs[T_PROTO]), 0);
	assert_int_not_equal(TabStringCmp("Tank Car", &tabs[T_PROTO]), 0);
	assert_int_not_equal(TabStringCmp("Box", &tabs[T_PROTO]), 0);
}

static void TabGetLongFloatTests(void **state)
{
	(void)state;
	char buf[64];
	tabString_t tabs[7];

	strcpy(buf, "Athearn\tBoxcar\tDesc\tPT\tATSF\tATSF\t1234");
	TabStringExtract(buf, 7, tabs);
	assert_int_equal(TabGetLong(&tabs[T_NUMBER]), 1234);

	strcpy(buf, "Athearn\t3.5\tDesc\tPT\tATSF\tATSF\t1234");
	TabStringExtract(buf, 7, tabs);
	assert_true(NEARLY_EQ(TabGetFloat(&tabs[T_PROTO]), 3.5));

	tabString_t empty = { NULL, 0 };
	assert_true(NEARLY_EQ(TabGetFloat(&empty), 0.0));
	assert_int_equal(TabGetLong(&empty), 0);
}

/* -----------------------------------------------------------------------
 * Cmp_part / Cmp_roadnameMap / MapCondition (10.3 Phase A item 4)
 * ----------------------------------------------------------------------- */

static void CmpPartMatchTests(void **state)
{
	(void)state;
	cmp_part_t key = { PARAM_CUSTOM, "12345", 5 };
	cmp_part_t elem = { PARAM_CUSTOM, "12345", 5 };
	assert_int_equal(Cmp_part(&key, &elem), 0);

	cmp_part_t elem2 = { PARAM_CUSTOM, "99999", 5 };
	assert_true(Cmp_part(&key, &elem2) != 0);
}

static void CmpPartParamFilePrecedenceTests(void **state)
{
	(void)state;
	/* Same partno, different paramFileIndex: DEMO always sorts first,
	 * CUSTOM next, LAYOUT last, ordinary param files by index. */
	cmp_part_t demoKey = { PARAM_DEMO, "1", 1 };
	cmp_part_t customElem = { PARAM_CUSTOM, "1", 1 };
	assert_true(Cmp_part(&demoKey, &customElem) < 0);

	cmp_part_t customKey = { PARAM_CUSTOM, "1", 1 };
	cmp_part_t layoutElem = { PARAM_LAYOUT, "1", 1 };
	assert_true(Cmp_part(&customKey, &layoutElem) < 0);
}

static void CmpRoadnameMapTests(void **state)
{
	(void)state;
	roadnameMap_t elem = { "ATSF", "ATSF" };
	cmp_key_t exact = { "ATSF", 4 };
	assert_int_equal(Cmp_roadnameMap(&exact, &elem), 0);

	cmp_key_t prefixOfLonger = { "ATS", 3 };
	assert_true(Cmp_roadnameMap(&prefixOfLonger, &elem) < 0);

	cmp_key_t caseInsensitive = { "atsf", 4 };
	assert_int_equal(Cmp_roadnameMap(&caseInsensitive, &elem), 0);

	cmp_key_t noMatch = { "BNSF", 4 };
	assert_true(Cmp_roadnameMap(&noMatch, &elem) != 0);
}

static void MapConditionTests(void **state)
{
	(void)state;
	assert_int_equal(MapCondition(0), 0);
	assert_int_equal(MapCondition(9), 0);
	assert_int_equal(MapCondition(10), 5);
	assert_int_equal(MapCondition(29), 5);
	assert_int_equal(MapCondition(30), 4);
	assert_int_equal(MapCondition(49), 4);
	assert_int_equal(MapCondition(50), 3);
	assert_int_equal(MapCondition(69), 3);
	assert_int_equal(MapCondition(70), 2);
	assert_int_equal(MapCondition(89), 2);
	assert_int_equal(MapCondition(90), 1);
	assert_int_equal(MapCondition(100), 1);
}

static void OffsetRoundTripTests(void **state)
{
	(void)state;
	assert_int_equal(OffsetInchesToFile(OffsetFileToInches(0)), 0);
	assert_int_equal(OffsetInchesToFile(OffsetFileToInches(1234)), 1234);
	assert_int_equal(OffsetInchesToFile(OffsetFileToInches(-1234)), -1234);
	assert_int_equal(OffsetInchesToFile(OffsetFileToInches(9999999)), 9999999);
}

static void OffsetInchesToFileTests(void **state)
{
	(void)state;
	assert_int_equal(OffsetInchesToFile(0.0), 0);
	assert_int_equal(OffsetInchesToFile(1.5), 1500);
	assert_int_equal(OffsetInchesToFile(-1.5), -1500);
	/* truncates toward zero via the (long) cast, not rounds */
	assert_int_equal(OffsetInchesToFile(0.1235), 123);
}

static void OffsetFileToInchesTests(void **state)
{
	(void)state;
	assert_true(NEARLY_EQ(OffsetFileToInches(0), 0.0));
	assert_true(NEARLY_EQ(OffsetFileToInches(1500), 1.5));
	assert_true(NEARLY_EQ(OffsetFileToInches(-1500), -1.5));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(IsLocoTypeSpecCodesTests),
		cmocka_unit_test(IsLocoTypeBandBoundaryTests),
		cmocka_unit_test(IsLocoTypeOldBitBehaviorDiffersTests),
		cmocka_unit_test(RotatePtsTests),
		cmocka_unit_test(RescalePtsTests),
		cmocka_unit_test(MovePtsTests),
		cmocka_unit_test(TabStringExtractBasicTests),
		cmocka_unit_test(TabStringExtractEmptyFieldsTests),
		cmocka_unit_test(TabStringExtractManufDefaultTests),
		cmocka_unit_test(TabStringExtractShortCountTests),
		cmocka_unit_test(TabStringCpyRoundTripTests),
		cmocka_unit_test(TabStringDupTests),
		cmocka_unit_test(TabStringCmpTests),
		cmocka_unit_test(TabGetLongFloatTests),
		cmocka_unit_test(CmpPartMatchTests),
		cmocka_unit_test(CmpPartParamFilePrecedenceTests),
		cmocka_unit_test(CmpRoadnameMapTests),
		cmocka_unit_test(MapConditionTests),
		cmocka_unit_test(OffsetRoundTripTests),
		cmocka_unit_test(OffsetInchesToFileTests),
		cmocka_unit_test(OffsetFileToInchesTests),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
