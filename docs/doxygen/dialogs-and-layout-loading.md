\page dialogs-and-layout-loading Dialogs and Layout Loading

What happens to open, non-modal dialogs when a new layout replaces the current one (File > Open,
File > New, demo playback), and what a new dialog has to declare so it behaves correctly. The
implementation is in `app/bin/form/dialog.c` (`FormCloseLayoutDialogs()`,
`FormReloadLayoutDialogs()`); the hooks are in `app/bin/track.c` (`ClearTracks()`) and the load
paths in `app/bin/fileio.c`, `app/bin/misc.c` and `app/bin/macro.c`.

\tableofcontents

# The problem

Most dialogs are non-modal: the main window and its menus stay usable while they are open, so a
user can open another layout with, say, Layout Options still on screen. Before this mechanism
existed such a dialog kept showing the old layout's values, and pressing its Ok button wrote
them into the layout that had just been loaded. Dialogs that hold a track or a car were worse:
`ClearTracks()` frees those, and the next click in the dialog used freed memory.

`RegisterChangeNotification()` doesn't solve this. The `CHANGE_ALL` broadcast after a load
reaches the handlers, but most handlers only call `FormLoadControls()`, which copies the bound
variables back into the widgets and skips the setup the dialog's open function does (Layout
Options computes the room size and background name, Layers copies the current layer into its
variables, Note fills its text control). Writing and maintaining a second, correct "refresh"
function for every dialog would be the same bug waiting to happen again.

# Three kinds of dialogs

Every dialog falls into one of three groups. A new dialog must be put into the right one.

| Kind | Declared by | What happens on a new layout | Examples |
|---|---|---|---|
| **Reload** | `paramGroup_t.reloadProc` | stays open, reopened with the new layout's data | Layout Options, Layers, Layer Groups, Note, Manage Notes, Car Inventory, Manage Layout Control Elements, Parts List, Select Layers/Groups, every report |
| **Close** | `PGO_LAYOUT_CLOSE` in `paramGroup_t.options` | closed | Describe/Properties, Elevation, Profile, Helix, Structure, Add Fixed-Track, Print, Export to bitmap, Raise/Lower Elevations, Filter Layers/Groups, the name-entry sub-dialogs of Layer Groups and Manage Notes |
| **None** | nothing | left alone | Display Options, Color, Preferences, Snap Grid, Easement, Map, Parameter Files, Turnout Designer, About |

How to choose:

- The dialog shows or edits data that is saved in the layout file and belongs to the layout as a
  whole: **reload**.
- The dialog works on a selection, a single track or car, or belongs to the active command: **close**.
  There is nothing sensible to show once that object is gone.
- The dialog only shows or edits preferences or parameter files: **none**.
- Modal dialogs (`F_MODAL`) need nothing: the menus can't be reached while one is open. Demo
  playback turns modality off, but demos replay scripted steps and don't open a layout under an
  open editor.

# How it works

1. **Before the old layout is freed.** `ClearTracks()` starts with `TrainModeExit()` and
   `FormCloseLayoutDialogs()`, while every track and car still exists. Every path that replaces
   the layout goes through `ClearTracks()`. `LoadTracks()` also calls `Reset()` before
   `ClearTracks()`, so the active command is cancelled while its tracks are still valid (it used
   to be cancelled only at the end of the load).
2. **`FormCloseLayoutDialogs()`** walks every registered group (`DialogGroupIter()`) whose window
   is visible:
   - `PGO_LAYOUT_CLOSE`: calls the group's `cancelProc`, as the Cancel button would. A group
     without one gets `changeProc(pg, -1, NULL)`, the notification its window's close button
     sends. If the window is still visible afterwards it is hidden with `wHide()`.
   - `reloadProc` set: the group is remembered.
   - Otherwise nothing happens.
3. **After the new layout is complete.** `FormReloadLayoutDialogs()` calls `reloadProc(NULL)` for
   each remembered group and forgets them. It is called at the end of `LoadTracks()` (after the
   successful and the failed load alike: a failed load leaves an empty layout), at the end of
   File > New (`DoClearAfter()`) and when demo playback ends.

The close step deliberately doesn't send `wClose_e` through `DialogProc()`: that ends in
`DefaultProc()` → `ConfirmReset()`, which can show a prompt in the middle of a load.

A missed `FormReloadLayoutDialogs()` call site is harmless: the dialogs just stay open with the
old content, as before. The list is cleared at the start of every `FormCloseLayoutDialogs()`, so
a stale entry can't reopen a dialog later.

# Declaring a new dialog

**The group must be registered with `FormRegister()`.** Only registered groups are visited.
Registering lazily on first show is fine; the window can't be open before that.

**Reload:** set `reloadProc` next to the `FormRegister()` call, to the function that opens the
dialog:

```c
FormRegister(&layoutPG);
layoutPG.reloadProc = DoLayout;
```

The function has the usual `void (*)(void *)` menu-callback signature and is called with `NULL`.
It must fully repopulate the dialog from the current layout when the window already exists,
exactly as when the user opens it a second time. That is the normal shape of an open function
(`if (win == NULL) create; load the data; wShow(win);`), so usually nothing new is needed. If
the open function can return early without touching the window, wrap it. `EnumReload()` in
`denum.c` hides the Parts List first because `EnumerateTracks()` returns without showing
anything when the new layout has no parts, which would leave the old list on screen.

Reports get this for free: `ReportsShowDialog()` sets `rd->pg->reloadProc = rd->refreshProc`
(see \ref creating-a-report).

**Close:** add `PGO_LAYOUT_CLOSE` to the group's options, in its initialiser or, for groups
created at run time, right after creation (as `DoDescribe()` does). Check that the group's
`cancelProc` doesn't ask anything: it runs in the middle of a load, and the user has already
agreed to drop the old layout.

# Special cases

- **Train mode.** `Reset()` doesn't cancel the active command in train mode (see the comment in
  `command.c`), and train mode holds tracks (`trainControlDlg_t.train` and others). Opening a
  file in train mode is possible, so `ClearTracks()` calls `TrainModeExit()` first. It ends train
  mode the same way the Exit button does and hides the Train Control dialog.
- **New Car Part / car edit dialog.** Its `cancelProc` (`CarDlgCancel()`) asks before
  discarding changes, and the dialog can be editing a car item that `ClearCars()` frees. So it
  is not marked `PGO_LAYOUT_CLOSE`; `ClearCars()` closes it through `CarDlgDiscard()`, which
  never asks.
- **Car Inventory.** Its list holds pointers to car items. `ClearCars()` empties it through
  `CarInvDlgClearList()` before freeing the items: the `CHANGE_PARAMS` notification during the
  load would otherwise clear the list later, and the resulting selection callback would read
  freed items.
- **Rotate, Move and Select Index** (`menu.c`) are not registered and so are not visited. They
  act on the selection, which is empty after a load, so their Ok does nothing harmful.

# See also

`app/bin/include/form.h` documents `PGO_LAYOUT_CLOSE` and `reloadProc`. The \ref
creating-a-report page describes `refreshProc`, which reports use as their `reloadProc`.
