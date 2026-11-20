"""generate_track_plan: lay a track plan along the benchwork shelves.

A track plan (YAML, see mcp/docs/layout-config-guide.md) names the shelves
the main runs along, in order, plus a stub yard at the start and stations
along the way: a passing siding (or, at the end of the line, a terminus
runaround) and spurs. The planner works out the geometry and builds a
trackgeom.TrackModel; it says what doesn't fit, and by how much, instead of
guessing.

Route coordinates: s is the distance along the main from its start; the
lateral offset v is positive to the left of the direction of travel. A
station's parts are laid out in station coordinates u along its shelf.

v1 limits: shelves are axis-aligned rectangles along a wall, consecutive
shelves meet at a right angle, one station per shelf, turnouts only on
straight track.
"""

from __future__ import annotations

import math
import re
from dataclasses import dataclass, field

from .config import LayoutConfig, _parse_length_in
from .models import SCALE_RATIOS, cars_per_real_ft
from .trackgeom import (Curve, Straight, TrackModel, Turnout, TurnoutDef, TurnoutPair,
                        compass, direction, find_turnouts)

_WALL_TOL = 0.5          # a shelf edge this close to the room edge is on the wall
_GAP = 0.5               # clear track between two turnouts, inches
_END_GAP = 2.0           # between two tracks ending face to face (HO; scaled)
_END_MARGIN = 1.0        # the line ends this far short of a shelf end


class PlanError(ValueError):
    """The track plan itself is wrong (unknown key, shelf, bad length)."""


# -- the plan as written ---------------------------------------------------------

@dataclass
class SpurSpec:
    id: str
    name: str | None
    kind: str              # industry | team_track | facility
    type: str | None       # facility type
    length_in: float
    side: str              # wall | room | left | right
    points_toward: str     # start | end


@dataclass
class StationSpec:
    id: str
    name: str | None
    shelf: str
    at_in: float | None
    siding_in: float
    siding_side: str
    terminus: bool
    tail_in: float
    spurs: list[SpurSpec] = field(default_factory=list)


@dataclass
class YardSpec:
    id: str
    name: str | None
    tracks: int
    track_length_in: float
    side: str


@dataclass
class TrackPlanSpec:
    level: int
    curve_radius_in: float
    turnout: str
    track_offset_in: float | None
    track_spacing_in: float
    clearance_in: float     # track centres this far apart clear each other
    car_in: float
    route: list[str]
    yard: YardSpec | None
    stations: list[StationSpec]
    notes: bool


_PLAN_KEYS = {"level", "curve_radius", "turnout", "track_offset", "track_spacing",
              "clearance", "route", "yard", "stations", "notes"}
_CLEARANCE_FT = 13.0     # prototype track centres at a clearance point (US practice)
_YARD_KEYS = {"id", "name", "tracks", "track_length", "side"}
_STATION_KEYS = {"id", "name", "shelf", "at", "siding", "siding_side", "terminus", "tail",
                 "spurs"}
_SPUR_KEYS = {"id", "name", "kind", "type", "length", "side", "points_toward"}
_SPUR_KINDS = ("industry", "team_track", "facility")
_SIDES = ("wall", "room", "left", "right")


def _check_keys(raw: dict, allowed: set[str], where: str) -> None:
    unknown = sorted(set(raw) - allowed)
    if unknown:
        raise PlanError(f"{where}: unknown key(s) {', '.join(unknown)}; "
                        f"known: {', '.join(sorted(allowed))}")


def _length(value, car_in: float, what: str) -> float:
    text = str(value).strip().lower()
    m = re.fullmatch(r"([0-9.]+)\s*cars?", text)
    try:
        inches = float(m.group(1)) * car_in if m else _parse_length_in(text)
    except ValueError:
        raise PlanError(f"{what}: {value!r} is not a length (in, ft or cars)") from None
    if inches <= 0:
        raise PlanError(f"{what}: {value!r} must be above 0")
    return inches


def _side(value, what: str, default: str) -> str:
    side = str(value if value is not None else default).lower()
    if side not in _SIDES:
        raise PlanError(f"{what}: side {value!r} is not one of {', '.join(_SIDES)}")
    return side


def parse_track_plan(raw: dict, config: LayoutConfig) -> TrackPlanSpec:
    """Read a track plan (the YAML as a dict) against the layout config."""
    if not isinstance(raw, dict):
        raise PlanError("track plan: expected a mapping")
    _check_keys(raw, _PLAN_KEYS, "track plan")
    if raw.get("notes"):
        raise PlanError("track plan: notes: true (writing the convention notes too) "
                        "isn't available yet; leave it out")
    scale = config.scale or "HO"
    car_in = 12.0 / cars_per_real_ft(scale)
    ratio = SCALE_RATIOS.get(scale, 87.1)
    level = int(raw.get("level", 1))
    shelves = {b.label: b for b in config.benchwork_sections}
    route = [str(s) for s in raw.get("route") or []]
    if not route:
        raise PlanError("track plan: route is empty; list the shelves the main runs along")
    for label in route:
        if label not in shelves:
            raise PlanError(f"route: no benchwork section {label!r}; sections: "
                            f"{', '.join(shelves)}")
        if shelves[label].level != level:
            raise PlanError(f"route: {label!r} is on level {shelves[label].level}, "
                            f"the plan is on level {level}")
    radius = raw.get("curve_radius")
    radius_in = (_length(radius, car_in, "curve_radius") if radius is not None
                 else config.curve_radius_in)
    if not radius_in:
        raise PlanError("track plan: no curve_radius (here or in the layout config)")
    offset = raw.get("track_offset")
    spacing = raw.get("track_spacing")
    clearance = raw.get("clearance")

    yard = None
    if raw.get("yard") is not None:
        y = raw["yard"]
        _check_keys(y, _YARD_KEYS, "yard")
        yard = YardSpec(str(y.get("id", "YARD")), y.get("name"), int(y.get("tracks", 2)),
                        _length(y.get("track_length", "6 cars"), car_in, "yard track_length"),
                        _side(y.get("side"), "yard", "room"))
        if yard.tracks < 1:
            raise PlanError("yard: tracks must be 1 or more")

    stations = []
    for st in raw.get("stations") or []:
        _check_keys(st, _STATION_KEYS, f"station {st.get('id', '?')}")
        sid = str(st.get("id") or "")
        if not sid:
            raise PlanError("station: every station needs an id")
        shelf = str(st.get("shelf") or "")
        if shelf not in route:
            raise PlanError(f"station {sid}: shelf {shelf!r} is not on the route")
        spurs = []
        for sp in st.get("spurs") or []:
            _check_keys(sp, _SPUR_KEYS, f"spur {sp.get('id', '?')}")
            kind = str(sp.get("kind", "industry"))
            if kind not in _SPUR_KINDS:
                raise PlanError(f"spur {sp.get('id')}: kind {kind!r} is not one of "
                                f"{', '.join(_SPUR_KINDS)}")
            toward = str(sp.get("points_toward", "start"))
            if toward not in ("start", "end"):
                raise PlanError(f"spur {sp.get('id')}: points_toward must be start or end")
            spurs.append(SpurSpec(str(sp.get("id")), sp.get("name"), kind, sp.get("type"),
                                  _length(sp.get("length", "2 cars"), car_in,
                                          f"spur {sp.get('id')} length"),
                                  _side(sp.get("side"), f"spur {sp.get('id')}", "wall"),
                                  toward))
        at = st.get("at")
        stations.append(StationSpec(
            sid, st.get("name"), shelf,
            _length(at, car_in, f"station {sid} at") if at is not None else None,
            _length(st.get("siding", "5 cars"), car_in, f"station {sid} siding"),
            _side(st.get("siding_side"), f"station {sid}", "room"),
            bool(st.get("terminus", False)),
            _length(st.get("tail", "2 cars"), car_in, f"station {sid} tail"),
            spurs))
    by_shelf: dict[str, str] = {}
    for st in stations:
        if st.shelf in by_shelf:
            raise PlanError(f"stations {by_shelf[st.shelf]} and {st.id}: one station per "
                            f"shelf for now ({st.shelf})")
        by_shelf[st.shelf] = st.id
        if st.terminus and st.shelf != route[-1]:
            raise PlanError(f"station {st.id}: a terminus must be on the last shelf of "
                            f"the route ({route[-1]})")
    return TrackPlanSpec(
        level=level, curve_radius_in=radius_in,
        turnout=str(raw.get("turnout") or "PECO Code 83 #6"),
        track_offset_in=_length(offset, car_in, "track_offset") if offset is not None
        else None,
        track_spacing_in=_length(spacing, car_in, "track_spacing") if spacing is not None
        else 2.0 * 87.1 / ratio,
        clearance_in=_length(clearance, car_in, "clearance") if clearance is not None
        else _CLEARANCE_FT * 12.0 / ratio,
        car_in=car_in, route=route, yard=yard, stations=stations,
        notes=bool(raw.get("notes", False)))


# -- the route -----------------------------------------------------------------------

@dataclass
class Run:
    """The main's straight run along one shelf, s0..s1 on the route."""
    shelf: str
    s0: float
    s1: float
    origin: tuple[float, float]     # the point at s0
    heading: float
    wall_side: str                  # left | right of the direction of travel
    room_space: float               # how far track may go toward the room
    wall_space: float               # ... and toward the wall


@dataclass
class Corner:
    s0: float
    s1: float
    center: tuple[float, float]
    radius: float
    turn: str                       # right (clockwise) | left
    pos0: float                     # compass position of the start, seen from the centre


@dataclass
class Route:
    runs: list[Run]
    corners: list[Corner]
    length: float

    def run(self, shelf: str) -> Run:
        return next(r for r in self.runs if r.shelf == shelf)

    def point_at(self, s: float) -> tuple[float, float, float]:
        """(x, y, heading) of the main at s."""
        for r in self.runs:
            if r.s0 - 1e-9 <= s <= r.s1 + 1e-9:
                dx, dy = direction(r.heading)
                return r.origin[0] + (s - r.s0) * dx, r.origin[1] + (s - r.s0) * dy, r.heading
        for c in self.corners:
            if c.s0 <= s <= c.s1:
                turned = math.degrees((s - c.s0) / c.radius)
                pos = c.pos0 + turned if c.turn == "right" else c.pos0 - turned
                dx, dy = direction(pos)
                heading = pos + 90.0 if c.turn == "right" else pos - 90.0
                return (c.center[0] + c.radius * dx, c.center[1] + c.radius * dy,
                        heading % 360.0)
        raise ValueError(f"s={s} is off the route (0..{self.length})")

    def at(self, run: Run, s: float, v: float) -> tuple[float, float]:
        """World point at s on a run, v to the left."""
        dx, dy = direction(run.heading)
        lx, ly = direction(run.heading - 90.0)
        return (run.origin[0] + (s - run.s0) * dx + v * lx,
                run.origin[1] + (s - run.s0) * dy + v * ly)


@dataclass
class _Line:
    shelf: str
    vertical: bool
    coord: float            # x of a vertical line, y of a horizontal one
    lo: float
    hi: float
    wall: tuple[float, float]   # unit vector toward the wall
    depth: float
    offset: float


def _shelf_line(bw, room_w: float, room_h: float, offset: float | None) -> _Line:
    xs, ys = [v[0] for v in bw.vertices], [v[1] for v in bw.vertices]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    vertical = (y1 - y0) >= (x1 - x0)
    depth = (x1 - x0) if vertical else (y1 - y0)
    off = depth / 2.0 if offset is None else offset
    if vertical:
        if abs(x0) <= _WALL_TOL:
            return _Line(bw.label, True, x0 + off, y0, y1, (-1.0, 0.0), depth, off)
        if abs(x1 - room_w) <= _WALL_TOL:
            return _Line(bw.label, True, x1 - off, y0, y1, (1.0, 0.0), depth, off)
    else:
        if abs(y0) <= _WALL_TOL:
            return _Line(bw.label, False, y0 + off, x0, x1, (0.0, -1.0), depth, off)
        if abs(y1 - room_h) <= _WALL_TOL:
            return _Line(bw.label, False, y1 - off, x0, x1, (0.0, 1.0), depth, off)
    raise PlanError(f"shelf {bw.label!r} is not along a wall of the room")


def _room_size(config: LayoutConfig) -> tuple[float, float]:
    fp = config.floor_plan
    if fp is not None and fp.total_width_in:
        return fp.total_width_in, fp.total_depth_in
    return config.room_width_ft * 12.0, config.room_depth_ft * 12.0


def _end_point(line: _Line, at_lo: bool) -> tuple[float, float]:
    t = line.lo + _END_MARGIN if at_lo else line.hi - _END_MARGIN
    return (line.coord, t) if line.vertical else (t, line.coord)


def plan_route(spec: TrackPlanSpec, config: LayoutConfig) -> Route:
    """The main along the route's shelves, with a curve at each corner."""
    shelves = {b.label: b for b in config.benchwork_sections}
    room_w, room_h = _room_size(config)
    lines = [_shelf_line(shelves[s], room_w, room_h, spec.track_offset_in)
             for s in spec.route]
    radius = spec.curve_radius_in
    # Corner points between consecutive shelves.
    corners_p: list[tuple[float, float]] = []
    for a, b in zip(lines, lines[1:]):
        if a.vertical == b.vertical:
            raise PlanError(f"shelves {a.shelf} and {b.shelf} don't meet at a right angle")
        corners_p.append((a.coord, b.coord) if a.vertical else (b.coord, a.coord))

    def along(line: _Line, p: tuple[float, float]) -> float:
        return p[1] if line.vertical else p[0]

    # Each shelf's start and end points, in travel order.
    ends: list[tuple[tuple[float, float], tuple[float, float]]] = []
    for i, line in enumerate(lines):
        if len(lines) == 1:
            start, end = _end_point(line, True), _end_point(line, False)
        elif i == 0:
            p = corners_p[0]
            start = _end_point(line, abs(along(line, p) - line.lo) > abs(along(line, p) - line.hi))
            end = p
        else:
            start = corners_p[i - 1]
            if i == len(lines) - 1:
                p = start
                end = _end_point(line, abs(along(line, p) - line.lo) > abs(along(line, p) - line.hi))
            else:
                end = corners_p[i]
        ends.append((start, end))

    runs: list[Run] = []
    corners: list[Corner] = []
    s = 0.0
    for i, (line, (start, end)) in enumerate(zip(lines, ends)):
        heading = compass(end[0] - start[0], end[1] - start[1])
        d = direction(heading)
        trim0 = radius if i > 0 else 0.0
        trim1 = radius if i < len(lines) - 1 else 0.0
        p0 = (start[0] + trim0 * d[0], start[1] + trim0 * d[1])
        span = math.dist(start, end) - trim0 - trim1
        if span < 0:
            raise PlanError(f"shelf {line.shelf}: too short for its corner curves "
                            f"({math.dist(start, end):.1f} in, needs {trim0 + trim1:.1f} in)")
        lx, ly = direction(heading - 90.0)
        wall_side = "left" if (lx * line.wall[0] + ly * line.wall[1]) > 0 else "right"
        runs.append(Run(line.shelf, s, s + span, p0, heading, wall_side,
                        room_space=line.depth - line.offset, wall_space=line.offset))
        s += span
        if i < len(lines) - 1:
            nxt_start, nxt_end = ends[i + 1]
            h2 = compass(nxt_end[0] - nxt_start[0], nxt_end[1] - nxt_start[1])
            turn = "right" if (h2 - heading) % 360.0 < 180.0 else "left"
            ta = (end[0] - radius * d[0], end[1] - radius * d[1])
            n = direction(heading + 90.0) if turn == "right" else direction(heading - 90.0)
            center = (ta[0] + radius * n[0], ta[1] + radius * n[1])
            pos0 = compass(ta[0] - center[0], ta[1] - center[1])
            arc = radius * math.radians(90.0)
            corners.append(Corner(s, s + arc, center, radius, turn, pos0))
            s += arc
    return Route(runs, corners, s)


# -- building ---------------------------------------------------------------------------

def offset_run_length(defn: TurnoutDef, spacing: float, radius: float) -> float:
    """Along-track length from a turnout's points to where its diverging
    track runs parallel at ``spacing``: the turnout, a straight at the frog
    angle, a curve of ``radius`` back to parallel."""
    return _offset_run(defn, spacing, radius)[0]


def _offset_run(defn: TurnoutDef, spacing: float, radius: float):
    """(along, straight, radius used) for the offset run."""
    theta = math.radians(defn.frog_angle)
    dy = abs(defn.diverge_offset)
    curve_gain = radius * (1.0 - math.cos(theta))
    rest = spacing - dy - curve_gain
    if rest >= 0:
        straight = rest / math.sin(theta)
        r = radius
    else:
        straight = 0.0
        r = (spacing - dy) / (1.0 - math.cos(theta))
        if r < radius:
            raise PlanError(f"track spacing {spacing:g} in is too small for a "
                            f"{radius:g} in radius after this turnout")
    along = defn.diverge_length + straight * math.cos(theta) + r * math.sin(theta)
    return along, straight, r


def _diverging_samples(defn: TurnoutDef, step: float = 0.02) -> list[tuple[float, float]]:
    """Points along a turnout's diverging route, in its local frame (x from
    the points toward the frog, |y| off the straight route), from the
    definition's own segments."""
    segs = defn.segments
    best: list[tuple[float, float]] = []
    for line in defn.paths:
        ids = [int(t) for t in re.findall(r"\s(\d+)", line.split('"')[-1]) if int(t) > 0]
        pts: list[tuple[float, float]] = []
        for i in ids:
            if i > len(segs):
                continue
            w = segs[i - 1].split()
            if w[0] == "S":
                x0, y0, x1, y1 = map(float, w[3:7])
                n = max(2, int(math.dist((x0, y0), (x1, y1)) / step))
                pts += [(x0 + (x1 - x0) * k / n, abs(y0 + (y1 - y0) * k / n))
                        for k in range(n + 1)]
            elif w[0] == "C":
                r, cx, cy, a0, sw = abs(float(w[3])), float(w[4]), float(w[5]), \
                    float(w[6]), float(w[7])
                n = max(2, int(r * math.radians(sw) / step))
                for k in range(n + 1):
                    dx, dy = direction(a0 + sw * k / n)
                    pts.append((cx + r * dx, abs(cy + r * dy)))
        if pts and max(y for _x, y in pts) > max((y for _x, y in best), default=-1.0):
            best = pts
    return sorted(best)


def clearance_point(defn: TurnoutDef, clearance: float, spacing: float,
                    radius: float) -> float:
    """How far from a turnout's points (along the straight route) its
    diverging track is ``clearance`` from the straight one: through the
    turnout, then the run out at the frog angle and the curve back to
    parallel at ``spacing``. At or beyond ``spacing``, where they run
    parallel."""
    return _clearance_run(defn, clearance, spacing, radius)[0]


def clearance_tail(defn: TurnoutDef, clearance: float, spacing: float,
                   radius: float) -> float:
    """Track length, measured along the diverging track, from its clearance
    point to where it runs parallel: the part of the run out that holds
    cars. The check and the export measure capacity along the track, so
    the planner builds to this."""
    return _clearance_run(defn, clearance, spacing, radius)[1]


def straight_clearance_point(defn: TurnoutDef, clearance: float, spacing: float,
                             radius: float) -> float:
    """How far from a turnout's points the *straight* track is clear of the
    diverging one: where the nearest distance from it to the diverging track
    reaches ``clearance``. The diverging track curves back toward it, so this
    is a little beyond clearance_point() (the diverging track's own offset
    there), which is what the export measures a straight track by."""
    xs = clearance_point(defn, clearance, spacing, radius)
    pts = _diverging_path(defn, spacing, radius)
    segs = list(zip(pts, pts[1:]))

    def nearest(x: float) -> float:
        best = math.inf
        for (x0, y0), (x1, y1) in segs:
            dx, dy = x1 - x0, y1 - y0
            sq = dx * dx + dy * dy
            t = 0.0 if sq < 1e-12 else max(0.0, min(1.0, ((x - x0) * dx - y0 * dy) / sq))
            best = min(best, math.hypot(x - (x0 + t * dx), -(y0 + t * dy)))
        return best

    while nearest(xs) < clearance - 1e-9 and xs < pts[-1][0] + 1.0:
        xs += 0.002
    return xs


def _diverging_path(defn: TurnoutDef, spacing: float, radius: float):
    """The diverging track as points (along the straight route, offset from
    it): through the turnout, the run out at the frog angle, the curve back
    to parallel at ``spacing``."""
    _along, straight, r = _offset_run(defn, spacing, radius)
    th = math.radians(defn.frog_angle)
    xd, yd = defn.diverge_length, abs(defn.diverge_offset)
    pts = _diverging_samples(defn)
    pts += [(xd + t * math.cos(th), yd + t * math.sin(th))
            for t in [straight * k / 200 for k in range(201)]]
    x2 = xd + straight * math.cos(th) + r * math.sin(th)
    pts += [(x2 - r * math.sin(phi), spacing - r * (1.0 - math.cos(phi)))
            for phi in [th * (1 - k / 200) for k in range(201)]]
    return pts


def _clearance_run(defn: TurnoutDef, clearance: float, spacing: float,
                   radius: float) -> tuple[float, float]:
    along, straight, r = _offset_run(defn, spacing, radius)
    if clearance >= spacing - 1e-9:
        return along, 0.0
    th = math.radians(defn.frog_angle)
    xd, yd = defn.diverge_length, abs(defn.diverge_offset)
    pts = _diverging_samples(defn)
    pts += [(xd + t * math.cos(th), yd + t * math.sin(th))
            for t in [straight * k / 200 for k in range(201)]]
    x2 = xd + straight * math.cos(th) + r * math.sin(th)
    pts += [(x2 - r * math.sin(phi), spacing - r * (1.0 - math.cos(phi)))
            for phi in [th * (1 - k / 200) for k in range(201)]]
    # Path length from each point to the end (where the track runs parallel).
    rest = [0.0] * len(pts)
    for i in range(len(pts) - 2, -1, -1):
        rest[i] = rest[i + 1] + math.dist(pts[i], pts[i + 1])
    for i, (x, y) in enumerate(pts):
        if y >= clearance:
            if i == 0 or y == pts[i - 1][1]:
                return x, rest[i]
            f = (clearance - pts[i - 1][1]) / (y - pts[i - 1][1])
            xc = pts[i - 1][0] + (x - pts[i - 1][0]) * f
            return xc, rest[i] + math.dist(pts[i], pts[i - 1]) * (1 - f)
    return along, 0.0


def _curve_between(pid, layer, center, radius, pa, pb, sweep_deg):
    """A curve through two points; ends stored clockwise as XTrkCAD does."""
    a = compass(pa[0] - center[0], pa[1] - center[1])
    b = compass(pb[0] - center[0], pb[1] - center[1])
    start = a if abs(((b - a) % 360.0) - sweep_deg) < 1e-6 else b
    return Curve(pid, layer, center, radius, start, sweep_deg)


@dataclass
class PlanResult:
    model: TrackModel | None
    features: list[dict]
    errors: list[str]
    warnings: list[str] = field(default_factory=list)
    route: Route | None = None

    def feature(self, ident: str) -> dict:
        return next(f for f in self.features if f["id"] == ident)


class _Builder:
    def __init__(self, spec: TrackPlanSpec, route: Route, turnouts: TurnoutPair,
                 layers: dict[str, int], scale: str):
        self.spec, self.route, self.t = spec, route, turnouts
        self.model = TrackModel(scale)
        lv = spec.level
        self.layer = {cat: layers[f"L{lv}-{cat}"] for cat in
                      ("Main", "Passing", "Storage", "Service")
                      if f"L{lv}-{cat}" in layers}
        self.L = turnouts.left.length
        self.fp, self.straight, self.r = _offset_run(turnouts.left, spec.track_spacing_in,
                                                     spec.curve_radius_in)
        self.theta = turnouts.left.frog_angle
        self.clearance_x = clearance_point(turnouts.left, spec.clearance_in,
                                           spec.track_spacing_in, spec.curve_radius_in)
        # Track from the clearance point to where it runs parallel, along the
        # track (what holds cars there); xc is where that starts, measured
        # along the main as the layout is laid out.
        self.tail = clearance_tail(turnouts.left, spec.clearance_in, spec.track_spacing_in,
                                   spec.curve_radius_in)
        self.xc = self.fp - self.tail
        # Where the straight track beside a diverging one is clear of it
        # (nearest distance), along the main: the yard's straight tracks.
        self.xs = straight_clearance_point(turnouts.left, spec.clearance_in,
                                           spec.track_spacing_in, spec.curve_radius_in)
        self.main_turnouts: list[tuple[float, float]] = []   # [a, b] main intervals
        self.errors: list[str] = []

    def lat(self, run: Run, side: str) -> float:
        """+1 for left, -1 for right of travel."""
        if side in ("wall", "room"):
            side = run.wall_side if side == "wall" else (
                "right" if run.wall_side == "left" else "left")
        return 1.0 if side == "left" else -1.0

    def space(self, run: Run, sign: float) -> float:
        left_is_wall = run.wall_side == "left"
        return run.wall_space if (sign > 0) == left_is_wall else run.room_space

    def check_edge(self, ident: str, run: Run, sign: float, offset: float) -> None:
        edge = 1.5 * 87.1 / SCALE_RATIOS.get(self.model.scale, 87.1)
        room = self.space(run, sign) - edge
        if offset > room + 1e-9:
            self.errors.append(f"{ident}: track {offset:.1f} in from the main would leave "
                               f"shelf {run.shelf} ({room:.1f} in of shelf on that side)")

    def turnout(self, run: Run, s: float, v: float, dirn: int, sign: float,
                layer: int) -> Turnout:
        """A turnout with its points at (s, v), frog toward dirn (+1 = the
        route's end), diverging to sign (+1 left of travel)."""
        heading = run.heading if dirn > 0 else (run.heading + 180.0) % 360.0
        hand_left = (sign > 0) == (dirn > 0)
        defn = self.t.left if hand_left else self.t.right
        return self.model.add(Turnout.at_points(0, layer, defn, self.route.at(run, s, v),
                                                heading))

    def offset_run(self, run: Run, turnout: Turnout, s: float, v: float, dirn: int,
                   sign: float, layer: int) -> tuple[float, float]:
        """Track from a turnout's diverging leg to parallel, spacing further
        to ``sign``; returns (s, v) where the parallel track starts."""
        defn = turnout.defn
        th = math.radians(self.theta)
        u_d = s + dirn * defn.diverge_length
        v_d = v + sign * abs(defn.diverge_offset)
        pd = turnout.ends()[2][:2]
        u1 = u_d + dirn * self.straight * math.cos(th)
        v1 = v_d + sign * self.straight * math.sin(th)
        if self.straight > 1e-9:
            self.model.add(Straight(0, layer, pd, self.route.at(run, u1, v1)))
        u2 = u1 + dirn * self.r * math.sin(th)
        v2 = v1 + sign * self.r * (1.0 - math.cos(th))
        center = self.route.at(run, u2, v2 - sign * self.r)
        self.model.add(_curve_between(0, layer, center, self.r, self.route.at(run, u1, v1),
                                      self.route.at(run, u2, v2), self.theta))
        return u2, v2

    def main_turnout(self, a: float, b: float) -> None:
        self.main_turnouts.append((min(a, b), max(a, b)))


def _pack_station(st: StationSpec, fp: float, L: float,
                  end_gap: float = _END_GAP, xc: float | None = None) -> tuple[float, list, float]:
    """Lay out a station's parts in u: the siding (or runaround) at 0, then
    each spur where it adds least to the station's length. Lengths are
    between clearance points (``xc`` from a turnout's points). Returns
    (width, placements [(spur, u_points, dirn)], siding width)."""
    xc = fp if xc is None else xc
    width_s = st.siding_in + 2.0 * xc
    main: list[tuple[float, float]] = [(0.0, L), (width_s - L, width_s)]
    sides: dict[str, list[tuple[float, float]]] = {"siding": [(0.0, width_s)]}
    lo, hi = 0.0, width_s + (st.tail_in if st.terminus else 0.0)
    if st.terminus:
        main.append((width_s, hi))          # the tail stays plain track
    placements = []

    def free(iv, taken, gap=_GAP):
        return all(iv[1] + gap <= a or iv[0] >= b + gap for a, b in taken)

    for sp in st.spurs:
        span = xc + sp.length_in
        best = None
        # Where the spur's turnout butts against a turnout already on the
        # main, first: a tie goes to them, so no short filler straight is
        # left between two turnouts (XTrkCAD's Short Tracks report lists
        # those). Then every 0.25 in.
        start = sp.points_toward == "start"
        butts = [(e if start else e - span + L) for _a, e in main] + \
                [(a - L if start else a - span) for a, _e in main]
        grid, u = [], lo - span - 2 * end_gap
        while u <= hi + 2 * end_gap:
            grid.append(u)
            u += 0.25
        for u in butts + grid:
            side_iv = (u, u + span)
            main_iv = (u, u + L) if start else (u + span - L, u + span)
            if st.terminus and side_iv[1] > hi:
                continue
            key = sp.side
            taken_side = sides.get(key, []) + (sides["siding"] if key == st.siding_side else [])
            if free(main_iv, main, 0.0) and free(side_iv, taken_side, end_gap):
                ext = max(hi, side_iv[1]) - min(lo, side_iv[0])
                if best is None or ext < best[0] - 1e-9:
                    best = (ext, u, main_iv, side_iv)
        if best is None:
            raise PlanError(f"spur {sp.id}: no room beside station {st.id}")
        _ext, u, main_iv, side_iv = best
        main.append(main_iv)
        sides.setdefault(sp.side, []).append(side_iv)
        lo, hi = min(lo, side_iv[0]), max(hi, side_iv[1])
        placements.append((sp, u, main_iv))
    return hi - lo, [(sp, u - lo, (a - lo, b - lo)) for sp, u, (a, b) in placements], -lo


def build_track_plan(spec: TrackPlanSpec, config: LayoutConfig,
                     layers: dict[str, int]) -> PlanResult:
    """Build the track plan. ``layers`` maps the target file's layer names
    to their indexes (L1-Main, L1-Passing, ...)."""
    scale = config.scale or "HO"
    route = plan_route(spec, config)
    turnouts = find_turnouts(spec.turnout, scale)
    missing = [f"L{spec.level}-{c}" for c in ("Main", "Passing", "Storage", "Service")
               if f"L{spec.level}-{c}" not in layers]
    if missing:
        return PlanResult(None, [], [f"the layout has no layer {', '.join(missing)}; "
                                     "generate it with generate_layout first"], route=route)
    b = _Builder(spec, route, turnouts, layers, scale)
    features: list[dict] = []
    car = spec.car_in
    main_start, main_end = 0.0, route.length

    # Yard at the start of the route.
    if spec.yard is not None:
        y = spec.yard
        run = route.runs[0]
        sign = b.lat(run, y.side)
        # Track k's points are (k-1) offset runs out from the throat; each
        # track holds cars from its clearance point with the next track out,
        # so the last two tracks are the shortest.
        # The last two tracks are the shortest: the straight one is clear a
        # little later (xs) than the diverging one (xc).
        s_y = run.s0 + y.track_length_in + (
            (y.tracks - 2) * b.fp + max(b.xc, b.xs) if y.tracks > 1 else 0.0)
        if s_y > run.s1:
            b.errors.append(f"yard {y.id}: needs {s_y - run.s0:.1f} in on shelf {run.shelf}, "
                            f"which has {run.s1 - run.s0:.1f} in")
        else:
            b.check_edge(f"yard {y.id}", run, sign, (y.tracks - 1) * spec.track_spacing_in)
            tracks = []
            s_k, v_k = s_y, 0.0
            for k in range(1, y.tracks + 1):
                if k < y.tracks:
                    layer = b.layer["Main"] if k == 1 else b.layer["Storage"]
                    t = b.turnout(run, s_k, v_k, -1, sign, layer)
                    if k == 1:
                        b.main_turnout(s_k, s_k - b.L)
                    s_next, v_next = b.offset_run(run, t, s_k, v_k, -1, sign,
                                                  b.layer["Storage"])
                    b.model.add(Straight(0, b.layer["Storage"], t.ends()[1][:2],
                                         route.at(run, run.s0, v_k)))
                    usable = s_y - (k - 1) * b.fp - b.xs - run.s0
                    s_k, v_k = s_next, v_next
                else:
                    if y.tracks > 1:
                        b.model.add(Straight(0, b.layer["Storage"], route.at(run, s_k, v_k),
                                             route.at(run, run.s0, v_k)))
                    else:
                        b.model.add(Straight(0, b.layer["Storage"], route.at(run, s_y, 0.0),
                                             route.at(run, run.s0, 0.0)))
                    usable = (s_y - (y.tracks - 2) * b.fp - b.xc - run.s0
                              if y.tracks > 1 else y.track_length_in)
                tracks.append({"label": f"Track {k}", "capacity_in": round(usable, 2),
                               "capacity_cars": int(usable / car + 1e-9)})
            features.append({"id": y.id, "kind": "yard", "name": y.name, "shelf": run.shelf,
                             "tracks": tracks, "throat": route.at(run, s_y, 0.0)})
            main_start = s_y

    # Stations.
    for st in spec.stations:
        run = route.run(st.shelf)
        lo = max(run.s0, main_start + _GAP) if run is route.runs[0] else run.s0
        hi = run.s1
        try:
            width, spurs, siding_u = _pack_station(
                st, b.fp, b.L, _END_GAP * 87.1 / SCALE_RATIOS.get(scale, 87.1), b.xc)
        except PlanError as e:
            b.errors.append(str(e))
            continue
        if st.terminus:
            u0 = run.s1 - width
        elif st.at_in is not None:
            u0 = run.s0 + st.at_in
        else:
            u0 = lo + (hi - lo - width) / 2.0
        if u0 < lo - 1e-9 or u0 + width > hi + 1e-9:
            b.errors.append(f"station {st.id}: needs {width:.1f} in on shelf {st.shelf}, "
                            f"{hi - lo:.1f} in free there")
            continue
        sign = b.lat(run, st.siding_side)
        b.check_edge(f"station {st.id}", run, sign, spec.track_spacing_in)
        clear = st.siding_in - 2.0 * b.tail
        if clear < 0:
            b.errors.append(f"station {st.id}: a {st.siding_in:.1f} in siding is shorter than "
                            f"its two curves beyond the clearance points "
                            f"({2.0 * b.tail:.1f} in)")
            continue
        s_a = u0 + siding_u
        s_b = s_a + st.siding_in + 2.0 * b.xc
        ta = b.turnout(run, s_a, 0.0, +1, sign, b.layer["Main"])
        tb = b.turnout(run, s_b, 0.0, -1, sign, b.layer["Main"])
        b.main_turnout(s_a, s_a + b.L)
        b.main_turnout(s_b, s_b - b.L)
        pa = b.offset_run(run, ta, s_a, 0.0, +1, sign, b.layer["Passing"])
        pb = b.offset_run(run, tb, s_b, 0.0, -1, sign, b.layer["Passing"])
        b.model.add(Straight(0, b.layer["Passing"], route.at(run, *pa), route.at(run, *pb)))
        features.append({
            "id": st.id, "kind": "station", "name": st.name, "shelf": st.shelf,
            "capacity_in": round(st.siding_in, 2),
            "capacity_cars": int(st.siding_in / car + 1e-9),
            "clear_in": round(clear, 2), "clearance_in": round(b.clearance_x, 2),
            "run_in": round(b.fp, 2), "terminus": st.terminus,
            "tail_in": round(route.length - s_b, 2) if st.terminus else None,
            "switches": [route.at(run, s_a, 0.0), route.at(run, s_b, 0.0)],
        })
        if st.terminus:
            main_end = route.length
        for sp, u, (ma, mb) in spurs:
            sign_s = b.lat(run, sp.side)
            b.check_edge(f"spur {sp.id}", run, sign_s, spec.track_spacing_in)
            parallel = sp.length_in - b.tail
            if parallel <= 1e-9:
                b.errors.append(f"spur {sp.id}: {sp.length_in:.1f} in is no longer than its "
                                f"curve beyond the clearance point ({b.tail:.1f} in)")
                continue
            dirn = +1 if sp.points_toward == "start" else -1
            s_p = u0 + (ma if dirn > 0 else mb)
            layer = b.layer["Service"] if sp.kind == "facility" else b.layer["Storage"]
            t = b.turnout(run, s_p, 0.0, dirn, sign_s, b.layer["Main"])
            b.main_turnout(s_p, s_p + dirn * b.L)
            s2, v2 = b.offset_run(run, t, s_p, 0.0, dirn, sign_s, layer)
            end = (s2 + dirn * parallel, v2)
            b.model.add(Straight(0, layer, route.at(run, s2, v2), route.at(run, *end)))
            features.append({"id": sp.id, "kind": sp.kind, "name": sp.name, "type": sp.type,
                             "station": st.id, "capacity_in": round(sp.length_in, 2),
                             "capacity_cars": int(sp.length_in / car + 1e-9),
                             "clear_in": round(parallel, 2),
                             "clearance_in": round(b.clearance_x, 2),
                             "run_in": round(b.fp, 2), "layer": layer,
                             "side": sp.side, "end": route.at(run, *end)})

    if b.errors:
        return PlanResult(None, features, b.errors, route=route)

    # The main between the turnouts and around the corners.
    stops = sorted(b.main_turnouts)
    cursor = main_start
    pieces_s: list[tuple[float, float]] = []
    for a, c in stops:
        if a > cursor + 1e-9:
            pieces_s.append((cursor, a))
        cursor = max(cursor, c)
    if main_end > cursor + 1e-9:
        pieces_s.append((cursor, main_end))
    bounds = sorted({r.s0 for r in route.runs} | {r.s1 for r in route.runs})
    for a, c in pieces_s:
        cuts = [a] + [x for x in bounds if a + 1e-9 < x < c - 1e-9] + [c]
        for p, q in zip(cuts, cuts[1:]):
            corner = next((k for k in route.corners
                           if k.s0 - 1e-9 <= p and q <= k.s1 + 1e-9), None)
            pa, pq = route.point_at(p)[:2], route.point_at(q)[:2]
            if corner is None:
                b.model.add(Straight(0, b.layer["Main"], pa, pq))
            else:
                sweep = math.degrees((q - p) / corner.radius)
                b.model.add(_curve_between(0, b.layer["Main"], corner.center, corner.radius,
                                           pa, pq, sweep))
    b.model.connect_all()
    return PlanResult(b.model, features, [], route=route)
