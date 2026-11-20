# Layout Config Guide

This guide walks you through building a layout configuration file from scratch,
using the **Hillside Division** — a simple two-deck HO layout in a 12×16 ft room
— as the worked example.  By the end you will have four YAML files that together
describe the layout, the room, the benchwork and the track plan; and two tools
that turn them into an XTrkCAD `.xtc` file with the track already laid.

The complete Hillside Division files live in `mcp/tests/fixtures/`:

| File | Purpose |
|------|---------|
| `hillside_division.yaml` | Top-level config — scale, levels, links to the other three files |
| `hillside_floor_plan.yaml` | Room geometry — walls, doors, partitions, restricted areas |
| `hillside_benchwork.yaml` | Physical benchwork — shelf shapes, elevations, thicknesses |
| `hillside_track_plan.yaml` | Track plan — the main, the yard, the stations and their spurs |

All four files use the same coordinate system: **origin (0, 0) at the SW corner
of the room, X increases east, Y increases north, dimensions in model inches**
(1 inch in the file = 1 inch on the physical layout).

---

## 1. Top-level config

`hillside_division.yaml` is the entry point.  Open it and you will see:

```yaml
name: Hillside Division
scale: HO

floor_plan_file: hillside_floor_plan.yaml
benchwork_file:  hillside_benchwork.yaml

mainline: single
curve_radius: 20in
track_plan_file: hillside_track_plan.yaml
switch_size: "#6"
levels: 2
```

**`name` and `scale`** are required.  Scale sets the default minimum curve
radius, and the car length and track spacing the track plan uses (section 5).
Accepted scale names: `Z N TT HO S O G I Nn3 HOn3 On3 Sn3`.

**`floor_plan_file` and `benchwork_file`** load the room geometry and benchwork
plan from separate files.  Splitting them lets you share one floor plan across
several layout variants (a different track plan for each `.yaml`, all pointing at
the same `hillside_floor_plan.yaml`).  You can also write the floor plan and
benchwork inline using `floor_plan:` and `benchwork:` keys — useful for quick
experiments — but separate files are easier to maintain.

**`levels: 2`** tells the generator to create two sets of track layers plus the
Floor layer.  Each physical level gets seven layers:

| Layer name     | Purpose                            | Fugate category |
|----------------|------------------------------------|-----------------|
| `Ln-Main`      | Main line                          | mainline        |
| `Ln-Passing`   | Passing sidings                    | passing         |
| `Ln-Storage`   | Storage sidings / industry track   | storage         |
| `Ln-Staging`   | Staging yard                       | staging         |
| `Ln-Connecting`| Helix / ramp to adjacent level     | connecting      |
| `Ln-Service`   | Loco service, turntable leads      | service         |
| `Ln-Benchwork` | Benchwork outlines (filled polygons)| ignore         |

For a 2-level layout the layers are: Floor (0), L1-Main (1) … L1-Benchwork (7),
L2-Main (8) … L2-Benchwork (14).  The `get_operation_density` tool recognises
these names automatically — no manual category mapping needed.  Set `levels: 1`
for a single-deck layout.

**Sub-divisions on the same deck** (e.g. a connecting railroad's own staging
yard) can use an extra name segment: `L1-CO-Main`, `L1-CO-Passing`,
`L1-CO-Staging`.  Both `write_plan_view`'s level grouping and
`get_operation_density`'s category detection match on the leading `Ln-`
prefix and the trailing category segment respectively, so the middle
segment (`CO`) is free-form and doesn't need to be registered anywhere.

**Controlling which track-type layers are generated:**

By default all six Fugate track-type layers are created for every deck.  To
generate a subset, add `track_types:` to your config:

```yaml
track_types:
  - Main
  - Storage
  - Staging
```

To give each track type a distinct color in the Layers dialog (instead of one
color per deck), add:

```yaml
distinct_track_colors: true
```

The default per-type colors when `distinct_track_colors: true` are: Main=black,
Passing=teal, Storage=dark red, Staging=dark blue, Connecting=olive,
Service=dark gray.

The operation density report includes a **Track by Level** breakdown in addition
to the overall by-category totals.  This lets you see how each deck contributes
to mainline footage, car capacity, and turnout count — useful when balancing
operations across a multi-deck layout.  Helix or ramp track can be assigned to
a special pseudo-level (e.g. `L1H-Connecting`) and will appear as its own row.

**`mainline`, `curve_radius`, `switch_size`** are optional.  If you omit
`curve_radius` the generator uses the scale minimum (18 in for HO);
`curve_radius` is also the track plan's default radius.

**`track_plan_file`** names the track plan that `generate_track_plan` lays
along the benchwork, once the room and benchwork are in place (section 5).

---

## 2. Floor plan

`hillside_floor_plan.yaml` describes the physical room.  The Hillside room is a
simple 12×16 ft (144×192 in) rectangle with one door, one partition, and one
restricted area.

### Rooms

```yaml
rooms:
  - name: main
    x: 0in
    y: 0in
    width: 144in   # 12 ft east-west
    depth: 192in   # 16 ft north-south
```

A `rooms:` list defines the usable floor area.  Each room is a named rectangle.
`x`/`y` is the SW corner of the room within the total space; `width` runs
east–west, `depth` runs north–south.  All lengths accept a unit suffix —
`144in`, `12ft`, or a bare `144` (bare numbers are treated as inches).

When a floor plan is present the room bounding box is derived automatically, so
you do not need a separate `room: 12x16` key in the top-level config.

Multi-room layouts simply add more entries to the list.  Give each room a
distinct `name` — doors reference rooms by name.

### Doors

```yaml
doors:
  - room: main
    wall: east
    from: 36in
    width: 36in
    swing: inward
    clearance: 36in
```

This door is in the east wall of the `main` room.  `from: 36in` is the offset
from the *south* end of that wall (the lower-coordinate end).  `width: 36in` is
the opening width.

`swing: inward` tells the generator to draw a clearance zone inside the room so
you remember not to place benchwork in the door swing arc.  `clearance: 36in`
is the depth of that arc; if you omit it, it defaults to the door width.  Valid
swing values: `inward`, `outward`, `both`, `none`.

In the generated `.xtc` the clearance zone appears as a light-gray filled
polygon on the Floor layer.

### Partitions

```yaml
partitions:
  - label: furnace_wall
    x0: 92in
    y0: 0in
    x1: 92in
    y1: 36in
    thickness: 4in
```

A partition is a partial wall that does not follow a room perimeter — a knee
wall, a column, a stub wall closing off a utility corner.  `x0`/`y0` and `x1`/`y1` are
its endpoints; `thickness` is the wall depth.  The generator renders it as a
medium-gray filled rectangle on the Floor layer.

The Hillside furnace wall runs from (92, 0) to (92, 36) — a 36-inch vertical
stub closing off the furnace corner (the restricted area below) from the room.
The rectangle is drawn east of `x0`, so it fills x=92 to 96.

If a door opening falls along a partition the generator automatically cuts a gap
in the rendered wall — both in the `.xtc` Floor layer and in `write_plan_view`'s
SVG plan view, which share the same gap-cutting geometry
(`partition_door_openings()` in `config.py`). The gap is cut regardless of
`swing`: even a `swing: none` doorway (no door hardware, just an opening) is
still a hole in the wall, not a solid one.

### Restricted areas

```yaml
restricted:
  - label: furnace_corner
    x: 96in
    y: 0in
    width: 48in
    depth: 36in
    reason: HVAC unit
```

Restricted areas are no-go zones — HVAC units, stairs, structural columns,
electrical panels.  `x`/`y` is the SW corner; `width` and `depth` give the
size.  The `reason` field is informational only.  The generator renders the zone
as a light-gray polygon on the Floor layer alongside the door clearance zones.

For non-rectangular zones you can supply an explicit `vertices:` list instead of
`x`/`y`/`width`/`depth`.

---

## 3. Benchwork

`hillside_benchwork.yaml` is a YAML list — each entry is one physical benchwork
section.  Every entry has at minimum a `label`, `level`, and `shape`.

### Coordinate note

Benchwork is placed in the same coordinate system as the floor plan.  Sections
are free-floating; they are not attached to walls.  The floor plan is reference
geometry for your planning — benchwork placement is your design decision.

### Required fields

```yaml
- label: west_wall
  shape: rect
  level: 1
  elevation: 40in
  thickness: 3.5in
  x: 0
  y: 0
  width: 24
  length: 168
```

| Field | Meaning |
|-------|---------|
| `label` | Unique name for this section (used in reports) |
| `shape` | One of `rect`, `rotated_rect`, `polygon`, `arc`, `spline` |
| `level` | Deck number, 1-based.  Controls which XTrkCAD layer the polygon lands on. |
| `elevation` | Height of the **top surface** above the floor, real inches.  Track elevation is always above benchwork elevation — benchwork is the physical base, not the track surface itself. |
| `thickness` | Board depth below the top surface, real inches.  Used in elevation reports and section drawings.  Typical values: 3½" for a single-deck main frame, 1½" for an upper-deck shelf. |

### Shape types

**`rect`** — axis-aligned rectangle.  `x`/`y` is the SW corner; `width` runs
east–west; `length` runs north–south.

```yaml
- label: west_wall
  shape: rect
  level: 1
  elevation: 40in
  thickness: 3.5in
  x: 0
  y: 0
  width: 24        # 2 ft deep into room
  length: 168      # y=0 to y=168 (meets the north shelf)
```

**`rotated_rect`** — rectangle rotated about its centre.  Use this for 45°
corner modules.  `cx`/`cy` is the centre; `rotation` is counter-clockwise
degrees.  The hypotenuse of a 45° corner module with shelf depth *d* has length
*d* × √2 (e.g. a 26-inch shelf → 36.8-inch diagonal).

```yaml
- label: sw_corner
  shape: rotated_rect
  level: 1
  elevation: 40in
  thickness: 3.5in
  cx: 26
  cy: 26
  width: 24        # narrow dimension
  length: 36       # hypotenuse of 26×26 right triangle
  rotation: 315
```

**`polygon`** — arbitrary shape given as an explicit vertex list.  Use for
right-angle corner fills and any irregular outline.  The key is `vertices:`, not
`points:`.

```yaml
- label: se_corner_fill
  shape: polygon
  level: 2
  elevation: 58in
  thickness: 1.5in
  vertices:
    - [125, 176]
    - [97,  176]
    - [125, 204]
```

**`arc`** — curved shelf approximated as a closed polygon at 2°/step.  `cx`/`cy`
is the arc centre; `radius` is the centreline radius; `width` is the radial
shelf depth; `start_angle` and `sweep_angle` are in degrees.

```yaml
- label: curved_corner
  shape: arc
  level: 1
  elevation: 40in
  thickness: 3.5in
  cx: 144
  cy: 0
  radius: 24
  width: 18
  start_angle: 90
  sweep_angle: 90
```

**`spline`** — a narrow strip following a polyline centreline, useful for
peninsula spines or connecting runs that follow a track path.  `points:` (not
`vertices:`) defines the centreline; `width` is the strip width.  The generator
builds a miter-joined offset polygon from the centreline.

```yaml
- label: peninsula_spine
  shape: spline
  level: 1
  elevation: 36in
  thickness: 3.5in
  width: 12
  points:
    - [60,  96]
    - [120, 96]
    - [180, 96]
```

### The Hillside benchwork

The Hillside Division uses `rect` for its three Level 1 wall shelves, a U
around the west, north and east walls.  The north shelf runs the full width of
the room; the west and east shelves run up to it, so no two sections overlap:

```yaml
- label: north_wall
  shape: rect
  level: 1
  elevation: 40in
  thickness: 3.5in
  x: 0
  y: 168           # 192 - 24 = 168 (24 in from north wall)
  width: 144       # x=0 to x=144, the full width
  length: 24
```

The east wall shelf starts at y=72 to leave headroom above the entry door:

```yaml
- label: east_wall
  shape: rect
  level: 1
  elevation: 40in
  thickness: 3.5in
  x: 120           # 144 - 24 = 120 (24 in from east wall)
  y: 72            # starts above door clearance zone
  width: 24
  length: 96       # y=72 to y=168 (meets the north shelf)
```

The two Level 2 sections demonstrate the `elevation:` field — same footprint as
the L1 north and east walls but 18 inches higher:

```yaml
- label: north_wall_l2
  shape: rect
  level: 2
  elevation: 58in  # 40 + 18 in separation
  thickness: 1.5in
  x: 0
  y: 178           # 192 - 14 = 178 (14 in deep at upper deck)
  width: 144
  length: 14

- label: east_wall_l2
  shape: rect
  level: 2
  elevation: 58in
  thickness: 1.5in
  x: 130           # 144 - 14 = 130
  y: 72
  width: 14
  length: 106
```

### Outer-edge connection rule

When a corner module meets a wall shelf, extend the wall shelf to the corner
module's *outer toe* — the outermost face that touches the wall — not just to
the inner face.  This keeps the outer edge of the benchwork visually continuous
at the wall face.  In the Hillside example the west and east shelves run up to
the north shelf (y=168), and the north shelf runs the full width to both walls,
so the outer edge is continuous along every wall.

---

## 4. Generating the .xtc file and views

### MCP server tools

The layout config system is exposed as an MCP server with tools you can call
from Claude or any MCP-compatible client:

| Tool | What it does |
|------|-------------|
| `load_layout_config` | Validate a config and show a summary before generating |
| `generate_layout` | Write the `.xtc` file from a validated config |
| `generate_track_plan` | Lay the config's track plan into that `.xtc` (section 5) |
| `merge_benchwork` | Replace floor/benchwork layers in an existing layout (preserves tracks and NOTEs) |
| `write_plan_view` | Top-down SVG plan — one deck or all decks |
| `write_elevation_view` | Wall-profile SVG showing benchwork heights |
| `write_benchwork_report` | Text/HTML table of every section with area and bounding box |
| `add_track_layers` | Add standard Fugate track-type layers to any .xtc |
| `rename_layers` | Rename layers by name so auto-categorization works |
| `get_operation_density` | Compute Fugate op-density metrics; returns `by_level` breakdown |
| `write_operation_density_report` | Write op-density report (txt / md / html) |

**Generate the layout:**
```
generate_layout("hillside_division.yaml", "hillside_division.xtc")
```

**Generate plan views** (one per deck):
```
write_plan_view("hillside_division.yaml", "hillside_plan_l1.svg", level=1)
write_plan_view("hillside_division.yaml", "hillside_plan_l2.svg", level=2)
```
Pass `level=0` (the default) to draw all decks together on one view.

Pass `xtc_path` to overlay real track geometry (straights, curves, turnouts,
turntables) from a generated `.xtc`/`.xtce` file on top of the benchwork,
filtered to the same deck via each track's layer name (`L1-*` → level 1,
`L2-*` → level 2):
```
write_plan_view("hillside_division.yaml", "hillside_plan_l1.svg", level=1,
                 xtc_path="hillside_division.xtc")
```
Without `xtc_path` the plan view shows benchwork only, which is normal when
no `.xtc` has been generated yet.

**Generate elevation views** (one per wall):
```
write_elevation_view("hillside_division.yaml", "hillside_elev_west.svg",  wall="west")
write_elevation_view("hillside_division.yaml", "hillside_elev_south.svg", wall="south")
```
The `levels` argument filters by deck: `levels=[1]` for Level 1 only,
`levels=[1, 2]` for both.  Valid walls: `west`, `south`, `east`, `north`.

Open the SVG files in any web browser to review the benchwork plan visually
before committing to the `.xtc`.

### Direct Python API

The same operations work as a Python library:

```python
from pathlib import Path
from xtrkcad_mcp.config import load_config
from xtrkcad_mcp.generator import generate, default_output_path
from xtrkcad_mcp.svg_views import generate_plan_view, generate_elevation_view

p = Path("hillside_division.yaml")
result = load_config(p)
out = default_output_path(result.config, p)   # hillside_division.xtc
generate(result.config, out)

generate_plan_view(str(p), "hillside_plan_l1.svg", level=1)
generate_elevation_view(str(p), "hillside_elev_west.svg", wall="west")
```

If a `.xtc` file already exists at `out` it is backed up automatically as
`hillside_division_v1.xtc`, `_v2`, etc. before being overwritten.

Open the `.xtc` in XTrkCAD.  The Floor layer (0) shows the room perimeter,
door clearance zones, the furnace wall partition, and the HVAC restricted area.  The
L1-Benchwork layer (7) shows Level 1 shelf outlines.  The L2-Benchwork layer
(14) shows Level 2 shelves.  Layers 1–6 (L1-Main through L1-Service) and 8–13
(L2-Main through L2-Service) are ready for track placement.  Use the Layers
dialog to toggle each level or track type independently.

The file also defines **Layer Groups** (Manage → Layer Groups in XTrkCAD):
one per track type across all decks (`Mainline` holds L1-Main and L2-Main,
`Staging` holds L1-Staging and L2-Staging, and so on) and, on a multi-deck
layout, one per deck (`Level 1`, `Level 2`).  Use them in XTrkCAD to show,
select or report on a whole category or deck at once.  The category groups
also tell `get_operation_density` how to classify track (see section 6).

The generated file uses XTrkCAD's current file format (`VERSION 13`), so it
needs XTrkCAD 5.4 or later.

**Adding track-type layers to an existing file:**

If you started placing track before running `generate_layout` with the current
config, or if you have a hand-built `.xtc`, use `add_track_layers` to inject
the standard layers without disturbing any existing track:

```
add_track_layers("hillside_division.xtc")
```

The tool detects the number of levels from existing `Ln-*` layer names, adds
any missing layers (skipping ones already present), and backs up the file
before writing.  Pass `levels=N` to override the auto-detected count.

**Fixing benchwork in a hand-drawn layout:**

If you drew your own room walls and started placing track before generating
benchwork, or if you later revised the benchwork YAML (e.g. extended a corner
module), use `merge_benchwork` to replace only the floor/benchwork fill objects
while leaving all track and NOTE objects intact:

```
merge_benchwork(
    "my_layout.xtc",          # existing hand-drawn layout
    "my_layout_config.yaml",  # updated benchwork config
)
```

The existing file must already have `Floor` and `L{n}-Benchwork` layers — use
`add_track_layers` first if they're missing.  The tool backs up the file
(to `_v1.xtc`, `_v2.xtc`, etc.) then writes the merged output in-place.  Room
wall DRAWs (Q3 type) are also preserved.

Pass a third argument to write to a different path instead of replacing in-place:

```
merge_benchwork("my_layout.xtc", "my_layout_config.yaml", "my_layout_fixed.xtc")
```

**Renaming custom layer names to standard names:**

If you placed track using your own layer names and want op-density
auto-categorization to work without always passing a `layer_categories` dict,
use `rename_layers` to update the names in-place:

```
rename_layers("hillside_division.xtc", {
    "My Yard":    "L1-Staging",
    "Upper Main": "L2-Main",
})
```

Layer IDs and all track object references stay unchanged — only the display
name in the LAYERS record is updated.  The tool returns `renamed` (applied
pairs) and `not_found` (names that weren't in the file) so you can verify
the result.  After renaming, `get_operation_density` will auto-categorize
using the standard suffix rules with no extra arguments.

---

## 5. Laying the track plan

`hillside_division.yaml` names a fourth file, `track_plan_file:
hillside_track_plan.yaml`.  It describes the track you want, not its geometry:
which shelves the main runs along, a yard at the start, and a station on each
wall.  `generate_track_plan` works out the curves, turnouts and track and adds
them to the `.xtc` from `generate_layout`:

```
generate_layout("hillside_division.yaml", "hillside_division.xtc")
generate_track_plan("hillside_division.yaml", "hillside_division.xtc")
```

The Hillside plan is a single-track main from a stub yard at the south end of
the west wall, north to Alder, east along the north wall through Birch, and
south down the east wall to Cedar, the end of the line, short of the door:

```yaml
level: 1
curve_radius: 20in
turnout: "PECO Code 83 #6"     # from XTrkCAD's parameter files, both hands
track_offset: 12in             # the main's distance from the wall

route:                         # the main line, in order, along these shelves
  - west_wall
  - north_wall
  - east_wall

yard:                          # stub yard at the start of the route
  id: HY
  name: Hillside Yard
  tracks: 3
  track_length: 6 cars
  side: room

stations:
  - id: AL
    name: Alder
    shelf: west_wall
    siding: 5 cars
    siding_side: room
    spurs:
      - {id: AL_FEED, name: Alder Feed Mill, length: 3 cars, side: wall,
         points_toward: start}
      - {id: AL_TEAM, name: Alder team track, kind: team_track, length: 2 cars,
         side: wall, points_toward: end}
  # ... Birch on the north wall, Cedar (terminus: true) on the east wall
```

| Key | Meaning |
|-----|---------|
| `level` | The deck the track goes on; every shelf in `route` must be on it |
| `curve_radius` | The corner curves and the curves back to parallel after a turnout (default: the top-level `curve_radius`) |
| `turnout` | Manufacturer and number, matched against XTrkCAD's parameter files: `"PECO Code 83 #6"` finds the #6 left and right hand turnouts |
| `track_offset` | The main's distance from the wall (default: the shelf's centre) |
| `track_spacing` | Between parallel tracks (default 2 in in HO, scaled for other scales) |
| `clearance` | Track centres this far apart clear each other: a car standing beyond the clearance point doesn't foul the other track (default 13 ft scaled, 1.79 in in HO) |
| `route` | The shelves the main runs along, in order.  It starts at the first shelf's far end and ends at the last one's; each corner gets a curve |
| `yard` | A stub yard at the start: `tracks`, `track_length` (what the shortest track holds), `side` |
| `stations` | One per shelf: `siding` (what the siding holds), `siding_side`, optional `at` (from the shelf's start; default centred), `spurs`.  `terminus: true` makes it the end of the line: the siding becomes a runaround with `tail` of plain track beyond it |
| `spurs` | `kind` (`industry`, `team_track` or `facility`, with a facility `type`), `length` (what the spur holds), `side`, and `points_toward`: which end of the line the turnout's points face |

Lengths take `in`, `ft` or `cars` (one car is the scale's car length, 6 in in
HO).  A side is `wall` or `room`, or `left`/`right` of the direction of travel.

A siding, spur or yard track's length is what it **holds**: the track between its
**clearance points**, where it is far enough from the track it leaves that cars on
the two clear each other (`clearance`, 13 ft between centres on the prototype).
A car standing nearer the switch fouls the other track.  For a Peco #6 in HO the
clearance point is 13.95 in from the points, about a car length past the frog;
the tool builds each track long enough to hold what you asked between its
clearance points, and reports for each `capacity_in`/`capacity_cars` (what it
holds), `clear_in` (its straight part) and `clearance_in`.

The main goes on `L1-Main` with the turnouts it runs through, sidings on
`L1-Passing`, the yard and industry spurs on `L1-Storage`, and facility spurs on
`L1-Service`.  A spur on the other side from a siding sits beside it; on the same
side the two run end to end.

When something doesn't fit, nothing is written, and the error says what and by
how much, e.g. `station BI: needs 99.9 in on shelf north_wall, 80.0 in free
there` for a 12-car siding.  A layout that already has track on the plan's level is refused, so
generate the layout again to start over.  Version 1 of the tool takes straight
(rectangular) shelves along the walls, meeting at right angles, with one
station per shelf.

---

## 6. Next: operating data

The layout from sections 1–5 has its room, benchwork and track.  To operate it,
the layout file also records its railroad as built: stations, industries, yards,
mileposts and which layers are main track.  That's the job of the
[Operating Data Conventions](operating-conventions.md): JSON Notes placed on the
track and Layer Groups, which `check_conventions` checks and
`export_layout_data` writes out as `layout_data.json`
([schema](layout_data_v2.md)).  [Part 2, the operating-conventions
tutorial](operating-conventions-tutorial.md), picks up the Hillside layout where
this guide leaves it.

### Reporting on part of a layout

Most read and report tools take two optional arguments that limit them to part
of the layout: `layers` (layer names or index numbers) and `groups` (Layer
Group names).  Only tracks and notes on the selected layers are used.  This is
how one file holding two railroads gets a separate report for each:

```
get_layer_groups("my_layout.xtc")
write_layout_report("my_layout.xtc", "nye_report.html", format="html",
                    groups=["NYE"])
get_station_distances("my_layout.xtc", groups=["C&O"])
```

`get_operation_density` classifies each layer, in this order: an explicit
`layer_categories` entry, a Layer Group named after a category (`Mainline`,
`Passing`, `Storage`, `Staging`, `Connecting`, `Service`), then the layer
name's suffix (`L1-Staging`, `L2-CO-Passing`).  Anything else counts as
mainline.

---

## Quick-start checklist

1. Copy the three Hillside files to a new folder and rename them for your layout.
2. Edit the floor plan: adjust room size, door position, any partitions or restricted areas.
3. Sketch your benchwork plan on paper, then translate it to benchwork sections in the YAML — one entry per physical shelf module.
4. Set `elevation:` and `thickness:` on every section.
5. Set `levels:` in the top-level config to match your deck count.
6. Generate the `.xtc` and open it in XTrkCAD to verify the floor plan and benchwork look right before placing any track.
7. Write the track plan and run `generate_track_plan`, then open the `.xtc` in XTrkCAD and refine the plan from there.
