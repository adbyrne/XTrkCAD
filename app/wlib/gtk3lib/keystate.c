/**
 * \file   keystate.c
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

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "gtkint.h"

#include <wlib.h>

static int keyState;
static int keyStateFromButton;

/**
 * Get current state of shift, ctrl or alt keys.
 *
 * \return    or'ed value of WKEY_SHIFT, WKEY_CTRL and WKEY_ALT depending on
 * state
 */

int wGetKeyState(void) { return keyState; }
int wGetKeyStateFromButton(void) { return keyStateFromButton; }
void wResetKeyStateFromButton(void) { keyStateFromButton = 0; }

wBool_t UpdateModifierKeyState(GdkEventKey *event)
{
	int state = 0;
	switch (event->keyval) {
	case GDK_KEY_Shift_L:
	case GDK_KEY_Shift_R:
		state |= WKEY_SHIFT;
		break;

	case GDK_KEY_Control_L:
	case GDK_KEY_Control_R:
		state |= WKEY_CTRL;
		break;

	case GDK_KEY_Alt_L:
	case GDK_KEY_Alt_R:
		state |= WKEY_ALT;
		break;

	case GDK_KEY_Meta_L:
	case GDK_KEY_Meta_R:
		// Pressing SHIFT and then ALT generates a Meta key
		state |= WKEY_ALT;
		break;
	}

	if (state != 0) {
		if (event->type == GDK_KEY_PRESS) {
			keyState |= state;
		} else {
			keyState &= ~state;
		}
		return TRUE;
	}
	return FALSE;
}

wBool_t UpdateModifierKeyStateFromButton(GdkEventButton *event)
{
	int state = 0;

	if (event->state & GDK_SHIFT_MASK) {
		state |= WKEY_SHIFT;
	}

	if (event->state & GDK_CONTROL_MASK) {
		state |= WKEY_CTRL;
	}

	if (event->state & GDK_MOD1_MASK) {
		state |= WKEY_ALT;
	}

	if (event->state & GDK_META_MASK) {
		state |= WKEY_ALT;        /* or WKEY_META if you have it */
	}

	// may need to use different static variable and function to return it for
	// buttons pressed with modifiers
	keyStateFromButton = state;
	return state != 0;
}
