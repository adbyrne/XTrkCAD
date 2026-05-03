"""Data models for XTrkCAD layout objects."""

import math
from dataclasses import dataclass, field


# Scale ratio lookup (model:prototype = 1:ratio)
SCALE_RATIOS: dict[str, float] = {
    "Z": 220.0,
    "N": 160.0,
    "TT": 120.0,
    "HO": 87.1,
    "S": 64.0,
    "O": 48.0,
    "G": 22.5,
    "I": 32.0,
    "Nn3": 160.0,
    "HOn3": 87.1,
    "On3": 48.0,
    "Sn3": 64.0,
}

# Car length in model inches for a standard 40-ft freight car, by scale
def car_length_model_inches(scale: str) -> float:
    ratio = SCALE_RATIOS.get(scale.upper(), SCALE_RATIOS.get(scale, 87.1))
    return (40.0 * 12.0) / ratio  # 40 prototype ft → model inches


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
    connected_to: int | None = None  # track object ID if T, None if E


def _normalize_angle(a: float) -> float:
    """Bring angle to [0, 360)."""
    return ((a % 360.0) + 360.0) % 360.0


@dataclass
class TrackObject:
    """A single track element (STRAIGHT, CURVE, TURNOUT, etc.)."""
    id: int
    kind: str
    layer: int
    endpoints: list[Endpoint] = field(default_factory=list)
    extra: dict = field(default_factory=dict)

    def length_model_inches(self) -> float:
        """Compute track length in model inches.

        STRAIGHT / JOINT: Euclidean distance between endpoints 0 and 1.
        CURVE: r × 2π × arc_angle / 360, where arc_angle uses the formula
               from XTrkCAD's GetCurveAngles(): a1 = NormalizeAngle(ep1 - ep0 + 180).
        TURNOUT: straight-line distance ep0 → ep1 (main path approximation).
        Others: 0.0.
        """
        eps = self.endpoints
        if len(eps) < 2:
            return 0.0

        if self.kind in {"STRAIGHT", "JOINT"}:
            dx = eps[1].x - eps[0].x
            dy = eps[1].y - eps[0].y
            return math.hypot(dx, dy)

        if self.kind == "CURVE":
            radius = self.extra.get("radius", 0.0)
            if radius <= 0.0:
                # Fallback to chord length
                dx = eps[1].x - eps[0].x
                dy = eps[1].y - eps[0].y
                return math.hypot(dx, dy)
            arc_angle = _normalize_angle(eps[1].angle - eps[0].angle + 180.0)
            return radius * 2.0 * math.pi * arc_angle / 360.0

        if self.kind == "TURNOUT":
            dx = eps[1].x - eps[0].x
            dy = eps[1].y - eps[0].y
            return math.hypot(dx, dy)

        return 0.0

    def length_real_feet(self) -> float:
        """Length in real (layout room) feet: model_inches / 12."""
        return self.length_model_inches() / 12.0


@dataclass
class Layout:
    """Parsed XTrkCAD layout file."""
    version: str = ""
    title1: str = ""
    title2: str = ""
    scale: str = ""
    room_width: float = 0.0
    room_height: float = 0.0
    tracks: list[TrackObject] = field(default_factory=list)
    layers: dict[int, LayerInfo] = field(default_factory=dict)

    def track_counts(self) -> dict[str, int]:
        counts: dict[str, int] = {}
        for t in self.tracks:
            counts[t.kind] = counts.get(t.kind, 0) + 1
        return counts

    def unconnected_endpoints(self) -> list[tuple[int, Endpoint]]:
        """Return (track_id, endpoint) pairs where endpoint.connected_to is None."""
        return [
            (t.id, ep)
            for t in self.tracks
            for ep in t.endpoints
            if ep.connected_to is None
        ]

    def total_length_real_feet(self) -> float:
        return sum(t.length_real_feet() for t in self.tracks)

    def length_by_layer(self) -> dict[int, float]:
        """Real feet of track per layer index."""
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
        """All CURVE radii in model inches."""
        return [
            t.extra["radius"]
            for t in self.tracks
            if t.kind == "CURVE" and t.extra.get("radius", 0.0) > 0.0
        ]
