"""Layout generator — produce an initial .xtc from a validated LayoutConfig."""

import datetime
import shutil
from dataclasses import dataclass, field
from pathlib import Path

import math

from xtrkcad_mcp.config import (
    Benchwork, BenchworkWall, FloorPartition, FloorPlan,
    FloorRestricted, GridPlacement, LayoutConfig,
)
from xtrkcad_mcp.models import SCALE_RATIOS
from xtrkcad_mcp.parser import parse_file
from xtrkcad_mcp.templates import TemplateInfo, TemplateLibrary, load_library

HO_RATIO = SCALE_RATIOS["HO"]  # 87.1


@dataclass
class PlacedElement:
    template_id: str
    grid_range: str
    x: float               # top-left x in layout inches
    y: float               # top-left y in layout inches (XTrkCAD +Y = up)
    track_ids: list[int] = field(default_factory=list)


@dataclass
class GenerationResult:
    output_path: Path | None
    backup_path: Path | None       # set if an existing file was backed up
    placed: list[PlacedElement]
    skipped: list[str]             # grid entries skipped, with reason
    warnings: list[str]


# ---------------------------------------------------------------------------
# Coordinate helpers
# ---------------------------------------------------------------------------

def _scale_factor(scale: str) -> float:
    """Multiply HO template coords by this to get target-scale layout inches."""
    return HO_RATIO / SCALE_RATIOS.get(scale, HO_RATIO)


def _col_to_x_in(col: str, cell_in: float) -> float:
    """'A' → 0, 'B' → cell_in, 'C' → 2*cell_in, 'AA' → 26*cell_in."""
    idx = 0
    for c in col.upper():
        idx = idx * 26 + (ord(c) - ord("A") + 1)
    return (idx - 1) * cell_in


def _range_origin(pl: GridPlacement, cell_in: float, room_h_in: float) -> tuple[float, float]:
    """Top-left corner of the grid range in XTrkCAD inches.

    Spreadsheet-style grid: row 1 = top of room, Y increases upward in XTrkCAD.
    Top of row_start = room_h_in - (row_start - 1) * cell_in.
    """
    x = _col_to_x_in(pl.col_start, cell_in)
    y = room_h_in - (pl.row_start - 1) * cell_in
    return x, y


def _in_bounds(pl: GridPlacement, cell_in: float, room_w_in: float, room_h_in: float) -> bool:
    x_end = _col_to_x_in(pl.col_end, cell_in) + cell_in
    y_bottom = room_h_in - pl.row_end * cell_in
    return x_end <= room_w_in + 0.5 and y_bottom >= -0.5


# ---------------------------------------------------------------------------
# Template resolution
# ---------------------------------------------------------------------------

def _find_template_id(pl: GridPlacement, lib: TemplateLibrary) -> str | None:
    """Map a grid placement to a template id.

    Tries to match a sub-type keyword from params (e.g. 'single-ended') to a
    template in the category; falls back to the first template in the category.
    Normalises hyphens ↔ spaces for matching.
    """
    candidates = lib.by_category(pl.element_type)
    if not candidates:
        return None
    params_norm = pl.params.lower().replace("-", " ")
    for t in candidates:
        subtype = t.id.split("/", 1)[1].replace("-", " ")
        if subtype in params_norm:
            return t.id
    return candidates[0].id


# ---------------------------------------------------------------------------
# File versioning
# ---------------------------------------------------------------------------

def _version_backup(path: Path) -> Path | None:
    """Copy path → path_vN.xtc (N increments) before overwriting. Returns copy path."""
    if not path.exists():
        return None
    n = 1
    while True:
        candidate = path.parent / f"{path.stem}_v{n}{path.suffix}"
        if not candidate.exists():
            shutil.copy2(path, candidate)
            return candidate
        n += 1


# ---------------------------------------------------------------------------
# .xtc line builders
# ---------------------------------------------------------------------------

def _track_header(kind: str, track_id: int, layer: int, scale: str, extra: dict) -> str:
    if kind == "CURVE" and "cx" in extra and "cy" in extra and "radius" in extra:
        return (
            f"CURVE {track_id} {layer} 0 0 0 {scale} 0 "
            f"{extra['cx']:.6f} {extra['cy']:.6f} 0 {extra['radius']:.6f} 0"
        )
    # STRAIGHT, JOINT, and stubs
    return f"{kind} {track_id} {layer} 0 0 0 {scale} 0"


def _endpoint_line(x: float, y: float, angle: float) -> str:
    return f"\tE {x:.6f} {y:.6f} {angle:.6f}"


# ---------------------------------------------------------------------------
# Benchwork helpers
# ---------------------------------------------------------------------------

# RGB colors per layer index (0=Floor, 1=Main, 2+=Benchwork levels).
# Stored as XTrkCAD long integer (R*65536 + G*256 + B).
_LAYER_COLORS = [
    8421504,   # 0 Floor: gray  (128,128,128)
    0,         # 1 Main: black
    32768,     # 2 L1-Benchwork: dark green
    255,       # 3 L2-Benchwork: blue
]

def _layer_line(lyr_id: int, name: str, current: bool) -> str:
    color = _LAYER_COLORS[lyr_id] if lyr_id < len(_LAYER_COLORS) else 0
    cur_flag = 1 if current else 0
    return (
        f'LAYERS {lyr_id} 1 0 1 {color} 0 0 0 0 "{name}" '
        f'1 0 0.000000 0.000000 0.000000 0.000000 0.000000'
    )


def _layers_header(num_levels: int) -> list[str]:
    """Emit LAYERS lines: Floor (0), Main (1), then one benchwork layer per level."""
    result = [
        _layer_line(0, "Floor", False),
        _layer_line(1, "Main", True),
    ]
    for lv in range(1, num_levels + 1):
        result.append(_layer_line(lv + 1, f"L{lv}-Benchwork", False))
    result.append("LAYERS CURRENT 1")
    return result


def _shelf_draw(
    x0: float, y0: float, x1: float, y1: float,
    layer: int, track_id: int,
) -> tuple[list[str], int]:
    """Emit one DRAW object with 4 SEG_STRLIN segments forming a shelf rectangle.

    Returns (lines, next_track_id).  One ID consumed per shelf.
    """
    color = _LAYER_COLORS[layer] if layer < len(_LAYER_COLORS) else 0
    corners = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    edges = [
        (corners[0], corners[1]),
        (corners[1], corners[2]),
        (corners[2], corners[3]),
        (corners[3], corners[0]),
    ]
    lines = [f"DRAW {track_id} {layer} 0 0 0 0.000000 0.000000 0.000000 0.000000"]
    for (ax, ay), (bx, by) in edges:
        lines.append(
            f"\tL3 {color} 0.000000 {ax:.6f} {ay:.6f} 0 {bx:.6f} {by:.6f} 0"
        )
    lines.append("END")
    return lines, track_id + 1


def _room_tableedges(room_w: float, room_h: float, track_id: int) -> tuple[list[str], int]:
    """Emit 4 TABLEEDGE lines forming the room perimeter on layer 0 (Floor)."""
    edges = [
        (0.0, 0.0, 0.0, room_h),        # west
        (0.0, room_h, room_w, room_h),   # north
        (room_w, room_h, room_w, 0.0),   # east
        (room_w, 0.0, 0.0, 0.0),         # south
    ]
    lines: list[str] = []
    tid = track_id
    for x0, y0, x1, y1 in edges:
        # VERSION >= 9 format: dL000pfpf — each position needs an elevation float.
        lines.append(
            f"TABLEEDGE {tid} 0 0 0 0 {x0:.6f} {y0:.6f} 0.000000 {x1:.6f} {y1:.6f} 0.000000"
        )
        tid += 1
    return lines, tid


_PARTITION_COLOR = 8421504     # medium gray (128,128,128) — walls and partitions
_RESTRICTION_COLOR = 12632256  # light gray  (192,192,192) — access/clearance zones


def _filpoly_draw(
    vertices: list[tuple[float, float]],
    layer: int, color: int, track_id: int,
) -> tuple[list[str], int]:
    """Emit one DRAW+SEG_FILPOLY for an arbitrary polygon.  One ID consumed."""
    lines = [
        f"DRAW {track_id} {layer} 0 0 0 0.000000 0.000000 0.000000 0.000000",
        f"\tF4 {color} 0.000000 {len(vertices)} 0 ",
    ]
    for x, y in vertices:
        lines.append(f"\t\t{x:.6f} {y:.6f} 0")
    lines.append("END")
    return lines, track_id + 1


def _obstruction_draw(
    obs: "BenchworkObstruction", track_id: int,
) -> tuple[list[str], int]:
    """Emit one DRAW+FILPOLY for a rect or triangle hard obstruction on layer 0."""
    if obs.kind == "rect":
        x0, y0 = obs.x, obs.y
        x1, y1 = obs.x + obs.width, obs.y + obs.height
        vertices = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    elif obs.kind == "triangle":
        vertices = [(v[0], v[1]) for v in obs.vertices]
    else:
        return [], track_id
    return _filpoly_draw(vertices, 0, _PARTITION_COLOR, track_id)


def _wall_rect(
    wall: BenchworkWall, room_w_in: float, room_h_in: float,
) -> tuple[float, float, float, float]:
    """Return (x0, y0, x1, y1) for the shelf rectangle of a wall."""
    if wall.side == "interior":
        return wall.x0, wall.y0, wall.x1, wall.y1
    d = wall.depth_in
    s = wall.side
    if s == "west":
        return 0.0, wall.from_in, d, wall.to_in
    if s == "south":
        return wall.from_in, 0.0, wall.to_in, d
    if s == "east":
        return room_w_in - d, wall.from_in, room_w_in, wall.to_in
    # north
    return wall.from_in, room_h_in - d, wall.to_in, room_h_in


def _write_benchwork(
    bw: Benchwork,
    room_w_in: float, room_h_in: float,
    num_levels: int,
    lines: list[str],
    track_id: int,
) -> int:
    """Append floor-plan fills (layer 0) and shelf outlines (layers 2+)."""
    # Hard obstructions as filled polygons on layer 0 (physical no-go areas)
    for obs in bw.obstructions:
        new_lines, track_id = _obstruction_draw(obs, track_id)
        lines += new_lines

    # Shelf outlines on benchwork layers (2, 3, …) per level
    for lv in range(1, num_levels + 1):
        for wall in bw.walls:
            if lv not in wall.levels:
                continue
            x0, y0, x1, y1 = _wall_rect(wall, room_w_in, room_h_in)
            new_lines, track_id = _shelf_draw(x0, y0, x1, y1, lv + 1, track_id)
            lines += new_lines

    return track_id


# ---------------------------------------------------------------------------
# Floor plan rendering helpers
# ---------------------------------------------------------------------------

def _floor_plan_restricted_draw(
    restricted: FloorRestricted, track_id: int,
) -> tuple[list[str], int]:
    """Emit light-gray polygon for a restricted zone on layer 0."""
    if restricted.vertices:
        vertices = [(float(x), float(y)) for x, y in restricted.vertices]
    else:
        x0, y0 = restricted.x, restricted.y
        x1, y1 = restricted.x + restricted.width, restricted.y + restricted.depth
        vertices = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    return _filpoly_draw(vertices, 0, _RESTRICTION_COLOR, track_id)


def _partition_door_openings(
    partition: FloorPartition,
    doors: list,
    rooms_by_name: dict,
) -> list[tuple[float, float]]:
    """Return sorted (start, end) gaps along the partition axis where doors cut through."""
    is_vertical = abs(partition.y1 - partition.y0) >= abs(partition.x1 - partition.x0)
    p_x = min(partition.x0, partition.x1)
    p_y = min(partition.y0, partition.y1)
    openings = []

    for door in doors:
        room = rooms_by_name.get(door.room)
        if room is None:
            continue
        if is_vertical:
            if door.wall not in ("east", "west"):
                continue
            wall_x = (room.x + room.width) if door.wall == "east" else room.x
            if abs(wall_x - p_x) > partition.thickness:
                continue
            gap_start = room.y + door.from_in
            gap_end = gap_start + door.width_in
            p_start, p_end = min(partition.y0, partition.y1), max(partition.y0, partition.y1)
        else:
            if door.wall not in ("north", "south"):
                continue
            wall_y = (room.y + room.depth) if door.wall == "north" else room.y
            if abs(wall_y - p_y) > partition.thickness:
                continue
            gap_start = room.x + door.from_in
            gap_end = gap_start + door.width_in
            p_start, p_end = min(partition.x0, partition.x1), max(partition.x0, partition.x1)

        clipped = (max(gap_start, p_start), min(gap_end, p_end))
        if clipped[0] < clipped[1]:
            openings.append(clipped)

    return sorted(openings)


def _floor_plan_partition_draw(
    partition: FloorPartition,
    openings: list[tuple[float, float]],
    track_id: int,
) -> tuple[list[str], int]:
    """Emit filled medium-gray polygons for an interior partition, skipping door openings."""
    t = partition.thickness
    is_vertical = abs(partition.y1 - partition.y0) >= abs(partition.x1 - partition.x0)
    all_lines: list[str] = []

    if is_vertical:
        p_x = min(partition.x0, partition.x1)
        p_start = min(partition.y0, partition.y1)
        p_end = max(partition.y0, partition.y1)
        cursor = p_start
        for gap_start, gap_end in sorted(openings):
            if cursor < gap_start:
                verts = [(p_x, cursor), (p_x + t, cursor), (p_x + t, gap_start), (p_x, gap_start)]
                new_lines, track_id = _filpoly_draw(verts, 0, _PARTITION_COLOR, track_id)
                all_lines += new_lines
            cursor = max(cursor, gap_end)
        if cursor < p_end:
            verts = [(p_x, cursor), (p_x + t, cursor), (p_x + t, p_end), (p_x, p_end)]
            new_lines, track_id = _filpoly_draw(verts, 0, _PARTITION_COLOR, track_id)
            all_lines += new_lines
    else:
        p_y = min(partition.y0, partition.y1)
        p_start = min(partition.x0, partition.x1)
        p_end = max(partition.x0, partition.x1)
        cursor = p_start
        for gap_start, gap_end in sorted(openings):
            if cursor < gap_start:
                verts = [(cursor, p_y), (gap_start, p_y), (gap_start, p_y + t), (cursor, p_y + t)]
                new_lines, track_id = _filpoly_draw(verts, 0, _PARTITION_COLOR, track_id)
                all_lines += new_lines
            cursor = max(cursor, gap_end)
        if cursor < p_end:
            verts = [(cursor, p_y), (p_end, p_y), (p_end, p_y + t), (cursor, p_y + t)]
            new_lines, track_id = _filpoly_draw(verts, 0, _PARTITION_COLOR, track_id)
            all_lines += new_lines

    return all_lines, track_id


def _write_floor_plan(
    fp: FloorPlan,
    lines: list[str],
    track_id: int,
) -> int:
    """Append floor plan features as layer 0 fills: restricted zones and partitions."""
    rooms_by_name = {r.name: r for r in fp.rooms}

    for restricted in fp.restricted:
        new_lines, track_id = _floor_plan_restricted_draw(restricted, track_id)
        lines += new_lines

    for partition in fp.partitions:
        openings = _partition_door_openings(partition, fp.doors, rooms_by_name)
        new_lines, track_id = _floor_plan_partition_draw(partition, openings, track_id)
        lines += new_lines

    return track_id


# ---------------------------------------------------------------------------
# Core generator
# ---------------------------------------------------------------------------

def generate(config: LayoutConfig, output_path: Path) -> GenerationResult:
    """Generate a .xtc layout file from a validated LayoutConfig.

    Backs up any existing file at output_path, then writes fresh content.
    """
    result = GenerationResult(
        output_path=output_path,
        backup_path=_version_backup(output_path),
        placed=[],
        skipped=[],
        warnings=[],
    )

    room_w_in = config.room_width_ft * 12.0
    room_h_in = config.room_depth_ft * 12.0
    cell_in = config.grid_size_ft * 12.0
    sf = _scale_factor(config.scale)
    lib = load_library()

    lines: list[str] = []
    timestamp = datetime.date.today().isoformat()

    num_levels = config.levels if config.benchwork is not None else 0

    lines += [
        f"VERSION 10 3.0.0",
        f"TITLE1 {config.name}",
        f"TITLE2 Generated {timestamp} scale={config.scale} main={config.mainline}",
        f"SCALE {config.scale}",
        f"ROOMSIZE {room_w_in:.0f} x {room_h_in:.0f}",
        "",
    ]

    lines += _layers_header(num_levels)
    lines.append("")

    track_id = 1

    room_lines, track_id = _room_tableedges(room_w_in, room_h_in, track_id)
    lines += room_lines
    lines.append("")

    if config.floor_plan is not None:
        track_id = _write_floor_plan(config.floor_plan, lines, track_id)

    if config.benchwork is not None:
        track_id = _write_benchwork(
            config.benchwork, room_w_in, room_h_in,
            config.levels, lines, track_id,
        )

    for pl in config.placements:
        template_id = _find_template_id(pl, lib)
        if template_id is None:
            result.skipped.append(
                f"{pl.cell_range}={pl.element_type}: no template found for this type"
            )
            continue

        template_info = lib.by_id(template_id)

        if not _in_bounds(pl, cell_in, room_w_in, room_h_in):
            col_max = int(room_w_in / cell_in)
            row_max = int(room_h_in / cell_in)
            result.warnings.append(
                f"{pl.cell_range}={pl.element_type}: placement partially outside room "
                f"({config.room_width_ft:.0f}×{config.room_depth_ft:.0f} ft, "
                f"max col {'ABCDEFGHIJKLMNOPQRSTUVWXYZ'[col_max-1] if col_max <= 26 else str(col_max)}"
                f" row {row_max})"
            )

        x0, y0 = _range_origin(pl, cell_in, room_h_in)
        placed = PlacedElement(
            template_id=template_id,
            grid_range=pl.cell_range,
            x=x0,
            y=y0,
        )

        template_layout = parse_file(template_info.xtc_path)

        for track in template_layout.tracks:
            lines.append(_track_header(track.kind, track_id, 1, config.scale, track.extra))
            for ep in track.endpoints:
                lines.append(_endpoint_line(ep.x * sf + x0, ep.y * sf + y0, ep.angle))
            lines.append("END")
            placed.track_ids.append(track_id)
            track_id += 1

        result.placed.append(placed)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    return result


def default_output_path(config: LayoutConfig, config_path: Path) -> Path:
    """Derive a default output .xtc path from the config name and location."""
    stem = config.name.lower().replace(" ", "_")
    return config_path.parent / f"{stem}.xtc"
