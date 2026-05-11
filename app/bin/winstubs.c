/** \file winstubs.c
 * Windows-only stubs for GTK3/parts functions not built on Windows.
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2025 Dave Bullis
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

#include "common.h"

/* PriceListInit is defined in parts/dpricels.c which is GTK3-only and not
   built on Windows. Return NULL so the menu item is simply absent. */
addButtonCallBack_t PriceListInit(void)
{
	return NULL;
}
