/**
 * \file   stickytoggle.h
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

#ifndef HAVE_STICKYTOGGLE_H
#define HAVE_STICKYTOGGLE_H

#include <gtk/gtk.h>

typedef enum {
	STICKY_TOGGLE_BUTTON_TEMP,
	STICKY_TOGGLE_BUTTON_FIXED
} STICKY_TOGGLE_BUTTON_MODE;

#define STICKY_TOGGLE_BUTTON_TYPE (sticky_toggle_button_get_type())
#define IS_STICKY_TOGGLE_BUTTON(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), STICKY_TOGGLE_BUTTON_TYPE))
#define IS_STICKY_TOGGLE_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), STICKY_TOGGLE_BUTTON_TYPE))

G_DECLARE_FINAL_TYPE(StickyToggleButton, sticky_toggle_button, STICKY, TOGGLE_BUTTON, GtkToggleButton)

GtkWidget* sticky_toggle_button_new(void);
GtkWidget* sticky_toggle_button_new_with_mode(STICKY_TOGGLE_BUTTON_MODE mode);

void sticky_toggle_button_set_sticky(StickyToggleButton* self, gboolean sticky_state);
gboolean sticky_toggle_button_get_sticky(StickyToggleButton* self);

void sticky_toggle_button_set_mode(StickyToggleButton* self, STICKY_TOGGLE_BUTTON_MODE mode);
gboolean sticky_toggle_button_get_mode(StickyToggleButton* self);

#endif // !HAVE_STICKYTOGGLE_H
