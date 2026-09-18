/** \file elevjoin.h
 *
 * Pure elevation-mode merge logic for joining two track endpoints.
 *
 * Extracted from elev.c:SetTrkElevModes() so the precedence rules can be
 * unit-tested without a live track graph.  The one caller is
 * elev.c:SetTrkElevModes(); the mode / height / station constants come from
 * track.h (elevMode_e, ELEV_MASK, ELEV_VISIBLE).
 */

#ifndef ELEVJOIN_H
#define ELEVJOIN_H

#include "common.h"

/** Height mismatch (model inches) above which a Defined/Defined join warns. */
#define ELEVJOIN_DIFF_EPS (0.1)

/** One endpoint's elevation state, read from the track object. */
typedef struct {
	int mode;		/**< masked mode, ie GetTrkEndElevMode() */
	int unmasked;		/**< raw option word, ie GetTrkEndElevUnmaskedMode() */
	DIST_T height;		/**< defined height; valid only when mode==ELEV_DEF */
	const char * station;	/**< name; valid only when mode==ELEV_STATION */
} endElev_t;

/** Result of merging two endpoints for a join. */
typedef struct {
	int mode;		/**< mode to apply to both endpoints */
	DIST_T elev;		/**< height to apply (0 when not height-bearing) */
	const char * station;	/**< station to apply (NULL when not a station) */
	BOOL_T heightsDiffer;	/**< both DEF and the gap exceeds ELEVJOIN_DIFF_EPS */
	DIST_T diff;		/**< that height gap (0 unless heightsDiffer) */
	BOOL_T stationsDiffer;	/**< both STATION and the names differ */
} joinElev_t;

/**
 * Merge the elevation state of two endpoints being joined.
 *
 * Precedence: Defined > Station > Grade > Computed; anything else (None,
 * Ignore) loses to all of them.  Within one tier the first endpoint (\a a)
 * wins.  When both endpoints are Defined the result height is their average
 * and \a heightsDiffer / \a diff report whether they were far enough apart to
 * warrant a warning.  When both are Station, \a stationsDiffer reports a name
 * mismatch.  The caller is responsible for emitting any user message.
 *
 * \param a IN first endpoint (wins ties)
 * \param b IN second endpoint
 * \return the merged mode / height / station plus mismatch flags
 */
joinElev_t MergeJoinElev(endElev_t a, endElev_t b);

#endif
