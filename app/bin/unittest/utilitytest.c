/** \file utilitytest.c
 * Unit tests for geometric utility functions in utility.c
 */

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>

#include "common.h"
#include "../utility.c"

/* XTrkCAD uses compass-bearing angles: 0=north(+y), 90=east(+x), 180=south, 270=west */

#define NEARLY_EQ(a, b)  (fabs((a) - (b)) < 1e-5)
#define ASSERT_DBL(actual, expected) \
	assert_true(NEARLY_EQ((actual), (expected)))
#define ASSERT_COORD(p, ex, ey) \
	do { ASSERT_DBL((p).x, (ex)); ASSERT_DBL((p).y, (ey)); } while (0)

/* -----------------------------------------------------------------------
 * NormalizeAngle
 * ----------------------------------------------------------------------- */

static void NormalizeAngleTests(void **state)
{
	(void)state;

	ASSERT_DBL(NormalizeAngle(0.0),    0.0);
	ASSERT_DBL(NormalizeAngle(90.0),   90.0);
	ASSERT_DBL(NormalizeAngle(360.0),  0.0);
	ASSERT_DBL(NormalizeAngle(450.0),  90.0);
	ASSERT_DBL(NormalizeAngle(-90.0),  270.0);
	ASSERT_DBL(NormalizeAngle(-1.0),   359.0);
	ASSERT_DBL(NormalizeAngle(720.0),  0.0);
	ASSERT_DBL(NormalizeAngle(359.0),  359.0);
}

/* -----------------------------------------------------------------------
 * DifferenceBetweenAngles
 * ----------------------------------------------------------------------- */

static void AngleDifferenceTests(void **state)
{
	(void)state;

	/* Simple cases */
	ASSERT_DBL(DifferenceBetweenAngles(0.0,   90.0),  90.0);
	ASSERT_DBL(DifferenceBetweenAngles(90.0,   0.0), -90.0);
	ASSERT_DBL(DifferenceBetweenAngles(0.0,  180.0), 180.0);

	/* Wrap-around: shortest path crosses 0/360 boundary */
	ASSERT_DBL(DifferenceBetweenAngles(350.0,  10.0),  20.0);
	ASSERT_DBL(DifferenceBetweenAngles(10.0,  350.0), -20.0);

	/* Identical angles */
	ASSERT_DBL(DifferenceBetweenAngles(45.0,   45.0),   0.0);

	/* Near ±180 boundary */
	ASSERT_DBL(DifferenceBetweenAngles(0.0,  179.0),  179.0);
	ASSERT_DBL(DifferenceBetweenAngles(0.0,  181.0), -179.0);
}

/* -----------------------------------------------------------------------
 * FindDistance
 * ----------------------------------------------------------------------- */

static void FindDistanceTests(void **state)
{
	(void)state;
	coOrd p0, p1;

	/* 3-4-5 right triangle */
	p0.x = 0.0; p0.y = 0.0;
	p1.x = 3.0; p1.y = 4.0;
	ASSERT_DBL(FindDistance(p0, p1), 5.0);

	/* Zero distance */
	p1.x = 0.0; p1.y = 0.0;
	ASSERT_DBL(FindDistance(p0, p1), 0.0);

	/* Horizontal distance */
	p0.x = 1.0; p0.y = 0.0;
	p1.x = 6.0; p1.y = 0.0;
	ASSERT_DBL(FindDistance(p0, p1), 5.0);

	/* Negative coordinates */
	p0.x = -1.0; p0.y = -1.0;
	p1.x =  2.0; p1.y =  3.0;
	ASSERT_DBL(FindDistance(p0, p1), 5.0);
}

/* -----------------------------------------------------------------------
 * FindAngle  (compass bearing: 0=north/+y, 90=east/+x)
 * ----------------------------------------------------------------------- */

static void FindAngleTests(void **state)
{
	(void)state;
	coOrd origin = {0.0, 0.0};
	coOrd p;

	p.x =  0.0; p.y =  1.0; ASSERT_DBL(FindAngle(origin, p),   0.0);  /* north */
	p.x =  1.0; p.y =  0.0; ASSERT_DBL(FindAngle(origin, p),  90.0);  /* east  */
	p.x =  0.0; p.y = -1.0; ASSERT_DBL(FindAngle(origin, p), 180.0);  /* south */
	p.x = -1.0; p.y =  0.0; ASSERT_DBL(FindAngle(origin, p), 270.0);  /* west  */

	/* Coincident points return 0 */
	ASSERT_DBL(FindAngle(origin, origin), 0.0);

	/* FindAngle is consistent with NormalizeAngle output range */
	p.x = 1.0; p.y = 1.0;
	double a = FindAngle(origin, p);
	assert_true(a >= 0.0 && a < 360.0);
}

/* -----------------------------------------------------------------------
 * Rotate
 * ----------------------------------------------------------------------- */

static void RotateTests(void **state)
{
	(void)state;
	coOrd p, orig = {0.0, 0.0};

	/* 0° rotation is identity */
	p.x = 1.0; p.y = 0.0;
	Rotate(&p, orig, 0.0);
	ASSERT_COORD(p, 1.0, 0.0);

	/* 90° clockwise in compass convention: (1,0) -> (0,-1) */
	p.x = 1.0; p.y = 0.0;
	Rotate(&p, orig, 90.0);
	ASSERT_COORD(p, 0.0, -1.0);

	/* 180°: (1,0) -> (-1,0) */
	p.x = 1.0; p.y = 0.0;
	Rotate(&p, orig, 180.0);
	ASSERT_COORD(p, -1.0, 0.0);

	/* 270°: (1,0) -> (0,1) */
	p.x = 1.0; p.y = 0.0;
	Rotate(&p, orig, 270.0);
	ASSERT_COORD(p, 0.0, 1.0);

	/* 360° is identity */
	p.x = 3.0; p.y = 4.0;
	Rotate(&p, orig, 360.0);
	ASSERT_COORD(p, 3.0, 4.0);

	/* Rotation around non-origin pivot */
	orig.x = 1.0; orig.y = 0.0;
	p.x = 2.0; p.y = 0.0;  /* 1 unit east of pivot */
	Rotate(&p, orig, 90.0);   /* rotate 90°: (2,0) -> (1,-1) */
	ASSERT_COORD(p, 1.0, -1.0);
}

/* -----------------------------------------------------------------------
 * Translate
 * ----------------------------------------------------------------------- */

static void TranslateTests(void **state)
{
	(void)state;
	coOrd orig = {0.0, 0.0}, res;

	/* North (0°) moves along +y */
	Translate(&res, orig, 0.0, 5.0);
	ASSERT_COORD(res, 0.0, 5.0);

	/* East (90°) moves along +x */
	Translate(&res, orig, 90.0, 5.0);
	ASSERT_COORD(res, 5.0, 0.0);

	/* South (180°) moves along -y */
	Translate(&res, orig, 180.0, 5.0);
	ASSERT_COORD(res, 0.0, -5.0);

	/* West (270°) moves along -x */
	Translate(&res, orig, 270.0, 5.0);
	ASSERT_COORD(res, -5.0, 0.0);

	/* Translate and FindAngle are inverses */
	orig.x = 1.0; orig.y = 2.0;
	Translate(&res, orig, 45.0, 10.0);
	ASSERT_DBL(FindAngle(orig, res), 45.0);
	ASSERT_DBL(FindDistance(orig, res), 10.0);
}

/* -----------------------------------------------------------------------
 * PointOnCircle
 * ----------------------------------------------------------------------- */

static void PointOnCircleTests(void **state)
{
	(void)state;
	coOrd center = {0.0, 0.0}, p;

	PointOnCircle(&p, center, 5.0, 0.0);
	ASSERT_COORD(p, 0.0, 5.0);   /* north */

	PointOnCircle(&p, center, 5.0, 90.0);
	ASSERT_COORD(p, 5.0, 0.0);   /* east */

	PointOnCircle(&p, center, 5.0, 180.0);
	ASSERT_COORD(p, 0.0, -5.0);  /* south */

	/* Point lies at the correct radius */
	PointOnCircle(&p, center, 3.0, 45.0);
	ASSERT_DBL(FindDistance(center, p), 3.0);
}

/* -----------------------------------------------------------------------
 * FindIntersection (line-line)
 * ----------------------------------------------------------------------- */

static void LineLineIntersectionTests(void **state)
{
	(void)state;
	coOrd P0, P1, Pc;
	int ok;

	/* Perpendicular lines meeting at origin:
	 * Line 0: through (0,0) heading north (0°) — stays on x=0
	 * Line 1: through (1,0) heading east (90°) — stays on y=0
	 * Intersection: (0,0) */
	P0.x = 0.0; P0.y = 0.0;
	P1.x = 1.0; P1.y = 0.0;
	ok = FindIntersection(&Pc, P0, 0.0, P1, 90.0);
	assert_int_equal(ok, TRUE);
	ASSERT_COORD(Pc, 0.0, 0.0);

	/* Diagonal lines meeting at (1,1):
	 * Line 0: through (0,0) heading NE (45°)
	 * Line 1: through (2,0) heading NW (315°) */
	P0.x = 0.0; P0.y = 0.0;
	P1.x = 2.0; P1.y = 0.0;
	ok = FindIntersection(&Pc, P0, 45.0, P1, 315.0);
	assert_int_equal(ok, TRUE);
	ASSERT_COORD(Pc, 1.0, 1.0);

	/* Parallel lines: both heading north → no intersection */
	P0.x = 0.0; P0.y = 0.0;
	P1.x = 1.0; P1.y = 0.0;
	ok = FindIntersection(&Pc, P0, 0.0, P1, 0.0);
	assert_int_equal(ok, FALSE);

	/* Anti-parallel (opposite directions) are still parallel */
	ok = FindIntersection(&Pc, P0, 0.0, P1, 180.0);
	assert_int_equal(ok, FALSE);
}

/* -----------------------------------------------------------------------
 * FindArcIntersections (circle-circle)
 * ----------------------------------------------------------------------- */

static void CircleCircleIntersectionTests(void **state)
{
	(void)state;
	coOrd c1, c2, Pc, Pc2;
	int ok;

	/* Two unit circles offset 1 unit apart: known intersections at (0.5, ±√3/2) */
	c1.x = 0.0; c1.y = 0.0;
	c2.x = 1.0; c2.y = 0.0;
	ok = FindArcIntersections(&Pc, &Pc2, c1, 1.0, c2, 1.0);
	assert_int_equal(ok, TRUE);
	ASSERT_DBL(Pc.x,  0.5);
	ASSERT_DBL(fabs(Pc.y),  sqrt(3.0) / 2.0);
	ASSERT_DBL(Pc2.x, 0.5);
	ASSERT_DBL(fabs(Pc2.y), sqrt(3.0) / 2.0);
	/* The two intersection points are symmetric (different y signs) */
	assert_true(NEARLY_EQ(Pc.y, -Pc2.y));

	/* Circles too far apart */
	c2.x = 10.0;
	ok = FindArcIntersections(&Pc, &Pc2, c1, 1.0, c2, 1.0);
	assert_int_equal(ok, FALSE);

	/* One circle entirely inside the other */
	c2.x = 0.0; c2.y = 0.0;
	ok = FindArcIntersections(&Pc, &Pc2, c1, 5.0, c2, 1.0);
	assert_int_equal(ok, FALSE);

	/* Coincident circles (same center, same radius) */
	ok = FindArcIntersections(&Pc, &Pc2, c1, 1.0, c1, 1.0);
	assert_int_equal(ok, FALSE);
}

/* -----------------------------------------------------------------------
 * FindArcAndLineIntersections (circle-line)
 * ----------------------------------------------------------------------- */

static void CircleLineIntersectionTests(void **state)
{
	(void)state;
	coOrd c, p1, p2, i1, i2;
	int ok;

	c.x = 0.0; c.y = 0.0;

	/* Horizontal secant through center: line from (-10,0) to (10,0),
	 * circle r=5 → intersections at (±5, 0) */
	p1.x = -10.0; p1.y = 0.0;
	p2.x =  10.0; p2.y = 0.0;
	ok = FindArcAndLineIntersections(&i1, &i2, c, 5.0, p1, p2);
	assert_int_equal(ok, TRUE);
	/* One intersection at x=5, one at x=-5, both y=0 */
	assert_true((NEARLY_EQ(i1.x, 5.0) || NEARLY_EQ(i1.x, -5.0)));
	assert_true((NEARLY_EQ(i2.x, 5.0) || NEARLY_EQ(i2.x, -5.0)));
	ASSERT_DBL(i1.y, 0.0);
	ASSERT_DBL(i2.y, 0.0);

	/* Tangent line: vertical line x=5, circle r=5 → single point (5,0) */
	p1.x = 5.0; p1.y = -10.0;
	p2.x = 5.0; p2.y =  10.0;
	ok = FindArcAndLineIntersections(&i1, &i2, c, 5.0, p1, p2);
	assert_int_equal(ok, TRUE);
	ASSERT_COORD(i1, 5.0, 0.0);
	ASSERT_COORD(i2, 5.0, 0.0);

	/* Line entirely outside circle: vertical line x=6, circle r=5 */
	p1.x = 6.0; p1.y = -10.0;
	p2.x = 6.0; p2.y =  10.0;
	ok = FindArcAndLineIntersections(&i1, &i2, c, 5.0, p1, p2);
	assert_int_equal(ok, FALSE);
}

/* -----------------------------------------------------------------------
 * MidPtCoOrd
 * ----------------------------------------------------------------------- */

static void MidpointTests(void **state)
{
	(void)state;
	coOrd p0, p1, mid;

	p0.x = 0.0; p0.y = 0.0;
	p1.x = 4.0; p1.y = 6.0;
	mid = MidPtCoOrd(p0, p1);
	ASSERT_COORD(mid, 2.0, 3.0);

	/* Midpoint of identical points */
	mid = MidPtCoOrd(p0, p0);
	ASSERT_COORD(mid, 0.0, 0.0);

	/* Negative coordinates */
	p0.x = -4.0; p0.y = -2.0;
	p1.x =  2.0; p1.y =  4.0;
	mid = MidPtCoOrd(p0, p1);
	ASSERT_COORD(mid, -1.0, 1.0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(NormalizeAngleTests),
		cmocka_unit_test(AngleDifferenceTests),
		cmocka_unit_test(FindDistanceTests),
		cmocka_unit_test(FindAngleTests),
		cmocka_unit_test(RotateTests),
		cmocka_unit_test(TranslateTests),
		cmocka_unit_test(PointOnCircleTests),
		cmocka_unit_test(LineLineIntersectionTests),
		cmocka_unit_test(CircleCircleIntersectionTests),
		cmocka_unit_test(CircleLineIntersectionTests),
		cmocka_unit_test(MidpointTests),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
