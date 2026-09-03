/** \file reports.h
 * Native "Reports" feature (SF #217): a generic viewer shared by every
 * report phase, plus the phase-1 (unconnected endpoints) report itself.
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

/**
 * Show a report in the generic, reusable report viewer (Save/Print/Print
 * Setup, same `denum.c`-style dialog for every report phase).
 *
 * \param[in] title dialog title, e.g. "Unconnected Endpoints Report"
 * \param[in] content report body text, already fully formatted
 */
void ReportsShowText(const char *title, DynString *content);

/**
 * Menu callback (phase 1): compute the unconnected-endpoints list for the
 * current layout and show it via ReportsShowText().
 *
 * \param[in] unused menu-callback signature, unused
 */
void ReportsUnconnectedEndpoints(void *unused);

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
