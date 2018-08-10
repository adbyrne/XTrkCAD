/*
 * eggwrapbox-enums.h
 *
 *  Created on: Aug 8, 2018
 *      Author: richardsa
 */

#ifndef EGG_WRAPBOX_ENUMS_H_
#define EGG_WRAPBOX_ENUMS_H_

#include <glib-object.h>

G_BEGIN_DECLS

/* enumerations from eggwrapbox.h */
GType egg_wrap_allocation_mode_get_type (void) G_GNUC_CONST;
#define EGG_TYPE_WRAP_ALLOCATION_MODE (egg_wrap_allocation_mode_get_type())
GType egg_wrap_box_spreading_get_type (void) G_GNUC_CONST;
#define EGG_TYPE_WRAP_BOX_SPREADING (egg_wrap_box_spreading_get_type())
GType egg_wrap_box_packing_get_type (void) G_GNUC_CONST;
#define EGG_TYPE_WRAP_BOX_PACKING (egg_wrap_box_packing_get_type())

GType
egg_wrap_box_packing_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GFlagsValue values[] = {
      { EGG_WRAP_BOX_H_EXPAND, "EGG_WRAP_BOX_H_EXPAND", "hexpand" },
      { EGG_WRAP_BOX_V_EXPAND, "EGG_WRAP_BOX_V_EXPAND", "vexpand" },
      { 0, NULL, NULL }
    };
    etype = g_flags_register_static ("GWrapBoxPacking", values);
  }
  return etype;
}

G_END_DECLS

#endif /* EGG_WRAPBOX_ENUMS_H_ */

