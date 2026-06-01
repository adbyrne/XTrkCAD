/** \file cornutest.c
 * Unit tests for the pure-math functions in ccornu.c.
 *
 * ccornu.c is coupled to the UI subsystem, so the five mathematical functions
 * tested here are copied verbatim from ccornu.c with source-line anchors.
 * When ccornu.c changes, verify those line numbers still match and update
 * these copies accordingly.
 *
 * Note: CornuLength, CornuOffsetLength, and CornuMinRadius have identical
 * implementations to their BezierXxx counterparts in cbezier.c.  The tests
 * here are intentionally brief smoke-tests; see beziertest.c for thorough
 * coverage of that logic.
 */

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>

#include "common.h"
#include "trkseg.h"
#include "../utility.c"    /* FindDistance, D2R, etc. */

#define NEARLY_EQ(a, b)  (fabs((a) - (b)) < 1e-9)
#define ASSERT_DBL(actual, expected) \
	assert_true(NEARLY_EQ((actual), (expected)))

/* -----------------------------------------------------------------------
 * SegProc stub — only SEGPROC_LENGTH is used by the functions under test
 * ----------------------------------------------------------------------- */

void SegProc(segProc_e op, trkSeg_p s, segProcData_p data)
{
	if (op != SEGPROC_LENGTH) { return; }
	if (s->type == SEG_CRVTRK || s->type == SEG_CRVLIN) {
		data->length.length = fabs(s->u.c.radius * D2R(s->u.c.a1));
	} else if (s->type == SEG_STRTRK || s->type == SEG_STRLIN) {
		data->length.length = FindDistance(s->u.l.pos[0], s->u.l.pos[1]);
	} else {
		data->length.length = 0.0;
	}
}

/* -----------------------------------------------------------------------
 * Functions copied from ccornu.c (update when originals change)
 * ----------------------------------------------------------------------- */

/* ccornu.c:2260 */
static DIST_T CornuLength(coOrd pos[4], dynArr_t segs)
{
	DIST_T dd = 0.0;
	if (segs.cnt == 0) { return dd; }
	for (int i = 0; i < segs.cnt; i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t, segs, i);
		if (t.type == SEG_CRVTRK || t.type == SEG_CRVLIN) {
			dd += fabs(t.u.c.radius * D2R(t.u.c.a1));
		} else if (t.type == SEG_BEZLIN || t.type == SEG_BEZTRK) {
			dd += CornuLength(t.u.b.pos, t.bezSegs);
		} else if (t.type == SEG_STRLIN || t.type == SEG_STRTRK) {
			dd += FindDistance(t.u.l.pos[0], t.u.l.pos[1]);
		}
	}
	return dd;
}

/* ccornu.c:2278 */
static DIST_T CornuOffsetLength(dynArr_t segs, double offset)
{
	DIST_T dd = 0.0;
	if (segs.cnt == 0) { return dd; }
	for (int i = 0; i < segs.cnt; i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t, segs, i);
		if (t.type == SEG_CRVTRK || t.type == SEG_CRVLIN) {
			dd += fabs((t.u.c.radius+(t.u.c.radius>0?offset:-offset))*D2R(t.u.c.a1));
		} else if (t.type == SEG_BEZLIN || t.type == SEG_BEZTRK) {
			dd += CornuOffsetLength(t.bezSegs, offset);
		} else if (t.type == SEG_STRLIN || t.type == SEG_STRTRK) {
			dd += FindDistance(t.u.l.pos[0], t.u.l.pos[1]);
		}
	}
	return dd;
}

/* ccornu.c:2295 */
static DIST_T CornuMinRadius(coOrd pos[4], dynArr_t segs)
{
	DIST_T r = DIST_INF, rr;
	if (segs.cnt == 0) { return r; }
	for (int i = 0; i < segs.cnt; i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t, segs, i);
		if (t.type == SEG_CRVTRK || t.type == SEG_CRVLIN) {
			rr = fabs(t.u.c.radius);
		} else if (t.type == SEG_BEZLIN || t.type == SEG_BEZTRK) {
			rr = CornuMinRadius(t.u.b.pos, t.bezSegs);
		} else { rr = DIST_INF; }
		if (rr < r) { r = rr; }
	}
	return r;
}

/* ccornu.c:2311 */
static DIST_T CornuTotalWindingArc(coOrd pos[4], dynArr_t segs)
{
	DIST_T rr = 0;
	if (segs.cnt == 0) { return 0; }
	for (int i = 0; i < segs.cnt; i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t, segs, i);
		if (t.type == SEG_CRVTRK || t.type == SEG_CRVLIN) {
			rr += t.u.c.a1;
		} else if (t.type == SEG_BEZLIN || t.type == SEG_BEZTRK) {
			rr += CornuTotalWindingArc(t.u.b.pos, t.bezSegs);
		}
	}
	return rr;
}

/* ccornu.c:2326 */
static DIST_T CornuMaxRateofChangeofCurvature(coOrd pos[4], dynArr_t segs,
                                              DIST_T *last_c)
{
	DIST_T r_max = 0.0, rc, lc = 0.0;
	lc = *last_c;
	segProcData_t segProcData;
	if (segs.cnt == 0) { return r_max; }
	for (int i = 0; i < segs.cnt; i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t, segs, i);
		if (t.type == SEG_FILCRCL) { continue; }
		SegProc(SEGPROC_LENGTH, &t, &segProcData);
		if (t.type == SEG_CRVTRK || t.type == SEG_CRVLIN) {
			rc = fabs(1/fabs(t.u.c.radius) - lc) / segProcData.length.length / 2;
			lc = 1/fabs(t.u.c.radius);
		} else if (t.type == SEG_BEZLIN || t.type == SEG_BEZTRK) {
			rc = CornuMaxRateofChangeofCurvature(t.u.b.pos, t.bezSegs, &lc);
		} else {
			rc = fabs(0.0 - lc) / segProcData.length.length / 2;
			lc = 0.0;
		}
		if (rc > r_max) { r_max = rc; }
	}
	*last_c = lc;
	return r_max;
}

/* -----------------------------------------------------------------------
 * Helpers for building fixed-size segment arrays on the stack
 * ----------------------------------------------------------------------- */

#define SEGS_FROM_ARRAY(arr) \
	((dynArr_t){ .cnt = (int)(sizeof(arr)/sizeof(arr[0])), \
	             .max = (int)(sizeof(arr)/sizeof(arr[0])), \
	             .ptr = (arr) })

static trkSeg_t make_arc(double radius, double a1)
{
	trkSeg_t s;
	memset(&s, 0, sizeof s);
	s.type = SEG_CRVLIN;
	s.u.c.radius = radius;
	s.u.c.a1     = a1;
	return s;
}

static trkSeg_t make_str(double x0, double y0, double x1, double y1)
{
	trkSeg_t s;
	memset(&s, 0, sizeof s);
	s.type          = SEG_STRLIN;
	s.u.l.pos[0].x  = x0; s.u.l.pos[0].y = y0;
	s.u.l.pos[1].x  = x1; s.u.l.pos[1].y = y1;
	return s;
}

static trkSeg_t make_filcrcl(void)
{
	trkSeg_t s;
	memset(&s, 0, sizeof s);
	s.type = SEG_FILCRCL;
	return s;
}

/* -----------------------------------------------------------------------
 * CornuLength, CornuOffsetLength, CornuMinRadius
 * (logic identical to BezierLength/OffsetLength/MinRadius — smoke tests only)
 * ----------------------------------------------------------------------- */

static void CornuLengthSmokeTests(void **state)
{
	(void)state;
	coOrd dummy[4] = { {0} };
	dynArr_t empty = {0};

	ASSERT_DBL(CornuLength(dummy, empty), 0.0);

	trkSeg_t arc_segs[1] = { make_arc(10.0, 90.0) };
	dynArr_t arc_da = SEGS_FROM_ARRAY(arc_segs);
	ASSERT_DBL(CornuLength(dummy, arc_da), 10.0 * M_PI / 2.0);

	trkSeg_t mix[2] = { make_arc(10.0, 90.0), make_str(0,0,0,5) };
	dynArr_t mix_da = SEGS_FROM_ARRAY(mix);
	ASSERT_DBL(CornuLength(dummy, mix_da), 10.0 * M_PI / 2.0 + 5.0);
}

static void CornuOffsetLengthSmokeTests(void **state)
{
	(void)state;
	dynArr_t empty = {0};

	ASSERT_DBL(CornuOffsetLength(empty, 2.0), 0.0);

	trkSeg_t arc_segs[1] = { make_arc(10.0, 90.0) };
	dynArr_t arc_da = SEGS_FROM_ARRAY(arc_segs);
	ASSERT_DBL(CornuOffsetLength(arc_da, 2.0), 12.0 * M_PI / 2.0);
}

static void CornuMinRadiusSmokeTests(void **state)
{
	(void)state;
	coOrd dummy[4] = { {0} };
	dynArr_t empty = {0};

	assert_true(CornuMinRadius(dummy, empty) >= DIST_INF);

	trkSeg_t arc_segs[1] = { make_arc(7.5, 90.0) };
	dynArr_t arc_da = SEGS_FROM_ARRAY(arc_segs);
	ASSERT_DBL(CornuMinRadius(dummy, arc_da), 7.5);
}

/* -----------------------------------------------------------------------
 * CornuTotalWindingArc — sums angular extents of arc segments
 * ----------------------------------------------------------------------- */

static void CornuTotalWindingArcTests(void **state)
{
	(void)state;
	coOrd dummy[4] = { {0} };
	dynArr_t empty = {0};

	/* Empty → 0 */
	ASSERT_DBL(CornuTotalWindingArc(dummy, empty), 0.0);

	/* Single arc */
	trkSeg_t one_arc[1] = { make_arc(10.0, 90.0) };
	dynArr_t one_da = SEGS_FROM_ARRAY(one_arc);
	ASSERT_DBL(CornuTotalWindingArc(dummy, one_da), 90.0);

	/* Two arcs — angular extents accumulate */
	trkSeg_t two_arcs[2] = { make_arc(10.0, 90.0), make_arc(5.0, 45.0) };
	dynArr_t two_da = SEGS_FROM_ARRAY(two_arcs);
	ASSERT_DBL(CornuTotalWindingArc(dummy, two_da), 135.0);

	/* Straight segments do not contribute */
	trkSeg_t str_only[1] = { make_str(0,0,10,0) };
	dynArr_t str_da = SEGS_FROM_ARRAY(str_only);
	ASSERT_DBL(CornuTotalWindingArc(dummy, str_da), 0.0);

	/* Arc + straight: only arc counts */
	trkSeg_t arc_str[2] = { make_arc(10.0, 60.0), make_str(0,0,5,0) };
	dynArr_t arc_str_da = SEGS_FROM_ARRAY(arc_str);
	ASSERT_DBL(CornuTotalWindingArc(dummy, arc_str_da), 60.0);

	/* SEG_FILCRCL is not an arc — not counted */
	trkSeg_t with_fil[2] = { make_filcrcl(), make_arc(10.0, 30.0) };
	dynArr_t fil_da = SEGS_FROM_ARRAY(with_fil);
	ASSERT_DBL(CornuTotalWindingArc(dummy, fil_da), 30.0);
}

/* -----------------------------------------------------------------------
 * CornuMaxRateofChangeofCurvature
 *
 * Formula for a single arc segment:
 *   length = |radius * D2R(a1)|
 *   rc     = |1/radius - lc_before| / length / 2
 *   lc_after = 1/radius
 *
 * Formula for a straight segment:
 *   rc     = |0 - lc_before| / length / 2
 *   lc_after = 0
 * ----------------------------------------------------------------------- */

static void CornuMaxRCCEmpty(void **state)
{
	(void)state;
	coOrd dummy[4] = { {0} };
	dynArr_t empty = {0};
	DIST_T lc = 0.0;
	ASSERT_DBL(CornuMaxRateofChangeofCurvature(dummy, empty, &lc), 0.0);
}

static void CornuMaxRCCSingleArc(void **state)
{
	(void)state;
	/* Single arc r=10, a1=90°, starting from zero curvature.
	 * length = 10 * π/2 = 5π
	 * rc     = (1/10 − 0) / (5π) / 2 = 0.1/(10π) = 1/(100π) */
	trkSeg_t segs[1] = { make_arc(10.0, 90.0) };
	dynArr_t da = SEGS_FROM_ARRAY(segs);
	coOrd dummy[4] = { {0} };
	DIST_T lc = 0.0;
	double length   = 10.0 * M_PI / 2.0;
	double expected = (1.0/10.0) / length / 2.0;
	ASSERT_DBL(CornuMaxRateofChangeofCurvature(dummy, da, &lc), expected);
	/* last_c is updated to the final curvature */
	ASSERT_DBL(lc, 1.0/10.0);
}

static void CornuMaxRCCTwoArcs(void **state)
{
	(void)state;
	/* Arc 1: r=10, a1=90°.  Arc 2: r=5, a1=90°.
	 *
	 * Arc 1 (lc=0):
	 *   length1 = 5π
	 *   rc1 = (1/10 − 0) / (5π) / 2 = 1/(100π)   lc→0.1
	 *
	 * Arc 2 (lc=0.1):
	 *   length2 = 5*π/2 = 2.5π
	 *   rc2 = |1/5 − 0.1| / (2.5π) / 2 = 0.1/(5π) = 1/(50π)   lc→0.2
	 *
	 * max = rc2 = 1/(50π) */
	trkSeg_t segs[2] = { make_arc(10.0, 90.0), make_arc(5.0, 90.0) };
	dynArr_t da = SEGS_FROM_ARRAY(segs);
	coOrd dummy[4] = { {0} };
	DIST_T lc = 0.0;
	double length2   = 5.0 * M_PI / 2.0;
	double expected  = 0.1 / length2 / 2.0;
	ASSERT_DBL(CornuMaxRateofChangeofCurvature(dummy, da, &lc), expected);
	ASSERT_DBL(lc, 1.0/5.0);
}

static void CornuMaxRCCArcThenStraight(void **state)
{
	(void)state;
	/* Arc r=10 a1=90° then straight length=10.
	 *
	 * Arc (lc=0):
	 *   length=5π,  rc = (1/10) / (5π) / 2 = 1/(100π) ≈ 0.00318   lc→0.1
	 *
	 * Straight (lc=0.1):
	 *   rc = 0.1 / 10 / 2 = 0.005   lc→0
	 *
	 * max = 0.005 (straight has higher rate of change) */
	trkSeg_t segs[2] = { make_arc(10.0, 90.0), make_str(0,0,0,10) };
	dynArr_t da = SEGS_FROM_ARRAY(segs);
	coOrd dummy[4] = { {0} };
	DIST_T lc = 0.0;
	ASSERT_DBL(CornuMaxRateofChangeofCurvature(dummy, da, &lc), 0.005);
	ASSERT_DBL(lc, 0.0);
}

static void CornuMaxRCCFilcrclSkipped(void **state)
{
	(void)state;
	/* A SEG_FILCRCL decorating segment must be silently skipped.
	 * Straight length=10 with lc=0.2 coming from outside.
	 *   rc = 0.2 / 10 / 2 = 0.01 */
	trkSeg_t segs[2] = { make_filcrcl(), make_str(0,0,0,10) };
	dynArr_t da = SEGS_FROM_ARRAY(segs);
	coOrd dummy[4] = { {0} };
	DIST_T lc = 0.2;
	ASSERT_DBL(CornuMaxRateofChangeofCurvature(dummy, da, &lc), 0.01);
	ASSERT_DBL(lc, 0.0);
}

static void CornuMaxRCCLastCPropagated(void **state)
{
	(void)state;
	/* Verify last_c is correctly threaded across two calls, simulating
	 * how the caller walks a sequence of segments. */
	trkSeg_t first[1] = { make_arc(10.0, 90.0) };
	dynArr_t da1 = SEGS_FROM_ARRAY(first);
	coOrd dummy[4] = { {0} };
	DIST_T lc = 0.0;
	CornuMaxRateofChangeofCurvature(dummy, da1, &lc);
	ASSERT_DBL(lc, 1.0/10.0);   /* curvature after a r=10 arc */

	trkSeg_t second[1] = { make_arc(20.0, 90.0) };
	dynArr_t da2 = SEGS_FROM_ARRAY(second);
	/* second call picks up where the first left off */
	double length2   = 20.0 * M_PI / 2.0;
	double expected  = fabs(1.0/20.0 - lc) / length2 / 2.0;
	ASSERT_DBL(CornuMaxRateofChangeofCurvature(dummy, da2, &lc), expected);
	ASSERT_DBL(lc, 1.0/20.0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(CornuLengthSmokeTests),
		cmocka_unit_test(CornuOffsetLengthSmokeTests),
		cmocka_unit_test(CornuMinRadiusSmokeTests),
		cmocka_unit_test(CornuTotalWindingArcTests),
		cmocka_unit_test(CornuMaxRCCEmpty),
		cmocka_unit_test(CornuMaxRCCSingleArc),
		cmocka_unit_test(CornuMaxRCCTwoArcs),
		cmocka_unit_test(CornuMaxRCCArcThenStraight),
		cmocka_unit_test(CornuMaxRCCFilcrclSkipped),
		cmocka_unit_test(CornuMaxRCCLastCPropagated),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
