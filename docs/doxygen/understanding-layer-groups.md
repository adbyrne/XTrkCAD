\page understanding-layer-groups Understanding Layer Groups

How Layer Groups (SF #222) is put together -- a named, flat, user-managed collection of `Layer`
memberships, and the small set of primitives every consumer of it (Select Layers/Groups, the
Reports filter, the Print/Export filter, and any future site) is built from. Read
`app/bin/include/dlayergroup.h` alongside this page -- it's the authoritative reference; this page
is the map.

\tableofcontents

# Shipped in three phases

- **Phase 0** (SF #782): the data model itself, the Manage Layer Groups dialog
  (`dlayergroupui.c`), Linked-Layers migration, and the "Show Only" visibility action.
- **Phase 1** (SF #787): two scope sites -- Select Layers/Groups (`dselectlayers.c`, acts on
  canvas selection) and a shared Filter Layers/Groups dialog reused by all seven Reports menu
  dialogs (`dreportsfilter.c`, acts as a report's row-inclusion scope).
- **Phase 2** (SF #789): Print/Export filter (`dprintexportfilter.c`) -- a single filter shared
  across Print, DXF export, and SVG export, reusing phase 1's Filter dialog directly rather than
  building a fourth copy.

# The data model

`dlayergroup.c`/`.h` is deliberately standalone -- plain `int`/array types, no dependency on
`common.h`/`dynarray.h`/`wlib` -- so it links and CMocka-tests the same lightweight way as
`reportsformat.c`. A group is just a name (`LAYERGROUP_NAME_SIZE`, 32 chars) plus an
insertion-ordered array of 1-based layer indices. `LayerGroupCreate()`/`LayerGroupRename()` both
reject an empty name or one that collides with another existing group (`NameCollides()`, SF #789
gap-review fix) -- every Groups list in the UI identifies a group by its displayed name, so two
same-named groups would be indistinguishable there even though the data model itself doesn't
require uniqueness.

**1-based vs. 0-based is a real seam, not a typo waiting to happen.** `LayerGroupMemberAt()`/
`LayerGroupHasMember()` take and return 1-based layer numbers (matching the main window's own
layer buttons and layer selector). Every consumer that bridges into a 0-based array -- most
visibly `reportsFilter_t.included[]` in `dreportsfilter.c` -- has to subtract 1 explicitly
(`FilterAddGroup()`'s `layerIdx1Based - 1` is the pattern to copy). Get this wrong and a group's
membership silently maps onto the wrong layer, one off.

`LayerGroupMigrateFromLinkLists()` converts the older, per-layer `layerLinkList` field (still
read from `LAYERS LINK` lines in older/`.xtc` files) into groups at load time, once, for any file
with `paramVersion < 13` -- see `fileio.c`'s call site and `unittest/layergrouptest.c`'s
`test_migrate_*` cases (including the exact mutual-link-dedupe shape the shipped
`Ondaville Franklin and Carolina RR.xtc` example exercises).

# Two ways a dialog uses group data, and which one to reuse

Almost nothing outside `dlayergroupui.c` should call `LayerGroupCreate`/`Rename`/`Delete`/the
membership mutators directly -- those all live behind the Manage Layer Groups dialog. Every other
site needing "let the user pick some layers/groups" reuses one of two existing building blocks
instead of inventing a third:

- **Selection scope** -- `dselectlayers.c`'s `SelectLayerSet()` (via `cselect.c`), driving the
  canvas's actual track selection. Use this when the site's job is "select these tracks", full
  stop; the Select Layers/Groups dialog itself is the only current consumer.
- **Filter scope** -- `dreportsfilter.c`'s `reportsFilter_t` type (an `included[NUM_LAYERS]`
  bool array + a cached count) plus `ShowReportsFilterDialog()`. Use this when the site needs a
  *persistent, named* scope that isn't the canvas selection -- a report's row-inclusion filter,
  or (SF #789) Print/Export's draw-time filter. `dprintexportfilter.c` is the pattern to copy for
  a new filter-scope site: one static `reportsFilter_t` instance, one `PD_BUTTON` callback that
  calls `ShowReportsFilterDialog(&yourInstance)`, done -- no new dialog, no new `.ui` file.

Both shuttle Available/Included lists rather than using checkboxes, for the same reason: `wlib`
has no API to pre-select specific `PD_LIST` rows, so a checkbox-per-row design has nowhere to
persist "this row is checked" across a dialog re-show. A group applied to either is a one-shot
*preset*, not a stateful link -- moving a group's current members into Included/selection doesn't
keep tracking that group afterward, and each row stays independently movable.

# Gotchas

- **These dialogs are non-modal on GTK3, even though `F_BLOCK` is passed to `FormCreateDialog()`.**
  `F_BLOCK` is defined in `wlib.h` but the GTK3 backend's `wWinDialogCreate()` never checks it
  (confirmed by reading it) -- so Manage Layer Groups can be open and edited at the same time as
  a Filter/Select dialog that's displaying a Groups list built from the *old* state.
  `dreportsfilter.c`/`dselectlayers.c` both register a `CHANGE_LAYER` notification callback
  (`RegisterChangeNotification()`, same pattern as `dlayer.c`'s own `LayerChange()`) that
  refreshes their Groups list live while visible -- copy this for any new filter/select site,
  don't assume "populated once when shown" is good enough. Before this fix (SF #789 gap review),
  a stale cached Groups-list row could point at a *different* group than the one displayed after
  a group elsewhere was deleted (`LayerGroupDelete()` shifts every later index down by one) --
  silently wrong data applied, not a crash, which is what made it easy to miss.
- **A group is a one-shot preset, not a live link** (see above) -- don't design a new site that
  expects "included via group membership" to stay true if the group's own membership changes
  later. If that's genuinely the semantics a new site needs, it's a different feature, not a
  reuse of the existing Filter/Select machinery.
- **`LayerGroupDelete()` shifts every later group's index down by one.** Never hold a bare `int`
  group index across any user-visible gap where Manage Layer Groups could plausibly run
  concurrently (i.e. basically any modeless dialog) -- re-resolve by name, or refresh the whole
  list, instead.
- **The buttons were renamed after review; the code kept the old names.** Martin's #4404 review
  asked for Show All / Show Only / Show Not. Ken's testing (dev-ML #4490) showed that "Show All"
  read as "make every layer visible", so he never found the view restore he was asking for. The
  labels became **Show Only / Hide Only / Restore View**, positive actions throughout. The
  functions (`LayerGroupShowOnly()`, `LayerGroupShowNot()`, `LayerGroupShowAll()`) and the `.ui`
  widget IDs (`showonly`, `shownot`, `showall`) still use the old names.
- **The current layer can never be hidden by Show Only or Hide Only.** Both buttons go through
  one shared filter in `dlayer.c` (`LayerGroupShowOnly()`/`LayerGroupShowNot()` ->
  `LayerGroupApplyFilter()`), and the per-layer rule lives in the unit-tested
  `LayerGroupFilterShows()` (`dlayergroup.c`): it always keeps `curLayer` visible, matching the
  existing rule that the current layer can't be hidden. A new group-driven visibility action
  should reuse that rule rather than apply group membership literally. Hide Only (dev-ML #4404,
  added after phase 2) is the mirror of Show Only: it hides the group and shows everything else.
- **The filter is a snapshot, not a stack.** The first Show Only or Hide Only copies every
  layer's real visibility into `layerVisibleSnapshot[]` and sets
  `layerVisibilityOverrideActive`. Further Show Only / Hide Only clicks, on any group, change
  `layers[].visible` but leave the snapshot alone, so Restore View (`LayerGroupShowAll()`) always
  returns to the view from before the *first* filter, never to the previous step. Hide Only is
  the mirror of the group, not an undo of Show Only: Show Only G1 then Hide Only G1 shows the
  complement of G1, not the starting view. A "Current View" preset group is therefore not
  needed (it was suggested in #4490); the snapshot already is one. With no filter active there's
  nothing to restore, so `LayerGroupShowAll()` makes every layer visible and returns `TRUE`, and
  only then does the dialog mark the file changed.
- **What stays real while a filter is active.** `layers[].visible` is the one value that drawing
  and the toolbar read, so filtered layers really are hidden. But `WriteLayers()` writes the
  snapshot, so the filter never reaches the file, and `ResetLayers()` (New, Open) ends it. A
  deliberate change to one layer, through `FlipLayer()` or the Layers dialog's Canvas check-box,
  goes through `LayerVisibilitySetReal()`, which updates the snapshot as well, so Restore View
  keeps it.
- **Every programmatic change to `layers[].visible` must also set that layer's toolbar
  button.** The layer buttons are GTK toggles. A click flips the button's own state before
  `FlipLayer()` runs, so `FlipLayer()` only updates the buttons of *linked* layers, never the
  layer it flipped. Code that changes visibility any other way has to call
  `wButtonSetBusy(layer_btns[l], layers[l].visible)` itself, guarded by `l < NUM_BUTTONS`,
  `!layers[l].button_off` and `layer_btns[l]`, as `LayerGroupApplyFilter()` and
  `LayerGroupShowAll()` do. Otherwise the button shows the opposite of the layer, and every later
  click does the opposite of what the user sees. `SetCurrLayer()` missed this when it unhid a
  hidden layer picked as the current layer, an old bug in plain layers that predates Layer Groups
  (found through #4490).

# See also

SF #222 (feature-requests tracker) is the original umbrella request; #782/#787/#789 (bugs
tracker) are the three implementation phases above, each with its own build/verification history.
\ref creating-a-report "Creating a New Report" is the closest sibling page in shape (a shared
mechanism with several call sites) if you want a second example of this page's own structure.
