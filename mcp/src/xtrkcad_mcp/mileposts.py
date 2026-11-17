"""Mileposts and station limits (Operating Data Conventions v1, spec 5).

milepost_model() computes, for every MP line (a subdivision, or a railroad
without subdivisions, or the whole layout), the MP of each station, its
limits, and where industries and team tracks leave the main. The v2 export
uses the same model; check_mileposts() reports the rules that depend on it.

MPs: within a line they increase away from the anchor with ``mp: 0`` (or,
for a line without a zero, from its lowest anchor). The MP at a point is
the MP of the last anchor passed plus the main-track distance since it,
divided by the MP scale; an anchor met on the way resets the count.
Distances follow main track only (the line's Mainline layers). A switch is
measured at its points.
"""

from __future__ import annotations

import heapq
import math
from dataclasses import dataclass, field

from .conventions import (
    SNAP_WARN,
    ConventionNote,
    Issue,
    _group_layers,
    _is_number,
    _note_issue,
    _track_polylines,
    layer_categories,
    main_turnouts_from,
    read_convention_notes,
    snap_note,
)
from .models import Layout, TrackObject

# Layout inches per MP unit of each mp_unit (spec 4.2).
UNIT_INCHES = {"in": 1.0, "ft": 12.0, "cm": 1.0 / 2.54, "m": 100.0 / 2.54}

_PATH_TOLERANCE = 0.5     # layout inches: "this anchor lies on the shortest path"
_MP_EQUAL = 0.005         # MP units: a calculated MP equal to an anchor's

Node = tuple[int, int]    # (track id, endpoint index)


@dataclass
class Anchor:
    note: ConventionNote
    track: TrackObject
    mp: float


@dataclass
class Line:
    """One MP line: a subdivision, a railroad without subdivisions, or the
    whole layout when it has neither."""
    railroad: str | None
    subdivision: str | None
    layers: set[int]
    anchors: list[Anchor] = field(default_factory=list)
    scale_in: float | None = None        # layout inches per MP unit
    origin: Anchor | None = None
    _dist: dict[int, dict[Node, float]] = field(default_factory=dict, repr=False)

    def key(self) -> tuple[str | None, str | None]:
        return (self.railroad, self.subdivision)


@dataclass
class StationMP:
    id: str
    note_id: int
    railroad: str | None
    subdivision: str | None
    mp: float | None
    mp_entry: float | None
    mp_exit: float | None
    limits: str                          # "siding" | "switchback" | "note"
    switch_ids: list[int] = field(default_factory=list)
    terminus: bool = False


@dataclass
class MilepostModel:
    lines: list[Line]
    stations: list[StationMP]
    branch_mp: dict[str, float | None]   # industry / team track id -> MP of its switch
    boards: dict[tuple, list[float]]     # (railroad, subdivision, yard limit id) -> board MPs
    issues: list[Issue]


# -- the main-track graph ----------------------------------------------------

def _polyline_length(line: list[tuple[float, float]]) -> float:
    return sum(math.hypot(x1 - x0, y1 - y0) for (x0, y0), (x1, y1) in zip(line, line[1:]))


def _legs(track: TrackObject) -> list[tuple[int, list[tuple[float, float]]]]:
    """(other endpoint index, polyline from endpoint 0 to it) per leg."""
    lines = _track_polylines(track)
    if track.kind == "TURNOUT":
        return list(zip(range(1, len(track.endpoints)), lines))
    return [(1, lines[0])] if lines else []


def _along(track: TrackObject, x: float, y: float) -> dict[int, float]:
    """Distance along the track from (x, y)'s projection to each endpoint of
    the leg it lies on."""
    best: tuple[float, int, float, float] | None = None   # perp, k, s, length
    for k, line in _legs(track):
        s_acc = 0.0
        length = _polyline_length(line)
        for (x0, y0), (x1, y1) in zip(line, line[1:]):
            dx, dy = x1 - x0, y1 - y0
            seg = math.hypot(dx, dy)
            t = 0.0 if seg < 1e-12 else max(0.0, min(1.0, ((x - x0) * dx + (y - y0) * dy) / (seg * seg)))
            perp = math.hypot(x - (x0 + t * dx), y - (y0 + t * dy))
            if best is None or perp < best[0]:
                best = (perp, k, s_acc + t * seg, length)
            s_acc += seg
    if best is None:
        return {}
    _, k, s, length = best
    return {0: s, k: length - s}


def _build_graph(layout: Layout, layers: set[int]) -> dict[Node, list[tuple[Node, float]]]:
    """Endpoint graph of the tracks on the given layers. A turnout joins its
    points (endpoint 0) to each other end, never one leg to another."""
    by_id = {t.id: t for t in layout.tracks if t.layer in layers}
    adj: dict[Node, list[tuple[Node, float]]] = {}

    def edge(a: Node, b: Node, w: float) -> None:
        adj.setdefault(a, []).append((b, w))
        adj.setdefault(b, []).append((a, w))

    for track in by_id.values():
        for k, line in _legs(track):
            edge((track.id, 0), (track.id, k), _polyline_length(line))
        for i, ep in enumerate(track.endpoints):
            other = by_id.get(ep.connected_to) if ep.connected_to is not None else None
            if other is None:
                continue
            j = min(range(len(other.endpoints)), key=lambda n: math.hypot(
                other.endpoints[n].x - ep.x, other.endpoints[n].y - ep.y))
            edge((track.id, i), (other.id, j), 0.0)
    return adj


def _dijkstra(adj: dict[Node, list[tuple[Node, float]]],
              seeds: dict[Node, float]) -> dict[Node, float]:
    dist = dict(seeds)
    heap = [(d, n) for n, d in seeds.items()]
    heapq.heapify(heap)
    while heap:
        d, node = heapq.heappop(heap)
        if d > dist.get(node, math.inf):
            continue
        for nxt, w in adj.get(node, ()):
            nd = d + w
            if nd < dist.get(nxt, math.inf):
                dist[nxt] = nd
                heapq.heappush(heap, (nd, nxt))
    return dist


@dataclass
class _Point:
    """A place on main track: a track and its distances to its endpoints."""
    track: TrackObject
    along: dict[int, float]


def _point(track: TrackObject, x: float, y: float) -> _Point:
    return _Point(track, _along(track, x, y))


def _node_point(track: TrackObject, ep_idx: int) -> _Point:
    return _Point(track, {ep_idx: 0.0})


def _distance(dist: dict[Node, float], src: _Point, dst: _Point) -> float:
    """Main-track distance from src (whose Dijkstra result is dist) to dst."""
    best = min((dist.get((dst.track.id, i), math.inf) + a for i, a in dst.along.items()),
               default=math.inf)
    if src.track.id == dst.track.id:
        for i, a in src.along.items():
            if i in dst.along:
                best = min(best, abs(a - dst.along[i]))
    return best


class _Measurer:
    """MPs on one line."""

    def __init__(self, layout: Layout, line: Line):
        self.line = line
        self.adj = _build_graph(layout, line.layers)
        self.points = {id(a): _point(a.track, a.note.x, a.note.y) for a in line.anchors}
        self.dists = {id(a): _dijkstra(self.adj, {(a.track.id, i): d for i, d in
                                                  self.points[id(a)].along.items()})
                      for a in line.anchors}

    def _dist(self, anchor: Anchor, p: _Point) -> float:
        return _distance(self.dists[id(anchor)], self.points[id(anchor)], p)

    def previous_anchor(self, p: _Point, exclude: Anchor | None = None) -> Anchor | None:
        """The last anchor passed on the way from the origin to p."""
        origin = self.line.origin
        d_origin = self._dist(origin, p)
        if math.isinf(d_origin):
            return None
        best, best_d = origin, -1.0
        for a in self.line.anchors:
            if a is exclude:
                continue
            d_oa = self._dist(origin, self.points[id(a)])
            if math.isinf(d_oa) or d_oa <= best_d:
                continue
            if abs(d_oa + self._dist(a, p) - d_origin) <= _PATH_TOLERANCE:
                best, best_d = a, d_oa
        return best

    def mp(self, p: _Point) -> float | None:
        if self.line.origin is None or self.line.scale_in is None:
            return None
        a = self.previous_anchor(p)
        if a is None:
            return None
        return a.mp + self._dist(a, p) / self.line.scale_in

    def mp_at(self, track: TrackObject, x: float, y: float) -> float | None:
        if track.layer not in self.line.layers:
            return None
        return self.mp(_point(track, x, y))

    def mp_of_switch(self, turnout: TrackObject) -> float | None:
        """A switch's MP: at its points (endpoint 0)."""
        if turnout.layer not in self.line.layers:
            return None
        return self.mp(_node_point(turnout, 0))


# -- lines and their anchors -------------------------------------------------

def _lines(layout: Layout, notes: list[ConventionNote]) -> list[Line]:
    cats = layer_categories(layout)
    main = {idx for idx, (cat, _) in cats.items() if cat == "mainline"}
    groups = _group_layers(layout)
    railroads = {n.id: n for n in notes if n.kind == "railroad"}
    subs = [n for n in notes if n.kind == "subdivision"]

    def layers_of(note: ConventionNote | None) -> set[int] | None:
        if note is None or note.str_field("group") is None:
            return None
        return groups.get(note.str_field("group").lower())

    lines = []
    for sub in subs:
        layers = layers_of(sub) or layers_of(railroads.get(sub.railroad)) or main
        lines.append(Line(sub.railroad, sub.id, layers & main))
    with_subs = {s.railroad for s in subs}
    for rr in railroads.values():
        if rr.id not in with_subs:
            lines.append(Line(rr.id, None, (layers_of(rr) or main) & main))
    if not lines:
        lines.append(Line(None, None, set(main)))
    return lines


def _line_of(note: ConventionNote, lines: list[Line]) -> Line | None:
    if len(lines) == 1:
        return lines[0]
    if note.subdivision is not None:
        hits = [ln for ln in lines if ln.subdivision == note.subdivision]
    else:
        hits = [ln for ln in lines if ln.subdivision is None and ln.railroad == note.railroad]
    return hits[0] if len(hits) == 1 else None


def _scale_in(note: ConventionNote) -> float | None:
    scale = note.fields.get("mp_scale")
    if not (_is_number(scale) and scale > 0):
        return None
    unit = note.fields.get("mp_unit", "in")
    return scale * UNIT_INCHES[unit] if unit in UNIT_INCHES else None


def _setup_line(layout: Layout, line: Line, notes: list[ConventionNote],
                snapped: dict[int, TrackObject], issues: list[Issue]) -> None:
    for note in notes:
        track = snapped.get(note.note_id)
        if (note.kind == "reference" and _is_number(note.fields.get("mp"))
                and track is not None and track.layer in line.layers):
            line.anchors.append(Anchor(note, track, float(note.fields["mp"])))
    if not line.anchors:
        if any(t.layer in line.layers for t in layout.tracks):
            hint = "Add a reference note with \"mp\" (0 at MP zero) on its main track."
            if any(n.kind == "reference" and "mp" not in n.fields and "mp_scale" in n.fields
                   for n in notes):
                ref = next(n for n in notes if n.kind == "reference"
                           and "mp" not in n.fields and "mp_scale" in n.fields)
                hint = (f"Reference {ref.id} has mp_scale but no \"mp\": "
                        f"add \"mp\": 0 to make it the MP zero anchor.")
            who = line.subdivision or line.railroad or "The layout"
            issues.append(Issue("error", "MP_NO_ANCHOR",
                                f"{who} has main track but no MP anchor", hint=hint))
        return

    scales = {}
    for a in line.anchors:
        s = _scale_in(a.note)
        if s is not None:
            scales.setdefault(round(s, 6), a)
    if not scales:
        issues.append(_note_issue(
            "error", "MP_NO_SCALE",
            f"MP anchors of {line.subdivision or line.railroad or 'the layout'} "
            f"have no mp_scale", line.anchors[0].note,
            hint='Give "mp_scale" (and "mp_unit") on one anchor, normally the first.'))
    elif len(scales) > 1:
        extra = list(scales.values())[1]
        issues.append(_note_issue(
            "error", "MP_TWO_SCALES",
            f"Two MP scales in {line.subdivision or line.railroad or 'the layout'}",
            extra.note, hint="One mp_scale per subdivision."))
    else:
        line.scale_in = next(iter(scales))

    zeros = [a for a in line.anchors if a.mp == 0]
    line.origin = zeros[0] if zeros else min(line.anchors, key=lambda a: a.mp)


def _check_anchors(m: _Measurer, issues: list[Issue]) -> None:
    line = m.line
    if line.scale_in is None:
        return
    for a in line.anchors:
        if a is line.origin:
            continue
        p = m.points[id(a)]
        prev = m.previous_anchor(p, exclude=a)
        if prev is None:
            issues.append(_note_issue(
                "warning", "MP_UNREACHABLE",
                f"MP anchor {a.note.id} can't be reached along the main from "
                f"{line.origin.note.id}", a.note))
            continue
        calc = prev.mp + m._dist(prev, p) / line.scale_in
        if abs(calc - a.mp) > _MP_EQUAL:
            issues.append(_note_issue(
                "info", "MP_ANCHOR_RESET",
                f"MP anchor {a.note.id} resets the count: calculated {calc:.2f}, "
                f"anchor {a.mp:g}", a.note))
        if a.mp < calc - _MP_EQUAL:
            issues.append(_note_issue(
                "warning", "MP_DECREASES",
                f"MP anchor {a.note.id} ({a.mp:g}) is lower than the MP calculated "
                f"there ({calc:.2f}): the count goes backwards", a.note,
                hint="MPs increase away from MP zero."))


# -- stations ----------------------------------------------------------------

def _passing_sidings(layout: Layout, line: Line) -> list[list[TrackObject]]:
    """For each connected group of passing-track tracks, the line's main
    turnouts it joins; only groups joining exactly two are sidings."""
    cats = layer_categories(layout)
    passing = {t.id: t for t in layout.tracks if cats.get(t.layer, ("",))[0] == "passing"}
    by_id = {t.id: t for t in layout.tracks}
    seen: set[int] = set()
    sidings = []
    for start in passing.values():
        if start.id in seen:
            continue
        stack, turnouts = [start], {}
        seen.add(start.id)
        while stack:
            track = stack.pop()
            for ep in track.endpoints:
                nxt = by_id.get(ep.connected_to) if ep.connected_to is not None else None
                if nxt is None:
                    continue
                if nxt.id in passing:
                    if nxt.id not in seen:
                        seen.add(nxt.id)
                        stack.append(nxt)
                elif nxt.kind == "TURNOUT" and nxt.layer in line.layers:
                    turnouts[nxt.id] = nxt
        if len(turnouts) == 2:
            sidings.append(list(turnouts.values()))
    return sidings


def _station_mp(m: _Measurer, layout: Layout, note: ConventionNote, track: TrackObject,
                sidings: list[tuple[float, float, list[int]]],
                tail_switches: list[TrackObject] | None,
                refs: dict[str, tuple[TrackObject, float, float]]) -> StationMP:
    at_note = m.mp_at(track, note.x, note.y)
    tag = note.str_field("ref_tag")
    if at_note is None and tag in refs:
        # Spec 4.3: a note on an isolated track routes from its named point.
        ref_track, rx, ry = refs[tag]
        at_note = m.mp_at(ref_track, rx, ry)
    siding = None
    if at_note is not None:
        tol = SNAP_WARN / m.line.scale_in
        fits = [s for s in sidings if s[0] - tol <= at_note <= s[1] + tol]
        siding = min(fits, key=lambda s: s[1] - s[0], default=None)

    mp = lo = hi = at_note
    limits, switch_ids = "note", []
    tails = [(m.mp_of_switch(t), t.id) for t in tail_switches or ()]
    tails = [(t_mp, tid) for t_mp, tid in tails if t_mp is not None]
    if tails:
        # Switchback: from the entry switch (the siding's lower switch, or
        # the tail switch itself) to the tail switch.
        tail_mp, tail_id = min(tails)
        entry = siding[0] if siding is not None else tail_mp
        lo, hi = sorted((entry, tail_mp))
        mp = (lo + hi) / 2
        limits = "switchback"
        switch_ids = (list(siding[2]) if siding is not None else []) + [tail_id]
    elif siding is not None:
        lo, hi, switch_ids = siding[0], siding[1], list(siding[2])
        mp = (lo + hi) / 2
        limits = "siding"
    if note.fields.get("mp_at_note") is True:
        mp = at_note
    return StationMP(note.id, note.note_id, note.railroad, note.subdivision, mp, lo, hi,
                     limits, switch_ids, terminus=note.fields.get("terminus") is True)


def _route_hint(layout: Layout, line: Line, start: TrackObject, goal: TrackObject) -> str:
    """Why goal can't be reached along the main from start: the off-main
    tracks on the route that uses fewest of them."""
    by_id = {t.id: t for t in layout.tracks}
    cost = {start.id: 0}
    prev: dict[int, int | None] = {start.id: None}
    heap = [(0, start.id)]
    while heap:
        c, tid = heapq.heappop(heap)
        if tid == goal.id:
            break
        if c > cost[tid]:
            continue
        for ep in by_id[tid].endpoints:
            nxt = by_id.get(ep.connected_to) if ep.connected_to is not None else None
            if nxt is None:
                continue
            nc = c + (0 if nxt.layer in line.layers else 1)
            if nc < cost.get(nxt.id, math.inf):
                cost[nxt.id] = nc
                prev[nxt.id] = tid
                heapq.heappush(heap, (nc, nxt.id))
    if goal.id not in prev:
        return ("Its track isn't connected to the main at all; give \"ref_tag\" naming a "
                "reference note on the main.")
    off: list[TrackObject] = []
    tid = goal.id
    while tid is not None:
        if by_id[tid].layer not in line.layers:
            off.append(by_id[tid])
        tid = prev[tid]
    names = {}
    for t in reversed(off):
        names.setdefault(layout.layers[t.layer].name if t.layer in layout.layers
                         else f"Layer {t.layer}", []).append(str(t.id))
    where = "; ".join(f"{', '.join(ids)} on {layer}" for layer, ids in names.items())
    return (f"The main is broken by tracks on other layers: {where}. Move them to a "
            f"Mainline layer if they are main track, or give \"ref_tag\".")


def milepost_model(layout: Layout, notes: list[ConventionNote] | None = None,
                   snapped: dict[int, TrackObject] | None = None) -> MilepostModel:
    """Compute MPs, station limits and branch MPs for the whole layout."""
    if notes is None:
        notes, _ = read_convention_notes(layout)
    if snapped is None:
        snapped = {}
        for note in notes:
            if note.kind in ("railroad", "subdivision"):
                continue
            track, dist = snap_note(layout, note)
            if track is not None:
                snapped[note.note_id] = track
    issues: list[Issue] = []
    lines = _lines(layout, notes)
    for line in lines:
        _setup_line(layout, line, notes, snapped, issues)

    measurers = {line.key(): _Measurer(layout, line) for line in lines if line.anchors}
    for m in measurers.values():
        _check_anchors(m, issues)

    cats = layer_categories(layout)
    main = {idx for idx, (cat, _) in cats.items() if cat == "mainline"}
    by_id = {t.id: t for t in layout.tracks}

    tails: dict[str, list[TrackObject]] = {}
    for note in notes:
        if note.kind != "switchback" or note.str_field("serves") is None:
            continue
        track = snapped.get(note.note_id)
        if track is None:
            continue
        given = note.fields.get("switches")
        if isinstance(given, list) and given:
            found = [by_id[s] for s in given if isinstance(s, int) and s in by_id]
        else:
            found = main_turnouts_from(layout, track, main)
        tails.setdefault(note.str_field("serves"), []).extend(found)

    refs = {n.id: (snapped[n.note_id], n.x, n.y) for n in notes
            if n.kind == "reference" and n.note_id in snapped}
    stations: list[StationMP] = []
    sidings_by_line: dict = {}
    for note in notes:
        if note.kind != "station":
            continue
        track = snapped.get(note.note_id)
        line = _line_of(note, lines)
        m = measurers.get(line.key()) if line is not None else None
        if track is None or m is None or line.scale_in is None:
            continue
        if line.key() not in sidings_by_line:
            sidings_by_line[line.key()] = []
            for pair in _passing_sidings(layout, line):
                mps = [(m.mp_of_switch(t), t.id) for t in pair]
                if all(mp is not None for mp, _ in mps):
                    (a, ia), (b, ib) = sorted(mps)
                    sidings_by_line[line.key()].append((a, b, [ia, ib]))
        st = _station_mp(m, layout, note, track, sidings_by_line[line.key()],
                         tails.get(note.id), refs)
        if st.mp is None:
            issues.append(_note_issue(
                "warning", "MP_UNREACHABLE",
                f"Station {note.id} can't be reached along the main from "
                f"{line.origin.note.id}", note,
                hint=_route_hint(layout, line, line.origin.track, track)))
        stations.append(st)

    branch_mp: dict[str, float | None] = {}
    for note in notes:
        if note.kind not in ("industry", "team_track"):
            continue
        track = snapped.get(note.note_id)
        line = _line_of(note, lines)
        m = measurers.get(line.key()) if line is not None else None
        if track is None or m is None:
            continue
        mps = [mp for mp in (m.mp_of_switch(t) for t in main_turnouts_from(layout, track, main))
               if mp is not None]
        branch_mp[note.id] = min(mps) if mps else None
        if branch_mp[note.id] is None and note.kind == "team_track" \
                and note.str_field("station") is None:
            issues.append(_note_issue(
                "warning", "MP_UNREACHABLE",
                f"Team track {note.id} (a timetable station) has no MP: its track "
                f"doesn't reach the main", note))

    boards: dict[tuple, list[float]] = {}
    for note in notes:
        if note.kind != "yard_limit":
            continue
        track = snapped.get(note.note_id)
        line = _line_of(note, lines)
        m = measurers.get(line.key()) if line is not None else None
        if track is None or m is None:
            continue
        mp = m.mp_at(track, note.x, note.y)
        if mp is not None:
            boards.setdefault(line.key() + (note.id,), []).append(mp)

    return MilepostModel(lines, stations, branch_mp, boards, issues)


def check_mileposts(layout: Layout, notes: list[ConventionNote],
                    snapped: dict[int, TrackObject]) -> list[Issue]:
    """The milepost and station-limit rules (spec 4.2, 4.3, 5)."""
    model = milepost_model(layout, notes, snapped)
    issues = list(model.issues)
    for st in model.stations:
        if st.mp is None:
            continue
        note = next(n for n in notes if n.note_id == st.note_id)
        line = _line_of(note, model.lines)
        zones = [(min(v), max(v)) for k, v in model.boards.items() if k[:2] == line.key()]
        tol = SNAP_WARN / line.scale_in
        if not any(lo - tol <= st.mp_exit and st.mp_entry <= hi + tol for lo, hi in zones):
            issues.append(_note_issue(
                "warning", "STATION_NO_YARD_LIMIT",
                f"Station {st.id} (MP {st.mp:.2f}) has no yard-limit board within its "
                f"limits", note, hint="Usual, not required: YL boards at the station's "
                                      "switches let local moves work."))
    return issues
