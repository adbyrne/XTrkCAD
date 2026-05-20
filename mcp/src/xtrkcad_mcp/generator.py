"""Layout generator — produce an initial .xtc from a validated LayoutConfig."""

import datetime
import shutil
from dataclasses import dataclass, field
from pathlib import Path

import math

from xtrkcad_mcp.config import Benchwork, BenchworkWall, GridPlacement, LayoutConfig
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

def _layers_header(num_levels: int) -> list[str]:
    """Emit LAYERS lines for a multi-level layout with benchwork."""
    result = [
        'LAYERS 0 1 0 1 0 0 0 0 0 "Main" 1 7 18.0 5.0 0 0 0',
    ]
    for lv in range(1, num_levels + 1):
        result.append(
            f'LAYERS {lv} 1 0 1 0 0 0 0 0 "L{lv}-Benchwork" 0 7 18.0 5.0 0 0 0'
        )
    result.append("LAYERS CURRENT 0")
    return result


def _shelf_straights(
    x0: float, y0: float, x1: float, y1: float,
    layer: int, track_id: int, scale: str,
) -> tuple[list[str], int]:
    """Emit 4 STRAIGHT lines forming a closed rectangle (x0,y0)–(x1,y1).

    Returns (lines, next_track_id).
    """
    corners = [
        (x0, y0), (x1, y0), (x1, y1), (x0, y1),
    ]
    edges = [
        (corners[0], corners[1]),
        (corners[1], corners[2]),
        (corners[2], corners[3]),
        (corners[3], corners[0]),
    ]
    lines: list[str] = []
    tid = track_id
    for (ax, ay), (bx, by) in edges:
        angle = math.degrees(math.atan2(by - ay, bx - ax))
        lines.append(f"STRAIGHT {tid} {layer} 0 0 0 {scale} 0")
        lines.append(f"\tE {ax:.6f} {ay:.6f} {angle + 180.0:.6f}")
        lines.append(f"\tE {bx:.6f} {by:.6f} {angle:.6f}")
        lines.append("")
        tid += 1
    return lines, tid


def _wall_rect(
    wall: BenchworkWall, room_w_in: float, room_h_in: float,
) -> tuple[float, float, float, float]:
    """Return (x0, y0, x1, y1) for the shelf rectangle of a wall."""
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
    scale: str,
    lines: list[str],
    track_id: int,
) -> int:
    """Append LAYERS header + shelf rectangles to lines; return next track_id."""
    lines += _layers_header(num_levels)
    lines.append("")

    for lv in range(1, num_levels + 1):
        for wall in bw.walls:
            if lv not in wall.levels:
                continue
            x0, y0, x1, y1 = _wall_rect(wall, room_w_in, room_h_in)
            new_lines, track_id = _shelf_straights(x0, y0, x1, y1, lv, track_id, scale)
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

    lines += [
        f"VERSION 2 5.3.0Dev",
        f"TITLE1 {config.name}",
        f"TITLE2 Generated {timestamp} scale={config.scale} main={config.mainline}",
        f"SCALE {config.scale}",
        f"ROOMSIZE {room_w_in:.0f} x {room_h_in:.0f}",
        "",
    ]

    track_id = 1

    if config.benchwork is not None:
        track_id = _write_benchwork(
            config.benchwork, room_w_in, room_h_in,
            config.levels, config.scale, lines, track_id,
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
            lines.append(_track_header(track.kind, track_id, track.layer, config.scale, track.extra))
            for ep in track.endpoints:
                lines.append(_endpoint_line(ep.x * sf + x0, ep.y * sf + y0, ep.angle))
            lines.append("")
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
