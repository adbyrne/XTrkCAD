/** \file notenames.h
 * Manage Notes name registry (SF #800 phase 3): the flat list of
 * ROOT-level JSON Note field names a user has registered as meaningful
 * for the Notes Report's "ROOT Names" grouping/filter (see
 * ReportsNoteResolveGroup(), reports.h). A note's own JSON body is never
 * consulted here -- this is purely the list of names to match against.
 *
 * Deliberately standalone (plain \c int/array types, no dependency on
 * common.h/dynarray.h/wlib) so it links and CMocka-tests the same
 * lightweight way as dlayergroup.h/reportsformat.c -- see
 * unittest/notenamestest.c's header comment.
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

#ifndef NOTENAMES_H
#define NOTENAMES_H

/** Max characters (incl. terminator) in a registered name -- generous
 * enough for a realistic JSON field name without depending on
 * common.h's STR_SHORT_SIZE. */
#define NOTENAME_SIZE 64

/** Reset the registry to empty. Mainly for test isolation, but also the
 * right call before loading a new file's NOTENAME lines. */
void NoteNameResetAll(void);

/** Number of names currently registered. */
int NoteNameCount(void);

/** Register \p name. Returns its index, or -1 if \p name is NULL/empty.
 * A no-op (still returns the existing index) if \p name is already
 * registered -- registration is a set, not an ordered log, so a repeat
 * add (e.g. from the search-assist re-finding an already-registered
 * name) is harmless. */
int NoteNameAdd(const char *name);

/** Remove the name at \p idx. Returns 1 on success, 0 if \p idx is
 * invalid. Shifts later names' indices down by one. */
int NoteNameRemove(int idx);

/** The name at \p idx, or NULL if \p idx is invalid. */
const char *NoteNameAt(int idx);

#endif
