/** \file reports.c
 * Native "Reports" feature (SF #217), phase 1: a generic report viewer
 * (Refresh/Save/Print/Print Setup, same button shape as denum.c's "Parts
 * List" dialog -- see the phase-1 implementation plan for why this is built
 * generic from the start, meant for every future report phase to reuse)
 * plus the phase-1 report itself, unconnected track endpoints.
 *
 * Phase 1.5 (SF #772, folded into the same ticket -- see the phase-1.5
 * implementation plan) adds interactive click-to-navigate: selecting a row
 * in the visible PD_LIST control pans the main canvas to that endpoint and
 * draws a transient open-circle indicator. The indicator is a pure
 * draw-overlay (drawn each redraw from draw.c's DrawTempContent(), see
 * ReportsDrawIndicator()) -- never a real track object, so it can never end
 * up in a saved .xtc/.xtce file and needs no undo/redo handling.
 *
 * The list is also grouped/flagged for "Nearby" endpoints -- see
 * ReportsMarkNearby() -- and can be recomputed in place via the Refresh
 * button (`reportsDialog_t.refreshProc`) without closing/reopening the
 * dialog.
 *
 * Save/Print build their output on demand (ReportsBuildText()/
 * ReportsRefreshPrintText()) into a hidden PD_TEXT-equivalent control
 * (`reportsDialog_t.text`) that's never shown to the user -- see
 * ReportsShowDialog() for why it's created via wTextCreate()'s
 * standalone/no-.ui-needed code path rather than as a builder-bound
 * paramData_t entry.
 *
 * Phase 2 (track lengths/curve stats/turnout density/equipment
 * suitability -- see the phase-2 implementation plan) is report-only, no
 * click-to-navigate/indicator, and needs its own dialog per report (each
 * with a genuinely different PD_LIST column shape -- `reportsturnout.ui`
 * for this file's first phase-2 report, Turnout Density). This is the
 * point at which the phase-1-only plumbing above (originally hardcoded to
 * a single `reportsW`/`reportsT`/`reportsFile_fs`/`reportsRefreshProc` set
 * of module statics) was generalized into `reportsDialog_t` -- one
 * instance per report phase, all sharing the same `ReportsShowDialog()`/
 * `DoReportsOp()`/`ReportsRefreshPrintText()` machinery -- see
 * `reportsDialog_t`'s own doc comment for why this was deferred until a
 * second report type existed to prove the shape against, rather than
 * speculatively generalized in phase 1.
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
#include "dlayer.h"
#include "draw.h"
#include "fileio.h"
#include "layout.h"
#include "form.h"
#include "paths.h"
#include "scale.h"
#include "track.h"
#include "utility.h"
#include "include/reports.h"

/** Debug log category for manual/visual testing (`-d reports=1 -l <file>`)
 * -- covers report-open/recompute, row-click navigation, and toolbar
 * button presses. Lazily resolved via LogFindIndex(), same pattern as
 * every other file's `log_*` (see misc.c's `log_misc`). Added 2026-09-04
 * specifically so a session can tail a log file while a person clicks
 * through the report UI, without needing screen access. */
static int log_reports = -1;

#define REPORTSOP_SAVE    (1)
#define REPORTSOP_PRINT   (2)
#define REPORTSOP_REFRESH (3)

/** Bundles the per-report-phase state the shared viewer plumbing needs --
 * one instance per report phase (`reportsUnconnectedDlg` for phase 1,
 * `reportsTurnoutDlg` for phase 2's Turnout Density, etc.). Originally
 * this was all hardcoded to phase 1's own module statics
 * (`reportsW`/`reportsT`/`reportsFile_fs`/a single shared
 * `reportsRefreshProc`); phase 1's own implementation plan flagged
 * "retitling on reuse becomes relevant once phase 2 adds a second report
 * type, deliberately not solved here" -- this struct is that resolution,
 * written once a second report type actually existed to prove the shape
 * against rather than speculatively upfront. Each report phase gets its
 * own dialog/window (never retitled/reused across report types), so no
 * retitling logic is needed after all -- see ReportsShowDialog(). */
typedef struct {
	paramGroup_p pg;
	/** Dialog window, created lazily on first ReportsShowDialog() call. */
	wControl_p window;
	/** Hidden control backing Save/Print (ReportsRefreshPrintText()) --
	 * deliberately NOT a paramData_t/PD_TEXT entry in the report's own
	 * paramData_t[] array, so it isn't tied to that builder-defined
	 * dialog. Created once, lazily, in ReportsShowDialog() via
	 * wTextCreate()'s genuinely standalone/no-.ui-needed code path
	 * (text.c) against its own permanently-unshown backing window -- see
	 * that call site for why. */
	wControl_p text;
	struct wFilSel_t *fileSel;
	/** Builds this report's full Save/Print text fresh from its current
	 * rows (header + formatted body, e.g. ReportsBuildText()). */
	void (*buildText)(DynString *out);
	/** Recompute + repopulate this report in place -- what the shared
	 * Refresh button calls. Always the report's own menu-callback
	 * function (e.g. ReportsUnconnectedEndpoints()), since that always
	 * recomputes from scratch already. */
	void (*refreshProc)(void *);
	/** Row-selection changeProc, or NULL for a report-only phase with no
	 * click-to-navigate (phase 2's whole batch) -- passed straight
	 * through to FormCreateDialog(). */
	paramChangeProc changeProc;
	/** Dialog-cancel handler, or NULL to use the default
	 * FormCancel_Current (nothing extra to clean up on close). Phase 1
	 * uses this to clear its interactive-navigation indicator
	 * (ReportsCancel()); report-only phases don't need it. */
	paramActionCancelProc cancelProc;
} reportsDialog_t;

/** Ties one report's PD_BUTTON entry (Save/Print/Refresh) to both which
 * dialog it belongs to and which operation it performs -- the single
 * `void *context` a paramData_t button callback receives (see
 * form/createcontrols.c's ButtonPush()) has to carry both now that
 * DoReportsOp() is shared across every report phase's dialog. */
typedef struct {
	reportsDialog_t *rd;
	int op;
} reportsOpCtx_t;

static void DoReportsOp(void *data);
static void ReportsDlgUpdate(paramGroup_p pg, int inx, void *valueP);
static void ReportsCancel(paramGroup_p pg);
static void ReportsShowDialog(reportsDialog_t *rd, const char *title);

/** Tentative declaration -- reportsUnconnectedDlg (below) needs `&reportsPG`
 * before reportsPLs[]/reportsPG's own full definition can be written (the
 * definitions are mutually referential: the button paramData_t entries need
 * their reportsOpCtx_t's `&reportsUnconnectedDlg`, which needs `&reportsPG`).
 * Given its own initializer further down, same as any other static. */
static paramGroup_t reportsPG;
static void ReportsBuildText(DynString *out);

/** Phase 1's dialog state -- see reportsDialog_t's doc comment. */
static reportsDialog_t reportsUnconnectedDlg = {
	&reportsPG, NULL, NULL, NULL,
	ReportsBuildText, ReportsUnconnectedEndpoints,
	ReportsDlgUpdate, ReportsCancel
};
static reportsOpCtx_t reportsUnconnectedRefreshOp = { &reportsUnconnectedDlg, REPORTSOP_REFRESH };
static reportsOpCtx_t reportsUnconnectedSaveOp    = { &reportsUnconnectedDlg, REPORTSOP_SAVE };
static reportsOpCtx_t reportsUnconnectedPrintOp   = { &reportsUnconnectedDlg, REPORTSOP_PRINT };

static wWinPix_t reportsListWidths[] = { 60, 80, 80, 70, 60 };
static const char * reportsListTitles[] = {
	N_("Track"), N_("X"), N_("Y"), N_("Angle"), N_("Nearby")
};
static paramListData_t reportsListData = { 8, 300, 5, reportsListWidths, reportsListTitles };

static paramData_t reportsPLs[] = {
#define I_REPORTSSUMMARY (0)
#define reportsSummary (reportsPLs[I_REPORTSSUMMARY].control)
	{ PD_MESSAGE, "", "summary", 0, I2VP(37) },
#define I_REPORTSLIST (1)
#define reportsList (reportsPLs[I_REPORTSLIST].control)
	{ PD_LIST, NULL, "list", PDO_DLGRESIZE, &reportsListData, NULL, 0 },
	{ PD_BUTTON, DoReportsOp, "refresh", 0, NULL, NULL, 0, &reportsUnconnectedRefreshOp },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, &reportsUnconnectedSaveOp },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, &reportsUnconnectedPrintOp },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
};
static paramGroup_t reportsPG = { "reports", PGO_FULLDIALOGFROMBUILDER, reportsPLs, COUNT( reportsPLs ) };

/** The current report's rows, kept alive for as long as the dialog might
 * still reference them via the list's per-row context pointers (phase
 * 1.5) -- unlike a plain local, this must outlive the compute function
 * that builds it. Replaced (old rows freed first) each time a report is
 * (re)generated; see ReportsUnconnectedEndpoints(). */
static dynArr_t reportsCurrentList_da;

/** TRUE while ReportsPopulateList() is clearing/rebuilding reportsList.
 * Found 2026-09-04 (real crash, not hypothetical -- SIGSEGV in
 * DrawRulerWithBackground, root-caused via coredumpctl backtrace):
 * wListClear()'s row deletions make GTK relocate the tree view's cursor,
 * which *synchronously* re-fires ReportsDlgUpdate() (the selection-changed
 * handler) mid-clear, before the new rows exist -- so it reads a stale
 * per-row context pointer into the *previous* report's reportsEndPt_t
 * array, already DYNARR_FREE()'d by ReportsUnconnectedEndpoints() just
 * before this call. That dangling read produced garbage track IDs, a
 * garbage scale index (surfaced as a spurious "Scale index ... is not
 * valid" notice), and eventually a NaN position that crashed ruler
 * drawing. ReportsDlgUpdate() checks this flag and no-ops while it's set
 * -- selection state is meaningless during a clear/repopulate transition
 * regardless of what garbage or valid data it might resolve to, so this
 * guards the transition itself rather than trying to validate the data. */
static BOOL_T reportsPopulating = FALSE;

/** Phase 1.5 interactive-navigation indicator state -- see
 * ReportsSetIndicator()/ReportsClearIndicator()/ReportsDrawIndicator(). */
static BOOL_T reportIndicatorActive = FALSE;
static coOrd reportIndicatorPos;
static SCALEINX_T reportIndicatorScale;

static void ReportsBuildText(DynString *out);

/** Refresh `rd`'s hidden text control from its current report rows right
 * before Save or Print actually use it -- see reports.ui's comment on
 * "scrollwindow"/"text" for why this control exists at all despite never
 * being shown. */
static void ReportsRefreshPrintText(reportsDialog_t *rd)
{
	DynString content;

	rd->buildText( &content );
	wTextClear( rd->text );
	wTextAppend( rd->text, DynStringToCStr(&content) );
	DynStringFree( &content );
}

/** wFilSelCallBack_p for every report's Save file selector -- `data` is
 * the `reportsDialog_t *` passed as wFilSelCreate()'s context in
 * ReportsShowDialog(), so this one callback serves every report phase. */
static int DoReportsSave(
        int files,
        char **fileName,
        void * data )
{
	reportsDialog_t *rd = (reportsDialog_t *)data;

	CHECK( fileName != NULL );
	CHECK( files == 1 );

	SetCurrentPath( REPORTPATHKEY, fileName[0] );
	ReportsRefreshPrintText( rd );
	return wTextSave( rd->text, fileName[ 0 ] );
}

/** PD_BUTTON callback for every report's Refresh/Save/Print buttons --
 * `data` is the button's own `reportsOpCtx_t *` (its paramData_t entry's
 * `context` field), which carries both which dialog and which operation,
 * so this one callback serves every report phase's dialog. */
static void DoReportsOp(
        void * data )
{
	reportsOpCtx_t *ctx = (reportsOpCtx_t *)data;

	if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }

	switch( ctx->op ) {
	case REPORTSOP_SAVE:
		LOG( log_reports, 1, ( "reports: Save clicked\n" ) )
		wFilSelect( ctx->rd->fileSel, GetCurrentPath(REPORTPATHKEY) );
		break;
	case REPORTSOP_PRINT:
		LOG( log_reports, 1, ( "reports: Print clicked\n" ) )
		ReportsRefreshPrintText( ctx->rd );
		wTextPrint( ctx->rd->text );
		break;
	case REPORTSOP_REFRESH:
		LOG( log_reports, 1, ( "reports: Refresh clicked\n" ) )
		if ( ctx->rd->refreshProc ) {
			ctx->rd->refreshProc( NULL );
		}
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

	/* Ignore selection-changed events GTK fires synchronously mid-clear/
	 * repopulate (ReportsPopulateList()) -- see reportsPopulating's doc
	 * comment. Real crash without this guard, not a hypothetical. */
	if (reportsPopulating) {
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

	if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
	LOG( log_reports, 1, ( "reports: row %d selected -> track %d @ (%.3f,%.3f)\n",
	                       sel, entry->trackId, entry->pos.x, entry->pos.y ) )

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

/**
 * Show one report's dialog in the generic, reusable report viewer -- a
 * one-line summary label above an interactive, selectable list (Save/
 * Print/Print Setup buttons; Save/Print are built fresh from the report's
 * current rows on demand when clicked, not from pre-formatted text passed
 * in here -- see ReportsBuildText()/ReportsRefreshPrintText()). This call
 * only creates/shows the dialog frame -- the caller is responsible for
 * setting the summary label's text and populating the list afterward;
 * ReportsShowDialog() itself doesn't touch either.
 *
 * Each `reportsDialog_t` gets its own window/dialog, created lazily on
 * first call and reused (never retitled) on every subsequent call for
 * that same report -- unlike phase 1's original single-report design,
 * this no longer needs a "what if a second report type reuses this
 * window" answer, since every report phase now gets its own `rd`.
 *
 * \param[in,out] rd this report phase's dialog state
 * \param[in] title dialog title, e.g. "Unconnected Endpoints Report"
 */
static void ReportsShowDialog(reportsDialog_t *rd, const char *title)
{
	if (rd->window == NULL) {
		FormRegister( rd->pg );
		rd->window = FormCreateDialog( rd->pg, MakeWindowTitle(title),
		                              NULL, NULL,
		                              NULL, rd->cancelProc ? rd->cancelProc : FormCancel_Current,
		                              TRUE, F_RESIZE,
		                              rd->changeProc );
		rd->fileSel = wFilSelCreate( mainW, FS_SAVE, 0, title,
		                            sReportsFilePattern, DoReportsSave, rd );
		/* rd->text backs Save/Print only -- it's never shown to the user
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
		 * was added). This also means rd->text is created via
		 * wTextCreate()'s standalone/no-.ui-needed code path (parent
		 * lacks F_DEFINEDINBUILDER) -- otherwise unexercised anywhere in
		 * this codebase (checked: not even wlib/test/testapp.c calls
		 * wTextCreate() against a non-builder parent). */
		{
			wControl_p textBacking = wWinDialogCreate( NULL, NULL, NULL,
			                             "reportstextbacking", 0L, NULL, NULL );
			wControlShow( textBacking, FALSE );
			rd->text = wTextCreate( textBacking, 0, 0, NULL, NULL,
			                        BO_READONLY, 0, 0 );
		}
	}
	/* Deliberately does NOT touch rd->text here -- the hidden text
	 * control is not kept in sync eagerly every time a report is shown;
	 * it's built fresh only when Save/Print are actually clicked
	 * (ReportsRefreshPrintText()), so it always reflects the current
	 * report without needing to be rebuilt on every regeneration whether
	 * or not it's ever used. */

	FormLoadControls( rd->pg );
	wShow( rd->window );
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

/** Flag entries with another open endpoint, on a *different* track, within
 * connectDistance (track.h -- the same threshold/definition of "close
 * enough to connect" used throughout track.c/cturnout.c/ccornu.c/etc.), then
 * group flagged entries first in reportsCurrentList_da -- a default row
 * order, not a filter, every entry stays present either way. Endpoints on
 * the *same* track (e.g. a turntable's own spokes) never flag each other --
 * those are expected to sit close together and aren't a missed connection.
 * O(n^2) distance check; fine at this report's realistic scale (checked
 * against the layout23.xtc fixture, 100+ rows). Must run after the compute
 * loop that fills reportsCurrentList_da and before ReportsPopulateList(),
 * since row order here becomes list row order there. */
static void ReportsMarkNearby(void)
{
	int i, j, cnt, outIdx;
	reportsEndPt_t *entries;
	reportsEndPt_t *grouped;

	cnt = reportsCurrentList_da.cnt;
	if ( cnt == 0 ) {
		return;
	}
	entries = (reportsEndPt_t *)reportsCurrentList_da.ptr;

	for ( i = 0; i < cnt; i++ ) {
		entries[i].nearby = FALSE;
	}
	for ( i = 0; i < cnt; i++ ) {
		for ( j = i + 1; j < cnt; j++ ) {
			if ( entries[i].trackId == entries[j].trackId ) {
				continue;
			}
			if ( FindDistance(entries[i].pos, entries[j].pos) <= connectDistance ) {
				entries[i].nearby = TRUE;
				entries[j].nearby = TRUE;
			}
		}
	}

	grouped = malloc( cnt * sizeof *grouped );
	outIdx = 0;
	for ( i = 0; i < cnt; i++ ) {
		if ( entries[i].nearby ) {
			grouped[outIdx++] = entries[i];
		}
	}
	for ( i = 0; i < cnt; i++ ) {
		if ( !entries[i].nearby ) {
			grouped[outIdx++] = entries[i];
		}
	}
	memcpy( entries, grouped, cnt * sizeof *grouped );
	free( grouped );
}

/** Populate the interactive list (phase 1.5) from reportsCurrentList_da --
 * one row per entry, tab-separated (wListAddValue() splits on tabs into
 * columns), each row's context set to that entry's address so
 * ReportsDlgUpdate() can recover it on selection. Must run after
 * ReportsShowDialog() has run at least once (the list control doesn't exist
 * before the dialog's first creation).
 *
 * Rows are shown in reportsCurrentList_da's own order -- ReportsMarkNearby()
 * has already grouped nearby-flagged entries first, all rows still present,
 * nothing filtered. */
static void ReportsPopulateList(void)
{
	int i;
	char row[128];

	/* Guards both directions -- wListClear()'s deletions AND the
	 * wListAddValue() loop below can each make GTK synchronously fire a
	 * selection-changed signal (cursor relocating to the nearest
	 * surviving row on delete, or auto-selecting a newly-added first row)
	 * -- see reportsPopulating's doc comment for the crash this caused. */
	reportsPopulating = TRUE;

	wListClear( reportsList );
	for ( i = 0; i < reportsCurrentList_da.cnt; i++ ) {
		reportsEndPt_t *entry = &DYNARR_N(reportsEndPt_t, reportsCurrentList_da, i);
		snprintf( row, sizeof row, "%d\t%.3f\t%.3f\t%.3f\t%s",
		          entry->trackId, entry->pos.x, entry->pos.y, entry->angle,
		          entry->nearby ? _("Yes") : "" );
		wListAddValue( reportsList, row, NULL, entry );
	}

	reportsPopulating = FALSE;
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

	ReportsMarkNearby();

	{
		int i, nearbyCnt = 0;
		char summary[128];

		for ( i = 0; i < reportsCurrentList_da.cnt; i++ ) {
			if ( DYNARR_N( reportsEndPt_t, reportsCurrentList_da, i ).nearby ) {
				nearbyCnt++;
			}
		}

		if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
		LOG( log_reports, 1,
		     ( "reports: Unconnected Endpoints computed -- %d open endpoint(s), %d flagged nearby\n",
		       reportsCurrentList_da.cnt, nearbyCnt ) )

		ReportsShowDialog( &reportsUnconnectedDlg, _("Unconnected Endpoints Report") );
		snprintf( summary, sizeof summary,
		          _("%d open endpoint(s), %d flagged Nearby"),
		          reportsCurrentList_da.cnt, nearbyCnt );
		wMessageSetValue( reportsSummary, summary );
	}

	ReportsPopulateList();
}

/* ---------------------------------------------------------------------
 * Phase 2 (SF #217): Turnout Density -- first of the phase-2 batch
 * (track lengths/curve stats/turnout density/equipment suitability), and
 * the proof-of-pattern slice for the other three: report-only (no
 * click-to-navigate, no draw indicator), own `.ui` dialog
 * (reportsturnout.ui) with a summary PD_MESSAGE + one PD_LIST table, own
 * reportsDialog_t instance sharing the same ReportsShowDialog()/
 * DoReportsOp() plumbing phase 1 uses. See the phase-2 implementation
 * plan for the compute-pass primitives (GetTrkLength()/GetTrkLayer()/
 * GetTrkType()==T_TURNOUT) and MCP-parity notes.
 * ------------------------------------------------------------------- */

/** Tentative declaration -- same reason as reportsPG above. */
static paramGroup_t reportsTurnoutPG;
static void ReportsBuildTurnoutText(DynString *out);

/** Phase 2's Turnout Density dialog state -- report-only, so no
 * changeProc/cancelProc (NULL means "no row-selection handling" /
 * "use the default FormCancel_Current", respectively -- see
 * reportsDialog_t's doc comment). */
static reportsDialog_t reportsTurnoutDlg = {
	&reportsTurnoutPG, NULL, NULL, NULL,
	ReportsBuildTurnoutText, ReportsTurnoutDensity,
	NULL, NULL
};
static reportsOpCtx_t reportsTurnoutRefreshOp = { &reportsTurnoutDlg, REPORTSOP_REFRESH };
static reportsOpCtx_t reportsTurnoutSaveOp    = { &reportsTurnoutDlg, REPORTSOP_SAVE };
static reportsOpCtx_t reportsTurnoutPrintOp   = { &reportsTurnoutDlg, REPORTSOP_PRINT };

static wWinPix_t reportsTurnoutListWidths[] = { 50, 150, 70, 70, 90, 60 };
static const char * reportsTurnoutListTitles[] = {
	N_("Layer"), N_("Name"), N_("Feet"), N_("Turnouts"), N_("Density/100ft"), N_("Flag")
};
static paramListData_t reportsTurnoutListData = { 8, 400, 6, reportsTurnoutListWidths,
                reportsTurnoutListTitles
              };

static paramData_t reportsTurnoutPLs[] = {
#define I_REPORTSTURNOUTSUMMARY (0)
#define reportsTurnoutSummary (reportsTurnoutPLs[I_REPORTSTURNOUTSUMMARY].control)
	{ PD_MESSAGE, "", "summary", 0, I2VP(37) },
#define I_REPORTSTURNOUTLIST (1)
#define reportsTurnoutList (reportsTurnoutPLs[I_REPORTSTURNOUTLIST].control)
	{ PD_LIST, NULL, "list", PDO_DLGRESIZE, &reportsTurnoutListData, NULL, 0 },
	{ PD_BUTTON, DoReportsOp, "refresh", 0, NULL, NULL, 0, &reportsTurnoutRefreshOp },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, &reportsTurnoutSaveOp },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, &reportsTurnoutPrintOp },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
};
static paramGroup_t reportsTurnoutPG = { "reportsturnout", PGO_FULLDIALOGFROMBUILDER,
                reportsTurnoutPLs, COUNT( reportsTurnoutPLs )
              };

/** The current Turnout Density report's by-layer rows -- same
 * outlives-the-compute-function lifetime rule as reportsCurrentList_da
 * (phase 1), even though nothing here needs a per-row context pointer
 * (report-only, no click-to-navigate). Replaced (old rows freed first)
 * each time the report is (re)generated. */
static dynArr_t reportsTurnoutList_da;

/** Turnouts with fewer than 3 endpoints -- folded into the Save/Print
 * text only (ReportsBuildTurnoutText()), never a second interactive list
 * (see the phase-2 implementation plan's dialog-architecture section). */
static dynArr_t reportsTurnoutPartial_da;

/** Populate the interactive list from reportsTurnoutList_da -- one row
 * per layer, tab-separated (wListAddValue() splits on tabs into
 * columns), in reportsTurnoutList_da's own (layer-ascending) order.
 * Unlike ReportsPopulateList() (phase 1), no reportsPopulating guard is
 * needed: this dialog has no changeProc, so GTK's synchronous
 * selection-changed signal during wListClear()/wListAddValue() has
 * nothing to call back into. Must run after ReportsShowDialog() has run
 * at least once for this dialog (the list control doesn't exist before
 * the dialog's first creation). */
static void ReportsPopulateTurnoutList(void)
{
	int i;
	char row[160];

	wListClear( reportsTurnoutList );
	for ( i = 0; i < reportsTurnoutList_da.cnt; i++ ) {
		reportsTurnoutLayer_t *entry = &DYNARR_N(reportsTurnoutLayer_t,
		                               reportsTurnoutList_da, i);
		snprintf( row, sizeof row, "%u\t%s\t%.1f\t%d\t%.1f\t%s",
		          entry->layer, entry->name, entry->feet, entry->turnoutCount,
		          entry->density, entry->heavy ? _("HEAVY") : "" );
		wListAddValue( reportsTurnoutList, row, NULL, NULL );
	}
}

/** Build the full formatted Turnout Density report text (header +
 * by-layer table + partial-turnouts list) fresh from
 * reportsTurnoutList_da/reportsTurnoutPartial_da. Used only by
 * ReportsRefreshPrintText() -- i.e. only when Save or Print is actually
 * clicked -- so it always reflects whatever the list is currently
 * showing, never a stale copy from an earlier ReportsTurnoutDensity()
 * call. */
static void ReportsBuildTurnoutText(DynString *out)
{
	DynStringMalloc( out, 256 );
	ReportsAddHeader( out, _("Turnout Density Report") );

	if ( reportsTurnoutList_da.cnt == 0 ) {
		DynStringCatCStrs( out, "\n", _("No track found."), "\n", NULL );
	} else {
		/* snprintf into a local buffer, then append -- see
		 * ReportsBuildText()'s comment on the same pattern for why
		 * DynStringPrintf() directly on `out` would be wrong here. */
		char headerLine[96];
		snprintf( headerLine, sizeof headerLine, "\n%5s  %-16s  %8s  %5s  %14s\n",
		          _("Lyr"), _("Name"), _("Feet"), _("T/O"), _("Density/100ft") );
		DynStringCatCStr( out, headerLine );
		ReportsFormatTurnoutList( out, &DYNARR_N(reportsTurnoutLayer_t,
		                          reportsTurnoutList_da, 0),
		                          reportsTurnoutList_da.cnt );
	}

	DynStringCatCStrs( out, "\n",
	                   _("PARTIAL TURNOUTS (fewer than 3 endpoints -- may be broken/incomplete)"),
	                   "\n", NULL );
	if ( reportsTurnoutPartial_da.cnt == 0 ) {
		DynStringCatCStrs( out, "  ", _("None found."), "\n", NULL );
	} else {
		ReportsFormatPartialTurnoutList( out, &DYNARR_N(reportsPartialTurnout_t,
		                                 reportsTurnoutPartial_da, 0),
		                                 reportsTurnoutPartial_da.cnt );
	}
}

void ReportsTurnoutDensity( void * unused )
{
	track_p trk;
	DIST_T layerFeet[NUM_LAYERS];
	int layerTurnouts[NUM_LAYERS];
	unsigned int layer;
	DIST_T totalFt = 0.0;
	int totalTurnouts = 0;

	for ( layer = 0; layer < NUM_LAYERS; layer++ ) {
		layerFeet[layer] = 0.0;
		layerTurnouts[layer] = 0;
	}

	DYNARR_FREE( reportsTurnoutLayer_t, reportsTurnoutList_da );
	DYNARR_INIT( reportsTurnoutLayer_t, reportsTurnoutList_da );
	DYNARR_FREE( reportsPartialTurnout_t, reportsTurnoutPartial_da );
	DYNARR_INIT( reportsPartialTurnout_t, reportsTurnoutPartial_da );

	TRK_ITERATE( trk ) {
		unsigned int trkLayer = GetTrkLayer(trk);
		DIST_T lengthFt = GetTrkLength(trk, 0, 1) / 12.0;
		BOOL_T isTurnout = (GetTrkType(trk) == T_TURNOUT);

		if ( trkLayer < NUM_LAYERS ) {
			layerFeet[trkLayer] += lengthFt;
			if ( isTurnout ) {
				layerTurnouts[trkLayer]++;
			}
		}
		totalFt += lengthFt;
		if ( isTurnout ) {
			totalTurnouts++;
			if ( GetTrkEndPtCnt(trk) < 3 ) {
				reportsPartialTurnout_t *p;
				DYNARR_APPEND( reportsPartialTurnout_t, reportsTurnoutPartial_da, 5 );
				p = &DYNARR_LAST( reportsPartialTurnout_t, reportsTurnoutPartial_da );
				p->trackId = GetTrkIndex(trk);
				p->endPtCnt = GetTrkEndPtCnt(trk);
				p->layer = trkLayer;
			}
		}
	}

	for ( layer = 0; layer < NUM_LAYERS; layer++ ) {
		reportsTurnoutLayer_t *row;

		if ( layerFeet[layer] <= 0.0 && layerTurnouts[layer] == 0 ) {
			continue;
		}
		DYNARR_APPEND( reportsTurnoutLayer_t, reportsTurnoutList_da, 10 );
		row = &DYNARR_LAST( reportsTurnoutLayer_t, reportsTurnoutList_da );
		row->layer = layer;
		row->name = GetLayerName(layer);
		row->feet = layerFeet[layer];
		row->turnoutCount = layerTurnouts[layer];
		row->density = layerFeet[layer] > 0.0 ?
		               (layerTurnouts[layer] / layerFeet[layer] * 100.0) : 0.0;
		row->heavy = ReportsIsTurnoutHeavy(row->density);
	}

	{
		DIST_T globalDensity = totalFt > 0.0 ? (totalTurnouts / totalFt * 100.0) : 0.0;
		char summary[160];

		if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
		LOG( log_reports, 1,
		     ( "reports: Turnout Density computed -- %d turnout(s), %.1f ft track, %.1f/100ft, %d partial\n",
		       totalTurnouts, totalFt, globalDensity, reportsTurnoutPartial_da.cnt ) )

		ReportsShowDialog( &reportsTurnoutDlg, _("Turnout Density Report") );
		snprintf( summary, sizeof summary,
		          _("%d turnout(s), %.1f ft track, %.1f per 100ft overall%s"),
		          totalTurnouts, totalFt, globalDensity,
		          reportsTurnoutPartial_da.cnt > 0 ?
		          _(" (partial turnouts found -- see Save/Print)") : "" );
		wMessageSetValue( reportsTurnoutSummary, summary );
	}

	ReportsPopulateTurnoutList();
}
