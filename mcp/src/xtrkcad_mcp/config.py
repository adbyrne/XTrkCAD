"""Layout config parser — YAML config file → validated LayoutConfig.

YAML CONFIG FORMAT
==================

Required keys
-------------
  name: "My Layout"
  scale: HO           # Z N TT HO S O G I  Nn3 HOn3 On3 Sn3
  room: 12x16         # WxD in feet — OR omit when floor_plan is present (see below)

Optional keys
-------------
  mainline: single          # single (default) | dual
  curve_radius: 22in        # model inches; 'ft' suffix also accepted
  switch_size: "#6"         # default "#6"
  levels: 1                 # multi-deck count
  level_separation: 18in    # vertical spacing between decks
  level_break: E            # grid row where upper deck begins
  grid_size: 3ft            # grid cell size; derived from curve_radius if omitted

floor_plan section
------------------
Describes the physical room in detail.  When present, the bounding box of all
rooms is used as the room size (overriding a bare 'room:' key if both appear).
All lengths accept a unit suffix: 16ft, 192in, 16 (bare numbers = inches).
Use the 'ft' suffix for room-scale dimensions to avoid confusion.

Two ways to supply the floor plan:

  floor_plan_file: path/to/room.yaml   # standalone file — share one room
                                       # across many layout configs
  floor_plan:                          # inline — embed directly in the config

Both are parsed identically.  floor_plan_file takes precedence if both appear.
The standalone file contains just the floor_plan body (no wrapping key needed).

  floor_plan:
    wall_thickness: 4in     # default interior wall thickness (studs ~3.5in finished ~4-5in)

    rooms:                  # one or more named rectangular sub-areas
      - name: main
        x: 0ft              # SW corner offset within the total space
        y: 0ft
        width: 16ft
        depth: 10ft
      - name: alcove
        x: 0ft
        y: 10ft
        width: 10ft
        depth: 4ft

    doors:                  # wall openings with optional swing clearance
      - room: main          # FloorRoom.name
        wall: south         # north | south | east | west
        from: 3ft           # offset from left/bottom end of that wall
        width: 32in         # opening width
        swing: inward       # inward | outward | both | none (default inward)
        clearance: 32in     # swing arc depth; defaults to door width when omitted

    partitions:             # partial walls, knee walls, freestanding columns
      - label: half wall
        x0: 8ft             # start point
        y0: 4ft
        x1: 11ft            # end point (axis-aligned or diagonal)
        y1: 4ft
        thickness: 4in      # defaults to floor_plan wall_thickness

    restricted:             # rectangular no-go zones (HVAC, stairs, panels, columns)
      - label: HVAC unit
        x: 12ft             # SW corner of the rectangle
        y: 0ft
        width: 3ft
        depth: 2ft
        reason: HVAC        # informational tag (optional)

benchwork section
-----------------
A YAML list of physical benchwork sections.  Each entry must have label,
level (deck number, 1-based), shape, and the shape's geometry keys.

Two ways to supply the benchwork:

  benchwork_file: path/to/benchwork.yaml   # standalone file — share across configs
  benchwork:                               # inline list

benchwork_file takes precedence when both are present.

Supported shapes:

  rect (axis-aligned rectangle):
    shape: rect
    x: 0          # SW corner, model inches
    y: 0
    width: 24
    length: 156

  rotated_rect (rectangle rotated around its centre):
    shape: rotated_rect
    cx: 60        # centre x, model inches
    cy: 96        # centre y
    width: 24
    length: 72
    rotation: 45  # degrees counter-clockwise

  polygon (arbitrary convex/concave polygon):
    shape: polygon
    vertices:
      - [0, 0]
      - [24, 0]
      - [24, 96]
      - [0, 96]

  arc (curved shelf — outer+inner ring approximated at 2°/step):
    shape: arc
    cx: 72        # arc centre, model inches
    cy: 72
    radius: 48    # centreline radius
    width: 24     # shelf depth (radial)
    start_angle: 0
    sweep_angle: 90

  spline (narrow strip following a track centreline path):
    shape: spline
    width: 12     # strip width, model inches
    points:
      - [0, 0]
      - [48, 48]
      - [96, 96]

Optional fields (apply to all shapes):
  elevation: 40in   # top surface height above floor, real inches (in/ft/bare number)
  thickness: 3.5in  # board depth below top surface, real inches (default 0)

Example inline list:

  benchwork:
    - label: west wall
      shape: rect
      level: 1
      x: 0
      y: 0
      width: 24
      length: 192
    - label: curved corner
      shape: arc
      level: 1
      cx: 144
      cy: 0
      radius: 24
      width: 18
      start_angle: 90
      sweep_angle: 90

grid section
------------
Track element placements expressed as grid-cell ranges:

  grid:
    - A3-C5=yard=single-ended, 3 tracks, 60ft min
    - F6-F8=staging=double-ended, 4 tracks
    - B1-B8=mainline
    - E4=station=passing siding, 100ft
    - G2-G4=helix=22in radius, 2% grade, 18in rise

  Valid element types: yard  staging  mainline  station  helix  siding  module

SUGGESTED PROMPTS
=================

Starting from a blank layout:

  "I need a layout YAML config. My room is [W×D] feet. Scale is [HO/N/O/…].
   My theme is [era / prototype / operational goal]. What do you need to know?"

Defining a multi-room or irregular space:

  "My layout space has multiple connected areas. The main room is [W×D] feet.
   There is also a [alcove / utility area / closet] that is [W×D] feet, located
   off the [north/south/east/west] wall starting [N] feet from the [SW/NW/SE/NE]
   corner. The wall between them is [~4 inches] thick.
   Help me write the floor_plan section of my layout YAML config."

Adding doors and swing clearances:

  "There are [N] door(s) in the layout room:
     Door 1: [wall] wall of [room name], [width]-inch opening, [N] feet from
             the [corner], swinging [inward / outward].
     Door 2: …
   Add these to the floor_plan doors list."

Adding restricted areas:

  "Some areas of the room cannot be used for the layout:
     - [Label]: roughly [W×D] feet, located [description of position].
   Add these as restricted entries in the floor_plan."

Reviewing and validating the floor plan:

  "Load my config and summarise the floor plan: total bounding box, usable area
   per room, door clearance zones, and restricted areas. Flag any overlaps or
   anything that might conflict with track placement."

Iterating on an existing plan:

  "Here is my current floor_plan YAML section: [paste]. I need to add a partial
   wall (knee wall) along the [description]. Update the partitions list."
"""

import math
import re
from dataclasses import dataclass, field
from pathlib import Path

import yaml

# Minimum curve radius in model inches, by scale
SCALE_MIN_RADIUS_IN: dict[str, float] = {
    "Z": 7.0, "N": 9.75, "TT": 12.0, "HO": 18.0,
    "S": 24.0, "O": 36.0, "G": 48.0, "I": 32.0,
    "Nn3": 9.75, "HOn3": 18.0, "On3": 36.0, "Sn3": 24.0,
}

KNOWN_SCALES = frozenset(SCALE_MIN_RADIUS_IN)

VALID_ELEMENT_TYPES = frozenset({
    "yard", "staging", "mainline", "station", "helix", "siding", "module",
})


_CELL_RE = re.compile(r'^([A-Z]+)(\d+)$')
_RANGE_RE = re.compile(r'^([A-Z]+)(\d+)-([A-Z]+)(\d+)$')


def _default_grid_cell_ft(radius_in: float) -> float:
    """Grid cell = 2 × curve diameter rounded up to nearest foot, min 3."""
    return max(3.0, math.ceil((2.0 * radius_in) / 12.0))


def _parse_cell_or_range(s: str) -> tuple[str, int, str, int]:
    """'A3' → (A,3,A,3);  'A3-C5' → (A,3,C,5)."""
    m = _RANGE_RE.match(s)
    if m:
        return m.group(1), int(m.group(2)), m.group(3), int(m.group(4))
    m = _CELL_RE.match(s)
    if m:
        col, row = m.group(1), int(m.group(2))
        return col, row, col, row
    raise ValueError(f"invalid cell/range {s!r} — expected A3 or A3-C5")


def _parse_room(s: str) -> tuple[float, float]:
    """'12x16' or '12 x 16' → (width_ft, depth_ft)."""
    s = str(s).strip().replace("×", "x").replace(" ", "")
    parts = s.lower().split("x")
    if len(parts) != 2:
        raise ValueError(f"invalid room size {s!r} — expected WxD in feet")
    try:
        return float(parts[0]), float(parts[1])
    except ValueError:
        raise ValueError(f"invalid room size {s!r} — values must be numbers")


def _parse_length_in(s: str) -> float:
    """'22in', '22\"', '18.5ft', '18.5' → model inches."""
    s = str(s).strip().lower()
    if s.endswith("in") or s.endswith('"'):
        return float(s.rstrip('in"').strip())
    if s.endswith("ft") or s.endswith("'"):
        return float(s.rstrip("ft'").strip()) * 12.0
    return float(s)


@dataclass
class GridPlacement:
    col_start: str
    row_start: int
    col_end: str
    row_end: int
    element_type: str
    params: str
    raw: str = ""
    level: int = 1  # layout level this element lives on (1, 2, …)

    @property
    def cell_range(self) -> str:
        if self.col_start == self.col_end and self.row_start == self.row_end:
            return f"{self.col_start}{self.row_start}"
        return f"{self.col_start}{self.row_start}-{self.col_end}{self.row_end}"


@dataclass
class Benchwork:
    """A physical benchwork section — free-floating polygon at a layout level."""
    label: str
    level: int                                    # 1, 2, 3… → XTrkCAD layer 2N
    vertices: list = field(default_factory=list)  # [(x, y)…] model inches, always set
    elevation_in: float = 0.0                     # top surface height above floor, real inches
    thickness_in: float = 0.0                     # board depth below top surface, real inches


@dataclass
class FloorRoom:
    """A named rectangular sub-area within the total layout space."""
    name: str
    x: float        # model inches from SW corner of total space
    y: float
    width: float    # model inches east-west
    depth: float    # model inches north-south


@dataclass
class FloorDoor:
    """A door opening in a room wall, with optional swing clearance zone."""
    room: str           # FloorRoom.name this door belongs to
    wall: str           # north / south / east / west
    from_in: float      # offset from the left/bottom end of that wall, model inches
    width_in: float     # opening width, model inches
    swing: str          # inward / outward / both / none
    clearance_in: float  # swing arc depth, model inches (defaults to width_in)


@dataclass
class FloorPartition:
    """A partial wall or column not aligned to a room perimeter edge."""
    label: str
    x0: float       # start point, model inches
    y0: float
    x1: float       # end point, model inches
    y1: float
    thickness: float  # wall thickness, model inches


@dataclass
class FloorRestricted:
    """A no-go zone within the layout space — rectangle or arbitrary polygon."""
    label: str
    x: float        # SW corner, model inches (ignored when vertices is set)
    y: float
    width: float    # model inches east-west (ignored when vertices is set)
    depth: float    # model inches north-south (ignored when vertices is set)
    reason: str     # informational tag: HVAC, stairs, column, …
    vertices: list = field(default_factory=list)  # [(x,y)…] for polygon shape


@dataclass
class FloorPlan:
    """Complete physical floor plan for the layout room."""
    wall_thickness_in: float                  # default interior wall thickness
    rooms: list[FloorRoom]
    doors: list[FloorDoor]
    partitions: list[FloorPartition]
    restricted: list[FloorRestricted]
    # Bounding box derived from the union of all rooms (set during parsing)
    total_width_in: float = 0.0
    total_depth_in: float = 0.0


@dataclass
class LayoutConfig:
    # Required
    name: str = ""
    scale: str = ""
    room_width_ft: float = 0.0
    room_depth_ft: float = 0.0

    # Optional — 0.0 / "" = use scale default
    mainline: str = "single"
    curve_radius_in: float = 0.0
    switch_size: str = "#6"
    levels: int = 1
    level_separation_in: float = 18.0
    level_break_row: str = ""
    grid_size_ft: float = 0.0

    placements: list[GridPlacement] = field(default_factory=list)
    benchwork_sections: list[Benchwork] = field(default_factory=list)
    benchwork_file: str = ""   # path to standalone benchwork YAML (relative to config)
    floor_plan: "FloorPlan | None" = None
    floor_plan_file: str = ""  # path to standalone floor plan YAML (relative to config)


@dataclass
class ConfigResult:
    config: LayoutConfig
    missing: list[tuple[str, str]]  # (field_name, question_text)
    warnings: list[str]
    summary: str
    ready: bool                     # True when no required fields are missing


def _parse_grid_entry(s: str) -> GridPlacement:
    """'A3-C5=yard=3 tracks, single-ended' → GridPlacement."""
    parts = s.split("=", 2)
    if len(parts) < 2:
        raise ValueError(f"need at least cell=type, got {s!r}")
    cell_part = parts[0].strip().upper()
    element_type = parts[1].strip().lower()
    params = parts[2].strip() if len(parts) > 2 else ""
    if element_type not in VALID_ELEMENT_TYPES:
        raise ValueError(
            f"unknown element type {element_type!r}; "
            f"valid: {', '.join(sorted(VALID_ELEMENT_TYPES))}"
        )
    # Optional @N level tag on the range: "A1-B2@2" → level 2
    level = 1
    if "@" in cell_part:
        cell_part, level_str = cell_part.split("@", 1)
        try:
            level = int(level_str)
        except ValueError:
            pass  # bad tag → default level 1
    col_s, row_s, col_e, row_e = _parse_cell_or_range(cell_part)
    return GridPlacement(col_s, row_s, col_e, row_e, element_type, params, raw=s, level=level)


def _rect_vertices(
    x: float, y: float, width: float, length: float,
) -> list[tuple[float, float]]:
    return [(x, y), (x + width, y), (x + width, y + length), (x, y + length)]


def _rotated_rect_vertices(
    cx: float, cy: float, width: float, length: float, rotation_deg: float,
) -> list[tuple[float, float]]:
    angle = math.radians(rotation_deg)
    hw, hl = width / 2.0, length / 2.0
    corners = [(-hw, -hl), (hw, -hl), (hw, hl), (-hw, hl)]
    return [
        (
            cx + dx * math.cos(angle) - dy * math.sin(angle),
            cy + dx * math.sin(angle) + dy * math.cos(angle),
        )
        for dx, dy in corners
    ]


def _arc_vertices(
    cx: float, cy: float, radius: float, width: float,
    start_angle_deg: float, sweep_angle_deg: float,
) -> list[tuple[float, float]]:
    """Approximate a curved arc shelf as a closed polygon.

    Generates points every 2° along the arc (minimum 4 steps).
    Outer ring at radius+width/2, inner ring at radius-width/2, closed at ends.
    """
    n = max(4, int(abs(sweep_angle_deg) / 2.0))
    r_outer = radius + width / 2.0
    r_inner = radius - width / 2.0
    start = math.radians(start_angle_deg)
    sweep = math.radians(sweep_angle_deg)
    outer = [
        (cx + r_outer * math.cos(start + sweep * i / n),
         cy + r_outer * math.sin(start + sweep * i / n))
        for i in range(n + 1)
    ]
    inner = [
        (cx + r_inner * math.cos(start + sweep * i / n),
         cy + r_inner * math.sin(start + sweep * i / n))
        for i in range(n + 1)
    ]
    return outer + list(reversed(inner))


def _spline_offset_polygon(
    points: list[tuple[float, float]], width: float,
) -> list[tuple[float, float]]:
    """Generate a closed offset polygon from a polyline centerline + width.

    At each vertex, averages the normals of adjacent segments to produce a
    smooth miter join.  Ends are capped flat perpendicular to the terminal
    segment.
    """
    if len(points) < 2:
        return []
    half_w = width / 2.0

    def seg_normal(a: tuple, b: tuple) -> tuple[float, float]:
        dx, dy = b[0] - a[0], b[1] - a[1]
        mag = math.hypot(dx, dy)
        if mag < 1e-9:
            return 0.0, 1.0
        return -dy / mag, dx / mag

    normals: list[tuple[float, float]] = []
    for i in range(len(points)):
        if i == 0:
            nx, ny = seg_normal(points[0], points[1])
        elif i == len(points) - 1:
            nx, ny = seg_normal(points[-2], points[-1])
        else:
            n1 = seg_normal(points[i - 1], points[i])
            n2 = seg_normal(points[i], points[i + 1])
            nx, ny = (n1[0] + n2[0]) / 2.0, (n1[1] + n2[1]) / 2.0
            mag = math.hypot(nx, ny)
            if mag > 1e-9:
                nx, ny = nx / mag, ny / mag
        normals.append((nx, ny))

    left = [(p[0] + half_w * n[0], p[1] + half_w * n[1])
            for p, n in zip(points, normals)]
    right = [(p[0] - half_w * n[0], p[1] - half_w * n[1])
             for p, n in zip(points, normals)]
    return left + list(reversed(right))


def _parse_benchwork_sections(
    raw_list: list,
    warnings: list[str],
) -> list[Benchwork]:
    """Parse a list of benchwork section entries into Benchwork dataclasses.

    Each entry must be a dict with at least label, level, and shape fields.
    Supported shapes:
      rect        — x/y/width/length (axis-aligned rectangle)
      rotated_rect — cx/cy/width/length/rotation (rotated rectangle)
      polygon     — explicit vertices list
      arc         — cx/cy/radius/width/start_angle/sweep_angle (curved shelf)
      spline      — points polyline + width (narrow track-following strip)
    """
    if not isinstance(raw_list, list):
        warnings.append("benchwork must be a YAML list of section entries; skipping")
        return []

    sections: list[Benchwork] = []
    for entry in raw_list:
        if not isinstance(entry, dict):
            warnings.append(f"skipping non-dict benchwork entry: {entry!r}")
            continue
        label = str(entry.get("label", "")).strip()
        level = int(entry.get("level", 1))
        shape = str(entry.get("shape", "rect")).strip().lower()

        try:
            if shape == "arc":
                cx = _parse_length_in(str(entry["cx"]))
                cy = _parse_length_in(str(entry["cy"]))
                radius = _parse_length_in(str(entry["radius"]))
                width = _parse_length_in(str(entry["width"]))
                start_angle = float(entry.get("start_angle", 0))
                sweep_angle = float(entry.get("sweep_angle", 90))
                verts: list[tuple[float, float]] = _arc_vertices(
                    cx, cy, radius, width, start_angle, sweep_angle,
                )
            elif shape == "spline":
                raw_pts = entry["points"]
                pts = [(float(v[0]), float(v[1])) for v in raw_pts]
                width = _parse_length_in(str(entry["width"]))
                verts = _spline_offset_polygon(pts, width)
            elif shape == "polygon" or "vertices" in entry:
                raw_verts = entry["vertices"]
                verts = [(float(v[0]), float(v[1])) for v in raw_verts]
            elif shape == "rotated_rect" or "cx" in entry or "rotation" in entry:
                cx = _parse_length_in(str(entry["cx"]))
                cy = _parse_length_in(str(entry["cy"]))
                width = _parse_length_in(str(entry["width"]))
                length = _parse_length_in(str(entry["length"]))
                rotation = float(entry.get("rotation", 0))
                verts = _rotated_rect_vertices(cx, cy, width, length, rotation)
            else:
                x = _parse_length_in(str(entry.get("x", "0in")))
                y = _parse_length_in(str(entry.get("y", "0in")))
                width = _parse_length_in(str(entry["width"]))
                length = _parse_length_in(str(entry["length"]))
                verts = _rect_vertices(x, y, width, length)
        except (KeyError, ValueError, TypeError, IndexError) as exc:
            warnings.append(f"skipping benchwork {label!r}: {exc}")
            continue

        elevation_in = 0.0
        if "elevation" in entry:
            try:
                elevation_in = _parse_length_in(str(entry["elevation"]))
            except (ValueError, TypeError):
                warnings.append(f"benchwork {label!r}: invalid elevation; using 0in")
        thickness_in = 0.0
        if "thickness" in entry:
            try:
                thickness_in = _parse_length_in(str(entry["thickness"]))
            except (ValueError, TypeError):
                warnings.append(f"benchwork {label!r}: invalid thickness; using 0in")
        sections.append(Benchwork(label=label, level=level, vertices=verts,
                                   elevation_in=elevation_in, thickness_in=thickness_in))
    return sections


_VALID_DOOR_WALLS = frozenset({"north", "south", "east", "west"})
_VALID_SWINGS = frozenset({"inward", "outward", "both", "none"})


def _parse_floor_plan(raw_fp: dict, warnings: list[str]) -> "FloorPlan | None":
    """Parse a floor_plan: mapping into a FloorPlan dataclass."""
    if not isinstance(raw_fp, dict):
        warnings.append("floor_plan must be a YAML mapping; skipping")
        return None

    default_thickness = _parse_length_in(str(raw_fp.get("wall_thickness", "4in")))

    # --- Rooms ---
    rooms: list[FloorRoom] = []
    for entry in raw_fp.get("rooms", []):
        if not isinstance(entry, dict):
            warnings.append(f"skipping non-dict room entry: {entry!r}")
            continue
        name = str(entry.get("name", "")).strip()
        if not name:
            warnings.append("skipping room with no name")
            continue
        try:
            x = _parse_length_in(str(entry.get("x", "0ft")))
            y = _parse_length_in(str(entry.get("y", "0ft")))
            width = _parse_length_in(str(entry["width"]))
            depth = _parse_length_in(str(entry["depth"]))
        except (KeyError, ValueError) as exc:
            warnings.append(f"skipping room {name!r}: {exc}")
            continue
        if width <= 0 or depth <= 0:
            warnings.append(f"skipping room {name!r}: width and depth must be > 0")
            continue
        rooms.append(FloorRoom(name=name, x=x, y=y, width=width, depth=depth))

    # --- Doors ---
    doors: list[FloorDoor] = []
    room_names = {r.name for r in rooms}
    for entry in raw_fp.get("doors", []):
        if not isinstance(entry, dict):
            warnings.append(f"skipping non-dict door entry: {entry!r}")
            continue
        room = str(entry.get("room", "")).strip()
        wall = str(entry.get("wall", "")).strip().lower()
        if room not in room_names:
            warnings.append(
                f"skipping door: room {room!r} not found in rooms list"
            )
            continue
        if wall not in _VALID_DOOR_WALLS:
            warnings.append(
                f"skipping door in {room!r}: invalid wall {wall!r}; "
                f"valid: {', '.join(sorted(_VALID_DOOR_WALLS))}"
            )
            continue
        try:
            from_in = _parse_length_in(str(entry.get("from", "0ft")))
            width_in = _parse_length_in(str(entry["width"]))
        except (KeyError, ValueError) as exc:
            warnings.append(f"skipping door in {room!r}/{wall}: {exc}")
            continue
        swing = str(entry.get("swing", "inward")).strip().lower()
        if swing not in _VALID_SWINGS:
            warnings.append(
                f"door in {room!r}/{wall}: unknown swing {swing!r}; using 'inward'"
            )
            swing = "inward"
        try:
            clearance_in = _parse_length_in(str(entry["clearance"]))
        except (KeyError, ValueError):
            clearance_in = width_in
        doors.append(FloorDoor(
            room=room, wall=wall,
            from_in=from_in, width_in=width_in,
            swing=swing, clearance_in=clearance_in,
        ))

    # --- Partitions ---
    partitions: list[FloorPartition] = []
    for entry in raw_fp.get("partitions", []):
        if not isinstance(entry, dict):
            warnings.append(f"skipping non-dict partition entry: {entry!r}")
            continue
        label = str(entry.get("label", "")).strip()
        try:
            x0 = _parse_length_in(str(entry.get("x0", "0ft")))
            y0 = _parse_length_in(str(entry.get("y0", "0ft")))
            x1 = _parse_length_in(str(entry.get("x1", "0ft")))
            y1 = _parse_length_in(str(entry.get("y1", "0ft")))
            thickness = _parse_length_in(
                str(entry.get("thickness", f"{default_thickness}in"))
            )
        except ValueError as exc:
            warnings.append(f"skipping partition {label!r}: {exc}")
            continue
        partitions.append(FloorPartition(
            label=label, x0=x0, y0=y0, x1=x1, y1=y1, thickness=thickness,
        ))

    # --- Restricted areas ---
    restricted: list[FloorRestricted] = []
    for entry in raw_fp.get("restricted", []):
        if not isinstance(entry, dict):
            warnings.append(f"skipping non-dict restricted entry: {entry!r}")
            continue
        label = str(entry.get("label", "")).strip()
        reason = str(entry.get("reason", "")).strip()
        raw_verts = entry.get("vertices", [])
        if raw_verts:
            try:
                verts = [(float(v[0]), float(v[1])) for v in raw_verts]
            except (TypeError, IndexError, ValueError) as exc:
                warnings.append(f"skipping restricted {label!r}: bad vertices: {exc}")
                continue
            restricted.append(FloorRestricted(
                label=label, x=0, y=0, width=0, depth=0, reason=reason, vertices=verts,
            ))
        else:
            try:
                x = _parse_length_in(str(entry.get("x", "0ft")))
                y = _parse_length_in(str(entry.get("y", "0ft")))
                width = _parse_length_in(str(entry["width"]))
                depth = _parse_length_in(str(entry["depth"]))
            except (KeyError, ValueError) as exc:
                warnings.append(f"skipping restricted {label!r}: {exc}")
                continue
            restricted.append(FloorRestricted(
                label=label, x=x, y=y, width=width, depth=depth, reason=reason,
            ))

    # --- Bounding box from rooms ---
    if rooms:
        max_x = max(r.x + r.width for r in rooms)
        max_y = max(r.y + r.depth for r in rooms)
    else:
        max_x = max_y = 0.0

    return FloorPlan(
        wall_thickness_in=default_thickness,
        rooms=rooms,
        doors=doors,
        partitions=partitions,
        restricted=restricted,
        total_width_in=max_x,
        total_depth_in=max_y,
    )


def load_config(path: str | Path) -> ConfigResult:
    """Parse a YAML layout config file. Always returns a ConfigResult."""
    p = Path(path).expanduser()
    warnings: list[str] = []
    config = LayoutConfig()

    if not p.exists():
        warnings.append(f"config file not found: {p}")
        return ConfigResult(
            config=config,
            missing=_required_missing(config),
            warnings=warnings,
            summary="",
            ready=False,
        )

    try:
        with open(p) as f:
            raw = yaml.safe_load(f) or {}
    except yaml.YAMLError as e:
        warnings.append(f"YAML parse error: {e}")
        return ConfigResult(
            config=config,
            missing=_required_missing(config),
            warnings=warnings,
            summary="",
            ready=False,
        )

    if not isinstance(raw, dict):
        warnings.append("config file must be a YAML mapping at the top level")
        return ConfigResult(
            config=config,
            missing=_required_missing(config),
            warnings=warnings,
            summary="",
            ready=False,
        )

    # --- Required fields ---
    if "name" in raw:
        config.name = str(raw["name"]).strip()
    if "scale" in raw:
        config.scale = str(raw["scale"]).strip()
        if config.scale not in KNOWN_SCALES:
            warnings.append(
                f"unrecognised scale {config.scale!r}; "
                f"known scales: {', '.join(sorted(KNOWN_SCALES))}"
            )
    if "room" in raw:
        try:
            config.room_width_ft, config.room_depth_ft = _parse_room(str(raw["room"]))
        except ValueError as e:
            warnings.append(str(e))

    # --- Optional fields ---
    if "mainline" in raw:
        v = str(raw["mainline"]).strip().lower()
        if v in {"single", "dual"}:
            config.mainline = v
        else:
            warnings.append(f"mainline must be 'single' or 'dual'; got {v!r}, using 'single'")

    if "curve_radius" in raw:
        try:
            config.curve_radius_in = _parse_length_in(str(raw["curve_radius"]))
        except ValueError:
            warnings.append(f"invalid curve_radius {raw['curve_radius']!r}")

    if "switch_size" in raw:
        config.switch_size = str(raw["switch_size"]).strip()

    if "levels" in raw:
        try:
            config.levels = int(raw["levels"])
        except (TypeError, ValueError):
            warnings.append(f"invalid levels {raw['levels']!r}, using 1")

    if "level_separation" in raw:
        try:
            config.level_separation_in = _parse_length_in(str(raw["level_separation"]))
        except ValueError:
            warnings.append(f"invalid level_separation {raw['level_separation']!r}")

    if "level_break" in raw:
        config.level_break_row = str(raw["level_break"]).strip().upper()

    if "grid_size" in raw:
        try:
            config.grid_size_ft = _parse_length_in(str(raw["grid_size"]))
        except ValueError:
            warnings.append(f"invalid grid_size {raw['grid_size']!r}")

    # --- Benchwork (file reference takes precedence over inline) ---
    if "benchwork_file" in raw:
        bw_path = Path(str(raw["benchwork_file"]).strip()).expanduser()
        if not bw_path.is_absolute():
            bw_path = p.parent / bw_path
        config.benchwork_file = str(bw_path)
        if "benchwork" in raw:
            warnings.append(
                "both benchwork_file and benchwork are set; benchwork_file takes precedence"
            )
        try:
            raw_bw_list = yaml.safe_load(bw_path.read_text()) or []
        except FileNotFoundError:
            warnings.append(f"benchwork_file not found: {bw_path}")
            raw_bw_list = []
        except yaml.YAMLError as _e:
            warnings.append(f"benchwork_file YAML parse error: {_e}")
            raw_bw_list = []
        config.benchwork_sections = _parse_benchwork_sections(raw_bw_list, warnings)
    elif "benchwork" in raw:
        config.benchwork_sections = _parse_benchwork_sections(raw["benchwork"], warnings)

    # --- Floor plan (file reference takes precedence over inline) ---
    if "floor_plan_file" in raw:
        fp_path = Path(str(raw["floor_plan_file"]).strip()).expanduser()
        if not fp_path.is_absolute():
            fp_path = p.parent / fp_path
        config.floor_plan_file = str(fp_path)
        if "floor_plan" in raw:
            warnings.append(
                "both floor_plan_file and floor_plan are set; "
                "floor_plan_file takes precedence"
            )
        try:
            with open(fp_path) as _f:
                fp_raw = yaml.safe_load(_f) or {}
        except FileNotFoundError:
            warnings.append(f"floor_plan_file not found: {fp_path}")
            fp_raw = None
        except yaml.YAMLError as _e:
            warnings.append(f"floor_plan_file YAML parse error: {_e}")
            fp_raw = None
        if isinstance(fp_raw, dict):
            config.floor_plan = _parse_floor_plan(fp_raw, warnings)
    elif "floor_plan" in raw:
        config.floor_plan = _parse_floor_plan(raw["floor_plan"], warnings)

    if config.floor_plan and config.floor_plan.total_width_in > 0:
        # Derive room dimensions from the bounding box when not set by 'room:'
        if config.room_width_ft <= 0:
            config.room_width_ft = config.floor_plan.total_width_in / 12.0
        if config.room_depth_ft <= 0:
            config.room_depth_ft = config.floor_plan.total_depth_in / 12.0


    # --- Grid placements ---
    for entry in raw.get("grid", []):
        try:
            config.placements.append(_parse_grid_entry(str(entry).strip()))
        except ValueError as e:
            warnings.append(f"skipping grid entry {entry!r}: {e}")

    # --- Fill scale-dependent defaults ---
    if config.scale in SCALE_MIN_RADIUS_IN:
        if config.curve_radius_in <= 0.0:
            config.curve_radius_in = SCALE_MIN_RADIUS_IN[config.scale]
        if config.grid_size_ft <= 0.0:
            config.grid_size_ft = _default_grid_cell_ft(config.curve_radius_in)

    missing = _required_missing(config)
    ready = len(missing) == 0
    summary = _build_summary(config) if ready else ""
    return ConfigResult(
        config=config, missing=missing, warnings=warnings,
        summary=summary, ready=ready,
    )


def _required_missing(config: LayoutConfig) -> list[tuple[str, str]]:
    missing = []
    if not config.name:
        missing.append(("name", "Layout name?"))
    if not config.scale:
        missing.append(("scale", "Scale? (HO, N, O, S, Z, G, ...)"))
    has_room_dims = config.room_width_ft > 0 and config.room_depth_ft > 0
    has_floor_plan = (
        config.floor_plan is not None
        and config.floor_plan.rooms
        and config.floor_plan.total_width_in > 0
    )
    if not has_room_dims and not has_floor_plan:
        missing.append(("room", "Room size in feet? (e.g. 12x16) or add a floor_plan section"))
    return missing


def _build_summary(config: LayoutConfig) -> str:
    lines = [
        f"Layout:  {config.name}",
        f"Scale:   {config.scale}  |  Room: {config.room_width_ft:.0f}×{config.room_depth_ft:.0f} ft",
        f"Main:    {config.mainline}  |  Levels: {config.levels}"
        + (f"  (separation: {config.level_separation_in:.0f} in)" if config.levels > 1 else ""),
        f"Radius:  {config.curve_radius_in:.1f} in  |  Switch: {config.switch_size}"
        f"  |  Grid cell: {config.grid_size_ft:.0f} ft",
    ]
    if config.floor_plan:
        fp = config.floor_plan
        lines.append(
            f"Floor plan: {len(fp.rooms)} room(s), "
            f"bounding box {fp.total_width_in/12:.1f}×{fp.total_depth_in/12:.1f} ft"
            + (f", wall thickness {fp.wall_thickness_in:.1f} in" if fp.wall_thickness_in else "")
        )
        for r in fp.rooms:
            area = (r.width / 12) * (r.depth / 12)
            lines.append(
                f"  Room '{r.name}': {r.width/12:.1f}×{r.depth/12:.1f} ft "
                f"({area:.0f} sq ft) at ({r.x/12:.1f}′, {r.y/12:.1f}′)"
            )
        if fp.doors:
            door_strs = [
                f"{d.room}/{d.wall} {d.width_in:.0f}in ({d.swing})"
                for d in fp.doors
            ]
            lines.append(f"  Doors ({len(fp.doors)}): {', '.join(door_strs)}")
        if fp.partitions:
            lines.append(
                f"  Partitions: {', '.join(p.label for p in fp.partitions)}"
            )
        if fp.restricted:
            restr_strs = [
                f"{r.label} {'polygon' if r.vertices else f'{r.width/12:.1f}×{r.depth/12:.1f}ft'}"
                + (f" [{r.reason}]" if r.reason else "")
                for r in fp.restricted
            ]
            lines.append(f"  Restricted ({len(fp.restricted)}): {', '.join(restr_strs)}")
    if config.benchwork_sections:
        by_level: dict[int, list[str]] = {}
        for bw in config.benchwork_sections:
            by_level.setdefault(bw.level, []).append(bw.label)
        for lv in sorted(by_level):
            labels = ", ".join(by_level[lv])
            lines.append(f"  Benchwork L{lv} ({len(by_level[lv])} sections): {labels}")
    if config.placements:
        lines.append(f"Elements ({len(config.placements)}):")
        for pl in config.placements:
            param_str = f"  [{pl.params}]" if pl.params else ""
            lines.append(f"  {pl.cell_range} → {pl.element_type}{param_str}")
    return "\n".join(lines)
