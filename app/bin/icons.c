/**
 * \file   icons.c
 * \brief
 *
 * \author Martin Fischer
 */

/*  XTrackCad - Model Railroad CAD
 *  Copyright (C) 2005, 2025 Dave Bullis
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

#include <wlib.h>
#include <xtrkcad-config.h>

#include "icons.h"

wIcon_p
CreateSymbolFromResource(const char* filename)
{
	wIcon_p icon;

	icon = wIconCreatePixBufFromResource(XTRKCAD_SYMBOLS_PATH,
	                                     (const char*)filename);

	return(icon);
}


wIcon_p
CreateToolbarIconFromResource(char* filename)
{
	wIcon_p icon;

	icon = wIconCreatePixBufFromResource(XTRKCAD_ICONS_PATH, (const char*)filename);

	return(icon);
}
