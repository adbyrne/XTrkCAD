/** \file dlayergroup.c
 * Layer Groups (SF #222 phase 0, SF #782) -- see include/dlayergroup.h for
 * the design rationale and the standalone-dependency constraint this file
 * follows.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/dlayergroup.h"

typedef struct {
	char name[LAYERGROUP_NAME_SIZE];
	int *members;      /* 1-based layer indices, insertion order */
	int memberCount;
	int memberCap;
} layerGroupEntry_t;

static layerGroupEntry_t *groups = NULL;
static int groupCount = 0;
static int groupCap = 0;

static int GroupValid(int groupIdx)
{
	return groupIdx >= 0 && groupIdx < groupCount;
}

void LayerGroupResetAll(void)
{
	for (int i = 0; i < groupCount; i++) {
		free(groups[i].members);
	}
	free(groups);
	groups = NULL;
	groupCount = 0;
	groupCap = 0;
}

int LayerGroupCount(void)
{
	return groupCount;
}

int LayerGroupCreate(const char *name)
{
	if (name == NULL || name[0] == '\0') {
		return -1;
	}

	if (groupCount >= groupCap) {
		int newCap = groupCap == 0 ? 4 : groupCap * 2;
		layerGroupEntry_t *newGroups = realloc(groups, newCap * sizeof * groups);
		if (newGroups == NULL) {
			abort();
		}
		groups = newGroups;
		groupCap = newCap;
	}

	layerGroupEntry_t *g = &groups[groupCount];
	memset(g, 0, sizeof * g);
	strncpy(g->name, name, sizeof g->name - 1);
	g->name[sizeof g->name - 1] = '\0';

	return groupCount++;
}

int LayerGroupDelete(int groupIdx)
{
	if (!GroupValid(groupIdx)) {
		return 0;
	}

	free(groups[groupIdx].members);
	for (int i = groupIdx; i < groupCount - 1; i++) {
		groups[i] = groups[i + 1];
	}
	groupCount--;

	return 1;
}

int LayerGroupRename(int groupIdx, const char *newName)
{
	if (!GroupValid(groupIdx) || newName == NULL || newName[0] == '\0') {
		return 0;
	}

	strncpy(groups[groupIdx].name, newName, sizeof groups[groupIdx].name - 1);
	groups[groupIdx].name[sizeof groups[groupIdx].name - 1] = '\0';

	return 1;
}

const char *LayerGroupName(int groupIdx)
{
	if (!GroupValid(groupIdx)) {
		return NULL;
	}
	return groups[groupIdx].name;
}

static int MemberIndexOf(const layerGroupEntry_t *g, int layerIdx)
{
	for (int i = 0; i < g->memberCount; i++) {
		if (g->members[i] == layerIdx) {
			return i;
		}
	}
	return -1;
}

int LayerGroupAddMember(int groupIdx, int layerIdx)
{
	if (!GroupValid(groupIdx) || layerIdx <= 0) {
		return 0;
	}

	layerGroupEntry_t *g = &groups[groupIdx];
	if (MemberIndexOf(g, layerIdx) >= 0) {
		return 1;
	}

	if (g->memberCount >= g->memberCap) {
		int newCap = g->memberCap == 0 ? 4 : g->memberCap * 2;
		int *newMembers = realloc(g->members, newCap * sizeof * g->members);
		if (newMembers == NULL) {
			abort();
		}
		g->members = newMembers;
		g->memberCap = newCap;
	}

	g->members[g->memberCount++] = layerIdx;
	return 1;
}

int LayerGroupRemoveMember(int groupIdx, int layerIdx)
{
	if (!GroupValid(groupIdx)) {
		return 0;
	}

	layerGroupEntry_t *g = &groups[groupIdx];
	int i = MemberIndexOf(g, layerIdx);
	if (i < 0) {
		return 1;
	}

	for (; i < g->memberCount - 1; i++) {
		g->members[i] = g->members[i + 1];
	}
	g->memberCount--;

	return 1;
}

int LayerGroupHasMember(int groupIdx, int layerIdx)
{
	if (!GroupValid(groupIdx)) {
		return 0;
	}
	return MemberIndexOf(&groups[groupIdx], layerIdx) >= 0;
}

int LayerGroupMemberCount(int groupIdx)
{
	if (!GroupValid(groupIdx)) {
		return -1;
	}
	return groups[groupIdx].memberCount;
}

int LayerGroupMemberAt(int groupIdx, int i)
{
	if (!GroupValid(groupIdx) || i < 0 || i >= groups[groupIdx].memberCount) {
		return -1;
	}
	return groups[groupIdx].members[i];
}

void LayerGroupFormatMembers(int groupIdx, char *buf, size_t bufSize)
{
	if (bufSize == 0) {
		return;
	}
	buf[0] = '\0';

	if (!GroupValid(groupIdx)) {
		return;
	}

	layerGroupEntry_t *g = &groups[groupIdx];
	char *cp = buf;
	for (int i = 0; i < g->memberCount; i++) {
		int written;
		if (i == 0) {
			written = snprintf(cp, bufSize - (cp - buf), "%d", g->members[i]);
		} else {
			written = snprintf(cp, bufSize - (cp - buf), ";%d", g->members[i]);
		}
		if (written < 0 || (size_t) written >= bufSize - (cp - buf)) {
			break;
		}
		cp += written;
	}
}

void LayerGroupParseMembers(int groupIdx, const char *list)
{
	if (!GroupValid(groupIdx) || list == NULL) {
		return;
	}

	layerGroupEntry_t *g = &groups[groupIdx];
	g->memberCount = 0;

	char *copy = strdup(list);
	if (copy == NULL) {
		abort();
	}

	char *cp = copy;
	while (cp && *cp) {
		char *sep = strpbrk(cp, ",; ");
		if (sep) {
			*sep = '\0';
		}
		if (*cp) {
			int layerIdx = (int) strtol(cp, NULL, 0);
			if (layerIdx > 0) {
				LayerGroupAddMember(groupIdx, layerIdx);
			}
		}
		cp = sep ? sep + 1 : NULL;
	}

	free(copy);
}

/* Builds the candidate migrated member set for layer index i (0-based):
 * {i+1} union linkLists[i][0..linkCounts[i]). Returns the count written
 * into out (caller-provided buffer, sized linkCounts[i]+1). */
static int BuildMigratedSet(int i, const int *linkList, int linkCount, int *out)
{
	int n = 0;
	out[n++] = i + 1;
	for (int j = 0; j < linkCount; j++) {
		int already = 0;
		for (int k = 0; k < n; k++) {
			if (out[k] == linkList[j]) {
				already = 1;
				break;
			}
		}
		if (!already) {
			out[n++] = linkList[j];
		}
	}
	return n;
}

static int SetsEqual(const int *a, int aCount, const int *b, int bCount)
{
	if (aCount != bCount) {
		return 0;
	}
	for (int i = 0; i < aCount; i++) {
		int found = 0;
		for (int j = 0; j < bCount; j++) {
			if (a[i] == b[j]) {
				found = 1;
				break;
			}
		}
		if (!found) {
			return 0;
		}
	}
	return 1;
}

static int NextAutoGroupNumber(void)
{
	int n = 1;
	char candidate[LAYERGROUP_NAME_SIZE];
	for (;;) {
		snprintf(candidate, sizeof candidate, "Group %d", n);
		int collides = 0;
		for (int i = 0; i < groupCount; i++) {
			if (strcmp(groups[i].name, candidate) == 0) {
				collides = 1;
				break;
			}
		}
		if (!collides) {
			return n;
		}
		n++;
	}
}

int LayerGroupMigrateFromLinkLists(int layerCount,
                                   int *const *linkLists,
                                   const int *linkCounts)
{
	int created = 0;

	/* Track migrated-set membership for already-created groups this call
	 * made, so a mutual link (A->B and B->A) collapses to one group. */
	int **madeSets = calloc((size_t) layerCount, sizeof * madeSets);
	int *madeCounts = calloc((size_t) layerCount, sizeof * madeCounts);
	int madeN = 0;

	for (int i = 0; i < layerCount; i++) {
		int linkCount = linkCounts ? linkCounts[i] : 0;
		if (linkCount <= 0) {
			continue;
		}

		int *candidate = malloc((size_t)(linkCount + 1) * sizeof * candidate);
		if (candidate == NULL) {
			abort();
		}
		int candidateN = BuildMigratedSet(i, linkLists[i], linkCount, candidate);

		int duplicate = 0;
		for (int m = 0; m < madeN; m++) {
			if (SetsEqual(candidate, candidateN, madeSets[m], madeCounts[m])) {
				duplicate = 1;
				break;
			}
		}

		if (duplicate) {
			free(candidate);
			continue;
		}

		char name[LAYERGROUP_NAME_SIZE];
		snprintf(name, sizeof name, "Group %d", NextAutoGroupNumber());
		int g = LayerGroupCreate(name);
		for (int m = 0; m < candidateN; m++) {
			LayerGroupAddMember(g, candidate[m]);
		}
		created++;

		madeSets[madeN] = candidate;
		madeCounts[madeN] = candidateN;
		madeN++;
	}

	for (int m = 0; m < madeN; m++) {
		free(madeSets[m]);
	}
	free(madeSets);
	free(madeCounts);

	return created;
}
