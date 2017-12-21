/** \file mswstatus.c
 * Status bar
 */

/* 	XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis,
 *                2017 Martin Fischer <m_fischer@sf.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */

#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "mswint.h"

/**
 * Set the message text
 *
 * \param b IN widget
 * \param arg IN new text
 * \return
 */

void wStatusSetValue(
    wStatus_p b,
    const char * arg)
{
    wMessageSetValue((wMessage_p)b, arg);
}
/**
 * Create a window for a simple text.
 *
 * \param IN parent Handle of parent window
 * \param IN x position in x direction
 * \param IN y position in y direction
 * \param IN labelStr ???
 * \param IN width horizontal size of window
 * \param IN message message to display ( null terminated )
 * \param IN flags display options
 * \return handle for created window
 */

wStatus_p wStatusCreate(
    wWin_p	parent,
    wPos_t	x,
    wPos_t	y,
    const char 	* labelStr,
    wPos_t	width,
    const char	*message)
{
    return (wStatus_p)wMessageCreateEx(parent, x, y, labelStr, width, message, 0);
}

/**
 * Get the anticipated length of a message field
 *
 * \param testString IN string that should fit into the message box
 * \return expected width of message box
 */

wPos_t
wStatusGetWidth(const char *testString)
{
    return (wMessageGetWidth(testString));
}

/**
 * Get height of message text
 *
 * \param flags IN text properties (large or small size)
 * \return text height
 */

wPos_t wStatusGetHeight(
    long flags)
{
    return (wMessageGetHeight(flags));
}

/**
 * Set the width of the widget
 *
 * \param b IN widget
 * \param width IN  new width
 * \return
 */

void wStatusSetWidth(
    wStatus_p b,
    wPos_t width)
{
    wMessageSetWidth((wMessage_p)b, width);
}