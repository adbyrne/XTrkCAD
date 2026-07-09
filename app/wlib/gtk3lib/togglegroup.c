/** \file togglegroup.c
 * Create a group of toggle type widgets where the active state is
 * synchronized by GObject thus avoiding recursions.
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis, 2012 Martin Fischer
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


#include "glib-object.h"
#include "gtkint.h"
#include <wlib.h>

typedef struct {
	gchar   *name;
	GObject *master;
	GList   *bindings;   /* GBinding*          */
	GList   *members;
} ToggleGroup;

/* Global registry: group_name → ToggleGroup* */
static GHashTable *groups = NULL;

/**
 * \brief Free all resources owned by a ToggleGroup.
 *
 * Releases the name string, the bindings list, the members list, and the
 * ToggleGroup slice itself. Does not unbind GBinding objects or remove weak
 * references — callers are responsible for that before calling this function.
 *
 * \param grp  Pointer to the ToggleGroup to free. Must not be NULL.
 */
static void
ToggleGroupFree(ToggleGroup *grp)
{
	g_free(grp->name);
	g_list_free(grp->bindings);
	g_list_free(grp->members);
	g_slice_free(ToggleGroup, grp);
}

/**
 * \brief Destroy all toggle groups and release the global registry.
 *
 * Iterates over every entry in the global hash table, frees each ToggleGroup,
 * then destroys the hash table and sets the global pointer to NULL. Must be
 * called during application shutdown after all toggle widgets have been
 * destroyed.
 */
void
ToggleGroupsCleanup(void)
{
	GHashTableIter iter;
	gpointer value;

	g_hash_table_iter_init(&iter, groups);
	while (g_hash_table_iter_next(&iter, NULL, &value)) {
		ToggleGroupFree(value);
	}

	g_hash_table_destroy(groups);
	groups = NULL;
}

/* ------------------------------------------------------------------ */
/* Internal helpers                                                   */
/* ------------------------------------------------------------------ */

/**
 * \brief Look up a ToggleGroup by name, creating it if it does not exist.
 *
 * Searches the global hash table for a group with the given name. If no entry
 * is found a new, empty ToggleGroup is allocated and inserted. The group's
 * name is owned by the ToggleGroup struct; the hash table key points to that
 * same string.
 *
 * \param name  Name of the toggle group. Must not be NULL.
 * \return      Pointer to the existing or newly created ToggleGroup.
 */
static ToggleGroup *
EnsureGroup(const gchar *name)
{
	ToggleGroup *grp = g_hash_table_lookup(groups, name);

	if (grp == NULL) {
		grp = g_slice_new0(ToggleGroup);
		grp->name = g_strdup(name);
		g_hash_table_insert(groups, grp->name, grp);
	}

	return grp;
}

/**
 * \brief Create a bidirectional GObject property binding between two toggle widgets.
 *
 * Binds the \c active property of \p master to the \c active property of
 * \p member in both directions. \c G_BINDING_SYNC_CREATE causes \p member's
 * state to be set immediately from \p master upon creation, keeping the group
 * consistent without triggering a separate signal.
 *
 * \param master  The hub widget whose \c active state is the source of truth.
 * \param member  The widget to keep in sync with \p master.
 * \return        The newly created GBinding. The binding is owned by GObject
 *                and remains alive until either object is finalized or the
 *                binding is explicitly unbound.
 */
static GBinding *
CreateBinding(GObject *master, GObject *member )
{
	return g_object_bind_property(
	               master, "active",
	               member, "active",
	               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
}

/**
 * \brief Weak-reference notify callback invoked when a member widget is finalized.
 *
 * Removes the destroyed widget from the group's member list. If the destroyed
 * widget was the master, all existing bindings are released and the first
 * remaining member is elected as the new master; new bidirectional bindings
 * are then created between the new master and every other surviving member.
 *
 * \param data                  Pointer to the ToggleGroup that owned the widget.
 * \param where_the_object_was  Address of the finalized GObject (must not be
 *                              dereferenced — the object no longer exists).
 */
static void
on_member_destroyed(gpointer  data,
                    GObject  *where_the_object_was)
{
	ToggleGroup *grp = data;

	/* Remove from members and flags lists (keep them in sync) */
	GList *link = g_list_find(grp->members, where_the_object_was);
	if (link != NULL) {
		grp->members    = g_list_delete_link(grp->members, link);
	}

	if ((GObject *)grp->master != where_the_object_was) {
		return;
	}

	/* Master was destroyed – elect a new one and rebind */
	grp->master = NULL;
	g_list_free(grp->bindings);
	grp->bindings = NULL;

	if (grp->members == NULL) {
		return;
	}

	/* First remaining member becomes the new master */
	grp->master = grp->members->data;

	GList *m = grp->members->next;

	for (; m != NULL; m = m->next) {
		GBinding *b = CreateBinding(grp->master, m->data);
		grp->bindings = g_list_append(grp->bindings, b);
	}
}

/**
 * \brief Initialize the global toggle-group registry.
 *
 * Creates the GHashTable used to map group name strings to ToggleGroup
 * instances. Must be called before any other \c wToggleGroup* function.
 * Calling this function a second time without an intervening
 * ToggleGroupsCleanup() will leak the previous table.
 */
void
wlibToggleGroupsInit(void)
{
	groups = g_hash_table_new_full(g_str_hash, g_str_equal,
	                               NULL, NULL);
}

/**
 * \brief Connect a callback to the \c notify::active signal of a group's master widget.
 *
 * The callback is invoked whenever the master widget's \c active property
 * changes, which — through the bidirectional bindings — reflects any state
 * change in the group. Because only the master emits the signal, the callback
 * fires exactly once per logical toggle event regardless of how many members
 * the group contains.
 *
 * \param group_name  Name of an existing, non-empty toggle group.
 * \param callback    GCallback to connect to the \c notify::active signal.
 * \param user_data   Arbitrary data passed to \p callback as its last argument.
 */
void
wlibToggleGroupConnect(const gchar *group_name,
                       GCallback    callback,
                       wControl_p    user_data)
{
	g_return_if_fail(groups != NULL);

	ToggleGroup *grp = g_hash_table_lookup(groups, group_name);
	g_return_if_fail(grp != NULL && grp->master != NULL);

	g_signal_connect(grp->master, "notify::active",
	                 callback, user_data);
}

/**
 * \brief Signal handler for the \c notify::active property notification.
 *
 * Dispatches the appropriate application-level callback when the master
 * widget's \c active state changes. Handles two widget types:
 * - \c B_TOGGLE — calls the toggle's \c wChoiceCallBack_p with index 0.
 * - \c M_TOGGLE — calls the menu item's \c wMenuCallBack_p.
 *
 * Any other widget type is silently ignored.
 *
 * \param object     The GObject that emitted the \c notify::active signal
 *                   (the group master widget).
 * \param pspec      GParamSpec for the \c active property (unused).
 * \param user_data  The wControl_p associated with the master toggle.
 */
static void
notify_active(GObject *object, GParamSpec *pspec, gpointer user_data)
{
	wControl_p toggle = (wControl_p)user_data;

	if(toggle->type == B_TOGGLE) {
		wChoiceCallBack_p action = toggle->attributes.toggle.action;
		action(0, toggle->context);
	} else {
		if(toggle->type == M_TOGGLE) {
			wMenuCallBack_p action = toggle->attributes.menuitem.action;
			action(toggle->context);
		} else {
			return;
		}
	}
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

/**
 * \brief Register a toggle widget in a named toggle group.
 *
 * Adds \p toggle to the group identified by \p group_name, creating the group
 * if it does not yet exist. The widget must expose a boolean GObject property
 * named \c active; widgets that do not satisfy this requirement are rejected
 * and \c TOGGLE_GROUP_ERROR is returned.
 *
 * The per-toggle \c toggled signal handler stored under the \c "handler-id"
 * object-data key is disconnected so that state changes are driven exclusively
 * through the group's \c notify::active mechanism, preventing duplicate
 * callbacks.
 *
 * The first widget registered in a group becomes the \e master. All subsequent
 * widgets become \e members and are bound bidirectionally to the master via
 * g_object_bind_property(). A weak reference is installed on each member so
 * that the group can reorganise itself automatically when a widget is destroyed.
 *
 * \param toggle      The wControl_p wrapping the GTK widget to register.
 * \param group_name  Name of the toggle group. Must not be NULL.
 * \return            \c TOGGLE_GROUP_MASTER if the widget became the master,
 *                    \c TOGGLE_GROUP_MEMBER if it was added as a member, or
 *                    \c TOGGLE_GROUP_ERROR on invalid input.
 */
ToggleGroupResult
wToggleGroupRegister( wControl_p toggle,
                      const char *group_name )
{
	if(!groups) {
		wlibToggleGroupsInit();
	}

	g_return_val_if_fail(group_name != NULL, TOGGLE_GROUP_ERROR);

	// check that the widget has a boolean property "active". This allows all types
	// of toggles / switches to be used with a ToggleGroup

	GParamSpec *pspec = g_object_class_find_property(
	                            G_OBJECT_GET_CLASS(toggle->widget), "active");
	g_return_val_if_fail(pspec != NULL, TOGGLE_GROUP_ERROR);
	g_return_val_if_fail(pspec->value_type == G_TYPE_BOOLEAN, TOGGLE_GROUP_ERROR);

	ToggleGroup *grp = EnsureGroup(group_name);

	// per default, the toggled signal is connected for toggle controls. This is replaced by the
	// notify::active signal emitted by the master of the group only. So it has to be disconnected
	// for all toggles in the group

	gulong handler_id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(toggle->widget),
	                                     "handler-id"));
	if(handler_id) {
		g_signal_handler_disconnect(toggle->widget, handler_id);
		g_object_set_data(G_OBJECT(toggle->widget), "handler-id", NULL);
	}

	if (grp->master == NULL) {
		/* First toggle in this group becomes the master (hub) */
		grp->master = toggle->widget;

		wlibToggleGroupConnect(group_name, G_CALLBACK(notify_active), toggle);
		return TOGGLE_GROUP_MASTER;
	}
	/* Bind to existing master */
	GBinding *b = CreateBinding(grp->master, toggle->widget );
	grp->bindings = g_list_append(grp->bindings, b);

	grp->members = g_list_append(grp->members, toggle->widget);

	/* Clean up automatically when the widget is destroyed */
	g_object_weak_ref(G_OBJECT(toggle->widget), on_member_destroyed, grp);

	return TOGGLE_GROUP_MEMBER;
}

/**
 * \brief Report if the toggle group exists.
 *
 * Attempts to lookup the group name
 *
 * \param group_name  Name of a possibly existing toggle group.
 * \return            \c FALSE if \p group_name is not found or the group has no master, otherwise TRUE.
 */
wBool_t
wToggleGroupExists(const gchar *group_name)
{
	if (!groups)
		return FALSE;
	ToggleGroup *grp = g_hash_table_lookup(groups, group_name);
	return (grp && grp->master);
}

/**
 * \brief Set the active state for all widgets in a toggle group.
 *
 * Sets the \c active property on the group's master widget. Because all member
 * widgets are bound bidirectionally to the master, they are updated
 * automatically by GObject without additional signal emissions.
 *
 * \param group_name  Name of an existing, non-empty toggle group.
 * \param active      The desired active state to apply to the group.
 */
void
wToggleGroupSetActive(const gchar *group_name,
                      wBool_t     active)
{
	if (!groups)
		return;
	ToggleGroup *grp = g_hash_table_lookup(groups, group_name);
	if (!grp || !grp->master)
		return;

	g_object_set(grp->master, "active", active, NULL);
}

/**
 * \brief Query the current active state of a toggle group.
 *
 * Reads the \c active property from the group's master widget, which reflects
 * the shared state of all members.
 *
 * \param group_name  Name of an existing, non-empty toggle group.
 * \return            The current active state of the group, or \c FALSE if
 *                    \p group_name is not found or the group has no master.
 */
wBool_t
wToggleGroupGetActive(const gchar *group_name)
{
	if (!groups)
		return FALSE;
	ToggleGroup *grp = g_hash_table_lookup(groups, group_name);
	if (!grp || !grp->master)
		return FALSE;

	gboolean active;
	g_object_get(grp->master, "active", &active, NULL);
	return active;
}
