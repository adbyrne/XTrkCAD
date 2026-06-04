# Layout Config Guide

This guide walks you through building a layout configuration file from scratch,
using the **Hillside Division** — a simple two-deck HO layout in a 12×16 ft room
— as the worked example.  By the end you will have three YAML files that together
describe the room geometry, the benchwork plan, and the track layout; and a tool
that converts them into an XTrkCAD `.xtc` file.

The complete Hillside Division files live in `mcp/tests/fixtures/`:

| File | Purpose |
|------|---------|
| `hillside_division.yaml` | Top-level config — scale, levels, links to the other two files |
| `hillside_floor_plan.yaml` | Room geometry — walls, doors, partitions, restricted areas |
| `hillside_benchwork.yaml` | Physical benchwork — shelf shapes, elevations, thicknesses |

All three files use the same coordinate system: **origin (0, 0) at the SW corner
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
curve_radius: 22in
switch_size: "#6"
levels: 2

grid: []
```

**`name` and `scale`** are required.  Scale controls the minimum curve radius
default and the template geometry used when placing track elements in the grid.
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
`curve_radius` the generator uses the scale minimum (18 in for HO).  These
values influence grid-cell sizing and which track templates are selected.

**`grid: []`** is an empty track placement list.  Section 5 covers the grid
syntax once the room and benchwork are in place.

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
  - label: alcove_wall
    x0: 48in
    y0: 156in
    x1: 48in
    y1: 192in
    thickness: 4in
```

A partition is a partial wall that does not follow a room perimeter — a knee
wall, a column, a stub wall separating an alcove.  `x0`/`y0` and `x1`/`y1` are
its endpoints; `thickness` is the wall depth.  The generator renders it as a
medium-gray filled rectangle on the Floor layer.

The Hillside alcove wall runs from (48, 156) to (48, 192) — a 36-inch vertical
stub in the NW corner.  It splits the north wall benchwork into two sections
(see `hillside_benchwork.yaml`).

If a door opening falls along a partition the generator automatically cuts a gap
in the rendered wall.

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
  length: 156
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
  length: 156      # y=0 to y=156 (stops at alcove wall)
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

The Hillside Division uses `rect` for all four wall shelves.  The north wall is
split into two sections to clear the alcove partition at x=48:

```yaml
- label: north_wall_west
  shape: rect
  level: 1
  elevation: 40in
  thickness: 3.5in
  x: 0
  y: 168           # 192 - 24 = 168 (24 in from north wall)
  width: 48        # x=0 to x=48 (stops at west face of alcove_wall)
  length: 24

- label: north_wall_east
  shape: rect
  level: 1
  elevation: 40in
  thickness: 3.5in
  x: 52            # 4" clear of east face of alcove_wall (x=48+4)
  y: 168
  width: 92        # x=52 to x=144
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
  length: 120      # y=72 to y=192
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
at the wall face.  In the Hillside example the west wall runs all the way to
y=0 because the SW corner module's toe sits at (0, 0).

---

## 4. Generating the .xtc file and views

### MCP server tools

The layout config system is exposed as an MCP server with tools you can call
from Claude or any MCP-compatible client:

| Tool | What it does |
|------|-------------|
| `load_layout_config` | Validate a config and show a summary before generating |
| `generate_layout` | Write the `.xtc` file from a validated config |
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
door clearance zones, the alcove partition, and the HVAC restricted area.  The
L1-Benchwork layer (7) shows Level 1 shelf outlines.  The L2-Benchwork layer
(14) shows Level 2 shelves.  Layers 1–6 (L1-Main through L1-Service) and 8–13
(L2-Main through L2-Service) are ready for track placement.  Use the Layers
dialog to toggle each level or track type independently.

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

## 5. Grid placements

`hillside_division.yaml` has `grid: []` — no track elements placed yet.  When
you are ready to add track, each entry in the `grid:` list places a track
element type into a spreadsheet-style cell range:

```yaml
grid:
  - A1-B4=mainline
  - C3-E5=yard=single-ended, 3 tracks
  - F2=station=passing siding, 80ft
  - B6-B8@2=helix=22in radius, 2% grade, 18in rise
```

The cell range (`A1-B4`) maps onto the room using `grid_size` as the cell side
length (derived from `curve_radius` if not set explicitly — 3 ft for a 22-inch
HO minimum).  Column letters run east; row numbers run north from row 1 at the
south wall.

An `@2` suffix on the range assigns the element to Level 2.

Valid element types: `yard`, `staging`, `mainline`, `station`, `helix`,
`siding`, `module`.  Optional parameters after the second `=` are passed to the
template selector (e.g. `single-ended`, track count, minimum length in feet,
grade).

---

## Quick-start checklist

1. Copy the three Hillside files to a new folder and rename them for your layout.
2. Edit the floor plan: adjust room size, door position, any partitions or restricted areas.
3. Sketch your benchwork plan on paper, then translate it to benchwork sections in the YAML — one entry per physical shelf module.
4. Set `elevation:` and `thickness:` on every section.
5. Set `levels:` in the top-level config to match your deck count.
6. Generate the `.xtc` and open it in XTrkCAD to verify the floor plan and benchwork look right before placing any track.
7. Add grid entries to rough in the track plan.
