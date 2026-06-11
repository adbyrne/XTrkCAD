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

## Tests

```sh
uv run pytest          # 362 tests
```

## Tools

### Layout discovery and config

| Tool | Description |
|---|---|
| `list_track_plans` | List `.xtc`/`.xtce` files in a directory |
| `load_layout_config` | Load and validate a YAML layout config; returns questionnaire for missing fields |
| `list_templates` | List available track element templates, optionally filtered by category |
| `get_template_info` | Detailed metadata for one template by id |
| `generate_layout` | Generate an initial `.xtc` file from a YAML config |
| `merge_benchwork` | Replace floor/benchwork layers in an existing `.xtc`, preserving all track objects and NOTEs |

### Read / analyse

| Tool | Description |
|---|---|
| `get_layout_summary` | Title, scale, room size, track counts |
| `get_track_objects` | All track objects with endpoints and connections |
| `find_unconnected_endpoints` | Open (unconnected) endpoints |
| `get_track_lengths` | Total track length by kind and layer |
| `get_curve_stats` | Curve radius statistics (min/max/mean/distribution) |
| `get_operation_density` | Fugate OD formula — max cars and cars moved per session |
| `get_stations` | List all `STATION:` NOTE positions snapped to nearest track endpoint |
| `get_siding_capacities` | BFS track length and car capacity for every `SIDING:`/`STORAGE:` note |
| `get_station_distances` | Shortest track-graph distances between all station pairs |
| `list_labeled_segments` | All annotation notes (`STATION:`, `SIDING:`, `INDUSTRY:`, `YARD_TRACK:`, `REFERENCE:`) snapped to tracks |

### Annotation and validation

| Tool | Description |
|---|---|
| `validate_layout` | Check NOTE annotations vs stations.yaml — missing notes, far snaps, ISOLATED_MAIN_TRACK, LAYER_BRIDGE_TRACK |
| `write_validation_layer` | Write a copy of the layout with a red `Validation` layer marking every coordinate-bearing issue |
| `export_layout_data` | Export `layout_data.json` for RR_server (mileposts, siding lengths, industry spots, yard tracks, segments) |

### Reports (txt / md / html / json)

All write-report tools accept a `format` parameter: `"txt"` (default), `"md"`, `"html"`, or `"json"`.

| Tool | Description |
|---|---|
| `write_layout_report` | Full layout summary with layer lengths, curve stats, optional OD section |
| `write_operation_density_report` | Operational density by layer category |
| `write_station_distance_report` | Inter-station distances in both model and prototype units |
| `write_benchwork_report` | Benchwork section list with elevations and areas |
| `write_equipment_report` | Equipment suitability (PASS/MARGINAL/FAIL) vs. minimum curve radius |
| `write_turnout_report` | Turnout count and density per 100 ft, by layer |
| `write_gaps_report` | Open endpoints, near-miss pairs, turntable stall summary |
| `write_radius_map` | SVG colour-coded curve radius map |
| `write_plan_view` | SVG top-down track plan coloured by layer |
| `write_elevation_view` | SVG side elevation showing level heights and benchwork |

### Aisle and benchwork checks

| Tool | Description |
|---|---|
| `check_aisle_clearance` | Minimum aisle width between all facing benchwork section pairs; flags below 30″ (error) or 36″ (warning) |

### Edit

| Tool | Description |
|---|---|
| `add_track_layers` | Add standard `Ln-*` track-type layers to an existing layout |
| `rename_layers` | Rename layers by index |
| `delete_track` | Remove a single track object by ID |
| `find_dead_connections` | Find T-endpoint references to non-existent tracks |
| `fix_dead_connections` | Convert dead T-endpoints to open E-endpoints |

## Layout config YAML

```yaml
name: My Layout
scale: HO
room: 12x16           # width × depth in feet
curve_radius: 22in    # minimum mainline radius
mainline: single      # single or double
grid:
  - A1=yard=single-ended, 3 tracks, 60ft
  - B3-C4=staging=double-ended, 6 tracks, 80ft
  - E2=station=passing siding, 100ft
```

Grid entries use the DSL `[col][row]=type[=params]`. Column letters (`A`, `B`, …) map
left-to-right; row numbers map top-to-bottom. Ranges (`B3-C4`) span multiple cells.

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
the same floor plan file, varying only its `benchwork:` and `grid:` sections.

### Template categories

`yard` · `staging` · `station` · `mainline` · `helix` · `siding`

Each template carries an `od_role` used by `get_operation_density`:

| Role | Fugate formula contribution |
|---|---|
| `storage` | `max_cars += 0.8 × track_ft` |
| `staging` | `max_cars += 0.8 × ft`; `cars_moved += 0.4 × ft × 2` |
| `passing` | `max_cars += 0.8 × ft / 2`; `cars_moved += 0.4 × ft` |
| `connecting` | `cars_moved += 0.4 × ft` |

## Layout annotation conventions

Operations-aware tools read text NOTE objects placed in XTrkCAD. Place the note close to
the relevant track; the server snaps it to the nearest track endpoint.

| Prefix | Format | Purpose |
|---|---|---|
| `STATION: <id> [@ref]` | `STATION: WP @MP_ZERO` | Mainline station; optional `@<ref>` co-locates it with a REFERENCE: note for routing |
| `SIDING: <id> [description]` | `SIDING: WP Platform arrival track` | Measures BFS track capacity for a station's arrival/departure track or passing siding |
| `STORAGE: <id> [description]` | `STORAGE: WP_YARD1 North ladder` | Measures capacity of a storage/yard track; label does not need to match a station ID |
| `INDUSTRY: <id> [description]` | `INDUSTRY: TIMBER Logging spur` | Measures spur length; `id` matches a `types: [industry]` entry in stations.yaml |
| `YARD_TRACK: <yard_id> <label>` | `YARD_TRACK: WP LEAD1` | Measures an individual yard track; works for yards and mine sidings |
| `REFERENCE: MP_ZERO` | `REFERENCE: MP_ZERO` | Milepost origin; required for milepost calculations |
| `REFERENCE: CO_MP_ZERO` | `REFERENCE: CO_MP_ZERO` | Milepost origin for a foreign railroad (e.g. C&O) |

**ID vs. description:** The first whitespace-delimited token after the colon is the ID (matched
against stations.yaml).  Any remaining text is a human-readable description that appears in
reports and the JSON export — it does not affect matching.

**`@ref` tag on STATION: notes:** If the station NOTE is placed on an isolated yard track that
is not on the main graph (e.g. WP yard), add `@MP_ZERO` (or `@<any-REFERENCE-name>`) to the
note text.  The server substitutes the reference point's graph endpoint, so distances and
mileposts can be computed as if the station were at that position.
Example: `STATION: WP @MP_ZERO` — WP is assigned milepost 0 and is reachable from all other
stations via the main graph.

**Foreign railroad layers:** Name layers `CO-Main`, `CO-Passing`, `CO-Storage`, `CO-Staging`
(or any `<rr>-<type>` name not in the standard `Ln-*` scheme) to exclude them from NYE
operation density calculations.  The suffix must match an entry in `_SUFFIX_TO_CATEGORY`
or the layer will default to `"mainline"`.

### stations.yaml reference

```yaml
layout: my_layout_name
mp_scale: 1.0        # prototype feet per milepost unit

stations:
  - id: WP
    name: Williamsport Station
    sequence: 0
    types: [station, yard]
    switchback: false

  - id: TIMBER
    name: Timber Ltd
    sequence: 8
    types: [industry]
    switchback: false
    within_limits_of: JC   # parent station for branch milepost
    spots: 2               # car spots (operational); null = derive from spur length
```

`spots` is the number of simultaneous car positions at the industry (waybill/forwarding
use).  It is independent of `spur_length_cars` (physical track capacity).

## Module structure

```
src/xtrkcad_mcp/
  server.py      — MCP tool definitions (FastMCP)
  parser.py      — .xtc / .xtce file parser
  models.py      — Layout, Track, Endpoint dataclasses + scale ratios
  config.py      — YAML layout config loader + grid DSL parser
                   FloorPlan / FloorRoom / FloorDoor / FloorPartition / FloorRestricted
  generator.py   — .xtc file generator from LayoutConfig; merge_benchwork; write_validation_layer helpers
  stations.py    — Annotation parsing (STATION/SIDING/STORAGE/INDUSTRY/YARD_TRACK/REFERENCE),
                   capacity BFS, station distances, validation checks, layout_data.json export
  templates.py   — Template library loader
  svg_views.py   — Plan and elevation SVG renderers
  templates/     — 12 stub .xtc files + index.yaml catalog
```
