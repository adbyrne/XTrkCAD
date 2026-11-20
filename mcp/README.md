# xtrkcad-mcp

MCP server for reading, analysing, and generating [XTrkCAD](https://xtrkcad-fork.sourceforge.io/) model railroad layout files (`.xtc` / `.xtce`).

## Requirements

- Python ≥ 3.11
- [uv](https://docs.astral.sh/uv/) (recommended) or pip

## Install

```sh
uv sync            # installs dependencies from uv.lock
uv sync --extra dev   # also installs pytest
```

## Running

```sh
uv run xtrkcad-mcp          # stdio transport (for Claude Desktop / claude-code)
```

`xtrkcad-report` writes the layout report from the command line, optionally for
some layers or Layer Groups only:

```sh
uv run xtrkcad-report layout.xtc -f html -o report.html
uv run xtrkcad-report layout.xtc -g NYE -f md      # one Layer Group; repeat -g / -l for more
```

Configure in `~/.claude/settings.json` (Claude Code) or `claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "xtrkcad": {
      "command": "uv",
      "args": ["--directory", "/path/to/xtrkcad-git/mcp", "run", "xtrkcad-mcp"]
    }
  }
}
```

`generate_track_plan` reads turnouts from XTrkCAD's parameter files (`.xtp`). Run from the
source tree it finds them in `app/lib/params`; an installed package looks in
`$XTRKCAD_PARAMS_DIR`, then in an XTrkCAD install (`~/.local/share/xtrkcad*/params`,
`/usr/local/share/xtrkcad*/params`, `/usr/share/xtrkcad*/params`).

## Tests

```sh
uv run --extra dev pytest
```

## CI

`.github/workflows/ci-mcp.yml` runs the tests on Linux (Ubuntu 24.04 and 26.04), macOS and
Windows on pushes to `test/**` and PRs into `main` that change `mcp/` or one of the two MCP
workflows (on `main`, `release-mcp.yml` runs it). Its `tutorial-pdf` job builds the Hillside
tutorial as one PDF with `docs/build_tutorial_pdf.py` (pandoc and weasyprint): Part 1, Part 2
and the conventions spec as an appendix, with the screenshots. The PDF is an artifact, never
committed; the Markdown stays the source. It's separate from the C application's CI, and the other way round: the
app's workflows (`ci-gtk3.yml`, `release.yml`, ...) only run for the app's paths, so a change
only under `mcp/` runs just this one. Python is pinned to 3.14: unpinned, `uv` picked macOS's
3.15, which `rpds-py`'s PyO3 build can't target yet.

## Release

`.github/workflows/release-mcp.yml` publishes the package. Bump `version` in
`pyproject.toml`, then tag:

```sh
git tag mcp-v0.1.0 && git push origin mcp-v0.1.0
```

It runs the tests and builds the PDF (it calls `ci-mcp.yml`), checks the tag matches
`pyproject.toml`, builds the sdist and wheel with `uv build`, and drafts a GitHub Release named
"XTrkCAD MCP server <version>" with the wheel, the sdist and the tutorial PDF attached; publish
the draft by hand after a look.

Each merge into `main` that changes `mcp/` also replaces the rolling **`latest-mcp`**
prerelease (`releases/tag/latest-mcp`) with the same three files, built from that commit. Its
package version is `<version>.dev<run number>`, which sorts before the release. It's never
marked "Latest", so the app's releases keep that place. A trial tag,
`mcp-v0.1.0-test1`, does the whole run and drafts a prerelease; delete the draft and the tag
afterwards. A dispatch (`gh workflow run release-mcp.yml`) runs the tests and builds the
package and PDF as artifacts without releasing. The app's `release.yml` never publishes the MCP, and its `v*.*.*` tags
don't start this workflow.

`.github/dependabot.yml` has Dependabot open one grouped pull request a week that updates
`uv.lock`. It skips new major versions of the `mcp` SDK, since moving to one changes the
server's code, not just the lock file.

The MCP server isn't part of XTrkCAD upstream (SourceForge Hg), so this directory, its
workflows and the Dependabot config exist only in the GitHub repository.

## Layout files

The server reads `.xtc`/`.xtce` files of any XTrkCAD version, including Layer
Groups (`LAYERGROUP` records, file `VERSION 13`) and JSON Notes.  Layouts it
generates use XTrkCAD's current format, `VERSION 13`, and so need XTrkCAD 5.4
or later.

## Tools

### Layout discovery and config

| Tool | Description |
|---|---|
| `list_track_plans` | List `.xtc`/`.xtce` files in a directory |
| `load_layout_config` | Load and validate a YAML layout config; returns questionnaire for missing fields |
| `generate_layout` | Generate an initial `.xtc` file from a YAML config, with standard layers and Layer Groups |
| `generate_track_plan` | Lay a track plan (`track_plan_file`) along the benchwork shelves of that `.xtc`: main, corner curves, stub yard, passing sidings or a terminus runaround, spurs, with turnouts from XTrkCAD's parameter files; says what doesn't fit and by how much |
| `merge_benchwork` | Replace floor/benchwork layers in an existing `.xtc`, preserving all track objects and NOTEs |
| `add_track_layers` | Add the standard `L{n}-*` track-type layers to an existing `.xtc` |
| `rename_layers` | Rename layers by name, e.g. to standard names op-density recognizes |

### Views

| Tool | Description |
|---|---|
| `write_plan_view` | Top-down SVG of the benchwork, one deck or all, optionally with track from an `.xtc` |
| `write_elevation_view` | Wall-profile SVG of benchwork heights |
| `write_benchwork_report` | Table of every benchwork section with area and bounding box |

### Read / analyse

| Tool | Description |
|---|---|
| `get_layout_summary` | Title, scale, room size, track counts, layers, Layer Groups |
| `get_layer_groups` | Layer Groups and the layers in each |
| `get_track_objects` | All track objects with endpoints and connections |
| `find_unconnected_endpoints` | Open (unconnected) endpoints |
| `get_track_lengths` | Total track length by kind and layer |
| `get_curve_stats` | Curve radius statistics (min/max/mean/distribution) |
| `get_operation_density` | Fugate OD formula — max cars and cars moved per session |
| `check_conventions` | Checks the layout against the Operating Data Conventions (v1): JSON Notes, Layer Groups, note placement, mileposts. Lists errors, warnings and info, each with a stable rule id |

### Reports (txt / md / html / json)

All write-report tools accept a `format` parameter: `"txt"` (default), `"md"`, `"html"`, or `"json"`.

| Tool | Description |
|---|---|
| `write_layout_report` | Full layout summary with layer lengths, curve stats, optional OD section |
| `write_operation_density_report` | Operational density by layer category |
| `write_equipment_report` | Equipment suitability (PASS/MARGINAL/FAIL) vs. minimum curve radius |
| `write_turnout_report` | Turnout count and density per 100 ft, by layer |
| `write_gaps_report` | Open endpoints, near-miss pairs, turntable stall summary (a sectional turntable drawn as a turnout counts as a turntable). XTrkCAD's own Reports menu (Gaps with a maximum gap, Short Tracks, Track Clearance) is the fuller track check |
| `write_radius_map` | SVG colour-coded curve radius map |
| `write_station_distance_report` | Station-to-station distances. With convention notes: per railroad and subdivision, from each station's MP and the subdivision's `mp_scale` (a joint station once on each); otherwise track-graph distances between `STATION:` notes |

### Limiting a tool to part of the layout

The read, analyse, report and annotation tools take two optional arguments:
`layers` (layer names or index numbers) and `groups` (Layer Group names).  With
either, only tracks and notes on the selected layers are used, so a file holding
two railroads can be reported on one railroad at a time:

```
write_layout_report("layout.xtc", "nye.md", format="md", groups=["NYE"])
```

`get_operation_density` classifies layers by explicit `layer_categories` first,
then by Layer Groups named after a category (`Mainline`, `Passing`, `Storage`,
`Staging`, `Connecting`, `Service`), then by the layer-name suffix
(`L1-Staging`).

Storage, in Op Density, is how many cars can be parked on the layout while trains
still run on the main and passing tracks. With convention notes on the layout
(Operating Data Conventions v1.1), the result and the report add two views to
Fugate's by-layer one:

- **Industries and yards**: each industry's and team track's `car_spots` against its
  track capacity (the unserved remainder), yard tracks and `storage` tracks.
  Industries on main or passing track are listed apart and stay out of the totals.
- **Railroad service**: each `facility` note's `car_spots`. Only spots a note gives
  count, and they count as storage in `max_cars` unless the facility's track is
  already counted by length (a Storage, Staging or Passing layer). A facility
  without `car_spots` is listed as not counted.

### Annotation and export

These tools measure track capacity and compute station mileposts from notes placed
on the layout.  Annotations are XTrkCAD JSON Notes with a `kind` and an `id`:

| JSON Note | Purpose |
|---|---|
| `{"kind": "station", "id": "WP"}` | Station; optional `terminus`, `switchback` (true/false) and `ref_tag` |
| `{"kind": "reference", "id": "MP_ZERO"}` | Named reference point; `MP_ZERO` is the milepost origin, optional `mp_scale` |
| `{"kind": "industry", "id": "KIEL", "name": "Kiel Co", "station": "WP"}` | Industry spur; `name` and `station` optional |
| `{"kind": "storage", "id": "Car Barn"}` | Standalone freight spur (not a station house track) |
| `{"kind": "house_track", "id": "WP"}` | Station freight-house / team track; coexists with passing siding |
| `{"kind": "yard_track", "id": "WP", "label": "Track 2"}` | One yard track's capacity |

Text notes with the older prefixes (`STATION: WP !TERM`, `INDUSTRY: KIEL Kiel Co @WP`,
`YARD_TRACK: WP Track 2`, ...) are read the same way.  Passing siding capacity is
inferred automatically from `L{n}-Passing` layers — no note is required on the
passing track itself.  The [Operating Data Conventions](docs/operating-conventions.md) describe every
kind and field; the [tutorial](docs/operating-conventions-tutorial.md) adds them to the Hillside
layout step by step.

| Tool | Description |
|---|---|
| `get_stations` | The stations: with convention notes, each station note and its MP; with older `STATION:` text notes, the track each snaps to |
| `get_siding_capacities` | Usable track length and car capacity for all annotated segments |
| `get_station_distances` | Distances between stations: along the main between their MPs with convention notes (the export's figures), else the track path between `STATION:` notes |
| `list_labeled_segments` | Audit the older text annotations before `validate_layout` or the v1 export |
| `validate_layout` | The older check, for `STATION:`/`REFERENCE:` text notes and the v1 export; with convention notes use `check_conventions` |
| `export_layout_data` | Write `layout_data.json` v2 (schema: [docs/layout_data_v2.md](docs/layout_data_v2.md)); refuses while `check_conventions` finds errors unless `force=True`; `format="v1"` writes the previous format |
| `make_timetable` | A simple timetable from the layout's stations, MPs, yard limits and car spots plus an operations file (fast clock, speeds, stop times, trains; tutorial chapter 13), with the model speed it implies |

### Edit

Close the layout in XTrkCAD before editing it with a tool, or reload it after: XTrkCAD doesn't lock
files, so whichever saves last wins. Every editing tool copies the layout to `<name>_vN.xtc` first.

| Tool | Description |
|---|---|
| `delete_track` | Remove a single track object by ID (backs the layout up first) |
| `find_dead_connections` | Find T-endpoint references to non-existent tracks |
| `fix_dead_connections` | Convert dead T-endpoints to open E-endpoints (backs the layout up first) |

## Layout config YAML

```yaml
name: My Layout
scale: HO
room: 12x16           # width × depth in feet
curve_radius: 20in    # minimum mainline radius
mainline: single      # single or double
benchwork_file: my_benchwork.yaml
track_plan_file: my_track_plan.yaml   # for generate_track_plan
```

`generate_layout` writes the room, benchwork and layers; `generate_track_plan` then lays
the track plan along the benchwork shelves: the route of the main, a stub yard, stations
with passing sidings or a terminus runaround, and spurs. The format, with the Hillside
example, is in [`docs/layout-config-guide.md`](docs/layout-config-guide.md), section 5.

### Floor plan

For rooms that aren't a simple rectangle, replace `room:` with a `floor_plan` description.
The room bounding box is derived automatically from the union of all named rooms.

```yaml
floor_plan_file: my_room.yaml   # load from a standalone file (share across layouts)
# — OR —
floor_plan:                     # inline in the same config
  wall_thickness: 4in
  rooms:
    - name: main
      x: 0ft
      y: 0ft
      width: 16ft
      depth: 10ft
    - name: alcove
      x: 0ft
      y: 10ft
      width: 10ft
      depth: 4ft
  doors:
    - room: main
      wall: south
      from: 3ft
      width: 32in
      swing: inward
  partitions:
    - label: divider
      x0: 0ft
      y0: 10ft
      x1: 10ft
      y1: 10ft
      thickness: 4in
  restricted:
    - label: HVAC
      x: 12ft
      y: 0ft
      width: 3ft
      depth: 2ft
      reason: HVAC
```

`floor_plan_file` is resolved relative to the config file's directory.  Use it when
multiple layout experiments share the same physical room — each layout config references
the same floor plan file, varying only its benchwork and track plan.

## Module structure

```
src/xtrkcad_mcp/
  server.py      — MCP tool definitions (FastMCP)
  parser.py      — .xtc / .xtce file parser
  models.py      — Layout, Track, Endpoint dataclasses + scale ratios
  config.py      — YAML layout config loader
                   FloorPlan / FloorRoom / FloorDoor / FloorPartition / FloorRestricted
  generator.py   — .xtc file generator from LayoutConfig
  stations.py    — note annotations (JSON and text), station routing, capacities, export
  conventions.py — check_conventions: Operating Data Conventions rules
  export_v2.py   — export_layout_data v2 (layout_data.json schema 2)
  timetable.py   — make_timetable: the tutorial's simple timetable
  od_views.py    — Op Density views from the notes: industries and yards, service
  trackgeom.py   — track pieces in memory (turnouts from .xtp files), .xtc records
  track_plan.py  — generate_track_plan: the route along the shelves, yard, stations
  station_distances.py — station distance report from the milepost model
  mileposts.py   — milepost anchors, calculated mileposts, station and yard limits
  svg_views.py   — plan and elevation SVG views
  cli.py         — xtrkcad-report command-line entry point
```
