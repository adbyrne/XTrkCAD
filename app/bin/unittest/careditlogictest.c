/** \file careditlogictest.c
 * Unit tests for cars/careditlogic.c -- the pure validation/resolution
 * helpers extracted out of careditdlg.c's CarDlgOk/CarDlgUpdate so they can
 * be exercised directly, without a live dialog.
 *
 * Unlike carstest.c's copy-verbatim approach, careditlogic.c has exactly one
 * external dependency beyond libc (GetScaleRatio, from scale.c -- confirmed
 * by compiling the file standalone and checking undefined symbols), so it is
 * linked in as the real source and that one function is stubbed below.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <math.h>
#include <cmocka.h>

#include "common.h"
#include "cars/carsprivate.h"

#define NEARLY_EQ(a, b) (fabs((a) - (b)) < 1e-6)

/* -----------------------------------------------------------------------
 * Stub -- GetScaleRatio (scale.c) is the only non-libc symbol
 * careditlogic.c's SeedProtoDimsFromCatalog calls.
 * ----------------------------------------------------------------------- */

static DIST_T stubScaleRatio = 1.0;

DIST_T GetScaleRatio(SCALEINX_T si)
{
	(void)si;
	return stubScaleRatio;
}

/* -----------------------------------------------------------------------
 * CarDlgResState
 * ----------------------------------------------------------------------- */

static void CarDlgResStateResolvedTests(void **state)
{
	(void)state;
	/* A matched list index wins regardless of the typed text. */
	assert_int_equal(CarDlgResState(0, ""), RS_RESOLVED);
	assert_int_equal(CarDlgResState(3, "whatever"), RS_RESOLVED);
}

static void CarDlgResStateUnresolvedTests(void **state)
{
	(void)state;
	/* No matched index and no typed text: untouched, not "new". */
	assert_int_equal(CarDlgResState(-1, ""), RS_UNRESOLVED);
}

static void CarDlgResStateNewTests(void **state)
{
	(void)state;
	/* No matched index but genuinely typed text: new. */
	assert_int_equal(CarDlgResState(-1, "Athearn"), RS_NEW);
}

/* -----------------------------------------------------------------------
 * CarValidateDate
 * ----------------------------------------------------------------------- */

static void CarValidateDateEmptyTests(void **state)
{
	(void)state;
	long valL = 999;
	assert_int_equal(CarValidateDate("", &valL), CAR_DATE_EMPTY);
	assert_int_equal(valL, 0);

	valL = 999;
	assert_int_equal(CarValidateDate("   ", &valL), CAR_DATE_EMPTY);
	assert_int_equal(valL, 0);
}

static void CarValidateDateNotNumericTests(void **state)
{
	(void)state;
	long valL;
	assert_int_equal(CarValidateDate("2024-01-01", &valL), CAR_DATE_NOT_NUMERIC);
}

static void CarValidateDateWrongLengthTests(void **state)
{
	(void)state;
	long valL;
	assert_int_equal(CarValidateDate("2024101", &valL), CAR_DATE_WRONG_LENGTH);
	/* All-zero parses as valL==0, rejected by the same guard. */
	assert_int_equal(CarValidateDate("00000000", &valL), CAR_DATE_WRONG_LENGTH);
}

static void CarValidateDateOutOfRangeTests(void **state)
{
	(void)state;
	long valL;
	assert_int_equal(CarValidateDate("18991231", &valL), CAR_DATE_OUT_OF_RANGE);
	assert_int_equal(CarValidateDate("22000101", &valL), CAR_DATE_OUT_OF_RANGE);
}

static void CarValidateDateBadMonthTests(void **state)
{
	(void)state;
	long valL;
	assert_int_equal(CarValidateDate("20241301", &valL), CAR_DATE_BAD_MONTH);
}

static void CarValidateDateBadDayTests(void **state)
{
	(void)state;
	long valL;
	assert_int_equal(CarValidateDate("20240132", &valL), CAR_DATE_BAD_DAY);
}

static void CarValidateDateOkTests(void **state)
{
	(void)state;
	long valL = 0;
	assert_int_equal(CarValidateDate("20240115", &valL), CAR_DATE_OK);
	assert_int_equal(valL, 20240115);

	/* Leading whitespace is skipped before parsing. */
	valL = 0;
	assert_int_equal(CarValidateDate("  19000101", &valL), CAR_DATE_OK);
	assert_int_equal(valL, 19000101);

	valL = 0;
	assert_int_equal(CarValidateDate("21991231", &valL), CAR_DATE_OK);
	assert_int_equal(valL, 21991231);
}

/* -----------------------------------------------------------------------
 * CarValidatePrice
 * ----------------------------------------------------------------------- */

static void CarValidatePriceOkTests(void **state)
{
	(void)state;
	FLOAT_T price = -1.0;
	assert_true(CarValidatePrice("42.50", &price));
	assert_true(NEARLY_EQ(price, 42.50));

	assert_true(CarValidatePrice("  100", &price));
	assert_true(NEARLY_EQ(price, 100.0));
}

static void CarValidatePriceEmptyTests(void **state)
{
	(void)state;
	/* Empty string: strtod performs no conversion but consumes nothing,
	 * so the trailing-garbage check passes trivially -- treated as 0. */
	FLOAT_T price = -1.0;
	assert_true(CarValidatePrice("", &price));
	assert_true(NEARLY_EQ(price, 0.0));
}

static void CarValidatePriceInvalidTests(void **state)
{
	(void)state;
	FLOAT_T price;
	assert_false(CarValidatePrice("12.5abc", &price));
	assert_false(CarValidatePrice("abc", &price));
}

/* -----------------------------------------------------------------------
 * SetNextPartno
 * ----------------------------------------------------------------------- */

static void SetNextPartnoIncrementsTests(void **state)
{
	(void)state;
	char buf[32];

	strcpy(buf, "12345");
	SetNextPartno(buf);
	assert_string_equal(buf, "12346");

	/* Leading zeros are not preserved -- strtol/sprintf round trip. */
	strcpy(buf, "007");
	SetNextPartno(buf);
	assert_string_equal(buf, "8");
}

static void SetNextPartnoNonNumericClearsTests(void **state)
{
	(void)state;
	char buf[32];

	strcpy(buf, "ATSF-1");
	SetNextPartno(buf);
	assert_string_equal(buf, "");
}

static void SetNextPartnoZeroClearsTests(void **state)
{
	(void)state;
	char buf[32];

	/* number>0 is required, so "0" is treated the same as non-numeric. */
	strcpy(buf, "0");
	SetNextPartno(buf);
	assert_string_equal(buf, "");
}

static void SetNextPartnoEmptyStaysEmptyTests(void **state)
{
	(void)state;
	char buf[32];

	strcpy(buf, "");
	SetNextPartno(buf);
	assert_string_equal(buf, "");
}

static void SetNextPartnoNegativeClearsTests(void **state)
{
	(void)state;
	char buf[32];

	/* number>0 is required, so a negative number must clear the same as
	 * non-numeric -- previously wrapped to a huge unsigned value instead. */
	strcpy(buf, "-5");
	SetNextPartno(buf);
	assert_string_equal(buf, "");
}

/* -----------------------------------------------------------------------
 * SeedProtoDimsFromCatalog
 * ----------------------------------------------------------------------- */

static void SeedProtoDimsFromCatalogScalesAllFieldsTests(void **state)
{
	(void)state;
	carDim_t catalogDim = { 40.0, 10.0, 30.0, 1.0, 42.0 };
	carDim_t protoDim;

	stubScaleRatio = 87.1; /* HO */
	protoDim = SeedProtoDimsFromCatalog(catalogDim, 0 /* scaleInx unused by stub */);

	assert_true(NEARLY_EQ(protoDim.carLength, 40.0 * 87.1));
	assert_true(NEARLY_EQ(protoDim.carWidth, 10.0 * 87.1));
	assert_true(NEARLY_EQ(protoDim.truckCenter, 30.0 * 87.1));
	assert_true(NEARLY_EQ(protoDim.truckCenterOffset, 1.0 * 87.1));
	assert_true(NEARLY_EQ(protoDim.coupledLength, 42.0 * 87.1));

	stubScaleRatio = 1.0;
}

/* -----------------------------------------------------------------------
 * CarValidateDims
 * ----------------------------------------------------------------------- */

static void CarValidateDimsOkTests(void **state)
{
	(void)state;
	carDim_t dims = { 40.0, 10.0, 30.0, 0.0, 42.0 };
	assert_int_equal(CarValidateDims(&dims), 0);
}

static void CarValidateDimsWidthBadIsolatedTests(void **state)
{
	(void)state;
	/* carWidth<=0 alone: field-local bit only, no relational spillover
	 * since carLength(40) > carWidth(0) still holds. */
	carDim_t dims = { 40.0, 0.0, 30.0, 0.0, 42.0 };
	assert_int_equal(CarValidateDims(&dims), CAR_DIMS_WIDTH_BAD);
}

static void CarValidateDimsOffsetNegativeIsolatedTests(void **state)
{
	(void)state;
	carDim_t dims = { 40.0, 10.0, 30.0, -1.0, 42.0 };
	assert_int_equal(CarValidateDims(&dims), CAR_DIMS_TRKOFFSET_BAD);
}

static void CarValidateDimsTruckCenterRelationalTests(void **state)
{
	(void)state;
	/* truckCenter itself is a valid positive value, but >= carLength
	 * trips the relational check -- flags both fields, not just one. */
	carDim_t dims = { 40.0, 10.0, 45.0, 0.0, 42.0 };
	unsigned result = CarValidateDims(&dims);
	assert_int_equal(result, CAR_DIMS_TRKCENTER_BAD | CAR_DIMS_LENGTH_BAD);
}

static void CarValidateDimsCoupledLengthRelationalTests(void **state)
{
	(void)state;
	carDim_t dims = { 40.0, 10.0, 30.0, 0.0, 35.0 };
	unsigned result = CarValidateDims(&dims);
	assert_int_equal(result, CAR_DIMS_CPLDLEN_BAD | CAR_DIMS_LENGTH_BAD);
}

static void CarValidateDimsLengthZeroCascadesTests(void **state)
{
	(void)state;
	/* carLength<=0 is both a field-local violation AND trips every
	 * relational check that compares against it -- the whole point of
	 * the bitmask design (see careditlogic.c's header comment) is that
	 * a single bad field can legitimately light up several bits. */
	carDim_t dims = { 0.0, 10.0, 30.0, 0.0, 42.0 };
	unsigned result = CarValidateDims(&dims);
	assert_int_equal(result,
	                 CAR_DIMS_LENGTH_BAD | CAR_DIMS_WIDTH_BAD |
	                 CAR_DIMS_TRKCENTER_BAD);
}

/* -----------------------------------------------------------------------
 * CarValidateIdentity
 * ----------------------------------------------------------------------- */

static void CarValidateIdentityAllValidTests(void **state)
{
	(void)state;
	unsigned result = CarValidateIdentity(LVL_CAR, RS_RESOLVED, RS_RESOLVED,
	                                      "Athearn", "Boxcar", "12345");
	assert_int_equal(result, 0);
}

static void CarValidateIdentityProtoEmptyGatedByLevelTests(void **state)
{
	(void)state;
	/* Flagged at the proto/model levels ... */
	assert_int_equal(
	    CarValidateIdentity(LVL_PROTO, RS_RESOLVED, RS_RESOLVED, "Athearn", "", "12345"),
	    CAR_IDENT_PROTO_EMPTY);
	assert_int_equal(
	    CarValidateIdentity(LVL_MODEL, RS_RESOLVED, RS_RESOLVED, "Athearn", "", "12345"),
	    CAR_IDENT_PROTO_EMPTY);
	/* ... but not at the car level, where the prototype step doesn't apply. */
	assert_int_equal(
	    CarValidateIdentity(LVL_CAR, RS_RESOLVED, RS_RESOLVED, "Athearn", "", "12345"),
	    0);
}

static void CarValidateIdentityManufEmptyGatedByLevelTests(void **state)
{
	(void)state;
	assert_int_equal(
	    CarValidateIdentity(LVL_CAR, RS_RESOLVED, RS_RESOLVED, "", "Boxcar", "12345"),
	    CAR_IDENT_MANUF_EMPTY);
	assert_int_equal(
	    CarValidateIdentity(LVL_MODEL, RS_RESOLVED, RS_RESOLVED, "", "Boxcar", "12345"),
	    CAR_IDENT_MANUF_EMPTY);
	/* The prototype-only level doesn't carry a manufacturer field. */
	assert_int_equal(
	    CarValidateIdentity(LVL_PROTO, RS_RESOLVED, RS_RESOLVED, "", "Boxcar", "12345"),
	    0);
}

static void CarValidateIdentityPartnoEmptyGatedByLevelTests(void **state)
{
	(void)state;
	/* Same car/model gating as the manuf-empty check above. */
	assert_int_equal(
	    CarValidateIdentity(LVL_MODEL, RS_RESOLVED, RS_RESOLVED, "Athearn", "Boxcar", ""),
	    CAR_IDENT_PARTNO_EMPTY);
	assert_int_equal(
	    CarValidateIdentity(LVL_CAR, RS_RESOLVED, RS_RESOLVED, "Athearn", "Boxcar", ""),
	    CAR_IDENT_PARTNO_EMPTY);
	/* Not checked at the prototype-only level. */
	assert_int_equal(
	    CarValidateIdentity(LVL_PROTO, RS_RESOLVED, RS_RESOLVED, "Athearn", "Boxcar", ""),
	    0);
}

static void CarValidateIdentityCatalogUnresolvedTests(void **state)
{
	(void)state;
	assert_int_equal(
	    CarValidateIdentity(LVL_CAR, RS_UNRESOLVED, RS_RESOLVED, "Athearn", "Boxcar", "12345"),
	    CAR_IDENT_CATALOG_UNRESOLVED);
	/* Not checked once past the item level -- a nested Model/Proto panel
	 * carries its own resolution state instead. */
	assert_int_equal(
	    CarValidateIdentity(LVL_MODEL, RS_UNRESOLVED, RS_RESOLVED, "Athearn", "Boxcar", "12345"),
	    0);
}

static void CarValidateIdentityProtoUnresolvedTests(void **state)
{
	(void)state;
	assert_int_equal(
	    CarValidateIdentity(LVL_CAR, RS_RESOLVED, RS_UNRESOLVED, "Athearn", "Boxcar", "12345"),
	    CAR_IDENT_PROTO_UNRESOLVED);
}

static void CarValidateIdentityNoLevelNeverFlagsTests(void **state)
{
	(void)state;
	/* LVL_NONE (CarDlgStateLevel's default case, e.g. for S_Error) isn't
	 * named in any of the checks: an otherwise fully blank record never
	 * blocks Ok while transiently at no recognized level. */
	assert_int_equal(
	    CarValidateIdentity(LVL_NONE, RS_UNRESOLVED, RS_UNRESOLVED, "", "", ""), 0);
}

static void CarValidateIdentityCombinedBitsTests(void **state)
{
	(void)state;
	/* Everything blank at the car level: manuf/partno-empty and both
	 * unresolved bits all fire together (proto-empty does not, since
	 * LVL_CAR isn't in that check's level set). */
	unsigned result = CarValidateIdentity(LVL_CAR, RS_UNRESOLVED, RS_UNRESOLVED,
	                                      "", "Boxcar", "");
	assert_int_equal(result,
	                 CAR_IDENT_MANUF_EMPTY | CAR_IDENT_PARTNO_EMPTY |
	                 CAR_IDENT_CATALOG_UNRESOLVED | CAR_IDENT_PROTO_UNRESOLVED);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(CarDlgResStateResolvedTests),
		cmocka_unit_test(CarDlgResStateUnresolvedTests),
		cmocka_unit_test(CarDlgResStateNewTests),

		cmocka_unit_test(CarValidateDateEmptyTests),
		cmocka_unit_test(CarValidateDateNotNumericTests),
		cmocka_unit_test(CarValidateDateWrongLengthTests),
		cmocka_unit_test(CarValidateDateOutOfRangeTests),
		cmocka_unit_test(CarValidateDateBadMonthTests),
		cmocka_unit_test(CarValidateDateBadDayTests),
		cmocka_unit_test(CarValidateDateOkTests),

		cmocka_unit_test(CarValidatePriceOkTests),
		cmocka_unit_test(CarValidatePriceEmptyTests),
		cmocka_unit_test(CarValidatePriceInvalidTests),

		cmocka_unit_test(SetNextPartnoIncrementsTests),
		cmocka_unit_test(SetNextPartnoNonNumericClearsTests),
		cmocka_unit_test(SetNextPartnoZeroClearsTests),
		cmocka_unit_test(SetNextPartnoEmptyStaysEmptyTests),
		cmocka_unit_test(SetNextPartnoNegativeClearsTests),

		cmocka_unit_test(SeedProtoDimsFromCatalogScalesAllFieldsTests),

		cmocka_unit_test(CarValidateDimsOkTests),
		cmocka_unit_test(CarValidateDimsWidthBadIsolatedTests),
		cmocka_unit_test(CarValidateDimsOffsetNegativeIsolatedTests),
		cmocka_unit_test(CarValidateDimsTruckCenterRelationalTests),
		cmocka_unit_test(CarValidateDimsCoupledLengthRelationalTests),
		cmocka_unit_test(CarValidateDimsLengthZeroCascadesTests),

		cmocka_unit_test(CarValidateIdentityAllValidTests),
		cmocka_unit_test(CarValidateIdentityProtoEmptyGatedByLevelTests),
		cmocka_unit_test(CarValidateIdentityManufEmptyGatedByLevelTests),
		cmocka_unit_test(CarValidateIdentityPartnoEmptyGatedByLevelTests),
		cmocka_unit_test(CarValidateIdentityCatalogUnresolvedTests),
		cmocka_unit_test(CarValidateIdentityProtoUnresolvedTests),
		cmocka_unit_test(CarValidateIdentityNoLevelNeverFlagsTests),
		cmocka_unit_test(CarValidateIdentityCombinedBitsTests),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
