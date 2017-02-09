/** \file button.c
 * Toolbar button creation and handling
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"
#include "i18n.h"

#define MIN_BUTTON_WIDTH (80)

/*
 *****************************************************************************
 *
 * Simple Buttons
 *
 *****************************************************************************
 */

struct wButton_t {
    WOBJ_COMMON
    GtkLabel * labelG;
    GtkWidget * imageG;
    wButtonCallBack_p action;
    int busy;
    int recursion;
};

/**
 * Set the state of the button
 *
 * \param bb IN the button
 * \param value IN TRUE for active, FALSE for inactive
 */

void wButtonSetBusy(wButton_p bb, int value)
{
    bb->recursion++;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bb->widget), value);
    bb->recursion--;
    bb->busy = value;
}

/**
 * Set the label of a button, does also allow to set an icon
 *
 * \param widget IN
 * \param option IN
 * \param labelStr IN
 * \param labelG IN
 * \param imageG IN
 */

void wlibSetLabel(
    GtkWidget *widget,
    long option,
    const char * labelStr,
    GtkLabel * * labelG,
    GtkWidget * * imageG)
{
    wIcon_p bm;
    GdkBitmap * mask;

    if (widget == 0) {
        abort();
    }

    if (labelStr) {
        if (option&BO_ICON) {
            GdkPixbuf *pixbuf;

            bm = (wIcon_p)labelStr;

            if (bm->gtkIconType == gtkIcon_pixmap) {
                pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)bm->bits);
            } else {
                pixbuf = wlibPixbufFromXBM( bm );
            }
            if (*imageG==NULL) {
                *imageG = gtk_image_new_from_pixbuf(pixbuf);
                gtk_container_add(GTK_CONTAINER(widget), *imageG);
                gtk_widget_show(*imageG);
            } else {
                gtk_image_set_from_pixbuf(GTK_IMAGE(*imageG), pixbuf);
            }

            g_object_unref(pixbuf);
        } else {
            if (*labelG==NULL) {
                *labelG = (GtkLabel*)gtk_label_new(wlibConvertInput(labelStr));
                gtk_container_add(GTK_CONTAINER(widget), (GtkWidget*)*labelG);
                gtk_widget_show((GtkWidget*)*labelG);
            } else {
                gtk_label_set_text(*labelG, wlibConvertInput(labelStr));
            }
        }
    }
}

/**
 * Change only the text label of a button
 * \param bb IN button handle
 * \param labelStr IN new label string
 */

void wButtonSetLabel(wButton_p bb, const char * labelStr)
{
    wlibSetLabel(bb->widget, bb->option, labelStr, &bb->labelG, &bb->imageG);
}

/**
 * Perform the user callback function
 *
 * \param bb IN button handle
 */

void wlibButtonDoAction(
    wButton_p bb)
{
    if (bb->action) {
        bb->action(bb->data);
    }
}

/**
 * Signal handler for button push
 * \param widget IN the widget
 * \param value IN the button handle (same as widget???)
 */

static void pushButt(
    GtkWidget *widget,
    gpointer value)
{
    wButton_p b = (wButton_p)value;

    if (debugWindow >= 2) {
        printf("%s button pushed\n", b->labelStr?b->labelStr:"No label");
    }

    if (b->recursion) {
        return;
    }

    if (b->action) {
        b->action(b->data);
    }

    if (!b->busy) {
        b->recursion++;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->widget), FALSE);
        b->recursion--;
    }
}

/**
 * Create a button
 *
 * \param parent IN parent window
 * \param x IN X-position
 * \param y IN Y-position
 * \param helpStr IN Help string
 * \param labelStr IN Label
 * \param option IN Options
 * \param width IN Width of button
 * \param action IN Callback
 * \param data IN User data as context
 * \returns button widget
 */

wButton_p wButtonCreate(
    wWin_p	parent,
    wPos_t	x,
    wPos_t	y,
    const char 	* helpStr,
    const char	* labelStr,
    long 	option,
    wPos_t 	width,
    wButtonCallBack_p action,
    void 	* data)
{
    wButton_p b;
    b = wlibAlloc(parent, B_BUTTON, x, y, labelStr, sizeof *b, data);
    b->option = option;
    b->action = action;
    wlibComputePos((wControl_p)b);
    b->widget = gtk_toggle_button_new();
    g_signal_connect(GTK_OBJECT(b->widget), "clicked",
                     G_CALLBACK(pushButt), b);

    if (width > 0) {
        gtk_widget_set_size_request(b->widget, width, -1);
    }

    wButtonSetLabel(b, labelStr);
    gtk_fixed_put(GTK_FIXED(parent->widget), b->widget, b->realX, b->realY);

    if (option & BB_DEFAULT) {
        gtk_widget_set_can_default(b->widget, GTK_CAN_DEFAULT);
        gtk_widget_grab_default(b->widget);
        gtk_window_set_default(GTK_WINDOW(parent->gtkwin), b->widget);
    }

    wlibControlGetSize((wControl_p)b);

    if (width == 0 && b->w < MIN_BUTTON_WIDTH && (b->option&BO_ICON)==0) {
        b->w = MIN_BUTTON_WIDTH;
        gtk_widget_set_size_request(b->widget, b->w, b->h);
    }

    gtk_widget_show(b->widget);
    wlibAddButton((wControl_p)b);
    wlibAddHelpString(b->widget, helpStr);
    return b;
}


/*
 *****************************************************************************
 *
 * Choice Boxes
 *
 *****************************************************************************
 */

struct wChoice_t {
    WOBJ_COMMON
    long *valueP;
    wChoiceCallBack_p action;
    int recursion;
};


/**
 * Get the state of a group of buttons. If the group consists of
 * radio buttons, the return value is the index of the selected button
 * or -1 for none. If toggle buttons are checked, a bit is set for each
 * button that is active.
 *
 * \param bc IN
 * \returns state of group
 */

static long choiceGetValue(
    wChoice_p bc)
{
    GList * child, * children;
    long value, inx;

    if (bc->type == B_TOGGLE) {
        value = 0;
    } else {
        value = -1;
    }

    for (children=child=gtk_container_get_children(GTK_CONTAINER(bc->widget)),inx=0;
            child; child=child->next,inx++) {
        if (gtk_toggle_button_get_active(child->data)) {
            if (bc->type == B_TOGGLE) {
                value |= (1<<inx);
            } else {
                value = inx;
            }
        }
    }

    if (children) {
        g_list_free(children);
    }

    return value;
}

/**
 * Set the active radio button in a group
 *
 * \param bc IN button group
 * \param value IN index of active button
 */

void wRadioSetValue(
    wChoice_p bc,		/* Radio box */
    long value)		/* Value */
{
    GList * child, * children;
    long inx;

    for (children=child=gtk_container_get_children(GTK_CONTAINER(bc->widget)),inx=0;
            child; child=child->next,inx++) {
        if (inx == value) {
            bc->recursion++;
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(child->data), TRUE);
            bc->recursion--;
        }
    }

    if (children) {
        g_list_free(children);
    }
}

/**
 * Get the active button from a group of radio buttons
 *
 * \param bc IN
 * \returns
 */

long wRadioGetValue(
    wChoice_p bc)		/* Radio box */
{
    return choiceGetValue(bc);
}

/**
 * Set a group of toggle buttons from a bitfield
 *
 * \param bc IN button group
 * \param value IN bitfield
 */

void wToggleSetValue(
    wChoice_p bc,		/* Toggle box */
    long value)		/* Values */
{
    GList * child, * children;
    long inx;
    bc->recursion++;

    for (children=child=gtk_container_get_children(GTK_CONTAINER(bc->widget)),inx=0;
            child; child=child->next,inx++) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(child->data),
                                     (value&(1<<inx))!=0);
    }

    if (children) {
        g_list_free(children);
    }

    bc->recursion--;
}


/**
 * Get the active buttons from a group of toggle buttons
 *
 * \param b IN
 * \returns
 */

long wToggleGetValue(
    wChoice_p b)		/* Toggle box */
{
    return choiceGetValue(b);
}

/**
 * Signal handler for button selection in radio buttons and toggle
 * button group
 *
 * \param widget IN the button group
 * \param b IN user data (button group????)
 * \returns always 1
 */

static int pushChoice(
    GtkWidget *widget,
    gpointer b)
{
    wChoice_p bc = (wChoice_p)b;
    long value = choiceGetValue(bc);

    if (debugWindow >= 2) {
        printf("%s choice pushed = %ld\n", bc->labelStr?bc->labelStr:"No label",
               value);
    }

    if (bc->type == B_RADIO &&
            !(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))) {
        return 1;
    }

    if (bc->recursion) {
        return 1;
    }

    if (bc->valueP) {
        *bc->valueP = value;
    }

    if (bc->action) {
        bc->action(value, bc->data);
    }

    return 1;
}

/**
 * Signal handler used to draw a frame around a widget, used to visually
 * group several buttons together
 *
 * \param b IN  widget
 */

static void choiceRepaint(
    wControl_p b)
{
    wChoice_p bc = (wChoice_p)b;

    if (gtk_widget_get_visible(b->widget)) {
        wlibDrawBox(bc->parent, wBoxBelow, bc->realX-1, bc->realY-1, bc->w+1, bc->h+1);
    }
}

/**
 * Create a group of radio buttons.
 *
 * \param parent IN parent window
 * \param x IN X-position
 * \param y IN Y-position
 * \param helpStr IN Help string
 * \param labelStr IN Label
 * \param option IN Options
 * \param labels IN Labels
 * \param valueP IN Selected value
 * \param action IN Callback
 * \param data IN User data as context
 * \returns radio button widget
 */

wChoice_p wRadioCreate(
    wWin_p	parent,
    wPos_t	x,
    wPos_t	y,
    const char 	* helpStr,
    const char	* labelStr,
    long	option,
    const char	**labels,
    long	*valueP,
    wChoiceCallBack_p action,
    void 	*data)
{
    wChoice_p b;
    const char ** label;
    GtkWidget *butt0=NULL, *butt;

    if ((option & BC_NOBORDER)==0) {
        if (x>=0) {
            x++;
        } else {
            x--;
        }

        if (y>=0) {
            y++;
        } else {
            y--;
        }
    }

    b = wlibAlloc(parent, B_RADIO, x, y, labelStr, sizeof *b, data);
    b->option = option;
    b->action = action;
    b->valueP = valueP;
    wlibComputePos((wControl_p)b);

    if (option&BC_HORZ) {
        b->widget = gtk_hbox_new(FALSE, 0);
    } else {
        b->widget = gtk_vbox_new(FALSE, 0);
    }

    if (b->widget == 0) {
        abort();
    }

    for (label=labels; *label; label++) {
        butt = gtk_radio_button_new_with_label(
                   butt0?gtk_radio_button_get_group(GTK_RADIO_BUTTON(butt0)):NULL, _(*label));

        if (butt0==NULL) {
            butt0 = butt;
        }

        gtk_box_pack_start(GTK_BOX(b->widget), butt, TRUE, TRUE, 0);
        gtk_widget_show(butt);
        g_signal_connect(GTK_OBJECT(butt), "toggled",
                         G_CALLBACK(pushChoice), b);
        wlibAddHelpString(butt, helpStr);
    }

    if (option & BB_DEFAULT) {
        gtk_widget_set_can_default(b->widget, TRUE);
        gtk_widget_grab_default(b->widget);
    }

    if (valueP) {
        wRadioSetValue(b, *valueP);
    }

    if ((option & BC_NOBORDER)==0) {
        b->repaintProc = choiceRepaint;
        b->w += 2;
        b->h += 2;
    }

    gtk_fixed_put(GTK_FIXED(parent->widget), b->widget, b->realX, b->realY);
    wlibControlGetSize((wControl_p)b);

    if (labelStr) {
        b->labelW = wlibAddLabel((wControl_p)b, labelStr);
    }

    gtk_widget_show(b->widget);
    wlibAddButton((wControl_p)b);
    return b;
}

/**
 * Create a group of toggle buttons.
 *
 * \param parent IN parent window
 * \param x IN X-position
 * \param y IN Y-position
 * \param helpStr IN Help string
 * \param labelStr IN Label
 * \param option IN Options
 * \param labels IN Labels
 * \param valueP IN Selected value
 * \param action IN Callback
 * \param data IN User data as context
 * \returns toggle button widget
 */

wChoice_p wToggleCreate(
    wWin_p	parent,
    wPos_t	x,
    wPos_t	y,
    const char 	* helpStr,
    const char	* labelStr,
    long	option,
    const char	**labels,
    long	*valueP,
    wChoiceCallBack_p action,
    void 	*data)
{
    wChoice_p b;
    const char ** label;

    if ((option & BC_NOBORDER)==0) {
        if (x>=0) {
            x++;
        } else {
            x--;
        }

        if (y>=0) {
            y++;
        } else {
            y--;
        }
    }

    b = wlibAlloc(parent, B_TOGGLE, x, y, labelStr, sizeof *b, data);
    b->option = option;
    b->action = action;
    wlibComputePos((wControl_p)b);

    if (option&BC_HORZ) {
        b->widget = gtk_hbox_new(FALSE, 0);
    } else {
        b->widget = gtk_vbox_new(FALSE, 0);
    }

    if (b->widget == 0) {
        abort();
    }

    for (label=labels; *label; label++) {
        GtkWidget *butt;

        butt = gtk_check_button_new_with_label(_(*label));
        gtk_box_pack_start(GTK_BOX(b->widget), butt, TRUE, TRUE, 0);
        gtk_widget_show(butt);
        g_signal_connect(GTK_OBJECT(butt), "toggled",
                         G_CALLBACK(pushChoice), b);
        wlibAddHelpString(butt, helpStr);
    }

    if (valueP) {
        wToggleSetValue(b, *valueP);
    }

    if ((option & BC_NOBORDER)==0) {
        b->repaintProc = choiceRepaint;
        b->w += 2;
        b->h += 2;
    }

    gtk_fixed_put(GTK_FIXED(parent->widget), b->widget, b->realX, b->realY);
    wlibControlGetSize((wControl_p)b);

    if (labelStr) {
        b->labelW = wlibAddLabel((wControl_p)b, labelStr);
    }

    gtk_widget_show(b->widget);
    wlibAddButton((wControl_p)b);
    return b;
}
