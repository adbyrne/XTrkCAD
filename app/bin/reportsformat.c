/** \file reportsformat.c
 * Pure text-formatting helpers for the Reports feature (SF #217), split out
 * from reports.c so they carry no wlib/track-database dependencies -- same
 * rationale as cars/careditlogic.c: cheaply CMocka-testable on their own,
 * without stubbing a large dialog/viewer dependency surface (see
 * unittest/carregistrytest.c's header comment for why the alternative,
 * -ffunction-sections/--gc-sections dead-code stripping, was tried and
 * reverted as unreliable across toolchains).
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
#include <strings.h>

#include "cJSON.h"

#include "include/reports.h"

void ReportsFormatEndPtNote(const reportsEndPt_t *entry, char *buf,
                            size_t bufSize)
{
	BOOL_T first = 1;

	buf[0] = '\0';

	/* Plain string concatenation, not a fixed enum/switch -- any subset
	 * of these can apply to the same row (e.g. an isolated stub that
	 * also happens to sit near another open endpoint), and a future
	 * addition is just another `if` block, no format-string rework. */
	if (entry->turntable) {
		strncat(buf, "Turntable", bufSize - strlen(buf) - 1);
		first = 0;
	}
	if (entry->isolated) {
		if (!first) { strncat(buf, ", ", bufSize - strlen(buf) - 1); }
		strncat(buf, "Isolated", bufSize - strlen(buf) - 1);
		first = 0;
	}
	if (entry->nearby) {
		if (!first) { strncat(buf, ", ", bufSize - strlen(buf) - 1); }
		strncat(buf, "Nearby", bufSize - strlen(buf) - 1);
	}
}

void ReportsFormatUnconnectedList(DynString *out, const reportsEndPt_t *list,
                                  int count)
{
	char line[160];
	char note[32];
	int i;

	for (i = 0; i < count; i++) {
		ReportsFormatEndPtNote(&list[i], note, sizeof note);
		snprintf(line, sizeof line, "%6d | %5u | %-16s | %8.3f | %8.3f | %7.3f | %s\n",
		         list[i].trackId, list[i].layer, list[i].layerName,
		         list[i].pos.x, list[i].pos.y, list[i].angle, note);
		DynStringCatCStr(out, line);
	}
}

/* Phase 2 batch (track lengths / curve stats / turnout density / equipment
 * suitability) -- SF #217. Data/thresholds started as ports of an external
 * MCP project's reference implementation (its write_turnout_report()/
 * write_equipment_report() functions -- design history, not a citation a
 * reader here can open); as of 2026-09-04, exact *text* parity with those
 * is explicitly not required (user request) -- the row layouts below are
 * this file's own design, improved for readability where it helps, not a
 * byte-for-byte port. */

void ReportsFormatTrackLengthList(DynString *out,
                                  const reportsTrackLenLayer_t *list, int count, DIST_T totalFt)
{
	char line[128];
	int i;

	for (i = 0; i < count; i++) {
		DIST_T pct = totalFt > 0.0 ? (list[i].lengthFt / totalFt * 100.0) : 0.0;

		snprintf(line, sizeof line,
		         "  %3u  %-16s  %8.1f  %9.1f  %5d  %5d  %5.1f%%\n",
		         list[i].layer, list[i].name, list[i].lengthFt,
		         list[i].lengthIn, list[i].turnoutCount, list[i].carCapacity, pct);
		DynStringCatCStr(out, line);
	}
}

void ReportsFormatCurveHistogram(DynString *out,
                                 const reportsCurveBucket_t *list, int count)
{
	char line[64];
	int i;

	for (i = 0; i < count; i++) {
		snprintf(line, sizeof line, "  %-8s  %5d\n", list[i].label, list[i].count);
		DynStringCatCStr(out, line);
	}
}

DIST_T ReportsCarsPerFoot(const char *scaleName)
{
	static const struct {
		const char *name;
		DIST_T factor;
	} table[] = {
		{ "O", 1.0 }, { "S", 1.5 }, { "HO", 2.0 }, { "N", 4.0 }, { "Z", 5.0 },
		{ "HOn3", 2.0 }, { "On3", 1.0 }, { "Sn3", 1.5 }, { "Nn3", 4.0 },
		{ "G", 0.5 }, { "TT", 3.0 }, { "I", 0.75 },
	};
	size_t i;

	if (scaleName) {
		for (i = 0; i < sizeof table / sizeof table[0]; i++) {
			if (strcasecmp(scaleName, table[i].name) == 0) {
				return table[i].factor;
			}
		}
	}
	return 2.0;
}

const char *ReportsCurveBucketLabel(DIST_T radius)
{
	if (radius < 12.0) {
		return "< 12in";
	}
	if (radius < 18.0) {
		return "12-18in";
	}
	if (radius < 24.0) {
		return "18-24in";
	}
	if (radius < 36.0) {
		return "24-36in";
	}
	return "> 36in";
}

/* Sorts a local index array by list[idx].density descending, stable
 * (insertion sort: small N -- a handful of layers in any real layout --
 * so O(n^2) is irrelevant and this avoids qsort_r's non-portable
 * signature across platforms/toolchains). Falls back to unsorted order
 * (index 0..count-1) on allocation failure rather than crashing -- this
 * is a display nicety, not correctness-critical. */
void ReportsFormatTurnoutList(DynString *out,
                              const reportsTurnoutLayer_t *list, int count)
{
	char line[128];
	int i, j;
	int *order = count > 0 ? malloc((size_t) count * sizeof(int)) : NULL;

	for (i = 0; order && i < count; i++) {
		DIST_T keyDensity = list[i].density;

		j = i - 1;
		while (j >= 0 && list[order[j]].density < keyDensity) {
			order[j + 1] = order[j];
			j--;
		}
		order[j + 1] = i;
	}

	for (i = 0; i < count; i++) {
		int idx = order ? order[i] : i;

		snprintf(line, sizeof line, "  %3u  %-16s  %8.1f  %5d  %14.1f",
		         list[idx].layer, list[idx].name, list[idx].feet,
		         list[idx].turnoutCount, list[idx].density);
		DynStringCatCStr(out, line);
		if (list[idx].heavy) {
			DynStringCatCStr(out, "  HEAVY");
		}
		DynStringCatCStr(out, "\n");
	}

	free(order);
}

void ReportsFormatPartialTurnoutList(DynString *out,
                                     const reportsPartialTurnout_t *list, int count)
{
	char line[64];
	int i;

	for (i = 0; i < count; i++) {
		snprintf(line, sizeof line, "  ID %d: %d endpoint(s), layer %u\n",
		         list[i].trackId, list[i].endPtCnt, list[i].layer);
		DynStringCatCStr(out, line);
	}
}

static void ReportsFormatEquipmentGroup(DynString *out,
                                        const reportsEquipClass_t *list, int count, reportsEquipStatus_e status,
                                        const char *heading, BOOL_T *firstGroup)
{
	char line[128];
	int i;
	BOOL_T any = 0;

	for (i = 0; i < count; i++) {
		if (list[i].status != status) {
			continue;
		}
		if (!any) {
			if (!*firstGroup) {
				DynStringCatCStr(out, "\n");
			}
			DynStringCatCStr(out, heading);
			DynStringCatCStr(out, "\n");
			any = 1;
			*firstGroup = 0;
		}
		snprintf(line, sizeof line, "  %-44s  %8.1f\"\n",
		         list[i].name, list[i].thresholdIn);
		DynStringCatCStr(out, line);
	}
}

void ReportsFormatEquipmentList(DynString *out,
                                const reportsEquipClass_t *list, int count)
{
	BOOL_T firstGroup = 1;

	ReportsFormatEquipmentGroup(out, list, count, REPORTS_EQUIP_PASS, "PASS",
	                            &firstGroup);
	ReportsFormatEquipmentGroup(out, list, count, REPORTS_EQUIP_MARGINAL,
	                            "MARGINAL", &firstGroup);
	ReportsFormatEquipmentGroup(out, list, count, REPORTS_EQUIP_FAIL, "FAIL",
	                            &firstGroup);
}

BOOL_T ReportsIsTurnoutHeavy(DIST_T density)
{
	return density > 10.0;
}

reportsEquipStatus_e ReportsClassifyEquipment(DIST_T minRadius,
                DIST_T thresholdIn, DIST_T scaleFactor)
{
	if (minRadius >= thresholdIn) {
		return REPORTS_EQUIP_PASS;
	}
	if (minRadius >= thresholdIn - 2.0 * scaleFactor) {
		return REPORTS_EQUIP_MARGINAL;
	}
	return REPORTS_EQUIP_FAIL;
}

/* Phase 3/3a batch (Gaps, Kinked Joints) -- SF #217/#779. Both interactive
 * reports; these formatters are the pure/testable half only -- the compute
 * passes that walk live track_p state (near-miss pairing, kinked-joint
 * detection) live in reports.c, fixture/live-verified only, same split as
 * every prior phase's compute pass. */

void ReportsFormatGapList(DynString *out, const reportsGapPair_t *list,
                          int count)
{
	char line[128];
	int i;

	for (i = 0; i < count; i++) {
		snprintf(line, sizeof line,
		         "%6d | %5u | %-14s | %6d | %5u | %-14s | %8.4f\n",
		         list[i].trackA, list[i].layerA, list[i].layerNameA,
		         list[i].trackB, list[i].layerB, list[i].layerNameB,
		         list[i].gapIn);
		DynStringCatCStr(out, line);
	}
}

void ReportsFormatKinkedList(DynString *out, const reportsKinkedJoint_t *list,
                             int count)
{
	char line[128];
	int i;

	for (i = 0; i < count; i++) {
		snprintf(line, sizeof line,
		         "%6d | %5u | %-14s | %6d | %5u | %-14s | %7.3f\n",
		         list[i].trackA, list[i].layerA, list[i].layerNameA,
		         list[i].trackB, list[i].layerB, list[i].layerNameB,
		         list[i].angleDelta);
		DynStringCatCStr(out, line);
	}
}

/* Notes Report (SF #799, restructured SF #800 phase 2) -- same
 * grouped-subheading shape as ReportsFormatEquipmentList(), generalized
 * further: Type first (a fixed, closed set), then within JSON notes,
 * one heading per distinct \c group value actually present, sorted
 * alphabetically rather than drawn from a fixed list -- see
 * reportsNoteRow_t's own doc comment in reports.h. */

static const struct {
	int type;
	const char *heading;
} reportsNoteTypeGroups[] = {
	{ REPORTS_NOTE_OP_TEXT, "Text Notes" },
	{ REPORTS_NOTE_OP_LINK, "Weblink Notes" },
	{ REPORTS_NOTE_OP_FILE, "Document Notes" },
};

static void ReportsFormatNoteRow(DynString *out, const reportsNoteRow_t *row)
{
	/* id (63 chars) + label (127 chars) + surrounding literal text, with
	 * headroom -- both fields are already bounded by reportsNoteRow_t's
	 * own array sizes, this just needs to be at least as large. */
	char line[256];

	snprintf(line, sizeof line, "  ID %2d: %-12s %-32s layer %u\n",
	         row->noteIndex, row->id, row->label, row->layer);
	DynStringCatCStr(out, line);

	/* SF #802: the on-screen table only ever showed id/label/layer, never
	 * the note's actual JSON body -- print it here, Save/Print output
	 * only, indented under the row it belongs to. */
	if (row->rawJson) {
		DynStringCatCStr(out, "      ");
		DynStringCatCStr(out, row->rawJson);
		DynStringCatCStr(out, "\n");
	}
}

static void ReportsFormatNoteTypeGroup(DynString *out,
                                       const reportsNoteRow_t *list, int count, int type,
                                       const char *heading, BOOL_T *firstGroup)
{
	int i;
	BOOL_T any = 0;

	for (i = 0; i < count; i++) {
		if (list[i].type != type) {
			continue;
		}
		if (!any) {
			if (!*firstGroup) {
				DynStringCatCStr(out, "\n");
			}
			DynStringCatCStr(out, heading);
			DynStringCatCStr(out, "\n");
			any = 1;
			*firstGroup = 0;
		}
		ReportsFormatNoteRow(out, &list[i]);
	}
}

/** Collect the distinct \c group values present among \p list's JSON
 * (REPORTS_NOTE_OP_JSON) rows into \p groups (caller-sized array of
 * pointers into \p list's own storage -- valid only as long as \p list
 * is), sorted alphabetically. Returns the count found, capped at
 * \p maxGroups. */
static int ReportsCollectJsonGroups(const reportsNoteRow_t *list, int count,
                                    const char **groups, int maxGroups)
{
	int i, j, found = 0;

	for (i = 0; i < count; i++) {
		if (list[i].type != REPORTS_NOTE_OP_JSON) {
			continue;
		}
		BOOL_T seen = 0;
		for (j = 0; j < found; j++) {
			if (strcmp(groups[j], list[i].group) == 0) {
				seen = 1;
				break;
			}
		}
		if (!seen && found < maxGroups) {
			groups[found++] = list[i].group;
		}
	}

	/* Simple insertion sort -- found is at most a handful of distinct
	 * group names per report, no need for anything fancier. */
	for (i = 1; i < found; i++) {
		const char *key = groups[i];
		j = i - 1;
		while (j >= 0 && strcmp(groups[j], key) > 0) {
			groups[j + 1] = groups[j];
			j--;
		}
		groups[j + 1] = key;
	}

	return found;
}

static void ReportsFormatNoteJsonGroup(DynString *out,
                                       const reportsNoteRow_t *list, int count, const char *group,
                                       BOOL_T *firstGroup)
{
	int i;
	BOOL_T any = 0;

	for (i = 0; i < count; i++) {
		if (list[i].type != REPORTS_NOTE_OP_JSON || strcmp(list[i].group, group) != 0) {
			continue;
		}
		if (!any) {
			if (!*firstGroup) {
				DynStringCatCStr(out, "\n");
			}
			DynStringCatCStr(out, group);
			DynStringCatCStr(out, "\n");
			any = 1;
			*firstGroup = 0;
		}
		ReportsFormatNoteRow(out, &list[i]);
	}
}

void ReportsFormatNoteList(DynString *out, const reportsNoteRow_t *list,
                           int count)
{
	BOOL_T firstGroup = 1;
	size_t i;
	/* Headroom well beyond anything a real "Manage Notes" registry (SF
	 * #800 phase 3) is likely to hold -- this just bounds a local array,
	 * extra distinct names beyond this are silently omitted from the
	 * Save/Print grouping (still present in the interactive list). */
	const char *jsonGroups[64];
	int jsonGroupCount;

	for (i = 0; i < sizeof reportsNoteTypeGroups / sizeof
	     reportsNoteTypeGroups[0]; i++) {
		ReportsFormatNoteTypeGroup(out, list, count, reportsNoteTypeGroups[i].type,
		                           reportsNoteTypeGroups[i].heading, &firstGroup);
	}

	jsonGroupCount = ReportsCollectJsonGroups(list, count, jsonGroups,
	                 (int)(sizeof jsonGroups / sizeof jsonGroups[0]));
	for (i = 0; i < (size_t)jsonGroupCount; i++) {
		ReportsFormatNoteJsonGroup(out, list, count, jsonGroups[i], &firstGroup);
	}
}

const char *ReportsNoteResolveGroup(cJSON *parsed,
                                    const char * const *registeredNames, int registeredCount)
{
	cJSON *field;
	int i;

	/* No registry entries at all (SF #800 phase 2's own state, until
	 * phase 3's Manage Notes dialog exists) -- every JSON Note falls
	 * under "ROOT", the correct, expected interim state, not a bug.
	 * Skip the scan entirely rather than let it fall through the loop
	 * finding nothing every time. */
	if (registeredCount <= 0 || registeredNames == NULL) {
		return "ROOT";
	}

	/* cJSON_GetObjectItem walks a linked list, so scanning the note's own
	 * (typically small, single-digit) field count once per registered
	 * name is simplest correct approach; a registry large enough to make
	 * this a real cost is not a realistic near-term scenario. */
	for (i = 0; i < registeredCount; i++) {
		field = cJSON_GetObjectItemCaseSensitive(parsed, registeredNames[i]);
		if (field != NULL) {
			return registeredNames[i];
		}
	}

	return "ROOT";
}

void ReportsNoteExtractLabel(cJSON *parsed, char *buf, size_t bufSize)
{
	static const char *labelKeys[] = { "name", "label" };
	size_t i;

	if (bufSize == 0) {
		return;
	}
	buf[0] = '\0';

	for (i = 0; i < sizeof labelKeys / sizeof labelKeys[0]; i++) {
		cJSON *field = cJSON_GetObjectItemCaseSensitive(parsed, labelKeys[i]);
		if (field && cJSON_IsString(field) && field->valuestring) {
			strncpy(buf, field->valuestring, bufSize - 1);
			buf[bufSize - 1] = '\0';
			return;
		}
	}
}
