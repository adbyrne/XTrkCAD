"""Data models for XTrkCAD layout objects."""

import math
import re
from dataclasses import dataclass, field


# Fugate cars-per-real-foot factors (MRH Oct 2014).
# Based on a ~44-ft car (40-ft body + couplers) at each scale.
CARS_PER_FT: dict[str, float] = {
    "O": 1.0,
    "S": 1.5,
    "HO": 2.0,
    "N": 4.0,
    "Z": 5.0,
    # Narrow gauge — same body length as parent scale
    "HOn3": 2.0,
    "On3": 1.0,
    "Sn3": 1.5,
    "Nn3": 4.0,
    # Other common scales (approximated)
    "G": 0.5,
    "TT": 3.0,
    "I": 0.75,
}

# Scale prototype ratio (1:X) — used for radius conversions only
SCALE_RATIOS: dict[str, float] = {
    "Z": 220.0, "N": 160.0, "TT": 120.0, "HO": 87.1,
    "S": 64.0, "O": 48.0, "G": 22.5, "I": 32.0,
    "Nn3": 160.0, "HOn3": 87.1, "On3": 48.0, "Sn3": 64.0,
}


def cars_per_real_ft(scale: str) -> float:
    """Return Fugate cars/ft factor for the given scale name."""
    return CARS_PER_FT.get(scale.upper(), CARS_PER_FT.get(scale, 2.0))


def ft_to_cars(feet: float, scale: str) -> int:
    """Convert real feet of track to equivalent number of 40-ft cars (truncated)."""
    return int(feet * cars_per_real_ft(scale))


# Max-to-main interpretation bands
def max_to_main_label(pct: float) -> str:
    if pct < 50:
        return "Mainline focus"
    if pct <= 80:
        return "Mainline emphasis"
    if pct <= 120:
        return "Balanced"
    if pct <= 150:
        return "Switching emphasis"
    return "Switching focus"


@dataclass
class LayerInfo:
    """Named layer from the LAYERS record."""
    index: int
    name: str
    visible: bool = True


@dataclass
class Endpoint:
    """A track endpoint — connected (T) or open (E)."""
    x: float
    y: float
    angle: float
    connected_to: int | None = None


def _normalize_angle(a: float) -> float:
    return ((a % 360.0) + 360.0) % 360.0


@dataclass
class TrackObject:
    """A single track element."""
    id: int
    kind: str
    layer: int
    endpoints: list[Endpoint] = field(default_factory=list)
    extra: dict = field(default_factory=dict)

    def length_model_inches(self) -> float:
        """Track length in model inches.

        STRAIGHT / JOINT: Euclidean distance between ep[0] and ep[1].
        CURVE: r × 2π × arc_angle / 360 using GetCurveAngles() from tcurve.c:
               arc_angle = NormalizeAngle(ep[1].angle - ep[0].angle + 180).
        TURNOUT: chord ep[0] → ep[1] (main-path approximation).
        """
        eps = self.endpoints
        if len(eps) < 2:
            return 0.0
        if self.kind in {"STRAIGHT", "JOINT"}:
            return math.hypot(eps[1].x - eps[0].x, eps[1].y - eps[0].y)
        if self.kind == "CURVE":
            r = self.extra.get("radius", 0.0)
            if r <= 0.0:
                return math.hypot(eps[1].x - eps[0].x, eps[1].y - eps[0].y)
            arc_deg = _normalize_angle(eps[1].angle - eps[0].angle + 180.0)
            return r * 2.0 * math.pi * arc_deg / 360.0
        if self.kind == "TURNOUT":
            return math.hypot(eps[1].x - eps[0].x, eps[1].y - eps[0].y)
        return 0.0

    def length_real_feet(self) -> float:
        return self.length_model_inches() / 12.0


@dataclass
class NoteObject:
    """A NOTE object from the layout — text, link, or file annotation."""
    id: int
    layer: int
    x: float
    y: float
    op: int       # 0=text, 1=link, 2=file
    text: str     # body text (text content, URL, or file path)
    title: str = ""  # title string for link/file notes


@dataclass
class Layout:
    """Parsed XTrkCAD layout file."""
    param_version: int = 0
    version: str = ""
    title1: str = ""
    title2: str = ""
    scale: str = ""
    room_width: float = 0.0
    room_height: float = 0.0
    tracks: list[TrackObject] = field(default_factory=list)
    layers: dict[int, LayerInfo] = field(default_factory=dict)
    notes: list[NoteObject] = field(default_factory=list)

    def track_counts(self) -> dict[str, int]:
        counts: dict[str, int] = {}
        for t in self.tracks:
            counts[t.kind] = counts.get(t.kind, 0) + 1
        return counts

    def unconnected_endpoints(self) -> list[tuple[int, Endpoint]]:
        return [
            (t.id, ep) for t in self.tracks
            for ep in t.endpoints if ep.connected_to is None
        ]

    def total_length_real_feet(self) -> float:
        return sum(t.length_real_feet() for t in self.tracks)

    def length_by_layer(self) -> dict[int, float]:
        result: dict[int, float] = {}
        for t in self.tracks:
            result[t.layer] = result.get(t.layer, 0.0) + t.length_real_feet()
        return result

    def turnouts_by_layer(self) -> dict[int, int]:
        result: dict[int, int] = {}
        for t in self.tracks:
            if t.kind == "TURNOUT":
                result[t.layer] = result.get(t.layer, 0) + 1
        return result

    def curve_radii(self) -> list[float]:
        return [
            t.extra["radius"] for t in self.tracks
            if t.kind == "CURVE" and t.extra.get("radius", 0.0) > 0.0
        ]


def level_from_layer_name(name: str) -> str:
    """Return the level key from a standard layer name, or '0' if unrecognised.

    Examples: 'L1-Main' → '1', 'L2-Staging' → '2', 'L1H-Connecting' → '1H'.
    Layers without an 'Ln-' prefix (Floor, custom names) → '0'.
    """
    m = re.match(r"^[Ll](\d+[Hh]?)-", name)
    return m.group(1) if m else "0"


def arc_polyline(
    cx: float, cy: float, r: float,
    x1: float, y1: float, x2: float, y2: float,
    arc_deg: float, n: int = 24,
) -> list[tuple[float, float]]:
    """Sample n+1 points on the circular arc from (x1,y1) to (x2,y2)."""
    if arc_deg < 0.01 or r <= 0:
        return [(x1, y1), (x2, y2)]
    a_start = math.atan2(y1 - cy, x1 - cx)
    arc_rad = math.radians(arc_deg)

    def sample(direction: float) -> list[tuple[float, float]]:
        return [
            (cx + r * math.cos(a_start + direction * arc_rad * i / n),
             cy + r * math.sin(a_start + direction * arc_rad * i / n))
            for i in range(n + 1)
        ]

    pts_ccw = sample(1.0)
    pts_cw = sample(-1.0)

    def dist_end(pts: list) -> float:
        ex, ey = pts[-1]
        return math.hypot(ex - x2, ey - y2)

    return pts_ccw if dist_end(pts_ccw) <= dist_end(pts_cw) else pts_cw
