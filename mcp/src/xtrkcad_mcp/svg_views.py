"""SVG plan and elevation view generation from a layout YAML config.

Two public functions:
  generate_plan_view()      — top-down plan, optional level filter
  generate_elevation_view() — wall-profile elevation, one wall at a time
"""

import datetime
import xml.sax.saxutils as _esc
from pathlib import Path

from xtrkcad_mcp.config import (
    Benchwork, FloorDoor, FloorPartition, FloorRestricted, FloorRoom,
    load_config, partition_door_openings,
)
from xtrkcad_mcp.models import arc_polyline, level_from_layer_name
from xtrkcad_mcp.parser import parse_file


# ---------------------------------------------------------------------------
# Color palette
# ---------------------------------------------------------------------------

_LEVEL_FILL   = {1: "#A5D6A7", 2: "#90CAF9", 3: "#FFCC80"}
_LEVEL_STROKE = {1: "#2E7D32", 2: "#1565C0", 3: "#E65100"}

_ROOM_BG       = "#F5F5F5"
_ROOM_BORDER   = "#212121"
_RESTR_BG      = "#FFCDD2"
_RESTR_STR     = "#C62828"
_DOOR_BG       = "#FFF9C4"
_DOOR_STR      = "#F57F17"
_PART_BG       = "#B0BEC5"
_PART_STR      = "#37474F"
_TEXT_DK       = "#212121"
_TEXT_MD       = "#616161"
_GRID          = "#E0E0E0"
_TRACK_STROKE  = "#0D0D0D"


# ---------------------------------------------------------------------------
# Minimal SVG builder
# ---------------------------------------------------------------------------

class _S:
    """Lightweight SVG accumulator."""

    def __init__(self, w: int, h: int):
        self.w, self.h = w, h
        self._b: list[str] = []

    @staticmethod
    def _fmt(v) -> str:
        return f"{v:.1f}" if isinstance(v, float) else str(v)

    def _kw(self, d: dict) -> str:
        return " ".join(f'{k.replace("_", "-")}="{self._fmt(v)}"' for k, v in d.items() if v != "")

    def add(self, s: str):          self._b.append(s)
    def gopen(self, **kw):          self._b.append(f"<g {self._kw(kw)}>")
    def gclose(self):               self._b.append("</g>")

    def rect(self, x, y, w, h, rx=0, **kw):
        rx_s = f' rx="{rx}"' if rx else ""
        self._b.append(
            f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}"{rx_s} {self._kw(kw)}/>'
        )

    def poly(self, pts: list, **kw):
        ps = " ".join(f"{x:.1f},{y:.1f}" for x, y in pts)
        self._b.append(f'<polygon points="{ps}" {self._kw(kw)}/>')

    def line(self, x1, y1, x2, y2, **kw):
        self._b.append(
            f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" {self._kw(kw)}/>'
        )

    def polyline(self, pts: list, **kw):
        ps = " ".join(f"{x:.1f},{y:.1f}" for x, y in pts)
        self._b.append(f'<polyline points="{ps}" fill="none" {self._kw(kw)}/>')

    def circle(self, cx, cy, r, **kw):
        self._b.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r:.1f}" {self._kw(kw)}/>')

    def text(self, x, y, s: str, **kw):
        self._b.append(f'<text x="{x:.1f}" y="{y:.1f}" {self._kw(kw)}>{_esc.escape(s)}</text>')

    def save(self, path: Path):
        body = "\n".join(self._b)
        path.write_text(
            f'<?xml version="1.0" encoding="utf-8"?>\n'
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" height="{self.h}" '
            f'viewBox="0 0 {self.w} {self.h}">\n{body}\n</svg>',
            encoding="utf-8",
        )


# ---------------------------------------------------------------------------
# Coordinate transform (layout inches → SVG pixels, Y flipped)
# ---------------------------------------------------------------------------

class _XF:
    def __init__(self, x_min, y_min, x_max, y_max, svg_w: int, pad: int):
        rw, rh = x_max - x_min, y_max - y_min
        self.scale = (svg_w - 2 * pad) / rw
        self.svg_w = svg_w
        self.svg_h = int(rh * self.scale + 2 * pad)
        self.pad = pad
        self._x0, self._y0 = x_min, y_min

    def x(self, lx: float) -> float:
        return (lx - self._x0) * self.scale + self.pad

    def y(self, ly: float) -> float:
        return self.svg_h - ((ly - self._y0) * self.scale + self.pad)

    def pts(self, verts):
        return [(self.x(vx), self.y(vy)) for vx, vy in verts]

    def ln(self, l: float) -> float:
        return l * self.scale


# ---------------------------------------------------------------------------
# Geometry helpers
# ---------------------------------------------------------------------------

def _rect_v(x, y, w, h):
    return [(x, y), (x + w, y), (x + w, y + h), (x, y + h)]


def _restr_verts(r: FloorRestricted):
    if r.vertices:
        return [(float(v[0]), float(v[1])) for v in r.vertices]
    return _rect_v(r.x, r.y, r.width, r.depth)


def _partition_rects(p: FloorPartition, doors: list[FloorDoor], rooms: list[FloorRoom]):
    """Return one rectangle per partition segment, with door openings cut out.

    Uses the same partition_door_openings() geometry as the .xtc generator
    (generator.py) so a doorway never renders as a solid wall here while
    showing correctly as an opening in XTrkCAD itself.
    """
    t = p.thickness
    vertical = abs(p.y1 - p.y0) >= abs(p.x1 - p.x0)
    if vertical:
        line_pos = min(p.x0, p.x1)
        span_lo, span_hi = min(p.y0, p.y1), max(p.y0, p.y1)
    else:
        line_pos = min(p.y0, p.y1)
        span_lo, span_hi = min(p.x0, p.x1), max(p.x0, p.x1)

    rooms_by_name = {r.name: r for r in rooms}
    openings = partition_door_openings(p, doors, rooms_by_name)

    def seg_rect(lo, hi):
        if vertical:
            return [(line_pos, lo), (line_pos + t, lo), (line_pos + t, hi), (line_pos, hi)]
        return [(lo, line_pos), (hi, line_pos), (hi, line_pos + t), (lo, line_pos + t)]

    rects = []
    cursor = span_lo
    for gap_start, gap_end in openings:
        if cursor < gap_start:
            rects.append(seg_rect(cursor, gap_start))
        cursor = max(cursor, gap_end)
    if cursor < span_hi:
        rects.append(seg_rect(cursor, span_hi))
    return rects


def _door_verts(door: FloorDoor, rooms: list[FloorRoom]):
    if door.swing == "none":
        return None
    room = next((r for r in rooms if r.name == door.room), None)
    if room is None:
        return None
    c = door.clearance_in
    if door.wall in ("east", "west"):
        y0 = room.y + door.from_in
        y1 = y0 + door.width_in
        wx = (room.x + room.width) if door.wall == "east" else room.x
        x0, x1 = (wx - c, wx) if door.wall == "east" else (wx, wx + c)
        return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    x0 = room.x + door.from_in
    x1 = x0 + door.width_in
    wy = (room.y + room.depth) if door.wall == "north" else room.y
    y0, y1 = (wy - c, wy) if door.wall == "north" else (wy, wy + c)
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def _bbox(verts):
    xs = [v[0] for v in verts]
    ys = [v[1] for v in verts]
    return min(xs), min(ys), max(xs), max(ys)


def _centroid(verts):
    n = len(verts)
    return sum(v[0] for v in verts) / n, sum(v[1] for v in verts) / n


# ---------------------------------------------------------------------------
# Shared decorations: scale bar, north arrow, legend
# ---------------------------------------------------------------------------

def _scale_bar(svg: _S, xf: _XF):
    bar_in = 24.0  # 2 ft bar
    px = xf.ln(bar_in)
    bx, by = float(xf.pad), float(xf.svg_h - xf.pad + 12)
    svg.rect(bx, by, px, 7, fill=_TEXT_DK)
    # half-bar white divider
    svg.rect(bx, by, px / 2, 7, fill="white", stroke=_TEXT_DK, stroke_width="0.5")
    svg.text(bx, by + 18, "0",         font_family="sans-serif", font_size="9", text_anchor="middle", fill=_TEXT_MD)
    svg.text(bx + px / 2, by + 18, "1′", font_family="sans-serif", font_size="9", text_anchor="middle", fill=_TEXT_MD)
    svg.text(bx + px, by + 18, "2′",   font_family="sans-serif", font_size="9", text_anchor="middle", fill=_TEXT_MD)


def _north_arrow(svg: _S, xf: _XF):
    ax = float(xf.svg_w - xf.pad - 14)
    ay = float(xf.svg_h - xf.pad + 8)
    svg.line(ax, ay + 22, ax, ay + 2, stroke=_TEXT_DK, stroke_width="2")
    svg.poly([(ax, ay), (ax - 5, ay + 10), (ax + 5, ay + 10)], fill=_TEXT_DK)
    svg.text(ax, ay - 3, "N", font_family="sans-serif", font_size="11",
             font_weight="bold", text_anchor="middle", fill=_TEXT_DK)


def _legend(svg: _S, svg_w: int, pad: int, levels: list[int], has_fp: bool,
            has_tracks: bool = False):
    items: list[tuple[str, str, str]] = []
    for lv in levels:
        items.append((f"Level {lv} benchwork",
                      _LEVEL_FILL.get(lv, "#E0E0E0"),
                      _LEVEL_STROKE.get(lv, "#757575")))
    if has_tracks:
        items.append(("Track", _TRACK_STROKE, _TRACK_STROKE))
    if has_fp:
        items += [
            ("Restricted zone", _RESTR_BG, _RESTR_STR),
            ("Door clearance",  _DOOR_BG,  _DOOR_STR),
            ("Partition",       _PART_BG,  _PART_STR),
        ]
    row_h = 15
    box_w, box_h = 130, len(items) * row_h + 10
    lx = svg_w - pad - box_w + 5
    ly = pad - 4
    svg.rect(lx - 4, ly, box_w, box_h, rx=3,
             fill="white", stroke="#BDBDBD", stroke_width="1", opacity="0.92")
    for i, (label, fill, stroke) in enumerate(items):
        iy = ly + 6 + i * row_h
        svg.rect(lx, iy, 13, 11, rx=2, fill=fill, stroke=stroke, stroke_width="1")
        svg.text(lx + 17, iy + 10, label,
                 font_family="sans-serif", font_size="10", fill=_TEXT_DK)


# ---------------------------------------------------------------------------
# Track overlay (from a .xtc/.xtce file)
# ---------------------------------------------------------------------------

def _draw_tracks(svg: _S, xf: _XF, xtc_path: str, draw_levels: list[int]) -> None:
    """Overlay real track geometry from an .xtc file on a plan view.

    Tracks are filtered to the given levels via their layer name's
    'Ln-' prefix (e.g. 'L1-Main' -> level 1), matching the convention used
    by get_operation_density / write_operation_density_report.
    """
    layout = parse_file(xtc_path)
    want = {str(lv) for lv in draw_levels}

    def _level_of(layer_idx: int) -> str:
        name = layout.layers[layer_idx].name if layer_idx in layout.layers else ""
        return level_from_layer_name(name).rstrip("Hh")

    kw = dict(stroke=_TRACK_STROKE, stroke_width="1.3", stroke_linecap="round")

    for t in layout.tracks:
        if _level_of(t.layer) not in want:
            continue
        eps = t.endpoints
        if len(eps) < 2:
            continue

        if t.kind == "CURVE":
            cx_, cy_ = t.extra.get("cx"), t.extra.get("cy")
            r = t.extra.get("radius", 0.0)
            if cx_ is None or r <= 0:
                svg.line(xf.x(eps[0].x), xf.y(eps[0].y), xf.x(eps[1].x), xf.y(eps[1].y), **kw)
                continue
            arc_deg = ((eps[1].angle - eps[0].angle + 180.0) % 360.0 + 360.0) % 360.0
            pts = arc_polyline(cx_, cy_, r, eps[0].x, eps[0].y, eps[1].x, eps[1].y, arc_deg)
            svg.polyline([(xf.x(px), xf.y(py)) for px, py in pts], **kw)

        elif t.kind == "TURNOUT":
            x0, y0 = xf.x(eps[0].x), xf.y(eps[0].y)
            for ep in eps[1:]:
                svg.line(x0, y0, xf.x(ep.x), xf.y(ep.y), **kw)

        elif t.kind == "TURNTABLE":
            cx_, cy_ = t.extra.get("cx"), t.extra.get("cy")
            r = t.extra.get("radius", 0.0)
            if cx_ is not None and r > 0:
                svg.circle(xf.x(cx_), xf.y(cy_), xf.ln(r), fill="none", **kw)
            for ep in eps:
                if cx_ is not None:
                    svg.line(xf.x(cx_), xf.y(cy_), xf.x(ep.x), xf.y(ep.y), **kw)

        else:
            # STRAIGHT, JOINT, HANDLAID, CORNU, BEZIER — straight-line approximation
            svg.line(xf.x(eps[0].x), xf.y(eps[0].y), xf.x(eps[-1].x), xf.y(eps[-1].y), **kw)


# ---------------------------------------------------------------------------
# Plan view — public
# ---------------------------------------------------------------------------

def generate_plan_view(
    config_path: str,
    output_path: str,
    level: int = 0,
    xtc_path: str | None = None,
    svg_width: int = 840,
    padding: int = 55,
) -> str:
    """Generate a top-down SVG plan view.

    Args:
        config_path: YAML layout config path.
        output_path: Destination .svg path.
        level: 0 = all levels; 1, 2, … = that level only.
        xtc_path: Optional .xtc/.xtce file to overlay real track geometry on
            top of the benchwork, filtered to the same level(s).
        svg_width: Canvas width in pixels.
        padding: Margin around the room in pixels.

    Returns:
        Absolute path of the written file.
    """
    cr = load_config(config_path)
    cfg = cr.config
    fp = cfg.floor_plan

    if fp and fp.rooms:
        x_max, y_max = fp.total_width_in, fp.total_depth_in
    else:
        x_max, y_max = cfg.room_width_ft * 12.0, cfg.room_depth_ft * 12.0
    x_min = y_min = 0.0

    xf = _XF(x_min, y_min, x_max, y_max, svg_width, padding)
    title_h = 58
    footer_h = 38
    total_h = xf.svg_h + title_h + footer_h

    svg = _S(svg_width, total_h)
    svg.rect(0, 0, float(svg_width), float(total_h), fill="white")

    # Title
    level_str = f" — Level {level}" if level else " — All Levels"
    svg.text(svg_width / 2, 26,
             (cfg.name or "Layout") + level_str,
             font_family="sans-serif", font_size="17", font_weight="bold",
             text_anchor="middle", fill=_TEXT_DK)
    svg.text(svg_width / 2, 44,
             f"Scale: {cfg.scale}  |  Room: {x_max/12:.0f}×{y_max/12:.0f} ft  |  "
             f"{datetime.date.today().isoformat()}",
             font_family="sans-serif", font_size="10", text_anchor="middle", fill=_TEXT_MD)

    # Main drawing group offset by title
    svg.gopen(transform=f"translate(0 {title_h})")

    # Room background
    svg.rect(xf.x(x_min), xf.y(y_max), xf.ln(x_max), xf.ln(y_max),
             fill=_ROOM_BG, stroke=_ROOM_BORDER, stroke_width="2")

    # Floor plan overlays
    if fp:
        rooms = fp.rooms or []
        for r in fp.restricted:
            svg.poly(xf.pts(_restr_verts(r)),
                     fill=_RESTR_BG, stroke=_RESTR_STR, stroke_width="1", opacity="0.85")
        for d in fp.doors:
            v = _door_verts(d, rooms)
            if v:
                svg.poly(xf.pts(v), fill=_DOOR_BG, stroke=_DOOR_STR,
                         stroke_width="1", opacity="0.85")
        for p in fp.partitions:
            for verts in _partition_rects(p, fp.doors, rooms):
                svg.poly(xf.pts(verts), fill=_PART_BG, stroke=_PART_STR, stroke_width="1.5")

    # Benchwork
    all_levels = sorted({bw.level for bw in cfg.benchwork_sections})
    draw_levels = [level] if level else all_levels

    for lv in draw_levels:
        fill   = _LEVEL_FILL.get(lv, "#E0E0E0")
        stroke = _LEVEL_STROKE.get(lv, "#757575")
        for bw in cfg.benchwork_sections:
            if bw.level != lv or not bw.vertices:
                continue
            svg.poly(xf.pts(bw.vertices),
                     fill=fill, stroke=stroke, stroke_width="1.5", opacity="0.72")

    # Room outline on top
    svg.poly(xf.pts([(x_min, y_min), (x_max, y_min), (x_max, y_max), (x_min, y_max)]),
             fill="none", stroke=_ROOM_BORDER, stroke_width="2.5")

    # Track overlay (drawn over benchwork, under labels)
    if xtc_path:
        _draw_tracks(svg, xf, xtc_path, draw_levels)

    # Labels
    label_px = max(7, min(11, int(xf.ln(5.5))))
    for lv in draw_levels:
        ink = _LEVEL_STROKE.get(lv, "#424242")
        for bw in cfg.benchwork_sections:
            if bw.level != lv or not bw.vertices:
                continue
            x0, y0, x1, y1 = _bbox(bw.vertices)
            if xf.ln(x1 - x0) < 18 or xf.ln(y1 - y0) < 8:
                continue
            cx, cy = _centroid(bw.vertices)
            short = (bw.label
                     .replace("main_", "")
                     .replace("_l2", "")
                     .replace("ul_", "ul·")
                     .replace("_lower", "↓")
                     .replace("_upper", "↑")
                     .replace("_mid", "~")
                     .replace("_west", "W")
                     .replace("_east", "E"))
            svg.text(xf.x(cx), xf.y(cy) - 2, short,
                     font_family="sans-serif", font_size=str(label_px),
                     font_weight="bold", text_anchor="middle", fill=ink)
            if bw.elevation_in:
                svg.text(xf.x(cx), xf.y(cy) + label_px + 1,
                         f'{bw.elevation_in:.0f}"',
                         font_family="sans-serif", font_size=str(label_px - 1),
                         text_anchor="middle", fill=ink)

    _scale_bar(svg, xf)
    _north_arrow(svg, xf)
    _legend(svg, svg_width, padding, draw_levels, fp is not None, has_tracks=bool(xtc_path))
    svg.gclose()

    out = Path(output_path).expanduser()
    out.parent.mkdir(parents=True, exist_ok=True)
    svg.save(out)
    return str(out.resolve())


# ---------------------------------------------------------------------------
# Elevation view helpers
# ---------------------------------------------------------------------------

_WALL_DEPTH = 30.0  # sections within this many inches of a wall face count


def _wall_sections(
    sections: list[Benchwork], wall: str, levels: list[int],
) -> list[Benchwork]:
    """Return sections that touch the given wall, in the specified levels."""
    result = []
    for bw in sections:
        if levels and bw.level not in levels:
            continue
        if not bw.vertices:
            continue
        x0, y0, x1, y1 = _bbox(bw.vertices)
        if wall == "west"  and x0 <= _WALL_DEPTH:             result.append(bw)
        elif wall == "south" and y0 <= _WALL_DEPTH:           result.append(bw)
        elif wall == "east"  and x1 >= 0:                     # handled by caller passing x_max
            result.append(bw)
        elif wall == "north" and y1 >= 0:
            result.append(bw)
    return result


def _wall_span(bw: Benchwork, wall: str) -> tuple[float, float]:
    """Return the along-wall span (start, end) of a benchwork section."""
    x0, y0, x1, y1 = _bbox(bw.vertices)
    if wall in ("west", "east"):
        return y0, y1   # span in the N-S direction
    return x0, x1       # span in the E-W direction


# ---------------------------------------------------------------------------
# Elevation view — public
# ---------------------------------------------------------------------------

def generate_elevation_view(
    config_path: str,
    output_path: str,
    wall: str = "west",
    levels: list[int] | None = None,
    max_height_in: float = 80.0,
    svg_width: int = 960,
    h_pad: int = 70,
    v_pad: int = 55,
) -> str:
    """Generate a wall-profile elevation SVG.

    Shows the benchwork cross-section as seen from outside the room looking
    in, with the along-wall axis horizontal and height vertical.

    Args:
        config_path: YAML layout config path.
        output_path: Destination .svg path.
        wall: Which wall to profile — "west", "south", "east", or "north".
        levels: List of deck levels to include; None = all.
        max_height_in: Top of the Y axis in real inches (default 80").
        svg_width: Canvas width in pixels.
        h_pad: Horizontal padding (pixels).
        v_pad: Vertical padding (pixels) — used for top and bottom margins.

    Returns:
        Absolute path of the written file.
    """
    wall = wall.lower().strip()
    if wall not in ("west", "south", "east", "north"):
        raise ValueError(f"wall must be west/south/east/north, got {wall!r}")

    cr = load_config(config_path)
    cfg = cr.config
    fp = cfg.floor_plan

    if fp and fp.rooms:
        room_w, room_h = fp.total_width_in, fp.total_depth_in
    else:
        room_w = cfg.room_width_ft * 12.0
        room_h = cfg.room_depth_ft * 12.0

    # Along-wall axis range
    if wall in ("west", "east"):
        span_min, span_max = 0.0, room_h
        axis_label = "Position along wall — south → north (in)"
    else:
        span_min, span_max = 0.0, room_w
        axis_label = "Position along wall — west → east (in)"

    # Filter sections
    all_sections = cfg.benchwork_sections
    show_levels = levels if levels else sorted({bw.level for bw in all_sections})

    # For east/north walls we need the actual x_max / y_max to filter
    wall_sections: list[Benchwork] = []
    for bw in all_sections:
        if bw.level not in show_levels or not bw.vertices:
            continue
        x0, y0, x1, y1 = _bbox(bw.vertices)
        if wall == "west"  and x0 <= _WALL_DEPTH:            wall_sections.append(bw)
        elif wall == "south" and y0 <= _WALL_DEPTH:          wall_sections.append(bw)
        elif wall == "east"  and x1 >= room_w - _WALL_DEPTH: wall_sections.append(bw)
        elif wall == "north" and y1 >= room_h - _WALL_DEPTH: wall_sections.append(bw)

    # Coordinate transforms:
    # Horizontal: along-wall span → SVG x
    # Vertical: height (0..max_height_in) → SVG y  (0" = bottom)
    title_h = 56
    footer_h = 46
    draw_w = svg_width - 2 * h_pad
    draw_h = int(draw_w * (max_height_in / (span_max - span_min)) * 0.55)
    draw_h = max(260, min(480, draw_h))
    total_h = draw_w + title_h + footer_h  # placeholder, recalc below
    total_h = draw_h + title_h + footer_h + 2 * v_pad

    h_scale = draw_w / (span_max - span_min)
    v_scale = draw_h / max_height_in

    def hx(pos: float) -> float:     # along-wall position → SVG x
        return h_pad + (pos - span_min) * h_scale

    def vy(height: float) -> float:  # height above floor → SVG y (flipped)
        return title_h + v_pad + draw_h - height * v_scale

    svg = _S(svg_width, total_h)
    svg.rect(0, 0, float(svg_width), float(total_h), fill="white")

    # Title
    level_str = ", ".join(f"L{lv}" for lv in show_levels)
    wall_cap = wall.capitalize()
    svg.text(svg_width / 2, 24,
             f"{cfg.name or 'Layout'} — {wall_cap} Wall Elevation ({level_str})",
             font_family="sans-serif", font_size="15", font_weight="bold",
             text_anchor="middle", fill=_TEXT_DK)
    svg.text(svg_width / 2, 42,
             f"Viewed looking {'east' if wall=='west' else 'west' if wall=='east' else 'north' if wall=='south' else 'south'}  "
             f"|  Heights in real inches above floor  |  {datetime.date.today().isoformat()}",
             font_family="sans-serif", font_size="10", text_anchor="middle", fill=_TEXT_MD)

    # Drawing area background
    svg.rect(float(h_pad), float(title_h + v_pad),
             float(draw_w), float(draw_h),
             fill="#F9FAFB", stroke=_GRID, stroke_width="1")

    # Horizontal guide lines at key heights
    key_heights = [0, 12, 24, 36, 40, 43, 48, 51, 54, 60, 63, 66, 69, 72, 72.75, 80]
    for hgt in key_heights:
        if hgt > max_height_in:
            continue
        sy = vy(hgt)
        is_major = hgt % 12 == 0
        col = "#B0BEC5" if is_major else "#E0E0E0"
        lw = "0.8" if is_major else "0.4"
        svg.line(float(h_pad), sy, float(h_pad + draw_w), sy,
                 stroke=col, stroke_width=lw)
        lbl = f'{hgt:.2g}"'
        svg.text(float(h_pad - 4), sy + 3, lbl,
                 font_family="sans-serif", font_size="8", text_anchor="end", fill=_TEXT_MD)

    # Vertical position tick marks every 12"
    for pos in range(int(span_min), int(span_max) + 1, 12):
        sx = hx(float(pos))
        sy_bot = vy(0)
        svg.line(sx, sy_bot, sx, sy_bot + 5,
                 stroke="#B0BEC5", stroke_width="0.6")
        svg.text(sx, sy_bot + 14, f'{pos}"',
                 font_family="sans-serif", font_size="8", text_anchor="middle", fill=_TEXT_MD)

    # Floor line
    svg.line(float(h_pad), vy(0.0), float(h_pad + draw_w), vy(0.0),
             stroke=_TEXT_DK, stroke_width="2")

    # Benchwork section bars
    for bw in wall_sections:
        span_s, span_e = _wall_span(bw, wall)
        elev = bw.elevation_in
        thick = bw.thickness_in if bw.thickness_in else 3.5
        fill   = _LEVEL_FILL.get(bw.level, "#E0E0E0")
        stroke = _LEVEL_STROKE.get(bw.level, "#757575")
        sx  = hx(span_s)
        ex  = hx(span_e)
        top = vy(elev)
        bot = vy(elev - thick)
        bar_w = ex - sx
        bar_h = bot - top
        if bar_w < 1:
            continue
        svg.rect(sx, top, bar_w, bar_h,
                 fill=fill, stroke=stroke, stroke_width="1", opacity="0.82")
        # Elevation label on top edge
        mid_x = (sx + ex) / 2
        if bar_w >= 16:
            svg.text(mid_x, top - 3, f'{elev:.4g}"',
                     font_family="sans-serif", font_size="7",
                     text_anchor="middle", fill=stroke, font_weight="bold")
        # Section name inside bar if tall enough
        if bar_h >= 10 and bar_w >= 20:
            short = (bw.label
                     .replace("main_", "")
                     .replace("_l2", "")
                     .replace("ul_", "ul·")
                     .replace("_lower", "↓")
                     .replace("_upper", "↑")
                     .replace("_mid", "~")
                     .replace("_west", "W")
                     .replace("_east", "E"))
            svg.text(mid_x, (top + bot) / 2 + 3, short,
                     font_family="sans-serif", font_size="7",
                     text_anchor="middle", fill=stroke)

    # Axis labels
    svg.text(svg_width / 2, float(title_h + v_pad + draw_h + 28),
             axis_label,
             font_family="sans-serif", font_size="10",
             text_anchor="middle", fill=_TEXT_MD)

    # Y axis label (rotated)
    svg.gopen(transform=f"translate({h_pad - 42},{title_h + v_pad + draw_h // 2}) rotate(-90)")
    svg.text(0.0, 0.0, "Height above floor (in)",
             font_family="sans-serif", font_size="10",
             text_anchor="middle", fill=_TEXT_MD)
    svg.gclose()

    # Legend
    lx = float(svg_width - h_pad - 130)
    ly = float(title_h + v_pad + 6)
    shown = sorted({bw.level for bw in wall_sections})
    for i, lv in enumerate(shown):
        iy = ly + i * 16
        svg.rect(lx, iy, 13, 11, rx=2,
                 fill=_LEVEL_FILL.get(lv, "#E0E0E0"),
                 stroke=_LEVEL_STROKE.get(lv, "#757575"), stroke_width="1")
        svg.text(lx + 17, iy + 10, f"Level {lv}",
                 font_family="sans-serif", font_size="10", fill=_TEXT_DK)

    out = Path(output_path).expanduser()
    out.parent.mkdir(parents=True, exist_ok=True)
    svg.save(out)
    return str(out.resolve())
