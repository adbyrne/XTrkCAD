/**
 * \file   scalectrl.c
 * \brief  Scale control
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

#include <wlib.h>

#include "gtkint.h"

double
wScaleGetValue(wControl_p scale)
{
    g_assert(scale->type == B_SCALE);

    return(gtk_range_get_value(GTK_RANGE(scale->widget)));
}

void
wScaleSetValue(wControl_p scaleControl, double value)
{
    g_assert(scaleControl->type == B_SCALE);
    

    gtk_range_set_value(GTK_RANGE(scaleControl->widget), value);
    
    
}

void
ScaleValueChanged(GtkRange* range, gpointer data)
{
    double value;
    wControl_p control = (wControl_p)data;
    struct scale *scaleAttributes = CONTROL_GET_ATTRIBUTES_PTR(control, scale);

    value = wScaleGetValue(control);

    if (scaleAttributes->action) {
        scaleAttributes->action(value, control->context);
        *(scaleAttributes->valuePointer) = value;
    }

}

/**
 * Create a scale control from definition in builder
 *
 * ### Usage in dialogs, created by
 *
 * - runtime: no
 * - builder: yes
 *
 * ### Options
 *
 * \param parent    IN parent window
 * \param id        IN identifier of object
 * 
 * \returns toggle button widget
 */

wControl_p
wScaleCreate(wControl_p parent,
    const char* id,
    double* valuePointer,
    wScaleCallBack_p action,
    void *context )
{
    struct scale* scaleAttributes;

    g_assert(HASDIALOGBUILDER(parent));

    wControl_p scaleControl = wlibControlNew(B_SCALE, parent, NULL, NULL);
    scaleAttributes = CONTROL_GET_ATTRIBUTES_PTR(scaleControl, scale);

    scaleControl->context = context;
    scaleAttributes->action = action;
    scaleAttributes->valuePointer = valuePointer;

    scaleControl->widget =  wlibWidgetFromIdWarn(parent, id);

    g_signal_connect(scaleControl->widget, "value-changed", ScaleValueChanged, (gpointer)scaleControl);

    return(scaleControl);
}
