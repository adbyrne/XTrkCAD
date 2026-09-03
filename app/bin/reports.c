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

/** The hidden control that backs Save/Print (ReportsRefreshPrintText()) --
 * deliberately NOT a paramData_t/PD_TEXT entry in reportsPLs[] below, so it
 * isn't tied to reportsW's builder-defined dialog. Created once, lazily, in
 * ReportsShowText() via wTextCreate()'s genuinely standalone/no-.ui-needed
 * code path (text.c) against its own permanently-unshown backing window --
 * see that call site for why. */
static wControl_p reportsT;

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

static paramData_t reportsPLs[] = {
#define I_REPORTSLIST (0)
#define reportsList (reportsPLs[I_REPORTSLIST].control)
	{ PD_LIST, NULL, "list", PDO_DLGRESIZE, &reportsListData, NULL, 0 },
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

static void ReportsBuildText(DynString *out);

/** Refresh the hidden `reportsT` text control from the current report
 * (reportsCurrentList_da) right before Save or Print actually use it --
 * see reports.ui's comment on "scrollwindow"/"text" for why this control
 * exists at all despite never being shown. */
static void ReportsRefreshPrintText(void)
{
	DynString content;

	ReportsBuildText( &content );
	wTextClear( reportsT );
	wTextAppend( reportsT, DynStringToCStr(&content) );
	DynStringFree( &content );
}

static int DoReportsSave(
        int files,
        char **fileName,
        void * data )
{
	CHECK( fileName != NULL );
	CHECK( files == 1 );

	SetCurrentPath( REPORTPATHKEY, fileName[0] );
	ReportsRefreshPrintText();
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
		ReportsRefreshPrintText();
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
 * Sets mainD.orig directly (matching draw.c's own PanHere()'s exact same
 * centering math), then calls MainLayout() rather than a bare
 * MainRedraw() -- MainLayout() is what actually keeps everything in sync
 * after mainD.orig changes: it re-syncs tempD.orig/tempD.size to match
 * (the surface ReportsDrawIndicator() draws into -- without this the
 * first attempt's indicator was invisible until some unrelated event,
 * e.g. refocusing the window, forced a real MainLayout() and resynced
 * tempD for us) and calls MapDrawBoundingBox() (the first attempt also
 * missed this -- the Map window's viewport-position indicator never
 * moved). A bare MainRedraw() only repaints; it does neither.
 *
 * \param[in] pos the layout coordinate to center on
 */
static void ReportsCenterOn(coOrd pos)
{
	mainD.orig.x = pos.x - mainD.size.x / 2.0;
	mainD.orig.y = pos.y - mainD.size.y / 2.0;
	MainLayout( TRUE, FALSE );
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

void ReportsShowText(const char *title)
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
		/* reportsT backs Save/Print only -- it's never shown to the user
		 * (see ReportsRefreshPrintText()) -- so instead of creating it as
		 * a .ui-bound PD_TEXT control and then fighting wTextCreate()'s
		 * unconditional gtk_widget_show_all() (an earlier version of this
		 * code did exactly that: a real screenshot caught it leaving an
		 * empty visible box, worked around with wControlShow(FALSE)),
		 * give it its own backing window that's simply never wShow()n.
		 * "Never shown" needs an explicit push, though --
		 * wWinDialogCreate() (dialog.c) unconditionally calls
		 * gtk_widget_show() on itself at the end of its own construction
		 * (the same style of gotcha as wTextCreate()'s force-show,
		 * confirmed the same way: a real backing window DID appear on
		 * screen as a small stray window before this wControlShow() line
		 * was added). This also means reportsT is created via
		 * wTextCreate()'s standalone/no-.ui-needed code path (parent
		 * lacks F_DEFINEDINBUILDER) -- otherwise unexercised anywhere in
		 * this codebase (checked: not even wlib/test/testapp.c calls
		 * wTextCreate() against a non-builder parent). */
		{
			wControl_p reportsTBacking = wWinDialogCreate( NULL, NULL, NULL,
			                             "reportstextbacking", 0L, NULL, NULL );
			wControlShow( reportsTBacking, FALSE );
			reportsT = wTextCreate( reportsTBacking, 0, 0, NULL, NULL,
			                        BO_READONLY, 0, 0 );
		}
	}
	/* NOTE: subsequent calls reuse the same window/title -- fine while
	 * phase 1 is the only report; retitling on reuse becomes relevant
	 * once phase 2 adds a second report type, deliberately not solved
	 * here (see the phase-1 implementation plan).
	 *
	 * Deliberately does NOT touch reportsT here -- unlike phase 1's
	 * original design, the hidden text control is no longer kept in sync
	 * eagerly every time a report is shown; it's built fresh only when
	 * Save/Print are actually clicked (ReportsRefreshPrintText()), so it
	 * always reflects the current report without needing to be rebuilt on
	 * every regeneration whether or not it's ever used. */

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

/** Build the full formatted report text (header + column-aligned rows)
 * fresh from reportsCurrentList_da. Used only by ReportsRefreshPrintText()
 * -- i.e. only when Save or Print is actually clicked -- so it always
 * reflects whatever the list is currently showing, never a stale copy
 * from an earlier ReportsUnconnectedEndpoints() call. */
static void ReportsBuildText(DynString *out)
{
	DynStringMalloc( out, 256 );
	ReportsAddHeader( out, _("Unconnected Endpoints Report") );

	if ( reportsCurrentList_da.cnt == 0 ) {
		DynStringCatCStrs( out, "\n", _("No unconnected endpoints found."), "\n",
		                   NULL );
	} else {
		/* snprintf into a local buffer, then append -- DynStringPrintf()
		 * formats INTO its target starting at offset 0 (replacing whatever
		 * was already there, same as plain sprintf into a fresh buffer),
		 * it does not append. Calling it directly on `out` here would
		 * silently wipe out the header ReportsAddHeader() just wrote --
		 * exactly the bug this comment is here to stop someone
		 * reintroducing. Width 6 for the Track column matches
		 * ReportsFormatUnconnectedList()'s "%6d" exactly, so header and
		 * data rows stay aligned. */
		char headerLine[64];
		snprintf( headerLine, sizeof headerLine, "\n%6s | %8s | %8s | %7s\n",
		          _("Track"), _("X"), _("Y"), _("Angle") );
		DynStringCatCStr( out, headerLine );
		ReportsFormatUnconnectedList( out, &DYNARR_N(reportsEndPt_t,
		                              reportsCurrentList_da, 0),
		                              reportsCurrentList_da.cnt );
	}
}

void ReportsUnconnectedEndpoints( void * unused )
{
	track_p trk;

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

	ReportsShowText( _("Unconnected Endpoints Report") );
	ReportsPopulateList();
}
