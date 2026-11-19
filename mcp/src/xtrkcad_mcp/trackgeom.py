"""Track geometry: pieces of track held in memory, joined end to end, and
written as XTrkCAD .xtc records.

The model is independent of the file: generate_track_plan builds a
TrackModel and the .xtc writer is one way out of it (an automation link to a
running XTrkCAD could be another).

Angles are XTrkCAD's: compass degrees, 0 = north (+y), 90 = east (+x),
increasing clockwise. An endpoint's angle points out of the track at that
end. A turnout's local x axis runs from its points toward its frog; placed
at angle A, local x points toward compass A + 90 and local y toward A (so
A = 0 leaves the definition as drawn: x east, y north).
"""

from __future__ import annotations

import functools
import math
import os
import re
from dataclasses import dataclass, field
from pathlib import Path

_EPS = 1e-4          # ends closer than this (layout inches) are one point


def compass(dx: float, dy: float) -> float:
    """The compass angle of a direction."""
    return math.degrees(math.atan2(dx, dy)) % 360.0


def direction(angle: float) -> tuple[float, float]:
    """The unit vector of a compass angle."""
    rad = math.radians(angle)
    return math.sin(rad), math.cos(rad)


def _norm(angle: float) -> float:
    return angle % 360.0


# -- turnout definitions -------------------------------------------------------

@dataclass
class TurnoutDef:
    """One TURNOUT definition from an XTrkCAD parameter file (.xtp)."""
    title: str                    # "Manufacturer\tDescription\tPart number"
    scale: str
    endpoints: list[tuple[float, float, float]]   # local (x, y, angle)
    paths: list[str] = field(default_factory=list)      # "P ..." lines
    segments: list[str] = field(default_factory=list)   # "S/C ..." lines

    @property
    def length(self) -> float:
        """Points to the straight leg's end, along local x."""
        return self.endpoints[1][0]

    @property
    def diverge_offset(self) -> float:
        """The diverging leg's end, sideways: + left, - right."""
        return self.endpoints[2][1]

    @property
    def diverge_length(self) -> float:
        return self.endpoints[2][0]

    @property
    def frog_angle(self) -> float:
        """The diverging leg's angle off the straight leg, degrees."""
        return abs(90.0 - self.endpoints[2][2])


@dataclass
class TurnoutPair:
    left: TurnoutDef
    right: TurnoutDef

    def hand(self, side: str) -> TurnoutDef:
        return self.left if side == "left" else self.right


def params_dirs() -> list[Path]:
    """Where XTrkCAD parameter files are looked for, in order:
    $XTRKCAD_PARAMS_DIR, the source tree this package sits in, and the
    usual install locations."""
    dirs: list[Path] = []
    env = os.environ.get("XTRKCAD_PARAMS_DIR")
    if env:
        dirs.append(Path(env))
    repo = Path(__file__).resolve().parents[3] / "app" / "lib" / "params"
    dirs.append(repo)
    for base in (Path.home() / ".local" / "share", Path("/usr/local/share"),
                 Path("/usr/share")):
        dirs += sorted(base.glob("xtrkcad*/params"))
    return [d for d in dirs if d.is_dir()]


def read_turnout_defs(path: Path) -> list[TurnoutDef]:
    """Every TURNOUT definition in a parameter file."""
    defs: list[TurnoutDef] = []
    cur: TurnoutDef | None = None
    with open(path, encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            line = raw.rstrip("\r\n")
            if line.startswith("TURNOUT "):
                m = re.match(r'TURNOUT\s+(\S+)\s+"(.*)"', line)
                cur = TurnoutDef(m.group(2), m.group(1), []) if m else None
                continue
            if cur is None:
                continue
            word = line.strip().split(" ", 1)[0] if line.strip() else ""
            if word == "END":
                defs.append(cur)
                cur = None
            elif word == "E":
                try:
                    _e, x, y, a = line.split()[:4]
                    cur.endpoints.append((float(x), float(y), float(a)))
                except ValueError:       # malformed: the definition can't be used
                    cur.endpoints.append((math.nan, math.nan, math.nan))
            elif word == "P":
                cur.paths.append(line.strip())
            elif word in ("S", "C"):
                cur.segments.append(line.strip())
    return defs


@functools.lru_cache(maxsize=32)
def find_turnouts(name: str, scale: str) -> TurnoutPair:
    """The left and right hand of a turnout named like "PECO Code 83 #6":
    the manufacturer, then the number. Matched against the parameter
    files' titles ("Manufacturer\\tDescription\\tPart"), case-insensitive."""
    m = re.match(r"(.+?)\s+(#\S+)$", name.strip())
    if not m:
        raise LookupError(f"Turnout {name!r}: expected a manufacturer and a number, "
                          "e.g. \"PECO Code 83 #6\"")
    maker, number = m.group(1).lower(), m.group(2)
    desc_re = re.compile(rf"^{re.escape(number)}\s+(Left|Right)\b(?!.*Curved).*Turnout",
                         re.IGNORECASE)
    found: dict[str, TurnoutDef] = {}
    for d in params_dirs():
        for path in sorted(d.glob("*.xtp")):
            try:
                if maker not in path.read_text(encoding="utf-8", errors="replace").lower():
                    continue
            except OSError:
                continue
            for defn in read_turnout_defs(path):
                parts = defn.title.split("\t")
                if defn.scale != scale or len(parts) < 2 or parts[0].lower() != maker:
                    continue
                hit = desc_re.match(parts[1])
                if (hit and len(defn.endpoints) == 3
                        and not any(math.isnan(v) for e in defn.endpoints for v in e)):
                    found.setdefault(hit.group(1).lower(), defn)
        if len(found) == 2:
            return TurnoutPair(found["left"], found["right"])
    raise LookupError(f"No left and right {name!r} turnout in {scale} parameter files "
                      f"(looked in: {', '.join(str(d) for d in params_dirs()) or 'none'})")


# -- pieces ----------------------------------------------------------------------

End = tuple[float, float, float]     # world (x, y, angle out of the track)


@dataclass(eq=False)
class Piece:
    id: int
    layer: int

    def ends(self) -> list[End]:
        raise NotImplementedError


@dataclass(eq=False)
class Straight(Piece):
    p0: tuple[float, float] = (0.0, 0.0)
    p1: tuple[float, float] = (0.0, 0.0)

    def __init__(self, pid: int, layer: int, p0, p1):
        super().__init__(pid, layer)
        self.p0, self.p1 = (float(p0[0]), float(p0[1])), (float(p1[0]), float(p1[1]))

    def ends(self) -> list[End]:
        a = compass(self.p1[0] - self.p0[0], self.p1[1] - self.p0[1])
        return [(*self.p0, _norm(a + 180.0)), (*self.p1, a)]

    def length(self) -> float:
        return math.dist(self.p0, self.p1)


@dataclass(eq=False)
class Curve(Piece):
    """An arc from compass position ``start`` (seen from the centre)
    clockwise through ``sweep`` degrees."""
    center: tuple[float, float] = (0.0, 0.0)
    radius: float = 0.0
    start: float = 0.0
    sweep: float = 0.0

    def __init__(self, pid: int, layer: int, center, radius: float, start: float,
                 sweep: float):
        super().__init__(pid, layer)
        self.center, self.radius = (float(center[0]), float(center[1])), float(radius)
        self.start, self.sweep = _norm(start), float(sweep)

    def point(self, pos: float) -> tuple[float, float]:
        dx, dy = direction(pos)
        return self.center[0] + self.radius * dx, self.center[1] + self.radius * dy

    def ends(self) -> list[End]:
        end = self.start + self.sweep
        return [(*self.point(self.start), _norm(self.start - 90.0)),
                (*self.point(end), _norm(end + 90.0))]

    def length(self) -> float:
        return self.radius * math.radians(self.sweep)


@dataclass(eq=False)
class Turnout(Piece):
    defn: TurnoutDef | None = None
    origin: tuple[float, float] = (0.0, 0.0)
    angle: float = 0.0

    def __init__(self, pid: int, layer: int, defn: TurnoutDef, origin, angle: float):
        super().__init__(pid, layer)
        self.defn, self.origin, self.angle = defn, (float(origin[0]), float(origin[1])), \
            _norm(angle)

    @classmethod
    def at_points(cls, pid: int, layer: int, defn: TurnoutDef, points, heading: float):
        """A turnout with its points at ``points``, local x (points to frog)
        toward compass ``heading``."""
        return cls(pid, layer, defn, points, heading - 90.0)

    def to_world(self, x: float, y: float) -> tuple[float, float]:
        ax, ay = direction(self.angle + 90.0)    # local x
        bx, by = direction(self.angle)           # local y
        return self.origin[0] + x * ax + y * bx, self.origin[1] + x * ay + y * by

    def ends(self) -> list[End]:
        return [(*self.to_world(x, y), _norm(a + self.angle))
                for x, y, a in self.defn.endpoints]


# -- the model --------------------------------------------------------------------

class TrackModel:
    """Pieces of track and how their ends join."""

    def __init__(self, scale: str):
        self.scale = scale
        self.pieces: list[Piece] = []
        self._links: dict[tuple[int, int], tuple[int, int]] = {}

    def add(self, piece: Piece) -> Piece:
        piece.id = len(self.pieces)
        self.pieces.append(piece)
        return piece

    def connect_all(self) -> None:
        """Join every two ends at one point facing opposite ways."""
        self._links.clear()
        ends = [(p.id, i, e) for p in self.pieces for i, e in enumerate(p.ends())]
        for n, (pa, ia, ea) in enumerate(ends):
            for pb, ib, eb in ends[n + 1:]:
                if pa == pb or (pa, ia) in self._links or (pb, ib) in self._links:
                    continue
                if (math.dist(ea[:2], eb[:2]) < _EPS
                        and abs((ea[2] - eb[2]) % 360.0 - 180.0) < 0.01):
                    self._links[(pa, ia)] = (pb, ib)
                    self._links[(pb, ib)] = (pa, ia)

    def connected_to(self, piece: Piece, end: int) -> tuple[Piece, int] | None:
        link = self._links.get((piece.id, end))
        return (self.pieces[link[0]], link[1]) if link else None

    def open_ends(self) -> list[tuple[Piece, int, End]]:
        return [(p, i, e) for p in self.pieces for i, e in enumerate(p.ends())
                if (p.id, i) not in self._links]

    # -- .xtc records --------------------------------------------------------

    def xtc_records(self, first_id: int) -> list[str]:
        """The pieces as .xtc track records, numbered from ``first_id``."""
        num = {p.id: first_id + n for n, p in enumerate(self.pieces)}
        lines: list[str] = []
        for p in self.pieces:
            tid = num[p.id]
            if isinstance(p, Straight):
                lines.append(f"STRAIGHT {tid} {p.layer} 0 0 0 {self.scale} 2 "
                             "0.000000 0.000000")
            elif isinstance(p, Curve):
                lines.append(f"CURVE {tid} {p.layer} 0 0 0 {self.scale} 2 "
                             f"{p.center[0]:.6f} {p.center[1]:.6f} 0 {p.radius:.6f} 0 "
                             "0.000000 0.000000")
            elif isinstance(p, Turnout):
                lines.append(f"TURNOUT {tid} {p.layer} 0 0 0 {self.scale} 2 "
                             f"{p.origin[0]:.6f} {p.origin[1]:.6f} 0 {p.angle:.6f} "
                             f"\"{p.defn.title}\"")
            for i, (x, y, a) in enumerate(p.ends()):
                link = self._links.get((p.id, i))
                head = f"T4 {num[link[0]]}" if link else "E4"
                lines.append(f"\t{head} {x:.6f} {y:.6f} {a:.6f} 0 0.0 0.0 0.0 0.0 0 0 0 "
                             "0.000000 ")
            if isinstance(p, Turnout):
                lines.append("\tD 0.000000 0.000000")
                lines += [f"\t{ln}" for ln in p.defn.paths + p.defn.segments]
            lines.append("\tEND$SEGS")
        return lines
