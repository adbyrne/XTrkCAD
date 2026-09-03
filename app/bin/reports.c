/** \file reports.c
 * Native "Reports" feature (SF #217), phase 1: a generic report-text
 * viewer (Save/Print/Print Setup, same shape as denum.c's "Parts List"
 * dialog -- see the phase-1 implementation plan for why this is built
 * generic from the start, meant for every future report phase to reuse)
 * plus the phase-1 report itself, unconnected track endpoints.
 *
 * Phase 1.5 (SF #772, folded into the same ticket -- see the phase-1.5
 * implementation plan) adds interactive click-to-navigate: selecting a row
 * in the (new, alongside the original text view) list control pans the
 * main canvas to that endpoint and draws a transient open-circle
 * indicator. The indicator is a pure draw-overlay (drawn each redraw from
 * draw.c's DrawTempContent(), see ReportsDrawIndicator()) -- never a real
 * track object, so it can never end up in a saved .xtc/.xtce file and
 * needs no undo/redo handling.
 *
 * The date-header logic (AddReportDateString()) is deliberately duplicated
 * from denum.c's AddDateString() rather than shared, to keep this
 * feature's first PR scoped to new files only -- see the phase-1
 * implementation plan's "Shared helper duplication" section for the
 * reasoning and the follow-up cleanup this leaves flagged (extract to
 * app/dynstring/).
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
#include <time.h>

#include "custom.h"
#include <dynstring.h>
#include "draw.h"
#include "fileio.h"
#include "layout.h"
#include "form.h"
#include "paths.h"
#include "scale.h"
#include "track.h"
#include "include/reports.h"

static wControl_p reportsW;

#define REPORTSOP_SAVE  (1)
#define REPORTSOP_PRINT (2)

static void DoReportsOp(void *data);
static void ReportsDlgUpdate(paramGroup_p pg, int inx, void *valueP);
static void ReportsCancel(paramGroup_p pg);

static wWinPix_t reportsListWidths[] = { 60, 80, 80, 70 };
static const char * reportsListTitles[] = {
	N_("Track"), N_("X"), N_("Y"), N_("Angle")
};
static paramListData_t reportsListData = { 8, 300, 4, reportsListWidths, reportsListTitles };

static paramTextData_t reportsTextData = { 0, 0 };
static paramData_t reportsPLs[] = {
#define I_REPORTSLIST (0)
#define reportsList (reportsPLs[I_REPORTSLIST].control)
	{ PD_LIST, NULL, "list", PDO_DLGRESIZE, &reportsListData, NULL, 0 },
#define I_REPORTSTEXT (1)
#define reportsT (reportsPLs[I_REPORTSTEXT].control)
	{ PD_TEXT, NULL, "text", PDO_DLGRESIZE, &reportsTextData },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, I2VP(REPORTSOP_SAVE) },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, I2VP(REPORTSOP_PRINT) },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
};
static paramGroup_t reportsPG = { "reports", PGO_FULLDIALOGFROMBUILDER, reportsPLs, COUNT( reportsPLs ) };

static struct wFilSel_t * reportsFile_fs;

/** The current report's rows, kept alive for as long as the dialog might
 * still reference them via the list's per-row context pointers (phase
 * 1.5) -- unlike a plain local, this must outlive the compute function
 * that builds it. Replaced (old rows freed first) each time a report is
 * (re)generated; see ReportsUnconnectedEndpoints(). */
static dynArr_t reportsCurrentList_da;

/** Phase 1.5 interactive-navigation indicator state -- see
 * ReportsSetIndicator()/ReportsClearIndicator()/ReportsDrawIndicator(). */
static BOOL_T reportIndicatorActive = FALSE;
static coOrd reportIndicatorPos;
static SCALEINX_T reportIndicatorScale;

static int DoReportsSave(
        int files,
        char **fileName,
        void * data )
{
	CHECK( fileName != NULL );
	CHECK( files == 1 );

	SetCurrentPath( REPORTPATHKEY, fileName[0] );
	return wTextSave( reportsT, fileName[ 0 ] );
}

static void DoReportsOp(
        void * data )
{
	switch( VP2L(data) ) {
	case REPORTSOP_SAVE:
		wFilSelect( reportsFile_fs, GetCurrentPath(REPORTPATHKEY) );
		break;
	case REPORTSOP_PRINT:
		wTextPrint( reportsT );
		break;
	default:
		break;
	}
}

/** Duplicated from denum.c's AddDateString() -- see file header. */
static void AddReportDateString(DynString* output)
{
	struct tm tm;
	time_t currentTime;
	size_t length = 8;
	char* formatted = malloc(length);

	time(&currentTime);
#ifdef WINDOWS
	localtime_s(&tm, &currentTime);
#else
	localtime_r(&currentTime, &tm);
#endif

	while (!strftime(formatted, length, "%x\n\n", &tm)) {
		char *tmp;
		length *= 2;
		tmp = realloc(formatted, length);
		if (!tmp) {
			free(formatted);
			return;
		}
		formatted = tmp;
	}

	DynStringCatCStr(output, formatted);

	free(formatted);
}

/**
 * Pan the main canvas so `pos` is centered, then redraw. New in phase 1.5
 * -- no prior "center on a point" primitive existed anywhere in the app
 * (checked); this is deliberately minimal (re-centers at the current zoom
 * level, does not change scale).
 *
 * \param[in] pos the layout coordinate to center on
 */
static void ReportsCenterOn(coOrd pos)
{
	mainD.orig.x = pos.x - mainD.size.x / 2.0;
	mainD.orig.y = pos.y - mainD.size.y / 2.0;
	MainRedraw();
}

/**
 * Activate the phase-1.5 indicator at `pos` (for the given track's scale,
 * which sets the indicator's radius) and pan the canvas to it.
 *
 * \param[in] pos position to indicate and center on
 * \param[in] scale the originating track's scale (GetTrkScale()) -- the
 *            indicator radius is scale-relative, not a fixed pixel size
 */
static void ReportsSetIndicator(coOrd pos, SCALEINX_T scale)
{
	reportIndicatorActive = TRUE;
	reportIndicatorPos = pos;
	reportIndicatorScale = scale;
	ReportsCenterOn(pos);
}

/**
 * Deactivate the phase-1.5 indicator, if one is active, and redraw so it
 * actually disappears immediately rather than lingering until some other
 * redraw happens to occur.
 */
static void ReportsClearIndicator(void)
{
	if (!reportIndicatorActive) {
		return;
	}
	reportIndicatorActive = FALSE;
	MainRedraw();
}

void ReportsDrawIndicator(void *dArg)
{
	drawCmd_p d = (drawCmd_p)dArg;
	DIST_T radius;

	if (!reportIndicatorActive) {
		return;
	}

	radius = 2.0 * GetScaleTrackGauge(reportIndicatorScale);
	DrawArc( d, reportIndicatorPos, radius, 0.0, 360.0, FALSE, 0,
	         reportIndicatorColor );
}

/** paramGroup_t changeProc (phase 1.5): fires whenever any reportsPLs[]
 * control changes; only the list selection (I_REPORTSLIST) is acted on.
 * Matches the pg/inx/valueP-dispatch pattern denum.c's EnumDlgUpdate()
 * already uses for the same kind of "one shared changeProc, several
 * controls" dialog. */
static void ReportsDlgUpdate(paramGroup_p pg, int inx, void *valueP)
{
	wIndex_t sel;
	reportsEndPt_t *entry;

	if (inx != I_REPORTSLIST) {
		return;
	}

	sel = wListGetIndex(reportsList);
	if (sel < 0) {
		return;
	}
	entry = (reportsEndPt_t *)wListGetItemContext(reportsList, sel);
	if (!entry) {
		return;
	}

	ReportsSetIndicator(entry->pos, entry->scale);
}

/** paramActionCancelProc (phase 1.5): clear the indicator before the
 * dialog's normal cancel/close handling, so it doesn't linger on the
 * canvas after the report window is gone (matches the original design
 * note: the indicator is deleted when the report window closes). */
static void ReportsCancel(paramGroup_p pg)
{
	ReportsClearIndicator();
	FormCancel_Current(pg);
}

void ReportsShowText(const char *title, DynString *content)
{
	if (reportsW == NULL) {
		FormRegister( &reportsPG );
		reportsW = FormCreateDialog( &reportsPG, MakeWindowTitle(title),
		                             NULL, NULL,
		                             NULL, ReportsCancel,
		                             TRUE, F_RESIZE,
		                             ReportsDlgUpdate );
		reportsFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, title,
		                                sReportsFilePattern, DoReportsSave, NULL );
	}
	/* NOTE: subsequent calls reuse the same window/title -- fine while
	 * phase 1 is the only report; retitling on reuse becomes relevant
	 * once phase 2 adds a second report type, deliberately not solved
	 * here (see the phase-1 implementation plan). */

	wTextClear( reportsT );
	wTextAppend( reportsT, DynStringToCStr(content) );
	wTextSetPosition( reportsT, 0 );

	FormLoadControls( &reportsPG );
	wShow( reportsW );
}

/** Header text shared by every report: product/layout title + date.
 * Report-specific content is appended by the caller. */
static void ReportsAddHeader(DynString *out, const char *reportTitle)
{
	DynStringPrintf(out, "%s %s\n", sProdName, reportTitle);

	if (*GetLayoutTitle()) {
		DynStringCatCStrs(out, GetLayoutTitle(), "\n", NULL);
	}
	if (*GetLayoutSubtitle()) {
		DynStringCatCStrs(out, GetLayoutSubtitle(), "\n", NULL);
	}

	AddReportDateString(out);
}

/** Populate the interactive list (phase 1.5) from reportsCurrentList_da --
 * one row per entry, tab-separated (wListAddValue() splits on tabs into
 * columns), each row's context set to that entry's address so
 * ReportsDlgUpdate() can recover it on selection. Must run after
 * ReportsShowText() has run at least once (the list control doesn't exist
 * before the dialog's first creation). */
static void ReportsPopulateList(void)
{
	int i;
	char row[128];

	wListClear( reportsList );
	for ( i = 0; i < reportsCurrentList_da.cnt; i++ ) {
		reportsEndPt_t *entry = &DYNARR_N(reportsEndPt_t, reportsCurrentList_da, i);
		snprintf( row, sizeof row, "%d\t%.3f\t%.3f\t%.3f",
		          entry->trackId, entry->pos.x, entry->pos.y, entry->angle );
		wListAddValue( reportsList, row, NULL, entry );
	}
}

void ReportsUnconnectedEndpoints( void * unused )
{
	track_p trk;
	DynString content;

	ReportsClearIndicator();
	DYNARR_FREE( reportsEndPt_t, reportsCurrentList_da );
	DYNARR_INIT( reportsEndPt_t, reportsCurrentList_da );

	TRK_ITERATE( trk ) {
		EPINX_T ep;
		for ( ep = 0; ep < GetTrkEndPtCnt(trk); ep++ ) {
			if ( GetTrkEndTrk(trk, ep) == NULL ) {
				reportsEndPt_t *entry;
				DYNARR_APPEND( reportsEndPt_t, reportsCurrentList_da, 10 );
				entry = &DYNARR_LAST( reportsEndPt_t, reportsCurrentList_da );
				entry->trackId = GetTrkIndex(trk);
				entry->pos = GetTrkEndPos(trk, ep);
				entry->angle = GetTrkEndAngle(trk, ep);
				entry->scale = GetTrkScale(trk);
			}
		}
	}

	DynStringMalloc( &content, 256 );
	ReportsAddHeader( &content, _("Unconnected Endpoints Report") );

	if ( reportsCurrentList_da.cnt == 0 ) {
		DynStringCatCStrs( &content, "\n", _("No unconnected endpoints found."), "\n",
		                   NULL );
	} else {
		/* snprintf into a local buffer, then append -- DynStringPrintf()
		 * formats INTO its target starting at offset 0 (replacing whatever
		 * was already there, same as plain sprintf into a fresh buffer),
		 * it does not append. Calling it directly on `content` here would
		 * silently wipe out the header ReportsAddHeader() just wrote --
		 * exactly the bug this comment is here to stop someone
		 * reintroducing. Width 6 for the Track column matches
		 * ReportsFormatUnconnectedList()'s "%6d" exactly, so header and
		 * data rows stay aligned. */
		char headerLine[64];
		snprintf( headerLine, sizeof headerLine, "\n%6s | %8s | %8s | %7s\n",
		          _("Track"), _("X"), _("Y"), _("Angle") );
		DynStringCatCStr( &content, headerLine );
		ReportsFormatUnconnectedList( &content, &DYNARR_N(reportsEndPt_t,
		                              reportsCurrentList_da, 0),
		                              reportsCurrentList_da.cnt );
	}

	ReportsShowText( _("Unconnected Endpoints Report"), &content );
	ReportsPopulateList();

	DynStringFree( &content );
}
