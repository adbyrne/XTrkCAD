"""Data models for XTrkCAD layout objects."""

from dataclasses import dataclass, field


@dataclass
class Endpoint:
    """A track endpoint — connected (T) or open (E)."""
    x: float
    y: float
    angle: float
    connected_to: int | None = None  # track object ID if T, None if E


@dataclass
class TrackObject:
    """A single track element (STRAIGHT, CURVE, TURNOUT, etc.)."""
    id: int
    kind: str
    layer: int
    endpoints: list[Endpoint] = field(default_factory=list)
    # Kind-specific geometry
    extra: dict = field(default_factory=dict)


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
