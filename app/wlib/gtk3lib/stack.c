/**
 * \file   stack.c
 * \brief  
 *
 * \author Martin Fischer
 */

 /*  XTrackCad - Model Railroad CAD
  *  Copyright (C) 2005, 2024 Dave Bullis
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


#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"
#include <wlib.h>
//#include "stack.h"

void
wStackPageShow(wControl_p stack, const char* pageName)
{
    gtk_stack_set_visible_child_name(GTK_STACK(stack->widget), pageName);
}

/**
 * Create a stack
 *
 * ### Usage in dialogs, created by
 *
 * - runtime: no
 * - builder: yes
 *
 * ### Options
 *
 * \param parent IN parent window
 * \param x IN X-position
 * \param y IN Y-position
 * \param helpStr IN Help string
 * \param labelStr IN Label
 * \param option IN Options
 * \param width IN Width of button
 * \param action IN Callback
 * \param styleContext IN User styleContext
 * \returns button widget

 *
 */

wControl_p wStackCreate(
    wControl_p	parent,
    wWinPix_t	x,
    wWinPix_t	y,
    const char* helpStr,
    const char* labelStr,
    long 	option,
    wWinPix_t 	width,
    wButtonCallBack_p action,
    void* context)

{
    wControl_p newStack = wlibControlNew(B_STACK, parent, helpStr, context);
    struct stack* privateStack = CONTROL_GET_ATTRIBUTES_PTR(newStack, stack);

    newStack->widget = wlibWidgetFromIdWarn(parent, helpStr);
    privateStack->callback = action;

    return(newStack);
}


