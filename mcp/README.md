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

### Reports (txt / md / html / json)

All write-report tools accept a `format` parameter: `"txt"` (default), `"md"`, `"html"`, or `"json"`.

| Tool | Description |
|---|---|
| `write_layout_report` | Full layout summary with layer lengths, curve stats, optional OD section |
| `write_operation_density_report` | Operational density by layer category |
| `write_equipment_report` | Equipment suitability (PASS/MARGINAL/FAIL) vs. minimum curve radius |
| `write_turnout_report` | Turnout count and density per 100 ft, by layer |
| `write_gaps_report` | Open endpoints, near-miss pairs, turntable stall summary |
| `write_radius_map` | SVG colour-coded curve radius map |

### Edit

| Tool | Description |
|---|---|
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

## Module structure

```
src/xtrkcad_mcp/
  server.py      — MCP tool definitions (FastMCP)
  parser.py      — .xtc / .xtce file parser
  models.py      — Layout, Track, Endpoint dataclasses + scale ratios
  config.py      — YAML layout config loader + grid DSL parser
                   FloorPlan / FloorRoom / FloorDoor / FloorPartition / FloorRestricted
  generator.py   — .xtc file generator from LayoutConfig
  templates.py   — Template library loader
  templates/     — 12 stub .xtc files + index.yaml catalog
```
