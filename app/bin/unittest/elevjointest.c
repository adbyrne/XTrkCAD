/** \file elevjointest.c
 * Unit tests for elevjoin.c -- the endpoint elevation-mode merge logic
 * extracted from elev.c:SetTrkElevModes().
 *
 * elevjoin.c has no dependency beyond libc (strcmp / fabs) and the ELEV_*
 * constants in track.h, so it is linked in as the real source with no stubs.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <math.h>
#include <cmocka.h>

#include "common.h"
#include "track.h"
#include "elevjoin.h"

#define NEARLY_EQ(a, b) (fabs((a) - (b)) < 1e-9)

/* -----------------------------------------------------------------------
 * endElev_t builders
 * ----------------------------------------------------------------------- */

static endElev_t Def(DIST_T h)
{
	endElev_t e = { ELEV_DEF, ELEV_DEF, h, NULL };
	return e;
}

static endElev_t DefVis(DIST_T h)
{
	endElev_t e = { ELEV_DEF, ELEV_DEF | ELEV_VISIBLE, h, NULL };
	return e;
}

static endElev_t Sta(const char * name)
{
	endElev_t e = { ELEV_STATION, ELEV_STATION, 0.0, name };
	return e;
}

static endElev_t Mode(int m)
{
	endElev_t e = { m, m, 0.0, NULL };
	return e;
}

/* -----------------------------------------------------------------------
 * Defined / Defined: average + mismatch flag
 * ----------------------------------------------------------------------- */

static void DefDefAveragesTests(void **state)
{
	(void)state;
	joinElev_t j = MergeJoinElev(Def(5.0), Def(5.0));
	assert_int_equal(j.mode & ELEV_MASK, ELEV_DEF);
	assert_true(NEARLY_EQ(j.elev, 5.0));
	assert_false(j.heightsDiffer);
	assert_null(j.station);
}

static void DefDefFlagsMismatchTests(void **state)
{
	(void)state;
	joinElev_t j = MergeJoinElev(Def(5.0), Def(5.3));
	assert_int_equal(j.mode & ELEV_MASK, ELEV_DEF);
	assert_true(NEARLY_EQ(j.elev, 5.15));
	assert_true(j.heightsDiffer);
	assert_true(NEARLY_EQ(j.diff, 0.3));
}

static void DefDefSmallGapNoFlagTests(void **state)
{
	(void)state;
	/* a gap below ELEVJOIN_DIFF_EPS does not warn */
	joinElev_t j = MergeJoinElev(Def(1.0), Def(1.05));
	assert_true(NEARLY_EQ(j.elev, 1.025));
	assert_false(j.heightsDiffer);
}

static void DefDefOrsVisibleBitTests(void **state)
{
	(void)state;
	joinElev_t j = MergeJoinElev(Def(2.0), DefVis(2.0));
	assert_int_equal(j.mode & ELEV_MASK, ELEV_DEF);
	assert_true((j.mode & ELEV_VISIBLE) != 0);
}

/* -----------------------------------------------------------------------
 * Precedence: Defined > Station > Grade > Computed > (None)
 * ----------------------------------------------------------------------- */

static void DefBeatsStationEitherOrderTests(void **state)
{
	(void)state;
	joinElev_t j0 = MergeJoinElev(Def(4.0), Sta("A"));
	assert_int_equal(j0.mode & ELEV_MASK, ELEV_DEF);
	assert_true(NEARLY_EQ(j0.elev, 4.0));
	assert_null(j0.station);
	assert_false(j0.heightsDiffer);

	/* second endpoint is the defined one: must still resolve to DEF with
	 * that height (regression for the pre-extraction ladder bug). */
	joinElev_t j1 = MergeJoinElev(Sta("A"), Def(2.0));
	assert_int_equal(j1.mode & ELEV_MASK, ELEV_DEF);
	assert_true(NEARLY_EQ(j1.elev, 2.0));
	assert_null(j1.station);
	assert_false(j1.heightsDiffer);
}

static void StationBeatsGradeEitherOrderTests(void **state)
{
	(void)state;
	joinElev_t j0 = MergeJoinElev(Mode(ELEV_GRADE), Sta("X"));
	assert_int_equal(j0.mode & ELEV_MASK, ELEV_STATION);
	assert_string_equal(j0.station, "X");

	joinElev_t j1 = MergeJoinElev(Sta("X"), Mode(ELEV_GRADE));
	assert_int_equal(j1.mode & ELEV_MASK, ELEV_STATION);
	assert_string_equal(j1.station, "X");
}

static void GradeBeatsCompEitherOrderTests(void **state)
{
	(void)state;
	assert_int_equal(MergeJoinElev(Mode(ELEV_GRADE), Mode(ELEV_COMP)).mode
	                 & ELEV_MASK, ELEV_GRADE);
	assert_int_equal(MergeJoinElev(Mode(ELEV_COMP), Mode(ELEV_GRADE)).mode
	                 & ELEV_MASK, ELEV_GRADE);
}

static void CompBeatsNoneEitherOrderTests(void **state)
{
	(void)state;
	assert_int_equal(MergeJoinElev(Mode(ELEV_NONE), Mode(ELEV_COMP)).mode
	                 & ELEV_MASK, ELEV_COMP);
	assert_int_equal(MergeJoinElev(Mode(ELEV_COMP), Mode(ELEV_NONE)).mode
	                 & ELEV_MASK, ELEV_COMP);
}

static void TieGoesToFirstEndpointTests(void **state)
{
	(void)state;
	joinElev_t j = MergeJoinElev(Mode(ELEV_GRADE), Mode(ELEV_GRADE));
	assert_int_equal(j.mode & ELEV_MASK, ELEV_GRADE);
	assert_true(NEARLY_EQ(j.elev, 0.0));
	assert_null(j.station);

	assert_int_equal(MergeJoinElev(Mode(ELEV_NONE), Mode(ELEV_NONE)).mode
	                 & ELEV_MASK, ELEV_NONE);
}

/* -----------------------------------------------------------------------
 * Station / Station name mismatch (SF #707)
 * ----------------------------------------------------------------------- */

static void StationSameNameNoFlagTests(void **state)
{
	(void)state;
	joinElev_t j = MergeJoinElev(Sta("Yard"), Sta("Yard"));
	assert_int_equal(j.mode & ELEV_MASK, ELEV_STATION);
	assert_string_equal(j.station, "Yard");
	assert_false(j.stationsDiffer);
}

static void StationDiffNameFlagsTests(void **state)
{
	(void)state;
	joinElev_t j = MergeJoinElev(Sta("North"), Sta("South"));
	assert_int_equal(j.mode & ELEV_MASK, ELEV_STATION);
	assert_string_equal(j.station, "North");	/* first endpoint wins */
	assert_true(j.stationsDiffer);
}

static void StationNullNamesTreatedEqualTests(void **state)
{
	(void)state;
	joinElev_t j = MergeJoinElev(Sta(NULL), Sta(NULL));
	assert_false(j.stationsDiffer);
}

static void StationsDifferOnlyWhenBothStationTests(void **state)
{
	(void)state;
	/* a Station losing to a Defined winner must not surface as a mismatch */
	joinElev_t j = MergeJoinElev(Def(3.0), Sta("Ghost"));
	assert_false(j.stationsDiffer);
	assert_null(j.station);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(DefDefAveragesTests),
		cmocka_unit_test(DefDefFlagsMismatchTests),
		cmocka_unit_test(DefDefSmallGapNoFlagTests),
		cmocka_unit_test(DefDefOrsVisibleBitTests),
		cmocka_unit_test(DefBeatsStationEitherOrderTests),
		cmocka_unit_test(StationBeatsGradeEitherOrderTests),
		cmocka_unit_test(GradeBeatsCompEitherOrderTests),
		cmocka_unit_test(CompBeatsNoneEitherOrderTests),
		cmocka_unit_test(TieGoesToFirstEndpointTests),
		cmocka_unit_test(StationSameNameNoFlagTests),
		cmocka_unit_test(StationDiffNameFlagsTests),
		cmocka_unit_test(StationNullNamesTreatedEqualTests),
		cmocka_unit_test(StationsDifferOnlyWhenBothStationTests),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
