/** \file reportstest.c
* Unit tests for reports.c's pure row-formatting function.
*/

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include <cmocka.h>

#include <string.h>

#include <dynstring.h>
#include "../include/reports.h"

static void test_empty_list(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 16);

	ReportsFormatUnconnectedList(&out, NULL, 0);

	assert_string_equal(DynStringToCStr(&out), "");
	DynStringFree(&out);
}

static void test_single_row(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 16);
	reportsEndPt_t list[1] = {
		{ 1, { 0.0, 0.0 }, 180.0, 0, 0, 1, "Main" }
	};

	ReportsFormatUnconnectedList(&out, list, 1);

	assert_string_equal(DynStringToCStr(&out),
	                    "     1 |     1 | Main             |    0.000 |    0.000 | 180.000 | \n");
	DynStringFree(&out);
}

/* Matches the phase-1 .xtr fixture (dmreportuncon.xtr): three tracks, two
 * joined to each other (endpoints not listed here), four open endpoints
 * total -- one each on tracks 1/2, both on track 3 (which has no
 * connections at all). Track 3's two rows, in endpoint order, confirm
 * multiple open endpoints on the same track id are listed individually,
 * not rolled up. */
static void test_fixture_shaped_list(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 64);
	/* Track 3's two rows are its only endpoints -- both open, so it's
	 * flagged `isolated` (a fully disconnected/"ghost" track), unlike
	 * tracks 1/2 which are each a deliberate dead-end stub (their other
	 * endpoint is joined, not part of this list). */
	reportsEndPt_t list[4] = {
		{ 1, {  0.0, 0.0 }, 180.0, 0, 0, 1, "Main", 0, 0 },
		{ 2, {  8.0, 0.0 },   0.0, 0, 0, 2, "Yard", 0, 0 },
		{ 3, {  0.0, 4.0 }, 180.0, 0, 0, 1, "Main", 1, 0 },
		{ 3, {  4.0, 4.0 },   0.0, 0, 0, 1, "Main", 1, 0 },
	};

	ReportsFormatUnconnectedList(&out, list, 4);

	assert_string_equal(DynStringToCStr(&out),
	                    "     1 |     1 | Main             |    0.000 |    0.000 | 180.000 | \n"
	                    "     2 |     2 | Yard             |    8.000 |    0.000 |   0.000 | \n"
	                    "     3 |     1 | Main             |    0.000 |    4.000 | 180.000 | Isolated\n"
	                    "     3 |     1 | Main             |    4.000 |    4.000 |   0.000 | Isolated\n");
	DynStringFree(&out);
}

static void test_negative_and_fractional_values(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 16);
	reportsEndPt_t list[1] = {
		{ 42, { -1.5, 200.286947 }, 55.067831, 0, 0, 3, "Staging" }
	};

	ReportsFormatUnconnectedList(&out, list, 1);

	assert_string_equal(DynStringToCStr(&out),
	                    "    42 |     3 | Staging          |   -1.500 |  200.287 |  55.068 | \n");
	DynStringFree(&out);
}

static void test_appends_not_overwrites(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 16);
	DynStringCatCStr(&out, "existing content\n");
	reportsEndPt_t list[1] = {
		{ 1, { 0.0, 0.0 }, 0.0, 0, 0, 1, "Main" }
	};

	ReportsFormatUnconnectedList(&out, list, 1);

	assert_string_equal(DynStringToCStr(&out),
	                    "existing content\n"
	                    "     1 |     1 | Main             |    0.000 |    0.000 |   0.000 | \n");
	DynStringFree(&out);
}

/* ReportsFormatEndPtNote() -- the free-text Note column. Plain
 * concatenation, not a fixed enum, so any subset of Turntable/Isolated/
 * Nearby can apply to the same row. */

static void test_note_none_set(void **state)
{
	(void) state;
	char buf[32];
	reportsEndPt_t entry = { 1, { 0.0, 0.0 }, 0.0, 0, 0, 1, "Main", 0, 0 };

	ReportsFormatEndPtNote(&entry, buf, sizeof buf);

	assert_string_equal(buf, "");
}

static void test_note_nearby_only(void **state)
{
	(void) state;
	char buf[32];
	reportsEndPt_t entry = { 1, { 0.0, 0.0 }, 0.0, 0, 1, 1, "Main", 0, 0 };

	ReportsFormatEndPtNote(&entry, buf, sizeof buf);

	assert_string_equal(buf, "Nearby");
}

static void test_note_isolated_only(void **state)
{
	(void) state;
	char buf[32];
	reportsEndPt_t entry = { 1, { 0.0, 0.0 }, 0.0, 0, 0, 1, "Main", 1, 0 };

	ReportsFormatEndPtNote(&entry, buf, sizeof buf);

	assert_string_equal(buf, "Isolated");
}

static void test_note_turntable_only(void **state)
{
	(void) state;
	char buf[32];
	reportsEndPt_t entry = { 1, { 0.0, 0.0 }, 0.0, 0, 0, 1, "Main", 0, 1 };

	ReportsFormatEndPtNote(&entry, buf, sizeof buf);

	assert_string_equal(buf, "Turntable");
}

/* Isolated + Nearby together is a real, if unusual, case: two floating
 * stub tracks left close enough to look like a missed connection. Not
 * paired with `turntable` here -- ReportsUnconnectedEndpoints() never
 * sets both `isolated` and `turntable` TRUE on the same row (see its own
 * doc comment), so that combination is deliberately not exercised. */
static void test_note_isolated_and_nearby(void **state)
{
	(void) state;
	char buf[32];
	reportsEndPt_t entry = { 1, { 0.0, 0.0 }, 0.0, 0, 1, 1, "Main", 1, 0 };

	ReportsFormatEndPtNote(&entry, buf, sizeof buf);

	assert_string_equal(buf, "Isolated, Nearby");
}

static void test_note_turntable_and_nearby(void **state)
{
	(void) state;
	char buf[32];
	reportsEndPt_t entry = { 1, { 0.0, 0.0 }, 0.0, 0, 1, 1, "Main", 0, 1 };

	ReportsFormatEndPtNote(&entry, buf, sizeof buf);

	assert_string_equal(buf, "Turntable, Nearby");
}

/* ---------------------------------------------------------------------
 * Phase 2 batch (track lengths / curve stats / turnout density /
 * equipment suitability) -- SF #217. Written test-first, 2026-09-04.
 * Row layouts were revised the same day to drop MCP-text-parity in favor
 * of readability improvements (density-sorted turnout table with a
 * trailing HEAVY word instead of "***"/trailing whitespace, grouped
 * PASS/MARGINAL/FAIL equipment table, a %-of-total column on track
 * lengths) -- exact parity with the Python reference was never required,
 * only the underlying data/thresholds.
 * ------------------------------------------------------------------- */

static void test_tracklen_empty_list(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 16);

	ReportsFormatTrackLengthList(&out, NULL, 0, 0.0);

	assert_string_equal(DynStringToCStr(&out), "");
	DynStringFree(&out);
}

static void test_tracklen_single_row_full_pct(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 64);
	reportsTrackLenLayer_t list[1] = {
		{ 0, "Main", 120.0, 1440.0, 3, 40 }
	};

	ReportsFormatTrackLengthList(&out, list, 1, 120.0);

	assert_string_equal(DynStringToCStr(&out),
	                    "    0  Main                 120.0     1440.0      3     40  100.0%\n");
	DynStringFree(&out);
}

/* Two layers, hand-computed expected percentages (120.0/165.5*100 =
 * 72.5..., 45.5/165.5*100 = 27.49... -> 27.5 at .1f) -- verified against
 * the real format string via a standalone printf check, same
 * fixture-shaped-list style as phase 1's own test_fixture_shaped_list(). */
static void test_tracklen_multi_layer_pct(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 128);
	reportsTrackLenLayer_t list[2] = {
		{ 0, "Main", 120.0, 1440.0, 3, 40 },
		{ 2, "Staging", 45.5, 546.0, 0, 15 }
	};

	ReportsFormatTrackLengthList(&out, list, 2, 165.5);

	assert_string_equal(DynStringToCStr(&out),
	                    "    0  Main                 120.0     1440.0      3     40   72.5%\n"
	                    "    2  Staging               45.5      546.0      0     15   27.5%\n");
	DynStringFree(&out);
}

/* totalFt <= 0 must render 0.0%, not divide-by-zero/NaN -- same guard
 * shape as turnout density's zero-feet-layer case below. */
static void test_tracklen_zero_total_no_crash(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 64);
	reportsTrackLenLayer_t list[1] = {
		{ 0, "Solo", 45.5, 546.0, 0, 15 }
	};

	ReportsFormatTrackLengthList(&out, list, 1, 0.0);

	assert_string_equal(DynStringToCStr(&out),
	                    "    0  Solo                  45.5      546.0      0     15    0.0%\n");
	DynStringFree(&out);
}

static void test_curvehist_empty(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 16);

	ReportsFormatCurveHistogram(&out, NULL, 0);

	assert_string_equal(DynStringToCStr(&out), "");
	DynStringFree(&out);
}

static void test_curvehist_buckets(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 64);
	reportsCurveBucket_t list[2] = {
		{ "< 12in", 3 },
		{ "12-18in", 5 }
	};

	ReportsFormatCurveHistogram(&out, list, 2);

	assert_string_equal(DynStringToCStr(&out),
	                    "  < 12in        3\n"
	                    "  12-18in       5\n");
	DynStringFree(&out);
}

static void test_carsperfoot_known_scales(void **state)
{
	(void) state;
	assert_true(ReportsCarsPerFoot("HO") == 2.0);
	assert_true(ReportsCarsPerFoot("N") == 4.0);
	assert_true(ReportsCarsPerFoot("O") == 1.0);
	assert_true(ReportsCarsPerFoot("Z") == 5.0);
	assert_true(ReportsCarsPerFoot("G") == 0.5);
}

static void test_carsperfoot_narrow_gauge(void **state)
{
	(void) state;
	assert_true(ReportsCarsPerFoot("HOn3") == 2.0);
	assert_true(ReportsCarsPerFoot("On3") == 1.0);
}

static void test_carsperfoot_case_insensitive(void **state)
{
	(void) state;
	assert_true(ReportsCarsPerFoot("ho") == 2.0);
	assert_true(ReportsCarsPerFoot("on3") == 1.0);
	assert_true(ReportsCarsPerFoot("HON3") == 2.0);
}

static void test_carsperfoot_unknown_defaults_2(void **state)
{
	(void) state;
	assert_true(ReportsCarsPerFoot("Q") == 2.0);
	assert_true(ReportsCarsPerFoot("") == 2.0);
}

static void test_carsperfoot_null_defaults_2(void **state)
{
	(void) state;
	assert_true(ReportsCarsPerFoot(NULL) == 2.0);
}

static void test_curvebucket_under_12(void **state)
{
	(void) state;
	assert_string_equal(ReportsCurveBucketLabel(0.0), "< 12in");
	assert_string_equal(ReportsCurveBucketLabel(11.99), "< 12in");
}

static void test_curvebucket_boundaries_exclusive_upper(void **state)
{
	(void) state;
	assert_string_equal(ReportsCurveBucketLabel(12.0), "12-18in");
	assert_string_equal(ReportsCurveBucketLabel(17.99), "12-18in");
	assert_string_equal(ReportsCurveBucketLabel(18.0), "18-24in");
	assert_string_equal(ReportsCurveBucketLabel(23.99), "18-24in");
	assert_string_equal(ReportsCurveBucketLabel(24.0), "24-36in");
	assert_string_equal(ReportsCurveBucketLabel(35.99), "24-36in");
	assert_string_equal(ReportsCurveBucketLabel(36.0), "> 36in");
}

static void test_curvebucket_large_radius(void **state)
{
	(void) state;
	assert_string_equal(ReportsCurveBucketLabel(1000.0), "> 36in");
}

static void test_turnout_empty_list(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 16);

	ReportsFormatTurnoutList(&out, NULL, 0);

	assert_string_equal(DynStringToCStr(&out), "");
	DynStringFree(&out);
}

static void test_turnout_normal_no_flag(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 64);
	reportsTurnoutLayer_t list[1] = {
		{ 0, "Main", 120.0, 3, 2.5, 0 }
	};

	ReportsFormatTurnoutList(&out, list, 1);

	/* No trailing whitespace when not heavy -- 2026-09-04 redesign fixed
	 * this (the original byte-parity port left "  " before an empty
	 * flag field on every non-flagged row). */
	assert_string_equal(DynStringToCStr(&out),
	                    "    0  Main                 120.0      3             2.5\n");
	DynStringFree(&out);
}

static void test_turnout_heavy_flagged(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 64);
	reportsTurnoutLayer_t list[1] = {
		{ 1, "Yard", 50.0, 6, 12.0, 1 }
	};

	ReportsFormatTurnoutList(&out, list, 1);

	assert_string_equal(DynStringToCStr(&out),
	                    "    1  Yard                  50.0      6            12.0  HEAVY\n");
	DynStringFree(&out);
}

/* A layer with turnouts but zero total length -- confirms the formatter
 * renders a precomputed 0.0 density cleanly. Computing that 0.0 (instead
 * of dividing by zero) is the compute pass's job, matching the MCP
 * reference's `if ft > 0 else 0.0` guard -- not exercised by this pure
 * formatter test, only its rendering. */
static void test_turnout_zero_feet_layer(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 64);
	reportsTurnoutLayer_t list[1] = {
		{ 2, "Empty", 0.0, 1, 0.0, 0 }
	};

	ReportsFormatTurnoutList(&out, list, 1);

	assert_string_equal(DynStringToCStr(&out),
	                    "    2  Empty                  0.0      1             0.0\n");
	DynStringFree(&out);
}

/* Three layers passed in ascending-layer-index order; expected output is
 * density-descending (Yard 12.0 > Boundary 10.0 > Main 2.5), confirming
 * the formatter itself sorts rather than relying on the caller. */
static void test_turnout_sorted_by_density_descending(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 192);
	reportsTurnoutLayer_t list[3] = {
		{ 0, "Main", 120.0, 3, 2.5, 0 },
		{ 1, "Yard", 50.0, 6, 12.0, 1 },
		{ 3, "Boundary", 100.0, 10, 10.0, 0 }
	};

	ReportsFormatTurnoutList(&out, list, 3);

	assert_string_equal(DynStringToCStr(&out),
	                    "    1  Yard                  50.0      6            12.0  HEAVY\n"
	                    "    3  Boundary             100.0     10            10.0\n"
	                    "    0  Main                 120.0      3             2.5\n");
	DynStringFree(&out);
}

/* Equal-density layers keep the caller's original relative order
 * (stable sort) -- A before B here, both density 5.0. */
static void test_turnout_sort_stable_on_ties(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 128);
	reportsTurnoutLayer_t list[2] = {
		{ 5, "A", 10.0, 1, 5.0, 0 },
		{ 6, "B", 20.0, 2, 5.0, 0 }
	};

	ReportsFormatTurnoutList(&out, list, 2);

	assert_string_equal(DynStringToCStr(&out),
	                    "    5  A                     10.0      1             5.0\n"
	                    "    6  B                     20.0      2             5.0\n");
	DynStringFree(&out);
}

/* Strict >, matching write_turnout_report()'s `density > 10.0` exactly --
 * exactly 10.0 must NOT be flagged heavy. Easy off-by-one porting Python's
 * `>` to C, hence pinning both sides of the boundary explicitly. */
static void test_turnout_heavy_boundary_at_10_not_heavy(void **state)
{
	(void) state;
	assert_int_equal(ReportsIsTurnoutHeavy(10.0), 0);
}

static void test_turnout_heavy_boundary_just_over_10(void **state)
{
	(void) state;
	assert_int_equal(ReportsIsTurnoutHeavy(10.1), 1);
}

static void test_turnout_heavy_well_under_10(void **state)
{
	(void) state;
	assert_int_equal(ReportsIsTurnoutHeavy(2.5), 0);
}

static void test_partial_turnouts_empty(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 16);

	ReportsFormatPartialTurnoutList(&out, NULL, 0);

	assert_string_equal(DynStringToCStr(&out), "");
	DynStringFree(&out);
}

static void test_partial_turnouts_row(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 32);
	reportsPartialTurnout_t list[1] = {
		{ 42, 2, 1 }
	};

	ReportsFormatPartialTurnoutList(&out, list, 1);

	assert_string_equal(DynStringToCStr(&out),
	                    "  ID 42: 2 endpoint(s), layer 1\n");
	DynStringFree(&out);
}

static void test_equipment_empty_list(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 16);

	ReportsFormatEquipmentList(&out, NULL, 0);

	assert_string_equal(DynStringToCStr(&out), "");
	DynStringFree(&out);
}

/* Input list deliberately in FAIL/PASS/MARGINAL order -- the formatter
 * must regroup into PASS, then MARGINAL, then FAIL regardless of input
 * order, with a blank line between each non-empty group and none
 * trailing the last one. */
static void test_equipment_all_three_groups(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 256);
	reportsEquipClass_t list[3] = {
		{ "Long passenger car (Superliner)", 28.0, REPORTS_EQUIP_FAIL },
		{ "Short freight car (< 40 ft)", 15.0, REPORTS_EQUIP_PASS },
		{ "Standard freight car (40-50 ft)", 18.0, REPORTS_EQUIP_MARGINAL }
	};

	ReportsFormatEquipmentList(&out, list, 3);

	assert_string_equal(DynStringToCStr(&out),
	                    "PASS\n"
	                    "  Short freight car (< 40 ft)                       15.0\"\n"
	                    "\n"
	                    "MARGINAL\n"
	                    "  Standard freight car (40-50 ft)                   18.0\"\n"
	                    "\n"
	                    "FAIL\n"
	                    "  Long passenger car (Superliner)                   28.0\"\n");
	DynStringFree(&out);
}

/* MARGINAL absent entirely -- no heading for it, and exactly one blank
 * line separates the two groups that do appear. */
static void test_equipment_skips_absent_group(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 128);
	reportsEquipClass_t list[2] = {
		{ "Short freight car (< 40 ft)", 15.0, REPORTS_EQUIP_PASS },
		{ "Long passenger car (Superliner)", 28.0, REPORTS_EQUIP_FAIL }
	};

	ReportsFormatEquipmentList(&out, list, 2);

	assert_string_equal(DynStringToCStr(&out),
	                    "PASS\n"
	                    "  Short freight car (< 40 ft)                       15.0\"\n"
	                    "\n"
	                    "FAIL\n"
	                    "  Long passenger car (Superliner)                   28.0\"\n");
	DynStringFree(&out);
}

/* Both rows the same status -- one heading, both rows underneath in the
 * caller's original relative order. */
static void test_equipment_group_preserves_row_order(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 128);
	reportsEquipClass_t list[2] = {
		{ "Long passenger car (Superliner)", 28.0, REPORTS_EQUIP_FAIL },
		{ "Medium steam (4-6-2, 2-8-2, 4-6-4)", 22.0, REPORTS_EQUIP_FAIL }
	};

	ReportsFormatEquipmentList(&out, list, 2);

	assert_string_equal(DynStringToCStr(&out),
	                    "FAIL\n"
	                    "  Long passenger car (Superliner)                   28.0\"\n"
	                    "  Medium steam (4-6-2, 2-8-2, 4-6-4)                22.0\"\n");
	DynStringFree(&out);
}

/* HO reference, scaleFactor = 1.0. Three explicit boundary tests rather
 * than one spot check -- this exact PASS/MARGINAL/FAIL boundary is the
 * easiest thing to get backwards porting Python's `>=` to C. */
static void test_classify_equipment_pass_at_threshold(void **state)
{
	(void) state;
	assert_int_equal(ReportsClassifyEquipment(15.0, 15.0, 1.0),
	                 REPORTS_EQUIP_PASS);
}

/* Exactly threshold - 2.0*scaleFactor -> MARGINAL, inclusive (`>=`),
 * matching write_equipment_report()'s `elif min_r >= thresh - 2.0*sf`. */
static void test_classify_equipment_marginal_at_boundary(void **state)
{
	(void) state;
	assert_int_equal(ReportsClassifyEquipment(13.0, 15.0, 1.0),
	                 REPORTS_EQUIP_MARGINAL);
}

static void test_classify_equipment_fail_just_under_margin(void **state)
{
	(void) state;
	assert_int_equal(ReportsClassifyEquipment(12.9, 15.0, 1.0),
	                 REPORTS_EQUIP_FAIL);
}

/* N scale (ratio 160): confirms the 2" MARGINAL margin is itself
 * scale-adjusted (thresh - 2.0*scaleFactor), not a flat 2.0" -- exactly
 * the bug this function's separate scaleFactor parameter exists to
 * prevent (see its doc comment in reports.h). sf = 87.1/160 = 0.544375,
 * thresh = 15.0*sf = 8.165625, margin = thresh - 2.0*sf = 7.076875. */
static void test_classify_equipment_scaled_margin(void **state)
{
	(void) state;
	double sf = 87.1 / 160.0;
	double thresh = 15.0 * sf;

	assert_int_equal(ReportsClassifyEquipment(thresh - 2.0 * sf, thresh, sf),
	                 REPORTS_EQUIP_MARGINAL);
	assert_int_equal(ReportsClassifyEquipment(thresh - 2.0 * sf - 0.01, thresh, sf),
	                 REPORTS_EQUIP_FAIL);
}

/* Phase 3/3a batch (Gaps, Kinked Joints) -- SF #217/#779. */

static void test_gap_empty_list(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 16);

	ReportsFormatGapList(&out, NULL, 0);

	assert_string_equal(DynStringToCStr(&out), "");
	DynStringFree(&out);
}

static void test_gap_single_row(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 32);
	reportsGapPair_t list[1] = {
		{ 3, { 0.0, 0.0 }, 7, { 0.08, 0.0 }, 0.08, 0, 1, "Main", 1, "Main" }
	};

	ReportsFormatGapList(&out, list, 1);

	assert_string_equal(DynStringToCStr(&out),
	                    "     3 |     1 | Main           |      7 |     1 | Main           |   0.0800\n");
	DynStringFree(&out);
}

/* Different layers on each side -- the case this column exists for: a
 * multi-level layout can have two endpoints near in shared XY but on
 * different physical levels, so both sides' own layer must be visible. */
static void test_gap_multiple_rows(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 128);
	reportsGapPair_t list[2] = {
		{ 3,   { 0.0, 0.0 }, 7,   { 0.08, 0.0 }, 0.08,   0, 1, "Main", 1, "Main" },
		{ 12,  { 0.0, 0.0 }, 145, { 0.0,  0.0 }, 0.0523, 0, 2, "Yard", 3, "Staging" }
	};

	ReportsFormatGapList(&out, list, 2);

	assert_string_equal(DynStringToCStr(&out),
	                    "     3 |     1 | Main           |      7 |     1 | Main           |   0.0800\n"
	                    "    12 |     2 | Yard           |    145 |     3 | Staging        |   0.0523\n");
	DynStringFree(&out);
}

static void test_kinked_empty_list(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 16);

	ReportsFormatKinkedList(&out, NULL, 0);

	assert_string_equal(DynStringToCStr(&out), "");
	DynStringFree(&out);
}

static void test_kinked_single_row(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 32);
	reportsKinkedJoint_t list[1] = {
		{ 4, 9, { 0.0, 0.0 }, 3.5, 0, 1, "Main", 1, "Main" }
	};

	ReportsFormatKinkedList(&out, list, 1);

	assert_string_equal(DynStringToCStr(&out),
	                    "     4 |     1 | Main           |      9 |     1 | Main           |   3.500\n");
	DynStringFree(&out);
}

static void test_kinked_multiple_rows(void **state)
{
	(void) state;
	DynString out;
	DynStringMalloc(&out, 128);
	reportsKinkedJoint_t list[2] = {
		{ 4,   9,   { 0.0, 0.0 }, 3.5,   0, 1, "Main", 1, "Main" },
		{ 100, 250, { 0.0, 0.0 }, 12.75, 0, 2, "Yard", 5, "Upper Deck" }
	};

	ReportsFormatKinkedList(&out, list, 2);

	assert_string_equal(DynStringToCStr(&out),
	                    "     4 |     1 | Main           |      9 |     1 | Main           |   3.500\n"
	                    "   100 |     2 | Yard           |    250 |     5 | Upper Deck     |  12.750\n");
	DynStringFree(&out);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_empty_list),
		cmocka_unit_test(test_single_row),
		cmocka_unit_test(test_fixture_shaped_list),
		cmocka_unit_test(test_negative_and_fractional_values),
		cmocka_unit_test(test_appends_not_overwrites),
		cmocka_unit_test(test_note_none_set),
		cmocka_unit_test(test_note_nearby_only),
		cmocka_unit_test(test_note_isolated_only),
		cmocka_unit_test(test_note_turntable_only),
		cmocka_unit_test(test_note_isolated_and_nearby),
		cmocka_unit_test(test_note_turntable_and_nearby),
		cmocka_unit_test(test_tracklen_empty_list),
		cmocka_unit_test(test_tracklen_single_row_full_pct),
		cmocka_unit_test(test_tracklen_multi_layer_pct),
		cmocka_unit_test(test_tracklen_zero_total_no_crash),
		cmocka_unit_test(test_curvehist_empty),
		cmocka_unit_test(test_curvehist_buckets),
		cmocka_unit_test(test_carsperfoot_known_scales),
		cmocka_unit_test(test_carsperfoot_narrow_gauge),
		cmocka_unit_test(test_carsperfoot_case_insensitive),
		cmocka_unit_test(test_carsperfoot_unknown_defaults_2),
		cmocka_unit_test(test_carsperfoot_null_defaults_2),
		cmocka_unit_test(test_curvebucket_under_12),
		cmocka_unit_test(test_curvebucket_boundaries_exclusive_upper),
		cmocka_unit_test(test_curvebucket_large_radius),
		cmocka_unit_test(test_turnout_empty_list),
		cmocka_unit_test(test_turnout_normal_no_flag),
		cmocka_unit_test(test_turnout_heavy_flagged),
		cmocka_unit_test(test_turnout_zero_feet_layer),
		cmocka_unit_test(test_turnout_sorted_by_density_descending),
		cmocka_unit_test(test_turnout_sort_stable_on_ties),
		cmocka_unit_test(test_turnout_heavy_boundary_at_10_not_heavy),
		cmocka_unit_test(test_turnout_heavy_boundary_just_over_10),
		cmocka_unit_test(test_turnout_heavy_well_under_10),
		cmocka_unit_test(test_partial_turnouts_empty),
		cmocka_unit_test(test_partial_turnouts_row),
		cmocka_unit_test(test_equipment_empty_list),
		cmocka_unit_test(test_equipment_all_three_groups),
		cmocka_unit_test(test_equipment_skips_absent_group),
		cmocka_unit_test(test_equipment_group_preserves_row_order),
		cmocka_unit_test(test_classify_equipment_pass_at_threshold),
		cmocka_unit_test(test_classify_equipment_marginal_at_boundary),
		cmocka_unit_test(test_classify_equipment_fail_just_under_margin),
		cmocka_unit_test(test_classify_equipment_scaled_margin),
		cmocka_unit_test(test_gap_empty_list),
		cmocka_unit_test(test_gap_single_row),
		cmocka_unit_test(test_gap_multiple_rows),
		cmocka_unit_test(test_kinked_empty_list),
		cmocka_unit_test(test_kinked_single_row),
		cmocka_unit_test(test_kinked_multiple_rows),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
