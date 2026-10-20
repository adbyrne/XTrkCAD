\page creating-a-report Creating a New Report

How to add a new entry to the `Reports` menu (SF #217) -- unconnected endpoints, track lengths,
curve stats, turnout density, equipment suitability, gaps, and kinked joints are all built on the
same shared viewer, declared in `app/bin/include/reports.h` and implemented in
`app/bin/reports.c`/`app/bin/reportsformat.c`. Read those two source files alongside this page --
they're the authoritative reference; this page is the map.

\tableofcontents

# The shared viewer

Every report is a `reportsDialog_t` instance (reports.c) -- one dialog window, a summary label, an
interactive `PD_LIST`, and Refresh/Save/Print/Print Setup buttons, all driven by the same handful
of functions regardless of which report it is:

- `ReportsShowDialog()` creates/shows the dialog window (lazily, on first use) and the hidden
  text control that backs Save/Print.
- `DoReportsOp()` is the one `PD_BUTTON` callback for every report's Refresh/Save/Print buttons --
  a `reportsOpCtx_t` (dialog pointer + operation code) tells it which report and which button.
- `ReportsRefreshPrintText()` calls the report's own `buildText` function to regenerate the hidden
  text control just before Save or Print actually use it.

A report only needs to supply: a compute pass that walks the layout and builds a row list, a
formatting function that turns those rows into `DynString` text, a `.ui` dialog describing the
summary label and list columns, and the small block of `reportsDialog_t`/`reportsOpCtx_t`/
`paramData_t` plumbing that ties them together. Nothing above needs to change.

# Two shapes: report-only vs. interactive

Reports come in two shapes, distinguished by `reportsDialog_t`'s `changeProc`/`cancelProc`
fields:

- **Report-only** (`changeProc`/`cancelProc` both `NULL`) -- no click-to-navigate, no draw
  indicator on the main canvas. This is the simpler shape and the right default for a new report
  unless it specifically needs navigation. Turnout Density (`ReportsTurnoutDensity()`) is the
  cleanest example to copy from -- Track Lengths, Curve Stats, and Equipment Suitability all
  follow the identical shape.
- **Interactive** (real `changeProc`/`cancelProc`) -- selecting a row pans the main canvas to
  that row's position and draws a transient indicator (`ReportsDrawIndicator()`), and the cancel
  handler clears that indicator on dialog close. Unconnected Endpoints (the original phase-1
  report) and Gaps/Kinked Joints (`ReportsGaps()`/`ReportsKinkedJoints()`) are the examples to
  copy from. Only add this if a row genuinely corresponds to a navigable point on the layout --
  it costs a `.ui` `changeProc`/`cancelProc` wire-up and shares the module-level indicator state
  with every other interactive report (see \ref creating-a-report-gotchas below).

# Step by step

Using a new report-only report as the running example (substitute the interactive shape's extra
`changeProc`/`cancelProc` wiring if that's what you're building):

1. **Declare the menu-callback function** in `reports.h`, with a `/**` doc comment describing what
   it computes and why it's report-only or interactive -- follow the style of the existing
   `Reports*()` declarations there.
2. **Write the compute pass** in `reports.c`: walk `TRK_ITERATE`, build a `dynArr_t` of rows into
   a report-specific struct (see e.g. `reportsTurnoutList_da`). Guard against degenerate objects
   before calling per-object accessors -- `GetTrkEndPtCnt(trk) >= 2` before `GetTrkLength()` is
   the standing pattern (SF #776 fixed a crash on 0/1-endpoint objects such as benchwork and
   notes; every report added since then follows this guard). Free the previous run's array before
   replacing it.
3. **Add a pure-text formatting function** to `reportsformat.c`, not `reports.c` -- these have no
   `wlib`/track-database dependency, so they link directly into `unittest/reportstest.c` and are
   CMocka-testable without stubbing the dialog/viewer surface. `ReportsFormatTurnoutList()` is a
   good short example to copy.
4. **Wire up the `reportsDialog_t` block** in `reports.c`: a tentative `paramGroup_t` forward
   declaration, the `reportsDialog_t` instance itself, three `reportsOpCtx_t`s (refresh/save/
   print), the `paramListData_t`/column-title arrays, the `paramData_t[]` (summary message, list,
   the three op buttons, print setup), and the `paramGroup_t` that ties them together. Copy the
   Turnout Density block (`reportsTurnoutDlg` and its surrounding statics) verbatim and rename.
5. **Add the `.ui` file** under `app/wlib/gtk3lib/ui/` (e.g. `reportsturnout.ui`) -- a `GtkDialog`
   with Help/Refresh/Print/Page Setup/Save/Done actions and a `GtkBox` containing a summary
   `GtkLabel` and a `GtkScrolledWindow`/`GtkTreeView`/`GtkListStore` for the list. Copy an
   existing report's `.ui` file and adjust the column count/titles and the `GtkListStore`'s
   `<columns>` to match your row struct. Register the new file in
   `app/wlib/gtk3lib/ui/wlib.gresource.xml.in` (one `<file>` line per report `.ui`) -- a `.ui`
   file that exists on disk but isn't listed there is never compiled in.
6. **Add the menu entry** in `menu.c`'s `reportsM` block: one `MiscMenuItemCreate()` call, plus a
   matching `ACCL_REPORTS*` constant in `acclkeys.h` (`(0)` is fine -- most reports have no
   keyboard accelerator; `ACCL_REPORTSUNCONN` is the one exception, kept from phase 1).
7. **Add a unit test** for the new formatting function in `unittest/reportstest.c` (empty-list and
   single/few-row cases at minimum), or a new CMocka test binary if the report doesn't fit that
   file's existing dependency shape -- see `unittest/CMakeLists.txt`'s `reportstest` target for
   how it links `reportsformat.c` directly rather than stubbing it.
8. **Add a `.xtr` demo fixture** under `app/lib/demos/` if the report needs one for manual/replay
   testing (e.g. a small synthetic layout that exercises an edge case like a zero-endpoint
   object) -- `dmreporttracklen.xtr`/`dmreportcurve.xtr`/`dmreportequip.xtr` are examples. Use the
   current `PARAMVERSION` (`xtrkcad-config.h`), not whatever an older fixture happens to have.

# Debug logging

Every report shares one debug log category, `"reports"` (`log_reports` in `reports.c`, lazily
resolved via `LogFindIndex()`). Run with `-d reports=1 -l <file>` and tail that file while
clicking through the new report's dialog -- `DoReportsOp()` already logs a line for every
Refresh/Save/Print click; add a `LOG(log_reports, 1, (...))` call in the new compute pass too if
it's worth seeing when live-testing.

# Gotchas {#creating-a-report-gotchas}

A few non-obvious things every report (or every interactive report) has to get right, each
learned from a real bug rather than anticipated up front:

- **Clear the visible list before freeing its backing array, not after.** GTK's `wListClear()`
  relocates the tree view's cursor as it deletes rows, which can *synchronously* re-fire the
  row-selection callback mid-clear, before the new rows exist. If the backing `dynArr_t` was
  already freed by that point, the callback reads a dangling per-row context pointer. Every
  interactive report clears the list, guarded by a `...Populating` flag the selection callback
  checks and no-ops on, strictly before its `DYNARR_FREE()` call -- see `ReportsUnconnectedEndpoints()`
  and the matching code in `ReportsGaps()`/`ReportsKinkedJoints()`.
- **The interactive-navigation indicator is one set of module-level globals, not per-dialog.** If
  two interactive report dialogs are open at once, clicking a row in the second replaces the
  first's indicator. This has been true since phase 1; a new interactive report doesn't need to
  solve it, just be aware selection state is shared, last-click-wins, across every interactive
  report dialog open at the same time.
- **The hidden text control backing Save/Print must never actually be shown.** It's created via
  `wTextCreate()`'s standalone/no-`.ui`-needed code path against its own backing window, with an
  explicit `wControlShow(FALSE)` -- both `wWinDialogCreate()` and `wTextCreate()` unconditionally
  show themselves on construction otherwise, and an earlier version of this code visibly leaked a
  stray empty window before that call was added. Copy `ReportsShowDialog()`'s existing setup
  rather than re-deriving it.
- **Turntable stalls (or other legitimately-always-open endpoints) may need special-casing.**
  `QueryTrack(trk, Q_CAN_ADD_ENDPOINTS)` is how Gaps distinguishes an open turntable stall (open
  by design, counted separately) from a genuine unconnected endpoint -- decide deliberately
  whether a new report should do the same rather than flagging every turntable stall as a
  problem.

# See also

\ref building "Building and Testing" covers the CMake/ninja mechanics referenced above in more
detail (rebuilding after a `.ui`/gresource change, running `ctest` by name). SF #217 on the
project's SourceForge tracker has the original feature request and the running history of every
phase if you need more background than this page and the source comments provide.
