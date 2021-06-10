/** \file cturnout.c
 * Turnout object handling and drawing
 */

 /*  XTrkCad - Model Railroad CAD
  *  Copyright (C) 2005 Dave Bullis
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

#include "common.h"
#include "ccurve.h"
#include "tbezier.h"
#include "tcornu.h"
#include "cjoin.h"
#include "compound.h"
#include "cstraigh.h"
#include "cundo.h"
#include "custom.h"
#include "fileio.h"
#include "layout.h"
#include "param.h"
#include "cselect.h"
#include "include/paramfile.h"
#include "track.h"
#include "trackx.h"
#include "common-ui.h"

EXPORT TRKTYP_T T_TURNOUT = -1;

#define TURNOUTCMD

#define MIN_TURNOUT_SEG_CONNECT_DIST		(0.1)

EXPORT dynArr_t turnoutInfo_da;

EXPORT turnoutInfo_t* curTurnout = NULL;
EXPORT long curTurnoutEp = 0;
static int curTurnoutInx = -1;

static int log_turnout = 0;
static int log_traverseTurnout = 0;
static int log_suppressCheckPaths = 0;
static int log_splitturnout = 0;

static wMenu_p turnoutPopupM;

#ifdef TURNOUTCMD
static drawCmd_t turnoutD = {
		NULL,
		&screenDrawFuncs,
		0,
		1.0,
		0.0,
		{0.0,0.0}, {0.0,0.0},
		Pix2CoOrd, CoOrd2Pix };

static wIndex_t turnoutHotBarCmdInx;
static wIndex_t turnoutInx;
static long hideTurnoutWindow;
static void RedrawTurnout( wDraw_p d, void * context, wWinPix_t x, wWinPix_t y );
static void SelTurnoutEndPt(wIndex_t, coOrd);
static void HilightEndPt(void);

static wWinPix_t turnoutListWidths[] = { 80, 80, 220 };
static const char* turnoutListTitles[] = { N_("Manufacturer"), N_("Part No"), N_("Description") };
static paramListData_t listData = { 13, 400, 3, turnoutListWidths, turnoutListTitles };
static const char* hideLabels[] = { N_("Hide"), NULL };
static paramDrawData_t turnoutDrawData = { 490, 200, RedrawTurnout, SelTurnoutEndPt, &turnoutD };
static paramData_t turnoutPLs[] = {
#define I_LIST		(0)
#define turnoutListL    ((wList_p)turnoutPLs[I_LIST].control)
	{   PD_LIST, &turnoutInx, "list", PDO_NOPREF | PDO_DLGRESIZEW, &listData, NULL, BL_DUP },
#define I_DRAW		(1)
#define turnoutDrawD    ((wDraw_p)turnoutPLs[I_DRAW].control)
	{   PD_DRAW, NULL, "canvas", PDO_NOPSHUPD | PDO_DLGUNDERCMDBUTT | PDO_DLGRESIZE, &turnoutDrawData, NULL, 0 },
#define I_NEW		(2)
#define turnoutNewM     ((wMenu_p)turnoutPLs[I_NEW].control)
	{   PD_MENU, NULL, "new", PDO_DLGCMDBUTTON, NULL, N_("New") },
#define I_HIDE		(3)
#define turnoutHideT    ((wChoice_p)turnoutPLs[I_HIDE].control)
	{   PD_TOGGLE, &hideTurnoutWindow, "hide", PDO_DLGCMDBUTTON, hideLabels, NULL, BC_NOBORDER } };
static paramGroup_t turnoutPG = { "turnout", 0, turnoutPLs, COUNT( turnoutPLs ) };
#endif

/* Draw turnout data */
#define DTO_INVALID 0

#define DTO_NORMAL 1
#define DTO_THREE 2
#define DTO_WYE 3

#define DTO_CURVED 4

#define DTO_XING 5
#define DTO_XNG9 6
#define DTO_SSLIP 7
#define DTO_DSLIP 8

#define DTO_LCROSS 9
#define DTO_RCROSS 10
#define DTO_DCROSS 11

// Define to plot control points (DTO_NORMAL, DTO_CURVED, DTO_XING, DTO_LCROSS)
// #define DTO_DEBUG DTO_XING

#define DTO_DIM 16
#define DTO_SEGS 24

static struct DrawToData_t {
	TRKINX_T index;
	int toType;
	track_p trk;
	int bridge; 
	int endCnt;
	int pathCnt;
	int routeCnt;
	int strCnt;
	int crvCnt;
	int rgtCnt;
	int lftCnt;
	int strPath;
	int str2Path;
	int crvPath;
	int crv2Path;
	int origCnt;
	int origins[DTO_DIM];
	coOrd midPt;
	struct extraDataCompound_t* xx;
} dtod;

struct DrawTo_t {
	int n;
	trkSeg_p trkSeg[DTO_SEGS];
	coOrd base[DTO_SEGS];
	DIST_T dy[DTO_SEGS];
	ANGLE_T angle;
	ANGLE_T crvAngle;
	coOrd pts[DTO_SEGS];
	char type;
};

static struct DrawTo_t dto[DTO_DIM];



/****************************************
 *
 * TURNOUT LIST MANAGEMENT
 *
 */


EXPORT turnoutInfo_t* CreateNewTurnout(
	char* scale,
	char* title,
	wIndex_t segCnt,
	trkSeg_p segData,
	PATHPTR_T paths,
	EPINX_T endPtCnt,
	trkEndPt_t* endPts,
	DIST_T* radii,
	wBool_t updateList,
	long options)
{
	turnoutInfo_t* to;
	long changes = 0;

	to = FindCompound(FIND_TURNOUT, scale, title);
	if (to == NULL) {
		DYNARR_APPEND(turnoutInfo_t*, turnoutInfo_da, 10);
		to = (turnoutInfo_t*)MyMalloc(sizeof * to);
		turnoutInfo(turnoutInfo_da.cnt - 1) = to;
		to->title = MyStrdup(title);
		to->scaleInx = LookupScale(scale);
		changes = CHANGE_PARAMS;
	}
	to->segCnt = segCnt;
	trkSeg_p seg_p;
	to->segs = (trkSeg_p)memdup(segData, (sizeof(*segData) * segCnt));
	seg_p = to->segs;
	for (int i = 0; i < segCnt; i++) {
		seg_p[i].bezSegs.ptr = NULL;
		seg_p[i].bezSegs.cnt = 0;
		seg_p[i].bezSegs.max = 0;
	}
	CopyPoly(to->segs, segCnt);
	FixUpBezierSegs(to->segs, to->segCnt);
	GetSegBounds(zero, 0.0, segCnt, to->segs, &to->orig, &to->size);
	to->endCnt = endPtCnt;
	to->endPt = (trkEndPt_t*)memdup(endPts, (sizeof * endPts) * to->endCnt);

	if (options & COMPOUND_OPTION_PATH_OVERRIDE)
		to->pathOverRide = TRUE;
	if (options & COMPOUND_OPTION_PATH_NOCOMBINE)
		to->pathNoCombine = TRUE;
	wIndex_t pathsLen = GetPathsLength(paths);
	to->paths = (PATHPTR_T)memdup(paths, pathsLen * (sizeof * to->paths));
	to->paramFileIndex = curParamFileIndex;
	if (curParamFileIndex == PARAM_CUSTOM)
		to->contentsLabel = MyStrdup("Custom Turnouts");
	else
		to->contentsLabel = curSubContents;
#ifdef TURNOUTCMD
	if (updateList && turnoutListL != NULL) {
		FormatCompoundTitle(LABEL_TABBED | LABEL_MANUF | LABEL_PARTNO | LABEL_DESCR, title);
		if (message[0] != '\0')
			wListAddValue(turnoutListL, message, NULL, to);
	}
#endif

	to->barScale = curBarScale > 0 ? curBarScale : -1;
	to->special = TOnormal;
	if (radii) {
		to->special = TOcurved;
		DYNARR_SET(DIST_T, to->u.curved.radii, to->endCnt);
		for (int i = 0; i < to->endCnt; i++) {
			DYNARR_N(DIST_T, to->u.curved.radii, i) = radii[i];
		}
	}
	if (updateList && changes)
		DoChangeNotification(changes);
	return to;
}

/**
 * Delete a turnout parameter from the list and free the related memory
 *
 * \param [IN] toInfo turnout definition to be deleted
 *
 * \returns True if it succeeds
 */

BOOL_T
DeleteTurnout(void* toInfo)
{
	turnoutInfo_t* to = (turnoutInfo_t*)toInfo;
	MyFree(to->title);
	MyFree(to->segs);
	MyFree(to->endPt);
	MyFree(to->paths);
	if (to->special) {
		switch (to->special) {
		case TOcurved:
			DYNARR_FREE(DIST_T, to->u.curved.radii);
			break;
		case TOadjustable:
		default:;
		}
	}

	MyFree(to);
	return(TRUE);
}

/**
 * Delete all turnout definitions that came from a specific parameter file.
 * Due to the way the definitions are loaded from file it is safe to
 * assume that they form a contiguous block in the array.
 *
 * \param [IN] fileIndex parameter file
 */

void
DeleteTurnoutParams(int fileIndex)
{
	int inx = 0;
	int startInx = -1;
	int cnt = 0;

	// go to the start of the block
	while (inx < turnoutInfo_da.cnt &&
		turnoutInfo(inx)->paramFileIndex != fileIndex) {
		startInx = inx++;
	}

	// delete them
	for (; inx < turnoutInfo_da.cnt &&
		turnoutInfo(inx)->paramFileIndex == fileIndex; inx++) {
		turnoutInfo_t* to = turnoutInfo(inx);
		if (to->paramFileIndex == fileIndex) {
			DeleteTurnout(to);
			cnt++;
		}
	}

	// copy down the rest of the list to fill the gap
	startInx++;
	while (inx < turnoutInfo_da.cnt) {
		turnoutInfo(startInx++) = turnoutInfo(inx++);
	}

	// and reduce the actual number
	turnoutInfo_da.cnt -= cnt;
}

/**
 * Check to find out to what extent the contents of the parameter file can be used with
 * the current layout scale / gauge.
 *
 * If parameter scale == layout and parameter gauge == layout we have an exact fit.
 * If parameter gauge == layout we have compatible track.
 * OO, O and N scales are special cased. If the layout is in OO scale track in HO is considered
 * an exact fit in spite of scale differences.
 *
 * \param paramFileIndex
 * \param scaleIndex
 * \return enum paraFileState
 */

enum paramFileState
	GetTrackCompatibility(int paramFileIndex, SCALEINX_T scaleIndex)
{
	int i;
	enum paramFileState ret = PARAMFILE_NOTUSABLE;
	DIST_T gauge = GetScaleTrackGauge(scaleIndex);

	if (!IsParamValid(paramFileIndex)) {
		return(PARAMFILE_UNLOADED);
	}

	// loop over all parameter entries or until a exact fit is found
	for (i = 0; i < turnoutInfo_da.cnt && ret < PARAMFILE_FIT; i++) {
		turnoutInfo_t* to = turnoutInfo(i);
		if (to->paramFileIndex == paramFileIndex) {
			SCALE_FIT_T fit = CompatibleScale(FIT_TURNOUT, to->scaleInx, scaleIndex);
			if (fit == FIT_EXACT) {
				ret = PARAMFILE_FIT;
				break;
			}
			else if (fit == FIT_COMPATIBLE) {
				ret = PARAMFILE_COMPATIBLE;
			}
		}
	}
	return(ret);
}

/**
 * Check Paths verifies that each track segment is on at least one path.
 * It will assume new-P or old-P order is possible and does not change it.
 *
 * \param segCnt
 * \param segs
 * \param paths
 *
 * \returns -1 if a track segment is not on a path
 */
EXPORT wIndex_t CheckPaths(
	wIndex_t segCnt,
	trkSeg_p segs,
	PATHPTR_T paths)
{
	if ((segCnt == 0) || !segs) return -1;
	int pc, ps;
	PATHPTR_T pp = 0;

	int segInx[2], segEp[2];
	int segTrkLast = -1;

	// Check that each track segment is on at least one path
	// Note - In new-P the tracks may be preceded by draws (or interspersed by them)
	int suppressCheckPaths = log_suppressCheckPaths > 0 ? logTable(log_suppressCheckPaths).level : 0;
	if (suppressCheckPaths == 0) {
		for (int inx = 0; inx < segCnt; inx++) {
			if (IsSegTrack(&segs[inx])) {
				PATHPTR_T cp = paths;
				while (*cp) {
					// 0-9 are x00 to x09 or the negative equivalent (backwards)
					// Pathlist is: Path00Path000
					// Path is: NAME01203400
					for (; *cp; cp++);  //Skip Name
					cp++; //Skip 0 after name
					// check each path component 
					for (; cp[0] || cp[1]; cp++) {  //keeps going even if there are two or more parts
						if (!cp[0]) continue;   //ignore the 0 between parts of the same PATH!!
						GetSegInxEP(cp[0], &segInx[0], &segEp[0]); //GetSegInxEP subtracts one to match inx
						if (segInx[0] == inx) break;  //Found it!
					}
					if (*cp)	// we broke early
						break;  // get out - we found it
					cp++;
					cp++;  // Go to next path - past two 0s
				}
				if (!*cp) {	// we looked through all the paths and didn't find it
					InputError("Track segment %d not on Path", FALSE, inx + 1);
					return -1;;
				}
			}
		}
	}

	for (pc = 0, pp = paths; *pp; pp += 2, pc++) {
		for (ps = 0, pp += strlen((char*)pp) + 1; pp[0] != 0 || pp[1] != 0; pp++, ps++) {
			if (pp[0] != 0 && ps == 0) {  // First or only one
			}
			if (pp[0] != 0 && pp[1] != 0) {
				/* check connectivity */
				DIST_T d;
				GetSegInxEP(pp[0], &segInx[0], &segEp[0]);
				GetSegInxEP(pp[1], &segInx[1], &segEp[1]);
				if (!IsSegTrack(&segs[segInx[0]])) {
					InputError(_("CheckPath: Turnout path[%d] %d is not a track segment"),
						FALSE, pc, pp[0]);
					return -1;
				}
				if (!IsSegTrack(&segs[segInx[1]])) {
					InputError(_("CheckPath: Turnout path[%d] %d is not a track segment"),
						FALSE, pc, pp[1]);
					return -1;
				}
				coOrd p0 = GetSegEndPt(&segs[segInx[0]], 1 - segEp[0], FALSE, NULL);
				coOrd p1 = GetSegEndPt(&segs[segInx[1]], segEp[1], FALSE, NULL);
				d = FindDistance(p0, p1);
				if (d > MIN_TURNOUT_SEG_CONNECT_DIST) {
					InputError(_("CheckPath: Turnout path[%d] %d-%d not connected: %0.3f P0(%f,%f) P1(%f,%f)"),
						FALSE, pc, pp[0], pp[1], d, p0.x, p0.y, p1.x, p1.y);
					return -1;
				}
			}

		}
	}
	return (wIndex_t)(pp - paths + 1);
}


static BOOL_T ReadTurnoutParam(
	char* firstLine)
{
	char scale[10];
	char* title;
	turnoutInfo_t* to;
	PATHPTR_T cp;
	long options = 0;

	if (!GetArgs(firstLine + 8, "sqc", scale, &title, &cp))
		return FALSE;
	if (cp != NULL)
		if (!GetArgs((char*)cp, "l", &options))
			return FALSE;
	DYNARR_RESET(trkEndPt_t, tempEndPts_da);
	pathCnt = 0;
	if (!ReadSegs())
		return FALSE;
	CheckPaths(tempSegs_da.cnt, &tempSegs(0), pathPtr);
	to = CreateNewTurnout(scale, title, tempSegs_da.cnt, &tempSegs(0),
		pathPtr, tempEndPts_da.cnt, &tempEndPts(0), NULL, FALSE, options);
	MyFree(title);
	if (to == NULL)
		return FALSE;
	if (tempSpecial[0] != '\0') {
		if (strncmp(tempSpecial, ADJUSTABLE, strlen(ADJUSTABLE)) == 0) {
			to->special = TOadjustable;
			if (!GetArgs(tempSpecial + strlen(ADJUSTABLE), "ff",
				&to->u.adjustable.minD, &to->u.adjustable.maxD))
				return FALSE;
		}
		else {
			InputError(_("Unknown special case"), TRUE);
			return FALSE;
		}
	}
	if (tempCustom[0] != '\0') {
		to->customInfo = MyStrdup(tempCustom);
	}
	return TRUE;
}


EXPORT turnoutInfo_t* TurnoutAdd(long mode, SCALEINX_T scale, wList_p list, coOrd* maxDim, EPINX_T epCnt)
{
	wIndex_t inx;
	turnoutInfo_t* to, * to1 = NULL;
	turnoutInx = 0;
	for (inx = 0; inx < turnoutInfo_da.cnt; inx++) {
		to = turnoutInfo(inx);
		if (IsParamValid(to->paramFileIndex) &&
			to->segCnt > 0 &&
			(FIT_NONE != CompatibleScale(FIT_TURNOUT, to->scaleInx, scale)) &&
			/*strcasecmp( to->scale, scaleName ) == 0 && */
			(epCnt <= 0 || epCnt == to->endCnt)) {
			if (to1 == NULL)
				to1 = to;
			if (to == curTurnout) {
				to1 = to;
				turnoutInx = wListGetCount(list);
			}
			FormatCompoundTitle(mode, to->title);
			if (message[0] != '\0') {
				wListAddValue(list, message, NULL, to);
				if (maxDim) {
					if (to->size.x > maxDim->x)
						maxDim->x = to->size.x;
					if (to->size.y > maxDim->y)
						maxDim->y = to->size.y;
				}
			}
		}
	}
	return to1;
}

/****************************************
 *
 * Adjustable Track Support
 *
 */


static void ChangeAdjustableEndPt(
	track_p trk,
	EPINX_T ep,
	DIST_T d)
{
	struct extraDataCompound_t* xx = GET_EXTRA_DATA(trk, T_TURNOUT, extraDataCompound_t);
	coOrd pos;
	trkSeg_p segPtr;
	ANGLE_T angle = GetTrkEndAngle(trk, ep);
	Translate(&pos, GetTrkEndPos(trk, 1 - ep), angle, d);
	UndoModify(trk);
	SetTrkEndPoint(trk, ep, pos, angle);
	if (ep == 0)
		xx->orig = pos;
	for (segPtr = xx->segs; segPtr < &xx->segs[xx->segCnt]; segPtr++) {
		switch (segPtr->type) {
		case SEG_STRLIN:
		case SEG_STRTRK:
			segPtr->u.l.pos[1].x = d;
			break;
		default:
			;
		}
	}
	ComputeBoundingBox(trk);
	DrawNewTrack(trk);
}


EXPORT BOOL_T ConnectAdjustableTracks(
	track_p trk1,
	EPINX_T ep1,
	track_p trk2,
	EPINX_T ep2)
{
	struct extraDataCompound_t* xx1;
	struct extraDataCompound_t* xx2;
	BOOL_T adj1, adj2;
	coOrd p1, p2;
	ANGLE_T a, a1, a2;
	DIST_T d, maxD, d1, d2;
	BOOL_T rc;
	coOrd off;
	DIST_T beyond;

	if ((GetTrkType(trk1) != T_TURNOUT) && (GetTrkType(trk2) != T_TURNOUT)) return FALSE;

	adj1 = adj2 = FALSE;

	if (GetTrkType(trk1) == T_TURNOUT) {
		xx1 = GET_EXTRA_DATA(trk1, T_TURNOUT, extraDataCompound_t);
		if (xx1->special == TOadjustable)
			adj1 = TRUE;
	}
	if (GetTrkType(trk2) == T_TURNOUT) {
		xx2 = GET_EXTRA_DATA(trk2, T_TURNOUT, extraDataCompound_t);
		if (xx2->special == TOadjustable)
			adj2 = TRUE;
	}
	if (adj1 == FALSE && adj2 == FALSE)
		return FALSE;
	a1 = GetTrkEndAngle(trk1, ep1);
	a2 = GetTrkEndAngle(trk2, ep2);
	a = NormalizeAngle(a1 - a2 + 180.0 + connectAngle / 2.0);
	if (a > connectAngle)
		return FALSE;
	UndoStart(_("Connect Adjustable Tracks"), "changeAdjustableEndPt");
	maxD = 0.0;
	if (adj1) {
		p1 = GetTrkEndPos(trk1, 1 - ep1);
		Translate(&p1, p1, a1, xx1->u.adjustable.minD);
		maxD += xx1->u.adjustable.maxD - xx1->u.adjustable.minD;
	}
	else {
		p1 = GetTrkEndPos(trk1, ep1);
	}
	if (adj2) {
		p2 = GetTrkEndPos(trk2, 1 - ep2);
		Translate(&p2, p2, a2, xx2->u.adjustable.minD);
		maxD += xx2->u.adjustable.maxD - xx2->u.adjustable.minD;
	}
	else {
		p2 = GetTrkEndPos(trk2, ep2);
	}
	d = FindDistance(p1, p2);
	rc = TRUE;
	if (d > maxD) {
		d = maxD;
		rc = FALSE;
	}
	FindPos(&off, &beyond, p1, p2, a1, DIST_INF);
	if (fabs(off.y) > connectDistance)
		rc = FALSE;
	if (adj1) {
		UndrawNewTrack(trk1);
		d1 = d * (xx1->u.adjustable.maxD - xx1->u.adjustable.minD) / maxD + xx1->u.adjustable.minD;
		ChangeAdjustableEndPt(trk1, ep1, d1);
	}
	if (adj2) {
		UndrawNewTrack(trk2);
		d2 = d * (xx2->u.adjustable.maxD - xx2->u.adjustable.minD) / maxD + xx2->u.adjustable.minD;
		ChangeAdjustableEndPt(trk2, ep2, d2);
	}
	if (rc) {
		DrawEndPt(&mainD, trk1, ep1, wDrawColorWhite);
		DrawEndPt(&mainD, trk2, ep2, wDrawColorWhite);
		ConnectTracks(trk1, ep1, trk2, ep2);
		DrawEndPt(&mainD, trk1, ep1, wDrawColorBlack);
		DrawEndPt(&mainD, trk2, ep2, wDrawColorBlack);
	}
	return rc;
}

/****************************************
 *
 * Draw Turnout Roadbed
 *
 */

int roadbedOnScreen = 0;


void DrawTurnoutRoadbedSide(drawCmd_p d, wDrawColor color, coOrd orig, ANGLE_T angle, trkSeg_p sp, ANGLE_T side, int first, int last)
{
	segProcData_t data;
	if (last <= first)
		return;
	data.drawRoadbedSide.first = first;
	data.drawRoadbedSide.last = last;
	data.drawRoadbedSide.side = side;
	data.drawRoadbedSide.roadbedWidth = roadbedWidth;
	data.drawRoadbedSide.rbw = (wDrawWidth)floor(roadbedLineWidth * (d->dpi / d->scale) + 0.5);
	data.drawRoadbedSide.orig = orig;
	data.drawRoadbedSide.angle = angle;
	data.drawRoadbedSide.color = color;
	data.drawRoadbedSide.d = d;
	SegProc(SEGPROC_DRAWROADBEDSIDE, sp, &data);
}


static void ComputeAndDrawTurnoutRoadbedSide(
	drawCmd_p d,
	wDrawColor color,
	coOrd orig,
	ANGLE_T angle,
	trkSeg_p segPtr,
	int segCnt,
	int segInx,
	ANGLE_T side)
{
	unsigned long res, res1;
	int b0, b1;
	res = ComputeTurnoutRoadbedSide(segPtr, segCnt, segInx, side, roadbedWidth);
	if (res == 0L) {
	}
	else if (res == 0xFFFFFFFF) {
		DrawTurnoutRoadbedSide(d, color, orig, angle, &segPtr[segInx], side, 0, 32);
	}
	else {
		for (b0 = 0, res1 = 0x00000001; res1 && (res1 & res); b0++, res1 <<= 1);
		for (b1 = 32, res1 = 0x80000000; res1 && (res1 & res); b1--, res1 >>= 1);
		DrawTurnoutRoadbedSide(d, color, orig, angle, &segPtr[segInx], side, 0, b0);
		DrawTurnoutRoadbedSide(d, color, orig, angle, &segPtr[segInx], side, b1, 32);
	}
}


static void DrawTurnoutRoadbed(
	drawCmd_p d,
	wDrawColor color,
	coOrd orig,
	ANGLE_T angle,
	trkSeg_p segPtr,
	int segCnt)
{
	int inx, trkCnt = 0, segInx = 0;
	for (inx = 0; inx < segCnt; inx++) {
		if (IsSegTrack(&segPtr[inx])) {
			segInx = inx;
			trkCnt++;
			if (trkCnt > 1)
				break;
		}
	}
	if (trkCnt == 0)
		return;
	if (trkCnt == 1) {
		DrawTurnoutRoadbedSide(d, color, orig, angle, &segPtr[segInx], +90, 0, 32);
		DrawTurnoutRoadbedSide(d, color, orig, angle, &segPtr[segInx], -90, 0, 32);
	}
	else {
		for (inx = 0; inx < segCnt; inx++) {
			if (IsSegTrack(&segPtr[inx])) {
				ComputeAndDrawTurnoutRoadbedSide(d, color, orig, angle, segPtr, segCnt, inx, +90);
				ComputeAndDrawTurnoutRoadbedSide(d, color, orig, angle, segPtr, segCnt, inx, -90);
			}
		}
	}
}

/****************************************
 *
 * HAND LAID TURNOUTS
 *
 */

track_p NewHandLaidTurnout(
	coOrd p0,
	ANGLE_T a0,
	coOrd p1,
	ANGLE_T a1,
	coOrd p2,
	ANGLE_T a2,
	ANGLE_T frogA)
{
	track_p trk;
	struct extraDataCompound_t* xx;
	trkSeg_t segs[2];
	sprintf(message, "\tHand Laid Turnout, Angle=%0.1f\t", frogA);
	DYNARR_SET(trkEndPt_t, tempEndPts_da, 2);
	memset(&tempEndPts(0), 0, tempEndPts_da.cnt * sizeof tempEndPts(0));
	tempEndPts(0).pos = p0;
	tempEndPts(0).angle = a0;
	tempEndPts(1).pos = p1;
	tempEndPts(1).angle = a1;
	tempEndPts(2).pos = p2;
	tempEndPts(2).angle = a2;
	Rotate(&p1, p0, -a0);
	p1.x -= p0.x;
	p1.y -= p0.y;
	segs[0].type = SEG_STRTRK;
	segs[0].color = wDrawColorBlack;
	segs[0].u.l.pos[0] = zero;
	segs[0].u.l.pos[1] = p1;
	Rotate(&p2, p0, -a0);
	p2.x -= p0.x;
	p2.y -= p0.y;
	segs[1].type = SEG_STRTRK;
	segs[1].color = wDrawColorBlack;
	segs[1].u.l.pos[0] = zero;
	segs[1].u.l.pos[1] = p2;
	trk = NewCompound(T_TURNOUT, 0, p0, a0, message, 3, &tempEndPts(0), NULL, (PATHPTR_T)"Normal\0\1\0\0Reverse\0\2\0\0\0", 2, segs);
	xx = GET_EXTRA_DATA(trk, T_TURNOUT, extraDataCompound_t);
	xx->handlaid = TRUE;

	return trk;
}

/****************************************
 *
 * GENERIC FUNCTIONS
 *
 */

static coOrd MapPathPos(
	struct extraDataCompound_t* xx,
	signed char segInx,
	EPINX_T ep)
{
	trkSeg_p segPtr;
	coOrd pos;

	if (segInx < 0) {
		segInx = -segInx;
		ep = 1 - ep;
	}

	segPtr = xx->segs + (segInx - 1);
	if (!IsSegTrack(segPtr)) {
		fprintf(stderr, "mapPathPos: bad segInx: %d\n", segInx);
		return zero;
	}
	pos = GetSegEndPt(segPtr, ep, FALSE, NULL);
	REORIGIN1(pos, xx->angle, xx->orig);
	return pos;

}

static trkSeg_p MapPathSeg(
	struct extraDataCompound_t* xx,
	signed char segInx) {

	if (segInx < 0) {
		segInx = -segInx;
	}
	return xx->segs + (segInx - 1);
}


/****************************************
 *
 * TURNOUT DRAWING
 *
 */

 /**
 * Get the paths from the turnout definition. Puts the results into static dto structure.
 *
 * \param trk track_p pointer to a track
 * \param xx  pointer to the extraDataCompound struct
 *
 * \returns the number of paths
 */
int GetTurnoutPaths(track_p trk, struct extraDataCompound_t* xx) {
	wIndex_t segInx;
	wIndex_t segEP;

	SCALEINX_T scaleInx = GetTrkScale(trk);
	tieData_p td = GetScaleTieData(scaleInx);

	int i;
	ANGLE_T a0, a1, aa0, aa1;
	DIST_T r, l;
	coOrd p0, p1;

	PATHPTR_T pp;
	int pathCnt = 0, routeCnt = 0;

	for (i = 0; i < DTO_DIM; i++)
		dto[i].n = 0;

	dtod.trk = trk;
	dtod.index = trk->index;
	dtod.xx = xx;

	// Validate that the first segment starts at (0, 0)
	// and if STR p1.y == 0, if CRV angle == 0 or angle == 180
	GetSegInxEP(1, &segInx, &segEP);
	trkSeg_p segPtr = &xx->segs[segInx];
	switch (segPtr->type) {
	case SEG_STRTRK:
		p0 = segPtr->u.l.pos[0];
		p1 = segPtr->u.l.pos[1];
		if ((FindDistance(p0, zero) > EPSILON) || (fabs(p1.y) > EPSILON))
			return -1;
		break;
	case SEG_CRVTRK:
		r = fabs(segPtr->u.c.radius);
		a0 = segPtr->u.c.a0;
		a1 = segPtr->u.c.a1;

		if (segPtr->u.c.radius > 0) {
			aa0 = a0;
		}
		else {
			aa0 = a0 + a1;
		}
		PointOnCircle(&p0, segPtr->u.c.center, r, aa0);
		if ((FindDistance(p0, zero) > EPSILON)
			|| ((fabs(aa0 - 180) > EPSILON) && (fabs(aa0) > EPSILON)))
			return -1;
		break;
	}

	pp = GetPaths(trk);
	while (pp[0]) {
		pp += strlen((char*)pp) + 1;

		ANGLE_T angle = 0;
		while (pp[0]) {
			if (pathCnt < DTO_DIM)
				dto[pathCnt].type = 'S';
			while (pp[0]) {
				GetSegInxEP(pp[0], &segInx, &segEP);
				// trkSeg_p 
				segPtr = &xx->segs[segInx];
				switch (segPtr->type) {
				case SEG_STRTRK:
					p0 = segPtr->u.l.pos[0];
					p1 = segPtr->u.l.pos[1];

					wIndex_t n = dto[pathCnt].n;
					dto[pathCnt].trkSeg[n] = segPtr;
					dto[pathCnt].base[n] = p0;
					n++;
					dto[pathCnt].trkSeg[n] = segPtr;
					dto[pathCnt].base[n] = p1;
					// n++;
					dto[pathCnt].n = n;

					if (n >= DTO_SEGS - 1) return -1;

					break;
				case SEG_CRVTRK:
					r = fabs(segPtr->u.c.radius);

					dto[pathCnt].type = segPtr->u.c.radius > 0 ? 'R' : 'L';

					a0 = segPtr->u.c.a0;
					a1 = segPtr->u.c.a1;

					angle += a1;

					l = D2R(a1) * r;
					// Every 5 degrees or 5 * tie width
					int cnt = (int)floor(a1 / 5.0);
					int cnt2 = (int)floor(l / 5 / td->spacing);
					if (cnt2 > cnt) cnt = cnt2;

					aa1 = a1 / cnt;
					if (segPtr->u.c.radius > 0) {
						aa0 = a0;
					}
					else {
						aa0 = a0 + a1;
						aa1 = -aa1;
					}
					PointOnCircle(&p0, segPtr->u.c.center, r, aa0);
					n = dto[pathCnt].n;
					dto[pathCnt].trkSeg[n] = segPtr;
					dto[pathCnt].base[n] = p0;
					n++;
					dto[pathCnt].n = n;

					// cnt--; 
					while (cnt > 0) {
						aa0 += aa1;
						PointOnCircle(&p0, segPtr->u.c.center, r, aa0);

						// n = dto[pathCnt].n;
						dto[pathCnt].trkSeg[n] = segPtr;
						dto[pathCnt].base[n] = p0;
						n++;
						// dto[pathCnt].n = n;

						if (n >= DTO_SEGS - 1) return -1;

						cnt--;
					}
					n--; // remove that last point count
					dto[pathCnt].n = n;
				}
				pp++;
			}
			// Include the last point
			dto[pathCnt].crvAngle = angle;
			dto[pathCnt].n++;

			pathCnt++;
			if (pathCnt >= DTO_DIM) return -1;
			pp++;
		}
		routeCnt++;
		pp++;
	}
	dtod.pathCnt = pathCnt;
	dtod.routeCnt = routeCnt;
	dtod.endCnt = trk->endCnt;

	// Guard value: n < DTO_SEGS - 2
	for (i = 0; i < pathCnt; i++)
		dto[i].pts[dto[i].n].x = DIST_INF;

	return pathCnt;
}

/**
* Sets the turnout type if compatible with enhanced drawing methods. The data is
* from the path data saved in dtod and dto by GetTurnoutPaths. The turnout type is
* stored in the dtod.toType. DTO_INVALID (0) if the enhanced methods cannot handle
* it.
*/
void GetTurnoutType() {
	dtod.strPath = -1;
	dtod.str2Path = -1;
	dtod.crvPath = -1;
	dtod.crv2Path = -1;

	dtod.toType = DTO_INVALID;

	int strCnt = 0, crvCnt = 0, lftCnt = 0, rgtCnt = 0;
	int toType = 0;
	int i, j;

	// Count path origins
	dtod.origCnt = 1;
	dtod.origins[0] = 0;

	for (i = 1; i < dtod.pathCnt; i++) {
		int eq = 0;
		for (j = 0; j < i; j++) {
			if (CoOrdEqual(dto[dtod.origins[j]].base[0], dto[i].base[0]))
				eq++;
		}
		if (eq == 0) {
			dtod.origins[dtod.origCnt] = i;
			dtod.origCnt++;
		}

		if (dtod.origCnt > 4)
			return;
	}

	// Determine the path type
	for (i = 0; i < dtod.pathCnt; i++) {
		switch (dto[i].type) {
		case 'S':
			strCnt++;
			if (strCnt == 1)
				dtod.strPath = i;
			else
				dtod.str2Path = i;
			break;
		case 'L':
			lftCnt++;
			crvCnt++;
			if (crvCnt == 1)
				dtod.crvPath = i;
			else
				dtod.crv2Path = i;
			break;
		case 'R':
			rgtCnt++;
			crvCnt++;
			if (crvCnt == 1)
				dtod.crvPath = i;
			else
				dtod.crv2Path = i;
			break;
		}
	}

	dtod.strCnt = strCnt;
	dtod.crvCnt = crvCnt;
	dtod.lftCnt = lftCnt;
	dtod.rgtCnt = rgtCnt;

	// Normal two- or three-way turnout, or a curved turnout
	if (dtod.origCnt == 1) {
		// Make sure each path ends in a straight segment
		/*
		for (i = 0; i < dtod.pathCnt; i++) {
			int n = dto[i].n - 1;
			if (dto[i].trkSeg[n]->type == SEG_CRVTRK) {
				return;
			}
		}
		*/
		if (dtod.pathCnt == 2) {
			if (strCnt == 1 && crvCnt == 1) {
				dtod.toType = DTO_NORMAL;
			}
			else if ((strCnt == 0) && ((lftCnt == 2) || (rgtCnt == 2))) {
				if ((dto[0].crvAngle <= 20) && (dto[1].crvAngle - dto[0].crvAngle <= 15))
					dtod.toType = DTO_CURVED;
			}
			else if (lftCnt == 1 && rgtCnt == 1) {
				dtod.toType = DTO_WYE;
			}
		}
		else if ((dtod.pathCnt == 3) && (strCnt == 1)
			&& (lftCnt == 1) && (rgtCnt == 1)) {
			dtod.toType = DTO_THREE;
		}
	}
	else
		// Crossing, single- and double-slip
		if ((dtod.origCnt == 2) && (dtod.endCnt == 4)
			&& strCnt == 2) {

			ANGLE_T a0, a1, a2;
			a1 = FindAngle(dto[dtod.strPath].base[0], dto[dtod.strPath].base[1]);
			a2 = FindAngle(dto[dtod.str2Path].base[0], dto[dtod.str2Path].base[1]);
			a0 = DifferenceBetweenAngles(a1, a2);
			coOrd p1 = dto[dtod.strPath].base[0];
			coOrd p2 = dto[dtod.str2Path].base[0];
			coOrd pos = zero;
			int intersect = FindIntersection(&pos, p1, a1, p2, a2);

			if (intersect) {
				if (strCnt == 2 && dtod.pathCnt == 2)
					if (a0 <= 30)
						dtod.toType = DTO_XING;
					else
						dtod.toType = DTO_XNG9;
				else if (dtod.pathCnt == 3 && (lftCnt == 1 || rgtCnt == 1))
					dtod.toType = DTO_SSLIP;
				else if (dtod.pathCnt == 4 && lftCnt == 1 && rgtCnt == 1)
					dtod.toType = DTO_DSLIP;
			}
			// No intersect, it could be a crossover
			else if (strCnt == 2) {
				if (dtod.pathCnt == 4 && lftCnt == 1 && rgtCnt == 1) {
					dtod.toType = DTO_DCROSS;
				}
				else if (dtod.pathCnt == 3)
					// Perverse test because the cross paths go Left then Right, for example
					if (lftCnt == 1)
						dtod.toType = DTO_RCROSS;
					else if (rgtCnt == 1)
						dtod.toType = DTO_LCROSS;
			}
		}
}

/**
 * Draw Layout lines and points
 * 
 * \param d The drawing object
 * \param scaleInx The layout/track scale index
 */
static void DrawDtoLayout(
	drawCmd_p d,
	SCALEINX_T scaleInx
)
{
	tieData_p td;
	td = GetScaleTieData(scaleInx);

	// Draw the points and lines from dto
	double r = td->width / 2;
	// if (r < 1) r = 1;

	int i, j;
	for (i = 0; i < DTO_DIM; i++) {
		for (j = 0; j < dto[i].n; j++) {
			DrawFillCircle(d, dto[i].pts[j], r, drawColorPurple);
			if (j < dto[i].n - 1)
				DrawLine(d, dto[i].pts[j], dto[i].pts[j + 1], 0, drawColorPurple);
		}
	}
}

/**
* Use the coOrds to build a polygon and draw the bridge fill. Note that the coordinates are 
* passed as pairs, and rearranged into a polygon with the 1,2,4,3 order. 
* 
* \param d The drawing object
* \param b1 The first coordinate
* \param b2 The second coordinate
* \param b3 The third coordinate
* \param b4 The fourth coordinate
*/
static void DrawBridgeFill(
	drawCmd_p d, 
	coOrd b1,
	coOrd b2,
	coOrd b3,
	coOrd b4
	) 
{
	coOrd p[4] = {b1, b2, b4, b3};
	DrawPoly(d,4,p,NULL,drawColorGrey90,0,DRAW_FILL ); 
}

/**
* Draw Bridge parapets and background for a turnout
* 
* \param d The drawing object
* \param path1 The first path
* \param path2 The second path
*/
static void DrawTurnoutBridge(
    drawCmd_p d,
    int path1,
    int path2
)
{
    DIST_T trackGauge = GetTrkGauge(dtod.trk);
    wDrawWidth width2 = (wDrawWidth)round((2.0 * d->dpi) / BASE_DPI);

    coOrd b1,b2,b3,b4,b5,b6;
    ANGLE_T angle = dtod.xx->angle,a = 0.0;
    int i,j,i1,i2;
    i1 = path1;
    i2 = path2;
    if(dto[i1].base[dto[i1].n - 1].y < dto[i2].base[dto[i2].n - 1].y) {
        i1 = path2;
        i2 = path1;
        // a = -a;
    }

    if(dtod.toType == DTO_THREE) {
        i = dtod.strPath;
        DIST_T dy = fabs(dto[i].dy[0]) + trackGauge * 1.5;
        b1 = dto[i].pts[0];
        Translate(&b3,b1,(angle + a),dy);
        b1 = dto[i].pts[dto[i].n - 1];
        Translate(&b4,b1,(angle + a),dy);
        b2 = dto[i].pts[0];
        Translate(&b5,b2,(angle + a),-dy);
        b2 = dto[i].pts[dto[i].n - 1];
        Translate(&b6,b2,(angle + a),-dy);

        // Draw the bridge background
        DrawBridgeFill(d,b3,b4,b5,b6);
    }

    for(i = i1; 1; i = i2,a = 180.0) {
        DIST_T dy = fabs(dto[i].dy[0]) + trackGauge * 1.5;
        b1 = dto[i].pts[0];
        Translate(&b3,b1,(angle + a),dy);
        Translate(&b5,b1,(angle + a),-(dy * 0.75));
        for(j = 1; j < dto[i].n; j++) {
            dy = fabs(dto[i].dy[j]) + trackGauge * 1.5;
            b2 = dto[i].pts[j];
            Translate(&b4,b2,(angle + a),dy);
            Translate(&b6,b2,(angle + a),-(dy * 0.75));

            // Draw the bridge background
            DrawBridgeFill(d,b3,b4,b5,b6);

            // Draw the bridge edge
            DrawLine(d,b3,b4,width2,drawColorBlack);

            b1 = b2;
            b3 = b4;
            b5 = b6;
        }

        if(i == i2)
            break;
    }

    EPINX_T ep;
    coOrd p;
    track_p trk1;
    coOrd p0,p1;

    for(ep = 0; ep < 3; ep++) {
        trk1 = GetTrkEndTrk(dtod.trk,ep);

        if((trk1) && (!GetTrkBridge(trk1))) {

            p = GetTrkEndPos(dtod.trk,ep);
            a = GetTrkEndAngle(dtod.trk,ep) + 90.0;

            int i = (dtod.lftCnt > 0) && (dtod.rgtCnt == 0) ? 2 : 1;
            if(ep != i) {
                Translate(&p0,p,a,trackGauge * 1.5);
                Translate(&p1,p0,a - 45.0,trackGauge * 1.5);
                DrawLine(d,p0,p1,width2,drawColorBlack);
            }
            if(ep != (3 - i)) {
                Translate(&p0,p,a,-trackGauge * 1.5);
                Translate(&p1,p0,a + 45.0,-trackGauge * 1.5);
                DrawLine(d,p0,p1,width2,drawColorBlack);
            }
        }
    }
}

/**
* Draw Bridge parapets and background for a cross-over
* 
* \param d The drawing object
* \param path1 The first path, straight
* \param path2 The second path, straight
*/
static void DrawCrossBridge(
	drawCmd_p d, 
	int path1,
	int path2
)
{
	DIST_T trackGauge = GetTrkGauge(dtod.trk);
	wDrawWidth width2 = (wDrawWidth)round((2.0 * d->dpi)/BASE_DPI);

	coOrd b1, b2, b3, b4, b5, b6;
	ANGLE_T angle = dtod.xx->angle, a = 0.0;
	int i1, i2;
	i1 = path1;
	i2 = path2;
	if(dto[i1].base[dto[i1].n - 1].y < dto[i2].base[dto[i2].n - 1].y) {
		i1 = path2;
		i2 = path1;
		// a = -a;
	}

	DIST_T dy = fabs(dto[i1].dy[0]) + trackGauge * 1.5;
	b1 = dto[i1].pts[0];
	Translate(&b3,b1,(angle + a),dy);
	b1 = dto[i1].pts[dto[i1].n-1];
	Translate(&b4,b1,(angle + a),dy);
	b2 = dto[i2].pts[0];
	Translate(&b5,b2,(angle + a),-dy);
	b2 = dto[i2].pts[dto[i2].n-1];
	Translate(&b6,b2,(angle + a),-dy);

	// Draw the bridge background
	DrawBridgeFill(d, b3, b4, b5, b6);

	// Draw the bridge edges
	DrawLine(d,b3,b4,width2,drawColorBlack);
	DrawLine(d,b5,b6,width2,drawColorBlack);

    EPINX_T ep;
    coOrd p;
    track_p trk1;
    coOrd p0,p1;

    for(ep = 0; ep < 4; ep++) {
        trk1 = GetTrkEndTrk(dtod.trk,ep);

        if((trk1) && (!GetTrkBridge(trk1))) {
            p = GetTrkEndPos(dtod.trk,ep);
            a = GetTrkEndAngle(dtod.trk,ep) + 90.0;

            if((ep == 1) || (ep == 2)) {
                Translate(&p0,p,a,trackGauge * 1.5);
                Translate(&p1,p0,a - 45.0,trackGauge * 1.5);
                DrawLine(d,p0,p1,width2,drawColorBlack);
            }
            if((ep == 0) || (ep == 3)) {
                Translate(&p0,p,a,-trackGauge * 1.5);
                Translate(&p1,p0,a + 45.0,-trackGauge * 1.5);
                DrawLine(d,p0,p1,width2,drawColorBlack);
            }
        }
    }
}

/**
* Draw Bridge parapets and background for a crossing
* 
* \param d The drawing object
* \param path1 The first path
* \param path2 The second path
*/
static void DrawXingBridge(
	drawCmd_p d, 
	int path1,
	int path2
)
{
	DIST_T trackGauge = GetTrkGauge(dtod.trk);
	wDrawWidth width2 = (wDrawWidth)round((2.0 * d->dpi)/BASE_DPI);

	coOrd b0, b1, b2, b3, b4, b5, b6;
	int i, j, i1, i2;
	i1 = dtod.strPath;
	i2 = dtod.str2Path;

	// Bridge fill both straight sections
	wDrawWidth width3 = (wDrawWidth)round(trackGauge * 3 * d->dpi/d->scale); 
	b1 = dto[i1].pts[0];
	b2 = dto[i1].pts[dto[i1].n-1];
	DrawLine(d,b1,b2,width3,wDrawColorGrey90);
	b1 = dto[i2].pts[0];
	b2 = dto[i2].pts[dto[i1].n-1];
	DrawLine(d,b1,b2,width3,wDrawColorGrey90);

	i1 = path1;
	i2 = path2;
	if(dto[i1].base[dto[i1].n - 1].y < dto[i2].base[dto[i2].n - 1].y) {
		i1 = path2;
		i2 = path1;
	}

	// Handle curved sections for slips
	BOOL_T hasLeft = 0, hasRgt = 0;
	ANGLE_T angle = dtod.xx->angle, a = 0.0;
	for(i = i1; 1; i = i2,a = 180.0) {
		DIST_T dy = fabs(dto[i].dy[0]) + trackGauge * 1.5;
		b1 = dto[i].pts[0];
		Translate(&b3,b1,(angle + a),dy);
		Translate(&b5,b1,(angle + a),-(dy * 0.75));
		if(dto[i].type != 'S') {
			if(dto[i].type == 'L')
				hasLeft = 1;
			else if(dto[i].type == 'R')
				hasRgt = 1;
			for(j = 1; j < dto[i].n; j++) {
				dy = fabs(dto[i].dy[j]) + trackGauge * 1.5;
				b2 = dto[i].pts[j];
				Translate(&b4,b2,(angle + a),dy);
				Translate(&b6,b2,(angle + a),-(dy * 0.75));

				// Draw the bridge background
				DrawBridgeFill(d,b3,b4,b5,b6);

				// Draw the bridge edge
				DrawLine(d,b3,b4,width2,drawColorBlack);
				b1 = b2;
				b3 = b4;
				b5 = b6;
			}
		}
		if(i == i2)
			break;
	}

	if(dtod.strPath >= 0 && dtod.str2Path >= 0) {
		i1 = dtod.strPath;
		i2 = dtod.str2Path;
		if(!hasRgt) {
			DIST_T dy = trackGauge * 1.5;
			ANGLE_T a1, a2;
			b1 = dto[i1].pts[0];
            a1 = dto[i1].angle + 90;
			Translate(&b3,b1,a1,dy);

			b2 = dto[i2].pts[dto[i2].n - 1];
            a2 = dto[i2].angle + 90;
			Translate(&b4,b2,a2,dy);

			FindIntersection(&b0, b3, a1-90.0, b4, a2-90.0);

			// Draw the bridge edge
			DrawLine(d,b3,b0,width2,drawColorBlack);
			DrawLine(d,b0,b4,width2,drawColorBlack);
		}

		if(!hasLeft) {
			DIST_T dy = trackGauge * 1.5;
			ANGLE_T a1, a2;
			b1 = dto[i2].pts[0];
			a1 = dto[i2].angle - 90;
			Translate(&b3,b1,a1,dy);

			b2 = dto[i1].pts[dto[i1].n - 1];
			a2 = dto[i1].angle - 90;
			Translate(&b4,b2,a2,dy);

			FindIntersection(&b0, b3, a1+90.0, b4, a2+90.0);

			// Draw the bridge edge
			DrawLine(d,b3,b0,width2,drawColorBlack);
			DrawLine(d,b0,b4,width2,drawColorBlack);
		}

		if(dtod.toType == DTO_XNG9) {
			DIST_T dy = trackGauge * 1.5;
			ANGLE_T a1, a2;
			b1 = dto[i1].pts[dto[i1].n - 1];
			a1 = dto[i1].angle + 90;
			Translate(&b3,b1,a1,dy);

			b2 = dto[i2].pts[dto[i2].n - 1];
			a2 = dto[i2].angle - 90;
			Translate(&b4,b2,a2,dy);

			FindIntersection(&b0, b3, a1-90.0, b4, a2+90.0);

			// Draw the bridge edge
			DrawLine(d,b3,b0,width2,drawColorBlack);
			DrawLine(d,b0,b4,width2,drawColorBlack);

			b1 = dto[i1].pts[0];
			a1 = dto[i1].angle - 90;
			Translate(&b3,b1,a1,dy);

			b2 = dto[i2].pts[0];
			a2 = dto[i2].angle + 90;
			Translate(&b4,b2,a2,dy);

			FindIntersection(&b0, b3, a1+90.0, b4, a2-90.0);

			// Draw the bridge edge
			DrawLine(d,b3,b0,width2,drawColorBlack);
			DrawLine(d,b0,b4,width2,drawColorBlack);
		}
	}

    // Bridge wings
    EPINX_T ep;
    coOrd p;
    track_p trk1;
    coOrd p0,p1;

    for(ep = 0; ep < 4; ep++) {
        trk1 = GetTrkEndTrk(dtod.trk,ep);

        if((trk1) && (!GetTrkBridge(trk1))) {
            p = GetTrkEndPos(dtod.trk,ep);
            a = GetTrkEndAngle(dtod.trk,ep) + 90.0;

            if((dtod.toType == DTO_XNG9) || (ep == 2) || (ep == 3)) {
                Translate(&p0,p,a,trackGauge * 1.5);
                Translate(&p1,p0,a - 45.0,trackGauge * 1.5);
                DrawLine(d,p0,p1,width2,drawColorBlack);
            }
            if((dtod.toType == DTO_XNG9) || (ep == 0) || (ep == 1)) {
                Translate(&p0,p,a,-trackGauge * 1.5);
                Translate(&p1,p0,a + 45.0,-trackGauge * 1.5);
                DrawLine(d,p0,p1,width2,drawColorBlack);
            }
        }
    }
}

/**
 * Draw Normal (Single Origin) Turnout Bridge and Ties. Uses the static dto and dtod structures.
 *
 * \param d The drawing object
 * \param scaleInx The layout/track scale index
 * \param color The tie color. If black the color is read from the global tieColor.
 */
static void DrawNormalTurnout(
	drawCmd_p d,
	SCALEINX_T scaleInx,
	BOOL_T omitTies,
	wDrawColor color)
{
	tieData_p td;
	DIST_T len;
	coOrd pos;
	int cnt;
	ANGLE_T angle;
	coOrd s1, s2, p1, p2, q1, q2;
	int s0, p0, q0;
	ANGLE_T a0;

	// coOrd b1, b2, b3, b4, bb1, bb2, bb3, bb4; // bridge
	// DIST_T blen1, blen2;

	if (color == wDrawColorBlack)
		color = tieColor;

	DIST_T trackGauge = GetTrkGauge(dtod.trk);
	struct extraDataCompound_t* xx = dtod.xx;

	int i, j;
	for (i = 0; i < DTO_DIM; i++)
		for (j = 0; j < dto[i].n; j++) {
			REORIGIN(p1, dto[i].base[j], xx->angle, xx->orig);
			dto[i].pts[j] = p1;
			if (j < dto[i].n - 1)
				dto[i].dy[j] = (dto[i].base[j + 1].y - dto[i].base[j].y) / (dto[i].base[j + 1].x - dto[i].base[j].x);
		}

	// draw the points
#ifdef DTO_DEBUG
	if (DTO_DEBUG == DTO_NORMAL) DrawDtoLayout(d, scaleInx);
#endif

	int strPath = dtod.strPath, othPath = 0, secPath = 1;
	int toType = dtod.toType;
	int first = 1;

	switch (toType) {
	case DTO_NORMAL:
		othPath = 1 - strPath;
		secPath = strPath;
		break;
	case DTO_WYE:
		// strPath = 2;
		othPath = 0; secPath = 1;
		break;
	case DTO_THREE:
		switch (strPath) {
		case 0:
			othPath = 1; secPath = 2;
			break;
		case 1:
			othPath = 0; secPath = 2;
			break;
		case 2:
			othPath = 0; secPath = 1;
			break;
		}
		break;
	}

	if(dtod.bridge) {
		DrawTurnoutBridge(d,othPath,secPath);
	}
	if (omitTies)
		return;

	// Straight vector for tie angle
	if (toType == DTO_WYE) {
		s1 = dto[othPath].pts[0];
		s2 = MidPtCoOrd(dto[othPath].pts[dto[othPath].n - 1], dto[secPath].pts[dto[secPath].n - 1]);
	}
	else {
		s1 = dto[strPath].pts[0];
		s2 = dto[strPath].pts[dto[strPath].n - 1];
	}
	// Diverging vector(s)
	p1 = dto[othPath].pts[0];
	p2 = dto[othPath].pts[dto[othPath].n - 1];
	q1 = dto[secPath].pts[0];
	q2 = dto[secPath].pts[dto[secPath].n - 1];

	td = GetScaleTieData(scaleInx);
	len = FindDistance(s1, s2);
	angle = FindAngle(s1, s2); // The straight segment

	cnt = (int)floor(len / td->spacing + 0.5);
	if (cnt > 0) {
		int pn = dto[othPath].n;
		int qn = dto[secPath].n;
		DIST_T dx = len / cnt;
		s0 = p0 = q0 = 0;
		DIST_T tdlen = td->length;
		DIST_T tdmax = (toType == DTO_WYE) ? 2.0 * tdlen : 2.5 * tdlen;
		DIST_T px = len, dlenx = dx / 2;

		cnt = cnt > 1 ? cnt - 1 : 1;
		for (px = dlenx; cnt; cnt--, px += dx) {
			if (px >= dto[othPath].base[p0 + 1].x) p0++;
			if (px >= dto[secPath].base[q0 + 1].x) q0++;
			if (p0 >= pn || q0 >= qn)
				break;

			if ((px + dx >= dto[othPath].base[pn - 1].x) || (px + dx >= dto[secPath].base[qn - 1].x)) {
				break;
			}

			DIST_T dy1 = dto[othPath].base[p0].y + (px - dto[othPath].base[p0].x) * dto[othPath].dy[p0];
			DIST_T dy2 = dto[secPath].base[q0].y + (px - dto[secPath].base[q0].x) * dto[secPath].dy[q0];
			tdlen = td->length + fabs(dy1) + fabs(dy2);
			if (tdlen > tdmax)
				break;

			DIST_T dy = dy1 + dy2;
			Translate(&pos, s1, angle, px);
			Translate(&pos, pos, (angle - 90.0), dy / 2);

			DrawTie(d, pos, angle, tdlen, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);
		}

		// Draw remaining ties, if any
		if (px + dx < dto[othPath].base[pn - 1].x) {
			p1 = dto[othPath].pts[p0];
			p2 = dto[othPath].pts[pn - 1];
			angle = FindAngle(p1, p2);
			a0 = FindAngle(dto[othPath].base[p0], dto[othPath].base[pn - 1]);
			DIST_T lenr = (dto[othPath].base[pn - 1].x - px + dlenx) / cos(D2R(90.0 - a0));
			Translate(&p1, p2, angle, -lenr);
			DrawStraightTies(d, scaleInx, p1, p2, color);
		}
		else {
			p1 = dto[othPath].pts[pn - 2];
			a0 = FindAngle(p1, p2);
			Translate(&pos, p2, a0, -dx / 2);
			DrawTie(d, pos, a0, td->length, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);
		}

		if (px + dx < dto[secPath].base[qn - 1].x) {
			q1 = dto[secPath].pts[q0];
			q2 = dto[secPath].pts[qn - 1];
			angle = FindAngle(q1, q2);
			a0 = FindAngle(dto[secPath].base[q0], dto[secPath].base[qn - 1]);
			DIST_T lenr = (dto[secPath].base[qn - 1].x - px + dlenx) / cos(D2R(90.0 - a0));
			Translate(&q1, q2, angle, -lenr);
			DrawStraightTies(d, scaleInx, q1, q2, color);
		}
		else {
			q1 = dto[secPath].pts[qn - 2];
			a0 = FindAngle(q1, q2);
			Translate(&pos, q2, a0, -dx / 2);
			DrawTie(d, pos, a0, td->length, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);
		}

		// Final ties at end
		if (dtod.toType == DTO_THREE) {

			int n = (int)(dto[strPath].base[qn - 1].x);
			if (px + dx < len) {
				angle = FindAngle(s1, s2);
				DIST_T lenr = len - px + dlenx;
				Translate(&s1, s2, angle, -lenr);
				DrawStraightTies(d, scaleInx, s1, s2, color);
			}
			else {
				n = dto[strPath].n;
				s1 = dto[strPath].pts[n - 2];
				a0 = FindAngle(s1, s2);
				Translate(&pos, s2, a0, -dx / 2);
				DrawTie(d, pos, a0, td->length, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);
			}
		}
	}
}

/**
 * Draw Curved (Single Origin) Turnout Bridge and Ties. Uses the static dto and dtod structures.
 *
 * \param d The drawing object
 * \param scaleInx The layout/track scale index
 * \param color The tie color. If black the color is read from the global tieColor.
 */
static void DrawCurvedTurnout(
	drawCmd_p d,
	SCALEINX_T scaleInx,
	BOOL_T omitTies,
	wDrawColor color)
{
	tieData_p td;
	DIST_T len, r;
	coOrd pos;
	int cnt;
	ANGLE_T angle, dang;
	coOrd center;
	coOrd p1, p2, q1, q2;
	ANGLE_T a0, a1, a2;

	if (color == wDrawColorBlack)
		color = tieColor;

	struct extraDataCompound_t* xx = dtod.xx;

	int i, j;
	for (i = 0; i < DTO_DIM; i++)
		for (j = 0; j < dto[i].n; j++) {
			REORIGIN(p1, dto[i].base[j], xx->angle, xx->orig);
			dto[i].pts[j] = p1;
			if (j < dto[i].n - 1)
				dto[i].dy[j] = (dto[i].base[j + 1].y - dto[i].base[j].y) / (dto[i].base[j + 1].x - dto[i].base[j].x);
		}

	// draw the points
#ifdef DTO_DEBUG
	if (DTO_DEBUG == DTO_CURVED) DrawDtoLayout(d, scaleInx);
#endif

	int othPath = 0, secPath = 1;
	int toType = dtod.toType;

	if(dtod.bridge) {
		DrawTurnoutBridge(d,othPath,secPath);
	}
	if(omitTies)
		return;

	td = GetScaleTieData(scaleInx);

	// Save the ending coordinates
	coOrd othEnd = zero, secEnd = zero;

	trkSeg_p trk;
	DIST_T tdlen = td->length, tdmax = tdlen * 2.5;
	DIST_T tdspc = td->spacing, tdspc2 = tdspc / 2.0;
	double rdot = td->width / 2;

	int pn = dto[othPath].n;
	int qn = dto[secPath].n;
	int p0 = 0, q0 = 0;
	DIST_T px = 0, qx = 0, dy = 0, dy1 = 0, dy2 = 0;

	double cosAdj = 1.0;

	angle = 0;
	px = tdspc2;
	qx = tdspc2;
	int segs = max(dto[othPath].n, dto[secPath].n);
	for (; segs > 0; segs--) {

		if (px >= dto[othPath].base[p0 + 1].x)
			p0++;
		if (qx >= dto[secPath].base[q0 + 1].x)
			q0++;
		if ((p0 >= pn - 1) || (q0 >= qn - 1)) {
			break;
		}

		trk = dto[othPath].trkSeg[p0];
		if (trk->type == SEG_CRVTRK) {

			center = trk->u.c.center;
			r = fabs(trk->u.c.radius);
			a0 = NormalizeAngle(trk->u.c.a0 + dtod.xx->angle);
			a1 = trk->u.c.a1;

			pos = center;
			REORIGIN(center, pos, xx->angle, xx->orig);

			len = r * D2R(a1);
			cnt = (int)floor(len / tdspc + 0.5);
			if (len - tdspc * cnt > tdspc2) {
				cnt++;
			}
			DIST_T tdlen = td->length;
			DIST_T dx = len / cnt, dx2 = dx / 2;

			if (cnt != 0) {
				dang = (len / cnt) * 360 / (2 * M_PI * r);
				DIST_T dx = len / cnt, dx2 = dx / 2;

				if (dto[othPath].type == 'R') {
					a2 = a0 + dang / 2;
				}
				else {
					a2 = a0 + a1 - dang / 2;
					dang = -dang;
				}
				angle += fabs(dang / 2);

				cosAdj = fabs(cos(D2R(angle)));
				px += dx2 * cosAdj;
				qx += dx2 * cosAdj;

				for (; cnt; cnt--, a2 += dang, angle += dang) {
					if (px >= dto[othPath].base[p0 + 1].x)
						p0++;
					if (qx >= dto[secPath].base[q0 + 1].x)
						q0++;
					if ((p0 >= pn - 1) || (q0 >= qn - 1)) {
						break;
					}

					coOrd e1, e2;
					PointOnCircle(&e1, center, r, a2);

					q1 = dto[secPath].pts[q0];
					q2 = dto[secPath].pts[q0 + 1];
					FindIntersection(&e2, e1, a2, q1, FindAngle(q1, q2));

					dy = FindDistance(e1, e2);
					DIST_T tlen = tdlen + dy;

					if (tlen > tdmax) {
						break;
					}

					Translate(&pos, e1, a2, -dy / 2);
					DrawTie(d, pos, angle + xx->angle + 90, tlen, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);

					// Assures that these ends are the last point drawn before break
					othEnd = e1;
					secEnd = e2;

					cosAdj = fabs(cos(D2R(angle)));
					if (cnt > 1) {
						px += dx * cosAdj;
						qx += dx * cosAdj;
					}
					else {
						px += dx2 * cosAdj;
						qx += dx2 * cosAdj;
					}
				}
			}
		}
		else {
			cosAdj = fabs(cos(D2R(angle)));

			p1 = dto[othPath].base[p0];
			p2 = dto[othPath].base[p0 + 1];
			len = FindDistance(p1, p2);
			cnt = (int)floor(len / tdspc + 0.5);
			if (cnt > 0) {
				DIST_T dx = len / cnt, dx2 = dx / 2;

				for (; cnt; cnt--) {
					if (px >= dto[othPath].base[p0 + 1].x)
						p0++;
					if (qx >= dto[secPath].base[q0 + 1].x)
						q0++;
					if ((p0 >= pn - 1) || (q0 >= qn - 1)) {
						break;
					}

					p1 = dto[othPath].base[p0];
					p2 = dto[othPath].base[p0 + 1];

					if ((px >= dto[othPath].base[pn - 1].x)
						|| (qx >= dto[secPath].base[qn - 1].x)) {
						break;
					}

					dy1 = dto[secPath].base[q0].y + (qx - dto[secPath].base[q0].x) * dto[secPath].dy[q0];
					dy2 = dto[othPath].base[p0].y + (px - dto[othPath].base[p0].x) * dto[othPath].dy[p0];
					dy = dy1 - dy2;
					DIST_T tlen = tdlen + fabs(cosAdj * dy);
					if (tlen > tdmax) {
						break;
					}

					q1 = dto[secPath].pts[q0];
					q2 = dto[secPath].pts[q0 + 1];
					a1 = FindAngle(q1, q2);
					DIST_T xlen = qx - dto[secPath].base[q0].x;
					Translate(&pos, q1, a1, xlen);
					secEnd = pos;

					q1 = dto[othPath].pts[p0];
					q2 = dto[othPath].pts[p0 + 1];
					a1 = FindAngle(q1, q2);
					xlen = px - dto[othPath].base[p0].x;
					Translate(&pos, q1, a1, xlen);
					othEnd = pos;
					Translate(&pos, pos, (a1 - 90.0), dy / 2);
					DrawTie(d, pos, a1, tlen, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);

					cosAdj = fabs(cos(D2R(angle)));
					px += dx * cosAdj;
					qx += dx * cosAdj;
				}
			}
			else {
				break;
			}
		}
	}

#ifdef DTO_DEBUG
	if (DTO_DEBUG == DTO_CURVED) {
		DrawFillCircle(d, othEnd, rdot, drawColorGreen);
		DrawFillCircle(d, secEnd, rdot, drawColorGreen);

		DrawFillCircle(d, dto[othPath].pts[p0], rdot, drawColorBlue);
		DrawFillCircle(d, dto[secPath].pts[q0], rdot, drawColorBlue);
	}
#endif

	// Draw remaining ties, if any
	p1 = othEnd;
	p2 = dto[othPath].pts[pn - 1];
	a0 = FindAngle(p1, p2);
	len = FindDistance(p1, p2);
	if (len >= 2 * tdspc) {
		Translate(&p1, p1, a0, tdspc2);
		//Translate(&p2, p2, a0, -tdspc2);
		DrawStraightTies(d, scaleInx, p1, p2, color);
	}
	else if (len > tdspc2) { 
		Translate(&p2, p2, a0, -tdspc2);
		DrawTie(d, p2, a0, td->length, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);
	}

	q1 = secEnd;
	q2 = dto[secPath].pts[qn - 1];
	a0 = FindAngle(q1, q2);
	len = FindDistance(q1, q2);
	if (len >= 2 * tdspc) {
		Translate(&q1, q1, a0, tdspc2);
		//Translate(&q2, q2, a0, -tdspc2);
		DrawStraightTies(d, scaleInx, q1, q2, color);
	}
	else if (len > tdspc2) {
		Translate(&q2, q2, a0, -tdspc2);
		DrawTie(d, q2, a0, td->length, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);
	}
}

/**
  * Draw Crossing and Slip Turnout Bridge and Ties - Uses the static dto and dtod structures.
  *
  * \param d The drawing object
  * \param scaleInx The layout/track scale index
  * \param color The tie color. If black the color is read from the global tieColor.
  */
static void DrawXingTurnout(
	drawCmd_p d,
	SCALEINX_T scaleInx,
	BOOL_T omitTies,
	wDrawColor color)
{
	tieData_p td;
	DIST_T len;
	coOrd pos;
	int cnt;
	ANGLE_T cAngle;

	if (color == wDrawColorBlack)
		color = tieColor;

	coOrd c1, c2, s1, s2, p1, p2, q1;
	int p0, q0;
	ANGLE_T a0, a1, a2;
	int strPath = dtod.strPath, str2Path = dtod.str2Path;

	struct extraDataCompound_t* xx = dtod.xx;

	// Create drawing points
	int i, j;
	for (i = 0; i < DTO_DIM; i++) {
		for (j = 0; j < dto[i].n; j++) {
			REORIGIN(p1, dto[i].base[j], xx->angle, xx->orig);
			dto[i].pts[j] = p1;
		}
	}

	i = strPath;
	dto[i].angle = FindAngle(dto[i].pts[0], dto[i].pts[dto[i].n - 1]);
	i = str2Path;
	dto[i].angle = FindAngle(dto[i].pts[0], dto[i].pts[dto[i].n - 1]);

	int othPath = strPath, secPath = str2Path;
	int toType = dtod.toType;

	othPath = dtod.strPath;
	secPath = dtod.str2Path;

	switch (toType) {
	case DTO_XING:
	case DTO_XNG9:
		break;
	case DTO_SSLIP:
		for (i = 0; i < dtod.pathCnt; i++) {
			if (dto[i].type == 'L' || dto[i].type == 'R') {
				secPath = i;
				break;
			}
		}
		break;
	case DTO_DSLIP:
		for (i = 0; i < dtod.pathCnt; i++) {
			if (dto[i].type == 'L') {
				othPath = i;
			}
			else if (dto[i].type == 'R') {
				secPath = i;
			}
		}
		break;
	}

	if(dtod.bridge) {
		DrawXingBridge(d,othPath,secPath);
	}
	// draw the points
#ifdef DTO_DEBUG
	if (DTO_DEBUG == DTO_XING) DrawDtoLayout(d, scaleInx);
#endif

	if(omitTies)
		return;

	td = GetScaleTieData(scaleInx);
	DIST_T tdlen = td->length, tdmax = 2.5 * tdlen;
	DIST_T tdspc = td->spacing, tdspc2 = tdspc / 2;

	// Midpoint
	p1 = dto[strPath].pts[0];
	a1 = dto[strPath].angle;

	q1 = dto[str2Path].pts[0];
	a2 = dto[str2Path].angle;

	FindIntersection(&pos, p1, a1, q1, a2);
	dtod.midPt = pos;

	// Tie length adjust
	a0 = DifferenceBetweenAngles(a1, a2);
	double magic = 1.0;

	// Short circuit the complex code for this simple case
	if (toType == DTO_XNG9) {
		p1 = dto[strPath].pts[0];
		p2 = dto[strPath].pts[dto[strPath].n - 1];
		DrawStraightTies(d, scaleInx, p1, p2, color);

		p1 = dto[str2Path].pts[0];
		p2 = dto[str2Path].pts[dto[strPath].n - 1];
		
		// Omit the center ties
		magic = 1 / cos(D2R(90 - a0));
		DIST_T tdadj = (tdlen / 2) * magic;
		DIST_T tdadj2 = tdspc2 * magic;

		a0 = (a0 - 90) / 2;
		Translate(&pos, dtod.midPt, a2, -tdadj - tdadj2);
		DrawTie(d, pos, a2 - a0, tdlen, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);

		Translate(&pos, dtod.midPt, a2, -tdadj - tdspc);
		DrawStraightTies(d, scaleInx, p1, pos, color);

		Translate(&pos, dtod.midPt, a2, tdadj + tdadj2);
		DrawTie(d, pos, a2 - a0, tdlen, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);

		Translate(&pos, dtod.midPt, a2, tdadj + tdspc);
		DrawStraightTies(d, scaleInx, pos, p2, color);
		return;
	}

	// Straight vector for tie angle
	s1 = MidPtCoOrd(dto[strPath].base[0], dto[str2Path].base[0]);
	s2 = MidPtCoOrd(dto[strPath].base[dto[strPath].n - 1], dto[str2Path].base[dto[str2Path].n - 1]);
	cAngle = FindAngle(s1, s2);
	len = FindDistance(s1, s2);
	POS_T x1 = max(dto[strPath].base[0].x, dto[str2Path].base[0].x);
	POS_T x2 = min(dto[strPath].base[dto[strPath].n - 1].x, dto[str2Path].base[dto[str2Path].n - 1].x);
	// Clip to shortest length in x-coord
	POS_T dy = (s1.y - s2.y) / len;
	if (s1.x < x1) {
		POS_T dx = s1.x - x1;
		s1.x = x1;
		s1.y = s1.y + dy * dx;
	}
	if (s2.x > x2) {
		POS_T dx = s2.x - x2;
		s2.x = x2;
		s2.y = s2.y + dy * dx;
	}

	// Rotate base coordinates so that the tie line is aligned with x-axis and origin is at zero
	for (i = 0; i < DTO_DIM; i++)
		for (j = 0; j < dto[i].n; j++) {
			dto[i].base[j].x -= s1.x;
			dto[i].base[j].y -= s1.y;
			Rotate(&dto[i].base[j], zero, (90.0 - cAngle));
		}

	for (i = 0; i < DTO_DIM; i++) {
		for (j = 0; j < dto[i].n - 1; j++) {
			dto[i].dy[j] = (dto[i].base[j + 1].y - dto[i].base[j].y) / (dto[i].base[j + 1].x - dto[i].base[j].x);
		}
		if (dto[i].type == 'S')
			dto[i].angle = FindAngle(dto[i].pts[0], dto[i].pts[dto[i].n - 1]);
	}

	// Tie center line in drawing coordinates
	REORIGIN(c1, s1, xx->angle, xx->orig);
	REORIGIN(c2, s2, xx->angle, xx->orig);
	cAngle = FindAngle(c1, c2);

	int pn = dto[othPath].n;
	int qn = dto[secPath].n;

	// Tie length adjust
	magic = 1 / cos(0.75 * D2R(a0));

	// Draw right half
	len = FindDistance(dtod.midPt, c2);
	cnt = (int)floor(len / td->spacing + 0.5);
	if (cnt <= 0)
		return;

	DIST_T dx = len / cnt;
	p0 = q0 = 0;
	DIST_T dx2 = dx / 2;
	DIST_T px = len + dx2;

	while (p0 < pn && px > dto[othPath].base[p0 + 1].x) p0++;
	while (q0 < qn && px > dto[secPath].base[q0 + 1].x) q0++;
	while (p0 < pn && q0 < qn) {
		if (px > dto[othPath].base[p0 + 1].x) p0++;
		if (px > dto[secPath].base[q0 + 1].x) q0++;
		if (p0 >= pn || q0 >= qn)
			break;

		if ((px + dx >= dto[othPath].base[pn - 1].x)
			|| (px + dx >= dto[secPath].base[qn - 1].x)) {
			break;
		}

		DIST_T dy1 = dto[othPath].base[p0].y + (px - dto[othPath].base[p0].x) * dto[othPath].dy[p0];
		DIST_T dy2 = dto[secPath].base[q0].y + (px - dto[secPath].base[q0].x) * dto[secPath].dy[q0];
		tdlen = (td->length + fabs(dy1) + fabs(dy2)) * magic;
		if (tdlen > tdmax)
			break;

		DIST_T dy = (dy1 + dy2) / 2;
		Translate(&pos, dtod.midPt, cAngle, px - len);
		Translate(&pos, pos, (cAngle - 90.0), dy);
		DrawTie(d, pos, cAngle, tdlen, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);

		px += dx;
	}

	int n = dto[strPath].n;
	p1 = dtod.midPt;
	p2 = dto[strPath].pts[n - 1];
	a0 = dto[strPath].angle;
	DIST_T lenr = FindDistance(p1, p2) - len;
	if (lenr >= dx2) {
		Translate(&pos, p2, a0, -lenr - dx);
		DrawStraightTies(d, scaleInx, pos, p2, color);
	}
	else {
		Translate(&pos, p2, a0, -dx2);
		DrawTie(d, pos, a0, td->length, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);
	}
	n = dto[str2Path].n;
	// p1 = dtod.midPt;
	p2 = dto[str2Path].pts[n - 1];
	a0 = dto[str2Path].angle;
	lenr = FindDistance(p1, p2) - len;
	if (lenr > dx2) {
		Translate(&pos, p2, a0, -lenr - dx);
		DrawStraightTies(d, scaleInx, pos, p2, color);
	}
	else {
		Translate(&pos, p2, a0, -dx2);
		DrawTie(d, pos, a0, td->length, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);
	}

	// Draw left half
	// Change the straight path used
	if (dtod.toType == DTO_SSLIP) {
		othPath = str2Path;
	}

	len = FindDistance(c1, dtod.midPt);
	cnt = (int)floor(len / td->spacing + 0.5);
	if (cnt <= 0)
		return;

	p0 = q0 = 0;
	tdlen = td->length;

	dx = len / cnt;
	dx2 = dx / 2;
	px = len - dx2;
	while (p0 < pn && px > dto[othPath].base[p0 + 1].x) p0++;
	while (q0 < qn && px > dto[secPath].base[q0 + 1].x) q0++;
	while (p0 >= 0 && q0 >= 0) {
		if (px < dto[othPath].base[p0].x) p0--;
		if (px < dto[secPath].base[q0].x) q0--;
		if (p0 < 0 || q0 < 0)
			break;

		if ((px - dx < dto[othPath].base[0].x)
			|| (px - dx < dto[secPath].base[0].x)) {
			break;
		}

		DIST_T dy1 = dto[othPath].base[p0].y + (px - dto[othPath].base[p0].x) * dto[othPath].dy[p0];
		DIST_T dy2 = dto[secPath].base[q0].y + (px - dto[secPath].base[q0].x) * dto[secPath].dy[q0];
		tdlen = (td->length + fabs(dy1) + fabs(dy2)) * magic;
		if (tdlen > 2.5 * td->length)
			break;

		DIST_T dy = (dy1 + dy2) / 2;
		Translate(&pos, dtod.midPt, cAngle, px - len);
		Translate(&pos, pos, (cAngle - 90.0), dy);
		DrawTie(d, pos, cAngle, tdlen, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);

		px -= dx;
	}

	p1 = dto[strPath].pts[0];
	p2 = dtod.midPt;
	a0 = dto[strPath].angle;
	lenr = FindDistance(p1, p2) - len;
	if (lenr > dx2) {
		Translate(&pos, p1, a0, lenr + dx);
		DrawStraightTies(d, scaleInx, p1, pos, color);
	}
	else {
		Translate(&pos, p1, a0, dx2);
		DrawTie(d, pos, a0, td->length, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);
	}
	p1 = dto[str2Path].pts[0];
	// p2 = dtod.midPt;
	a0 = dto[str2Path].angle;
	lenr = FindDistance(p1, p2) - len;
	if (lenr > dx2) {
		Translate(&pos, p1, a0, lenr + dx);
		DrawStraightTies(d, scaleInx, p1, pos, color);
	}
	else {
		Translate(&pos, p1, a0, dx2);
		DrawTie(d, pos, a0, td->length, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);
	}
}

/**
 * Draw Crossover (Two Origin) Turnout Bridge and Ties. Uses the static dto and dtod structures.
 *
 * \param d The drawing object
 * \param scaleInx The layout/track scale index
 * \param color The tie color. If black the color is read from the global tieColor.
 */
static void DrawCrossTurnout(
	drawCmd_p d,
	SCALEINX_T scaleInx,
	BOOL_T omitTies,
	wDrawColor color)
{
	tieData_p td;
	DIST_T len, dx;
	coOrd pos;
	int cnt;
	ANGLE_T angle;

	if (color == wDrawColorBlack)
		color = tieColor;

	struct extraDataCompound_t* xx = dtod.xx;

	int i, j;
	for (i = 0; i < DTO_DIM; i++)
		for (j = 0; j < dto[i].n; j++) {
			REORIGIN(pos, dto[i].base[j], xx->angle, xx->orig);
			dto[i].pts[j] = pos;
			if (j < dto[i].n - 1)
				dto[i].dy[j] = (dto[i].base[j + 1].y - dto[i].base[j].y) / (dto[i].base[j + 1].x - dto[i].base[j].x);
		}

	i = dtod.strPath; 
	dto[i].angle = FindAngle(dto[i].pts[0], dto[i].pts[dto[i].n - 1]);
	i = dtod.str2Path;
	dto[i].angle = FindAngle(dto[i].pts[0], dto[i].pts[dto[i].n - 1]);

	// draw the points
#ifdef DTO_DEBUG
	if (DTO_DEBUG == DTO_LCROSS) DrawDtoLayout(d, scaleInx);
#endif

	int strPath = dtod.strPath, str2Path = dtod.str2Path;
	// Bad assumption
	int othPath = 2, secPath = 2;
	if (dtod.pathCnt == 4) secPath = 3;

	int toType = dtod.toType;

	if(dtod.bridge) {
		DrawCrossBridge(d,strPath,str2Path);
	}
	if (omitTies)
		return;

	td = GetScaleTieData(scaleInx);

	coOrd s1, s2, t1, t2, p1, p2, q1, q2;
	int s0, t0, p0, q0;

	int sn = dto[strPath].n;
	int tn = dto[str2Path].n;
	int pn = dto[othPath].n;
	int qn = dto[secPath].n;

	s1 = dto[strPath].pts[0];
	s2 = dto[strPath].pts[sn - 1];
	t1 = dto[str2Path].pts[0];
	t2 = dto[str2Path].pts[tn - 1];
	angle = dto[strPath].angle; // FindAngle(s1, s2); // The straight segment

	p1 = dto[othPath].base[0];
	p2 = dto[othPath].base[pn - 1];
	q1 = dto[secPath].base[0];
	q2 = dto[secPath].base[qn - 1];

	td = GetScaleTieData(scaleInx);
	len = FindDistance(s1, s2);
	angle = dto[strPath].angle;

	cnt = (int)floor(len / td->spacing + 0.5);
	if (cnt > 0) {
		DIST_T px = 0;
		DIST_T dy, dy1, dy2;
		int cflag = 0;
		dy = dto[str2Path].base[0].y - dto[strPath].base[0].y;

		dx = len / cnt;
		s0 = t0 = p0 = q0 = 0;
		DIST_T tdlen = td->length;
		DIST_T dlenx = dx / 2;

		DIST_T px1 = len / 2 - dlenx * 5,
			px2 = len / 2 + dlenx * 4;

		for (px = dlenx; cnt; cnt--, px += dx) {
			if (px >= dto[strPath].base[s0 + 1].x) s0++;
			if (px >= dto[str2Path].base[t0 + 1].x) t0++;
			if (px >= dto[othPath].base[p0 + 1].x) p0++;
			if (px >= dto[secPath].base[q0 + 1].x) q0++;
			if (s0 >= sn || t0 >= tn || p0 >= pn || q0 >= qn)
				break;

			if ((px >= dto[strPath].base[sn - 1].x)
				|| (px >= dto[str2Path].base[tn - 1].x)) {
				break;
			}

			dy1 = dy2 = 0;
			cflag = 0;
			if (px < px1) {
				switch (dtod.toType) {
				case DTO_DCROSS:
					dy1 = dto[othPath].base[p0].y + (px - dto[othPath].base[p0].x) * dto[othPath].dy[p0];
					dy2 = dy - dto[secPath].base[q0].y - (px - dto[secPath].base[q0].x) * dto[secPath].dy[q0];
					break;
				case DTO_LCROSS:
					dy1 = dto[othPath].base[p0].y + (px - dto[othPath].base[p0].x) * dto[othPath].dy[p0];
					dy2 = 0;
					break;
				case DTO_RCROSS:
					dy1 = 0;
					dy2 = dy - dto[secPath].base[q0].y - (px - dto[secPath].base[q0].x) * dto[secPath].dy[q0];
					break;
				}
			}
			else if (px < px2) {
				dy1 = (dto[str2Path].base[s0].y - dto[strPath].base[t0].y);
				dy2 = 0;
				cflag = 1;
			}
			else {
				switch (dtod.toType) {
				case DTO_DCROSS:
					dy1 = dto[secPath].base[q0].y + (px - dto[secPath].base[q0].x) * dto[secPath].dy[q0];
					dy2 = dy - dto[othPath].base[p0].y - (px - dto[othPath].base[p0].x) * dto[othPath].dy[p0];
					break;
				case DTO_LCROSS:
					dy1 = 0;
					dy2 = dy - dto[secPath].base[q0].y - (px - dto[secPath].base[q0].x) * dto[secPath].dy[q0];
					break;
				case DTO_RCROSS:
					dy1 = dto[othPath].base[p0].y + (px - dto[othPath].base[p0].x) * dto[othPath].dy[p0];
					dy2 = 0;
					break;
				}
			}

			if (fabs(dy1) + fabs(dy2) >= dy) {
				dy1 = (dto[str2Path].base[s0].y - dto[strPath].base[t0].y);
				dy2 = 0;
				cflag = 1;
			}

			tdlen = td->length + fabs(dy1);
			Translate(&pos, s1, angle, px);
			Translate(&pos, pos, (angle - 90.0), dy1 / 2);
			DrawTie(d, pos, angle, tdlen, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);

			if (!cflag) {
				tdlen = td->length + fabs(dy2);
				Translate(&pos, t1, angle, px);
				Translate(&pos, pos, (angle - 90.0), -dy2 / 2);
				DrawTie(d, pos, angle, tdlen, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);
			}
		}
		return;

		// Draw remaining ties, if any
		// Currently by definition, there won't be any
		/*
		if (px + dx < dto[strPath].base[pn - 1].x) {
			p1 = dto[strPath].pts[p0];
			p2 = dto[strPath].pts[pn - 1];
			angle = FindAngle(p1, p2);
			a0 = FindAngle(dto[strPath].base[p0], dto[strPath].base[pn - 1]);
			DIST_T lenr = (dto[strPath].base[pn - 1].x - px + dlenx) / cos(D2R(90.0 - a0));
			Translate(&p1, p2, angle, -lenr);
			DrawStraightTies(d, scaleInx, p1, p2, color);
		}
		else {
			p1 = dto[strPath].pts[pn - 2];
			a0 = FindAngle(p1, p2);
			Translate(&pos, p2, a0, -dx / 2);
			DrawTie(d, pos, a0, td->length, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);
		}

		if (px + dx < dto[str2Path].base[qn - 1].x) {
			q1 = dto[str2Path].pts[q0];
			q2 = dto[str2Path].pts[qn - 1];
			angle = FindAngle(q1, q2);
			a0 = FindAngle(dto[str2Path].base[q0], dto[str2Path].base[qn - 1]);
			DIST_T lenr = (dto[str2Path].base[qn - 1].x - px + dlenx) / cos(D2R(90.0 - a0));
			Translate(&q1, q2, angle, -lenr);
			DrawStraightTies(d, scaleInx, q1, q2, color);
		}
		else {
			q1 = dto[str2Path].pts[qn - 2];
			a0 = FindAngle(q1, q2);
			Translate(&pos, q2, a0, -dx / 2);
			DrawTie(d, pos, a0, td->length, td->width, color, tieDrawMode == TIEDRAWMODE_SOLID);
		}
		*/
	}
}

/**
  * Draw all turnout components: ties, rail, roadbed, etc. The turnout is checked
  * to see if the enhanced methods can be used. If so the ties are drawn and the
  * TB_NOTIES bit is set so that the rails and such are drawn on top of the ties.
  * That bit is restored to its previous state before return.
  *
  * \param trk Pointer to the track object
  * \param d The drawing object
  * \param color The turnout color.
  */
static void DrawTurnout(
	track_p trk,
	drawCmd_p d,
	wDrawColor color)
{
	struct extraDataCompound_t* xx = GET_EXTRA_DATA(trk, T_TURNOUT, extraDataCompound_t);
	wIndex_t i;
	long widthOptions = 0;
	SCALEINX_T scaleInx = GetTrkScale(trk);
	DIST_T scale2rail = (d->options & DC_PRINT) ? (twoRailScale * 2 + 1) : twoRailScale;
	BOOL_T omitTies = !DoDrawTies(d, trk) || ((d->scale >= scale2rail) && (d->options & DC_SIMPLE) == 0 && (scaleInx >= 0));

	widthOptions = DTS_LEFT | DTS_RIGHT;

	int noTies = 0;
	int bridge = GetTrkBridge(trk);

	int pathCnt = GetTurnoutPaths(trk, xx);

	if ((pathCnt > 1) && (pathCnt <= DTO_DIM)
		&& (trk->endCnt <= 4)
		&& (xx->special == TOnormal || xx->special == TOcurved))
	{

		dtod.bridge = bridge; 

		int strPath = -1;
		GetTurnoutType();

		if (dtod.toType > DTO_INVALID) {

			switch (dtod.toType)
			{
			case DTO_NORMAL:
			case DTO_THREE:
			case DTO_WYE:
				DrawNormalTurnout(d, scaleInx, omitTies, color);
				break;
			case DTO_CURVED:
				DrawCurvedTurnout(d, scaleInx, omitTies, color);
				break;
			case DTO_XING:
			case DTO_XNG9:
			case DTO_SSLIP:
			case DTO_DSLIP:
				DrawXingTurnout(d, scaleInx, omitTies, color);
				break;
			case DTO_LCROSS:
			case DTO_RCROSS:
			case DTO_DCROSS:
				DrawCrossTurnout(d, scaleInx, omitTies, color);
				break;
			}
			noTies = 1;
			SetTrkNoTies(trk, 1); // ->bits |= TB_NOTIES;
            ClrTrkBits(trk, TB_BRIDGE); // ->bits &= ~TB_BRIDGE;
		}
	}

	// Begin standard DrawTurnout code to draw rails or centerline
	DrawSegsO(d, trk, xx->orig, xx->angle, xx->segs, xx->segCnt, GetTrkGauge(trk), color, widthOptions | DTS_NOCENTER);  // no curve center for turnouts


	for (i = 0; i < GetTrkEndPtCnt(trk); i++) {
		DrawEndPt(d, trk, i, color);
	}
	if ((d->options & DC_SIMPLE) == 0 &&
		(labelWhen == 2 || (labelWhen == 1 && (d->options & DC_PRINT))) &&
		labelScale >= d->scale &&
		(GetTrkBits(trk) & TB_HIDEDESC) == 0) {
		DrawCompoundDescription(trk, d, color);
		if (!xx->handlaid)
			LabelLengths(d, trk, color);
	}
	if (roadbedWidth > GetTrkGauge(trk) &&
		(((d->options & DC_PRINT) && d->scale <= (twoRailScale * 2 + 1) / 2.0) ||
			(roadbedOnScreen && d->scale <= twoRailScale)))
		DrawTurnoutRoadbed(d, color, xx->orig, xx->angle, xx->segs, xx->segCnt);

	// Restore these settings
	if (noTies) ClrTrkBits(trk, TB_NOTIES);
	if (bridge) SetTrkBits(trk, TB_BRIDGE);
}


static BOOL_T ReadTurnout(
	char* line)
{
	if (!ReadCompound(line + 8, T_TURNOUT))
		return FALSE;
	return TRUE;
}


static ANGLE_T GetAngleTurnout(
	track_p trk,
	coOrd pos,
	EPINX_T* ep0,
	EPINX_T* ep1)
{
	struct extraDataCompound_t* xx = GET_EXTRA_DATA(trk, T_TURNOUT, extraDataCompound_t);
	wIndex_t segCnt, segInx;
	ANGLE_T angle;

	if (ep0 && ep1)
		*ep0 = *ep1 = PickEndPoint(pos, trk);
	coOrd pos0 = pos;
	double dd = DIST_INF;
	int found = -1;
	//Cope with tracks not being first
	for (segCnt = 0; segCnt < xx->segCnt; segCnt++) {
		if (IsSegTrack(&xx->segs[segCnt])) {
			double d = DistanceSegs(xx->orig, xx->angle, 1, &xx->segs[segCnt], &pos0, NULL);
			if (d < dd) {
				dd = d;
				found = segCnt;
			}
		}
		pos0 = pos;
	}
	if (found >= 0) {
		pos.x -= xx->orig.x;
		pos.y -= xx->orig.y;
		Rotate(&pos, zero, -xx->angle);
		angle = GetAngleSegs(1, &xx->segs[found], &pos, &segInx, NULL, NULL, NULL, NULL);
		return NormalizeAngle(angle + xx->angle);
	}
	else return 0.0;
}


static BOOL_T SplitTurnoutCheckPath(
	wIndex_t segInxEnd,
	PATHPTR_T pp1,
	int dir1,
	PATHPTR_T pp2,
	int dir2,
	trkSeg_p segs,
	coOrd epPos)
{
	wIndex_t segInx1, segInx2;
	EPINX_T segEP;
	coOrd pos;
	DIST_T dist;

	GetSegInxEP(pp2[0], &segInx2, &segEP);
	if (dir2 < 0) segEP = 1 - segEP;
	pos = GetSegEndPt(&segs[segInx2], segEP, FALSE, NULL);
	dist = FindDistance(pos, epPos);
	if (dist > connectDistance)
		return TRUE;
	while (pp2[0]) {
		GetSegInxEP(pp1[0], &segInx1, &segEP);
		GetSegInxEP(pp2[0], &segInx2, &segEP);
		if (segInx1 != segInx2)
			break;
		if (segInxEnd == segInx2)
			return TRUE;
		pp1 += dir1;
		pp2 += dir2;
	}
	return FALSE;
}


static BOOL_T SplitTurnoutCheckEP(
	wIndex_t segInx0,
	coOrd epPos,
	PATHPTR_T pp1,
	int dir1,
	PATHPTR_T pp,
	trkSeg_p segs)
{
	while (pp[0]) {
		pp += strlen((char*)pp) + 1;
		while (pp[0]) {
			if (!SplitTurnoutCheckPath(segInx0, pp1, dir1, pp, 1, segs, epPos))
				return FALSE;
			while (pp[0])
				pp++;
			if (!SplitTurnoutCheckPath(segInx0, pp1, dir1, pp - 1, -1, segs, epPos))
				return FALSE;
			pp++;
		}
		pp++;
	}
	return TRUE;
}


EXPORT EPINX_T TurnoutPickEndPt(
	coOrd epPos,
	track_p trk)
{
	struct extraDataCompound_t* xx = GET_EXTRA_DATA(trk, T_TURNOUT, extraDataCompound_t);
	wIndex_t segInx, segInx0;
	EPINX_T segEP;
	PATHPTR_T cp, cq, pps[2];
	coOrd pos;
	DIST_T dist, dists[2];
	int dir;
	EPINX_T ep, epCnt, eps[2];
	BOOL_T unique_eps[2];

	DistanceSegs(xx->orig, xx->angle, xx->segCnt, xx->segs, &epPos, &segInx0);
	Rotate(&epPos, xx->orig, xx->angle);
	epPos.x -= xx->orig.x;
	epPos.y -= xx->orig.y;
	epCnt = GetTrkEndPtCnt(trk);
	cp = GetPaths(trk);
	eps[0] = eps[1] = -1;
	unique_eps[0] = unique_eps[1] = TRUE;
	while (cp[0]) {
		cp += strlen((char*)cp) + 1;
		while (cp[0]) {
			while (cp[0]) {
				GetSegInxEP(cp[0], &segInx, &segEP);
				if (segInx == segInx0) {
					for (dir = 0; dir < 2; dir++) {
						for (cq = cp; cq[dir ? -1 : 1]; cq += (dir ? -1 : 1));
						GetSegInxEP(cq[0], &segInx, &segEP);
						if (dir == 0) segEP = 1 - segEP;
						pos = GetSegEndPt(&xx->segs[segInx], segEP, FALSE, NULL);
						dist = FindDistance(pos, epPos);
						if (eps[dir] < 0 || dist < dists[dir]) {
							dists[dir] = dist;
							pos.x += xx->orig.x;
							pos.y += xx->orig.y;
							Rotate(&pos, xx->orig, xx->angle);
							for (ep = 0; ep < epCnt; ep++) {
								if (FindDistance(pos, GetTrkEndPos(trk, ep)) < connectDistance)
									break;
							}
							if (ep < epCnt) {
								if (eps[dir] >= 0 && eps[dir] != ep)
									unique_eps[dir] = FALSE;
								eps[dir] = ep;
								dists[dir] = dist;
								pps[dir] = cq;
							}
						}
					}
				}
				cp++;
			}
			cp++;
		}
		cp++;
	}

	for (dir = 0; dir < 2; dir++) {
		if (unique_eps[dir] && eps[dir] >= 0) {
			GetSegInxEP(pps[dir][0], &segInx, &segEP);
			if (dir == 0) segEP = 1 - segEP;
			epPos = GetSegEndPt(&xx->segs[segInx], segEP, FALSE, NULL);
			if (!SplitTurnoutCheckEP(segInx0, epPos, pps[dir], dir ? 1 : -1, GetPaths(trk), xx->segs))
				unique_eps[dir] = FALSE;
		}
	}

	if (unique_eps[0] == unique_eps[1]) {
		if (eps[0] >= 0 && eps[1] >= 0)
			return (dists[0] < dists[1]) ? eps[0] : eps[1];
	}
	if (unique_eps[0] && eps[0] >= 0)
		return eps[0];
	if (unique_eps[1] && eps[1] >= 0)
		return eps[1];
	if (eps[0] >= 0 && eps[1] >= 0)
		return (dists[0] < dists[1]) ? eps[0] : eps[1];
	return eps[0] >= 0 ? eps[0] : eps[1];
}


static PATHPTR_T splitTurnoutPath;
static PATHPTR_T splitTurnoutRoot;
static int splitTurnoutDir;

static void SplitTurnoutCheckEndPt(
	PATHPTR_T path,
	int dir,
	trkSeg_p segs,
	coOrd epPos,
	coOrd splitPos)
{
	PATHPTR_T path0;
	wIndex_t segInx;
	EPINX_T segEP;
	coOrd pos;
	DIST_T dist, minDist;

	path0 = path;
	GetSegInxEP(path[0], &segInx, &segEP);
	if (dir < 0) segEP = 1 - segEP;
	pos = GetSegEndPt(&segs[segInx], segEP, FALSE, NULL);
	dist = FindDistance(pos, epPos);
	LOG(log_splitturnout, 1, ("  SPTChkEp P%d DIR:%d SegInx:%d SegEP:%d POS[%0.3f %0.3f] DIST:%0.3f\n", *path, dir, segInx, segEP, pos.x, pos.y, dist));
	if (dist > connectDistance)
		return;
	minDist = trackGauge;
	while (path[0]) {
		GetSegInxEP(path[0], &segInx, &segEP);
		if (dir < 0) segEP = 1 - segEP;
		pos = splitPos;
		dist = DistanceSegs(zero, 0.0, 1, &segs[segInx], &pos, NULL);
		LOG(log_splitturnout, 1, ("  - P:%d SegInx:%d SegEP:%d DIST:%0.3f\n", path[0], segInx, segEP, dist));
		if (dist < minDist) {
			minDist = dist;
			splitTurnoutPath = path;
			splitTurnoutDir = -dir;
			splitTurnoutRoot = path0;
		}
		path += dir;
	}
}

EXPORT BOOL_T SplitTurnoutCheck(
	track_p trk,
	coOrd pos,
	EPINX_T ep,
	track_p* leftover,
	EPINX_T* ep0,
	EPINX_T* ep1,
	BOOL_T check,
	coOrd* outPos,
	ANGLE_T* outAngle)
{
	struct extraDataCompound_t* xx = GET_EXTRA_DATA(trk, T_TURNOUT, extraDataCompound_t);
	wIndex_t segInx0, segInx, segCnt;
	EPINX_T segEP, epCnt, ep2 = 0, epN;
	PATHPTR_T pp, pp1, pp2;
	unsigned char c;
	char* cp;
	int negCnt, posCnt, pathCnt, dir;
	segProcData_t segProcDataSplit;
	segProcData_t segProcDataNewTrack;
	track_p trk2 = NULL;
	static dynArr_t segIndexMap_da;
#define segIndexMap(N) DYNARR_N( int, segIndexMap_da, N )
	static dynArr_t newPath_da;
#define newPath(N) DYNARR_N( char, newPath_da, N )
	coOrd orig, size, epPos;
	ANGLE_T epAngle;
	PATHPTR_T path;
	int s0, s1;
	trkSeg_t newSeg;

	if ((MyGetKeyState() & WKEY_SHIFT) == 0) {
		if (!check)
			ErrorMessage(MSG_CANT_SPLIT_TRK, _("Turnout"));
		return FALSE;
	}

	/*
	 * 1. Find segment on path that ends at 'ep'
	 */
	epCnt = GetTrkEndPtCnt(trk);
	epPos = GetTrkEndPos(trk, ep);
	for (segCnt = 0; segCnt < xx->segCnt && IsSegTrack(&xx->segs[segCnt]); segCnt++);
	Rotate(&pos, xx->orig, -xx->angle);
	pos.x -= xx->orig.x;
	pos.y -= xx->orig.y;
	Rotate(&epPos, xx->orig, -xx->angle);
	epPos.x -= xx->orig.x;
	epPos.y -= xx->orig.y;
	splitTurnoutPath = NULL;
	pp = GetPaths(trk);
	LOG(log_splitturnout, 1, ("SplitTurnoutCheck T%d POS[%0.3f %0.3f] EP:%d CHK:%d EPPOS[%0.3f %0.3f]\n", trk ? trk->index : 0, pos.x, pos.y, ep, check, epPos.x, epPos.y));
	while (pp[0]) {
		pp += strlen((char*)pp) + 1;
		while (pp[0]) {
			SplitTurnoutCheckEndPt(pp, 1, xx->segs, epPos, pos);
			if (splitTurnoutPath != NULL)
				goto foundSeg;
			while (pp[0])
				pp++;
			SplitTurnoutCheckEndPt(pp - 1, -1, xx->segs, epPos, pos);
			if (splitTurnoutPath != NULL)
				goto foundSeg;
			pp++;
		}
		pp++;
	}
	if (!check)
		ErrorMessage(_("splitTurnout: can't find segment"));
	return FALSE;
foundSeg:

	/*
	 * 2a. Check that all other paths thru found segment are the same
	 */
	GetSegInxEP(splitTurnoutPath[0], &segInx0, &segEP);
	LOG(log_splitturnout, 1, (" Found Seg: %d SEG:%d EP:%d\n", *splitTurnoutPath, segInx0, segEP));
	pp = GetPaths(trk);
	pathCnt = 0;
	while (pp[0]) {
		pp += strlen((char*)pp) + 1;
		while (pp[0]) {
			while (pp[0]) {
				GetSegInxEP(pp[0], &segInx, &segEP);
				if (segInx == segInx0) {
					pp1 = splitTurnoutPath;
					pp2 = pp;
					dir = (pp2[0] > 0 ? 1 : -1) * splitTurnoutDir;
					while (pp1[0] && pp2[0]) {
						if (splitTurnoutDir * pp1[0] != dir * pp2[0])
							break;
						pp1 += splitTurnoutDir;
						pp2 += dir;
					}
					if (pp1[0] != '\0' || pp2[0] != '\0') {
						if (!check)
							ErrorMessage(MSG_SPLIT_POS_BTW_MERGEPTS);
						return FALSE;
					}
				}
				pp++;
			}
			pp++;
		}
		pp++;
	}

	/*
	 * 2b. Check that all paths from ep pass thru segInx0
	 */
	if (!SplitTurnoutCheckEP(segInx0, epPos, splitTurnoutRoot, -splitTurnoutDir, GetPaths(trk), xx->segs)) {
		if (!check)
			ErrorMessage(MSG_SPLIT_PATH_NOT_UNIQUE);
		return FALSE;
	}

	if (check) {
		segProcDataSplit.getAngle.pos = pos;
		SegProc(SEGPROC_GETANGLE, xx->segs + segInx0, &segProcDataSplit);
		*outAngle = NormalizeAngle(segProcDataSplit.getAngle.angle + xx->angle);
		*outPos = segProcDataSplit.getAngle.pos;
		(*outPos).x += xx->orig.x;
		(*outPos).y += xx->orig.y;
		Rotate(outPos, xx->orig, xx->angle);
		return TRUE;
	}

	/*
	 * 3. Split the found segment.
	 */
	segProcDataSplit.split.pos = pos;
	s0 = (splitTurnoutPath[0] > 0) != (splitTurnoutDir > 0);
	s1 = 1 - s0;
	SegProc(SEGPROC_SPLIT, xx->segs + segInx0, &segProcDataSplit);
	if (segProcDataSplit.split.length[s1] <= minLength) {
		if (splitTurnoutPath[splitTurnoutDir] == '\0')
			return FALSE;
		segProcDataSplit.split.length[s0] += segProcDataSplit.split.length[s1];
		segProcDataSplit.split.length[s1] = 0;
		segProcDataSplit.split.newSeg[s0] = xx->segs[segInx0];
		epPos = GetSegEndPt(&segProcDataSplit.split.newSeg[s0], s1, FALSE, &epAngle);
	}
	else if (segProcDataSplit.split.length[s0] <= minLength) {
		segProcDataSplit.split.length[s1] += segProcDataSplit.split.length[s0];
		segProcDataSplit.split.length[s0] = 0;
		segProcDataSplit.split.newSeg[s1] = xx->segs[segInx0];
		epPos = GetSegEndPt(&segProcDataSplit.split.newSeg[s1], s0, FALSE, &epAngle);
		epAngle += 180.0;
	}
	else {
		epPos = GetSegEndPt(&segProcDataSplit.split.newSeg[s1], s0, FALSE, &epAngle);
		epAngle += 180.0;
	}

	/*
	 * 4. Map the old segments to new
	 */
	DYNARR_SET(int, segIndexMap_da, xx->segCnt);
	for (segInx = 0; segInx < xx->segCnt; segInx++)
		segIndexMap(segInx) = segInx + 1;
	pp = splitTurnoutPath;
	if (segProcDataSplit.split.length[s0] > minLength)
		pp += splitTurnoutDir;
	negCnt = 0;
	while (*pp) {
		GetSegInxEP(*pp, &segInx, &segEP);
		segIndexMap(segInx) = -segIndexMap(segInx);
		negCnt++;
		pp += splitTurnoutDir;
	}
	for (segInx = posCnt = 0; segInx < xx->segCnt; segInx++) {
		if (segIndexMap(segInx) > 0)
			segIndexMap(segInx) = ++posCnt;
	}
	DYNARR_SET(trkSeg_t, tempSegs_da, posCnt);
	for (segInx = posCnt = 0; segInx < xx->segCnt; segInx++) {
		if (segIndexMap(segInx) > 0) {
			if (segInx == segInx0) {
				tempSegs(segIndexMap(segInx) - 1) = segProcDataSplit.split.newSeg[s0];
			}
			else {
				tempSegs(segIndexMap(segInx) - 1) = xx->segs[segInx];
			}
			posCnt++;
		}
	}

	/*
	 * 5. Remap paths by removing trailing segments
	 */
	pp = GetPaths(trk);
	DYNARR_SET(char, newPath_da, GetPathsLength(pp));
	pp1 = (PATHPTR_T)&newPath(0);
	while (*pp) {
		strcpy((char*)pp1, (char*)pp);
		pp += strlen((char*)pp) + 1;
		pp1 += strlen((char*)pp1) + 1;
		while (*pp) {
			while (*pp) {
				GetSegInxEP(*pp, &segInx, &segEP);
				if (segIndexMap(segInx) > 0) {
					c = segIndexMap(segInx);
					if (*pp < 0)
						c = -c;
					*pp1++ = c;
				}
				pp++;
			}
			*pp1++ = '\0';
			pp++;
		}
		*pp1++ = '\0';
		pp++;
	}
	*pp1++ = '\0';

	/*
	 * 6. Reorigin segments
	 */
	GetSegBounds(zero, 0, tempSegs_da.cnt, &tempSegs(0), &orig, &size);
	orig.x = -orig.x;
	orig.y = -orig.y;
	MoveSegs(tempSegs_da.cnt, &tempSegs(0), orig);
	epPos.x += orig.x;
	epPos.y += orig.y;
	cp = strchr(xx->title, '\t');
	if (cp) {
		if (strncmp(cp + 1, "Split ", 6) != 0) {
			memcpy(message, xx->title, cp - xx->title + 1);
			strcpy(message + (cp - xx->title + 1), "Split ");
			strcat(message, cp + 1);
		}
		else {
			strcpy(message, xx->title);
		}
	}
	else {
		sprintf(message, "Split %s", xx->title);
	}

	/*
	 * 7. Convert trailing segments to new tracks
	 */
	int trks = 0;
	path = splitTurnoutPath;
	if (segProcDataSplit.split.length[s1] < minLength)
		path += splitTurnoutDir;
	while (path[0]) {
		GetSegInxEP(path[0], &segInx, &segEP);
		s0 = (path[0] > 0) != (splitTurnoutDir > 0);
		if (segInx0 != segInx) {
			newSeg = xx->segs[segInx];
		}
		else {
			newSeg = segProcDataSplit.split.newSeg[s1];
		}
		MoveSegs(1, &newSeg, xx->orig);
		RotateSegs(1, &newSeg, xx->orig, xx->angle);
		SegProc(SEGPROC_NEWTRACK, &newSeg, &segProcDataNewTrack);
		if (*leftover == NULL) {
			*ep0 = segProcDataNewTrack.newTrack.ep[s0];
			*leftover = trk2 = segProcDataNewTrack.newTrack.trk;
			ep2 = 1 - *ep0;
		}
		else {
			epN = segProcDataNewTrack.newTrack.ep[s0];
			ConnectTracks(trk2, ep2, segProcDataNewTrack.newTrack.trk, epN);
			trk2 = segProcDataNewTrack.newTrack.trk;
			ep2 = 1 - epN;
		}
		++trks;
		path += splitTurnoutDir;
	}

	/*
	 * 8. Replace segments, paths, and endPt in original turnout
	 */
	xx->split = TRUE;
	Rotate(&orig, zero, xx->angle);
	xx->orig.x -= orig.x;
	xx->orig.y -= orig.y;
	xx->segCnt = tempSegs_da.cnt;
	xx->segs = (trkSeg_p)memdup(&tempSegs(0), tempSegs_da.cnt * sizeof tempSegs(0));
	CloneFilledDraw(xx->segCnt, xx->segs, TRUE);
	SetPaths(trk, (PATHPTR_T)&newPath(0));
	epAngle = NormalizeAngle(xx->angle + epAngle);
	epPos.x += xx->orig.x;
	epPos.y += xx->orig.y;
	Rotate(&epPos, xx->orig, xx->angle);
	SetTrkEndPoint(trk, ep, epPos, epAngle);
	ComputeCompoundBoundingBox(trk);

	return TRUE;
}

static BOOL_T SplitTurnout(
	track_p trk,
	coOrd pos,
	EPINX_T ep,
	track_p* leftover,
	EPINX_T* ep0,
	EPINX_T* ep1)
{
	return SplitTurnoutCheck(trk, pos, ep, leftover, ep0, ep1, FALSE, NULL, NULL);
}

static BOOL_T CheckTraverseTurnout(
	track_p trk,
	coOrd pos)
{
	struct extraDataCompound_t* xx = GET_EXTRA_DATA(trk, T_TURNOUT, extraDataCompound_t);
	coOrd pos1;
#ifdef LATER
	int inx, foundInx = 0;
	DIST_T d, foundD;
#endif
	DIST_T d;
	PATHPTR_T pathCurr;
	int segInx;
	EPINX_T segEP;

	LOG(log_traverseTurnout, 1, ("CheckTraverseTurnout( T%d, [%0.3f %0.3f])\n", GetTrkIndex(trk), pos.x, pos.y))
		Rotate(&pos, xx->orig, -xx->angle);
	pos.x -= xx->orig.x;
	pos.y -= xx->orig.y;
	LOG(log_traverseTurnout, 1, ("After rotation = [%0.3f %0.3f])\n", pos.x, pos.y))

#ifdef LATER
		for (inx = 0; inx < xx->segCnt; inx++) {
			switch (xx->segs[inx].type) {
			case SEG_STRTRK:
			case SEG_CRVTRK:
				pos1 = GetSegEndPt(&xx->segs[inx], 0, FALSE, NULL);
				d = FindDistance(pos, pos1);
				if (foundInx == 0 || d < foundD) {
					foundInx = inx + 1;
					foundD = d;
				}
				pos1 = GetSegEndPt(&xx->segs[inx], 1, FALSE, NULL);
				d = FindDistance(pos, pos1);
				if (foundInx == 0 || d < foundD) {
					foundInx = -(inx + 1);
					foundD = d;
				}
				break;
			}
		}
	if (foundInx == 0)
		return FALSE;
#endif
	PATHPTR_T pathName = GetCurrPath(trk);
	for (pathCurr = pathName + strlen((char*)pathName) + 1;
		pathCurr[0] || pathCurr[1]; pathCurr++) {
		LOG(log_traverseTurnout, 1, ("P[%d] = %d ", pathCurr - GetPaths(trk), pathCurr[0]))
			if (pathCurr[-1] == 0) {
				GetSegInxEP(pathCurr[0], &segInx, &segEP);
				pos1 = GetSegEndPt(&xx->segs[segInx], segEP, FALSE, NULL);
				d = FindDistance(pos, pos1);
				LOG(log_traverseTurnout, 1, ("d=%0.3f\n", d))
					if (d < connectDistance)
						return TRUE;
			}
		if (pathCurr[1] == 0) {
			GetSegInxEP(pathCurr[0], &segInx, &segEP);
			pos1 = GetSegEndPt(&xx->segs[segInx], 1 - segEP, FALSE, NULL);
			d = FindDistance(pos, pos1);
			LOG(log_traverseTurnout, 1, ("d=%0.3f\n", d))
				if (d < connectDistance)
					return TRUE;
		}
	}
	LOG(log_traverseTurnout, 1, (" not found\n"))
		return FALSE;
}


static BOOL_T TraverseTurnout(
	traverseTrack_p trvTrk,
	DIST_T* distR)
{
	track_p trk = trvTrk->trk;
	struct extraDataCompound_t* xx = GET_EXTRA_DATA(trk, T_TURNOUT, extraDataCompound_t);
	coOrd pos0, pos1, pos2;
	DIST_T d, dist;
	PATHPTR_T path, pathCurr;
	trkSeg_p segPtr;
	EPINX_T ep, epCnt, ep2;
	int segInx;
	EPINX_T segEP;
	segProcData_t segProcData;

	d = DIST_INF;
	pos0 = trvTrk->pos;
	Rotate(&pos0, xx->orig, -xx->angle);
	pos0.x -= xx->orig.x;
	pos0.y -= xx->orig.y;
	dist = *distR;
	LOG(log_traverseTurnout, 1, ("TraverseTurnout( T%d, [%0.3f %0.3f] [%0.3f %0.3f], A%0.3f, D%0.3f\n", GetTrkIndex(trk), trvTrk->pos.x, trvTrk->pos.y, pos0.x, pos0.y, trvTrk->angle, *distR))
		pathCurr = 0;
	path = GetCurrPath(trk);
	for (path += strlen((char*)path) + 1; path[0] || path[1]; path++) {
		if (path[0] == 0)
			continue;
		GetSegInxEP(path[0], &segInx, &segEP);
		segPtr = xx->segs + segInx;
		segProcData.distance.pos1 = pos0;
		SegProc(SEGPROC_DISTANCE, segPtr, &segProcData);
		if (segProcData.distance.dd < d) {
			d = segProcData.distance.dd;
			pos2 = segProcData.distance.pos1;
			pathCurr = path;
		}
	}
	if (d > 10 || pathCurr == 0) {
		ErrorMessage("traverseTurnout: Not near: %0.3f", d);
		return FALSE;
	}
	LOG(log_traverseTurnout, 1, ("  PC=%d ", pathCurr[0]))
		GetSegInxEP(pathCurr[0], &segInx, &segEP);
	segPtr = xx->segs + segInx;
	segProcData.traverse1.pos = pos2;
	segProcData.traverse1.angle = -xx->angle + trvTrk->angle;
	SegProc(SEGPROC_TRAVERSE1, segPtr, &segProcData);
	dist += segProcData.traverse1.dist;
	//Get ready for Traverse2 - copy all Traverse1 first
	BOOL_T backwards = segProcData.traverse1.backwards;
	BOOL_T segs_backwards = segProcData.traverse1.segs_backwards;
	BOOL_T neg = segProcData.traverse1.negative;
	int BezSegInx = segProcData.traverse1.BezSegInx;

	// Backwards means universally we going towards EP=0 on this segment.
	// But the overall direction we are going can have two types of reversal,
	// a curve that is flipped is negative (the end points are reversed) which Traverse1 handles,
	// and a path can also be reversed (negative path number) and will have segEP = 1
	BOOL_T turnout_backwards = backwards;
	if (segEP) turnout_backwards = !turnout_backwards; //direction modified if path reversed

	LOG(log_traverseTurnout, 2, (" SI%d TB%d SP%d B%d SB%d N%d BSI%d D%0.3f\n", segInx, turnout_backwards, segEP, backwards, segs_backwards, neg, BezSegInx, dist))
		while (*pathCurr) {
			//Set up Traverse2
			GetSegInxEP(pathCurr[0], &segInx, &segEP);
			segPtr = xx->segs + segInx;
			segProcData.traverse2.segDir = backwards;
			segProcData.traverse2.dist = dist;
			segProcData.traverse2.BezSegInx = BezSegInx;
			segProcData.traverse2.segs_backwards = segs_backwards;
			SegProc(SEGPROC_TRAVERSE2, segPtr, &segProcData);
			if (segProcData.traverse2.dist <= 0) {
				*distR = 0;
				REORIGIN(trvTrk->pos, segProcData.traverse2.pos, xx->angle, xx->orig);
				trvTrk->angle = NormalizeAngle(xx->angle + segProcData.traverse2.angle);
				LOG(log_traverseTurnout, 2, ("  -> [%0.3f %0.3f] A%0.3f D%0.3f\n", trvTrk->pos.x, trvTrk->pos.y, trvTrk->angle, *distR))
					return TRUE;
			}
			dist = segProcData.traverse2.dist;			//Remainder after segment
			pathCurr += (turnout_backwards ? -1 : 1);		//Use master direction for turnout
			//Redrive Traverse 1 for each segment for Bezier - to pick up backwards elements
			if (pathCurr[0] == '\0') continue;          //
			//Set up Traverse1 - copy all of Traverse2 values first
			GetSegInxEP(pathCurr[0], &segInx, &segEP);
			segPtr = xx->segs + segInx;
			ANGLE_T angle = segProcData.traverse2.angle;
			coOrd pos = segProcData.traverse2.pos;
			LOG(log_traverseTurnout, 1, (" Loop2-1 SI%d SP%d [%0.3f %0.3f] A%0.3f D%0.3f\n", segInx, segEP, pos.x, pos.y, angle, dist))
				segProcData.traverse1.pos = pos;
			segProcData.traverse1.angle = angle;
			SegProc(SEGPROC_TRAVERSE1, segPtr, &segProcData);
			// dist += segProcData.traverse1.dist;	               //Add distance from end to pos (could be zero or whole length if backwards)
			backwards = segProcData.traverse1.backwards;
			segs_backwards = segProcData.traverse1.segs_backwards;
			neg = segProcData.traverse1.negative;
			BezSegInx = segProcData.traverse1.BezSegInx;
			LOG(log_traverseTurnout, 1, (" Loop1-2 B%d SB%d N%d BSI%d D%0.3f\n", backwards, segs_backwards, neg, BezSegInx, dist))
		}

	pathCurr += (turnout_backwards ? 1 : -1);
	pos1 = MapPathPos(xx, pathCurr[0], (turnout_backwards ? 0 : 1));
	*distR = dist;
	epCnt = GetTrkEndPtCnt(trk);
	ep = 0;
	dist = FindDistance(pos1, GetTrkEndPos(trk, 0));
	for (ep2 = 1; ep2 < epCnt; ep2++) {
		d = FindDistance(pos1, GetTrkEndPos(trk, ep2));
		if (d < dist) {
			dist = d;
			ep = ep2;
		}
	}
	if (dist > connectDistance) {
		trk = NULL;
		trvTrk->pos = pos1;
	}
	else {
		trvTrk->pos = GetTrkEndPos(trk, ep);
		trvTrk->angle = GetTrkEndAngle(trk, ep);
		trk = GetTrkEndTrk(trk, ep);
	}
	dist = FindDistance(trvTrk->pos, pos1);
	LOG(log_traverseTurnout, 1, ("  -> [%0.3f %0.3f] A%0.3f D%0.3f\n", trvTrk->pos.x, trvTrk->pos.y, trvTrk->angle, *distR))
		trvTrk->trk = trk;
	return TRUE;
}


static STATUS_T ModifyTurnout(track_p trk, wAction_t action, coOrd pos)
{
	struct extraDataCompound_t* xx;
	static EPINX_T ep;
	static wBool_t curved;
	DIST_T d;

	xx = GET_EXTRA_DATA(trk, T_TURNOUT, extraDataCompound_t);
	if (xx->special == TOadjustable) {
		switch (action) {
		case C_START:
			ep = -1;
			curved = FALSE;
			return C_CONTINUE;
		case C_DOWN:
			ep = PickUnconnectedEndPoint(pos, trk);
			if (ep == -1)
				return C_ERROR;
			UndrawNewTrack(trk);
			tempSegs(0).type = SEG_STRTRK;
			tempSegs(0).width = 0;
			tempSegs(0).u.l.pos[0] = GetTrkEndPos(trk, 1 - ep);
			tempSegs_da.cnt = 1;
			InfoMessage(_("Drag to change track length"));
			return C_CONTINUE;
		case C_MOVE:
			d = FindDistance(tempSegs(0).u.l.pos[0], pos);
			if (d < xx->u.adjustable.minD)
				d = xx->u.adjustable.minD;
			else if (d > xx->u.adjustable.maxD)
				d = xx->u.adjustable.maxD;
			Translate(&tempSegs(0).u.l.pos[1], tempSegs(0).u.l.pos[0], GetTrkEndAngle(trk, ep), d);
			tempSegs_da.cnt = 1;
			if (action == C_MOVE)
				InfoMessage(_("Length=%s"), FormatDistance(d));
			return C_CONTINUE;
		case C_UP:
			d = FindDistance(tempSegs(0).u.l.pos[0], tempSegs(0).u.l.pos[1]);
			ChangeAdjustableEndPt(trk, ep, d);
			return C_TERMINATE;
		default:
			return C_CONTINUE;
		}
	}

	return ExtendTrackFromOrig(trk, action, pos);
}


static BOOL_T GetParamsTurnout(int inx, track_p trk, coOrd pos, trackParams_t* params)
{
	struct extraDataCompound_t* xx;
	xx = GET_EXTRA_DATA(trk, T_TURNOUT, extraDataCompound_t);
	params->type = curveTypeStraight;
	if (inx == PARAMS_TURNOUT) {
		params->len = 0.0;
		int epCnt = GetTrkEndPtCnt(trk);
		if (epCnt < 3) {
			double d = DIST_INF;
			params->centroid = zero;
			//calculate path length from endPt (either to end or to other end)
			segProcData_t segProcData;
			trkSeg_p seg;
			int segInx;
			int segEP;
			trkSeg_p segPtr;
			PATHPTR_T path, pathCurr;
			//Find starting seg on path (nearest to end Pt)
			path = GetCurrPath(trk);
			pathCurr = path;
			for (path += strlen((char*)path) + 1; path[0] || path[1]; path++) {
				if (path[0] == 0)
					continue;
				GetSegInxEP(path[0], &segInx, &segEP);
				segPtr = xx->segs + segInx;
				segProcData.distance.pos1 = pos;
				SegProc(SEGPROC_DISTANCE, segPtr, &segProcData);
				if (segProcData.distance.dd < d) {
					d = segProcData.distance.dd;
					pathCurr = path;
				}
			}
			GetSegInxEP(pathCurr[0], &segInx, &segEP);
			seg = xx->segs + segInx;
			d = 0.0;
			//Loop through segs on path from endPt adding
			while (pathCurr[0]) {
				GetSegInxEP(pathCurr[0], &segInx, &segEP);
				seg = xx->segs + segInx;
				SegProc(SEGPROC_LENGTH, seg, &segProcData);
				d += segProcData.length.length;
				pathCurr += segEP ? 1 : -1;
			}
			params->len = d;
		}
		else {
			// Centroid is middle of bounding box
			params->centroid.x = (trk->lo.x + trk->hi.x) / 2.0;
			params->centroid.y = (trk->lo.y + trk->hi.y) / 2.0;
			params->len = FindDistance(params->centroid, pos) * 2;  //Times two because it will be halved by track.c
		}
		return TRUE;
	}
	if ((inx == PARAMS_CORNU) || (inx == PARAMS_EXTEND)) {
		params->type = curveTypeStraight;
		params->arcR = 0.0;
		params->arcP = zero;
		params->ep = PickEndPoint(pos, trk);
		params->circleOrHelix = FALSE;
		if (params->ep >= 0) {
			params->angle = GetTrkEndAngle(trk, params->ep);
			params->track_angle = params->angle + params->ep ? 0 : 180;
		}
		else {
			params->angle = params->track_angle = 0;
			return FALSE;
		}
		/* Use end radii if we have them */
		//if (xx->special == TOcurved) {
		//	params->type = curveTypeCurve;
		//	params->arcR = fabs(DYNARR_N(DIST_T,xx->u.curved.radii,params->ep));
		//	if (params->arcR != 0.0)
		//		Translate(&params->arcP,pos,params->track_angle-90.0,params->arcR);
		//	else
		//		params->type = curveTypeStraight;
		//	return TRUE;
		//}
		/* Find the path we are closest to */
		PATHPTR_T pathCurr = 0;
		int segInx, subSegInx;
		trkSeg_p segPtr;
		DIST_T d = DIST_INF;
		struct extraDataCompound_t* xx = GET_EXTRA_DATA(trk, T_TURNOUT, extraDataCompound_t);
		/* Get parms from that seg */
		wBool_t back, negative;
		coOrd segPos = pos;
		Rotate(&segPos, xx->orig, -xx->angle);
		segPos.x -= xx->orig.x;
		segPos.y -= xx->orig.y;

		params->track_angle = GetAngleSegs(		  		//Find correct subSegment
			xx->segCnt, xx->segs,
			&segPos, &segInx, &d, &back, &subSegInx, &negative);
		if (segInx == -1) return FALSE;
		segPtr = xx->segs + segInx;
		switch (segPtr->type) {
		case SEG_BEZTRK:
			if (negative != back) params->track_angle = NormalizeAngle(params->track_angle + 180);  //Bezier is in reverse
			segPtr = xx->segs + segInx;
			trkSeg_p subSegPtr = (trkSeg_p)segPtr->bezSegs.ptr + subSegInx;
			if (subSegPtr->type == SEG_CRVTRK) {
				params->type = curveTypeCurve;
				params->arcR = fabs(subSegPtr->u.c.radius);
				params->arcP = subSegPtr->u.c.center;
				params->arcP.x += xx->orig.x;
				params->arcP.y += xx->orig.y;
				Rotate(&params->arcP, xx->orig, xx->angle);
				params->arcA0 = subSegPtr->u.c.a0;
				params->arcA1 = subSegPtr->u.c.a1;
			}
			return TRUE;
			break;
		case SEG_CRVTRK:
			params->type = curveTypeCurve;
			params->arcR = fabs(segPtr->u.c.radius);
			params->arcP = segPtr->u.c.center;
			params->arcP.x += xx->orig.x;
			params->arcP.y += xx->orig.y;
			Rotate(&params->arcP, xx->orig, xx->angle);
			params->arcA0 = segPtr->u.c.a0;
			params->arcA1 = segPtr->u.c.a1;
			return TRUE;
			break;
		}
		params->arcR = 0.0;
		params->arcP = zero;
		params->ep = PickEndPoint(pos, trk);   //Nearest
		if (params->ep >= 0) {
			params->angle = GetTrkEndAngle(trk, params->ep);
			params->track_angle = params->angle + params->ep ? 0 : 180;
		}
		else {
			params->angle = params->track_angle = 0;
			return FALSE;
		}
		return TRUE;
	}
	if ((inx == PARAMS_1ST_JOIN) || (inx == PARAMS_2ND_JOIN))
		params->ep = PickEndPoint(pos, trk);
	else
		params->ep = PickUnconnectedEndPointSilent(pos, trk);
	if (params->ep == -1)
		return FALSE;
	params->lineOrig = GetTrkEndPos(trk, params->ep);
	params->lineEnd = params->lineOrig;
	params->len = 0.0;
	params->angle = GetTrkEndAngle(trk, params->ep);
	params->arcR = 0.0;
	return TRUE;
}


static BOOL_T MoveEndPtTurnout(track_p* trk, EPINX_T* ep, coOrd pos, DIST_T d0)
{
	ANGLE_T angle0;
	DIST_T d;
	track_p trk1;

	angle0 = GetTrkEndAngle(*trk, *ep);
	d = FindDistance(GetTrkEndPos(*trk, *ep), pos);
	if (d0 > 0.0) {
		d -= d0;
		if (d < 0.0) {
			ErrorMessage(MSG_MOVED_BEFORE_END_TURNOUT);
			return FALSE;
		}
		Translate(&pos, pos, angle0 + 180, d0);
	}
	if (d > minLength) {
		trk1 = NewStraightTrack(GetTrkEndPos(*trk, *ep), pos);
		CopyAttributes(*trk, trk1);
		ConnectTracks(*trk, *ep, trk1, 0);
		*trk = trk1;
		*ep = 1;
		DrawNewTrack(*trk);
	}
	return TRUE;
}


static BOOL_T QueryTurnout(track_p trk, int query)
{
	switch (query) {
	case Q_IGNORE_EASEMENT_ON_EXTEND:
	case Q_DRAWENDPTV_1:
	case Q_CAN_GROUP:
	case Q_ISTRACK:
	case Q_NOT_PLACE_FROGPOINTS:
	case Q_HAS_DESC:
	case Q_MODIFY_REDRAW_DONT_UNDRAW_TRACK:
		return TRUE;
	case Q_MODIFY_CAN_SPLIT:
		return TRUE;
	case Q_IS_TURNOUT:
		return TRUE;
	case Q_CAN_PARALLEL:
		if (GetTrkEndPtCnt(trk) == 2 && fabs(GetTrkEndAngle(trk, 0) - GetTrkEndAngle(trk, 1)) == 180.0)
			return TRUE;
		else
			return FALSE;
	case Q_CAN_NEXT_POSITION:
		return (GetTrkEndPtCnt(trk) > 2);
	case Q_CORNU_CAN_MODIFY:
		return FALSE;
	default:
		return FALSE;
	}
}


EXPORT int doDrawTurnoutPosition = 1;
static wIndex_t drawTurnoutPositionWidth = 3;
static void DrawTurnoutPositionIndicator(
	track_p trk,
	wDrawColor color)
{
	struct extraDataCompound_t* xx = GET_EXTRA_DATA(trk, T_TURNOUT, extraDataCompound_t);
	PATHPTR_T path, path1;
	coOrd pos0 = zero, pos1;
	trkSeg_p seg;
	BOOL_T multiPart = FALSE;

	// Only 1 path?  Don't draw
	path = GetPaths(trk);
	for (path += strlen((char*)path) + 1; path[0] || path[1]; path++);
	if (path[2] == 0)
		return;

	path = GetCurrPath(trk);

	//Is this a multi-part path?
	path1 = path;
	for (path1 += strlen((char*)path1) + 1; path1[0]; path1++);
	if (path1[1] != 0) multiPart = TRUE;


	for (path += strlen((char*)path); path[0] || path[1]; path++) {

		if (path[0] == 0) {
			pos0 = MapPathPos(xx, path[1], 0);
			if ((tempD.scale <= 10) || !multiPart) {
				seg = MapPathSeg(xx, path[1]);
				DrawSegsO(&tempD, trk, xx->orig, xx->angle, seg, 1, GetTrkGauge(trk), color, DTS_CENTERONLY);
			}
		}
		else if (path[1] == 0) {
			pos1 = MapPathPos(xx, path[0], 1);
			if ((tempD.scale > 10) && multiPart)
				DrawLine(&tempD, pos0, pos1, drawTurnoutPositionWidth, color);
			else {
				seg = MapPathSeg(xx, path[0]);
				DrawSegsO(&tempD, trk, xx->orig, xx->angle, seg, 1, GetTrkGauge(trk), color, DTS_CENTERONLY);
			}
		}
		else if ((tempD.scale <= 10) || !multiPart) {
			seg = MapPathSeg(xx, path[0]);
			DrawSegsO(&tempD, trk, xx->orig, xx->angle, seg, 1, GetTrkGauge(trk), color, DTS_CENTERONLY);
		}
	}
}


EXPORT void AdvanceTurnoutPositionIndicator(
	track_p trk,
	coOrd pos,
	coOrd* posR,
	ANGLE_T* angleR)
{
	traverseTrack_t trvtrk;
	DIST_T dist;

	if (GetTrkType(trk) != T_TURNOUT)
		AbortProg("nextTurnoutPosition");

	SetCurrPathIndex(trk, GetCurrPathIndex(trk) + 1);
	InfoMessage(_("Turnout %d Path: %s"), GetTrkIndex(trk), GetCurrPath(trk));
	if (angleR == NULL || posR == NULL)
		return;
	trvtrk.trk = trk;
	trvtrk.length = 0;
	trvtrk.dist = 0;
	trvtrk.pos = *posR;
	trvtrk.angle = *angleR;
	dist = 0;
	if (!TraverseTurnout(&trvtrk, &dist))
		return;
	if (NormalizeAngle(trvtrk.angle - *angleR + 90.0) > 180)
		trvtrk.angle = NormalizeAngle(trvtrk.angle + 180.0);
	*posR = trvtrk.pos;
	*angleR = trvtrk.angle;
}

/**
 * Create a parallel track for a turnout.
 *
 *
 * \param trk IN existing track
 * \param pos IN ??
 * \param sep IN distance between existing and new track
 * \param newTrk OUT new track piece
 * \param p0R OUT starting point of new piece
 * \param p1R OUT ending point of new piece
 * \return    always TRUE
 */

static BOOL_T MakeParallelTurnout(
	track_p trk,
	coOrd pos,
	DIST_T sep,
	DIST_T factor,
	track_p* newTrk,
	coOrd* p0R,
	coOrd* p1R,
	BOOL_T track)
{
	ANGLE_T angle = GetTrkEndAngle(trk, 1);
	struct extraDataCompound_t* xx, * yy;
	coOrd* endPts;
	trkEndPt_p endPt;
	int i;
	int option;
	DIST_T d;

	if (NormalizeAngle(FindAngle(GetTrkEndPos(trk, 0), pos) - GetTrkEndAngle(trk, 1)) < 180.0)
		angle += 90;
	else
		angle -= 90;

	/*
	 * get all endpoints of current piece and translate them for the new piece
	 */
	endPts = MyMalloc(GetTrkEndPtCnt(trk) * sizeof(coOrd));
	for (i = 0; i < GetTrkEndPtCnt(trk); i++) {
		Translate(&(endPts[i]), GetTrkEndPos(trk, i), angle, sep);
	}

	/*
	 * get information about the current piece and copy data
	 */

	if (newTrk) {
		if (track) {
			endPt = MyMalloc(GetTrkEndPtCnt(trk) * sizeof(trkEndPt_t));
			endPt[0].pos = endPts[0];
			endPt[0].angle = GetTrkEndAngle(trk, 0);
			endPt[1].pos = endPts[1];
			endPt[1].angle = GetTrkEndAngle(trk, 1);

			yy = GET_EXTRA_DATA(trk, T_TURNOUT, extraDataCompound_t);

			DIST_T* radii = NULL;
			if (yy->special == TOcurved) {
				radii = MyMalloc(GetTrkEndPtCnt(trk) * sizeof(DIST_T));
				for (int i = 0; i < GetTrkEndPtCnt(trk); i++) {
					radii[i] = DYNARR_N(DIST_T, yy->u.curved.radii, i);
				}
			}

			PATHPTR_T paths = GetPaths(trk);
			*newTrk = NewCompound(T_TURNOUT, 0, endPt[0].pos, endPt[0].angle + 90.0,
				yy->title, 2, endPt, radii, paths,
				yy->segCnt, yy->segs);
			xx = GET_EXTRA_DATA(*newTrk, T_TURNOUT, extraDataCompound_t);
			xx->customInfo = yy->customInfo;

			/*	if (connection((int)curTurnoutEp).trk) {
				CopyAttributes( connection((int)curTurnoutEp).trk, newTrk );
				SetTrkScale( newTrk, curScaleInx );
			} */
			xx->special = yy->special;
			xx->pathOverRide = yy->pathOverRide;
			xx->pathNoCombine = yy->pathNoCombine;

			xx->u = yy->u;

			SetDescriptionOrig(*newTrk);
			xx->descriptionOff = zero;
			xx->descriptionSize = zero;

			SetTrkElev(*newTrk, GetTrkElevMode(trk), GetTrkElev(trk));
			GetTrkEndElev(trk, 0, &option, &d);
			SetTrkEndElev(*newTrk, 0, option, d, NULL);
			GetTrkEndElev(trk, 1, &option, &d);
			SetTrkEndElev(*newTrk, 1, option, d, NULL);

			MyFree(endPt);
		}
		else {
			tempSegs(0).color = wDrawColorBlack;
			tempSegs(0).width = 0;
			tempSegs_da.cnt = 1;
			tempSegs(0).type = track ? SEG_STRTRK : SEG_STRLIN;
			tempSegs(0).u.l.pos[0] = endPts[0];
			tempSegs(0).u.l.pos[1] = endPts[1];
			*newTrk = MakeDrawFromSeg(zero, 0.0, &tempSegs(0));
		}
	}
	else {
		/* draw some temporary track while command is in process */
		tempSegs(0).color = wDrawColorBlack;
		tempSegs(0).width = 0;
		tempSegs_da.cnt = 1;
		tempSegs(0).type = track ? SEG_STRTRK : SEG_STRLIN;
		tempSegs(0).u.l.pos[0] = endPts[0];
		tempSegs(0).u.l.pos[1] = endPts[1];
	}

	if (p0R) *p0R = endPts[0];
	if (p1R) *p1R = endPts[1];

	MyFree(endPts);
	return TRUE;
}

static wBool_t CompareTurnout(track_cp trk1, track_cp trk2)
{
	struct extraDataCompound_t* xx1 = GET_EXTRA_DATA(trk1, T_TURNOUT, extraDataCompound_t);
	struct extraDataCompound_t* xx2 = GET_EXTRA_DATA(trk2, T_TURNOUT, extraDataCompound_t);
	char* cp = message + strlen(message);
	REGRESS_CHECK_POS("Orig", xx1, xx2, orig)
		REGRESS_CHECK_ANGLE("Angle", xx1, xx2, angle)
		REGRESS_CHECK_INT("Handlaid", xx1, xx2, handlaid)
		REGRESS_CHECK_INT("Flipped", xx1, xx2, flipped)
		REGRESS_CHECK_INT("Ungrouped", xx1, xx2, ungrouped)
		REGRESS_CHECK_INT("Split", xx1, xx2, split)
		/* desc orig is not stable
		REGRESS_CHECK_POS( "DescOrig", xx1, xx2, descriptionOrig ) */
		REGRESS_CHECK_POS("DescOff", xx1, xx2, descriptionOff)
		REGRESS_CHECK_POS("DescSize", xx1, xx2, descriptionSize)
		return CompareSegs(xx1->segs, xx1->segCnt, xx1->segs, xx1->segCnt);
}

static trackCmd_t turnoutCmds = {
		"TURNOUT ",
		DrawTurnout,
		DistanceCompound,
		DescribeCompound,
		DeleteCompound,
		WriteCompound,
		ReadTurnout,
		MoveCompound,
		RotateCompound,
		RescaleCompound,
		NULL,
		GetAngleTurnout,
		SplitTurnout,
		TraverseTurnout,
		EnumerateCompound,
		NULL, /*redraw*/
		NULL, /*trim*/
		NULL, /*merge*/
		ModifyTurnout,
		NULL, /* getLength */
		GetParamsTurnout,
		MoveEndPtTurnout,
		QueryTurnout,
		UngroupCompound,
		FlipCompound,
		DrawTurnoutPositionIndicator,
		AdvanceTurnoutPositionIndicator,
		CheckTraverseTurnout,
		MakeParallelTurnout,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		CompareTurnout };


#ifdef TURNOUTCMD
/*****************************************
 *
 *   Turnout Dialog
 *
 */

static coOrd maxTurnoutDim;

static void AddTurnout(void);


static wWin_p turnoutW;


static void RescaleTurnout(void)
{
	DIST_T xscale, yscale;
	wWinPix_t ww, hh;
	DIST_T w, h;
	wDrawGetSize(turnoutD.d, &ww, &hh);
	w = ww / turnoutD.dpi;
	h = hh / turnoutD.dpi;
	xscale = maxTurnoutDim.x / w;
	yscale = maxTurnoutDim.y / h;
	turnoutD.scale = max(xscale, yscale);
	if (turnoutD.scale == 0.0)
		turnoutD.scale = 1.0;
	turnoutD.size.x = w * turnoutD.scale;
	turnoutD.size.y = h * turnoutD.scale;
	return;
}


static void TurnoutChange(long changes)
{
	static char* lastScaleName = NULL;
	if (turnoutW == NULL)
		return;
	wListSetIndex(turnoutListL, 0);
	if ((!wWinIsVisible(turnoutW)) ||
		(((changes & CHANGE_SCALE) == 0 || lastScaleName == curScaleName) &&
			(changes & CHANGE_PARAMS) == 0))
		return;
	lastScaleName = curScaleName;
	//curTurnout = NULL;
	curTurnoutEp = 0;
	wControlShow((wControl_p)turnoutListL, FALSE);
	wListClear(turnoutListL);
	maxTurnoutDim.x = maxTurnoutDim.y = 0.0;
	if (turnoutInfo_da.cnt <= 0)
		return;
	curTurnout = TurnoutAdd(LABEL_TABBED | LABEL_MANUF | LABEL_PARTNO | LABEL_DESCR, GetLayoutCurScale(), turnoutListL, &maxTurnoutDim, -1);
	wListSetIndex(turnoutListL, turnoutInx);
	wControlShow((wControl_p)turnoutListL, TRUE);
	if (curTurnout == NULL) {
		wDrawClear(turnoutD.d);
		return;
	}
	turnoutD.orig.x = -trackGauge;
	turnoutD.orig.y = -trackGauge;
	maxTurnoutDim.x += 2 * trackGauge;
	maxTurnoutDim.y += 2 * trackGauge;
	/*RescaleTurnout();*/
	RedrawTurnout( turnoutD.d, NULL, 0, 0 );
	return;
}

static void RedrawTurnout( wDraw_p d, void * context, wWinPix_t x, wWinPix_t y )
{
	RescaleTurnout();
	LOG(log_turnout, 2, ("SelTurnout(%s)\n", (curTurnout ? curTurnout->title : "<NULL>")))

		wDrawClear(turnoutD.d);
	if (curTurnout == NULL) {
		return;
	}
	turnoutD.orig.x = curTurnout->orig.x - trackGauge;
	turnoutD.orig.y = (curTurnout->size.y + curTurnout->orig.y) - turnoutD.size.y + trackGauge;
	DrawSegs(&turnoutD, zero, 0.0, curTurnout->segs, curTurnout->segCnt,
		trackGauge, wDrawColorBlack);
	curTurnoutEp = 0;
	HilightEndPt();
}


static void TurnoutOk(void)
{
	AddTurnout();
	Reset();
}


static void TurnoutDlgUpdate(
	paramGroup_p pg,
	int inx,
	void* valueP)
{
	turnoutInfo_t* to;
	if (inx != I_LIST) return;
	to = (turnoutInfo_t*)wListGetItemContext((wList_p)pg->paramPtr[inx].control, (wIndex_t) * (long*)valueP);
	AddTurnout();
	curTurnout = to;
	RedrawTurnout( turnoutD.d, NULL, 0, 0 );
	/*	ParamDialogOkActive( &turnoutPG, FALSE ); */
}


static wIndex_t TOpickEndPoint(
	coOrd p,
	turnoutInfo_t* to)
{
	wIndex_t inx, i;
	DIST_T d, dd;
	coOrd posI;

	d = FindDistance(p, to->endPt[0].pos);
	inx = 0;
	for (i = 1; i < to->endCnt; i++) {
		posI = to->endPt[i].pos;
		if ((dd = FindDistance(p, posI)) < d) {
			d = dd;
			inx = i;
		}
	}
	return inx;
}


static void HilightEndPt(void)
{
	coOrd p, s;
	p.x = curTurnout->endPt[(int)curTurnoutEp].pos.x - trackGauge;
	p.y = curTurnout->endPt[(int)curTurnoutEp].pos.y - trackGauge;
	s.x = s.y = trackGauge * 2.0 /*+ turnoutD.minSize*/;
	DrawHilight(&turnoutD, p, s, FALSE);
}


static void SelTurnoutEndPt(
	wIndex_t action,
	coOrd pos)
{
	if (action != C_DOWN) return;

	curTurnoutEp = TOpickEndPoint(pos, curTurnout);
	HilightEndPt();
	LOG(log_turnout, 3, (" selected (action=%d) %ld\n", action, curTurnoutEp))
}
#endif

/****************************************
 *
 * GRAPHICS COMMANDS
 *
 */

 /*
  * STATE INFO
  */
static struct {
	int state;
	coOrd pos;
	coOrd place;
	track_p trk;
	ANGLE_T angle;
	coOrd rot0, rot1;
} Dto;

static dynArr_t vector_da;
#define vector(N) DYNARR_N( vector_t, vector_da, N )

typedef struct {
	DIST_T off;
	ANGLE_T angle;
	EPINX_T ep;
	track_p trk;
} vector_t;

/*
 * PlaceTurnoutTrial
 *
 * OUT Track 	- the Track that the Turnout is closest to
 * IN/OUT Pos	- Position of the Turnout end
 * OUT Angle1   - The angle on the Track at the position
 * OUT Angle2   - The angle of the Turnout (can be reversed from Angle 2 if the point is to the left)
 * OUT Count	- The number of connections
 * OUT Max		- The maximum distance between the ends and the connection points
 * OUT Vector   - An array of end points positions and offsets
 */
static void PlaceTurnoutTrial(
	track_p* trkR,
	coOrd* posR,
	ANGLE_T* angle1R,
	ANGLE_T* angle2R,
	int* connCntR,
	DIST_T* maxDR,
	vector_t* v)
{
	coOrd pos = *posR;
	ANGLE_T aa;
	ANGLE_T angle;
	EPINX_T ep0, ep1;
	track_p trk, trk1;
	coOrd epPos, conPos, posI;
	ANGLE_T epAngle;
	int i, connCnt = 0;
	DIST_T d, maxD = 0;
	coOrd testP = pos;

	if (*trkR && (GetTrkDistance(*trkR, &testP) < trackGauge)) {   //Have Track, stick with it unless outside bounds
		trk = *trkR;
		pos = testP;
	}
	else *trkR = trk = OnTrack(&pos, FALSE, TRUE);
	if ((trk) != NULL &&
		!QueryTrack(trk, Q_CANNOT_PLACE_TURNOUT) &&
		(ep0 = PickEndPoint(pos, trk)) >= 0 &&
		!(GetTrkType(trk) == T_TURNOUT &&
			(trk1 = GetTrkEndTrk(trk, ep0)) &&
			GetTrkType(trk1) == T_TURNOUT) &&
		!GetLayerFrozen(GetTrkLayer(trk)) &&
		!GetLayerModule(GetTrkLayer(trk))) {
		epPos = GetTrkEndPos(trk, ep0);
		d = FindDistance(pos, epPos);
		if (d <= minLength)
			pos = epPos;
		if (GetTrkType(trk) == T_TURNOUT) {				//Only on the end
			ep0 = ep1 = PickEndPoint(pos, trk);
			angle = GetTrkEndAngle(trk, ep0);
		}
		else {
			angle = GetAngleAtPoint(trk, pos, &ep0, &ep1);
			if (ep0 == 1) angle = NormalizeAngle(angle + 180);      //Reverse if curve backwards
		}
		angle = NormalizeAngle(angle + 180.0);
		if (NormalizeAngle(FindAngle(pos, *posR) - angle) < 180.0 && ep0 != ep1)
			angle = NormalizeAngle(angle + 180);
		*angle2R = angle;
		epPos = curTurnout->endPt[(int)curTurnoutEp].pos;
		*angle1R = angle = NormalizeAngle(angle - curTurnout->endPt[(int)curTurnoutEp].angle);
		Rotate(&epPos, zero, angle);
		pos.x -= epPos.x;
		pos.y -= epPos.y;
		*posR = pos;			//The place the Turnout end sits
		LOG(log_turnout, 3, ("placeTurnout T%d (%0.3f %0.3f) A%0.3f\n",
			GetTrkIndex(trk), pos.x, pos.y, angle))
			/*InfoMessage( "Turnout(%d): Angle=%0.3f", GetTrkIndex(trk), angle );*/
			track_p ctrk = NULL;
		int ccnt = 0;
		DIST_T clarge = DIST_INF;
		for (i = 0; i < curTurnout->endCnt; i++) {
			posI = curTurnout->endPt[i].pos;
			epPos = AddCoOrd(pos, posI, angle);
			epAngle = NormalizeAngle(curTurnout->endPt[i].angle + angle);
			conPos = epPos;
			if ((trk = OnTrack(&conPos, FALSE, TRUE)) != NULL &&
				!GetLayerFrozen(GetTrkLayer(trk)) &&
				!GetLayerModule(GetTrkLayer(trk))) {
				v->off = FindDistance(epPos, conPos);
				v->angle = FindAngle(epPos, conPos);
				if (GetTrkType(trk) == T_TURNOUT) {
					ep0 = ep1 = PickEndPoint(conPos, trk);
					aa = GetTrkEndAngle(trk, ep0);
				}
				else {
					aa = GetAngleAtPoint(trk, conPos, &ep0, &ep1);
					if (ep0) 					//Backwards - so reverse
						aa = NormalizeAngle(aa + 180);
				}
				v->ep = i;
				aa = fabs(DifferenceBetweenAngles(aa, epAngle));
				if (QueryTrack(trk, Q_IS_CORNU)) {			//Make sure only two conns to each Cornu
					int k = 0;
					v->trk = trk;
					for (int j = 0; j < i; j++) {
						if (vector(j).trk == trk) k++;
					}
					if (k < 2) {						//Already two conns to this track
						connCnt++;
						if (v->off > maxD)
							maxD = v->off;
						v++;

					}
				}
				else if ((IsClose(v->off) && (aa < connectAngle || aa>180 - connectAngle) &&
					!(GetTrkType(trk) == T_TURNOUT &&
						(trk1 = GetTrkEndTrk(trk, ep0)) &&
						GetTrkType(trk1) == T_TURNOUT))) {
					if (v->off > maxD)
						maxD = v->off;
					connCnt++;
					v++;
				}
			}
		}
	}
	else {
		trk = NULL;
		*trkR = NULL;
	}
	*connCntR = connCnt;
	*maxDR = maxD;
}


static void PlaceTurnout(
	coOrd pos, track_p trk)
{
	coOrd p, pos1, pos2;
	track_p trk1, trk2;
	ANGLE_T a, a1, a2, a3;
	int i, connCnt1, connCnt2;
	DIST_T d, maxD1, maxD2, sina;
	vector_t* V, * maxV;



	pos1 = Dto.place = Dto.pos = pos;
	LOG(log_turnout, 1, ("Place Turnout @ %0.3fx%0.3f\n", Dto.pos.x, Dto.pos.y));
	if (curTurnoutEp >= (long)curTurnout->endCnt)
		curTurnoutEp = 0;
	DYNARR_SET(vector_t, vector_da, curTurnout->endCnt);
	if (trk) trk1 = trk;
	else trk1 = NULL;
	PlaceTurnoutTrial(&trk1, &pos1, &a1, &a2, &connCnt1, &maxD1, &vector(0));
	if (connCnt1 > 0) {
		Dto.pos = pos1;		//First track pos
		LOG(log_turnout, 1, (" trial 1 @ %0.3fx%0.3f\n", Dto.pos.x, Dto.pos.y));
		Dto.trk = trk1;		//Track
		Dto.angle = a1;		//Angle of track to put down
		if (((MyGetKeyState() & WKEY_SHIFT) == 0) && (connCnt1 > 1) && (maxD1 >= 0.001)) {  //Adjust if not Shift
			maxV = &vector(0);
			for (i = 1; i < connCnt1; i++) {		//Ignore first point
				V = &vector(i);
				if (V->off > maxV->off) {
					maxV = V;
				}
			}
			a3 = NormalizeAngle(Dto.angle + curTurnout->endPt[maxV->ep].angle);
			a = NormalizeAngle(a2 - a3);
			sina = sin(D2R(a));
			if (fabs(sina) > 0.01) {
				d = maxV->off / sina;
				if (NormalizeAngle(maxV->angle - a3) > 180)
					d = -d;
				Translate(&pos2, pos, a2, d);
				trk2 = trk1;
				PlaceTurnoutTrial(&trk2, &pos2, &a2, &a, &connCnt2, &maxD2, &vector(0));
				if (connCnt2 >= connCnt1 && maxD2 < maxD1) {
					Dto.pos = pos2;
					LOG(log_turnout, 1, (" trial 2 @ %0.3fx%0.3f\n", Dto.pos.x, Dto.pos.y));
					Dto.trk = trk2;
					Dto.angle = a2;
					maxD1 = maxD2;
					connCnt1 = connCnt2;
				}
			}
		}
	}
	if (connCnt1 > 0) {
		FormatCompoundTitle(listLabels, curTurnout->title);
		InfoMessage(_("%d connections, max distance %0.3f (%s)"),
			connCnt1, PutDim(maxD1), message);
	}
	else {
		Dto.trk = NULL;
		FormatCompoundTitle(listLabels, curTurnout->title);
		InfoMessage(_("0 connections (%s)"), message);
		p = curTurnout->endPt[(int)curTurnoutEp].pos;
		Rotate(&p, zero, Dto.angle);
		Dto.pos.x = pos.x - p.x;
		Dto.pos.y = pos.y - p.y;
		LOG(log_turnout, 1, (" final @ %0.3fx%0.3f\n", Dto.pos.x, Dto.pos.y));
	}
}

static void AddTurnout(void)
{
	track_p newTrk;
	track_p trk, trk1;
	struct extraDataCompound_t* xx;
	coOrd epPos;
	DIST_T d;
	ANGLE_T a, aa;
	EPINX_T ep0, ep1, epx, epy;
	wIndex_t i, j;
	typedef struct {
		track_p trk;
		EPINX_T ep;
	} junk_t;
	static dynArr_t connection_da;
	static dynArr_t leftover_da;
#define connection(N) DYNARR_N( junk_t, connection_da, N )
#define leftover(N) DYNARR_N( junk_t, leftover_da, N )
	BOOL_T visible;
	BOOL_T no_ties;
	BOOL_T noConnections;
	coOrd p0, p1;

	if (Dto.state == 0)
		return;

	if (curTurnout->segCnt < 1 || curTurnout->endCnt < 1) {
		AbortProg("addTurnout: bad cnt");
	}

	UndoStart(_("Place New Turnout"), "addTurnout");

	DYNARR_SET(trkEndPt_t, tempEndPts_da, curTurnout->endCnt);
	DYNARR_SET(junk_t, connection_da, curTurnout->endCnt);
	DYNARR_SET(junk_t, leftover_da, curTurnout->endCnt);

	for (i = 0; i < curTurnout->endCnt; i++) {
		coOrd posI;
		posI = curTurnout->endPt[i].pos;
		tempEndPts(i).pos = AddCoOrd(Dto.pos, posI, Dto.angle);
		tempEndPts(i).angle = NormalizeAngle(curTurnout->endPt[i].angle + Dto.angle);
	}

	AuditTracks("addTurnout begin");

	for (i = 0; i < curTurnout->endCnt; i++) {
		AuditTracks("addTurnout [%d]", i);
		connection(i).trk = leftover(i).trk = NULL;
		connection(i).ep = -1;
		leftover(i).ep = -1;
		/* connect each endPt ... */
		epPos = tempEndPts(i).pos;
		if ((trk = OnTrack(&epPos, FALSE, TRUE)) != NULL &&    //Adjust epPos onto existing track
			(!GetLayerFrozen(GetTrkLayer(trk))) &&
			(!GetLayerModule(GetTrkLayer(trk))) &&
			(!QueryTrack(trk, Q_CANNOT_PLACE_TURNOUT))) {
			LOG(log_turnout, 1, ("ep[%d] on T%d @(%0.3f %0.3f)\n",
				i, GetTrkIndex(trk), epPos.x, epPos.y))
				DIST_T dd = DIST_INF;
			int nearest = -1;
			for (int j = 0; j < curTurnout->endCnt; j++) {
				if (j < i && (connection(j).trk == trk)) {
					nearest = -1;
					goto nextEnd;  //Track already chosen in use
				}
				if (dd > FindDistance(epPos, tempEndPts(j).pos)) {
					dd = FindDistance(epPos, tempEndPts(j).pos);
					nearest = j;
				}
			}
			if (nearest != i) continue;    //Not this one
			d = FindDistance(tempEndPts(i).pos, epPos);
			if (GetTrkType(trk) == T_TURNOUT) {
				ep0 = ep1 = PickEndPoint(epPos, trk);
				a = GetTrkEndAngle(trk, ep0);
			}
			else {
				a = GetAngleAtPoint(trk, epPos, &ep0, &ep1);
			}
			aa = fabs(DifferenceBetweenAngles(a, tempEndPts(i).angle));
			if ((QueryTrack(trk, Q_IS_CORNU) && (d < trackGauge * 2)) ||
				((IsClose(d) && (((ep0 != ep1) && (aa <= connectAngle)) || ((aa <= connectAngle) || (aa > 180 - connectAngle))) &&
					!(GetTrkType(trk) == T_TURNOUT &&
						(trk1 = GetTrkEndTrk(trk, ep0)) &&
						GetTrkType(trk1) == T_TURNOUT)))) {
				/* ... if they are close enough to a track and line up */
				if (aa < 90) {
					epx = ep1;
					epy = ep0;
				}
				else {
					epx = ep0;
					epy = ep1;
				}
				LOG(log_turnout, 1, ("   Attach! epx=%d\n", epx))
					if ((epx != epy) &&
						((d = FindDistance(GetTrkEndPos(trk, epy), epPos)) < minLength) &&
						((trk1 = GetTrkEndTrk(trk, epy)) != NULL)) {
						epx = GetEndPtConnectedToMe(trk1, trk);
						trk = trk1;
					}
				/* split the track at the intersection point */
				AuditTracks("addTurnout [%d] before splitTrack", i);
				if (SplitTrack(trk, epPos, epx, &leftover(i).trk, TRUE)) {
					AuditTracks("addTurnout [%d], after splitTrack", i);
					/* remember so we can fix up connection later */
					connection(i).trk = trk;
					connection(i).ep = epx;
					if (leftover(i).trk != NULL) {
						leftover(i).ep = 1 - epx;
						/* did we already split this track? */
						for (j = 0; j < i; j++) {
							if (connection(i).trk == leftover(j).trk) {
								/* yes.  Remove the latest leftover piece */
								LOG(log_turnout, 1, ("   deleting leftover T%d\n",
									GetTrkIndex(leftover(i).trk)))
									AuditTracks("addTurnout [%d] before delete", i);
								UndrawNewTrack(leftover(i).trk);
								DeleteTrack(leftover(i).trk, FALSE);
								AuditTracks("addTurnout [%d] before delete", i);
								leftover(i).trk = NULL;
								leftover(i).ep = -1;
								leftover(j).trk = NULL; //Forget this leftover
								leftover(j).ep = -1;
								break;
							}
						}
					}
				}
			}
		}
	nextEnd:;
	}

	AuditTracks("addTurnout after loop");

	/*
	 * copy data */

	newTrk = NewCompound(T_TURNOUT, 0, Dto.pos, Dto.angle, curTurnout->title, tempEndPts_da.cnt, &tempEndPts(0), NULL, curTurnout->paths, curTurnout->segCnt, curTurnout->segs);
	xx = GET_EXTRA_DATA(newTrk, T_TURNOUT, extraDataCompound_t);
	xx->customInfo = curTurnout->customInfo;
	if (connection((int)curTurnoutEp).trk) {
		CopyAttributes(connection((int)curTurnoutEp).trk, newTrk);
		SetTrkScale(newTrk, GetLayoutCurScale());
	}
	xx->special = curTurnout->special;
	if (xx->special == TOcurved) {
		DYNARR_SET(DIST_T, xx->u.curved.radii, curTurnout->endCnt);
		for (int i = 0; i < curTurnout->endCnt; i++) {
			DYNARR_N(DIST_T, xx->u.curved.radii, i) = DYNARR_N(DIST_T, curTurnout->u.curved.radii, i);
		}
	}
	xx->u = curTurnout->u;
	xx->pathOverRide = curTurnout->pathOverRide;
	xx->pathNoCombine = curTurnout->pathNoCombine;

	/* Make the connections */

	visible = FALSE;
	no_ties = FALSE;
	noConnections = TRUE;
	AuditTracks("addTurnout T%d before connection", GetTrkIndex(newTrk));
	for (i = 0; i < curTurnout->endCnt; i++) {
		if (connection(i).trk != NULL) {
			if (GetTrkEndTrk(connection(i).trk, connection(i).ep)) continue;
			p0 = GetTrkEndPos(newTrk, i);
			p1 = GetTrkEndPos(connection(i).trk, connection(i).ep);
			ANGLE_T a0 = GetTrkEndAngle(newTrk, i);
			ANGLE_T a1 = GetTrkEndAngle(connection(i).trk, connection(i).ep);
			ANGLE_T a = fabs(DifferenceBetweenAngles(a0 + 180, a1));
			d = FindDistance(p0, p1);
			if (QueryTrack(connection(i).trk, Q_IS_CORNU)) {
				ANGLE_T a = DifferenceBetweenAngles(FindAngle(p0, p1), a0);
				if (IsClose(d) || fabs(a) <= 90.0) {
					trk1 = connection(i).trk;
					ep0 = connection(i).ep;
					if (GetTrkEndTrk(trk1, ep0)) continue;
					DrawEndPt(&mainD, trk1, ep0, wDrawColorWhite);
					trackParams_t params;
					GetTrackParams(PARAMS_EXTEND, newTrk, GetTrkEndPos(newTrk, i), &params);
					SetCornuEndPt(trk1, ep0, GetTrkEndPos(newTrk, i), params.arcP, NormalizeAngle(params.angle + 180.0), params.arcR);
					ConnectTracks(newTrk, i, trk1, ep0);
					DrawEndPt(&mainD, trk1, ep0, wDrawColorBlack);
				}
			}
			else if (d < connectDistance && (a <= connectAngle)) {
				noConnections = FALSE;
				trk1 = connection(i).trk;
				ep0 = connection(i).ep;
				DrawEndPt(&mainD, trk1, ep0, wDrawColorWhite);
				ConnectTracks(newTrk, i, trk1, ep0);
				visible |= GetTrkVisible(trk1);
				no_ties |= GetTrkNoTies(trk1);
				DrawEndPt(&mainD, trk1, ep0, wDrawColorBlack);
			}
		}
	}
	if (noConnections)
		visible = TRUE;
	SetTrkVisible(newTrk, visible);
	SetTrkNoTies(newTrk, no_ties);
	SetTrkBridge(newTrk, FALSE);

	AuditTracks("addTurnout T%d before dealing with leftovers", GetTrkIndex(newTrk));
	/* deal with the leftovers */
	for (i = 0; i < curTurnout->endCnt; i++) {
		if ((trk = leftover(i).trk) != NULL && !IsTrackDeleted(trk)) {
			/* move endPt beyond the turnout */
			/* it it is short then delete it */
			coOrd off;
			DIST_T maxX;
			track_p lt = leftover(i).trk;
			if (QueryTrack(lt, Q_IS_CORNU)) {
				UndrawNewTrack(lt);
				DeleteTrack(lt, TRUE);
				leftover(i).trk = NULL;
				continue;
			}
			EPINX_T ep, le = leftover(i).ep, nearest_ep = -1;
			coOrd pos, nearest_pos = zero;
			ANGLE_T nearest_angle = 0.0;
			DIST_T nearest_radius = 0.0;
			coOrd nearest_center = zero;
			trackParams_t params;
			maxX = 0.0;
			DIST_T dd = DIST_INF;
			a = NormalizeAngle(GetTrkEndAngle(lt, le) + 180.0);
			for (ep = 0; ep < curTurnout->endCnt; ep++) {
				FindPos(&off, NULL, GetTrkEndPos(newTrk, ep), GetTrkEndPos(lt, le), a, DIST_INF);
				pos = GetTrkEndPos(newTrk, ep);
				DIST_T d = GetTrkDistance(lt, &pos);
				if ((d < dd) && (d < trackGauge / 2)) {
					ANGLE_T a = GetTrkEndAngle(lt, le);
					ANGLE_T a2 = fabs(DifferenceBetweenAngles(GetTrkEndAngle(newTrk, ep), a + 180));
					if (GetTrkEndTrk(newTrk, ep) == NULL) { //Not if joined already
						if (a2 < 90 && QueryTrack(lt, Q_IS_CORNU)) { //And Cornu in the right direction
							GetTrackParams(PARAMS_EXTEND, newTrk, GetTrkEndPos(newTrk, ep), &params);
							nearest_pos = GetTrkEndPos(newTrk, ep);
							nearest_angle = NormalizeAngle(params.angle + 180.0);
							nearest_radius = params.arcR;
							nearest_center = params.arcP;
							nearest_ep = ep;
						}
						dd = d;
					}
				}
				if (off.x > maxX)
					maxX = off.x;
			}
			maxX += trackGauge;
			pos = Dto.pos;
			if (QueryTrack(lt, Q_IS_CORNU)) {
				if (nearest_ep >= 0) {
					SetCornuEndPt(lt, le, nearest_pos, nearest_center, nearest_angle, nearest_radius);
					ConnectTracks(newTrk, nearest_ep, lt, le);
				}
				else {
					UndrawNewTrack(lt);
					DeleteTrack(lt, TRUE);
				}
			}
			else {
				AuditTracks("addTurnout T%d[%d] before trimming L%d[%d]", GetTrkIndex(newTrk), i, GetTrkIndex(lt), le);
				wBool_t rc = TrimTrack(lt, le, maxX, nearest_pos, nearest_angle, nearest_radius, nearest_center);
				AuditTracks("addTurnout T%d[%d] after trimming L%d[%d]", GetTrkIndex(newTrk), i, GetTrkIndex(lt), le);

			}
		}
	}

	SetDescriptionOrig(newTrk);
	xx->descriptionOff = zero;
	xx->descriptionSize = zero;

	DrawNewTrack(newTrk);

	AuditTracks("addTurnout T%d returns", GetTrkIndex(newTrk));
	UndoEnd();
	Dto.state = 0;
	Dto.trk = NULL;
	Dto.angle = 0.0;
}


static void TurnoutRotate(void* pangle)
{
	if (Dto.state == 0)
		return;
	ANGLE_T angle = (ANGLE_T)VP2L(pangle);
	angle /= 1000.0;
	Dto.pos = cmdMenuPos;
	Rotate(&Dto.pos, cmdMenuPos, angle);
	Dto.angle += angle;
	TempRedraw(); // TurnoutRotate
}

static dynArr_t anchors_da;
#define anchors(N) DYNARR_N(trkSeg_t,anchors_da,N)

void static CreateArrowAnchor(coOrd pos, ANGLE_T a, DIST_T len) {
	DYNARR_APPEND(trkSeg_t, anchors_da, 1);
	int i = anchors_da.cnt - 1;
	anchors(i).type = SEG_STRLIN;
	anchors(i).width = 0;
	anchors(i).u.l.pos[0] = pos;
	Translate(&anchors(i).u.l.pos[1], pos, NormalizeAngle(a + 135), len);
	anchors(i).color = wDrawColorBlue;
	DYNARR_APPEND(trkSeg_t, anchors_da, 1);
	i = anchors_da.cnt - 1;
	anchors(i).type = SEG_STRLIN;
	anchors(i).width = 0;
	anchors(i).u.l.pos[0] = pos;
	Translate(&anchors(i).u.l.pos[1], pos, NormalizeAngle(a - 135), len);
	anchors(i).color = wDrawColorBlue;
}

void static CreateRotateAnchor(coOrd pos) {
	DIST_T d = tempD.scale * 0.15;
	DYNARR_APPEND(trkSeg_t, anchors_da, 1);
	int i = anchors_da.cnt - 1;
	anchors(i).type = SEG_CRVLIN;
	anchors(i).width = 0.5;
	anchors(i).u.c.center = pos;
	anchors(i).u.c.a0 = 180.0;
	anchors(i).u.c.a1 = 360.0;
	anchors(i).u.c.radius = d * 2;
	anchors(i).color = wDrawColorAqua;
	coOrd head;					//Arrows
	for (int j = 0; j < 3; j++) {
		Translate(&head, pos, j * 120, d * 2);
		CreateArrowAnchor(head, NormalizeAngle((j * 120) + 90), d);
	}
}

void static CreateMoveAnchor(coOrd pos) {
	DYNARR_SET(trkSeg_t, anchors_da, anchors_da.cnt + 5);
	DrawArrowHeads(&DYNARR_N(trkSeg_t, anchors_da, anchors_da.cnt - 5), pos, 0, TRUE, wDrawColorBlue);
	DYNARR_SET(trkSeg_t, anchors_da, anchors_da.cnt + 5);
	DrawArrowHeads(&DYNARR_N(trkSeg_t, anchors_da, anchors_da.cnt - 5), pos, 90, TRUE, wDrawColorBlue);
}

/**
 * Process the mouse events for laying track.
 *
 * \param action IN event type
 * \param pos    IN mouse position
 * \return    		next state
 */

EXPORT STATUS_T CmdTurnoutAction(
	wAction_t action,
	coOrd pos)
{
	ANGLE_T angle;
	static BOOL_T validAngle;
	static ANGLE_T baseAngle;
	static coOrd origPos;
#ifdef NEWROTATE
	static ANGLE_T origAngle;
#endif

	switch (action & 0xFF) {

	case C_START:
		Dto.state = 0;
		Dto.trk = NULL;
		Dto.angle = 0.0;
		DYNARR_RESET(trkSeg_t, anchors_da);
		SetAllTrackSelect(FALSE);
		return C_CONTINUE;

	case wActionMove:
		DYNARR_RESET(trkSeg_t, anchors_da);
		if (Dto.state && (MyGetKeyState() & WKEY_CTRL)) {
			CreateRotateAnchor(pos);
		}
		else {
			CreateMoveAnchor(pos);
		}
		return C_CONTINUE;
		break;
	case C_DOWN:
		DYNARR_RESET(trkSeg_t, anchors_da);
		if (curTurnout == NULL) return C_CONTINUE;
		PlaceTurnout(pos, NULL);
		Dto.state = 1;
		return C_CONTINUE;

	case C_MOVE:
		DYNARR_RESET(trkSeg_t, anchors_da);
		if (curTurnout == NULL) return C_CONTINUE;
		if (curTurnoutEp >= (long)curTurnout->endCnt)
			curTurnoutEp = 0;
		Dto.state = 1;
		PlaceTurnout(pos, Dto.trk);
		return C_CONTINUE;

	case C_UP:
		DYNARR_RESET(trkSeg_t, anchors_da);
		InfoMessage(_("Left-Drag to place, Ctrl+Left-Drag or Right-Drag to Rotate, Space or Enter to accept, Esc to Cancel"));
		return C_CONTINUE;

	case C_RDOWN:
		DYNARR_RESET(trkSeg_t, anchors_da);
		if (curTurnout == NULL) return C_CONTINUE;
		if (Dto.state == 0) {
			Dto.pos = pos;      // If first, use pos, otherwise use current
			LOG(log_turnout, 1, ("RDOWN @ %0.3fx%0.3f\n", Dto.pos.x, Dto.pos.y));
		}
		Dto.rot0 = Dto.rot1 = pos;
		Dto.state = 2;
		origPos = Dto.pos;
#ifdef NEWROTATE
		origAngle = Dto.angle;
#else
		Rotate(&origPos, Dto.rot0, -(Dto.angle + curTurnout->endPt[(int)curTurnoutEp].angle));
#endif
		validAngle = FALSE;
		return C_CONTINUE;

	case C_RMOVE:
		DYNARR_RESET(trkSeg_t, anchors_da);
		if (curTurnout == NULL) return C_CONTINUE;
		Dto.rot1 = pos;
		if (FindDistance(Dto.rot0, Dto.rot1) > 0.1 * mainD.scale) {
			angle = FindAngle(Dto.rot0, Dto.rot1);
			if (!validAngle) {
				baseAngle = angle/* - Dto.angle*/;
				validAngle = TRUE;
			}
			Dto.pos = origPos;
			LOG(log_turnout, 1, ("RMOVE pre @ %0.3fx%0.3f\n", Dto.pos.x, Dto.pos.y));
#ifdef NEWROTATE
			angle -= baseAngle;
			Dto.angle = NormalizeAngle(origAngle + angle);
#else
			angle += 180.0;
			Dto.angle = angle - curTurnout->endPt[(int)curTurnoutEp].angle;
#endif
			Rotate(&Dto.pos, Dto.rot0, angle);
			LOG(log_turnout, 1, ("RMOVE post @ %0.3fx%0.3f\n", Dto.pos.x, Dto.pos.y));
		}
		FormatCompoundTitle(listLabels, curTurnout->title);
		InfoMessage(_("Angle = %0.3f (%s)"), PutAngle(NormalizeAngle(Dto.angle + 90.0)), message);
		Dto.state = 2;
		return C_CONTINUE;

	case C_RUP:
		DYNARR_RESET(trkSeg_t, anchors_da);
		if (curTurnout == NULL) return C_CONTINUE;
		Dto.state = 1;
		InfoMessage(_("Left-Drag to place, Ctrl+Left-Drag or Right-Drag to Rotate, Space or Enter to accept, Esc to Cancel"));
		return C_CONTINUE;

	case C_LCLICK:
		DYNARR_RESET(trkSeg_t, anchors_da);
		if (curTurnout == NULL) return C_CONTINUE;
		if (MyGetKeyState() & WKEY_SHIFT) {
			angle = curTurnout->endPt[(int)curTurnoutEp].angle;
			curTurnoutEp++;
			if (curTurnoutEp >= (long)curTurnout->endCnt)
				curTurnoutEp = 0;
			if (Dto.trk == NULL)
				Dto.angle = NormalizeAngle(Dto.angle + (angle - curTurnout->endPt[(int)curTurnoutEp].angle));
			PlaceTurnout(Dto.place, Dto.trk);
		}
		else {
			CmdTurnoutAction(C_DOWN, pos);
			CmdTurnoutAction(C_UP, pos);
		}
		return C_CONTINUE;

	case C_REDRAW:
		wSetCursor(mainD.d, defaultCursor);
		if (Dto.state) {
			DrawSegs(&tempD, Dto.pos, Dto.angle,
				curTurnout->segs, curTurnout->segCnt, trackGauge, selectedColor);
		}
		if (anchors_da.cnt > 0) {
			DrawSegs(&tempD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack);
			wSetCursor(mainD.d, wCursorNone);
		}
		if (Dto.state == 2)
			DrawLine(&tempD, Dto.rot0, Dto.rot1, 0, wDrawColorBlue);
		return C_CONTINUE;

	case C_CANCEL:
		DYNARR_RESET(trkSeg_t, anchors_da);
		Dto.state = 0;
		Dto.trk = NULL;
		/*wHide( newTurn.reg.win );*/
		return C_TERMINATE;

	case C_TEXT:
		if ((action >> 8) != ' ')
			return C_CONTINUE;
		/*no break*/
	case C_OK:
		DYNARR_RESET(trkSeg_t, anchors_da);
		AddTurnout();
		Dto.state = 0;
		Dto.trk = NULL;
		return C_TERMINATE;

	case C_FINISH:
		DYNARR_RESET(trkSeg_t, anchors_da);
		if (Dto.state != 0 && Dto.trk != NULL)
			CmdTurnoutAction(C_OK, pos);
		else
			CmdTurnoutAction(C_CANCEL, pos);
		return C_TERMINATE;

	case C_CMDMENU:
		menuPos = pos;
		wMenuPopupShow(turnoutPopupM);
		return C_CONTINUE;

	default:
		return C_CONTINUE;
	}
}


#ifdef TURNOUTCMD
static STATUS_T CmdTurnout(
	wAction_t action,
	coOrd pos)
{
	wIndex_t turnoutIndex;
	turnoutInfo_t* turnoutPtr;

	switch (action & 0xFF) {

	case C_START:
		if (turnoutW == NULL) {
			/*			turnoutW = ParamCreateDialog( &turnoutPG, MakeWindowTitle("Turnout"), "Ok", , (paramActionCancelProc)Reset, TRUE, NULL, F_RESIZE|F_RECALLSIZE, TurnoutDlgUpdate ); */
			turnoutW = ParamCreateDialog(&turnoutPG, MakeWindowTitle(_("Turnout")), _("Close"), (paramActionOkProc)TurnoutOk, wHide, TRUE, NULL, F_RESIZE | F_RECALLSIZE | PD_F_ALT_CANCELLABEL, TurnoutDlgUpdate);
			InitNewTurn(turnoutNewM);
		}
		/*		ParamDialogOkActive( &turnoutPG, FALSE ); */
		turnoutIndex = wListGetIndex(turnoutListL);
		turnoutPtr = curTurnout;
		wShow(turnoutW);
		TurnoutChange(CHANGE_PARAMS | CHANGE_SCALE);
		if (curTurnout == NULL) {
			NoticeMessage2(0, MSG_TURNOUT_NO_TURNOUT, _("Ok"), NULL);
			return C_TERMINATE;
		}
		if (turnoutIndex > 0 && turnoutPtr) {
			curTurnout = turnoutPtr;
			wListSetIndex(turnoutListL, turnoutIndex);
			RedrawTurnout( turnoutD.d, NULL, 0, 0 );
		}
		InfoMessage(_("Pick turnout and active End Point, then place on the layout"));
		ParamLoadControls(&turnoutPG);
		ParamGroupRecord(&turnoutPG);
		SetAllTrackSelect(FALSE);
		return CmdTurnoutAction(action, pos);

	case wActionMove:
		return CmdTurnoutAction(action, pos);

	case C_DOWN:
	case C_RDOWN:
		ParamDialogOkActive(&turnoutPG, TRUE);
		if (hideTurnoutWindow)
			wHide(turnoutW);
		if (((action & 0xFF) == C_DOWN) && (MyGetKeyState() & WKEY_CTRL))
			return CmdTurnoutAction(C_RDOWN, pos);     //Convert CTRL into Right
		/*no break*/
	case C_MOVE:
		if (MyGetKeyState() & WKEY_CTRL)
			return CmdTurnoutAction(C_RMOVE, pos);
		/*no break*/
	case C_RMOVE:
		return CmdTurnoutAction(action, pos);

	case C_UP:
	case C_RUP:
		if (hideTurnoutWindow)
			wShow(turnoutW);

		InfoMessage(_("Left-Drag to place, Ctrl+Left-Drag or Right-Drag to Rotate, Space or Enter to accept, Esc to Cancel"));
		if (((action & 0xFF) == C_UP) && (MyGetKeyState() & WKEY_CTRL))
			return CmdTurnoutAction(C_RUP, pos);
		return CmdTurnoutAction(action, pos);

	case C_LCLICK:
		CmdTurnoutAction(action, pos);
		HilightEndPt();
		return C_CONTINUE;

	case C_CANCEL:
		wHide(turnoutW);
		return CmdTurnoutAction(action, pos);
	case C_TEXT:
		CmdTurnoutAction(action, pos);
		return C_CONTINUE;
	case C_OK:
	case C_FINISH:
	case C_CMDMENU:
	case C_REDRAW:
		return CmdTurnoutAction(action, pos);

	default:
		return C_CONTINUE;
	}
}

#endif

/**
 * Event procedure for the hotbar.
 *
 * \param op   IN requested function
 * \param data IN	pointer to info on selected element
 * \param d    IN
 * \param origP IN
 * \return
 */

static char* CmdTurnoutHotBarProc(
	hotBarProc_e op,
	void* data,
	drawCmd_p d,
	coOrd* origP)
{
	turnoutInfo_t* to = (turnoutInfo_t*)data;
	switch (op) {
	case HB_SELECT:		/* new element is selected */
		CmdTurnoutAction(C_FINISH, zero); 		/* finish current operation */
		curTurnout = to;
		DoCommandB(I2VP(turnoutHotBarCmdInx)); /* continue with new turnout / structure */
		return NULL;
	case HB_LISTTITLE:
		FormatCompoundTitle(listLabels, to->title);
		if (message[0] == '\0')
			FormatCompoundTitle(listLabels | LABEL_DESCR, to->title);
		return message;
	case HB_BARTITLE:
		FormatCompoundTitle(hotBarLabels << 1, to->title);
		return message;
	case HB_FULLTITLE:
		return to->title;
	case HB_DRAW:
		DrawSegs(d, *origP, 0.0, to->segs, to->segCnt, trackGauge, wDrawColorBlack);
		return NULL;
	}
	return NULL;
}


EXPORT void AddHotBarTurnouts(void)
{
	wIndex_t inx;
	turnoutInfo_t* to;
	for (inx = 0; inx < turnoutInfo_da.cnt; inx++) {
		to = turnoutInfo(inx);
		if (!(IsParamValid(to->paramFileIndex) &&
			to->segCnt > 0 &&
			(FIT_NONE != CompatibleScale(FIT_TURNOUT, to->scaleInx, GetLayoutCurScale()))))
			continue;
		AddHotBarElement(to->contentsLabel, to->size, to->orig, TRUE, FALSE, to->barScale, to, CmdTurnoutHotBarProc);
	}
}

/**
 * Handle mouse events for laying track when initiated from hotbar.
 *
 * \param action IN mouse event type
 * \param pos IN mouse position
 * \return    next state of operation
 */

static STATUS_T CmdTurnoutHotBar(
	wAction_t action,
	coOrd pos)
{

	switch (action & 0xFF) {

	case C_START:
		//TurnoutChange( CHANGE_PARAMS|CHANGE_SCALE );
		if (curTurnout == NULL) {
			NoticeMessage2(0, MSG_TURNOUT_NO_TURNOUT, _("Ok"), NULL);
			return C_TERMINATE;
		}
		FormatCompoundTitle(listLabels | LABEL_DESCR, curTurnout->title);
		InfoMessage(_("Place %s and draw into position"), message);
		wIndex_t listIndex = FindListItemByContext(turnoutListL, curTurnout);
		if (listIndex >= 0)
			turnoutInx = listIndex;
		ParamLoadControls(&turnoutPG);
		ParamGroupRecord(&turnoutPG);
		return CmdTurnoutAction(action, pos);

	case wActionMove:
		return CmdTurnoutAction(action, pos);

	case C_DOWN:
		if (MyGetKeyState() & WKEY_CTRL) {
			return CmdTurnoutAction(C_RDOWN, pos);
		}
		/*no break*/
	case C_RDOWN:
		return CmdTurnoutAction(action, pos);

	case C_MOVE:
		if (MyGetKeyState() & WKEY_CTRL) {
			return CmdTurnoutAction(C_RMOVE, pos);
		}
		/*no break*/
	case C_RMOVE:
		return CmdTurnoutAction(action, pos);

	case C_UP:
		if (MyGetKeyState() & WKEY_CTRL) {
			return CmdTurnoutAction(C_RUP, pos);
		}
		/*no break*/
	case C_RUP:
		InfoMessage(_("Left-Drag to place, Ctrl+Left-Drag or Right-Drag to Rotate, Space or Enter to accept, Esc to Cancel"));
		return CmdTurnoutAction(action, pos);

	case C_REDRAW:
		return CmdTurnoutAction(action, pos);
	case C_TEXT:
		if ((action >> 8) != ' ')
			return C_CONTINUE;
		/* no break*/
	case C_OK:
		CmdTurnoutAction(action, pos);
		return C_CONTINUE;

	case C_CANCEL:
		HotBarCancel();
		/*no break*/
	default:
		return CmdTurnoutAction(action, pos);
	}
}

#ifdef TURNOUTCMD
#include "bitmaps/turnout.xpm"


EXPORT void InitCmdTurnout(wMenu_p menu)
{
	AddMenuButton(menu, CmdTurnout, "cmdTurnout", _("Predefined Track"), wIconCreatePixMap(turnout_xpm[iconSize]), LEVEL0_50, IC_WANT_MOVE | IC_STICKY | IC_LCLICK | IC_CMDMENU | IC_POPUP2, ACCL_TURNOUT, NULL);
	turnoutHotBarCmdInx = AddMenuButton(menu, CmdTurnoutHotBar, "cmdTurnoutHotBar", "", NULL, LEVEL0_50, IC_WANT_MOVE | IC_STICKY | IC_LCLICK | IC_CMDMENU | IC_POPUP2, 0, NULL);
	RegisterChangeNotification(TurnoutChange);
	ParamRegister(&turnoutPG);
	log_turnout = LogFindIndex("turnout");
	log_traverseTurnout = LogFindIndex("traverseTurnout");
	log_suppressCheckPaths = LogFindIndex("suppresscheckpaths");
	log_splitturnout = LogFindIndex("splitturnout");
	if (turnoutPopupM == NULL) {
		turnoutPopupM = MenuRegister("Turnout Rotate");
		AddRotateMenu(turnoutPopupM, TurnoutRotate);
	}
}
#endif

EXPORT void InitTrkTurnout(void)
{
	T_TURNOUT = InitObject(&turnoutCmds);

	/*InitDebug( "Turnout", &debugTurnout );*/
	AddParam("TURNOUT ", ReadTurnoutParam);
}

#ifdef TEST

wDrawable_t turnoutD;

void wListAddValue(wList_p bl, char* val, wIcon_p, void* listData, void* itemData)
{
}

void wListClear(wList_p bl)
{
}

void wDrawSetScale(wDrawable_p d)
{
	d->scale = 1.0;
}

void wDrawClear(wDrawable_p d)
{
}

void GetTrkCurveCenter(track_p t, coOrd* pos, DIST_T* radius)
{
}

#ifdef NOTRACK_C

track_p NewTrack(wIndex_t index, TRKTYP_T type, EPINX_T endCnt, SIZE_T extraSize)
{
	return NULL;
}

track_p OnTrack(coOrd* pos)
{
	return NULL;
}

void ErrorMessage(char* msg, ...)
{
	lprintf("ERROR : %s\n", msg);
}

void DeleteTrack(track_p t)
{
}

void ConnectTracks(track_p t0, EPINX_T ep0, track_p t1, EPINX_T ep1)
{
}
#endif

main(INT_T argc, char* argv[])
{
	FILE* f;
	char line[STR_SIZE];
	wIndex_t lineCnt = 0;

	/*debugTurnout = 3;*/
	if ((f = fopen("turnout.params", "r")) == NULL) {
		Perror("turnout.params");
		Exit(1);
	}
	while (fgets(line, sizeof line, f) != NULL) {
		lineCnt++;
		ReadTurnoutParam(&lineCnt);
	}
}
#endif
