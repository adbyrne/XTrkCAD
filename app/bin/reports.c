/** \file reports.c
 * Native "Reports" feature (SF #217), phase 1: a generic report viewer
 * (Refresh/Save/Print/Print Setup, same button shape as denum.c's "Parts
 * List" dialog -- built generic from the start so every future report
 * phase can reuse it) plus the phase-1 report itself, unconnected track
 * endpoints.
 *
 * Phase 1.5 (SF #772, folded into the same ticket) adds interactive
 * click-to-navigate: selecting a row in the visible PD_LIST control pans
 * the main canvas to that endpoint and draws a transient open-circle
 * indicator. The indicator is a pure draw-overlay (drawn each redraw from
 * draw.c's DrawTempContent(), see ReportsDrawIndicator()) -- never a real
 * track object, so it can never end up in a saved .xtc/.xtce file and
 * needs no undo/redo handling.
 *
 * The list is also grouped/flagged for "Nearby" endpoints (see
 * ReportsMarkNearby()) and annotated with a free-text Note column --
 * Nearby plus "Isolated" (every endpoint on the row's track is open, a
 * fully disconnected/"ghost" track rather than a deliberate dead-end
 * stub) and "Turntable" (the row's endpoint is a turntable stall, open
 * by design) -- see ReportsFormatEndPtNote() (reportsformat.c). The list
 * can be recomputed in place via the Refresh button
 * (`reportsDialog_t.refreshProc`) without closing/reopening the dialog.
 *
 * Save/Print build their output on demand (ReportsBuildText()/
 * ReportsRefreshPrintText()) into a hidden PD_TEXT-equivalent control
 * (`reportsDialog_t.text`) that's never shown to the user -- see
 * ReportsShowDialog() for why it's created via wTextCreate()'s
 * standalone/no-.ui-needed code path rather than as a builder-bound
 * paramData_t entry.
 *
 * Phase 2 (track lengths/curve stats/turnout density/equipment
 * suitability) is report-only, no click-to-navigate/indicator, and needs
 * its own dialog per report (each
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
 * feature's first PR scoped to new files only; the follow-up cleanup this
 * leaves flagged is extracting a shared helper to app/dynstring/.
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
#include "cJSON.h"
#include "ccurve.h"
#include "dlayer.h"
#include "draw.h"
#include "fileio.h"
#include "layout.h"
#include "form.h"
#include "note.h"
#include "paths.h"
#include "scale.h"
#include "shortentext.h"
#include "track.h"
#include "utility.h"
#include "include/dreportsfilter.h"
#include "include/notenames.h"
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
 * `reportsRefreshProc`); retitling on reuse was flagged in phase 1 as
 * something that would only become relevant once a second report type
 * existed, and deliberately not solved then -- this struct is that
 * resolution, written once a second report type actually existed to prove
 * the shape against rather than speculatively upfront. Each report phase
 * gets its
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
static wBool_t ReportsDlgUpdate(paramGroup_cp pg, int inx, void *valueP);
static void ReportsCancel(paramGroup_cp pg);
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

static wWinPix_t reportsListWidths[] = { 60, 60, 110, 80, 80, 70, 130 };
static const char * reportsListTitles[] = {
	N_("Track"), N_("Layer #"), N_("Layer Name"), N_("X"), N_("Y"), N_("Angle"), N_("Note")
};
static paramListData_t reportsListData = { 8, 300, 7, reportsListWidths, reportsListTitles };

/** This report's layer/group scope -- see dreportsfilter.h. Zero-
 * initialized, so unfiltered (every layer shown) until the user opens
 * the Filter dialog and includes at least one layer. */
static reportsFilter_t reportsFilter;

/** "Filter..." button: open the shared Filter dialog against this
 * report's own reportsFilter. The user clicks Refresh afterward (same as
 * any other change) to see the new scope take effect.
 *
 * \param unused IN unused, required by the PD_BUTTON signature
 */
static void DoReportsFilter(void *unused)
{
	ShowReportsFilterDialog(&reportsFilter);
}

static paramData_t reportsPLs[] = {
#define I_REPORTSSUMMARY (0)
#define reportsSummary (reportsPLs[I_REPORTSSUMMARY].control)
	{ PD_MESSAGE, "", "summary", 0, I2VP(37) },
#define I_REPORTSLIST (1)
#define reportsList (reportsPLs[I_REPORTSLIST].control)
	{ PD_LIST, NULL, "list", 0, &reportsListData, NULL, 0 },
	{ PD_BUTTON, DoReportsOp, "refresh", 0, NULL, NULL, 0, &reportsUnconnectedRefreshOp },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, &reportsUnconnectedSaveOp },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, &reportsUnconnectedPrintOp },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
	{ PD_BUTTON, DoReportsFilter, "filter", 0, NULL, NULL, 0, NULL },
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
static wBool_t ReportsDlgUpdate(paramGroup_cp pg, int inx, void *valueP)
{
	wIndex_t sel;
	reportsEndPt_t *entry;

	if (inx != I_REPORTSLIST) {
		return TRUE;
	}

	/* Ignore selection-changed events GTK fires synchronously mid-clear/
	 * repopulate (ReportsPopulateList()) -- see reportsPopulating's doc
	 * comment. Real crash without this guard, not a hypothetical. */
	if (reportsPopulating) {
		return TRUE;
	}

	sel = wListGetIndex(reportsList);
	if (sel < 0) {
		return TRUE;
	}
	entry = (reportsEndPt_t *)wListGetItemContext(reportsList, sel);
	if (!entry) {
		return TRUE;
	}

	if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
	LOG( log_reports, 1, ( "reports: row %d selected -> track %d @ (%.3f,%.3f)\n",
	                       sel, entry->trackId, entry->pos.x, entry->pos.y ) )

	ReportsSetIndicator(entry->pos, entry->scale);
	return TRUE;
}

/** paramActionCancelProc (phase 1.5): clear the indicator before the
 * dialog's normal cancel/close handling, so it doesn't linger on the
 * canvas after the report window is gone (matches the original design
 * note: the indicator is deleted when the report window closes). */
static void ReportsCancel(paramGroup_cp pg)
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
		rd->pg->reloadProc = rd->refreshProc;
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
		char note[32];

		ReportsFormatEndPtNote( entry, note, sizeof note );
		snprintf( row, sizeof row, "%d\t%u\t%s\t%.3f\t%.3f\t%.3f\t%s",
		          entry->trackId, entry->layer, entry->layerName,
		          entry->pos.x, entry->pos.y, entry->angle,
		          note );
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
		          "\n%6s | %5s | %-16s | %8s | %8s | %7s | %s\n",
		          _("Track"), _("Lyr"), _("Layer Name"), _("X"), _("Y"), _("Angle"),
		          _("Note") );
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
	 * Gaps/Kinked Joints reports (same pattern), then confirmed present
	 * here too. Only needed once the dialog/list actually exists
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
		if ( QueryTrack( trk, Q_ISTRAIN ) ) {
			continue;       // a car is on the track, not track
		}
		EPINX_T ep;
		if ( !ReportsFilterLayerIncluded( &reportsFilter, GetTrkLayer(trk) ) ) {
			continue;
		}
		EPINX_T epCnt = GetTrkEndPtCnt(trk);
		/* Turntable stalls are open by design (QueryTrack(trk,
		 * Q_CAN_ADD_ENDPOINTS) -- same test the Gaps report uses to
		 * exclude them from its own pairing analysis), so they're
		 * never flagged `isolated` below even though every stall is
		 * typically open. */
		BOOL_T isTurntable = QueryTrack( trk, Q_CAN_ADD_ENDPOINTS );
		BOOL_T isIsolated = TRUE;

		for ( ep = 0; ep < epCnt; ep++ ) {
			if ( GetTrkEndTrk(trk, ep) != NULL ) {
				isIsolated = FALSE;
				break;
			}
		}
		isIsolated = isIsolated && !isTurntable;

		for ( ep = 0; ep < epCnt; ep++ ) {
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
				entry->isolated = isIsolated;
				entry->turntable = isTurntable;
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
 * DoReportsOp() plumbing phase 1 uses. The compute pass walks TRK_ITERATE
 * using GetTrkLength()/GetTrkLayer()/GetTrkType()==T_TURNOUT, mirroring
 * the external MCP project's own turnout-density report.
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

/** This report's layer/group scope -- see dreportsfilter.h. */
static reportsFilter_t reportsTurnoutFilter;

/** "Filter..." button -- see DoReportsFilter() above for the shared shape.
 *
 * \param unused IN unused, required by the PD_BUTTON signature
 */
static void DoReportsTurnoutFilter(void *unused)
{
	ShowReportsFilterDialog(&reportsTurnoutFilter);
}

static paramData_t reportsTurnoutPLs[] = {
#define I_REPORTSTURNOUTSUMMARY (0)
#define reportsTurnoutSummary (reportsTurnoutPLs[I_REPORTSTURNOUTSUMMARY].control)
	{ PD_MESSAGE, "", "summary", 0, I2VP(37) },
#define I_REPORTSTURNOUTLIST (1)
#define reportsTurnoutList (reportsTurnoutPLs[I_REPORTSTURNOUTLIST].control)
	{ PD_LIST, NULL, "list", 0, &reportsTurnoutListData, NULL, 0 },
	{ PD_BUTTON, DoReportsOp, "refresh", 0, NULL, NULL, 0, &reportsTurnoutRefreshOp },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, &reportsTurnoutSaveOp },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, &reportsTurnoutPrintOp },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
	{ PD_BUTTON, DoReportsTurnoutFilter, "filter", 0, NULL, NULL, 0, NULL },
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
 * text only (ReportsBuildTurnoutText()), never a second interactive list:
 * Turnout Density has no click-to-navigate at all, so a second list
 * would need dialog machinery this report doesn't otherwise use. */
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
		if ( QueryTrack( trk, Q_ISTRAIN ) ) {
			continue;       // a car is on the track, not track
		}
		unsigned int trkLayer = GetTrkLayer(trk);
		DIST_T lengthFt;
		if ( !ReportsFilterLayerIncluded( &reportsTurnoutFilter, trkLayer ) ) {
			continue;
		}
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
		lengthFt = (GetTrkEndPtCnt(trk) >= 2) ?
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
 * calling GetTrkLength() -- see SF #776's fix; this report's own
 * synthetic fixture also
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

/** This report's layer/group scope -- see dreportsfilter.h. */
static reportsFilter_t reportsTrackLenFilter;

/** "Filter..." button -- see DoReportsFilter() above for the shared shape.
 *
 * \param unused IN unused, required by the PD_BUTTON signature
 */
static void DoReportsTrackLenFilter(void *unused)
{
	ShowReportsFilterDialog(&reportsTrackLenFilter);
}

static paramData_t reportsTrackLenPLs[] = {
#define I_REPORTSTRACKLENSUMMARY (0)
#define reportsTrackLenSummary (reportsTrackLenPLs[I_REPORTSTRACKLENSUMMARY].control)
	{ PD_MESSAGE, "", "summary", 0, I2VP(37) },
#define I_REPORTSTRACKLENLIST (1)
#define reportsTrackLenList (reportsTrackLenPLs[I_REPORTSTRACKLENLIST].control)
	{ PD_LIST, NULL, "list", 0, &reportsTrackLenListData, NULL, 0 },
	{ PD_BUTTON, DoReportsOp, "refresh", 0, NULL, NULL, 0, &reportsTrackLenRefreshOp },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, &reportsTrackLenSaveOp },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, &reportsTrackLenPrintOp },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
	{ PD_BUTTON, DoReportsTrackLenFilter, "filter", 0, NULL, NULL, 0, NULL },
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
		const reportsTrackLenLayer_t *entry = &DYNARR_N(reportsTrackLenLayer_t,
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
		if ( QueryTrack( trk, Q_ISTRAIN ) ) {
			continue;       // a car is on the track, not track
		}
		unsigned int trkLayer = GetTrkLayer(trk);
		DIST_T lengthFt;
		BOOL_T isTurnout;
		if ( !ReportsFilterLayerIncluded( &reportsTrackLenFilter, trkLayer ) ) {
			continue;
		}
		/* Same guard as ReportsTurnoutDensity() -- see that function's
		 * comment for the real crash this prevents. */
		lengthFt = (GetTrkEndPtCnt(trk) >= 2) ?
		           GetTrkLength(trk, 0, 1) / 12.0 : 0.0;
		isTurnout = (GetTrkType(trk) == T_TURNOUT);

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

/** This report's layer/group scope -- see dreportsfilter.h. */
static reportsFilter_t reportsCurveFilter;

/** "Filter..." button -- see DoReportsFilter() above for the shared shape.
 *
 * \param unused IN unused, required by the PD_BUTTON signature
 */
static void DoReportsCurveFilter(void *unused)
{
	ShowReportsFilterDialog(&reportsCurveFilter);
}

static paramData_t reportsCurvePLs[] = {
#define I_REPORTSCURVESUMMARY (0)
#define reportsCurveSummary (reportsCurvePLs[I_REPORTSCURVESUMMARY].control)
	{ PD_MESSAGE, "", "summary", 0, I2VP(37) },
#define I_REPORTSCURVELIST (1)
#define reportsCurveList (reportsCurvePLs[I_REPORTSCURVELIST].control)
	{ PD_LIST, NULL, "list", 0, &reportsCurveListData, NULL, 0 },
	{ PD_BUTTON, DoReportsOp, "refresh", 0, NULL, NULL, 0, &reportsCurveRefreshOp },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, &reportsCurveSaveOp },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, &reportsCurvePrintOp },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
	{ PD_BUTTON, DoReportsCurveFilter, "filter", 0, NULL, NULL, 0, NULL },
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
		const reportsCurveBucket_t *entry = &DYNARR_N(reportsCurveBucket_t,
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
		if ( !ReportsFilterLayerIncluded( &reportsCurveFilter, GetTrkLayer(trk) ) ) {
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

/** This report's layer/group scope -- see dreportsfilter.h. */
static reportsFilter_t reportsEquipFilter;

/** "Filter..." button -- see DoReportsFilter() above for the shared shape.
 *
 * \param unused IN unused, required by the PD_BUTTON signature
 */
static void DoReportsEquipFilter(void *unused)
{
	ShowReportsFilterDialog(&reportsEquipFilter);
}

static paramData_t reportsEquipPLs[] = {
#define I_REPORTSEQUIPSUMMARY (0)
#define reportsEquipSummary (reportsEquipPLs[I_REPORTSEQUIPSUMMARY].control)
	{ PD_MESSAGE, "", "summary", 0, I2VP(37) },
#define I_REPORTSEQUIPLIST (1)
#define reportsEquipList (reportsEquipPLs[I_REPORTSEQUIPLIST].control)
	{ PD_LIST, NULL, "list", 0, &reportsEquipListData, NULL, 0 },
	{ PD_BUTTON, DoReportsOp, "refresh", 0, NULL, NULL, 0, &reportsEquipRefreshOp },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, &reportsEquipSaveOp },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, &reportsEquipPrintOp },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
	{ PD_BUTTON, DoReportsEquipFilter, "filter", 0, NULL, NULL, 0, NULL },
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
		const reportsEquipClass_t *entry = &DYNARR_N(reportsEquipClass_t,
		                                   reportsEquipList_da,
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
		if ( !ReportsFilterLayerIncluded( &reportsEquipFilter, GetTrkLayer(trk) ) ) {
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
		for ( size_t i = 0; i < REPORTS_EQUIP_COUNT; i++ ) {
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
 * fields since phase 1 introduced them.
 *
 * Both share the phase-1.5 indicator statics
 * (reportIndicatorActive/Pos/Scale, ReportsDrawIndicator()) with phase 1
 * and each other -- that state is a single set of module-level globals,
 * not per-dialog, so if two interactive report dialogs are open at once
 * and a row is clicked in each, the second click's indicator simply
 * replaces the first's (last-clicked-anywhere wins, globally). This was
 * already true with only phase 1 shipped; it just wasn't reachable until
 * a second interactive dialog existed. Not fixed here (would mean keying
 * the indicator state off the reportsDialog_t itself) -- a known
 * limitation, not yet worth a dedicated fix given how rarely two
 * interactive report dialogs are open at once.
 * ------------------------------------------------------------------- */

/** Tentative declaration -- same reason as reportsPG above. */
static paramGroup_t reportsGapsPG;
static void ReportsBuildGapsText(DynString *out);
static wBool_t ReportsDlgUpdateGaps(paramGroup_cp pg, int inx, void *valueP);
static void ReportsCancelGaps(paramGroup_cp pg);

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

/** This report's layer/group scope -- see dreportsfilter.h. */
static reportsFilter_t reportsGapsFilter;

/** "Filter..." button -- see DoReportsFilter() above for the shared shape.
 *
 * \param unused IN unused, required by the PD_BUTTON signature
 */
static void DoReportsGapsFilter(void *unused)
{
	ShowReportsFilterDialog(&reportsGapsFilter);
}

/** Development Issues #85: the Gaps report's own "Maximum gap" choice.
 * Index 0 is the Connection Distance preference (the editor's own snap
 * distance, and this report's only threshold before); the others widen the
 * report without changing how the editor connects track. Real layouts have
 * missed joins a little wider than the 0.1 in default. Saved as a
 * preference (reportsgaps.maxgap). */
static wIndex_t reportsGapsMaxGapInx = 0;
static const DIST_T reportsGapsMaxGapValues[] = { 0.0, 0.25, 0.5, 1.0 };
#define REPORTS_GAPS_MAXGAP_COUNT \
	((wIndex_t)(sizeof reportsGapsMaxGapValues / sizeof reportsGapsMaxGapValues[0]))

static paramData_t reportsGapsPLs[] = {
#define I_REPORTSGAPSSUMMARY (0)
#define reportsGapsSummary (reportsGapsPLs[I_REPORTSGAPSSUMMARY].control)
	{ PD_MESSAGE, "", "summary", 0, I2VP(37) },
#define I_REPORTSGAPSMAXGAP (1)
#define reportsGapsMaxGap (reportsGapsPLs[I_REPORTSGAPSMAXGAP].control)
	{ PD_DROPLIST, &reportsGapsMaxGapInx, "maxgap", PDO_LISTINDEX, I2VP(200), NULL, 0 },
#define I_REPORTSGAPSLIST (2)
#define reportsGapsList (reportsGapsPLs[I_REPORTSGAPSLIST].control)
	{ PD_LIST, NULL, "list", 0, &reportsGapsListData, NULL, 0 },
	{ PD_BUTTON, DoReportsOp, "refresh", 0, NULL, NULL, 0, &reportsGapsRefreshOp },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, &reportsGapsSaveOp },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, &reportsGapsPrintOp },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
	{ PD_BUTTON, DoReportsGapsFilter, "filter", 0, NULL, NULL, 0, NULL },
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

/** The distance the Gaps report pairs open endpoints within: the Maximum
 * gap choice, never less than the Connection Distance preference (a pair
 * the editor would connect is always reported). */
static DIST_T ReportsGapsMaxGap(void)
{
	DIST_T dist = 0.0;

	if ( reportsGapsMaxGapInx > 0 &&
	     reportsGapsMaxGapInx < REPORTS_GAPS_MAXGAP_COUNT ) {
		dist = reportsGapsMaxGapValues[reportsGapsMaxGapInx];
	}
	return dist > connectDistance ? dist : connectDistance;
}

/** Fill the Maximum gap dropdown and select the current choice. Rebuilt on
 * each run, so the Connection distance entry shows the preference's
 * current value. */
static void ReportsPopulateMaxGapList(void)
{
	char label[64];
	wIndex_t i;

	if ( reportsGapsMaxGapInx < 0 ||
	     reportsGapsMaxGapInx >= REPORTS_GAPS_MAXGAP_COUNT ) {
		reportsGapsMaxGapInx = 0;       // a stale or edited preference
	}
	reportsGapsPopulating = TRUE;
	wListClear( reportsGapsMaxGap );
	for ( i = 0; i < REPORTS_GAPS_MAXGAP_COUNT; i++ ) {
		if ( i == 0 ) {
			snprintf( label, sizeof label, _("Connection distance (%.2f in)"),
			          connectDistance );
		} else {
			snprintf( label, sizeof label, _("%.2f in"), reportsGapsMaxGapValues[i] );
		}
		wComboBoxAddValue( reportsGapsMaxGap, label, I2VP((int)i) );
	}
	wListSetIndex( reportsGapsMaxGap, reportsGapsMaxGapInx );
	reportsGapsPopulating = FALSE;
}

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
static wBool_t ReportsDlgUpdateGaps(paramGroup_cp pg, int inx, void *valueP)
{
	wIndex_t sel;
	reportsGapPair_t *entry;
	coOrd mid;

	if (reportsGapsPopulating) {
		return TRUE;
	}
	/* A new Maximum gap takes effect at once, like the Notes report's
	 * filters (SF #802), without a separate Refresh. */
	if (inx == I_REPORTSGAPSMAXGAP) {
		ReportsGaps(NULL);
		return TRUE;
	}
	if (inx != I_REPORTSGAPSLIST) {
		return TRUE;
	}

	sel = wListGetIndex(reportsGapsList);
	if (sel < 0) {
		return TRUE;
	}
	entry = (reportsGapPair_t *)wListGetItemContext(reportsGapsList, sel);
	if (!entry) {
		return TRUE;
	}

	if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
	LOG( log_reports, 1,
	     ( "reports: gaps row %d selected -> track %d <-> %d, gap=%.4f\n",
	       sel, entry->trackA, entry->trackB, entry->gapIn ) )

	mid.x = (entry->posA.x + entry->posB.x) / 2.0;
	mid.y = (entry->posA.y + entry->posB.y) / 2.0;
	ReportsSetIndicator(mid, entry->scale);
	return TRUE;
}

/** paramActionCancelProc for the Gaps dialog -- same shape as phase 1's
 * ReportsCancel(). */
static void ReportsCancelGaps(paramGroup_cp pg)
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
	char summaryLine[300];
	DIST_T maxGap = ReportsGapsMaxGap();

	DynStringMalloc( out, 256 );
	ReportsAddHeader( out, _("Gaps Report") );

	snprintf( summaryLine, sizeof summaryLine,
	          "\n%s: %.4f in%s\n%s: %d\n%s: %d\n%s: %d\n",
	          _("Maximum gap"), maxGap,
	          maxGap == connectDistance ? _(" (Connection distance)") : "",
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
	DIST_T maxGap = ReportsGapsMaxGap();

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
		if ( QueryTrack( trk, Q_ISTRAIN ) ) {
			continue;       // a car is on the track, not track
		}
		EPINX_T ep;
		/* Turntable stalls are open by design -- excluded from the
		 * pairing analysis entirely (counted separately), matching the
		 * external MCP project's own write_gaps_report() design. Unlike
		 * phase 1's own Unconnected Endpoints report (which lists every
		 * open endpoint including turntable stalls), this is a
		 * deliberate divergence between the two reports, not a bug to
		 * reconcile. */
		BOOL_T isTurntable = QueryTrack( trk, Q_CAN_ADD_ENDPOINTS );

		for ( ep = 0; ep < GetTrkEndPtCnt(trk); ep++ ) {
			if ( GetTrkEndTrk(trk, ep) != NULL ) {
				continue;
			}
			if ( isTurntable ) {
				turntableCnt++;
				continue;
			}
			if ( !ReportsFilterLayerIncluded( &reportsGapsFilter, GetTrkLayer(trk) ) ) {
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
			if ( dist <= maxGap ) {
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
		     ( "reports: Gaps computed -- %d pair(s) within %.4f, %d isolated, %d turntable stall(s) excluded\n",
		       reportsGapsList_da.cnt, maxGap, isolatedCnt, turntableCnt ) )

		ReportsShowDialog( &reportsGapsDlg, _("Gaps Report") );
		ReportsPopulateMaxGapList();
		snprintf( summary, sizeof summary,
		          _("%d near-miss pair(s) within %.2f in, %d isolated open endpoint(s), %d turntable stall(s) excluded"),
		          reportsGapsList_da.cnt, maxGap, isolatedCnt, turntableCnt );
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
static wBool_t ReportsDlgUpdateKinked(paramGroup_cp pg, int inx, void *valueP);
static void ReportsCancelKinked(paramGroup_cp pg);

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

/** This report's layer/group scope -- see dreportsfilter.h. */
static reportsFilter_t reportsKinkedFilter;

/** "Filter..." button -- see DoReportsFilter() above for the shared shape.
 *
 * \param unused IN unused, required by the PD_BUTTON signature
 */
static void DoReportsKinkedFilter(void *unused)
{
	ShowReportsFilterDialog(&reportsKinkedFilter);
}

static paramData_t reportsKinkedPLs[] = {
#define I_REPORTSKINKEDSUMMARY (0)
#define reportsKinkedSummary (reportsKinkedPLs[I_REPORTSKINKEDSUMMARY].control)
	{ PD_MESSAGE, "", "summary", 0, I2VP(37) },
#define I_REPORTSKINKEDLIST (1)
#define reportsKinkedList (reportsKinkedPLs[I_REPORTSKINKEDLIST].control)
	{ PD_LIST, NULL, "list", 0, &reportsKinkedListData, NULL, 0 },
	{ PD_BUTTON, DoReportsOp, "refresh", 0, NULL, NULL, 0, &reportsKinkedRefreshOp },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, &reportsKinkedSaveOp },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, &reportsKinkedPrintOp },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
	{ PD_BUTTON, DoReportsKinkedFilter, "filter", 0, NULL, NULL, 0, NULL },
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
static wBool_t ReportsDlgUpdateKinked(paramGroup_cp pg, int inx, void *valueP)
{
	wIndex_t sel;
	reportsKinkedJoint_t *entry;

	if (inx != I_REPORTSKINKEDLIST) {
		return TRUE;
	}
	if (reportsKinkedPopulating) {
		return TRUE;
	}

	sel = wListGetIndex(reportsKinkedList);
	if (sel < 0) {
		return TRUE;
	}
	entry = (reportsKinkedJoint_t *)wListGetItemContext(reportsKinkedList, sel);
	if (!entry) {
		return TRUE;
	}

	if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
	LOG( log_reports, 1,
	     ( "reports: kinked row %d selected -> track %d <-> %d, delta=%.3f\n",
	       sel, entry->trackA, entry->trackB, entry->angleDelta ) )

	ReportsSetIndicator(entry->pos, entry->scale);
	return TRUE;
}

/** paramActionCancelProc for the Kinked Joints dialog. */
static void ReportsCancelKinked(paramGroup_cp pg)
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
		if ( QueryTrack( trk, Q_ISTRAIN ) ) {
			continue;       // a car is on the track, not track
		}
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
			/* A kinked joint spans two tracks that may be on different
			 * layers -- only counted if BOTH sides are in scope, matching
			 * ReportsGaps()'s equivalent "both endpoints must be included"
			 * behavior for its own cross-track pairs. */
			if ( !ReportsFilterLayerIncluded( &reportsKinkedFilter, GetTrkLayer(trk) ) ||
			     !ReportsFilterLayerIncluded( &reportsKinkedFilter, GetTrkLayer(other) ) ) {
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

/* ---------------------------------------------------------------------
 * Short Tracks Report (Development Issues #86). Interactive, the same
 * shape as Gaps: a Maximum length choice (saved as a preference,
 * reportsshort.maxlen), the shared layer/group filter, and rows that pan
 * to the track. Joining two tracks can insert a short piece, and it goes
 * on the current layer rather than on the layer of the tracks it joins;
 * such a piece is often too small to see without hiding layers. Every
 * short track is listed; one on none of its connected tracks' layers is
 * flagged and listed first (see reportsShortTrack_t).
 * ------------------------------------------------------------------- */

/** Tentative declaration -- same reason as reportsPG above. */
static paramGroup_t reportsShortPG;
static void ReportsBuildShortText(DynString *out);
static wBool_t ReportsDlgUpdateShort(paramGroup_cp pg, int inx, void *valueP);
static void ReportsCancelShort(paramGroup_cp pg);

static reportsDialog_t reportsShortDlg = {
	&reportsShortPG, NULL, NULL, NULL,
	ReportsBuildShortText, ReportsShortTracks,
	ReportsDlgUpdateShort, ReportsCancelShort
};
static reportsOpCtx_t reportsShortRefreshOp = { &reportsShortDlg, REPORTSOP_REFRESH };
static reportsOpCtx_t reportsShortSaveOp    = { &reportsShortDlg, REPORTSOP_SAVE };
static reportsOpCtx_t reportsShortPrintOp   = { &reportsShortDlg, REPORTSOP_PRINT };

static wWinPix_t reportsShortListWidths[] = { 60, 80, 80, 60, 100, 200, 160 };
static const char * reportsShortListTitles[] = {
	N_("Track"), N_("Type"), N_("Length (in)"), N_("Layer #"), N_("Layer Name"),
	N_("Connected Layers"), N_("Expected Layer")
};
static paramListData_t reportsShortListData = { 8, 400, 7, reportsShortListWidths,
                                                reportsShortListTitles
                                              };

/** This report's layer/group scope -- see dreportsfilter.h. */
static reportsFilter_t reportsShortFilter;

/** "Filter..." button -- see DoReportsFilter() above for the shared shape.
 *
 * \param unused IN unused, required by the PD_BUTTON signature
 */
static void DoReportsShortFilter(void *unused)
{
	ShowReportsFilterDialog(&reportsShortFilter);
}

/** The Maximum length choice, in layout inches. 1 in (index 1) by default:
 * the filler pieces found on a real layout were 0.2 to 0.7 in long. */
static wIndex_t reportsShortMaxLenInx = 1;
static const DIST_T reportsShortMaxLenValues[] = { 0.5, 1.0, 2.0, 4.0 };
#define REPORTS_SHORT_MAXLEN_COUNT \
	((wIndex_t)(sizeof reportsShortMaxLenValues / sizeof reportsShortMaxLenValues[0]))

static paramData_t reportsShortPLs[] = {
#define I_REPORTSSHORTSUMMARY (0)
#define reportsShortSummary (reportsShortPLs[I_REPORTSSHORTSUMMARY].control)
	{ PD_MESSAGE, "", "summary", 0, I2VP(37) },
#define I_REPORTSSHORTMAXLEN (1)
#define reportsShortMaxLen (reportsShortPLs[I_REPORTSSHORTMAXLEN].control)
	{ PD_DROPLIST, &reportsShortMaxLenInx, "maxlen", PDO_LISTINDEX, I2VP(120), NULL, 0 },
#define I_REPORTSSHORTLIST (2)
#define reportsShortList (reportsShortPLs[I_REPORTSSHORTLIST].control)
	{ PD_LIST, NULL, "list", 0, &reportsShortListData, NULL, 0 },
	{ PD_BUTTON, DoReportsOp, "refresh", 0, NULL, NULL, 0, &reportsShortRefreshOp },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, &reportsShortSaveOp },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, &reportsShortPrintOp },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
	{ PD_BUTTON, DoReportsShortFilter, "filter", 0, NULL, NULL, 0, NULL },
};
static paramGroup_t reportsShortPG = { "reportsshort", PGO_FULLDIALOGFROMBUILDER,
                                       reportsShortPLs, COUNT( reportsShortPLs )
                                     };

/** The current report's short tracks -- each row's list context points
 * into it, so it outlives the compute pass (same rule as
 * reportsGapsList_da). Replaced, old rows freed first, on each run. */
static dynArr_t reportsShortList_da;

/** How many rows are flagged (on none of their connected tracks' layers). */
static int reportsShortFlaggedCnt = 0;

/** TRUE while the list or the Maximum length dropdown is being rebuilt --
 * same re-entrancy guard as reportsGapsPopulating. */
static BOOL_T reportsShortPopulating = FALSE;

/** The Maximum length choice in inches; a stale or edited preference
 * falls back to the default. */
static DIST_T ReportsShortMaxLenValue(void)
{
	if ( reportsShortMaxLenInx < 0 ||
	     reportsShortMaxLenInx >= REPORTS_SHORT_MAXLEN_COUNT ) {
		reportsShortMaxLenInx = 1;
	}
	return reportsShortMaxLenValues[reportsShortMaxLenInx];
}

/** Fill the Maximum length dropdown and select the current choice. */
static void ReportsPopulateMaxLenList(void)
{
	char label[32];
	wIndex_t i;

	(void) ReportsShortMaxLenValue();       // clamps the index
	reportsShortPopulating = TRUE;
	wListClear( reportsShortMaxLen );
	for ( i = 0; i < REPORTS_SHORT_MAXLEN_COUNT; i++ ) {
		snprintf( label, sizeof label, _("%.1f in"), reportsShortMaxLenValues[i] );
		wComboBoxAddValue( reportsShortMaxLen, label, I2VP((int)i) );
	}
	wListSetIndex( reportsShortMaxLen, reportsShortMaxLenInx );
	reportsShortPopulating = FALSE;
}

/** "<layer> <name>" for each of cnt layers, joined by sep, into buf. */
static void ReportsShortLayersText(char *buf, size_t size, int cnt,
                                   const unsigned int *layers,
                                   const char * const *names, const char *sep)
{
	int i;
	size_t len = 0;

	buf[0] = '\0';
	for ( i = 0; i < cnt && len < size; i++ ) {
		int n = snprintf( buf + len, size - len, "%s%u %s", i > 0 ? sep : "",
		                  layers[i], names[i] ? names[i] : "" );
		if ( n < 0 ) {
			break;
		}
		len += (size_t)n;
	}
}

/** Populate the interactive list from reportsShortList_da, each row's
 * context set to its entry so ReportsDlgUpdateShort() can recover it. */
static void ReportsPopulateShortList(void)
{
	int i;
	char row[400];
	char connected[160];
	char expected[160];

	reportsShortPopulating = TRUE;
	wListClear( reportsShortList );
	for ( i = 0; i < reportsShortList_da.cnt; i++ ) {
		reportsShortTrack_t *entry =
		        &DYNARR_N(reportsShortTrack_t, reportsShortList_da, i);

		if ( entry->nbrCnt > 0 ) {
			ReportsShortLayersText( connected, sizeof connected, entry->nbrCnt,
			                        entry->nbrLayer, entry->nbrLayerName, ", " );
		} else {
			snprintf( connected, sizeof connected, "-" );
		}
		expected[0] = '\0';
		if ( entry->flagged ) {
			ReportsShortLayersText( expected, sizeof expected, entry->expectedCnt,
			                        entry->expectedLayer, entry->expectedLayerName, " / " );
		}
		snprintf( row, sizeof row, "%d\t%s\t%.3f\t%u\t%s\t%s\t%s",
		          entry->track, entry->typeName, entry->lengthIn,
		          entry->layer, entry->layerName ? entry->layerName : "",
		          connected, expected );
		wListAddValue( reportsShortList, row, NULL, entry );
	}
	reportsShortPopulating = FALSE;
}

/** paramGroup_t changeProc: a new Maximum length takes effect at once (as
 * in Gaps); selecting a row pans/indicates at the track's midpoint. */
static wBool_t ReportsDlgUpdateShort(paramGroup_cp pg, int inx, void *valueP)
{
	wIndex_t sel;
	reportsShortTrack_t *entry;

	if (reportsShortPopulating) {
		return TRUE;
	}
	if (inx == I_REPORTSSHORTMAXLEN) {
		ReportsShortTracks(NULL);
		return TRUE;
	}
	if (inx != I_REPORTSSHORTLIST) {
		return TRUE;
	}
	sel = wListGetIndex(reportsShortList);
	if (sel < 0) {
		return TRUE;
	}
	entry = (reportsShortTrack_t *)wListGetItemContext(reportsShortList, sel);
	if (!entry) {
		return TRUE;
	}
	if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
	LOG( log_reports, 1,
	     ( "reports: short track row %d selected -> track %d, length=%.4f\n",
	       sel, entry->track, entry->lengthIn ) )
	ReportsSetIndicator(entry->pos, entry->scale);
	return TRUE;
}

/** paramActionCancelProc -- same shape as ReportsCancelGaps(). */
static void ReportsCancelShort(paramGroup_cp pg)
{
	ReportsClearIndicator();
	FormCancel_Current(pg);
}

/** Build the formatted report text for Save/Print. */
static void ReportsBuildShortText(DynString *out)
{
	char summaryLine[200];

	DynStringMalloc( out, 256 );
	ReportsAddHeader( out, _("Short Tracks Report") );

	snprintf( summaryLine, sizeof summaryLine, "\n%s: %.2f in\n%s: %d\n%s: %d\n",
	          _("Maximum length"), ReportsShortMaxLenValue(),
	          _("Short tracks"), reportsShortList_da.cnt,
	          _("On an unexpected layer"), reportsShortFlaggedCnt );
	DynStringCatCStr( out, summaryLine );

	if ( reportsShortList_da.cnt == 0 ) {
		DynStringCatCStrs( out, "\n", _("No short tracks found."), "\n", NULL );
	} else {
		char headerLine[160];
		snprintf( headerLine, sizeof headerLine,
		          "\n%6s | %-9s | %8s | %5s | %-14s | %s | %s\n",
		          _("Track"), _("Type"), _("Length"), _("Layer"), _("Layer Name"),
		          _("Connected Layers"), _("Expected Layer") );
		DynStringCatCStr( out, headerLine );
		ReportsFormatShortList( out,
		                        &DYNARR_N(reportsShortTrack_t, reportsShortList_da, 0),
		                        reportsShortList_da.cnt );
	}
}

/** Flagged rows first, then by track index. */
static int ReportsShortCompare(const void *a, const void *b)
{
	const reportsShortTrack_t *ra = a;
	const reportsShortTrack_t *rb = b;

	if ( ra->flagged != rb->flagged ) {
		return ra->flagged ? -1 : 1;
	}
	return (ra->track > rb->track) - (ra->track < rb->track);
}

void ReportsShortTracks( void * unused )
{
	track_p trk;
	DIST_T maxLen = ReportsShortMaxLenValue();

	ReportsClearIndicator();

	/* Clear the visible list before freeing the array its rows' contexts
	 * point into -- see ReportsUnconnectedEndpoints(). */
	if ( reportsShortDlg.window != NULL ) {
		reportsShortPopulating = TRUE;
		wListClear( reportsShortList );
		reportsShortPopulating = FALSE;
	}
	DYNARR_FREE( reportsShortTrack_t, reportsShortList_da );
	DYNARR_INIT( reportsShortTrack_t, reportsShortList_da );
	reportsShortFlaggedCnt = 0;

	TRK_ITERATE( trk ) {
		reportsShortTrack_t *entry;
		DIST_T len;
		coOrd p0, p1;
		EPINX_T ep;
		int i;

		if ( QueryTrack( trk, Q_ISTRAIN ) ) {
			continue;       // a car is on the track, not track
		}
		/* Two endpoints only: turnouts are what fillers join, and a
		 * turntable's stalls are open by design. Benchwork, notes and
		 * other objects without endpoints never get here. */
		if ( GetTrkEndPtCnt(trk) != 2 || QueryTrack( trk, Q_CAN_ADD_ENDPOINTS ) ) {
			continue;
		}
		if ( !ReportsFilterLayerIncluded( &reportsShortFilter, GetTrkLayer(trk) ) ) {
			continue;
		}
		len = GetTrkLength( trk, 0, 1 );
		if ( len >= maxLen ) {
			continue;
		}

		DYNARR_APPEND( reportsShortTrack_t, reportsShortList_da, 10 );
		entry = &DYNARR_LAST( reportsShortTrack_t, reportsShortList_da );
		memset( entry, 0, sizeof *entry );
		entry->track = GetTrkIndex(trk);
		entry->typeName = GetTrkTypeName(trk);
		entry->lengthIn = len;
		p0 = GetTrkEndPos(trk, 0);
		p1 = GetTrkEndPos(trk, 1);
		entry->pos.x = (p0.x + p1.x) / 2.0;
		entry->pos.y = (p0.y + p1.y) / 2.0;
		entry->scale = GetTrkScale(trk);
		entry->layer = GetTrkLayer(trk) + 1;
		entry->layerName = GetLayerName(GetTrkLayer(trk));

		for ( ep = 0; ep < 2; ep++ ) {
			track_p other = GetTrkEndTrk(trk, ep);
			if ( other == NULL ) {
				continue;
			}
			entry->nbrLayer[entry->nbrCnt] = GetTrkLayer(other) + 1;
			entry->nbrLayerName[entry->nbrCnt] = GetLayerName(GetTrkLayer(other));
			entry->nbrCnt++;
		}

		/* A short piece belongs on the layer of a track it is connected
		 * to, any of them (user 2026-11-18); flagged otherwise. */
		entry->flagged = entry->nbrCnt > 0;
		for ( i = 0; i < entry->nbrCnt; i++ ) {
			if ( entry->nbrLayer[i] == entry->layer ) {
				entry->flagged = FALSE;
			}
		}
		if ( entry->flagged ) {
			for ( i = 0; i < entry->nbrCnt; i++ ) {
				if ( entry->expectedCnt == 1 &&
				     entry->expectedLayer[0] == entry->nbrLayer[i] ) {
					continue;       // both ends on the same layer
				}
				entry->expectedLayer[entry->expectedCnt] = entry->nbrLayer[i];
				entry->expectedLayerName[entry->expectedCnt] = entry->nbrLayerName[i];
				entry->expectedCnt++;
			}
			reportsShortFlaggedCnt++;
		}
	}

	if ( reportsShortList_da.cnt > 1 ) {
		qsort( reportsShortList_da.ptr, (size_t)reportsShortList_da.cnt,
		       sizeof(reportsShortTrack_t), ReportsShortCompare );
	}

	{
		char summary[200];

		if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
		LOG( log_reports, 1,
		     ( "reports: Short Tracks computed -- %d under %.2f in, %d flagged\n",
		       reportsShortList_da.cnt, maxLen, reportsShortFlaggedCnt ) )

		ReportsShowDialog( &reportsShortDlg, _("Short Tracks Report") );
		ReportsPopulateMaxLenList();
		snprintf( summary, sizeof summary,
		          _("%d short track(s) under %.1f in, %d on an unexpected layer"),
		          reportsShortList_da.cnt, maxLen, reportsShortFlaggedCnt );
		wMessageSetValue( reportsShortSummary, summary );
	}

	ReportsPopulateShortList();
}

/* ---------------------------------------------------------------------
 * Track Clearance Report (Development Issues #87). Interactive, the same
 * shape as Short Tracks: a Clearance choice (saved as a preference,
 * reportsclearance.clearance), the shared layer/group filter, and rows that
 * pan to the place. It lists close runs (see reportsClearanceRun_t): two
 * tracks side by side, within REPORTS_CLEARANCE_MAX_ANGLE of parallel,
 * closer than the clearance over at least REPORTS_CLEARANCE_MIN_RUN. That
 * one rule covers a turnout leg that levels off too close to the other leg
 * and two unconnected tracks laid too close; near a frog the diverging
 * track is close but at the frog angle, so it is not a run. Tracks at
 * different heights (more than REPORTS_CLEARANCE_MIN_HEIGHT apart: decks
 * above each other) are left out. The MCP server's capacity measurement
 * uses the same definition.
 * ------------------------------------------------------------------- */

/** Tentative declaration -- same reason as reportsPG above. */
static paramGroup_t reportsClearancePG;
static void ReportsBuildClearanceText(DynString *out);
static wBool_t ReportsDlgUpdateClearance(paramGroup_cp pg, int inx,
                void *valueP);
static void ReportsCancelClearance(paramGroup_cp pg);

static reportsDialog_t reportsClearanceDlg = {
	&reportsClearancePG, NULL, NULL, NULL,
	ReportsBuildClearanceText, ReportsTrackClearance,
	ReportsDlgUpdateClearance, ReportsCancelClearance
};
static reportsOpCtx_t reportsClearanceSaveOp    = { &reportsClearanceDlg, REPORTSOP_SAVE };
static reportsOpCtx_t reportsClearancePrintOp   = { &reportsClearanceDlg, REPORTSOP_PRINT };

static wWinPix_t reportsClearanceListWidths[] = { 50, 70, 60, 50, 110, 60, 50, 110, 80, 80 };
static const char * reportsClearanceListTitles[] = {
	N_("Place"), N_("Change"),
	N_("Track A"), N_("Layer #"), N_("Layer Name"),
	N_("Track B"), N_("Layer #"), N_("Layer Name"),
	N_("Spacing (in)"), N_("Length (in)")
};
static paramListData_t reportsClearanceListData = { 8, 700, 10, reportsClearanceListWidths,
                                                    reportsClearanceListTitles
                                                  };

/** This report's layer/group scope -- see dreportsfilter.h. */
static reportsFilter_t reportsClearanceFilter;

/** "Filter..." button -- see DoReportsFilter() above for the shared shape.
 *
 * \param unused IN unused, required by the PD_BUTTON signature
 */
static void DoReportsClearanceFilter(void *unused)
{
	ShowReportsFilterDialog(&reportsClearanceFilter);
}

/** TRUE while a Refresh button press recomputes: only then are rows
 * compared with the report before it. Opening the report, changing the
 * Clearance or loading another layout starts afresh, without marks. */
static BOOL_T reportsClearanceMarkChanges = FALSE;

/** "Refresh" button: recompute and mark what changed since the last run.
 *
 * \param unused IN unused, required by the PD_BUTTON signature
 */
static void DoReportsClearanceRefresh(void *unused)
{
	reportsClearanceMarkChanges = TRUE;
	ReportsTrackClearance(NULL);
	reportsClearanceMarkChanges = FALSE;
}

/** The Clearance choice: prototype distance between track centres, in feet.
 * 13 ft (index 1) by default, US practice for a clearance point. */
static wIndex_t reportsClearanceInx = 1;
static const DIST_T reportsClearanceFtValues[] = { 12.0, 13.0, 14.0, 15.0 };
#define REPORTS_CLEARANCE_COUNT \
	((wIndex_t)(sizeof reportsClearanceFtValues / sizeof reportsClearanceFtValues[0]))

/** Two tracks this close to parallel (degrees) are side by side. */
#define REPORTS_CLEARANCE_MAX_ANGLE (5.0)
/** A close run at least this long (HO inches, scaled) is listed. */
#define REPORTS_CLEARANCE_MIN_RUN (1.0)
/** Distance between samples along a track (HO inches, scaled). */
#define REPORTS_CLEARANCE_STEP (0.25)
/** Tracks this far apart in height (HO inches, scaled; about a car's
 * height) can't foul each other: an upper deck above a lower one. */
#define REPORTS_CLEARANCE_MIN_HEIGHT (2.0)
/** Runs this close (HO inches, scaled) on shared or joined tracks are one
 * place. */
#define REPORTS_CLEARANCE_PLACE_GAP (2.0)

static paramData_t reportsClearancePLs[] = {
#define I_REPORTSCLEARANCESUMMARY (0)
#define reportsClearanceSummary (reportsClearancePLs[I_REPORTSCLEARANCESUMMARY].control)
	{ PD_MESSAGE, "", "summary", 0, I2VP(37) },
#define I_REPORTSCLEARANCECHOICE (1)
#define reportsClearanceChoice (reportsClearancePLs[I_REPORTSCLEARANCECHOICE].control)
	{ PD_DROPLIST, &reportsClearanceInx, "clearance", PDO_LISTINDEX, I2VP(160), NULL, 0 },
#define I_REPORTSCLEARANCELIST (2)
#define reportsClearanceList (reportsClearancePLs[I_REPORTSCLEARANCELIST].control)
	{ PD_LIST, NULL, "list", 0, &reportsClearanceListData, NULL, 0 },
	{ PD_BUTTON, DoReportsClearanceRefresh, "refresh", 0, NULL, NULL, 0, NULL },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, &reportsClearanceSaveOp },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, &reportsClearancePrintOp },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
	{ PD_BUTTON, DoReportsClearanceFilter, "filter", 0, NULL, NULL, 0, NULL },
};
static paramGroup_t reportsClearancePG = { "reportsclearance", PGO_FULLDIALOGFROMBUILDER,
                                           reportsClearancePLs, COUNT( reportsClearancePLs )
                                         };

/** The current report's close runs -- each row's list context points into
 * it, so it outlives the compute pass (same rule as reportsShortList_da).
 * Replaced, old rows freed first, on each run. */
static dynArr_t reportsClearanceList_da;

/** TRUE while the list or the Clearance dropdown is being rebuilt -- same
 * re-entrancy guard as reportsShortPopulating. */
static BOOL_T reportsClearancePopulating = FALSE;

/** The previous run's rows, compared with on a Refresh. */
static dynArr_t reportsClearancePrev_da;
/** How many places the list has. */
static int reportsClearancePlaceCnt = 0;
/** Whether the current list was compared with a previous run, and what
 * that found: runs gone (fixed), new runs, and runs whose spacing or length
 * changed. */
static BOOL_T reportsClearanceMarked = FALSE;
static int reportsClearanceFixedCnt = 0;
static int reportsClearanceNewCnt = 0;
static int reportsClearanceChangedCnt = 0;

/** One sample along a track: where, and the track's direction there. */
typedef struct {
	coOrd pos;
	ANGLE_T angle;
} reportsClearanceSample_t;

/** The layout's scale, as a multiplier on HO inches (1 in HO). */
static DIST_T ReportsClearanceScaleFactor(void)
{
	DIST_T ratio = GetScaleRatio( GetLayoutCurScale() );

	return ratio > 0.0 ? 87.1 / ratio : 1.0;
}

/** The Clearance choice in prototype feet; a stale or edited preference
 * falls back to the default. */
static DIST_T ReportsClearanceFt(void)
{
	if ( reportsClearanceInx < 0
	     || reportsClearanceInx >= REPORTS_CLEARANCE_COUNT ) {
		reportsClearanceInx = 1;
	}
	return reportsClearanceFtValues[reportsClearanceInx];
}

/** A prototype distance in feet, in layout inches at the layout's scale. */
static DIST_T ReportsClearanceFtToIn(DIST_T ft)
{
	DIST_T ratio = GetScaleRatio( GetLayoutCurScale() );

	return ratio > 0.0 ? ft * 12.0 / ratio : ft * 12.0 / 87.1;
}

/** Fill the Clearance dropdown ("13 ft (1.79 in)") and select the choice. */
static void ReportsPopulateClearanceChoices(void)
{
	char label[48];
	wIndex_t i;

	(void) ReportsClearanceFt();            // clamps the index
	reportsClearancePopulating = TRUE;
	wListClear( reportsClearanceChoice );
	for ( i = 0; i < REPORTS_CLEARANCE_COUNT; i++ ) {
		snprintf( label, sizeof label, _("%.0f ft (%.2f in)"),
		          reportsClearanceFtValues[i],
		          ReportsClearanceFtToIn( reportsClearanceFtValues[i] ) );
		wComboBoxAddValue( reportsClearanceChoice, label, I2VP((int)i) );
	}
	wListSetIndex( reportsClearanceChoice, reportsClearanceInx );
	reportsClearancePopulating = FALSE;
}

/** Populate the interactive list from reportsClearanceList_da, each row's
 * context set to its entry so ReportsDlgUpdateClearance() can recover it. */
static void ReportsPopulateClearanceList(void)
{
	int i;
	char row[400];

	reportsClearancePopulating = TRUE;
	wListClear( reportsClearanceList );
	for ( i = 0; i < reportsClearanceList_da.cnt; i++ ) {
		reportsClearanceRun_t *entry =
		        &DYNARR_N(reportsClearanceRun_t, reportsClearanceList_da, i);

		snprintf( row, sizeof row, "%d\t%s\t%d\t%u\t%s\t%d\t%u\t%s\t%.3f\t%.2f",
		          entry->place,
		          entry->change == REPORTS_CLEARANCE_NEW ? _("new") :
		          entry->change == REPORTS_CLEARANCE_CHANGED ? _("changed") : "",
		          entry->trackA, entry->layerA, entry->layerNameA ? entry->layerNameA : "",
		          entry->trackB, entry->layerB, entry->layerNameB ? entry->layerNameB : "",
		          entry->spacingIn, entry->lengthIn );
		wListAddValue( reportsClearanceList, row, NULL, entry );
	}
	reportsClearancePopulating = FALSE;
}

/** paramGroup_t changeProc: a new Clearance takes effect at once (as in
 * Short Tracks); selecting a row pans/indicates at the run's midpoint. */
static wBool_t ReportsDlgUpdateClearance(paramGroup_cp pg, int inx,
                void *valueP)
{
	wIndex_t sel;
	reportsClearanceRun_t *entry;

	if (reportsClearancePopulating) {
		return TRUE;
	}
	if (inx == I_REPORTSCLEARANCECHOICE) {
		ReportsTrackClearance(NULL);
		return TRUE;
	}
	if (inx != I_REPORTSCLEARANCELIST) {
		return TRUE;
	}
	sel = wListGetIndex(reportsClearanceList);
	if (sel < 0) {
		return TRUE;
	}
	entry = (reportsClearanceRun_t *)wListGetItemContext(reportsClearanceList, sel);
	if (!entry) {
		return TRUE;
	}
	if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
	LOG( log_reports, 1,
	     ( "reports: clearance row %d selected -> tracks %d/%d, spacing=%.4f\n",
	       sel, entry->trackA, entry->trackB, entry->spacingIn ) )
	ReportsSetIndicator(entry->pos, entry->scale);
	return TRUE;
}

/** paramActionCancelProc -- same shape as ReportsCancelShort(). */
static void ReportsCancelClearance(paramGroup_cp pg)
{
	ReportsClearIndicator();
	FormCancel_Current(pg);
}

/** Build the formatted report text for Save/Print. */
static void ReportsBuildClearanceText(DynString *out)
{
	char summaryLine[200];
	DIST_T ft = ReportsClearanceFt();

	DynStringMalloc( out, 256 );
	ReportsAddHeader( out, _("Track Clearance Report") );

	snprintf( summaryLine, sizeof summaryLine,
	          "\n%s: %.0f ft (%.3f in)\n%s: %d\n%s: %d\n",
	          _("Clearance"), ft, ReportsClearanceFtToIn( ft ),
	          _("Places"), reportsClearancePlaceCnt,
	          _("Tracks closer than the clearance"), reportsClearanceList_da.cnt );
	DynStringCatCStr( out, summaryLine );
	if ( reportsClearanceMarked ) {
		snprintf( summaryLine, sizeof summaryLine, "%s: %d %s, %d %s, %d %s\n",
		          _("Since last refresh"), reportsClearanceFixedCnt, _("fixed"),
		          reportsClearanceNewCnt, _("new"), reportsClearanceChangedCnt, _("changed") );
		DynStringCatCStr( out, summaryLine );
	}

	if ( reportsClearanceList_da.cnt == 0 ) {
		DynStringCatCStrs( out, "\n", _("No tracks closer than the clearance."), "\n",
		                   NULL );
	} else {
		char headerLine[240];
		snprintf( headerLine, sizeof headerLine,
		          "\n%5s | %-7s | %6s | %5s | %-14s | %6s | %5s | %-14s | %7s | %7s\n",
		          _("Place"), _("Change"),
		          _("Track"), _("Layer"), _("Layer Name"),
		          _("Track"), _("Layer"), _("Layer Name"),
		          _("Spacing"), _("Length") );
		DynStringCatCStr( out, headerLine );
		ReportsFormatClearanceList( out,
		                            &DYNARR_N(reportsClearanceRun_t, reportsClearanceList_da, 0),
		                            reportsClearanceList_da.cnt );
	}
}

/** By place, then track A and track B: a place's rows together. */
static int ReportsClearancePlaceCompare(const void *a, const void *b)
{
	const reportsClearanceRun_t *ra = a;
	const reportsClearanceRun_t *rb = b;

	if ( ra->place != rb->place ) {
		return (ra->place > rb->place) - (ra->place < rb->place);
	}
	if ( ra->trackA != rb->trackA ) {
		return (ra->trackA > rb->trackA) - (ra->trackA < rb->trackA);
	}
	return (ra->trackB > rb->trackB) - (ra->trackB < rb->trackB);
}

/** Closest first, then by track A and track B. */
static int ReportsClearanceCompare(const void *a, const void *b)
{
	const reportsClearanceRun_t *ra = a;
	const reportsClearanceRun_t *rb = b;

	if ( ra->spacingIn != rb->spacingIn ) {
		return ra->spacingIn < rb->spacingIn ? -1 : 1;
	}
	if ( ra->trackA != rb->trackA ) {
		return (ra->trackA > rb->trackA) - (ra->trackA < rb->trackA);
	}
	return (ra->trackB > rb->trackB) - (ra->trackB < rb->trackB);
}

/** A track the report samples along: a track with two endpoints (straight,
 * curve, easement, Cornu, Bezier, two-ended sectional piece). Turnouts are
 * compared against, not sampled: their routes meet by design. */
static BOOL_T ReportsClearanceSampled(track_p trk)
{
	return GetTrkEndPtCnt(trk) == 2 && GetTrkType(trk) != T_TURNOUT
	       && !QueryTrack( trk, Q_CAN_ADD_ENDPOINTS );
}

/** Sample a two-ended track every ``step`` from end 0 to end 1, the same way
 * AddTrkDetails() walks a track (TraverseTrack). */
static void ReportsClearanceSamples(track_p trk, DIST_T step, dynArr_t *samples)
{
	DIST_T len = GetTrkLength( trk, 0, 1 );
	int n = (int)ceil( len / step );
	DIST_T dist, left;
	traverseTrack_t tt;
	int i;

	DYNARR_RESET( reportsClearanceSample_t, *samples );
	if ( n < 1 ) {
		n = 1;
	}
	dist = len / n;
	tt.trk = trk;
	tt.pos = GetTrkEndPos( trk, 0 );
	tt.angle = NormalizeAngle( GetTrkEndAngle( trk, 0 ) + 180.0 );
	DYNARR_APPEND( reportsClearanceSample_t, *samples, 64 );
	DYNARR_LAST( reportsClearanceSample_t, *samples ).pos = tt.pos;
	DYNARR_LAST( reportsClearanceSample_t, *samples ).angle = tt.angle;
	for ( i = 1; i <= n; i++ ) {
		tt.dist = dist;
		left = dist;
		TraverseTrack( &tt, &left );
		if ( left > 0 || tt.trk != trk ) {
			break;
		}
		DYNARR_APPEND( reportsClearanceSample_t, *samples, 64 );
		DYNARR_LAST( reportsClearanceSample_t, *samples ).pos = tt.pos;
		DYNARR_LAST( reportsClearanceSample_t, *samples ).angle = tt.angle;
	}
}

/** TRUE when a and b are joined at an endpoint (they meet by design). */
static BOOL_T ReportsClearanceJoined(track_p a, track_p b)
{
	EPINX_T ep;

	for ( ep = 0; ep < GetTrkEndPtCnt(a); ep++ ) {
		if ( GetTrkEndTrk(a, ep) == b ) {
			return TRUE;
		}
	}
	for ( ep = 0; ep < GetTrkEndPtCnt(b); ep++ ) {
		if ( GetTrkEndTrk(b, ep) == a ) {
			return TRUE;
		}
	}
	return FALSE;
}

/** Is a sample of track A beside track B, closer than the clearance?
 * Beside: the nearest point on B isn't one of B's ends (two tracks ending
 * in line aren't side by side), and the two run within
 * REPORTS_CLEARANCE_MAX_ANGLE of parallel. Sets *spacing. */
static BOOL_T ReportsClearanceBeside(const reportsClearanceSample_t *smp,
                                     track_p b,
                                     DIST_T clearance, DIST_T *spacing)
{
	coOrd p = smp->pos;
	DIST_T d = GetTrkDistance( b, &p );
	EPINX_T ep, e0 = -1, e1 = -1;
	ANGLE_T diff;

	if ( d >= clearance ) {
		return FALSE;
	}
	for ( ep = 0; ep < GetTrkEndPtCnt(b); ep++ ) {
		if ( FindDistance( p, GetTrkEndPos(b, ep) ) < 0.01 ) {
			return FALSE;
		}
	}
	diff = fmod( fabs( NormalizeAngle( smp->angle - GetAngleAtPoint( b, p, &e0,
	                                   &e1 ) ) ),
	             180.0 );
	if ( diff > 90.0 ) {
		diff = 180.0 - diff;
	}
	if ( diff > REPORTS_CLEARANCE_MAX_ANGLE ) {
		return FALSE;
	}
	*spacing = d;
	return TRUE;
}

/** Add a close run to the list: samples [first, last] of track a beside b. */
static void ReportsClearanceAddRun(track_p a, track_p b,
                                   const dynArr_t *samples,
                                   int first, int last, DIST_T minSpacing,
                                   DIST_T step)
{
	reportsClearanceRun_t *entry;

	DYNARR_APPEND( reportsClearanceRun_t, reportsClearanceList_da, 10 );
	entry = &DYNARR_LAST( reportsClearanceRun_t, reportsClearanceList_da );
	memset( entry, 0, sizeof *entry );
	entry->trackA = GetTrkIndex(a);
	entry->layerA = GetTrkLayer(a) + 1;
	entry->layerNameA = GetLayerName(GetTrkLayer(a));
	entry->trackB = GetTrkIndex(b);
	entry->layerB = GetTrkLayer(b) + 1;
	entry->layerNameB = GetLayerName(GetTrkLayer(b));
	entry->spacingIn = minSpacing;
	entry->lengthIn = (last - first) * step;
	entry->pos = DYNARR_N( reportsClearanceSample_t, *samples,
	                       (first + last) / 2 ).pos;
	entry->start = DYNARR_N( reportsClearanceSample_t, *samples, first ).pos;
	entry->end = DYNARR_N( reportsClearanceSample_t, *samples, last ).pos;
	entry->scale = GetTrkScale(a);
}

/** TRUE when runs a and b share a track or have tracks joined to each
 * other, and come within ``gap`` of each other: one place. */
static BOOL_T ReportsClearanceSamePlace(const reportsClearanceRun_t *a,
                                        const reportsClearanceRun_t *b, DIST_T gap)
{
	TRKINX_T ta[2] = { a->trackA, a->trackB };
	TRKINX_T tb[2] = { b->trackA, b->trackB };
	const coOrd *pa[3] = { &a->start, &a->pos, &a->end };
	const coOrd *pb[3] = { &b->start, &b->pos, &b->end };
	BOOL_T related = FALSE;
	int i, j;

	for ( i = 0; i < 2 && !related; i++ ) {
		for ( j = 0; j < 2 && !related; j++ ) {
			track_p x, y;

			if ( ta[i] == tb[j] ) {
				related = TRUE;
				break;
			}
			x = FindTrack( ta[i] );
			y = FindTrack( tb[j] );
			related = x != NULL && y != NULL && ReportsClearanceJoined( x, y );
		}
	}
	if ( !related ) {
		return FALSE;
	}
	for ( i = 0; i < 3; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			if ( FindDistance( *pa[i], *pb[j] ) <= gap ) {
				return TRUE;
			}
		}
	}
	return FALSE;
}

/** The place a run belongs to, following the links set by
 * ReportsClearanceNumberPlaces(). */
static int ReportsClearanceRoot(const int *root, int i)
{
	while ( root[i] != i ) {
		i = root[i];
	}
	return i;
}

/** Number the places: runs at one spot (ReportsClearanceSamePlace(), taken
 * transitively) share a number, the place with the closest spacing first.
 * The list must be sorted closest first. */
static void ReportsClearanceNumberPlaces(DIST_T gap)
{
	int n = reportsClearanceList_da.cnt;
	int *root = MyMalloc( (n > 0 ? n : 1) * sizeof *root );
	int *number = MyMalloc( (n > 0 ? n : 1) * sizeof *number );
	int i, j, next = 0;

	for ( i = 0; i < n; i++ ) {
		root[i] = i;
		number[i] = 0;
	}
	for ( i = 0; i < n; i++ ) {
		for ( j = i + 1; j < n; j++ ) {
			int ri, rj;

			if ( !ReportsClearanceSamePlace(
			             &DYNARR_N( reportsClearanceRun_t, reportsClearanceList_da, i ),
			             &DYNARR_N( reportsClearanceRun_t, reportsClearanceList_da, j ), gap ) ) {
				continue;
			}
			ri = ReportsClearanceRoot( root, i );
			rj = ReportsClearanceRoot( root, j );
			if ( ri != rj ) {
				/* The closer one (lower index) leads the place. */
				root[ri > rj ? ri : rj] = ri < rj ? ri : rj;
			}
		}
	}
	for ( i = 0; i < n; i++ ) {
		int r = ReportsClearanceRoot( root, i );

		if ( number[r] == 0 ) {
			number[r] = ++next;
		}
		DYNARR_N( reportsClearanceRun_t, reportsClearanceList_da, i ).place = number[r];
	}
	reportsClearancePlaceCnt = next;
	MyFree( root );
	MyFree( number );
}

/** Compare with the previous run (reportsClearancePrev_da): mark new and
 * changed rows, count the runs gone. A run matches one of the same two
 * tracks that overlaps it (midpoints within half their lengths plus
 * ``slack``). */
static void ReportsClearanceMarkChanges(DIST_T slack)
{
	int i, j;
	int prevCnt = reportsClearancePrev_da.cnt;
	BOOL_T *matched = MyMalloc( (prevCnt > 0 ? prevCnt : 1) * sizeof *matched );

	for ( j = 0; j < prevCnt; j++ ) {
		matched[j] = FALSE;
	}
	reportsClearanceNewCnt = 0;
	reportsClearanceChangedCnt = 0;
	for ( i = 0; i < reportsClearanceList_da.cnt; i++ ) {
		reportsClearanceRun_t *r =
		        &DYNARR_N( reportsClearanceRun_t, reportsClearanceList_da, i );
		int hit = -1;

		for ( j = 0; j < prevCnt && hit < 0; j++ ) {
			reportsClearanceRun_t *p =
			        &DYNARR_N( reportsClearanceRun_t, reportsClearancePrev_da, j );
			BOOL_T samePair = ( p->trackA == r->trackA && p->trackB == r->trackB )
			                  || ( p->trackA == r->trackB && p->trackB == r->trackA );

			if ( !matched[j] && samePair
			     && FindDistance( p->pos, r->pos )
			     <= (p->lengthIn + r->lengthIn) / 2.0 + slack ) {
				hit = j;
			}
		}
		if ( hit < 0 ) {
			r->change = REPORTS_CLEARANCE_NEW;
			reportsClearanceNewCnt++;
		} else {
			reportsClearanceRun_t *p =
			        &DYNARR_N( reportsClearanceRun_t, reportsClearancePrev_da, hit );

			matched[hit] = TRUE;
			if ( fabs( p->spacingIn - r->spacingIn ) > 1e-3
			     || fabs( p->lengthIn - r->lengthIn ) > 1e-3 ) {
				r->change = REPORTS_CLEARANCE_CHANGED;
				reportsClearanceChangedCnt++;
			}
		}
	}
	reportsClearanceFixedCnt = 0;
	for ( j = 0; j < prevCnt; j++ ) {
		if ( !matched[j] ) {
			reportsClearanceFixedCnt++;
		}
	}
	MyFree( matched );
}

void ReportsTrackClearance( void * unused )
{
	track_p trk;
	DIST_T clearance = ReportsClearanceFtToIn( ReportsClearanceFt() );
	DIST_T factor = ReportsClearanceScaleFactor();
	DIST_T step = REPORTS_CLEARANCE_STEP * factor;
	DIST_T minRun = REPORTS_CLEARANCE_MIN_RUN * factor;
	DIST_T minHeight = REPORTS_CLEARANCE_MIN_HEIGHT * factor;
	dynArr_t candidates;
	dynArr_t heights;
	dynArr_t samples;
	int i, j, k;

	ReportsClearIndicator();

	/* Clear the visible list before freeing the array its rows' contexts
	 * point into -- see ReportsUnconnectedEndpoints(). */
	if ( reportsClearanceDlg.window != NULL ) {
		reportsClearancePopulating = TRUE;
		wListClear( reportsClearanceList );
		reportsClearancePopulating = FALSE;
	}
	DYNARR_FREE( reportsClearanceRun_t, reportsClearanceList_da );
	DYNARR_INIT( reportsClearanceRun_t, reportsClearanceList_da );

	DYNARR_INIT( track_p, candidates );
	DYNARR_INIT( DIST_T, heights );
	DYNARR_INIT( reportsClearanceSample_t, samples );
	TRK_ITERATE( trk ) {
		if ( QueryTrack( trk, Q_ISTRAIN ) || !IsTrack( trk ) ) {
			continue;       // a car, or not track (benchwork, notes, ...)
		}
		if ( GetTrkEndPtCnt(trk) < 2 ) {
			continue;
		}
		if ( !ReportsFilterLayerIncluded( &reportsClearanceFilter,
		                                  GetTrkLayer(trk) ) ) {
			continue;
		}
		DYNARR_APPEND( track_p, candidates, 64 );
		DYNARR_LAST( track_p, candidates ) = trk;
		/* Its height, once: 0 on a layout without elevations. */
		DYNARR_APPEND( DIST_T, heights, 64 );
		DYNARR_LAST( DIST_T, heights ) = GetElevation( trk );
	}

	for ( i = 0; i < candidates.cnt; i++ ) {
		track_p a = DYNARR_N( track_p, candidates, i );
		coOrd aLo, aHi;

		if ( !ReportsClearanceSampled( a ) ) {
			continue;
		}
		GetBoundingBox( a, &aHi, &aLo );
		aLo.x -= clearance;
		aLo.y -= clearance;
		aHi.x += clearance;
		aHi.y += clearance;
		ReportsClearanceSamples( a, step, &samples );
		for ( j = 0; j < candidates.cnt; j++ ) {
			track_p b = DYNARR_N( track_p, candidates, j );
			coOrd bLo, bHi;
			int first = -1;
			DIST_T minSpacing = 0.0;

			if ( b == a ) {
				continue;
			}
			/* Each pair of sampled tracks once: from the lower index. */
			if ( ReportsClearanceSampled( b ) && GetTrkIndex(b) < GetTrkIndex(a) ) {
				continue;
			}
			GetBoundingBox( b, &bHi, &bLo );
			if ( bLo.x > aHi.x || bHi.x < aLo.x || bLo.y > aHi.y || bHi.y < aLo.y ) {
				continue;
			}
			if ( ReportsClearanceJoined( a, b ) ) {
				continue;
			}
			if ( fabs( DYNARR_N( DIST_T, heights, i ) - DYNARR_N( DIST_T, heights, j ) )
			     > minHeight ) {
				continue;       // decks above each other
			}
			for ( k = 0; k <= samples.cnt; k++ ) {
				DIST_T spacing = 0.0;
				BOOL_T beside = k < samples.cnt
				                && ReportsClearanceBeside(
				                        &DYNARR_N( reportsClearanceSample_t, samples, k ),
				                        b, clearance, &spacing );

				if ( beside ) {
					if ( first < 0 ) {
						first = k;
						minSpacing = spacing;
					} else if ( spacing < minSpacing ) {
						minSpacing = spacing;
					}
					continue;
				}
				if ( first >= 0 && (k - 1 - first) * step >= minRun ) {
					ReportsClearanceAddRun( a, b, &samples, first, k - 1, minSpacing, step );
				}
				first = -1;
			}
		}
	}
	DYNARR_FREE( reportsClearanceSample_t, samples );
	DYNARR_FREE( DIST_T, heights );
	DYNARR_FREE( track_p, candidates );

	if ( reportsClearanceList_da.cnt > 1 ) {
		qsort( reportsClearanceList_da.ptr, (size_t)reportsClearanceList_da.cnt,
		       sizeof(reportsClearanceRun_t), ReportsClearanceCompare );
	}
	ReportsClearanceNumberPlaces( REPORTS_CLEARANCE_PLACE_GAP * factor );
	reportsClearanceMarked = reportsClearanceMarkChanges;
	if ( reportsClearanceMarked ) {
		ReportsClearanceMarkChanges( REPORTS_CLEARANCE_MIN_RUN * factor );
	}
	if ( reportsClearanceList_da.cnt > 1 ) {
		qsort( reportsClearanceList_da.ptr, (size_t)reportsClearanceList_da.cnt,
		       sizeof(reportsClearanceRun_t), ReportsClearancePlaceCompare );
	}
	/* This run is what the next Refresh compares with. */
	DYNARR_RESET( reportsClearanceRun_t, reportsClearancePrev_da );
	for ( i = 0; i < reportsClearanceList_da.cnt; i++ ) {
		DYNARR_APPEND( reportsClearanceRun_t, reportsClearancePrev_da, 10 );
		DYNARR_LAST( reportsClearanceRun_t, reportsClearancePrev_da ) =
		        DYNARR_N( reportsClearanceRun_t, reportsClearanceList_da, i );
	}

	{
		char summary[320];
		DIST_T ft = ReportsClearanceFt();

		if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
		LOG( log_reports, 1,
		     ( "reports: Track Clearance computed -- %d close run(s) under %.3f in\n",
		       reportsClearanceList_da.cnt, clearance ) )

		ReportsShowDialog( &reportsClearanceDlg, _("Track Clearance Report") );
		ReportsPopulateClearanceChoices();
		if ( reportsClearanceMarked ) {
			snprintf( summary, sizeof summary,
			          _("%d place(s) (%d row(s)) closer than %.0f ft (%.2f in) -- since last refresh: %d fixed, %d new, %d changed"),
			          reportsClearancePlaceCnt, reportsClearanceList_da.cnt, ft,
			          ReportsClearanceFtToIn( ft ), reportsClearanceFixedCnt,
			          reportsClearanceNewCnt, reportsClearanceChangedCnt );
		} else {
			snprintf( summary, sizeof summary,
			          _("%d place(s) (%d row(s)) where tracks are closer than %.0f ft (%.2f in)"),
			          reportsClearancePlaceCnt, reportsClearanceList_da.cnt, ft,
			          ReportsClearanceFtToIn( ft ) );
		}
		wMessageSetValue( reportsClearanceSummary, summary );
	}

	ReportsPopulateClearanceList();
}

/* ---------------------------------------------------------------------
 * Notes Report (SF #799, part of the JSON Note umbrella SF #795).
 * Interactive (click-to-navigate) as of #799's own follow-up, same
 * grouped-by-category Save/Print text (ReportsFormatNoteList()) shape as
 * Equipment Suitability above. Unlike every prior report, this one also
 * has Type and ROOT Names filters (both PD_DROPLIST, rebuilt on each
 * compute pass same as FillLayerList()'s own "rebuild on each
 * invocation" convention) alongside the standard Layer Group filter.
 * ------------------------------------------------------------------- */

/** Tentative declaration -- same reason as reportsPG above. */
static paramGroup_t reportsNotesPG;
static void ReportsBuildNoteText(DynString *out);
static wBool_t ReportsDlgUpdateNotes(paramGroup_cp pg, int inx, void *valueP);
static void ReportsCancelNotes(paramGroup_cp pg);

static reportsDialog_t reportsNotesDlg = {
	&reportsNotesPG, NULL, NULL, NULL,
	ReportsBuildNoteText, ReportsNotes,
	ReportsDlgUpdateNotes, ReportsCancelNotes
};
static reportsOpCtx_t reportsNotesRefreshOp = { &reportsNotesDlg, REPORTSOP_REFRESH };
static reportsOpCtx_t reportsNotesSaveOp    = { &reportsNotesDlg, REPORTSOP_SAVE };
static reportsOpCtx_t reportsNotesPrintOp   = { &reportsNotesDlg, REPORTSOP_PRINT };

static wWinPix_t reportsNotesListWidths[] = { 90, 80, 220, 60 };
static const char * reportsNotesListTitles[] = {
	N_("ROOT Names"), N_("ID"), N_("Name/Label"), N_("Layer")
};
static paramListData_t reportsNotesListData = { 8, 400, 4, reportsNotesListWidths,
                                                reportsNotesListTitles
                                              };

/** This report's layer/group scope -- see dreportsfilter.h. */
static reportsFilter_t reportsNotesFilter;

/** "Filter..." button -- see DoReportsFilter() above for the shared shape. */
static void DoReportsNotesFilter(void *unused)
{
	(void)unused;
	ShowReportsFilterDialog(&reportsNotesFilter);
}

/** ROOT Names filter (SF #800 phase 2): 0 = "All ROOT Names", 1..N map to
 * the "Manage Notes" registry's own N registered names, in registry
 * order -- built dynamically by ReportsPopulateNoteRootNamesFilter()
 * every invocation, not a fixed list any more (that hardcoding is
 * exactly what phase 2 removes). Only meaningful for Type "JSON" or "All
 * Types" -- see `reportsNoteTypeFilterInx` below. SF #800 phase 3 (the
 * "Manage Notes" dialog, dmanagenotesui.c) is what actually populates the
 * registry these two functions read from (notenames.h); until the user
 * registers a name there, NoteNameCount() is 0 and this dropdown shows
 * just "All ROOT Names" -- the correct, expected empty-registry state,
 * not a bug. */
static wIndex_t reportsNoteRootNamesFilterInx;

/** Thin wrapper over NoteNameCount() (notenames.h) -- kept as its own
 * function (not inlined into the filter-population/compute-pass call
 * sites) so the registry's real storage can change without touching any
 * call site here, only this definition. */
static int ReportsManagedNameCount(void)
{
	return NoteNameCount();
}

/** Thin wrapper over NoteNameAt() (notenames.h) -- see
 * ReportsManagedNameCount(). */
static const char *ReportsManagedNameAt(int i)
{
	return NoteNameAt(i);
}

/** Type filter (SF #800 phase 1): 0 = "All Types", 1..4 map directly to
 * `enum noteCommands` (OP_NOTETEXT..OP_NOTEJSON, note.h) -- a genuinely
 * closed, fixed set (the four native note types), unlike the ROOT Names
 * filter above, so hardcoding this list is fine. Independent of ROOT
 * Names -- filtering to a non-JSON type still applies ROOT Names too (a
 * non-JSON note's resolved group is only ever set for JSON notes, so
 * combining e.g. Type=Weblink with a ROOT Names filter other than "All
 * ROOT Names" yields an empty, if unsurprising, result). */
static wIndex_t reportsNoteTypeFilterInx;
static const char * reportsNoteTypeFilterLabels[] = {
	N_("All Types"), N_("Text"), N_("Weblink"), N_("Document"), N_("JSON")
};

static paramData_t reportsNotesPLs[] = {
#define I_REPORTSNOTESSUMMARY (0)
#define reportsNotesSummary (reportsNotesPLs[I_REPORTSNOTESSUMMARY].control)
	{ PD_MESSAGE, "", "summary", 0, I2VP(37) },
#define I_REPORTSNOTESTYPEFILTER (1)
#define reportsNotesTypeFilter (reportsNotesPLs[I_REPORTSNOTESTYPEFILTER].control)
	{ PD_DROPLIST, &reportsNoteTypeFilterInx, "typefilter", PDO_NOPREF | PDO_LISTINDEX, I2VP(110), NULL, 0 },
#define I_REPORTSNOTESROOTNAMESFILTER (2)
#define reportsNotesRootNamesFilter (reportsNotesPLs[I_REPORTSNOTESROOTNAMESFILTER].control)
	{ PD_DROPLIST, &reportsNoteRootNamesFilterInx, "rootnamesfilter", PDO_NOPREF | PDO_LISTINDEX, I2VP(120), NULL, 0 },
#define I_REPORTSNOTESLIST (3)
#define reportsNotesList (reportsNotesPLs[I_REPORTSNOTESLIST].control)
	{ PD_LIST, NULL, "list", 0, &reportsNotesListData, NULL, 0 },
	{ PD_BUTTON, DoReportsOp, "refresh", 0, NULL, NULL, 0, &reportsNotesRefreshOp },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, &reportsNotesSaveOp },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, &reportsNotesPrintOp },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
	{ PD_BUTTON, DoReportsNotesFilter, "filter", 0, NULL, NULL, 0, NULL },
};
static paramGroup_t reportsNotesPG = { "reportsnotes", PGO_FULLDIALOGFROMBUILDER,
                                       reportsNotesPLs, COUNT( reportsNotesPLs )
                                     };

/** The current Notes Report's rows, in TRK_ITERATE order (not grouped --
 * ReportsFormatNoteList() does that grouping for the Save/Print
 * text only, same as every other grouped-output report keeps its
 * interactive list in a different order than its own Save/Print text).
 * Kept alive for as long as the dialog might reference it via the list's
 * per-row context pointers (this report is interactive -- click-to-
 * navigate, same as Gaps/Kinked Joints), not just a plain local. */
static dynArr_t reportsNotesList_da;

/** TRUE while ReportsPopulateNoteList() is clearing/rebuilding
 * reportsNotesList -- same re-entrancy guard as reportsPopulating (phase
 * 1)/reportsGapsPopulating, required for any interactive report's list;
 * see reportsPopulating's own doc comment for the real SF #772 crash this
 * pattern exists to prevent. */
static BOOL_T reportsNotesPopulating = FALSE;

/** Populate the ROOT Names filter dropdown -- built from the "Manage
 * Notes" registry (SF #800 phase 3, not built yet, so this is always
 * just "All ROOT Names" for now), rebuilt on each invocation same as
 * FillLayerList()'s own convention. */
static void ReportsPopulateNoteRootNamesFilter(void)
{
	int i;
	int count = ReportsManagedNameCount();

	wListClear( reportsNotesRootNamesFilter );
	wComboBoxAddValue( reportsNotesRootNamesFilter, _("All ROOT Names"), I2VP(0) );
	for ( i = 0; i < count; i++ ) {
		wComboBoxAddValue( reportsNotesRootNamesFilter, ReportsManagedNameAt(i),
		                   I2VP(i + 1) );
	}
	if ( reportsNoteRootNamesFilterInx > count ) {
		reportsNoteRootNamesFilterInx = 0;
	}
	wListSetIndex( reportsNotesRootNamesFilter,
	               (int)reportsNoteRootNamesFilterInx );
}

/** Populate the Type filter dropdown -- fixed 5-entry list (SF #800 phase
 * 1), rebuilt on each invocation same as the ROOT Names filter's own
 * convention. */
static void ReportsPopulateNoteTypeFilter(void)
{
	size_t i;

	wListClear( reportsNotesTypeFilter );
	for ( i = 0; i < sizeof reportsNoteTypeFilterLabels / sizeof
	      reportsNoteTypeFilterLabels[0]; i++ ) {
		wComboBoxAddValue( reportsNotesTypeFilter, _(reportsNoteTypeFilterLabels[i]),
		                   I2VP((int)i) );
	}
	wListSetIndex( reportsNotesTypeFilter, (int)reportsNoteTypeFilterInx );
}

/** Populate the interactive list from reportsNotesList_da -- one row per
 * note, tab-separated, each row's context set to that note's address so
 * ReportsDlgUpdateNotes() can recover it on selection (interactive --
 * click-to-navigate, same as Gaps/Kinked Joints/Unconnected Endpoints). */
static void ReportsPopulateNoteList(void)
{
	int i;
	/* id (63) + label (127) + group (63) + tabs/layer digits, with
	 * headroom -- entry->group is a plain char[64] (reports.h), not a
	 * short string literal like the Text/Weblink/Document branches, so
	 * the compiler's worst-case bound is wider than it used to be. */
	char row[320];

	reportsNotesPopulating = TRUE;

	wListClear( reportsNotesList );
	for ( i = 0; i < reportsNotesList_da.cnt; i++ ) {
		reportsNoteRow_t *entry = &DYNARR_N(reportsNoteRow_t, reportsNotesList_da, i);
		/* Non-JSON: show the Type itself (a closed set). JSON: show the
		 * resolved ROOT Names group ("ROOT" until SF #800 phase 3
		 * registers real names) -- entry->group is only ever set for
		 * JSON rows. */
		const char *rootNamesStr = entry->type == REPORTS_NOTE_OP_TEXT ? _("Text") :
		                           entry->type == REPORTS_NOTE_OP_LINK ? _("Weblink") :
		                           entry->type == REPORTS_NOTE_OP_FILE ? _("Document") :
		                           entry->group;

		snprintf( row, sizeof row, "%s\t%s\t%s\t%u",
		          rootNamesStr, entry->id, entry->label, entry->layer );
		wListAddValue( reportsNotesList, row, NULL, entry );
	}

	reportsNotesPopulating = FALSE;
}

/** paramGroup_t changeProc for the Notes dialog -- selecting a row pans/
 * indicates at that note's own position. Same guard/recovery shape as
 * phase 1's ReportsDlgUpdate()/Gaps' ReportsDlgUpdateGaps(). */
static wBool_t ReportsDlgUpdateNotes(paramGroup_cp pg, int inx, void *valueP)
{
	wIndex_t sel;
	reportsNoteRow_t *entry;
	(void)pg;
	(void)valueP;

	/* SF #802: the Type/ROOT Names filter dropdowns used to require a
	 * separate manual Refresh click to take effect -- self-refresh here,
	 * the same way Refresh's own REPORTSOP_REFRESH case does. */
	if (inx == I_REPORTSNOTESTYPEFILTER || inx == I_REPORTSNOTESROOTNAMESFILTER) {
		ReportsNotes(NULL);
		return TRUE;
	}

	if (inx != I_REPORTSNOTESLIST) {
		return TRUE;
	}
	if (reportsNotesPopulating) {
		return TRUE;
	}

	sel = wListGetIndex(reportsNotesList);
	if (sel < 0) {
		return TRUE;
	}
	entry = (reportsNoteRow_t *)wListGetItemContext(reportsNotesList, sel);
	if (!entry) {
		return TRUE;
	}

	if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
	LOG( log_reports, 1,
	     ( "reports: notes row %d selected -> note %d @ (%.3f,%.3f)\n",
	       sel, entry->noteIndex, entry->pos.x, entry->pos.y ) )

	ReportsSetIndicator(entry->pos, entry->scale);
	return TRUE;
}

/** paramActionCancelProc for the Notes dialog -- same shape as phase 1's
 * ReportsCancel()/Gaps' ReportsCancelGaps(). */
static void ReportsCancelNotes(paramGroup_cp pg)
{
	ReportsClearIndicator();
	FormCancel_Current(pg);
}

/** Build the full formatted Notes Report text (header + Type/ROOT-Names-
 * grouped table) fresh from reportsNotesList_da. Used only by
 * ReportsRefreshPrintText() (Save/Print), same as every other report's
 * own build-text function. */
static void ReportsBuildNoteText(DynString *out)
{
	DynStringMalloc( out, 256 );
	ReportsAddHeader( out, _("Notes Report") );

	if ( reportsNotesList_da.cnt == 0 ) {
		DynStringCatCStrs( out, "\n", _("No notes found."), "\n", NULL );
	} else {
		DynStringCatCStr( out, "\n" );
		ReportsFormatNoteList( out, &DYNARR_N(reportsNoteRow_t,
		                                      reportsNotesList_da, 0), reportsNotesList_da.cnt );
	}
}

void ReportsNotes( void * unused )
{
	track_p trk;
	/* Built once, not per-note -- the "Manage Notes" registry doesn't
	 * change mid-compute. Always empty in SF #800 phase 2 (no registry
	 * UI exists yet), so registeredNames is unused in practice, but
	 * ReportsNoteResolveGroup() takes it as an explicit array (not
	 * reports.c's own static state) so it stays CMocka-testable. */
	const char *registeredNames[16];
	int registeredCount = ReportsManagedNameCount();
	int regI;
	(void)unused;

	if ( registeredCount > (int)(sizeof registeredNames / sizeof
	                             registeredNames[0]) ) {
		registeredCount = (int)(sizeof registeredNames / sizeof registeredNames[0]);
	}
	for ( regI = 0; regI < registeredCount; regI++ ) {
		registeredNames[regI] = ReportsManagedNameAt(regI);
	}

	/* Free each row's owned rawJson before the backing array itself goes --
	 * DYNARR_FREE only releases the array buffer, not what individual rows
	 * point to. */
	for ( int freeI = 0; freeI < reportsNotesList_da.cnt; freeI++ ) {
		char *rawJson = DYNARR_N( reportsNoteRow_t, reportsNotesList_da,
		                          freeI ).rawJson;
		if ( rawJson ) {
			MyFree(rawJson);
		}
	}
	DYNARR_FREE( reportsNoteRow_t, reportsNotesList_da );
	DYNARR_INIT( reportsNoteRow_t, reportsNotesList_da );

	TRK_ITERATE( trk ) {
		if ( GetTrkType(trk) != T_NOTE ) {
			continue;
		}
		if ( !ReportsFilterLayerIncluded( &reportsNotesFilter, GetTrkLayer(trk) ) ) {
			continue;
		}

		struct extraDataNote_t * xx = GET_EXTRA_DATA( trk, T_NOTE,
		                              extraDataNote_t );

		/* Type filter (SF #800 phase 1): 0 = "All Types", 1..4 map
		 * directly to enum noteCommands's OP_NOTETEXT..OP_NOTEJSON order
		 * -- see reportsNoteTypeFilterLabels' own doc comment. Checked
		 * before the more expensive JSON-group computation below. */
		if ( reportsNoteTypeFilterInx > 0 &&
		     (reportsNoteTypeFilterInx - 1) != (long)xx->op ) {
			continue;
		}

		/* group is only ever set for a JSON note -- see reportsNoteRow_t's
		 * own doc comment (reports.h). */
		char group[64] = "";
		char id[64] = "";
		char label[128] = "";
		/* SF #802: the note's raw JSON text, captured for Save/Print output
		 * only -- xx->noteData.text is the track's own persistent buffer,
		 * still valid after cJSON_Delete(parsed) below (that only frees the
		 * parsed tree, not the original text), but this row needs its own
		 * copy since it must outlive this loop iteration. */
		char *rawJson = NULL;

		if ( IsJsonNote(trk) ) {
			if ( xx->noteData.text ) {
				rawJson = MyStrdup(xx->noteData.text);
			}
			cJSON *parsed = cJSON_Parse(xx->noteData.text);

			if ( parsed != NULL && cJSON_IsObject(parsed) ) {
				strncpy( group, ReportsNoteResolveGroup(parsed, registeredNames,
				                                        registeredCount),
				         sizeof group - 1 );

				cJSON *idField = cJSON_GetObjectItemCaseSensitive(parsed, "id");
				if ( idField && cJSON_IsString(idField) && idField->valuestring ) {
					strncpy( id, idField->valuestring, sizeof id - 1 );
				}

				ReportsNoteExtractLabel(parsed, label, sizeof label);
			} else {
				/* parsed == NULL or not an object: shouldn't happen for a
				 * note saved through jsonnoteui.c's own Validate-on-save
				 * gate, but a hand-edited file could produce it -- falls
				 * through as an ungrouped JSON note rather than an error,
				 * same graceful-degradation precedent as
				 * ReportsNoteResolveGroup()'s own no-registered-fields
				 * case. */
				strncpy( group, "ROOT", sizeof group - 1 );
			}
			if ( parsed ) {
				cJSON_Delete(parsed);
			}
		} else {
			/* Non-JSON note: group stays empty (grouped by Type alone),
			 * id stays empty, label becomes a short raw-text preview --
			 * matching DescribeTextNote()'s own status-line precedent.
			 * Each legacy note type keeps its text in a different union
			 * member. */
			char *raw = xx->op == OP_NOTETEXT ? xx->noteData.text :
			            xx->op == OP_NOTELINK ? xx->noteData.linkData.title :
			            xx->op == OP_NOTEFILE ? xx->noteData.fileData.title : NULL;

			if ( raw ) {
				char *preview = MyMalloc(strlen(raw) + 1);
				RemoveFormatChars(raw, preview);
				EllipsizeString(preview, NULL, sizeof label - 1);
				strncpy( label, preview, sizeof label - 1 );
				MyFree(preview);
			}
		}

		/* ROOT Names filter (SF #800 phase 2): only ever meaningful for
		 * JSON notes -- a non-JSON note has no group, so any ROOT Names
		 * filter but "All ROOT Names" excludes it, matching phase 1's
		 * already-documented Type/ROOT-Names interaction.
		 * reportsNoteRootNamesFilterInx - 1 indexes the (currently
		 * always-empty) Manage Notes registry. */
		if ( reportsNoteRootNamesFilterInx > 0 &&
		     ( xx->op != OP_NOTEJSON ||
		       strcmp( group, ReportsManagedNameAt((int)reportsNoteRootNamesFilterInx - 1) ) !=
		       0 ) ) {
			MyFree(rawJson);
			continue;
		}

		{
			reportsNoteRow_t *row;
			DYNARR_APPEND( reportsNoteRow_t, reportsNotesList_da, 10 );
			row = &DYNARR_LAST( reportsNoteRow_t, reportsNotesList_da );
			row->type = (int)xx->op;
			strncpy( row->group, group, sizeof row->group - 1 );
			strncpy( row->id, id, sizeof row->id - 1 );
			strncpy( row->label, label, sizeof row->label - 1 );
			row->layer = GetTrkLayer(trk) + 1;
			row->noteIndex = GetTrkIndex(trk);
			row->pos = xx->pos;
			row->scale = GetTrkScale(trk);
			row->rawJson = rawJson;
		}
	}

	{
		char summary[160];

		if ( log_reports < 0 ) { log_reports = LogFindIndex( "reports" ); }
		LOG( log_reports, 1,
		     ( "reports: Notes computed -- %d note(s)\n", (int) reportsNotesList_da.cnt ) )

		ReportsShowDialog( &reportsNotesDlg, _("Notes Report") );
		snprintf( summary, sizeof summary, _("%d note(s) found"),
		          (int) reportsNotesList_da.cnt );
		wMessageSetValue( reportsNotesSummary, summary );
	}

	ReportsPopulateNoteTypeFilter();
	ReportsPopulateNoteRootNamesFilter();
	ReportsPopulateNoteList();
}
