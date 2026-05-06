/** \file beziertest.c
 * Unit tests for the pure-math functions in cbezier.c.
 *
 * cbezier.c is coupled to the UI subsystem (PlotCurve, DrawSegs, tempD, …),
 * so the file cannot be included wholesale.  The eight mathematical functions
 * tested here are copied verbatim from cbezier.c with source-line anchors in
 * the comments.  When cbezier.c changes, verify those line numbers still match
 * and update these copies accordingly.
 */

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>

#include "common.h"
#include "trkseg.h"
#include "../utility.c"    /* FindDistance, D2R, NormalizeAngle, etc. */

#define NEARLY_EQ(a, b)  (fabs((a) - (b)) < 1e-4)
#define ASSERT_DBL(actual, expected) \
	assert_true(NEARLY_EQ((actual), (expected)))
#define ASSERT_COORD(p, ex, ey) \
	do { ASSERT_DBL((p).x, (ex)); ASSERT_DBL((p).y, (ey)); } while (0)

/* -----------------------------------------------------------------------
 * Types and stubs needed by the copied functions
 * ----------------------------------------------------------------------- */

/* BezierType enum — mirrors cbezier.c:272 */
enum BezierType { PLAIN, LOOP, CUSP, INFLECTION, DOUBLEINFLECTION,
                  LINE, ENDS, COINCIDENT };

/* Minimal Da struct — AnalyseCurve only reads Da.track (cbezier.c:287) */
static struct { BOOL_T track; } Da;

/* -----------------------------------------------------------------------
 * Functions copied from cbezier.c (update when originals change)
 * ----------------------------------------------------------------------- */

/* cbezier.c:154 */
static coOrd getPoint(coOrd pos[4], double s)
{
	double mt = 1-s;
	double a = mt*mt*mt;
	double b = mt*mt*s*3;
	double c = mt*s*s*3;
	double d = s*s*s;
	coOrd ret;
	ret.x = a*pos[0].x + b*pos[1].x + c*pos[2].x + d*pos[3].x;
	ret.y = a*pos[0].y + b*pos[1].y + c*pos[2].y + d*pos[3].y;
	return ret;
}

/* cbezier.c:191 */
static double DistanceToLineSegment(coOrd p, coOrd l1, coOrd l2)
{
	double A = p.x - l1.x;
	double B = p.y - l1.y;
	double C = l2.x - l1.x;
	double D = l2.y - l1.y;

	double dot = A*C + B*D;
	double len_sq = C*C + D*D;
	double param = -1;
	if (len_sq != 0) {
		param = dot / len_sq;
	}
	double xx, yy;
	if (param < 0) {
		xx = l1.x; yy = l1.y;
	} else if (param > 1) {
		xx = l2.x; yy = l2.y;
	} else {
		xx = l1.x + param*C;
		yy = l1.y + param*D;
	}
	double dx = p.x - xx;
	double dy = p.y - yy;
	return sqrt(dx*dx + dy*dy);
}

/* cbezier.c:174 */
static double BezError(coOrd pos[4], coOrd center, coOrd start_point,
                       double start, double end)
{
	double quarter = (end - start) / 4;
	coOrd c1 = getPoint(pos, start + quarter);
	coOrd c2 = getPoint(pos, end   - quarter);
	double ref = FindDistance(center, start_point);
	double d1  = FindDistance(center, c1);
	double d2  = FindDistance(center, c2);
	return fabs(d1-ref) + fabs(d2-ref);
}

/* cbezier.c:228 */
static double BezErrorLine(coOrd pos[4], coOrd start_point, coOrd end_point,
                           double start, double end)
{
	double quarter = (end - start) / 4;
	coOrd c1 = getPoint(pos, start + quarter);
	coOrd c2 = getPoint(pos, end   - quarter);
	double d1 = DistanceToLineSegment(c1, start_point, end_point);
	double d2 = DistanceToLineSegment(c2, start_point, end_point);
	return fabs(d1) + fabs(d2);
}

/* cbezier.c:282 */
static enum BezierType AnalyseCurve(coOrd inpos[4], double *Rfx, double *Rfy,
                                    double *cusp)
{
	*Rfx = *Rfy = 0;
	if (Da.track && inpos[0].x == inpos[3].x && inpos[0].y == inpos[3].y) {
		return ENDS;
	}
	DIST_T d01 = FindDistance(inpos[0], inpos[1]);
	DIST_T d12 = FindDistance(inpos[1], inpos[2]);
	DIST_T d02 = FindDistance(inpos[0], inpos[2]);
	if (d01+d12 == d02) {
		DIST_T d23 = FindDistance(inpos[2], inpos[3]);
		DIST_T d03 = FindDistance(inpos[0], inpos[3]);
		if (d02+d23 == d03) { return LINE; }
	}
	int common_points = 0;
	for (int i=0; i<3; i++) {
		if (inpos[i].x == inpos[i+1].x && inpos[i].y == inpos[i+1].y) { common_points++; }
	}
	for (int i=0; i<2; i++) {
		if (inpos[i].x == inpos[i+2].x && inpos[i].y == inpos[i+2].y) { common_points++; }
	}
	if (common_points > 2) { return COINCIDENT; }

	coOrd pos[4];
	coOrd offset2, offset = inpos[0];
	for (int i=0; i<4; i++) {
		pos[i].x = inpos[i].x - offset.x;
		pos[i].y = inpos[i].y - offset.y;
	}
	offset2.x = -offset.x + pos[3].x;
	offset2.y = -offset.y + pos[3].y;
	if (pos[1].y == 0.0) {
		for (int i=0; i<4; i++) {
			coOrd temp_pos = pos[i];
			pos[i].x = pos[3-i].x - offset2.x;
			pos[i].y = pos[3-i].y - offset2.y;
			pos[3-i] = temp_pos;
		}
		if (pos[1].y == 0.0) { return PLAIN; }
	}
	double f21 = (pos[2].y) / (pos[1].y);
	double f31 = (pos[3].y) / (pos[1].y);
	if (fabs(pos[2].x - (pos[1].x*f21)) < 0.0001) { return PLAIN; }
	double fx = (pos[3].x - (pos[1].x*f31)) / (pos[2].x - (pos[1].x*f21));
	double fy = f31 + (1-f21)*fx;
	*Rfx = fx;
	*Rfy = fy;
	*cusp = fabs(fy - (-(fx*fx)+2*fx+3)/4);

	if (fy > 1.0) { return INFLECTION; }
	if (fx >= 1.0) { return PLAIN; }
	if (fabs(fy - (-(fx*fx)+2*fx+3)/4) < 0.100) { return CUSP; }
	if (fy < (-(fx*fx)+2*fx+3)/4) {
		if (fx <= 0.0 && fy >= (3*fx-(fx*fx))/3) { return LOOP; }
		if (fx > 0.0 && fy >= (sqrt(3*(4*fx-fx*fx))-fx)/2) { return LOOP; }
		return PLAIN;
	}
	return DOUBLEINFLECTION;
}

/* cbezier.c:977 */
static DIST_T BezierLength(coOrd pos[4], dynArr_t segs)
{
	DIST_T dd = 0.0;
	if (segs.cnt == 0) { return dd; }
	for (int i = 0; i < segs.cnt; i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t, segs, i);
		if (t.type == SEG_CRVTRK || t.type == SEG_CRVLIN) {
			dd += fabs(t.u.c.radius * D2R(t.u.c.a1));
		} else if (t.type == SEG_BEZLIN || t.type == SEG_BEZTRK) {
			dd += BezierLength(t.u.b.pos, t.bezSegs);
		} else if (t.type == SEG_STRLIN || t.type == SEG_STRTRK) {
			dd += FindDistance(t.u.l.pos[0], t.u.l.pos[1]);
		}
	}
	return dd;
}

/* cbezier.c:995 */
static DIST_T BezierOffsetLength(dynArr_t segs, double offset)
{
	DIST_T dd = 0.0;
	if (segs.cnt == 0) { return dd; }
	for (int i = 0; i < segs.cnt; i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t, segs, i);
		if (t.type == SEG_CRVTRK || t.type == SEG_CRVLIN) {
			dd += fabs((t.u.c.radius+(t.u.c.radius>0?offset:-offset))*D2R(t.u.c.a1));
		} else if (t.type == SEG_BEZLIN || t.type == SEG_BEZTRK) {
			dd += BezierOffsetLength(t.bezSegs, offset);
		} else if (t.type == SEG_STRLIN || t.type == SEG_STRTRK) {
			dd += FindDistance(t.u.l.pos[0], t.u.l.pos[1]);
		}
	}
	return dd;
}

/* cbezier.c:1013 */
static DIST_T BezierMinRadius(coOrd pos[4], dynArr_t segs)
{
	DIST_T r = DIST_INF, rr;
	if (segs.cnt == 0) { return r; }
	for (int i = 0; i < segs.cnt; i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t, segs, i);
		if (t.type == SEG_CRVTRK || t.type == SEG_CRVLIN) {
			rr = fabs(t.u.c.radius);
		} else if (t.type == SEG_BEZLIN || t.type == SEG_BEZTRK) {
			rr = BezierMinRadius(t.u.b.pos, t.bezSegs);
		} else { rr = DIST_INF; }
		if (rr < r) { r = rr; }
	}
	return r;
}

/* -----------------------------------------------------------------------
 * Helpers for building segment arrays without heap allocation
 * ----------------------------------------------------------------------- */

/* Build a dynArr_t pointing at a fixed-size stack array */
#define SEGS_FROM_ARRAY(arr) \
	((dynArr_t){ .cnt = (int)(sizeof(arr)/sizeof(arr[0])), \
	             .max = (int)(sizeof(arr)/sizeof(arr[0])), \
	             .ptr = (arr) })

static trkSeg_t make_arc_seg(double radius, double a1)
{
	trkSeg_t s;
	memset(&s, 0, sizeof s);
	s.type = SEG_CRVLIN;
	s.u.c.radius = radius;
	s.u.c.a1     = a1;
	return s;
}

static trkSeg_t make_str_seg(double x0, double y0, double x1, double y1)
{
	trkSeg_t s;
	memset(&s, 0, sizeof s);
	s.type        = SEG_STRLIN;
	s.u.l.pos[0].x = x0; s.u.l.pos[0].y = y0;
	s.u.l.pos[1].x = x1; s.u.l.pos[1].y = y1;
	return s;
}

/* -----------------------------------------------------------------------
 * getPoint — cubic Bezier evaluation  B(t) = Σ basis_i(t) · pos[i]
 * ----------------------------------------------------------------------- */

static void GetPointTests(void **state)
{
	(void)state;
	coOrd pos[4] = { {0,0}, {0,10}, {10,10}, {10,0} };
	coOrd p;

	/* t=0 must return the first endpoint */
	p = getPoint(pos, 0.0);
	ASSERT_COORD(p, 0.0, 0.0);

	/* t=1 must return the last endpoint */
	p = getPoint(pos, 1.0);
	ASSERT_COORD(p, 10.0, 0.0);

	/* t=0.5 on a straight-line Bezier returns the midpoint
	 * (collinear control points, uniform parameterisation) */
	coOrd line[4] = { {0,0}, {10.0/3,0}, {20.0/3,0}, {10,0} };
	p = getPoint(line, 0.5);
	ASSERT_COORD(p, 5.0, 0.0);

	/* t=0.5 on the C-curve above: known result (5, 7.5) */
	p = getPoint(pos, 0.5);
	ASSERT_COORD(p, 5.0, 7.5);
}

/* -----------------------------------------------------------------------
 * DistanceToLineSegment
 * ----------------------------------------------------------------------- */

static void DistanceToLineSegmentTests(void **state)
{
	(void)state;
	coOrd l1 = {0,0}, l2 = {10,0};
	coOrd p;

	/* Point on the segment */
	p.x = 5.0; p.y = 0.0;
	ASSERT_DBL(DistanceToLineSegment(p, l1, l2), 0.0);

	/* Point directly above midpoint */
	p.x = 5.0; p.y = 3.0;
	ASSERT_DBL(DistanceToLineSegment(p, l1, l2), 3.0);

	/* Point beyond the end — clamped to l2 */
	p.x = 15.0; p.y = 0.0;
	ASSERT_DBL(DistanceToLineSegment(p, l1, l2), 5.0);

	/* Point before the start — clamped to l1 */
	p.x = -3.0; p.y = 4.0;
	ASSERT_DBL(DistanceToLineSegment(p, l1, l2), 5.0);

	/* Zero-length segment: distance to that point */
	coOrd pt = {3,4};
	ASSERT_DBL(DistanceToLineSegment(pt, pt, pt), 0.0);
}

/* -----------------------------------------------------------------------
 * BezError — error between a Bezier and a proposed arc
 * ----------------------------------------------------------------------- */

static void BezErrorTests(void **state)
{
	(void)state;
	/* Straight-line Bezier: centre=(5,0), start=(0,0), radius=5.
	 * Quarter points land at (2.5,0) and (7.5,0).
	 * |dist - radius| = |2.5 - 5| + |7.5 - 5| = 5.0 */
	coOrd pos[4]   = { {0,0}, {10.0/3,0}, {20.0/3,0}, {10,0} };
	coOrd center   = {5,0};
	coOrd start_pt = {0,0};
	ASSERT_DBL(BezError(pos, center, start_pt, 0.0, 1.0), 5.0);

	/* Degenerate range (start==end): quarter points both equal pos[0].
	 * start_point == pos[0] and pos[0] is the start_point used for radius,
	 * so the two distances both equal the radius → error = 0. */
	ASSERT_DBL(BezError(pos, center, start_pt, 0.0, 0.0), 0.0);
}

/* -----------------------------------------------------------------------
 * BezErrorLine — error between a Bezier and a straight-line chord
 * ----------------------------------------------------------------------- */

static void BezErrorLineTests(void **state)
{
	(void)state;

	/* Straight-line Bezier vs its own endpoints: error ≈ 0 */
	coOrd straight[4] = { {0,0}, {10.0/3,0}, {20.0/3,0}, {10,0} };
	coOrd p0 = {0,0}, p3 = {10,0};
	ASSERT_DBL(BezErrorLine(straight, p0, p3, 0.0, 1.0), 0.0);

	/* C-curve (pos[1] and pos[2] above the x-axis) vs horizontal chord.
	 * Quarter points are at (1.5625, 5.625) and (8.4375, 5.625).
	 * Perpendicular distance from each to the x-axis = 5.625.
	 * Total error = 11.25. */
	coOrd ccurve[4] = { {0,0}, {0,10}, {10,10}, {10,0} };
	ASSERT_DBL(BezErrorLine(ccurve, p0, p3, 0.0, 1.0), 11.25);
}

/* -----------------------------------------------------------------------
 * AnalyseCurve — Bezier type classification
 * ----------------------------------------------------------------------- */

static void AnalyseCurveLineTests(void **state)
{
	(void)state;
	double fx, fy, cusp;
	Da.track = FALSE;

	/* Four collinear points → LINE */
	coOrd pts[4] = { {0,0}, {1,0}, {2,0}, {3,0} };
	assert_int_equal(AnalyseCurve(pts, &fx, &fy, &cusp), LINE);
}

static void AnalyseCurveEndsTests(void **state)
{
	(void)state;
	double fx, fy, cusp;

	/* Coincident endpoints with Da.track=TRUE → ENDS */
	coOrd pts[4] = { {5,5}, {2,2}, {8,2}, {5,5} };
	Da.track = TRUE;
	assert_int_equal(AnalyseCurve(pts, &fx, &fy, &cusp), ENDS);

	/* Same curve but Da.track=FALSE must NOT return ENDS */
	Da.track = FALSE;
	int result = AnalyseCurve(pts, &fx, &fy, &cusp);
	assert_true(result != ENDS);
}

static void AnalyseCurvePlainTests(void **state)
{
	(void)state;
	double fx, fy, cusp;
	Da.track = FALSE;

	/* Symmetric C-curve: pos[1] and pos[2] at y=5, yielding fx=1, fy=0 → PLAIN */
	coOrd pts[4] = { {0,0}, {0,5}, {10,5}, {10,0} };
	assert_int_equal(AnalyseCurve(pts, &fx, &fy, &cusp), PLAIN);

	/* Asymmetric C-curve: still yields fx > 1 → PLAIN */
	coOrd pts2[4] = { {0,0}, {2,4}, {8,4}, {10,0} };
	assert_int_equal(AnalyseCurve(pts2, &fx, &fy, &cusp), PLAIN);
}

static void AnalyseCurveInflectionTests(void **state)
{
	(void)state;
	double fx, fy, cusp;
	Da.track = FALSE;

	/* S-shaped curve: control arms cross → fy > 1 → INFLECTION
	 * pos[1]=(0,10) pulls left/up, pos[2]=(10,0) pulls right/down.
	 * Tracing the algorithm gives fx=1, fy=2. */
	coOrd pts[4] = { {0,0}, {0,10}, {10,0}, {10,10} };
	assert_int_equal(AnalyseCurve(pts, &fx, &fy, &cusp), INFLECTION);
	assert_true(fy > 1.0);
}

/* -----------------------------------------------------------------------
 * BezierLength
 * ----------------------------------------------------------------------- */

static void BezierLengthTests(void **state)
{
	(void)state;
	coOrd dummy[4] = { {0} };
	dynArr_t empty = {0};

	/* Empty segment array → 0 */
	ASSERT_DBL(BezierLength(dummy, empty), 0.0);

	/* Single arc: radius=10, a1=90° → arc length = 10 * π/2 */
	trkSeg_t arc_segs[1] = { make_arc_seg(10.0, 90.0) };
	dynArr_t arc_da = SEGS_FROM_ARRAY(arc_segs);
	ASSERT_DBL(BezierLength(dummy, arc_da), 10.0 * M_PI / 2.0);

	/* Single straight: (0,0)→(0,5) → length = 5 */
	trkSeg_t str_segs[1] = { make_str_seg(0, 0, 0, 5) };
	dynArr_t str_da = SEGS_FROM_ARRAY(str_segs);
	ASSERT_DBL(BezierLength(dummy, str_da), 5.0);

	/* Arc + straight combined */
	trkSeg_t mix_segs[2] = { make_arc_seg(10.0, 90.0), make_str_seg(0,0,0,5) };
	dynArr_t mix_da = SEGS_FROM_ARRAY(mix_segs);
	ASSERT_DBL(BezierLength(dummy, mix_da), 10.0 * M_PI / 2.0 + 5.0);

	/* Negative radius arc (reversed direction): absolute value taken */
	trkSeg_t neg_arc[1] = { make_arc_seg(-10.0, 90.0) };
	dynArr_t neg_da = SEGS_FROM_ARRAY(neg_arc);
	ASSERT_DBL(BezierLength(dummy, neg_da), 10.0 * M_PI / 2.0);
}

/* -----------------------------------------------------------------------
 * BezierMinRadius
 * ----------------------------------------------------------------------- */

static void BezierMinRadiusTests(void **state)
{
	(void)state;
	coOrd dummy[4] = { {0} };
	dynArr_t empty = {0};

	/* Empty → DIST_INF */
	assert_true(BezierMinRadius(dummy, empty) >= DIST_INF);

	/* Single arc radius=10 → 10 */
	trkSeg_t arc_segs[1] = { make_arc_seg(10.0, 90.0) };
	dynArr_t arc_da = SEGS_FROM_ARRAY(arc_segs);
	ASSERT_DBL(BezierMinRadius(dummy, arc_da), 10.0);

	/* Straight segment alone → DIST_INF */
	trkSeg_t str_segs[1] = { make_str_seg(0,0,0,5) };
	dynArr_t str_da = SEGS_FROM_ARRAY(str_segs);
	assert_true(BezierMinRadius(dummy, str_da) >= DIST_INF);

	/* Two arcs: min of radii 15 and 10 → 10 */
	trkSeg_t two_arcs[2] = { make_arc_seg(15.0,90.0), make_arc_seg(10.0,45.0) };
	dynArr_t two_da = SEGS_FROM_ARRAY(two_arcs);
	ASSERT_DBL(BezierMinRadius(dummy, two_da), 10.0);

	/* Straight + arc: min = arc radius */
	trkSeg_t mixed[2] = { make_str_seg(0,0,5,0), make_arc_seg(8.0,90.0) };
	dynArr_t mixed_da = SEGS_FROM_ARRAY(mixed);
	ASSERT_DBL(BezierMinRadius(dummy, mixed_da), 8.0);
}

/* -----------------------------------------------------------------------
 * BezierOffsetLength
 * ----------------------------------------------------------------------- */

static void BezierOffsetLengthTests(void **state)
{
	(void)state;
	dynArr_t empty = {0};

	/* Empty → 0 */
	ASSERT_DBL(BezierOffsetLength(empty, 2.0), 0.0);

	/* Positive-radius arc + positive offset: (r+offset)*angle */
	trkSeg_t pos_arc[1] = { make_arc_seg(10.0, 90.0) };
	dynArr_t pos_da = SEGS_FROM_ARRAY(pos_arc);
	ASSERT_DBL(BezierOffsetLength(pos_da, 2.0), 12.0 * M_PI / 2.0);

	/* Negative-radius arc + positive offset: |(-r-offset)*angle| = (r+offset)*angle */
	trkSeg_t neg_arc[1] = { make_arc_seg(-10.0, 90.0) };
	dynArr_t neg_da = SEGS_FROM_ARRAY(neg_arc);
	ASSERT_DBL(BezierOffsetLength(neg_da, 2.0), 12.0 * M_PI / 2.0);

	/* Straight segment: offset has no effect on length */
	trkSeg_t str_seg[1] = { make_str_seg(0,0,0,7) };
	dynArr_t str_da = SEGS_FROM_ARRAY(str_seg);
	ASSERT_DBL(BezierOffsetLength(str_da, 3.0), 7.0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(GetPointTests),
		cmocka_unit_test(DistanceToLineSegmentTests),
		cmocka_unit_test(BezErrorTests),
		cmocka_unit_test(BezErrorLineTests),
		cmocka_unit_test(AnalyseCurveLineTests),
		cmocka_unit_test(AnalyseCurveEndsTests),
		cmocka_unit_test(AnalyseCurvePlainTests),
		cmocka_unit_test(AnalyseCurveInflectionTests),
		cmocka_unit_test(BezierLengthTests),
		cmocka_unit_test(BezierMinRadiusTests),
		cmocka_unit_test(BezierOffsetLengthTests),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
