/** \file elevjoin.c
 *
 * Pure elevation-mode merge logic for joining two track endpoints.
 * See elevjoin.h.  The one caller is elev.c:SetTrkElevModes().
 */

#include "elevjoin.h"
#include "track.h"

#include <math.h>
#include <string.h>

/*
 * Precedence rank of an endpoint elevation mode when two endpoints are
 * joined: Defined beats Station beats Grade beats Computed; None / Ignore
 * (and anything unexpected) lose to all of them.  This is deliberately NOT
 * the order of the elevMode_e enum, which is
 * NONE, DEF, COMP, GRADE, IGNORE, STATION.
 */
static int ElevModeRank(int mode)
{
	switch (mode & ELEV_MASK) {
	case ELEV_DEF:
		return 4;
	case ELEV_STATION:
		return 3;
	case ELEV_GRADE:
		return 2;
	case ELEV_COMP:
		return 1;
	default:
		return 0;
	}
}

joinElev_t MergeJoinElev(endElev_t a, endElev_t b)
{
	joinElev_t r;
	const endElev_t * w;
	int modeA = a.mode & ELEV_MASK;
	int modeB = b.mode & ELEV_MASK;

	r.mode = modeA;
	r.elev = 0.0;
	r.station = NULL;
	r.heightsDiffer = FALSE;
	r.diff = 0.0;
	r.stationsDiffer = FALSE;

	/* higher rank wins; a wins ties */
	w = (ElevModeRank(b.mode) > ElevModeRank(a.mode)) ? &b : &a;

	switch (w->mode & ELEV_MASK) {
	case ELEV_DEF:
		if (modeA == ELEV_DEF && modeB == ELEV_DEF) {
			/*
			 * Both defined: average the heights, keep the OR of both
			 * option words so an ELEV_VISIBLE bit on either survives,
			 * and report the gap for MSG_JOIN_DIFFER_ELEV.
			 */
			r.elev = (a.height + b.height) / 2.0;
			r.mode = a.unmasked | b.unmasked;
			r.diff = fabs(a.height - b.height);
			r.heightsDiffer = (r.diff > ELEVJOIN_DIFF_EPS);
		} else {
			/*
			 * Only the winner is defined: adopt its height and its
			 * unmasked mode.  (The pre-extraction ladder had a bug
			 * here when the *second* endpoint was the defined one --
			 * it kept the first endpoint's non-DEF mode and dropped
			 * the definition.)
			 */
			r.elev = w->height;
			r.mode = w->unmasked;
		}
		break;
	case ELEV_STATION:
		r.mode = w->mode & ELEV_MASK;
		r.station = w->station;
		if (modeA == ELEV_STATION && modeB == ELEV_STATION) {
			const char * sa = a.station ? a.station : "";
			const char * sb = b.station ? b.station : "";
			r.stationsDiffer = (strcmp(sa, sb) != 0);
		}
		break;
	default:
		/* Grade / Computed / None / Ignore: carry the winning mode only. */
		r.mode = w->mode & ELEV_MASK;
		break;
	}
	return r;
}
