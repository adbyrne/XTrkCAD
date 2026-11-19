"""Capacity between clearance points, for any layout.

A car standing on a track near the turnout it leaves fouls the turnout's
other leg, until the two tracks are ``clearance`` apart: 13 ft between
centres on the prototype (US practice), scaled. A track's capacity is its
length less that fouled part at each turnout it leaves through a leg (the
track beyond a turnout's points is shared by both routes: nothing to clear).

A turnout is taken as straight chords from its points to each leg end (the
parser keeps its ends, not its curve); for the usual turnouts that is within
a few hundredths of an inch of the real diverging route where it matters.
generate_track_plan builds tracks to hold their length between clearance
points, so on a generated plan the two agree.
"""

from __future__ import annotations

import math

from .models import SCALE_RATIOS, Layout, TrackObject, _normalize_angle, arc_polyline

CLEARANCE_FT = 13.0
_STEP = 0.05             # sampling step along a track, layout inches
_REACH = 48.0            # how far the other leg's track is followed, inches


def default_clearance_in(scale: str) -> float:
    """13 ft between track centres, in the layout's scale."""
    return CLEARANCE_FT * 12.0 / SCALE_RATIOS.get(scale, 87.1)


def _oriented(track: TrackObject, entry: int) -> list[tuple[float, float]]:
    """The track's centreline from endpoint ``entry`` to its other end."""
    eps = track.endpoints
    if track.kind == "CURVE" and track.extra.get("radius", 0.0) > 0.0:
        arc_deg = _normalize_angle(eps[1].angle - eps[0].angle + 180.0)
        pts = arc_polyline(track.extra["cx"], track.extra["cy"], track.extra["radius"],
                           eps[0].x, eps[0].y, eps[1].x, eps[1].y, arc_deg, n=64)
    else:
        pts = [(eps[0].x, eps[0].y), (eps[1].x, eps[1].y)]
    return pts if entry == 0 else list(reversed(pts))


def _segments_distance(px: float, py: float, segs) -> float:
    best = math.inf
    for (x0, y0), (x1, y1) in segs:
        dx, dy = x1 - x0, y1 - y0
        sq = dx * dx + dy * dy
        t = 0.0 if sq < 1e-12 else max(0.0, min(1.0, ((px - x0) * dx + (py - y0) * dy) / sq))
        best = min(best, math.hypot(px - (x0 + t * dx), py - (y0 + t * dy)))
    return best


def _entry_index(track: TrackObject, other_id: int) -> int | None:
    return next((i for i, ep in enumerate(track.endpoints) if ep.connected_to == other_id),
                None)


def _turnout_through(track: TrackObject, ent: int) -> tuple[list, int] | None:
    """A turnout's straight route from endpoint ``ent``: its chord and the
    endpoint it leaves by (points <-> first leg; the diverging leg back to
    the points)."""
    if len(track.endpoints) < 2:
        return None
    out = 1 if ent == 0 else 0
    a, b = track.endpoints[ent], track.endpoints[out]
    return [(a.x, a.y), (b.x, b.y)], out


def _follow(layout: Layout, by_id: dict, start: TrackObject, entry: int,
            reach: float, allowed: set[int] | None = None, through: bool = False):
    """Walk from ``start`` (entered at endpoint ``entry``) along connected
    tracks: yields (track, its centreline from the entry) until ``reach``
    inches are covered or the line ends. It stops at a turnout, or with
    ``through`` carries on along the turnout's straight route (the line a
    car fouls doesn't stop at the next switch)."""
    covered, track, ent, seen = 0.0, start, entry, set()
    while track is not None and track.id not in seen and covered < reach:
        seen.add(track.id)
        if track.kind == "TURNOUT":
            route = _turnout_through(track, ent)
            if route is None:
                return
            pts, out_i = route
        else:
            if len(track.endpoints) != 2:
                return
            pts, out_i = _oriented(track, ent), 1 - ent
        yield track, pts
        covered += sum(math.dist(a, b) for a, b in zip(pts, pts[1:]))
        out = track.endpoints[out_i]
        nxt = by_id.get(out.connected_to) if out.connected_to is not None else None
        if nxt is None or (nxt.kind == "TURNOUT" and not through) or (
                allowed is not None and nxt.id not in allowed):
            return
        ent = _entry_index(nxt, track.id)
        if ent is None:
            return
        track = nxt


def fouled_length(layout: Layout, turnout: TrackObject, first: TrackObject,
                  clearance: float, allowed: set[int] | None = None) -> float:
    """How much of the track starting with ``first`` (beyond ``turnout``)
    is within ``clearance`` of the turnout's other leg(s) and the track
    beyond them: where a car fouls them. 0 for track beyond the points."""
    by_id = {t.id: t for t in layout.tracks}
    leg = _entry_index(turnout, first.id)
    if leg is None or leg == 0 or len(turnout.endpoints) < 3:
        return 0.0
    pts0 = turnout.endpoints[0]
    others = []
    for j, ep in enumerate(turnout.endpoints):
        if j in (0, leg):
            continue
        others.append(((pts0.x, pts0.y), (ep.x, ep.y)))
        beyond = by_id.get(ep.connected_to) if ep.connected_to is not None else None
        if beyond is not None:
            ent = _entry_index(beyond, turnout.id)
            if ent is not None:
                for _t, pts in _follow(layout, by_id, beyond, ent, _REACH, through=True):
                    others += list(zip(pts, pts[1:]))
    entry = _entry_index(first, turnout.id)
    if entry is None:
        return 0.0
    def clear_at(a, b, f):
        return _segments_distance(a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f,
                                  others) >= clearance

    walked = 0.0
    for _t, pts in _follow(layout, by_id, first, entry, math.inf, allowed):
        for a, b in zip(pts, pts[1:]):
            seg = math.dist(a, b)
            n = max(1, int(seg / _STEP))
            for k in range(n + 1):
                if clear_at(a, b, k / n):
                    lo, hi = max(0, k - 1) / n, k / n      # refine the crossing
                    for _ in range(30):
                        mid = (lo + hi) / 2
                        lo, hi = (lo, mid) if clear_at(a, b, mid) else (mid, hi)
                    return walked + seg * hi
            walked += seg
    return walked


def clear_usable_length(layout: Layout, start_id: int,
                        clearance: float | None = None) -> float:
    """What the track ``start_id`` is part of holds: its connected
    non-turnout tracks, less the fouled part at each turnout they leave."""
    by_id = {t.id: t for t in layout.tracks}
    start = by_id.get(start_id)
    if start is None:
        return 0.0
    if clearance is None:
        clearance = default_clearance_in(layout.scale or "HO")
    comp: list[TrackObject] = []
    seen: set[int] = set()
    stack = [start] if start.kind != "TURNOUT" else []
    while stack:
        t = stack.pop()
        if t.id in seen:
            continue
        seen.add(t.id)
        comp.append(t)
        for ep in t.endpoints:
            nxt = by_id.get(ep.connected_to) if ep.connected_to is not None else None
            if nxt is not None and nxt.kind != "TURNOUT" and nxt.id not in seen:
                stack.append(nxt)
    total = sum(t.length_model_inches() for t in comp)
    fouled = 0.0
    for t in comp:
        for ep in t.endpoints:
            nxt = by_id.get(ep.connected_to) if ep.connected_to is not None else None
            if nxt is not None and nxt.kind == "TURNOUT":
                fouled += fouled_length(layout, nxt, t, clearance, allowed=seen)
    return max(0.0, total - fouled)
