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
#include "ccurve.h"
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

static wWinPix_t reportsListWidths[] = { 60, 60, 110, 80, 80, 70, 60 };
static const char * reportsListTitles[] = {
	N_("Track"), N_("Layer #"), N_("Layer Name"), N_("X"), N_("Y"), N_("Angle"), N_("Nearby")
};
static paramListData_t reportsListData = { 8, 300, 7, reportsListWidths, reportsListTitles };

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
		snprintf( row, sizeof row, "%d\t%u\t%s\t%.3f\t%.3f\t%.3f\t%s",
		          entry->trackId, entry->layer, entry->layerName,
		          entry->pos.x, entry->pos.y, entry->angle,
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
		char headerLine[96];
		snprintf( headerLine, sizeof headerLine,
		          "\n%6s | %5s | %-16s | %8s | %8s | %7s\n",
		          _("Track"), _("Lyr"), _("Layer Name"), _("X"), _("Y"), _("Angle") );
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

	/* Clear the currently-visible list's rows (and their about-to-be-
	 * stale context pointers) BEFORE freeing the backing array below --
	 * otherwise, whenever this dialog is already open with real rows,
	 * there's a live window (from here until ReportsPopulateList() runs
	 * again at the end of this function) where the visible list still
	 * references memory this function is about to free. Any GTK-driven
	 * selection-changed signal landing in that window -- not necessarily
	 * a literal click; a window move/focus event can trigger GTK to
	 * re-touch the treeview's selection too -- reads a genuine dangling
	 * pointer. Found via live user testing 2026-09-05 on the sibling
	 * Gaps/Kinked Joints reports (same pattern, see
	 * [[feedback_reports_stale_list_use_after_free]]), then confirmed
	 * present here too. Only needed once the dialog/list actually exists
	 * -- skip on the very first call, before ReportsShowDialog() has
	 * created it. Distinct from reportsPopulating's own synchronous
	 * mid-wListClear() reentrancy guard (SF #772) -- that one protects a
	 * signal firing *during* this same clear call; this early clear
	 * closes the wider window a *later, asynchronous* signal could land
	 * in. */
	if ( reportsUnconnectedDlg.window != NULL ) {
		reportsPopulating = TRUE;
		wListClear( reportsList );
		reportsPopulating = FALSE;
	}

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
				/* +1: matches the main UI's own 1-based layer display
				 * (dlayer.c) -- see reports.h's layer/layerName doc
				 * comment. */
				entry->layer = GetTrkLayer(trk) + 1;
				entry->layerName = GetLayerName(GetTrkLayer(trk));
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
		/* GetTrkLength(trk,0,1) reads endpoints 0 and 1 unconditionally
		 * (track.c) -- TRK_ITERATE walks every object on the track list,
		 * not just track with a real length (benchwork, notes, groups,
		 * ...), and a turnout with fewer than 2 endpoints is exactly what
		 * this report's own PARTIAL TURNOUTS section exists to flag. Any
		 * of these crash a CHECK() in GetTrkEndPos (trkendpt.c) without
		 * this guard -- confirmed the hard way (real crash on a live
		 * layout, 2026-09-04) after this fixture's synthetic dmreport-
		 * turnout.xtr, built with only 2-endpoint straights and one
		 * 3-endpoint turnout, had nothing under 2 endpoints to catch it.
		 * Matches the MCP reference's own guard
		 * (TrackObject.length_model_inches(): "if len(eps) < 2: return
		 * 0.0"), just not carried over into this port originally. */
		DIST_T lengthFt = (GetTrkEndPtCnt(trk) >= 2) ?
		                  GetTrkLength(trk, 0, 1) / 12.0 : 0.0;
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
				/* +1: see the identical comment in ReportsTurnoutDensity()'s
				 * BY LAYER loop below -- same 0-based-vs-1-based mismatch. */
				p->layer = trkLayer + 1;
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
		/* +1: the layer index is 0-based internally, but every other
		 * layer-numbered UI in the app (the layer buttons, the layer
		 * combo box -- see dlayer.c's "%2d %c %s", layerNumber + 1, ...)
		 * displays 1-based numbers. Found via live user testing
		 * 2026-09-05 -- the report's own Layer column read "4" for the
		 * exact same layer the main window's selector showed as "5". */
		row->layer = layer + 1;
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

/* ---------------------------------------------------------------------
 * Phase 2 (SF #217): Track Lengths -- second of the batch, same
 * report-only shape as Turnout Density (own .ui dialog, own
 * reportsDialog_t instance, no click-to-navigate/indicator). Compute
 * pass shares Turnout Density's GetTrkEndPtCnt(trk) >= 2 guard before
 * calling GetTrkLength() -- see [[feedback_trk_iterate_endpoint_guard]]
 * (memory) / SF #776's fix; this report's own synthetic fixture also
 * includes a zero-endpoint object from the start, not added after a
 * live crash this time.
 * ------------------------------------------------------------------- */

/** Tentative declaration -- same reason as reportsPG above. */
static paramGroup_t reportsTrackLenPG;
static void ReportsBuildTrackLenText(DynString *out);

static reportsDialog_t reportsTrackLenDlg = {
	&reportsTrackLenPG, NULL, NULL, NULL,
	ReportsBuildTrackLenText, ReportsTrackLengths,
	NULL, NULL
};
static reportsOpCtx_t reportsTrackLenRefreshOp = { &reportsTrackLenDlg, REPORTSOP_REFRESH };
static reportsOpCtx_t reportsTrackLenSaveOp    = { &reportsTrackLenDlg, REPORTSOP_SAVE };
static reportsOpCtx_t reportsTrackLenPrintOp   = { &reportsTrackLenDlg, REPORTSOP_PRINT };

static wWinPix_t reportsTrackLenListWidths[] = { 50, 150, 70, 70, 60, 60 };
static const char * reportsTrackLenListTitles[] = {
	N_("Layer"), N_("Name"), N_("Feet"), N_("Turnouts"), N_("Cars"), N_("% Total")
};
static paramListData_t reportsTrackLenListData = { 8, 400, 6, reportsTrackLenListWidths,
                                                   reportsTrackLenListTitles
                                                 };

static paramData_t reportsTrackLenPLs[] = {
#define I_REPORTSTRACKLENSUMMARY (0)
#define reportsTrackLenSummary (reportsTrackLenPLs[I_REPORTSTRACKLENSUMMARY].control)
	{ PD_MESSAGE, "", "summary", 0, I2VP(37) },
#define I_REPORTSTRACKLENLIST (1)
#define reportsTrackLenList (reportsTrackLenPLs[I_REPORTSTRACKLENLIST].control)
	{ PD_LIST, NULL, "list", PDO_DLGRESIZE, &reportsTrackLenListData, NULL, 0 },
	{ PD_BUTTON, DoReportsOp, "refresh", 0, NULL, NULL, 0, &reportsTrackLenRefreshOp },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, &reportsTrackLenSaveOp },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, &reportsTrackLenPrintOp },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
};
static paramGroup_t reportsTrackLenPG = { "reportstracklen", PGO_FULLDIALOGFROMBUILDER,
                                          reportsTrackLenPLs, COUNT( reportsTrackLenPLs )
                                        };

/** The current Track Lengths report's by-layer rows -- same lifetime rule
 * as reportsTurnoutList_da. */
static dynArr_t reportsTrackLenList_da;

/** Sum of every row's lengthFt -- kept alongside reportsTrackLenList_da
 * so both ReportsPopulateTrackLenList() (the live "% Total" column) and
 * ReportsBuildTrackLenText() (ReportsFormatTrackLengthList()'s totalFt
 * parameter) can compute percentages without re-summing the array
 * themselves or risking the two disagreeing. */
static DIST_T reportsTrackLenTotalFt;

/** Populate the interactive list from reportsTrackLenList_da -- one row
 * per layer, tab-separated. Same no-reportsPopulating-guard-needed
 * reasoning as ReportsPopulateTurnoutList() (no changeProc on this
 * dialog). */
static void ReportsPopulateTrackLenList(void)
{
	int i;
	char row[160];

	wListClear( reportsTrackLenList );
	for ( i = 0; i < reportsTrackLenList_da.cnt; i++ ) {
		reportsTrackLenLayer_t *entry = &DYNARR_N(reportsTrackLenLayer_t,
		                                reportsTrackLenList_da, i);
		DIST_T pct = reportsTrackLenTotalFt > 0.0 ?
		             (entry->lengthFt / reportsTrackLenTotalFt * 100.0) : 0.0;

		snprintf( row, sizeof row, "%u\t%s\t%.1f\t%d\t%d\t%.1f%%",
		          entry->layer, entry->name, entry->lengthFt,
		          entry->turnoutCount, entry->carCapacity, pct );
		wListAddValue( reportsTrackLenList, row, NULL, NULL );
	}
}

/** Build the full formatted Track Lengths report text (header + by-layer
 * table) fresh from reportsTrackLenList_da/reportsTrackLenTotalFt. Used
 * only by ReportsRefreshPrintText() (Save/Print), same as
 * ReportsBuildTurnoutText(). */
static void ReportsBuildTrackLenText(DynString *out)
{
	DynStringMalloc( out, 256 );
	ReportsAddHeader( out, _("Track Lengths Report") );

	if ( reportsTrackLenList_da.cnt == 0 ) {
		DynStringCatCStrs( out, "\n", _("No track found."), "\n", NULL );
	} else {
		char headerLine[112];
		snprintf( headerLine, sizeof headerLine,
		          "\n%5s  %-16s  %8s  %9s  %5s  %5s  %6s\n",
		          _("Lyr"), _("Name"), _("Feet"), _("Inches"), _("T/O"), _("Cars"), _("% Tot") );
		DynStringCatCStr( out, headerLine );
		ReportsFormatTrackLengthList( out, &DYNARR_N(reportsTrackLenLayer_t,
		                              reportsTrackLenList_da, 0),
		                              reportsTrackLenList_da.cnt, reportsTrackLenTotalFt );
	}
}

void ReportsTrackLengths( void * unused )
{
	track_p trk;
	DIST_T layerFeet[NUM_LAYERS];
	int layerTurnouts[NUM_LAYERS];
	unsigned int layer;
	DIST_T totalFt = 0.0;
	int totalTurnouts = 0;
	DIST_T carsPerFt = ReportsCarsPerFoot( GetScaleName( GetLayoutCurScale() ) );

	for ( layer = 0; layer < NUM_LAYERS; layer++ ) {
		layerFeet[layer] = 0.0;
		layerTurnouts[layer] = 0;
	}

	DYNARR_FREE( reportsTrackLenLayer_t, reportsTrackLenList_da );
	DYNARR_INIT( reportsTrackLenLayer_t, reportsTrackLenList_da );

	TRK_ITERATE( trk ) {
		unsigned int trkLayer = GetTrkLayer(trk);
		/* Same guard as ReportsTurnoutDensity() -- see that function's
		 * comment for the real crash this prevents. */
		DIST_T lengthFt = (GetTrkEndPtCnt(trk) >= 2) ?
		                  GetTrkLength(trk, 0, 1) / 12.0 : 0.0;
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
		}
	}

	for ( layer = 0; layer < NUM_LAYERS; layer++ ) {
		reportsTrackLenLayer_t *row;

		if ( layerFeet[layer] <= 0.0 && layerTurnouts[layer] == 0 ) {
			continue;
		}
		DYNARR_APPEND( reportsTrackLenLayer_t, reportsTrackLenList_da, 10 );
		row = &DYNARR_LAST( reportsTrackLenLayer_t, reportsTrackLenList_da );
		/* +1: see the identical comment in ReportsTurnoutDensity() --
		 * every other layer-numbered UI in the app displays 1-based
		 * numbers, this report's Layer column was showing the raw
		 * 0-based internal index instead. */
		row->layer = layer + 1;
		row->name = GetLayerName(layer);
		row->lengthFt = layerFeet[layer];
		row->lengthIn = layerFeet[layer] * 12.0;
		row->turnoutCount = layerTurnouts[layer];
		row->carCapacity = (int)(layerFeet[layer] * carsPerFt);
	}
	reportsTrackLenTotalFt = totalFt;

	{
		int totalCarCapacity = (int)(totalFt * carsPerFt);
		char summary[160];

		if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
		LOG( log_reports, 1,
		     ( "reports: Track Lengths computed -- %.1f ft track, %d turnout(s), %d car(s) capacity\n",
		       totalFt, totalTurnouts, totalCarCapacity ) )

		ReportsShowDialog( &reportsTrackLenDlg, _("Track Lengths Report") );
		snprintf( summary, sizeof summary,
		          _("%.1f ft track, %d turnout(s), ~%d car(s) capacity"),
		          totalFt, totalTurnouts, totalCarCapacity );
		wMessageSetValue( reportsTrackLenSummary, summary );
	}

	ReportsPopulateTrackLenList();
}

/* ---------------------------------------------------------------------
 * Phase 2 (SF #217): Curve Stats -- third of the batch, same
 * report-only shape as Turnout Density/Track Lengths. Uses
 * GetCurveRadius() (ccurve.h), not GetTrackParams(PARAMS_CORNU, ...) --
 * see that function's own doc comment (tcurve.c) for why the originally-
 * planned accessor isn't safe to call on every TRK_ITERATE object (a
 * CHECK(FALSE) crash risk for non-getTrackParams types, same class of bug
 * as SF #776's fix, plus a real ErrorMessage() popup risk on undersized
 * curves under easement mode).
 * ------------------------------------------------------------------- */

/** Tentative declaration -- same reason as reportsPG above. */
static paramGroup_t reportsCurvePG;
static void ReportsBuildCurveText(DynString *out);

static reportsDialog_t reportsCurveDlg = {
	&reportsCurvePG, NULL, NULL, NULL,
	ReportsBuildCurveText, ReportsCurveStats,
	NULL, NULL
};
static reportsOpCtx_t reportsCurveRefreshOp = { &reportsCurveDlg, REPORTSOP_REFRESH };
static reportsOpCtx_t reportsCurveSaveOp    = { &reportsCurveDlg, REPORTSOP_SAVE };
static reportsOpCtx_t reportsCurvePrintOp   = { &reportsCurveDlg, REPORTSOP_PRINT };

static wWinPix_t reportsCurveListWidths[] = { 90, 70 };
static const char * reportsCurveListTitles[] = {
	N_("Range"), N_("Count")
};
static paramListData_t reportsCurveListData = { 8, 300, 2, reportsCurveListWidths,
                                                reportsCurveListTitles
                                              };

static paramData_t reportsCurvePLs[] = {
#define I_REPORTSCURVESUMMARY (0)
#define reportsCurveSummary (reportsCurvePLs[I_REPORTSCURVESUMMARY].control)
	{ PD_MESSAGE, "", "summary", 0, I2VP(37) },
#define I_REPORTSCURVELIST (1)
#define reportsCurveList (reportsCurvePLs[I_REPORTSCURVELIST].control)
	{ PD_LIST, NULL, "list", PDO_DLGRESIZE, &reportsCurveListData, NULL, 0 },
	{ PD_BUTTON, DoReportsOp, "refresh", 0, NULL, NULL, 0, &reportsCurveRefreshOp },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, &reportsCurveSaveOp },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, &reportsCurvePrintOp },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
};
static paramGroup_t reportsCurvePG = { "reportscurvestats", PGO_FULLDIALOGFROMBUILDER,
                                       reportsCurvePLs, COUNT( reportsCurvePLs )
                                     };

/** The current Curve Stats report's rows -- always exactly 5
 * (ReportsCurveBucketLabel()'s fixed bucket set), including zero-count
 * buckets: unlike the MCP reference's own radius_distribution dict
 * (which only includes buckets a curve actually falls in), always
 * showing all 5 lets a viewer see the whole distribution shape at a
 * glance -- e.g. "no curves under 12in" reads as a real, positive
 * finding rather than a row that's simply missing (2026-09-04, same
 * "improve display, keep the underlying data/thresholds" standing as
 * Turnout Density's HEAVY flag). */
static dynArr_t reportsCurveList_da;

/** Populate the interactive list from reportsCurveList_da -- one row per
 * bucket, tab-separated, in ReportsCurveStats()'s fixed bucket order. */
static void ReportsPopulateCurveList(void)
{
	int i;
	char row[64];

	wListClear( reportsCurveList );
	for ( i = 0; i < reportsCurveList_da.cnt; i++ ) {
		reportsCurveBucket_t *entry = &DYNARR_N(reportsCurveBucket_t,
		                                        reportsCurveList_da, i);
		snprintf( row, sizeof row, "%s\t%d", entry->label, entry->count );
		wListAddValue( reportsCurveList, row, NULL, NULL );
	}
}

/** Build the full formatted Curve Stats report text (header + histogram)
 * fresh from reportsCurveList_da. Used only by ReportsRefreshPrintText()
 * (Save/Print), same as the other phase-2 reports' own build-text
 * functions. */
static void ReportsBuildCurveText(DynString *out)
{
	DynStringMalloc( out, 256 );
	ReportsAddHeader( out, _("Curve Stats Report") );

	if ( reportsCurveList_da.cnt == 0 ) {
		DynStringCatCStrs( out, "\n", _("No curves found."), "\n", NULL );
	} else {
		char headerLine[48];
		snprintf( headerLine, sizeof headerLine, "\n%-8s  %5s\n", _("Range"),
		          _("Count") );
		DynStringCatCStr( out, headerLine );
		ReportsFormatCurveHistogram( out, &DYNARR_N(reportsCurveBucket_t,
		                             reportsCurveList_da, 0), reportsCurveList_da.cnt );
	}
}

void ReportsCurveStats( void * unused )
{
	track_p trk;
	int curveCount = 0;
	DIST_T minR = 0.0, maxR = 0.0, sumR = 0.0;
	/* Must match ReportsCurveBucketLabel()'s (reportsformat.c) own bucket
	 * set/order exactly -- compared by string below, not re-derived from
	 * the radius thresholds a second time here, so the two can't drift
	 * apart silently. */
	static const char * const bucketLabels[] = {
		"< 12in", "12-18in", "18-24in", "24-36in", "> 36in"
	};
	int bucketCounts[5] = { 0, 0, 0, 0, 0 };
	int i;

	DYNARR_FREE( reportsCurveBucket_t, reportsCurveList_da );
	DYNARR_INIT( reportsCurveBucket_t, reportsCurveList_da );

	TRK_ITERATE( trk ) {
		/* GetCurveRadius() itself already guards on GetTrkType(trk) ==
		 * T_CURVE (returns 0.0 otherwise) -- safe to call unconditionally
		 * on every TRK_ITERATE object, no CHECK()/ErrorMessage() risk;
		 * see that function's own doc comment (tcurve.c). */
		DIST_T r = GetCurveRadius(trk);
		const char *label;

		if ( r <= 0.0 ) {
			continue;
		}
		curveCount++;
		if ( curveCount == 1 || r < minR ) {
			minR = r;
		}
		if ( curveCount == 1 || r > maxR ) {
			maxR = r;
		}
		sumR += r;

		label = ReportsCurveBucketLabel(r);
		for ( i = 0; i < 5; i++ ) {
			if ( strcmp(label, bucketLabels[i]) == 0 ) {
				bucketCounts[i]++;
				break;
			}
		}
	}

	for ( i = 0; i < 5; i++ ) {
		reportsCurveBucket_t *row;
		DYNARR_APPEND( reportsCurveBucket_t, reportsCurveList_da, 5 );
		row = &DYNARR_LAST( reportsCurveBucket_t, reportsCurveList_da );
		row->label = bucketLabels[i];
		row->count = bucketCounts[i];
	}

	{
		DIST_T meanR = curveCount > 0 ? (sumR / curveCount) : 0.0;
		char summary[160];

		if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
		LOG( log_reports, 1,
		     ( "reports: Curve Stats computed -- %d curve(s), min %.2f, max %.2f, mean %.2f\n",
		       curveCount, minR, maxR, meanR ) )

		ReportsShowDialog( &reportsCurveDlg, _("Curve Stats Report") );
		if ( curveCount > 0 ) {
			snprintf( summary, sizeof summary,
			          _("%d curve(s), radius %.1f-%.1f in (mean %.1f)"),
			          curveCount, minR, maxR, meanR );
		} else {
			snprintf( summary, sizeof summary, "%s", _("No curves found") );
		}
		wMessageSetValue( reportsCurveSummary, summary );
	}

	ReportsPopulateCurveList();
}

/* ---------------------------------------------------------------------
 * Phase 2 (SF #217): Equipment Suitability -- last of the batch. Unlike
 * the other three, this compute pass doesn't walk TRK_ITERATE for a
 * by-layer breakdown -- it only needs the layout-wide minimum usable
 * curve radius (same GetCurveRadius() as Curve Stats) and a fixed,
 * scale-adjusted equipment-class list, classified via the already-built
 * ReportsClassifyEquipment(). Same report-only shape (own .ui dialog,
 * own reportsDialog_t instance, no click-to-navigate/indicator) as the
 * other three.
 * ------------------------------------------------------------------- */

/** Fixed equipment-class list (name, HO-reference minimum radius in
 * inches) -- ported from an external MCP project's reference
 * implementation for data parity (design history, not a citation a
 * reader here can open -- see reports.h's file header). Each threshold
 * is scale-adjusted at compute time (ReportsEquipmentSuitability()) via
 * `thresholdIn * scaleFactor`, matching ReportsClassifyEquipment()'s own
 * contract. */
static const struct {
	const char *name;
	DIST_T hoThresholdIn;
} reportsEquipThresholds[] = {
	{ N_("Short freight car (< 40 ft)"), 15.0 },
	{ N_("Standard freight car (40-50 ft)"), 18.0 },
	{ N_("Long freight car (55-60 ft flatcar)"), 22.0 },
	{ N_("4-axle diesel (GP/RS-type)"), 18.0 },
	{ N_("6-axle diesel (SD40/ES44)"), 22.0 },
	{ N_("Small/medium steam (2-6-0, 2-8-0, 4-6-0)"), 18.0 },
	{ N_("Medium steam (4-6-2, 2-8-2, 4-6-4)"), 22.0 },
	{ N_("Large steam (4-8-4, 2-10-4, 4-8-2)"), 24.0 },
	{ N_("Articulated steam (2-8-8-2, 4-8-8-4)"), 28.0 },
	{ N_("Standard passenger car (85 ft)"), 22.0 },
	{ N_("Long passenger car (Superliner)"), 28.0 },
};
#define REPORTS_EQUIP_COUNT (sizeof reportsEquipThresholds / sizeof reportsEquipThresholds[0])

/** Curves under this radius are excluded from the minimum-radius scan as
 * likely decorative (e.g. a small radius used for a spur into a scenic
 * detail, not something rolling stock actually needs to negotiate at
 * speed) -- matches the external MCP project's own reference behavior.
 * Same future-preference-candidate standing as ReportsIsTurnoutHeavy()'s
 * 10.0 threshold -- not made configurable in this pass. */
#define REPORTS_EQUIP_MIN_USABLE_RADIUS (9.0)

/** Tentative declaration -- same reason as reportsPG above. */
static paramGroup_t reportsEquipPG;
static void ReportsBuildEquipText(DynString *out);

static reportsDialog_t reportsEquipDlg = {
	&reportsEquipPG, NULL, NULL, NULL,
	ReportsBuildEquipText, ReportsEquipmentSuitability,
	NULL, NULL
};
static reportsOpCtx_t reportsEquipRefreshOp = { &reportsEquipDlg, REPORTSOP_REFRESH };
static reportsOpCtx_t reportsEquipSaveOp    = { &reportsEquipDlg, REPORTSOP_SAVE };
static reportsOpCtx_t reportsEquipPrintOp   = { &reportsEquipDlg, REPORTSOP_PRINT };

static wWinPix_t reportsEquipListWidths[] = { 260, 90, 80 };
static const char * reportsEquipListTitles[] = {
	N_("Equipment Class"), N_("Min Radius"), N_("Status")
};
static paramListData_t reportsEquipListData = { 8, 400, 3, reportsEquipListWidths,
                                                reportsEquipListTitles
                                              };

static paramData_t reportsEquipPLs[] = {
#define I_REPORTSEQUIPSUMMARY (0)
#define reportsEquipSummary (reportsEquipPLs[I_REPORTSEQUIPSUMMARY].control)
	{ PD_MESSAGE, "", "summary", 0, I2VP(37) },
#define I_REPORTSEQUIPLIST (1)
#define reportsEquipList (reportsEquipPLs[I_REPORTSEQUIPLIST].control)
	{ PD_LIST, NULL, "list", PDO_DLGRESIZE, &reportsEquipListData, NULL, 0 },
	{ PD_BUTTON, DoReportsOp, "refresh", 0, NULL, NULL, 0, &reportsEquipRefreshOp },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, &reportsEquipSaveOp },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, &reportsEquipPrintOp },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
};
static paramGroup_t reportsEquipPG = { "reportsequipment", PGO_FULLDIALOGFROMBUILDER,
                                       reportsEquipPLs, COUNT( reportsEquipPLs )
                                     };

/** The current Equipment Suitability report's rows -- always exactly
 * REPORTS_EQUIP_COUNT (the fixed equipment-class list above), in that
 * list's own order (not grouped by status -- ReportsFormatEquipmentList()
 * does that grouping for the Save/Print text only, same as the other
 * phase-2 reports keep their interactive list in a different order than
 * their own Save/Print text). */
static dynArr_t reportsEquipList_da;

/** Populate the interactive list from reportsEquipList_da -- one row per
 * equipment class, tab-separated. */
static void ReportsPopulateEquipList(void)
{
	int i;
	char row[192];

	wListClear( reportsEquipList );
	for ( i = 0; i < reportsEquipList_da.cnt; i++ ) {
		reportsEquipClass_t *entry = &DYNARR_N(reportsEquipClass_t, reportsEquipList_da,
		                                       i);
		const char *statusStr = entry->status == REPORTS_EQUIP_PASS ? _("PASS") :
		                        entry->status == REPORTS_EQUIP_MARGINAL ? _("MARGINAL") : _("FAIL");

		snprintf( row, sizeof row, "%s\t%.1f\"\t%s",
		          entry->name, entry->thresholdIn, statusStr );
		wListAddValue( reportsEquipList, row, NULL, NULL );
	}
}

/** Build the full formatted Equipment Suitability report text (header +
 * PASS/MARGINAL/FAIL-grouped table) fresh from reportsEquipList_da. Used
 * only by ReportsRefreshPrintText() (Save/Print), same as the other
 * phase-2 reports' own build-text functions. */
static void ReportsBuildEquipText(DynString *out)
{
	DynStringMalloc( out, 256 );
	ReportsAddHeader( out, _("Equipment Suitability Report") );

	if ( reportsEquipList_da.cnt == 0 ) {
		DynStringCatCStrs( out, "\n",
		                   _("No usable curves found -- cannot assess equipment suitability."),
		                   "\n", NULL );
	} else {
		DynStringCatCStr( out, "\n" );
		ReportsFormatEquipmentList( out, &DYNARR_N(reportsEquipClass_t,
		                            reportsEquipList_da, 0), reportsEquipList_da.cnt );
	}
}

void ReportsEquipmentSuitability( void * unused )
{
	track_p trk;
	BOOL_T haveRadius = FALSE;
	DIST_T minR = 0.0;
	DIST_T scaleFactor;
	size_t i;

	{
		SCALEINX_T hoIdx = LookupScale( "HO" );
		DIST_T hoRatio = GetScaleRatio( hoIdx );
		DIST_T curRatio = GetScaleRatio( GetLayoutCurScale() );

		scaleFactor = curRatio > 0.0 ? hoRatio / curRatio : 1.0;
	}

	TRK_ITERATE( trk ) {
		/* GetCurveRadius() already guards on GetTrkType(trk) == T_CURVE
		 * (returns 0.0 otherwise) -- see that function's own doc comment
		 * (tcurve.c). Curves under REPORTS_EQUIP_MIN_USABLE_RADIUS are
		 * excluded as likely decorative, matching the external MCP
		 * project's own reference behavior. */
		DIST_T r = GetCurveRadius(trk);

		if ( r < REPORTS_EQUIP_MIN_USABLE_RADIUS ) {
			continue;
		}
		if ( !haveRadius || r < minR ) {
			minR = r;
			haveRadius = TRUE;
		}
	}

	DYNARR_FREE( reportsEquipClass_t, reportsEquipList_da );
	DYNARR_INIT( reportsEquipClass_t, reportsEquipList_da );

	if ( haveRadius ) {
		for ( i = 0; i < REPORTS_EQUIP_COUNT; i++ ) {
			reportsEquipClass_t *row;
			DIST_T thresholdIn = reportsEquipThresholds[i].hoThresholdIn * scaleFactor;

			DYNARR_APPEND( reportsEquipClass_t, reportsEquipList_da, REPORTS_EQUIP_COUNT );
			row = &DYNARR_LAST( reportsEquipClass_t, reportsEquipList_da );
			row->name = _(reportsEquipThresholds[i].name);
			row->thresholdIn = thresholdIn;
			row->status = ReportsClassifyEquipment( minR, thresholdIn, scaleFactor );
		}
	}

	{
		char summary[160];

		if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
		LOG( log_reports, 1,
		     ( "reports: Equipment Suitability computed -- min radius %.2f, %d class(es) rated\n",
		       minR, (int) reportsEquipList_da.cnt ) )

		ReportsShowDialog( &reportsEquipDlg, _("Equipment Suitability Report") );
		if ( haveRadius ) {
			snprintf( summary, sizeof summary,
			          _("Min usable curve radius %.1f in -- %d equipment class(es) rated"),
			          minR, (int) reportsEquipList_da.cnt );
		} else {
			snprintf( summary, sizeof summary, "%s",
			          _("No usable curves found -- cannot assess equipment suitability") );
		}
		wMessageSetValue( reportsEquipSummary, summary );
	}

	ReportsPopulateEquipList();
}

/* ---------------------------------------------------------------------
 * Phase 3/3a (SF #217, SF #779): Gaps and Kinked Joints. Both interactive
 * (click-to-navigate + draw indicator), same phase-1/1.5 shape -- own
 * .ui dialog (reportsgaps.ui/reportskinked.ui), own reportsDialog_t
 * instance with a real changeProc/cancelProc (unlike the phase-2 batch
 * above, which passes NULL/NULL). This is the first reuse of those two
 * fields since phase 1 introduced them -- see the phase-3/3a
 * implementation plan for the full design record.
 *
 * Both share the phase-1.5 indicator statics
 * (reportIndicatorActive/Pos/Scale, ReportsDrawIndicator()) with phase 1
 * and each other -- that state is a single set of module-level globals,
 * not per-dialog, so if two interactive report dialogs are open at once
 * and a row is clicked in each, the second click's indicator simply
 * replaces the first's (last-clicked-anywhere wins, globally). This was
 * already true with only phase 1 shipped; it just wasn't reachable until
 * a second interactive dialog existed. Not fixed here (would mean keying
 * the indicator state off the reportsDialog_t itself) -- see the
 * implementation plan's "still open" list.
 * ------------------------------------------------------------------- */

/** Tentative declaration -- same reason as reportsPG above. */
static paramGroup_t reportsGapsPG;
static void ReportsBuildGapsText(DynString *out);
static void ReportsDlgUpdateGaps(paramGroup_p pg, int inx, void *valueP);
static void ReportsCancelGaps(paramGroup_p pg);

static reportsDialog_t reportsGapsDlg = {
	&reportsGapsPG, NULL, NULL, NULL,
	ReportsBuildGapsText, ReportsGaps,
	ReportsDlgUpdateGaps, ReportsCancelGaps
};
static reportsOpCtx_t reportsGapsRefreshOp = { &reportsGapsDlg, REPORTSOP_REFRESH };
static reportsOpCtx_t reportsGapsSaveOp    = { &reportsGapsDlg, REPORTSOP_SAVE };
static reportsOpCtx_t reportsGapsPrintOp   = { &reportsGapsDlg, REPORTSOP_PRINT };

static wWinPix_t reportsGapsListWidths[] = { 60, 60, 100, 60, 60, 100, 70 };
static const char * reportsGapsListTitles[] = {
	N_("Track A"), N_("Layer A #"), N_("Layer A Name"),
	N_("Track B"), N_("Layer B #"), N_("Layer B Name"), N_("Gap (in)")
};
static paramListData_t reportsGapsListData = { 8, 400, 7, reportsGapsListWidths,
                                               reportsGapsListTitles
                                             };

static paramData_t reportsGapsPLs[] = {
#define I_REPORTSGAPSSUMMARY (0)
#define reportsGapsSummary (reportsGapsPLs[I_REPORTSGAPSSUMMARY].control)
	{ PD_MESSAGE, "", "summary", 0, I2VP(37) },
#define I_REPORTSGAPSLIST (1)
#define reportsGapsList (reportsGapsPLs[I_REPORTSGAPSLIST].control)
	{ PD_LIST, NULL, "list", PDO_DLGRESIZE, &reportsGapsListData, NULL, 0 },
	{ PD_BUTTON, DoReportsOp, "refresh", 0, NULL, NULL, 0, &reportsGapsRefreshOp },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, &reportsGapsSaveOp },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, &reportsGapsPrintOp },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
};
static paramGroup_t reportsGapsPG = { "reportsgaps", PGO_FULLDIALOGFROMBUILDER,
                                      reportsGapsPLs, COUNT( reportsGapsPLs )
                                    };

/** The current Gaps report's near-miss pairs -- same outlives-the-compute-
 * function lifetime rule as reportsCurrentList_da (phase 1), since each
 * row's list context pointer must stay valid for click-to-navigate.
 * Replaced (old rows freed first) each time the report is (re)generated. */
static dynArr_t reportsGapsList_da;

/** Count of open endpoints with no near-miss partner (not a second
 * interactive list -- folded into the summary line + Save/Print text
 * only, same PARTIAL TURNOUTS precedent as phase 2). Recomputed alongside
 * reportsGapsList_da each call. */
static int reportsGapsIsolatedCnt = 0;

/** Count of open endpoints excluded from the pairing analysis because
 * they belong to a turntable (QueryTrack(trk, Q_CAN_ADD_ENDPOINTS)) --
 * turntable stalls are open by design, not a missed connection. Matches
 * the external MCP project's own write_gaps_report() design (reports
 * turntable stalls as a count, never pairs them). */
static int reportsGapsTurntableCnt = 0;

/** TRUE while ReportsPopulateGapsList() is clearing/rebuilding
 * reportsGapsList -- same re-entrancy guard as reportsPopulating (phase
 * 1), see that flag's own doc comment for the real SF #772 crash this
 * pattern exists to prevent. Not optional for a dialog with a changeProc. */
static BOOL_T reportsGapsPopulating = FALSE;

/** Populate the interactive list from reportsGapsList_da -- one row per
 * near-miss pair, tab-separated, each row's context set to that pair's
 * address so ReportsDlgUpdateGaps() can recover it on selection. */
static void ReportsPopulateGapsList(void)
{
	int i;
	char row[160];

	reportsGapsPopulating = TRUE;

	wListClear( reportsGapsList );
	for ( i = 0; i < reportsGapsList_da.cnt; i++ ) {
		reportsGapPair_t *entry = &DYNARR_N(reportsGapPair_t, reportsGapsList_da, i);
		snprintf( row, sizeof row, "%d\t%u\t%s\t%d\t%u\t%s\t%.4f",
		          entry->trackA, entry->layerA, entry->layerNameA,
		          entry->trackB, entry->layerB, entry->layerNameB,
		          entry->gapIn );
		wListAddValue( reportsGapsList, row, NULL, entry );
	}

	reportsGapsPopulating = FALSE;
}

/** paramGroup_t changeProc for the Gaps dialog -- selecting a row pans/
 * indicates at the pair's midpoint (using trackA's own scale for the
 * indicator radius, see reportsGapPair_t's doc comment). Same guard/
 * recovery shape as phase 1's ReportsDlgUpdate(). */
static void ReportsDlgUpdateGaps(paramGroup_p pg, int inx, void *valueP)
{
	wIndex_t sel;
	reportsGapPair_t *entry;
	coOrd mid;

	if (inx != I_REPORTSGAPSLIST) {
		return;
	}
	if (reportsGapsPopulating) {
		return;
	}

	sel = wListGetIndex(reportsGapsList);
	if (sel < 0) {
		return;
	}
	entry = (reportsGapPair_t *)wListGetItemContext(reportsGapsList, sel);
	if (!entry) {
		return;
	}

	if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
	LOG( log_reports, 1,
	     ( "reports: gaps row %d selected -> track %d <-> %d, gap=%.4f\n",
	       sel, entry->trackA, entry->trackB, entry->gapIn ) )

	mid.x = (entry->posA.x + entry->posB.x) / 2.0;
	mid.y = (entry->posA.y + entry->posB.y) / 2.0;
	ReportsSetIndicator(mid, entry->scale);
}

/** paramActionCancelProc for the Gaps dialog -- same shape as phase 1's
 * ReportsCancel(). */
static void ReportsCancelGaps(paramGroup_p pg)
{
	ReportsClearIndicator();
	FormCancel_Current(pg);
}

/** Build the full formatted Gaps report text (header + summary counts +
 * near-miss-pairs table) fresh from reportsGapsList_da/
 * reportsGapsIsolatedCnt/reportsGapsTurntableCnt. Used only by
 * ReportsRefreshPrintText() (Save/Print). */
static void ReportsBuildGapsText(DynString *out)
{
	char summaryLine[200];

	DynStringMalloc( out, 256 );
	ReportsAddHeader( out, _("Gaps Report") );

	snprintf( summaryLine, sizeof summaryLine,
	          "\n%s: %d\n%s: %d\n%s: %d\n",
	          _("Near-miss pairs"), reportsGapsList_da.cnt,
	          _("Isolated open endpoints"), reportsGapsIsolatedCnt,
	          _("Turntable stalls excluded"), reportsGapsTurntableCnt );
	DynStringCatCStr( out, summaryLine );

	if ( reportsGapsList_da.cnt == 0 ) {
		DynStringCatCStrs( out, "\n", _("No near-miss pairs found."), "\n", NULL );
	} else {
		char headerLine[112];
		snprintf( headerLine, sizeof headerLine,
		          "\n%6s | %5s | %-14s | %6s | %5s | %-14s | %8s\n",
		          _("Trk A"), _("Lyr A"), _("Layer A Name"),
		          _("Trk B"), _("Lyr B"), _("Layer B Name"), _("Gap") );
		DynStringCatCStr( out, headerLine );
		ReportsFormatGapList( out, &DYNARR_N(reportsGapPair_t, reportsGapsList_da, 0),
		                      reportsGapsList_da.cnt );
	}
}

/** One open (unconnected) endpoint, scratch data local to ReportsGaps()'s
 * own pairing pass -- unlike reportsGapPair_t (which must survive for
 * click-to-navigate), this never leaves the function, freed before
 * ReportsGaps() returns. */
typedef struct {
	TRKINX_T trackId;
	coOrd    pos;
	SCALEINX_T scale;
	BOOL_T   paired;
	unsigned int layer;
	const char *layerName;
} reportsGapOpenEndPt_t;

void ReportsGaps( void * unused )
{
	track_p trk;
	dynArr_t open_da;
	reportsGapOpenEndPt_t *entries;
	int i, j, cnt;
	int turntableCnt = 0;
	int isolatedCnt = 0;

	ReportsClearIndicator();

	/* Clear the visible list before freeing the array its rows'
	 * contexts point into -- see ReportsUnconnectedEndpoints()'s doc
	 * comment on the identical pattern for the full rationale. Found via
	 * live user testing 2026-09-05: with this dialog already open
	 * showing a real row, a subsequent refresh computing zero rows still
	 * left a stale row-selection event readable moments later, reading
	 * memory this function had already freed. */
	if ( reportsGapsDlg.window != NULL ) {
		reportsGapsPopulating = TRUE;
		wListClear( reportsGapsList );
		reportsGapsPopulating = FALSE;
	}

	DYNARR_FREE( reportsGapPair_t, reportsGapsList_da );
	DYNARR_INIT( reportsGapPair_t, reportsGapsList_da );

	/* open_da is a fresh local, not a static -- DYNARR_INIT (not FREE)
	 * here, same local-vs-static distinction as every other dynArr_t in
	 * this file. Freed explicitly below before returning. */
	DYNARR_INIT( reportsGapOpenEndPt_t, open_da );

	TRK_ITERATE( trk ) {
		EPINX_T ep;
		/* Turntable stalls are open by design -- excluded from the
		 * pairing analysis entirely (counted separately), matching the
		 * external MCP project's own write_gaps_report() design. Unlike
		 * phase 1's own Unconnected Endpoints report (which lists every
		 * open endpoint including turntable stalls), this is a
		 * deliberate divergence between the two reports, not a bug to
		 * reconcile -- see the implementation plan's decision #3. */
		BOOL_T isTurntable = QueryTrack( trk, Q_CAN_ADD_ENDPOINTS );

		for ( ep = 0; ep < GetTrkEndPtCnt(trk); ep++ ) {
			if ( GetTrkEndTrk(trk, ep) != NULL ) {
				continue;
			}
			if ( isTurntable ) {
				turntableCnt++;
				continue;
			}
			{
				reportsGapOpenEndPt_t *entry;
				DYNARR_APPEND( reportsGapOpenEndPt_t, open_da, 10 );
				entry = &DYNARR_LAST( reportsGapOpenEndPt_t, open_da );
				entry->trackId = GetTrkIndex(trk);
				entry->pos = GetTrkEndPos(trk, ep);
				entry->scale = GetTrkScale(trk);
				entry->paired = FALSE;
				entry->layer = GetTrkLayer(trk) + 1;
				entry->layerName = GetLayerName(GetTrkLayer(trk));
			}
		}
	}

	cnt = open_da.cnt;
	entries = (reportsGapOpenEndPt_t *)open_da.ptr;

	/* O(n^2) near-miss pairing, same realistic-scale justification as
	 * phase 1's ReportsMarkNearby() (checked against layout23.xtc, 100+
	 * rows). Same-track pairs excluded (a same-track pair this short is a
	 * legitimate stub, not a missed connection) -- a deliberate, more
	 * correct divergence from the external MCP reference's own
	 * _near_miss_pairs(), which has no such exclusion. */
	for ( i = 0; i < cnt; i++ ) {
		for ( j = i + 1; j < cnt; j++ ) {
			DIST_T dist;

			if ( entries[i].trackId == entries[j].trackId ) {
				continue;
			}
			dist = FindDistance( entries[i].pos, entries[j].pos );
			if ( dist <= connectDistance ) {
				reportsGapPair_t *pair;
				DYNARR_APPEND( reportsGapPair_t, reportsGapsList_da, 10 );
				pair = &DYNARR_LAST( reportsGapPair_t, reportsGapsList_da );
				pair->trackA = entries[i].trackId;
				pair->posA = entries[i].pos;
				pair->trackB = entries[j].trackId;
				pair->posB = entries[j].pos;
				pair->gapIn = dist;
				pair->scale = entries[i].scale;
				pair->layerA = entries[i].layer;
				pair->layerNameA = entries[i].layerName;
				pair->layerB = entries[j].layer;
				pair->layerNameB = entries[j].layerName;
				entries[i].paired = TRUE;
				entries[j].paired = TRUE;
			}
		}
	}

	for ( i = 0; i < cnt; i++ ) {
		if ( !entries[i].paired ) {
			isolatedCnt++;
		}
	}

	DYNARR_FREE( reportsGapOpenEndPt_t, open_da );

	reportsGapsIsolatedCnt = isolatedCnt;
	reportsGapsTurntableCnt = turntableCnt;

	{
		char summary[200];

		if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
		LOG( log_reports, 1,
		     ( "reports: Gaps computed -- %d pair(s), %d isolated, %d turntable stall(s) excluded\n",
		       reportsGapsList_da.cnt, isolatedCnt, turntableCnt ) )

		ReportsShowDialog( &reportsGapsDlg, _("Gaps Report") );
		snprintf( summary, sizeof summary,
		          _("%d near-miss pair(s), %d isolated open endpoint(s), %d turntable stall(s) excluded"),
		          reportsGapsList_da.cnt, isolatedCnt, turntableCnt );
		wMessageSetValue( reportsGapsSummary, summary );
	}

	ReportsPopulateGapsList();
}

/* ---------------------------------------------------------------------
 * Phase 3a (SF #217, SF #779): Kinked Joints -- same interactive shape as
 * Gaps above, different detection: already-connected joints whose angle
 * discontinuity exceeds connectAngle. No MCP counterpart, see reports.h's
 * reportsKinkedJoint_t doc comment.
 * ------------------------------------------------------------------- */

/** Tentative declaration -- same reason as reportsPG above. */
static paramGroup_t reportsKinkedPG;
static void ReportsBuildKinkedText(DynString *out);
static void ReportsDlgUpdateKinked(paramGroup_p pg, int inx, void *valueP);
static void ReportsCancelKinked(paramGroup_p pg);

static reportsDialog_t reportsKinkedDlg = {
	&reportsKinkedPG, NULL, NULL, NULL,
	ReportsBuildKinkedText, ReportsKinkedJoints,
	ReportsDlgUpdateKinked, ReportsCancelKinked
};
static reportsOpCtx_t reportsKinkedRefreshOp = { &reportsKinkedDlg, REPORTSOP_REFRESH };
static reportsOpCtx_t reportsKinkedSaveOp    = { &reportsKinkedDlg, REPORTSOP_SAVE };
static reportsOpCtx_t reportsKinkedPrintOp   = { &reportsKinkedDlg, REPORTSOP_PRINT };

static wWinPix_t reportsKinkedListWidths[] = { 60, 60, 100, 60, 60, 100, 90 };
static const char * reportsKinkedListTitles[] = {
	N_("Track A"), N_("Layer A #"), N_("Layer A Name"),
	N_("Track B"), N_("Layer B #"), N_("Layer B Name"), N_("Angle Diff (deg)")
};
static paramListData_t reportsKinkedListData = { 8, 400, 7, reportsKinkedListWidths,
                                                 reportsKinkedListTitles
                                               };

static paramData_t reportsKinkedPLs[] = {
#define I_REPORTSKINKEDSUMMARY (0)
#define reportsKinkedSummary (reportsKinkedPLs[I_REPORTSKINKEDSUMMARY].control)
	{ PD_MESSAGE, "", "summary", 0, I2VP(37) },
#define I_REPORTSKINKEDLIST (1)
#define reportsKinkedList (reportsKinkedPLs[I_REPORTSKINKEDLIST].control)
	{ PD_LIST, NULL, "list", PDO_DLGRESIZE, &reportsKinkedListData, NULL, 0 },
	{ PD_BUTTON, DoReportsOp, "refresh", 0, NULL, NULL, 0, &reportsKinkedRefreshOp },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, &reportsKinkedSaveOp },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, &reportsKinkedPrintOp },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
};
static paramGroup_t reportsKinkedPG = { "reportskinked", PGO_FULLDIALOGFROMBUILDER,
                                        reportsKinkedPLs, COUNT( reportsKinkedPLs )
                                      };

/** The current Kinked Joints report's rows -- same outlives-the-compute-
 * function lifetime rule as reportsGapsList_da above. */
static dynArr_t reportsKinkedList_da;

/** Same re-entrancy guard as reportsGapsPopulating above. */
static BOOL_T reportsKinkedPopulating = FALSE;

/** Populate the interactive list from reportsKinkedList_da -- one row per
 * kinked joint, tab-separated. */
static void ReportsPopulateKinkedList(void)
{
	int i;
	char row[160];

	reportsKinkedPopulating = TRUE;

	wListClear( reportsKinkedList );
	for ( i = 0; i < reportsKinkedList_da.cnt; i++ ) {
		reportsKinkedJoint_t *entry = &DYNARR_N(reportsKinkedJoint_t,
		                                        reportsKinkedList_da, i);
		snprintf( row, sizeof row, "%d\t%u\t%s\t%d\t%u\t%s\t%.3f",
		          entry->trackA, entry->layerA, entry->layerNameA,
		          entry->trackB, entry->layerB, entry->layerNameB,
		          entry->angleDelta );
		wListAddValue( reportsKinkedList, row, NULL, entry );
	}

	reportsKinkedPopulating = FALSE;
}

/** paramGroup_t changeProc for the Kinked Joints dialog -- selecting a
 * row pans/indicates at the joint's own position. */
static void ReportsDlgUpdateKinked(paramGroup_p pg, int inx, void *valueP)
{
	wIndex_t sel;
	reportsKinkedJoint_t *entry;

	if (inx != I_REPORTSKINKEDLIST) {
		return;
	}
	if (reportsKinkedPopulating) {
		return;
	}

	sel = wListGetIndex(reportsKinkedList);
	if (sel < 0) {
		return;
	}
	entry = (reportsKinkedJoint_t *)wListGetItemContext(reportsKinkedList, sel);
	if (!entry) {
		return;
	}

	if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
	LOG( log_reports, 1,
	     ( "reports: kinked row %d selected -> track %d <-> %d, delta=%.3f\n",
	       sel, entry->trackA, entry->trackB, entry->angleDelta ) )

	ReportsSetIndicator(entry->pos, entry->scale);
}

/** paramActionCancelProc for the Kinked Joints dialog. */
static void ReportsCancelKinked(paramGroup_p pg)
{
	ReportsClearIndicator();
	FormCancel_Current(pg);
}

/** Build the full formatted Kinked Joints report text fresh from
 * reportsKinkedList_da. Used only by ReportsRefreshPrintText(). */
static void ReportsBuildKinkedText(DynString *out)
{
	DynStringMalloc( out, 256 );
	ReportsAddHeader( out, _("Kinked Joints Report") );

	if ( reportsKinkedList_da.cnt == 0 ) {
		DynStringCatCStrs( out, "\n", _("No kinked joints found."), "\n", NULL );
	} else {
		char headerLine[112];
		snprintf( headerLine, sizeof headerLine,
		          "\n%6s | %5s | %-14s | %6s | %5s | %-14s | %7s\n",
		          _("Trk A"), _("Lyr A"), _("Layer A Name"),
		          _("Trk B"), _("Lyr B"), _("Layer B Name"), _("Angle") );
		DynStringCatCStr( out, headerLine );
		ReportsFormatKinkedList( out, &DYNARR_N(reportsKinkedJoint_t,
		                                        reportsKinkedList_da, 0),
		                         reportsKinkedList_da.cnt );
	}
}

void ReportsKinkedJoints( void * unused )
{
	track_p trk;

	ReportsClearIndicator();

	/* Clear the visible list before freeing the array its rows'
	 * contexts point into -- see ReportsUnconnectedEndpoints()'s doc
	 * comment on the identical pattern for the full rationale. */
	if ( reportsKinkedDlg.window != NULL ) {
		reportsKinkedPopulating = TRUE;
		wListClear( reportsKinkedList );
		reportsKinkedPopulating = FALSE;
	}

	DYNARR_FREE( reportsKinkedJoint_t, reportsKinkedList_da );
	DYNARR_INIT( reportsKinkedJoint_t, reportsKinkedList_da );

	TRK_ITERATE( trk ) {
		EPINX_T ep;

		for ( ep = 0; ep < GetTrkEndPtCnt(trk); ep++ ) {
			track_p other = GetTrkEndTrk(trk, ep);
			EPINX_T otherEp;
			ANGLE_T delta;

			if ( other == NULL || other == trk ) {
				continue;
			}
			/* Dedup: TRK_ITERATE visits every joint from both sides (once
			 * via trk's endpoint, once via other's) -- only emit a row
			 * from the lower-indexed side. */
			if ( GetTrkIndex(trk) >= GetTrkIndex(other) ) {
				continue;
			}

			otherEp = GetEndPtConnectedToMe( other, trk );
			if ( otherEp < 0 ) {
				/* Shouldn't happen -- GetTrkEndTrk() above already
				 * confirmed trk and other are connected, so other must
				 * have a reverse endpoint back to trk. Defensive only. */
				continue;
			}

			/* Same formula ConnectTracks() itself uses at connect-time
			 * (track.c) -- a kinked joint is one that would fail that
			 * same re-check today, i.e. real post-connect drift/
			 * corruption, not a new invented tolerance. */
			delta = fabs( DifferenceBetweenAngles( GetTrkEndAngle(trk, ep),
			                                       GetTrkEndAngle(other, otherEp) + 180.0 ) );
			if ( delta > connectAngle ) {
				reportsKinkedJoint_t *row;
				DYNARR_APPEND( reportsKinkedJoint_t, reportsKinkedList_da, 10 );
				row = &DYNARR_LAST( reportsKinkedJoint_t, reportsKinkedList_da );
				row->trackA = GetTrkIndex(trk);
				row->trackB = GetTrkIndex(other);
				row->pos = GetTrkEndPos(trk, ep);
				row->angleDelta = delta;
				row->scale = GetTrkScale(trk);
				row->layerA = GetTrkLayer(trk) + 1;
				row->layerNameA = GetLayerName(GetTrkLayer(trk));
				row->layerB = GetTrkLayer(other) + 1;
				row->layerNameB = GetLayerName(GetTrkLayer(other));
			}
		}
	}

	{
		char summary[160];

		if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
		LOG( log_reports, 1,
		     ( "reports: Kinked Joints computed -- %d joint(s) beyond %.2f deg tolerance\n",
		       reportsKinkedList_da.cnt, connectAngle ) )

		ReportsShowDialog( &reportsKinkedDlg, _("Kinked Joints Report") );
		snprintf( summary, sizeof summary,
		          _("%d kinked joint(s) found (beyond %.1f deg Connection Angle tolerance)"),
		          reportsKinkedList_da.cnt, connectAngle );
		wMessageSetValue( reportsKinkedSummary, summary );
	}

	ReportsPopulateKinkedList();
}
