/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/gtkbitmap.c,v 1.1 2009-09-23 02:51:51 dspagnol Exp $
 */
/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2009 Daniel Spagnol
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#ifndef __APPLE__
#include <malloc.h>
#endif
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "gtkint.h"


struct wBitmap_t {
	WOBJ_COMMON
};

/**
 * Create a static control for displaying a bitmap.
 *
 * \param parent IN parent window
 * \param x, y   IN position in parent window
 * \param option IN ignored for now
 * \param iconP  IN icon to use
 * \return    the control
 */

wControl_p
wBitmapCreate( wWin_p parent, wPos_t x, wPos_t y, long option, wIcon_p iconP )
{
	wBitmap_p control;
	
	control = (wBitmap_p)gtkAlloc( parent, B_BITMAP, x, y, NULL, sizeof( struct wBitmap_t ), iconP );
	gtkComputePos( (wControl_p)control );
	control->option = option;
	
	/* TODO: implement me */
	control->widget = NULL;
	
	control->h = iconP->h;
	control->w = iconP->w;
	control->data = iconP;
	
	return (wControl_p)control;
}
