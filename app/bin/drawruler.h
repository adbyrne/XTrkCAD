/** @file drawruler.h
 * @brief Public interface for the ruler rendering module.
 *
 * Exposes the border size variables that drawprim.c and draw.c need for
 * coordinate conversion and clipping, and declares DrawRulerInit() which
 * must be called once from DrawInit() after the main drawing widget and DPI
 * are known.
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis
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

#ifndef DRAWRULER_H
#define DRAWRULER_H

#include "wlib.h"

/** @brief Left border strip width in pixels, computed from ruler font metrics. */
extern int lborder;

/** @brief Bottom border strip height in pixels, computed from ruler font metrics. */
extern int bborder;

/** @brief Width of one character in pixels for the ruler font. */
extern int charWidth;

/**
 * @brief Initialize ruler font, character metrics, and border sizes.
 *
 * Must be called once from DrawInit() after mainD.d and mainD.dpi are set.
 * Queries the font metrics for the monospace ruler font and stores the
 * results in lborder, bborder, and charWidth so that coordinate conversion
 * and clipping code can use them immediately.
 *
 * @param d  The main drawing widget handle, used for font metric queries.
 */
void DrawRulerInit(wDraw_p d);

void DrawRoomWalls(wBool_t drawBackground);
void DrawMarkers(void);

#endif /* DRAWRULER_H */
