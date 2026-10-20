/** \file notenames.c
 * Manage Notes name registry (SF #800 phase 3) -- see include/notenames.h
 * for the design rationale and the standalone-dependency constraint this
 * file follows.
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

#include <stdlib.h>
#include <string.h>

#include "include/notenames.h"

static char (*names)[NOTENAME_SIZE] = NULL;
static int nameCount = 0;
static int nameCap = 0;

static int IndexOf(const char *name)
{
	for (int i = 0; i < nameCount; i++) {
		if (strcmp(names[i], name) == 0) {
			return i;
		}
	}
	return -1;
}

void NoteNameResetAll(void)
{
	free(names);
	names = NULL;
	nameCount = 0;
	nameCap = 0;
}

int NoteNameCount(void)
{
	return nameCount;
}

int NoteNameAdd(const char *name)
{
	int existing;

	if (name == NULL || name[0] == '\0') {
		return -1;
	}

	existing = IndexOf(name);
	if (existing >= 0) {
		return existing;
	}

	if (nameCount >= nameCap) {
		int newCap = nameCap == 0 ? 4 : nameCap * 2;
		char (*newNames)[NOTENAME_SIZE] = realloc(names,
		                                  (size_t) newCap * sizeof * names);
		if (newNames == NULL) {
			abort();
		}
		names = newNames;
		nameCap = newCap;
	}

	strncpy(names[nameCount], name, NOTENAME_SIZE - 1);
	names[nameCount][NOTENAME_SIZE - 1] = '\0';

	return nameCount++;
}

int NoteNameRemove(int idx)
{
	if (idx < 0 || idx >= nameCount) {
		return 0;
	}

	for (int i = idx; i < nameCount - 1; i++) {
		memcpy(names[i], names[i + 1], NOTENAME_SIZE);
	}
	nameCount--;

	return 1;
}

const char *NoteNameAt(int idx)
{
	if (idx < 0 || idx >= nameCount) {
		return NULL;
	}
	return names[idx];
}
