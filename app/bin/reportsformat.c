/** \file reportsformat.c
 * Pure text-formatting helpers for the Reports feature (SF #217), split out
 * from reports.c so they carry no wlib/track-database dependencies -- same
 * rationale as cars/careditlogic.c: cheaply CMocka-testable on their own,
 * without stubbing a large dialog/viewer dependency surface (see
 * unittest/carregistrytest.c's header comment for why the alternative,
 * -ffunction-sections/--gc-sections dead-code stripping, was tried and
 * reverted as unreliable across toolchains).
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

#include <stdio.h>

#include "include/reports.h"

void ReportsFormatUnconnectedList(DynString *out, const reportsEndPt_t *list,
                                  int count)
{
	char line[128];
	int i;

	for (i = 0; i < count; i++) {
		snprintf(line, sizeof line, "%6d | %8.3f | %8.3f | %7.3f\n",
		         list[i].trackId, list[i].pos.x, list[i].pos.y,
		         list[i].angle);
		DynStringCatCStr(out, line);
	}
}
