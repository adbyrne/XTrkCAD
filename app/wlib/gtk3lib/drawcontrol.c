/**
 * \file   drawcontrol.c
 * \brief  Drawing area
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
#include <cairo.h>

#include <wlib.h>
#include "gtkint.h"

// Trace low level drawing actions
static int iDrawLog = 3;
static long lDrawCnt = 0;

/**
 * Check whether control should receive focus and grab it if so.
 *
 * \param control	pointer to draw control
 */

static inline void
CheckGrabFocus(wControl_p control)
{
	if (!(control->attributes.draw.option & BD_NOFOCUS)) {
		gtk_widget_grab_focus(control->widget);
	}
}

/**
 * Paint a surface to an existing cairo context. The surface is drawn on top
 * of the existing layer.
 *
 * \param cr		cairo context
 * \param surface	surface to paint
 */

void
PaintOverSurface(cairo_t* cr, cairo_surface_t* surface)
{
	cairo_set_source_surface(cr, surface, 0, 0);
	cairo_paint(cr);
}

/**
 * Clear a cairo surface.
 *
 * \param surface
 */

void
ClearSurface(cairo_surface_t *surface)
{
	cairo_t* cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cr);
	cairo_destroy(cr);
}


static gboolean draw_event(
        GtkWidget* widget,
        cairo_t* cr,
        wControl_p drawControl)
{
	struct draw* drawAttributes;

	if (iDrawLog >= 4) {
		printf("draw_event %ldx\n", lDrawCnt++);
	}

	drawAttributes = CONTROL_GET_ATTRIBUTES_PTR(drawControl, draw);
	PaintOverSurface(cr, drawAttributes->surface);
	PaintOverSurface(cr, drawAttributes->temp_surface);

	ClearSurface(drawAttributes->temp_surface);

	return TRUE;
}

/**
 * Clear a surface by painting to all black.
 *
 * \param surface	surface to be cleared
 */

//static void
//clear_surface (cairo_surface_t * surface)
//{
//	cairo_t *cr;
//
//	cr = cairo_create (surface);
//	cairo_set_source_rgb (cr, 0, 0, 0);
//	cairo_paint (cr);
//	cairo_destroy (cr);
//}

/**
 * Create and initialize a new surface for a window.
 *
 * \param widget		the window
 * \param oldSurface	previously created surface, NULL if none
 * \return				the new surface
 */

static cairo_surface_t*
CreateNewSurface(GtkWidget* widget, cairo_surface_t* oldSurface)
{
	cairo_surface_t* newSurface;

	if (oldSurface) {
		cairo_surface_destroy(oldSurface);
	}
	/** \todo can the allocation be used instead of API call? */
	newSurface = gdk_window_create_similar_surface(gtk_widget_get_window(widget),
	             CAIRO_CONTENT_COLOR_ALPHA,
	             gtk_widget_get_allocated_width(widget),
	             gtk_widget_get_allocated_height(widget));

	/* Initialize the surface */
	ClearSurface(newSurface);

	return(newSurface);
}

/**
 * Signal handler for configure_event. Called when size or position of
 * widget was changed. The surfaces are recreated for the new window size.
 *
 * \param widget	see GTK docs
 * \param event
 * \param drawControl		the control
 * \return
 */

static gboolean
draw_configure_event(
        GtkWidget* widget,
        GdkEventConfigure* event,
        wControl_p drawControl)
{
	GtkAllocation alloc;
	struct draw* drawAttributes = CONTROL_GET_ATTRIBUTES_PTR(drawControl, draw);

	gtk_widget_get_allocation(widget, &alloc);
	/**   \todo Use width and height from event if possible */

	if ((drawAttributes->width != alloc.width) ||
	    drawAttributes->height != alloc.height) {
		drawAttributes->width = alloc.width;
		drawAttributes->height = alloc.height;

		drawAttributes->surface = CreateNewSurface(widget,
		                          drawAttributes->surface);
		drawAttributes->temp_surface = CreateNewSurface(widget,
		                               drawAttributes->temp_surface);
	}

	/* We've handled the configure event, no need for further processing. */
	return TRUE;
}

static const char* actionNames[] = { "None", "Move", "LDown", "LDrag", "LUp", "RDown", "RDrag", "RUp", "Text", "ExtKey", "WUp", "WDown", "DblL", "ModK", "ScrU", "ScrD", "ScrL", "ScrR" };

/**
 * Handler for scroll events, ie mouse wheel activity
 */

/**
 * \todo this code seems overly complicated. The action is only executed after
 * several timeout cycles. So why is the timer not set to that larger timeout
 * at start?
 *
 */
static int scrollTimer;
static int timer_busy_count;

#define MIN_TIMER_INTERVALS 1

static int
ScrollTimerPop(wControl_p drawControl)
{
	struct draw* drawAttributes = CONTROL_GET_ATTRIBUTES_PTR(drawControl, draw);

	if (timer_busy_count > MIN_TIMER_INTERVALS) {
		timer_busy_count = 0;
		scrollTimer = 0;
	} else {
		timer_busy_count++;
		return TRUE;
	}
	if (iDrawLog >= 2) {
		printf("%s-Pop\n", actionNames[drawAttributes->lastAction]);
	}
	drawAttributes->action(drawControl, drawControl->context,
	                       drawAttributes->lastAction,
	                       (wDrawPix_t)0, (wDrawPix_t)0);

	return FALSE;
}

/**
 * Handle mouse wheel activity. For normal wheel activity the wheel actions
 * are  called to. If a modifier key is pressed the scroll actions are called
 * to.
 * For scroll actions a delay is added to reduce number of redraws.
 *
 * \param widget		see GTK docs
 * \param event
 * \param drawControl	the control
 * \return
 */

#define DELAYTIME 25
#define ISPRESSEDSCROLLMODIFIER(event) ((event->state) & (GDK_SHIFT_MASK|GDK_BUTTON2_MASK|GDK_MOD1_MASK))

static gint
draw_scroll_event(GtkWidget* widget, GdkEventScroll* event,
                  wControl_p drawControl)
{
	wAction_t action = 0;
	struct draw* drawAttributes = CONTROL_GET_ATTRIBUTES_PTR(drawControl, draw);

	if (ISPRESSEDSCROLLMODIFIER(event)) {

		switch (event->direction) {
		case GDK_SCROLL_UP:
			if (event->state & GDK_CONTROL_MASK) {
				drawAttributes->lastAction = wActionScrollRight;
			} else {
				drawAttributes->lastAction = wActionScrollUp;
			}
			break;
		case GDK_SCROLL_DOWN:
			if (event->state & GDK_CONTROL_MASK) {
				drawAttributes->lastAction = wActionScrollLeft;
			} else {
				drawAttributes->lastAction = wActionScrollDown;
			}
			break;
		case GDK_SCROLL_LEFT:
			drawAttributes->lastAction = wActionScrollLeft;
			break;
		case GDK_SCROLL_RIGHT:
			drawAttributes->lastAction = wActionScrollRight;
			break;
		default:
			return TRUE;
			break;
		}

		if (!scrollTimer) {				// Start a timer if not already done
			timer_busy_count = 0;
			scrollTimer = g_timeout_add(DELAYTIME, (GSourceFunc)ScrollTimerPop,
			                            drawControl);
		}
		return TRUE;

	} else {

		switch (event->direction) {
		case GDK_SCROLL_UP:
			action = wActionWheelUp;
			break;
		case GDK_SCROLL_DOWN:
			action = wActionWheelDown;
			break;
		case GDK_SCROLL_LEFT:
			return TRUE;
			break;
		case GDK_SCROLL_RIGHT:
			return TRUE;
			break;
		default:
			break;
		}
	}

	if (action != 0) {
		if (iDrawLog >= 2) {
			printf("%s[%ldx%ld]\n", actionNames[action],
			       drawAttributes->lastX, drawAttributes->lastY);
		}
		drawAttributes->action(drawControl, drawControl->context,
		                       action,
		                       (wDrawPix_t)drawAttributes->lastX,
		                       (wDrawPix_t)drawAttributes->lastY);
	}

	return TRUE;
}


// \todo Check necessity
//static gint draw_leave_event(
//        GtkWidget *widget,
//        GdkEvent * event )
//{
//	wlibHelpHideBalloon();
//	return TRUE;
//}

/**
 * Translate screen to world coordinates. Screen has origin at top left, world
 * at bottom left of drawing area.
 *
 * \param area	pointer to drawing area
 */

static inline void
Screen2WorldCoord(struct draw* area)
{
	area->lastY = area->height - 1 - area->lastY;
}

/**
 * Handler for mouse button clicks. Clicks on a 2 or 3 button mouse are
 * translated to wlib actions and the action handler is called.
 *
 * \param widget		GTK drawing area
 * \param event			see GTK docs
 * \param drawControl	the control for the drawing area
 *
 * \return TRUE is event is action, FALSE to propagate further
 */

static gint
draw_button_event(GtkWidget* widget, GdkEventButton* event,
                  wControl_p drawControl)
{
	wAction_t action = 0;
	struct draw* drawAttributes = CONTROL_GET_ATTRIBUTES_PTR(drawControl, draw);

	if (drawAttributes->action == NULL) {
		return TRUE;
	}

	drawAttributes->lastX = (long)event->x;
	drawAttributes->lastY = (long)event->y;
	Screen2WorldCoord(drawAttributes);

	switch (event->button) {
	case 1: /* left mouse button */
	case 2: /* middle mouse button */
		if (event->type == GDK_2BUTTON_PRESS) {
			action = wActionLDownDouble;
			break;
		}
		if (event->type == GDK_BUTTON_PRESS) {
			action = wActionLDown;
		} else {
			action = wActionLUp;
		}
		break;
	case 3: /* right mouse button */
		if (event->type == GDK_BUTTON_PRESS) {
			action = wActionRDown;
		} else {
			action = wActionRUp;
		}
		break;
	default:
		return FALSE;
	}

	if (iDrawLog >= 3) {
		printf("%ld: %s[%ldx%ld]\n", lDrawCnt++, actionNames[action],
		       drawAttributes->lastX,
		       drawAttributes->lastY);
	}

	drawAttributes->action(drawControl, drawControl->context, action,
	                       drawAttributes->lastX,
	                       drawAttributes->lastY);

	if (!(drawAttributes->option & BD_NOFOCUS)) {
		gtk_widget_grab_focus(drawControl->widget);
	}
	return FALSE;
}

/**
 * Mouse pointer moves over the drawing area. If a mouse button being pressed
 * a drag operation is assumed. The is translated to an action and the
 * handler is called.
 *
 * \todo it might be better to use the drag_* virtual methods available in GTK
 *
 * \param widget		GTK drawing area
 * \param event			see GTK docs
 * \param drawControl	the control for the drawing area
 * \return
 */

static gint
draw_motion_event(GtkWidget* widget, GdkEventMotion* event,
                  wControl_p drawControl)
{
	long x, y;
	GdkModifierType state;
	wAction_t action;
	struct draw* drawAttributes = CONTROL_GET_ATTRIBUTES_PTR(drawControl, draw);

	if (drawAttributes->action == NULL) {
		return TRUE;
	}

	/** \todo hint mask is deprecated and seems to be unnecessary here, test
	* replacement or remove */

	if (event->is_hint) {
		gdk_window_get_device_position(event->window, event->device,
		                               &x, &y, &state);
	} else {
		GdkModifierType modifiers;
		modifiers = gtk_accelerator_get_default_mod_mask();
		state = event->state & modifiers;
		x = (long)event->x;
		y = (long)event->y;
	}

	drawAttributes->lastX = x;
	drawAttributes->lastY = y;
	Screen2WorldCoord(drawAttributes);

	if (state & GDK_BUTTON1_MASK) {
		action = wActionLDrag;
	} else if (state & GDK_BUTTON2_MASK) {
		action = wActionLDrag;
	} else if (state & GDK_BUTTON3_MASK) {
		action = wActionRDrag;
	} else {
		action = wActionMove;
	}

	if (iDrawLog >= 3) {
		printf("%lx: %s[%ldx%ld] %s\n", lDrawCnt++, actionNames[action],
		       drawAttributes->lastX, drawAttributes->lastY,
		       event->is_hint ? "<Hint>" : "<>");
	}

	drawAttributes->action(drawControl, drawControl->context, action,
	                       drawAttributes->lastX,
	                       drawAttributes->lastY);

	CheckGrabFocus(drawControl);
	return TRUE;
}


static gint draw_char_release_event(
        GtkWidget* widget,
        GdkEventKey* event,
        wControl_p drawControl)
{
	guint key = event->keyval;
	wModKey_e modKey = wModKey_None;
	struct draw * drawAttributes = CONTROL_GET_ATTRIBUTES_PTR(drawControl, draw);

	switch (key) {
	case GDK_KEY_Alt_L:     modKey = wModKey_Alt; break;
	case GDK_KEY_Alt_R:     modKey = wModKey_Alt; break;
	case GDK_KEY_Shift_L:	modKey = wModKey_Shift; break;
	case GDK_KEY_Shift_R:	modKey = wModKey_Shift; break;
	case GDK_KEY_Control_L:	modKey = wModKey_Ctrl; break;
	case GDK_KEY_Control_R:	modKey = wModKey_Ctrl; break;
	default:;
	}

	UpdateModifierKeyState(event);

	if (modKey != wModKey_None && (drawAttributes->option & BD_MODKEYS)) {
		drawAttributes->action(drawControl, drawControl->context,
		                       wActionModKey + ((int)modKey << 8),
		                       (wDrawPix_t)drawAttributes->lastX, (wDrawPix_t)drawAttributes->lastY);
		CheckGrabFocus(drawControl);
		return TRUE;
	} else {
		return FALSE;
	}
	return FALSE;
}

/**
 * .
 *
 * \param widget
 * \param event
 * \param drawControl
 * \return
 */

// check for alphanumeric key without modifier key being pressed
#define ISALPHANUMERICKEY(key) ((key) <= 0xFF && !(event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)))

static gint draw_char_event(
        GtkWidget* widget,
        GdkEventKey* event,
        wControl_p drawControl)
{
	GdkModifierType modifiers = gtk_accelerator_get_default_mod_mask();;
	struct draw* drawAttributes = CONTROL_GET_ATTRIBUTES_PTR(drawControl, draw);
	guint key = event->keyval;
	wAccelKey_e functionKey = wAccelKey_None;
	wModKey_e modKey = wModKey_None;
	wAction_t keyboardAction;

	g_assert(drawAttributes->action != NULL);

	switch (key) {
	case GDK_KEY_Escape:	key = 0x1B; break;
	case GDK_KEY_Return:
		if (((event->state & modifiers) == GDK_CONTROL_MASK)
		    || ((event->state & modifiers) == GDK_MOD1_MASK)) {
			functionKey =
			        wAccelKey_LineFeed;        //If Return plus Control or Alt send in LineFeed
		}
		key = 0x0D;
		break;
	case GDK_KEY_Linefeed:	key = 0x0A; break;
	case GDK_KEY_Tab:		key = 0x09; break;
	case GDK_KEY_BackSpace:	key = 0x08; break;
	case GDK_KEY_Delete:    functionKey = wAccelKey_Del; break;
	case GDK_KEY_Insert:    functionKey = wAccelKey_Ins; break;
	case GDK_KEY_Home:      functionKey = wAccelKey_Home; break;
	case GDK_KEY_End:       functionKey = wAccelKey_End; break;
	case GDK_KEY_Page_Up:   functionKey = wAccelKey_Pgup; break;
	case GDK_KEY_Page_Down: functionKey = wAccelKey_Pgdn; break;
	case GDK_KEY_Up:        functionKey = wAccelKey_Up; break;
	case GDK_KEY_Down:      functionKey = wAccelKey_Down; break;
	case GDK_KEY_Right:     functionKey = wAccelKey_Right; break;
	case GDK_KEY_Left:      functionKey = wAccelKey_Left; break;
	case GDK_KEY_F1:        functionKey = wAccelKey_F1; break;
	case GDK_KEY_F2:        functionKey = wAccelKey_F2; break;
	case GDK_KEY_F3:        functionKey = wAccelKey_F3; break;
	case GDK_KEY_F4:        functionKey = wAccelKey_F4; break;
	case GDK_KEY_F5:        functionKey = wAccelKey_F5; break;
	case GDK_KEY_F6:        functionKey = wAccelKey_F6; break;
	case GDK_KEY_F7:        functionKey = wAccelKey_F7; break;
	case GDK_KEY_F8:        functionKey = wAccelKey_F8; break;
	case GDK_KEY_F9:        functionKey = wAccelKey_F9; break;
	case GDK_KEY_F10:       functionKey = wAccelKey_F10; break;
	case GDK_KEY_F11:       functionKey = wAccelKey_F11; break;
	case GDK_KEY_F12:       functionKey = wAccelKey_F12; break;
	case GDK_KEY_Alt_L:     modKey = wModKey_Alt; break;
	case GDK_KEY_Alt_R:     modKey = wModKey_Alt; break;
	case GDK_KEY_Shift_L:	modKey = wModKey_Shift; break;
	case GDK_KEY_Shift_R:	modKey = wModKey_Shift; break;
	case GDK_KEY_Control_L:	modKey = wModKey_Ctrl; break;
	case GDK_KEY_Control_R:	modKey = wModKey_Ctrl; break;
	default:;
	}

	UpdateModifierKeyState(event);

	if (functionKey) {
		keyboardAction = wActionExtKey + ((int)functionKey << 8);
	} else if (ISALPHANUMERICKEY(key)) {
		keyboardAction = wActionText + (key << 8);
	} else if (modKey && (drawAttributes->option & BD_MODKEYS)) {
		keyboardAction = wActionModKey + ((int)modKey << 8);
	} else {
		return FALSE;
	}
	/**
	 * \todo the following condition is always false as key is never set to
	 * wAccelKey_Up wAccelKey_Left or any value in between. So this code has
	 * been disabled for further examination
	 */

	//else if ((key >= wAccelKey_Up) && (key <= wAccelKey_Left) && drawAttributes->action) {
	//	drawAttributes->action(drawControl, drawControl->context, wActionText + (key << 8), (wDrawPix_t)drawAttributes->lastX,
	//		(wDrawPix_t)drawAttributes->lastY);
	//	CheckGrabFocus(drawControl);
	//	return TRUE;
	//}
	drawAttributes->action(drawControl,
	                       drawControl->context,
	                       keyboardAction,
	                       (wDrawPix_t)drawAttributes->lastX,
	                       (wDrawPix_t)drawAttributes->lastY
	                      );

	CheckGrabFocus(drawControl);
	return TRUE;
}

/**
 * Handle realize signal. A cross is created as the new cursor.
 *
 * \param widget	see GTK documentation
 * \param attributes		unused
 */

void
draw_realize(GtkWidget* widget,
             gpointer unused)
{
	GdkCursor* cursor;
	cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_TCROSS);
	gdk_window_set_cursor(gtk_widget_get_window(widget), cursor);
	g_object_unref(cursor);
}

/*******************************************************************************
 *
 * Create
 *
*******************************************************************************/

/**
 * Create a drawing area
 *
 * ### Usage in dialogs
 *
 * - Generated: no
 * - Builder: yes
 *
 * ### Options
 *
 *	\param IN parent		Parent window
 *	\param IN x, y			position
 *	\param IN helpStr		Help string
 *	\param IN option		Options
 *	\param IN width, height	Size
 *	\param IN attributes			Context
 *	\param IN redraw		pointer to redraw function
 *	\param IN action		pointer to action function
 */


wControl_p wDrawCreate(
        wControl_p	parent,
        wWinPix_t	x,
        wWinPix_t	y,
        const char* helpStr,
        long	option,
        wWinPix_t	width,
        wWinPix_t	height,
        void* context,
        wDrawRedrawCallBack_p redraw,
        wDrawActionCallBack_p action)
{
	if (iDrawLog >= 1)
		printf("%ld wDrawCreate %s %ldx%ld %ld+%ld %lx\n",
		       lDrawCnt++, helpStr, x, y, width, height, option);
	wControl_p drawControl;
	struct draw* drawAttributes;

	drawControl = wlibControlNew(B_DRAW, parent, helpStr, context);
	drawAttributes = CONTROL_GET_ATTRIBUTES_PTR(drawControl, draw);
	drawAttributes->action = action;
	drawAttributes->redraw = redraw;
	drawAttributes->bTempMode = FALSE;
	drawAttributes->option = option;
	drawAttributes->dpi = gdk_screen_get_resolution(gdk_screen_get_default());

	if (HASDIALOGBUILDER(parent)) {
		drawControl->widget = wlibWidgetFromIdWarn(parent, helpStr);
	} else {
		/**
		\todo generate drawing area from code if necessary
				wlibComputePos( (wControl_p)drawControl );
				drawControl->widget = gtk_drawing_area_new();
				gtk_widget_set_size_request( GTK_WIDGET(drawControl->widget), width, height );

				gtk_fixed_put( GTK_FIXED(parent->widget), drawControl->widget, drawControl->realX, drawControl->realY );
				wlibControlGetSize( (wControl_p)drawControl );

				drawControl->maxW = drawControl->w = width;
				drawControl->maxH = drawControl->h = height;
				*/
	}

	g_signal_connect((drawControl->widget), "realize",
	                 G_CALLBACK(draw_realize), drawControl);
	g_signal_connect((drawControl->widget), "draw",
	                 G_CALLBACK(draw_event), drawControl);
	g_signal_connect((drawControl->widget), "configure_event",
	                 G_CALLBACK(draw_configure_event), drawControl);
	g_signal_connect((drawControl->widget), "motion_notify_event",
	                 G_CALLBACK(draw_motion_event), drawControl);
	g_signal_connect((drawControl->widget), "button_press_event",
	                 G_CALLBACK(draw_button_event), drawControl);
	g_signal_connect((drawControl->widget), "button_release_event",
	                 G_CALLBACK(draw_button_event), drawControl);
	g_signal_connect((drawControl->widget), "scroll_event",
	                 G_CALLBACK(draw_scroll_event), drawControl);
	g_signal_connect((drawControl->widget), "key_press_event",
	                 G_CALLBACK(draw_char_event), drawControl);
	g_signal_connect((drawControl->widget), "key_release_event",
	                 G_CALLBACK(draw_char_release_event), drawControl);
	//g_signal_connect ((drawControl->widget), "leave_notify_event",
	//                  G_CALLBACK( draw_leave_event), drawControl);

	gtk_widget_add_events(drawControl->widget,
	                      GDK_BUTTON_PRESS_MASK
	                      | GDK_BUTTON_RELEASE_MASK
	                      //						   | GDK_LEAVE_NOTIFY_MASK
	                      | GDK_SCROLL_MASK
	                      | GDK_POINTER_MOTION_MASK
	                      | GDK_POINTER_MOTION_HINT_MASK
	                      | GDK_KEY_PRESS_MASK
	                      | GDK_KEY_RELEASE_MASK);

	gtk_widget_set_can_focus(drawControl->widget, !(option & BD_NOFOCUS));

	gtk_widget_show(drawControl->widget);
	wlibAddHelpString(drawControl->widget, helpStr);

	return drawControl;
}

