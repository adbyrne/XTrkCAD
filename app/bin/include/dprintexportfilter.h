/** \file dprintexportfilter.h
 * Shared "Filter Layers/Groups..." scope for Print and DXF/SVG export
 * (SF #789, Layer Groups phase 2). Reuses #787's reportsFilter_t /
 * ShowReportsFilterDialog() directly rather than a new type or dialog --
 * one filter instance, independent of every report's own filter and of
 * canvas selection/layer visibility.
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

#ifndef DPRINTEXPORTFILTER_H
#define DPRINTEXPORTFILTER_H

#include "dreportsfilter.h"

/** The shared Print/Export layer-group filter. Zero-initialized, so
 * unfiltered until the user opens the Filter dialog and includes at
 * least one layer -- same convention as every report's own filter. Read
 * directly with ReportsFilterActive()/ReportsFilterLayerIncluded()
 * (dreportsfilter.h); one instance shared by Print, DXF export, and SVG
 * export, not three independent ones. */
extern reportsFilter_t printExportFilter;

/** PD_BUTTON callback: open the shared Filter dialog against
 * printExportFilter. Wired from both the Print dialog (cprint.c) and the
 * File > Export menu (menu.c) so it's reachable from either workflow.
 *
 * \param unused IN unused, required by the PD_BUTTON signature
 */
void DoPrintExportFilter(void *unused);

#endif
