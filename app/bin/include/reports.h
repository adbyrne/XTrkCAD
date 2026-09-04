/** \file reports.h
 * Native "Reports" feature (SF #217): a generic viewer shared by every
 * report phase, plus the phase-1 (unconnected endpoints) report itself.
 *
 * Several structs/functions below note where they mirror "the MCP
 * reference implementation" -- an external MCP project (a separate
 * Python tool, not part of this source tree) used during design to
 * ground native field shapes/thresholds against a working implementation
 * rather than guessing. Every such mention below is design history, not
 * a citation a reader here can open and check.
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2026 XTrkCAD contributors
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef REPORTS_H
#define REPORTS_H

#include <dynstring.h>
#include "xtctypes.h"

/** One open (unconnected) track endpoint, as returned by the compute pass
 * and formatted by ReportsFormatUnconnectedList(). trackId/pos/angle mirror
 * the MCP reference implementation's find_unconnected_endpoints() return
 * shape exactly, for parity -- scale is a phase-1.5 addition (not part of
 * that parity contract), needed so the interactive indicator can size
 * itself to the endpoint's own track scale rather than assuming one
 * layout-wide scale. Appended at the end so existing positional
 * initializers (tests included) don't need updating. */
typedef struct {
	TRKINX_T trackId;
	coOrd    pos;
	ANGLE_T  angle;
	SCALEINX_T scale;
	/** TRUE if another open endpoint, on a *different* track, lies within
	 * connectDistance (track.h) of this one -- i.e. this one looks like a
	 * missed connection rather than a deliberate dead-end stub. Computed
	 * by ReportsUnconnectedEndpoints() after the full list is known (needs
	 * every other entry to compare against), not by the compute loop that
	 * fills the rest of this struct. Appended at the end, after `scale`,
	 * so existing positional initializers (tests included) don't need
	 * updating. */
	BOOL_T nearby;
} reportsEndPt_t;

/**
 * Format a list of open endpoints as report body text, one row per entry,
 * no header/footer (those are added by the caller, since they carry
 * translatable strings this pure function deliberately stays out of).
 * Pure function -- no track_p/live layout state needed, safe to unit test
 * directly against hand-built input.
 *
 * \param[in,out] out appended to, not cleared first
 * \param[in] list array of open endpoints
 * \param[in] count number of entries in list
 */
void ReportsFormatUnconnectedList(DynString *out, const reportsEndPt_t *list,
                                  int count);

/* ---------------------------------------------------------------------
 * Phase 2 batch (track lengths / curve stats / turnout density /
 * equipment suitability) -- SF #217, report-only (no click-to-navigate,
 * no draw indicator, unlike phase 1). Row structs, formatters, and the
 * two pure classification helpers below were written test-first
 * 2026-09-04 (unittest/reportstest.c) and are now implemented in
 * reportsformat.c; the compute pass that walks the live layout to fill
 * these rows in is still to come. `name` fields
 * are borrowed `const char *` (GetLayerName()'s own return convention),
 * not owned by the row -- caller must keep the layout alive for the
 * formatter call's duration, same lifetime rule GetLayerName() itself
 * already implies elsewhere in the app.
 * ------------------------------------------------------------------- */

/** One layer's row in the track-lengths report. Mirrors the MCP
 * get_track_lengths() by_layer breakdown shape (no write_track_lengths_report
 * exists to match text formatting against -- get_track_lengths/get_curve_stats
 * are the only MCP parity targets for phase 2's first third, and they return
 * structured data, not report text, so this row layout is a fresh design,
 * not a port -- 2026-09-04: explicitly not required to match the MCP text
 * layout, this and the other phase-2 formatters below improve on it where
 * it helps readability). */
typedef struct {
	unsigned int layer;
	const char *name;
	DIST_T lengthFt;
	DIST_T lengthIn;
	int turnoutCount;
	int carCapacity;
} reportsTrackLenLayer_t;

/**
 * Format the track-lengths by-layer table body, one row per layer, plus a
 * trailing "% of total" column computed from totalFt (2026-09-04 addition,
 * no MCP equivalent -- more useful at a glance than raw feet alone).
 * totalFt <= 0 renders every row's percent as 0.0, not a divide-by-zero.
 * Pure function, see ReportsFormatUnconnectedList()'s doc comment for the
 * header/footer-stays-with-caller rationale.
 *
 * \param[in] totalFt sum of every row's lengthFt (not recomputed from
 *     list[] -- passed explicitly so the caller's own total, e.g. one that
 *     might include layers not in this filtered list, is authoritative)
 */
void ReportsFormatTrackLengthList(DynString *out,
                                  const reportsTrackLenLayer_t *list, int count, DIST_T totalFt);

/**
 * Cars-per-real-foot factor for a scale name, Fugate methodology (MRH Oct
 * 2014, ~44ft car incl. couplers) -- same table `get_operation_density()`'s
 * `cars_per_real_ft()` uses on the MCP side, ported for parity: O=1.0,
 * S=1.5, HO=2.0, N=4.0, Z=5.0, G=0.5, TT=3.0, I=0.75; narrow-gauge variants
 * (HOn3/On3/Sn3/Nn3) match their parent scale's body length. Case-
 * insensitive match against the whole table -- the MCP reference's own
 * lookup (`CARS_PER_FT.get(scale.upper(), CARS_PER_FT.get(scale, 2.0))`)
 * is only actually case-insensitive for the plain-uppercase entries
 * (`scale.upper()` never equals a mixed-case key like "HOn3"), so e.g.
 * "on3"/"ON3" silently fall through to its 2.0 default instead of On3's
 * real 1.0 -- an incidental quirk of the two-tier dict lookup, not a
 * deliberate design choice, so not reproduced here (2026-09-04: matches
 * the underlying data/thresholds, not incidental reference behavior, same
 * standing as ReportsFormatTurnoutList()'s HEAVY-flag cleanup). Falls back
 * to 2.0 (HO's own factor, the single most common scale) for anything not
 * in the table, same default the MCP reference uses.
 *
 * \param[in] scaleName e.g. "HO", "On3" (GetScaleName(GetLayoutCurScale())
 *     on the caller's side) -- NULL also falls back to 2.0
 */
DIST_T ReportsCarsPerFoot(const char *scaleName);

/** One radius-range bucket in the curve-stats histogram. Mirrors
 * get_curve_stats()'s radius_distribution dict (label -> count); label is
 * caller-owned (a literal, e.g. "< 12in"), not copied. */
typedef struct {
	const char *label;
	int count;
} reportsCurveBucket_t;

/**
 * Format the curve-radius histogram body, one row per bucket, in whatever
 * order the caller passes them (the compute pass is responsible for a
 * stable/meaningful bucket order -- this formatter does not sort).
 */
void ReportsFormatCurveHistogram(DynString *out,
                                 const reportsCurveBucket_t *list, int count);

/**
 * Classify a curve's radius into one of get_curve_stats()'s five
 * radius_distribution buckets (model inches, same boundaries as the MCP
 * reference): "< 12in", "12-18in", "18-24in", "24-36in", "> 36in" (an
 * ASCII hyphen, not the MCP reference's Unicode en-dash -- a cosmetic
 * difference, not a data one, same "data parity not text parity" standing
 * as this batch's other formatters). Pure boundary decision, split out for
 * direct unit testing same as ReportsIsTurnoutHeavy()/
 * ReportsClassifyEquipment(). Every boundary is the bucket's own upper
 * bound, exclusive (radius == 12.0 falls in "12-18in", not "< 12in").
 *
 * \param[in] radius the curve's radius (GetCurveRadius()), model inches
 * \return a literal string, safe to store without copying
 */
const char *ReportsCurveBucketLabel(DIST_T radius);

/** One layer's row in the turnout-density report. `heavy` is TRUE when
 * density > 10.0/100ft (strict). Field shape originally mirrored
 * write_turnout_report()'s BY LAYER table; the row content is unchanged
 * but the *display* diverges from that MCP text as of 2026-09-04 (see
 * ReportsFormatTurnoutList()) -- exact text parity was never required,
 * only the underlying data/threshold. */
typedef struct {
	unsigned int layer;
	const char *name;
	DIST_T feet;
	int turnoutCount;
	DIST_T density;
	BOOL_T heavy;
} reportsTurnoutLayer_t;

/**
 * Format the turnout-density BY LAYER table body, one row per layer,
 * **sorted by density descending** (stable -- ties keep the caller's
 * original relative order) so switching-heavy layers surface immediately
 * without scanning a flag column. Heavy rows get a trailing right-aligned
 * "HEAVY" word instead of the MCP reference's "***"; non-heavy rows have
 * no flag text or trailing whitespace at all (2026-09-04: replaces an
 * earlier byte-parity port of the MCP layout that left two trailing
 * spaces on every non-flagged row -- sloppy output, not a deliberate
 * choice, worth fixing outright rather than preserving for parity's sake).
 */
void ReportsFormatTurnoutList(DynString *out,
                              const reportsTurnoutLayer_t *list, int count);

/**
 * TRUE if a layer's turnout density counts as "switching-heavy" (the
 * "HEAVY" flag), i.e. density > 10.0/100ft. Pure boundary decision, split
 * out of the compute pass specifically so it's directly unit-testable
 * rather than only verifiable via an .xtr fixture -- originally ported
 * from write_turnout_report()'s `density > 10.0` (strict, not >=).
 *
 * The 10.0 threshold is a hardcoded literal, same as this report's other
 * magic numbers (equipment suitability's 2" MARGINAL margin, phase 1's
 * `connectDistance` before it became a real preference). Flagged
 * 2026-09-04 as a good future-preference candidate -- worth exposing as a
 * user setting (Prefs > Reports, alongside `reportIndicatorColor`) once
 * this report ships and there's real usage to justify it, rather than
 * guessing at a UI now. Not done in this pass -- logged, not implemented.
 */
BOOL_T ReportsIsTurnoutHeavy(DIST_T density);

/** One turnout with fewer than 3 endpoints (possibly broken/incomplete),
 * mirrors write_turnout_report()'s PARTIAL TURNOUTS list. */
typedef struct {
	TRKINX_T trackId;
	int endPtCnt;
	unsigned int layer;
} reportsPartialTurnout_t;

/** Format the partial-turnouts list body, one row per entry. Count == 0
 * formats as an empty string, same convention as
 * ReportsFormatUnconnectedList() -- the "None found." message is a
 * translatable string and belongs to the caller, not this pure function. */
void ReportsFormatPartialTurnoutList(DynString *out,
                                     const reportsPartialTurnout_t *list, int count);

/** PASS/MARGINAL/FAIL classification for one equipment class against the
 * layout's minimum curve radius. Values match write_equipment_report()'s
 * three literal status strings exactly (parity target) -- not translated,
 * same as that report's own untranslated ASCII tokens. */
typedef enum {
	REPORTS_EQUIP_PASS,
	REPORTS_EQUIP_MARGINAL,
	REPORTS_EQUIP_FAIL
} reportsEquipStatus_e;

/** One equipment class's row in the equipment-suitability report.
 * `thresholdIn` is already scale-adjusted by the caller (HO-reference
 * threshold * scale factor) -- this formatter does no scaling itself.
 * Row content originally mirrored write_equipment_report()'s
 * COMPATIBILITY table; the *display* diverges as of 2026-09-04 (grouped
 * by status, see ReportsFormatEquipmentList()) -- text parity was never
 * required, only the PASS/MARGINAL/FAIL decision itself. */
typedef struct {
	const char *name;
	DIST_T thresholdIn;
	reportsEquipStatus_e status;
} reportsEquipClass_t;

/**
 * Format the equipment-suitability table body, **grouped under PASS /
 * MARGINAL / FAIL subheadings** (in that order; a status with no rows
 * gets no heading at all) instead of one flat list with a per-row status
 * column -- lets a user see what doesn't fit at a glance instead of
 * scanning every row (2026-09-04, replaces an earlier flat byte-parity
 * port of the MCP layout). Within each group, rows keep the caller's
 * original relative order (stable, e.g. cheapest-to-hardest equipment
 * class order). A blank line separates consecutive non-empty groups; none
 * trails the last group.
 */
void ReportsFormatEquipmentList(DynString *out,
                                const reportsEquipClass_t *list, int count);

/**
 * Classify one equipment class's (already scale-adjusted) minimum radius
 * threshold against the layout's actual minimum curve radius. Pure
 * boundary decision, split out of the compute pass for direct unit
 * testing (see ReportsIsTurnoutHeavy()'s rationale above). Matches
 * write_equipment_report() exactly: PASS if minRadius >= thresholdIn;
 * MARGINAL if minRadius >= thresholdIn - 2.0*scaleFactor (inclusive,
 * `>=`); FAIL otherwise. The 2" MARGINAL margin is itself scale-adjusted
 * in the MCP reference (`thresh - 2.0 * sf`) -- easy to miss when
 * thresholdIn is already pre-scaled, hence the separate scaleFactor
 * parameter rather than baking a flat 2.0 into this function.
 *
 * The 2.0" MARGINAL margin is a hardcoded literal, same future-preference
 * candidate as ReportsIsTurnoutHeavy()'s 10.0 threshold above -- worth
 * exposing as a user setting once this report has shipped and there's
 * real usage to justify a UI for it. Not made configurable in this pass.
 *
 * \param[in] minRadius the layout's actual minimum curve radius (already
 *     scale-adjusted, same units as thresholdIn)
 * \param[in] thresholdIn the equipment class's minimum radius threshold
 *     (already scale-adjusted by the caller: HO-reference threshold *
 *     scaleFactor)
 * \param[in] scaleFactor HO_ratio / this layout's scale ratio -- the same
 *     factor used to produce thresholdIn, needed again here to scale the
 *     2" MARGINAL margin
 */
reportsEquipStatus_e ReportsClassifyEquipment(DIST_T minRadius,
                DIST_T thresholdIn, DIST_T scaleFactor);

/**
 * Menu callback (phase 1), also reused directly as the Refresh button's
 * refresh proc (see reports.c's `reportsDialog_t.refreshProc`) since it
 * always recomputes from scratch: walk the current layout for every open
 * (unconnected) track endpoint, flag/group Nearby ones
 * (ReportsMarkNearby()), show/update the report dialog (the internal,
 * generic `ReportsShowDialog()` -- see reports.c; every report phase
 * shares this same viewer machinery, parameterized by a `reportsDialog_t`
 * instance per report -- then the summary label and the interactive list
 * via ReportsPopulateList()), and log a one-line summary to the "reports"
 * debug category (`-d reports=1`) for live-testing.
 *
 * \param[in] unused menu-callback signature, unused
 */
void ReportsUnconnectedEndpoints(void *unused);

/**
 * Menu callback (phase 2): compute and show/refresh the Turnout Density
 * report -- total and by-layer track length/turnout counts, density per
 * 100ft of track (ReportsFormatTurnoutList()), switching-heavy layers
 * flagged via ReportsIsTurnoutHeavy(), and a partial-turnouts list (fewer
 * than 3 endpoints -- folded into the Save/Print text only, not a second
 * interactive list, per the phase-2 implementation plan). Report-only,
 * unlike phase 1: no click-to-navigate, no draw indicator -- the
 * `reportsturnout.ui` dialog has no row-selection changeProc at all.
 *
 * \param[in] unused menu-callback signature, unused
 */
void ReportsTurnoutDensity(void *unused);

/**
 * Menu callback (phase 2): compute and show/refresh the Track Lengths
 * report -- total and by-layer track length (feet/inches), turnout count,
 * and car capacity (ReportsCarsPerFoot(), Fugate methodology) per layer.
 * Report-only, no click-to-navigate/indicator, same shape as
 * ReportsTurnoutDensity(). Shares that function's `GetTrkEndPtCnt(trk) >=
 * 2` guard before any `GetTrkLength()` call -- see
 * [[feedback_trk_iterate_endpoint_guard]] (memory) / SF #776's fix.
 *
 * \param[in] unused menu-callback signature, unused
 */
void ReportsTrackLengths(void *unused);

/**
 * Menu callback (phase 2): compute and show/refresh the Curve Stats
 * report -- curve count, min/max/mean radius (model inches), and a
 * radius-distribution histogram (ReportsCurveBucketLabel()). Report-only,
 * no click-to-navigate/indicator. Uses GetCurveRadius() (ccurve.h), not
 * GetTrackParams(PARAMS_CORNU, ...) -- see that function's own doc
 * comment (tcurve.c) for why the more general accessor isn't safe to call
 * on every TRK_ITERATE object.
 *
 * \param[in] unused menu-callback signature, unused
 */
void ReportsCurveStats(void *unused);

/**
 * Menu callback (phase 2, last of the batch): compute and show/refresh
 * the Equipment Suitability report -- classifies a fixed list of 11
 * equipment classes (short/standard/long freight cars, diesel/steam
 * locomotives by size, passenger cars) against the layout's minimum
 * usable curve radius (ReportsClassifyEquipment(), PASS/MARGINAL/FAIL).
 * Unlike the other three phase-2 reports, this one doesn't walk
 * TRK_ITERATE for a by-layer breakdown -- it only needs the layout-wide
 * minimum curve radius (via GetCurveRadius(), same as Curve Stats, with
 * curves under 9in excluded as likely decorative -- matches the external
 * MCP project's own reference behavior) and a scale-adjustment factor
 * (LookupScale("HO")/GetScaleRatio() vs. the layout's own
 * GetLayoutCurScale()/GetScaleRatio()). Report-only, no
 * click-to-navigate/indicator.
 *
 * \param[in] unused menu-callback signature, unused
 */
void ReportsEquipmentSuitability(void *unused);

/**
 * Draw the current interactive-navigation indicator (phase 1.5), if one is
 * active -- an open circle at the last-clicked report row's position, no-op
 * otherwise. Called once per redraw from draw.c's DrawTempContent(), the
 * same transient/never-saved-to-file drawing pass the ruler crosshair and
 * command-feedback markers already use.
 *
 * Takes `void *` rather than `drawCmd_p` so this shared header (also
 * included by the lightweight reportstest.c CMocka target) doesn't need to
 * pull in draw.h; the real definition in reports.c casts back to
 * `drawCmd_p` internally. The only caller (draw.c) already has a real
 * `drawCmd_p` to pass, so the implicit `drawCmd_p` -> `void *` conversion
 * at the call site is always safe.
 *
 * \param[in] d the drawCmd_p (mainD/tempD) to draw into
 */
void ReportsDrawIndicator(void *d);

#endif // REPORTS_H
