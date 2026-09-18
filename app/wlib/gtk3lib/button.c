/**
 * \file	button.c
 * \brief	Buttons
 *
 * \author mf
 * \date   May 2024
 */
/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis, 2024 Martin Fischer
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

#include "i18n.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "gtkint.h"

#include "xtrkcad-config.h"

static void buttonClick(GtkWidget *widget, gpointer value);
static void buttonPress(GtkWidget *widget, GdkEventButton *event,
                        gpointer value);
/* Forward declarations */
static void splitButtonDropdownClick(GtkWidget *widget, gpointer data);
static void ApplySplitButtonStyle(GtkWidget *splitButton);

int wlibRecursionTrace = 0;

bool IsNewIcon(wIcon_p new, wIcon_p old)
{

	if (new->gtkIconType == ICON_PIXBUF_FROM_RESOURCE) {
		if (g_strcmp0(new->filename, old->filename)) {
			return (true);
		}
	}

	if (new->gtkIconType == ICON_PIXBUF_FROM_TEXT) {
		if ((new->color != old->color) || g_strcmp0(new->text, old->text)) {
			return (true);
		}
	}

	return (false);
}


/**
 * Change the label of a button. This can be used to set the text
 *
 * \param bb		IN button handle
 * \param labelStr	IN new label string
 *
 * \todo icons in XBM format
 */

void wButtonSetLabel(wControl_p bb, const char *labelStr)
{
	gtk_button_set_label(GTK_BUTTON(bb->widget), labelStr);
}

/**
 * Perform the user callback function
 *
 * \param bb IN button handle
 */

void wlibButtonDoAction(wControl_p bb)
{
	if (bb->attributes.button.recursion) {
		if (wlibRecursionTrace) {
			printf("Recurse: wlibButtonDoAction\n");
		}
		return;
	}
	if (bb->attributes.button.action) {
		bb->attributes.button.recursion++;
		bb->attributes.button.action(bb->context);
		bb->attributes.button.recursion--;
	}
}

/**
 * Signal handler for button click
 * \param widget IN the control or NULL for autorepeat
 * \param value IN the button handle (same as control???)
 */

static void buttonClick(GtkWidget *widget, gpointer value)
{
	struct button *b = CONTROL_GET_ATTRIBUTES_PTR(((wControl_p)value), button);
	if (b->recursion) {
		if (wlibRecursionTrace) {
			printf("Recursion: buttonClick\n");
		}
		return;
	}

	if (b->action) {
		b->recursion++;
		b->action(((wControl_p)value)->context);
		b->recursion--;
	}
}

static void buttonPress(GtkWidget *widget, GdkEventButton *event,
                        gpointer value)
{
	UpdateModifierKeyStateFromButton(event);
	struct button *b = CONTROL_GET_ATTRIBUTES_PTR(((wControl_p)value), button);
	if (b->recursion) {
		if (wlibRecursionTrace) {
			printf("Recursion: buttonPress\n");
		}
		return;
	}

	if (b->action) {
		b->recursion++;
		b->action(((wControl_p)value)->context);
		b->recursion--;
	}
}

#define REPEAT_INITIAL_DELAY 500
#define REPEAT_DELAY 150

static gboolean on_button_repeat(gpointer user_data)
{
	wControl_p button = (wControl_p)user_data;

	if (!button->attributes.button.is_pressed) {
		button->attributes.button.timeout_id = 0;
		return G_SOURCE_REMOVE;
	}

	if (button->attributes.button.timer_state == INITIAL_DELAY) {
		g_source_remove(button->attributes.button.timeout_id);
		button->attributes.button.timer_state = REPEATED;
		button->attributes.button.timeout_id =
		        g_timeout_add(REPEAT_DELAY, on_button_repeat, button);
	}

	g_signal_emit_by_name(button->widget, "clicked");

	return G_SOURCE_CONTINUE;
}

// Callback for Button-Press-Event
static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event,
                                gpointer user_data)
{
	wControl_p button = (wControl_p)user_data;

	if (event->button == GDK_BUTTON_PRIMARY) {
		button->attributes.button.is_pressed = TRUE;

		// start first timeout after initial delay
		button->attributes.button.timeout_id =
		        g_timeout_add(REPEAT_INITIAL_DELAY, on_button_repeat, button);
		button->attributes.button.timer_state = INITIAL_DELAY;
	}

	return FALSE; // forward event
}

// Callback for Button-Release-Event
static gboolean on_button_release(GtkWidget *widget, GdkEventButton *event,
                                  gpointer user_data)
{
	wControl_p button = (wControl_p)user_data;

	if (event->button == GDK_BUTTON_PRIMARY) {
		button->attributes.button.is_pressed = FALSE;

		if (button->attributes.button.timeout_id > 0) {
			g_source_remove(button->attributes.button.timeout_id);
			button->attributes.button.timeout_id = 0;
		}
	}

	return FALSE; // Forward event
}

// Callback when mouse leaves the button
static gboolean on_button_leave(GtkWidget *widget, GdkEventCrossing *event,
                                gpointer user_data)
{
	wControl_p button = (wControl_p)user_data;

	button->attributes.button.is_pressed = FALSE;

	if (button->attributes.button.timeout_id > 0) {
		g_source_remove(button->attributes.button.timeout_id);
		button->attributes.button.timeout_id = 0;
	}

	return FALSE;
}

static void SetAutoRepeat(wControl_p button)
{
	// activate events
	gtk_widget_add_events(button->widget, GDK_BUTTON_PRESS_MASK |
	                      GDK_BUTTON_RELEASE_MASK |
	                      GDK_LEAVE_NOTIFY_MASK);

	// connect event handlers
	g_signal_connect(button->widget, "button-press-event",
	                 G_CALLBACK(on_button_press), button);
	g_signal_connect(button->widget, "button-release-event",
	                 G_CALLBACK(on_button_release), button);
	g_signal_connect(button->widget, "leave-notify-event",
	                 G_CALLBACK(on_button_leave), button);
}

#define ISDIALOGACTION(options)                                                \
  ((options & BB_HELP) || (options & BB_CANCEL) || (options & BB_DEFAULT))
/**
 * Create a button
 *
 * ### Usage in dialogs, created by
 *
 * - runtime: yes
 * - builder: yes
 *
 * ### Options
 * BB_DEFAULT
 * : set button as default for dialog
 * BO_ICON
 * : use an icon instead of label,
 * BO_REPEAT
 * : autorepeat function triggered by longer press
 *
 * \param parent IN parent window
 * \param x IN X-position
 * \param y IN Y-position
 * \param helpStr IN Help string
 * \param labelStr IN Label
 * \param option IN Options
 * \param width IN Width of button
 * \param action IN Callback
 * \param context IN User context
 * \returns button control
 *
 */

wControl_p wButtonCreate(wControl_p parent, wWinPix_t x, wWinPix_t y,
                         const char *helpStr, const char *labelStr, long option,
                         wWinPix_t width, wButtonCallBack_p action,
                         void *context)
{
	wControl_p b;
	struct button *button;

	b = wlibControlNew(B_BUTTON, parent, helpStr, context);
	button = CONTROL_GET_ATTRIBUTES_PTR(b, button);
	button->action = action;

	if (ISDEFINEDINBUILDER(parent) || ISDIALOGACTION(option)) {
		b->widget = wlibWidgetFromIdWarn(parent, helpStr);
		if (labelStr) {
			wButtonSetLabel(b, labelStr);
		}
	} else {
		b->widget = GTK_WIDGET(gtk_toggle_button_new());

		if (width > 0) {
			gtk_widget_set_size_request(b->widget, width, -1);
		}

		if (labelStr) {
			if (option & BO_ICON) {
				if (((wIcon_p)labelStr)->bits == NULL) {
					printf("Invalid icon for %s\n", helpStr);
				}
				wButtonSetIcon(b, (wIcon_p)labelStr);
			} else {
				wButtonSetLabel(b, labelStr);
			}
		}

		wlibBasicGridAttach(parent, b->widget, x, y, 1, 1);

		if (option & BB_DEFAULT) {
			gtk_widget_set_can_default(b->widget, TRUE);
			gtk_widget_grab_default(b->widget);
			gtk_window_set_default(GTK_WINDOW(parent->widget), b->widget);
		}
	}
	gtk_widget_show_all(b->widget);

	if (GTK_IS_MENU_BUTTON(b->widget) &&
	    gtk_menu_button_get_popup(GTK_MENU_BUTTON(b->widget)) != NULL) {
		/* A GtkMenuButton with a builder-defined popup opens it from its
		 * own default button-press handling; stealing the event here
		 * (as done below for plain buttons) would stop that from firing
		 * and the popup would never appear. */
	} else {
		g_signal_connect(G_OBJECT(b->widget), "button-press-event",
		                 G_CALLBACK(buttonPress), b);
	}

	if (option & BO_REPEAT) {
		SetAutoRepeat(b);
	}

	wlibAddTooltip(b->widget, parent->name, helpStr);

	return b;
}

/**
 * Create a toolbar button
 *
 * ### Usage in dialogs
 *
 * - runtime: yes
 * - builder: not required
 *
 * ### Options
 * BO_GAP
 * : leave some space after the button. Technically this is an invisible
 * separator
 * BO_SPLITBUTTON
 * : create a button with an additional dropdown menu, context is the menu to
 * open
 *
 * \param parent IN		application main window
 * \param x,y  IN		unused
 * \param helpStr IN	help string
 * \param icon IN		pointer to icon (XPM)
 * \param option IN		options
 * \param width IN		unused
 * \param action IN		callback
 * \param context IN	dopdown menu in case of BO_SPLITBUTTON
 * \returns button control
 *
 */

wControl_p wButtonCreateForToolbar(wControl_p parent, wWinPix_t x, wWinPix_t y,
                                   const char *helpStr, wIcon_p icon,
                                   long option, wWinPix_t width,
                                   wButtonCallBack_p action, void *context)
{
	wControl_p buttonControl;
	struct button *buttonAttributes;

	g_assert(parent->type == W_MAIN);

	buttonControl = wlibControlNew(B_BUTTON, parent, helpStr, context);
	buttonAttributes = CONTROL_GET_ATTRIBUTES_PTR(buttonControl, button);
	buttonAttributes->action = action;

	// Create regular button
	if (icon && icon->bits) {
		buttonControl->widget = GTK_WIDGET(gtk_button_new());

		if (!buttonControl->widget) {
			fprintf(stderr, "ERROR: Failed to create button widget\n");
			g_free(buttonControl);
			return NULL;
		}

		wButtonSetIcon(buttonControl, icon);

		// Connect click signal for regular button
		g_signal_connect(G_OBJECT(buttonControl->widget), "clicked",
		                 G_CALLBACK(buttonClick), buttonControl);
	} else {
		fprintf(stderr, "ERROR: No icon provided for regular button\n");
		g_free(buttonControl);
		return NULL;
	}

	wlibAddButtonToToolbar(buttonControl, helpStr);

	return buttonControl;
}
/**
 * Wrap an existing button as a split button.
 * Creates a container with the button and a dropdown arrow.
 */
void wButtonMakeSplit(wControl_p button, wMenu_p popupMenu)
{
	GtkWidget *splitContainer;
	GtkWidget *dropdownButton;
	GtkWidget *arrow;
	GtkWidget *originalButton;

	if (!button || !button->widget) {
		fprintf(stderr, "ERROR: wButtonMakeSplit called with invalid button\n");
		return;
	}

	originalButton = button->widget;

	// Create horizontal box container
	splitContainer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_name(splitContainer, "split-button-container");

	// Create dropdown button with arrow
	dropdownButton = gtk_button_new();
	arrow =
	        gtk_image_new_from_icon_name("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(dropdownButton), arrow);
	gtk_button_set_relief(GTK_BUTTON(dropdownButton), GTK_RELIEF_NORMAL);
	gtk_widget_set_can_focus(dropdownButton, FALSE);

	// Add style class to dropdown for CSS targeting
	GtkStyleContext *ctx = gtk_widget_get_style_context(dropdownButton);
	gtk_style_context_add_class(ctx, "split-dropdown");

	// Remove button from its current parent (if any)
	GtkWidget *parent = gtk_widget_get_parent(originalButton);
	if (parent) {
		g_object_ref(originalButton); // Keep reference while reparenting
		gtk_container_remove(GTK_CONTAINER(parent), originalButton);
	}

	// Pack both buttons into container
	gtk_box_pack_start(GTK_BOX(splitContainer), originalButton, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(splitContainer), dropdownButton, FALSE, FALSE, 0);

	if (parent) {
		g_object_unref(originalButton); // Release temporary reference
		gtk_container_add(GTK_CONTAINER(parent), splitContainer);
	}

	// Apply linked style to make them look like one button
	ctx = gtk_widget_get_style_context(splitContainer);
	gtk_style_context_add_class(ctx, "linked");

	// Apply CSS for compact dropdown
	ApplySplitButtonStyle(splitContainer);

	// Store references and metadata
	g_object_set_data(G_OBJECT(splitContainer), "action-button", originalButton);
	g_object_set_data(G_OBJECT(splitContainer), "dropdown-button",
	                  dropdownButton);
	g_object_set_data(G_OBJECT(splitContainer), "is-split-button",
	                  GINT_TO_POINTER(1));
	g_object_set_data(G_OBJECT(splitContainer), "popup-menu", popupMenu);

	// Store back-reference so we can find container from button
	g_object_set_data(G_OBJECT(originalButton), "split-container",
	                  splitContainer);

	// Connect dropdown signal
	g_signal_connect(G_OBJECT(dropdownButton), "clicked",
	                 G_CALLBACK(splitButtonDropdownClick), splitContainer);

	// Set tooltip on dropdown
	gtk_widget_set_tooltip_text(dropdownButton, _("More options"));

	gtk_widget_show_all(splitContainer);

	// Update the button control to point to the container
	button->widget = splitContainer;
}

/**
 * Check if a button is a split button.
 */
wBool_t wButtonIsSplitButton(wControl_p button)
{
	if (!button || !button->widget) {
		return FALSE;
	}

	gpointer flag =
	        g_object_get_data(G_OBJECT(button->widget), "is-split-button");
	return (flag != NULL);
}

/**
 * Update the command context for a button's action.
 * This updates what gets passed to the button's callback function.
 * Works with both regular and split buttons.
 *
 * @param button Button control
 * @param context New context (typically command index via I2VP)
 */
void wButtonSetContext(wControl_p button, void *context)
{
	if (!button) {
		return;
	}

	// Update the control's data field
	button->context = context;
}

/**
 * Update icon on a button (handles both regular and split buttons).
 */
void wButtonSetIcon(wControl_p button, wIcon_p icon)
{
	if (!button || !button->widget || !icon) {
		return;
	}

	GtkWidget *targetButton;

	// Get the actual button widget
	if (wButtonIsSplitButton(button)) {
		targetButton = GTK_WIDGET(
		                       g_object_get_data(G_OBJECT(button->widget), "action-button"));
	} else {
		targetButton = button->widget;
	}

	if (!targetButton) {
		return;
	}

	// Set icon
	if (icon->bits) {
		GtkWidget *image = gtk_image_new_from_pixbuf(icon->bits);
		gtk_button_set_image(GTK_BUTTON(targetButton), image);
		gtk_button_set_always_show_image(GTK_BUTTON(targetButton), TRUE);
	}
}

/**
 * Set the popup menu for a split button's dropdown.
 */
void wButtonSetDropdownMenu(wControl_p button, wMenu_p menu)
{
	if (!button || !button->widget) {
		return;
	}

	if (!wButtonIsSplitButton(button)) {
		fprintf(stderr,
		        "WARNING: Trying to set dropdown menu on non-split button\n");
		return;
	}

	g_object_set_data(G_OBJECT(button->widget), "popup-menu", menu);
}

/**
 * Handle dropdown button click - show popup menu.
 */
static void splitButtonDropdownClick(GtkWidget *widget, gpointer data)
{
	GtkWidget *splitContainer = GTK_WIDGET(data);

	if (!splitContainer) {
		return;
	}

	wMenu_p popupMenu =
	        (wMenu_p)g_object_get_data(G_OBJECT(splitContainer), "popup-menu");

	if (popupMenu && popupMenu->widget) {
		gtk_menu_popup_at_widget(GTK_MENU(popupMenu->widget), widget,
		                         GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST,
		                         NULL);
	}
}

static void ApplySplitButtonStyle(GtkWidget *splitButton)
{
	static GtkCssProvider *cssProvider = NULL;
	static gboolean css_loaded = FALSE;

	if (!css_loaded) {
		cssProvider = gtk_css_provider_new();

		// Try to load from embedded resource first (void function, no return value)
		gtk_css_provider_load_from_resource(cssProvider,
		                                    XTRKCAD_RESOURCE_PATH CSS_FILENAME);

		// If resource loading failed, it will have triggered a warning
		// but we can't check it directly. Try file fallback.
		// Alternatively, just use the resource and assume it exists if properly
		// compiled.

		css_loaded = TRUE;
	}

	if (cssProvider) {
		GtkStyleContext *context = gtk_widget_get_style_context(splitButton);
		gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(cssProvider),
		                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}
}
