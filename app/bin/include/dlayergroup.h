/** \file dlayergroup.h
 * Layer Groups (SF #222 phase 0, SF #782): named, flat, user-managed
 * collections of layer memberships. Any layer may belong to any, all, or
 * none of the defined groups -- no composition/derivation between groups.
 *
 * Deliberately standalone (plain \c int/array types, no dependency on
 * common.h/dynarray.h/wlib) so it links and CMocka-tests the same
 * lightweight way as reportsformat.c/cars/careditlogic.c -- see
 * unittest/layergrouptest.c's header comment.
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2026 XTrkCAD contributors
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

#ifndef DLAYERGROUP_H
#define DLAYERGROUP_H

#include <stddef.h>

/** Max characters (incl. terminator) in a group name -- matches the
 * app-wide STR_SHORT_SIZE convention without depending on common.h. */
#define LAYERGROUP_NAME_SIZE 32

/** Reset the group registry to empty. Mainly for test isolation, but also
 * the right call before loading a new file's LAYERGROUP lines. */
void LayerGroupResetAll(void);

/** Number of groups currently defined. */
int LayerGroupCount(void);

/** Create a new, empty group named \p name. Returns its index, or -1 if
 * \p name is NULL/empty. Names are not required to be unique (uniqueness
 * scope is an open design question, deferred past phase 0). */
int LayerGroupCreate(const char *name);

/** Delete group \p groupIdx. Returns 1 on success, 0 if the index is
 * invalid. Shifts later groups' indices down by one. */
int LayerGroupDelete(int groupIdx);

/** Rename group \p groupIdx to \p newName. Returns 1 on success, 0 if the
 * index is invalid or \p newName is NULL/empty. */
int LayerGroupRename(int groupIdx, const char *newName);

/** Current name of group \p groupIdx, or NULL if the index is invalid. */
const char *LayerGroupName(int groupIdx);

/** Add 1-based layer index \p layerIdx to group \p groupIdx. A no-op (still
 * returns 1) if already a member. Returns 0 if \p groupIdx is invalid or
 * \p layerIdx <= 0. */
int LayerGroupAddMember(int groupIdx, int layerIdx);

/** Remove 1-based layer index \p layerIdx from group \p groupIdx. Returns 1
 * if it was a member (now removed) or if it simply wasn't a member; 0 only
 * if \p groupIdx is invalid. */
int LayerGroupRemoveMember(int groupIdx, int layerIdx);

/** 1 if 1-based layer index \p layerIdx is a member of group \p groupIdx,
 * else 0 (including when \p groupIdx is invalid). */
int LayerGroupHasMember(int groupIdx, int layerIdx);

/** Number of members in group \p groupIdx, or -1 if the index is invalid. */
int LayerGroupMemberCount(int groupIdx);

/** The \p i'th member (1-based layer index) of group \p groupIdx, in
 * insertion order. Returns -1 if either index is out of range. */
int LayerGroupMemberAt(int groupIdx, int i);

/** Format group \p groupIdx's members as a semicolon-separated list of
 * 1-based layer indices into \p buf (size \p bufSize), e.g. "2;5;7" -- the
 * same on-disk convention `layerLinkList` already uses
 * (GetLayerLinkString/PutLayerListArray in dlayer.c), for the LAYERGROUP
 * MEMBERS file-format line. Writes an empty string if \p groupIdx is
 * invalid or has no members. */
void LayerGroupFormatMembers(int groupIdx, char *buf, size_t bufSize);

/** Parse a semicolon/comma/space-separated list of 1-based layer indices
 * from \p list (same format LayerGroupFormatMembers produces, and the same
 * parsing convention as PutLayerListArray) and add them to group
 * \p groupIdx, replacing its existing members. No-op if \p groupIdx is
 * invalid. */
void LayerGroupParseMembers(int groupIdx, const char *list);

/** Migrate the old per-layer "Linked Layers" mechanism into groups: for
 * each of the \p layerCount layers (0-based index \c i) with a non-empty
 * link list, creates one group containing that layer (as 1-based index
 * \c i+1) plus every layer named in \p linkLists[i] (\p linkCounts[i]
 * entries, 1-based indices, matching layerLinkList's own convention).
 * Skips creating a duplicate group when a later layer's migrated member
 * set is identical to one already created (this is the common case for a
 * mutual link, e.g. layer 2 links to 5 and layer 5 links to 2). New
 * groups get an auto-generated name ("Group 1", "Group 2", ...), numbered
 * to avoid colliding with any existing group names. Returns the number of
 * new groups created. \p linkLists[i] may be NULL when \p linkCounts[i]
 * is 0. */
int LayerGroupMigrateFromLinkLists(int layerCount,
                                   int *const *linkLists,
                                   const int *linkCounts);

#endif
