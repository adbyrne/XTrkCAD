"""layout_data.json v2: the general, versioned export of a layout's operating
data (Operating Data Conventions spec v1, section 8).

Everything comes from the layout itself: convention JSON Notes, Layer Groups
and track geometry. Lengths are prototype feet unless a field says ``_in``
(layout inches); mileposts are in MP units. The file always carries the
convention check's ``issues``; the tool refuses to write it while there are
errors unless forced.

The v1 export (``stations.build_layout_export``) stays as it is until
IOTtrains imports v2.
"""

from __future__ import annotations

import datetime
import math

from .conventions import (CONVENTIONS_VERSION, LOCATION_KINDS, ConventionNote,
                          Issue, _main_layers, check_conventions, layer_categories,
                          main_turnouts_from, read_convention_notes,
                          resolve_ownership, snap_note)
from .mileposts import Line, MilepostModel, _lines, _Measurer, _setup_line
from .models import Layout, TrackObject, cars_per_real_ft
from .stations import _bfs_usable_length, _snap_to_segment, model_in_to_proto_ft

SCHEMA_VERSION = 2
CAR_SPOTS_DEFAULT = 2            # spec 4.5


class _Context:
    """What every section of the export reads: the notes, the tracks they
    snap to, the MP lines and the milepost model."""

    def __init__(self, layout: Layout):
        from .mileposts import milepost_model
        self.layout = layout
        self.notes, _ = read_convention_notes(layout)
        resolve_ownership(layout, self.notes)
        self.snapped: dict[int, TrackObject] = {}
        for note in self.notes:
            if note.kind in ("railroad", "subdivision"):
                continue
            track, _dist = snap_note(layout, note)
            if track is not None:
                self.snapped[note.note_id] = track
        self.lines: list[Line] = _lines(layout, self.notes)
        for line in self.lines:
            _setup_line(layout, line, self.notes, self.snapped, [])
        self.model: MilepostModel = milepost_model(layout, self.notes, self.snapped)
        self.measurers = {ln.key(): _Measurer(layout, ln) for ln in self.lines if ln.anchors}
        self.by_id = {t.id: t for t in layout.tracks}
        self.categories = layer_categories(layout)
        self.scale = layout.scale or "HO"
        self.location_kind = {n.id: n.kind for n in self.notes if n.kind in LOCATION_KINDS}

    def feet(self, inches: float) -> float:
        return round(model_in_to_proto_ft(inches, self.scale), 3)

    def cars(self, inches: float) -> int:
        """Fugate: whole cars per model foot, as the capacity reports count."""
        return int(inches / 12.0 * cars_per_real_ft(self.scale))

    def usable_in(self, note: ConventionNote) -> tuple[int | None, float | None]:
        """The track a note marks and its usable length: the connected
        non-turnout tracks, switch throats excluded."""
        track = self.snapped.get(note.note_id)
        if track is None:
            return None, None
        if track.kind == "TURNOUT":
            tid, _t, _d = _snap_to_segment(self.layout, note.x, note.y, note.layer,
                                           exclude_turnouts=True)
            if tid < 0:
                return track.id, None
            track = self.by_id[tid]
        return track.id, _bfs_usable_length(self.layout, track.id)

    def of_kind(self, kind: str) -> list[ConventionNote]:
        return [n for n in self.notes if n.kind == kind]


def _round(value: float | None, places: int = 3) -> float | None:
    return round(value, places) if value is not None else None


def _subdivision(line: Line, note: ConventionNote | None) -> dict:
    """One subdivisions[] entry: the MP line's scale and anchors."""
    scale_note = next((a.note for a in line.anchors if "mp_scale" in a.note.fields), None)
    return {
        "id": note.id if note is not None else None,
        "name": note.str_field("name") if note is not None else None,
        "mp_scale": scale_note.fields["mp_scale"] if scale_note is not None else None,
        "mp_unit": scale_note.fields.get("mp_unit", "in") if scale_note is not None else None,
        "anchors": [{"id": a.note.id, "mp": float(a.mp), "track_id": a.track.id}
                    for a in sorted(line.anchors, key=lambda a: a.mp)],
    }


def _railroads(ctx: _Context) -> list[dict]:
    subs = {n.id: n for n in ctx.of_kind("subdivision")}
    out = []
    for rr in ctx.of_kind("railroad"):
        lines = [ln for ln in ctx.lines if ln.railroad == rr.id]
        out.append({
            "id": rr.id,
            "name": rr.str_field("name"),
            "reporting_marks": rr.str_field("reporting_marks"),
            "subdivisions": [_subdivision(ln, subs.get(ln.subdivision)) for ln in lines],
        })
    return out


def _mp_key(value: float | None) -> float:
    return value if value is not None else float("inf")


def _siding(ctx: _Context, switch_ids: list[int]) -> tuple[float, list[int]] | None:
    """The passing track joining two main switches, at least one of them
    the station's: its length and the two switches. One is enough for a
    terminus, whose runaround starts at its terminus switch. None when the
    station has no siding."""
    switches = set(switch_ids)
    passing = {t.id for t in ctx.layout.tracks
               if ctx.categories.get(t.layer, ("",))[0] == "passing"}
    seen: set[int] = set()
    for start in sorted(passing):
        if start in seen:
            continue
        stack, comp, ends = [start], [], set()
        seen.add(start)
        while stack:
            track = ctx.by_id[stack.pop()]
            comp.append(track)
            for ep in track.endpoints:
                nxt = ctx.by_id.get(ep.connected_to) if ep.connected_to is not None else None
                if nxt is None:
                    continue
                if nxt.id in passing and nxt.id not in seen:
                    seen.add(nxt.id)
                    stack.append(nxt.id)
                elif (nxt.kind == "TURNOUT"
                      and ctx.categories.get(nxt.layer, ("",))[0] == "mainline"):
                    ends.add(nxt.id)
        if len(ends) == 2 and ends & switches:
            # Turnouts on the passing track itself (spurs off the siding)
            # count: cars stand on their through route. The station's main
            # switches are on the main, so they are not in comp.
            return sum(t.length_model_inches() for t in comp), sorted(ends)
    return None


def _main_between(ctx: _Context, layers: set[int], a: int, b: int) -> float | None:
    """Usable main track between switches a and b: the non-turnout tracks
    on the shortest main-track path between them. Measuring along the path
    (not by MP range) keeps it right where the main loops back on itself.
    None when no main-track path joins them."""
    import heapq
    dist = {a: 0.0}
    prev: dict[int, int] = {}
    heap = [(0.0, a)]
    while heap:
        d, tid = heapq.heappop(heap)
        if tid == b:
            break
        if d > dist.get(tid, math.inf):
            continue
        for ep in ctx.by_id[tid].endpoints:
            nxt = ctx.by_id.get(ep.connected_to) if ep.connected_to is not None else None
            if nxt is None or nxt.layer not in layers:
                continue
            nd = d + nxt.length_model_inches()
            if nd < dist.get(nxt.id, math.inf):
                dist[nxt.id], prev[nxt.id] = nd, tid
                heapq.heappush(heap, (nd, nxt.id))
    if b not in dist:
        return None
    total, tid = 0.0, prev.get(b)
    while tid is not None and tid != a:
        track = ctx.by_id[tid]
        if track.kind != "TURNOUT":
            total += track.length_model_inches()
        tid = prev.get(tid)
    return total


def _house_tracks(ctx: _Context, station_id: str, railroad: str | None,
                  joint: bool) -> list[dict]:
    out = []
    for n in ctx.of_kind("house_track"):
        if n.id != station_id or (joint and n.railroad != railroad):
            continue
        track_id, length = ctx.usable_in(n)
        out.append({
            "label": n.str_field("label"), "track_id": track_id,
            "length_ft": ctx.feet(length) if length is not None else None,
            "capacity_cars": ctx.cars(length) if length is not None else None,
        })
    return sorted(out, key=lambda h: (h["label"] or "", h["track_id"] or -1))


def _station_entry(ctx: _Context, kind: str, ident: str, name: str | None,
                   railroad: str | None, subdivision: str | None,
                   mp: float | None, mp_entry: float | None, mp_exit: float | None,
                   terminus: bool, switchback: bool) -> dict:
    return {
        "id": ident, "kind": kind, "name": name,
        "railroad": railroad, "subdivision": subdivision,
        "mp": _round(mp), "mp_entry": _round(mp_entry), "mp_exit": _round(mp_exit),
        "terminus": terminus, "switchback": switchback,
        "siding_ft": None, "siding_cars": None, "main_ft": None, "main_cars": None,
        "house_tracks": [],
    }


def _stations(ctx: _Context) -> list[dict]:
    """stations[]: one entry per station note, so a joint station (one id
    on several subdivisions, spec 4.3) has one entry per subdivision; and a
    team track that belongs to no station, at its switch's MP."""
    names = {n.note_id: n.str_field("name") for n in ctx.of_kind("station")}
    switchbacks = {n.str_field("serves") for n in ctx.of_kind("switchback")}
    per_id: dict[str, int] = {}
    for st in ctx.model.stations:
        per_id[st.id] = per_id.get(st.id, 0) + 1
    out = []
    for st in ctx.model.stations:
        entry = _station_entry(ctx, "station", st.id, names.get(st.note_id),
                               st.railroad, st.subdivision, st.mp, st.mp_entry,
                               st.mp_exit, st.terminus, st.id in switchbacks)
        m = ctx.measurers.get((st.railroad, st.subdivision))
        siding = _siding(ctx, st.switch_ids) if m is not None else None
        if siding is not None:
            siding_in, ends = siding
            entry["siding_ft"], entry["siding_cars"] = ctx.feet(siding_in), ctx.cars(siding_in)
            main_in = _main_between(ctx, m.line.layers, *ends)
            if main_in is not None:
                entry["main_ft"], entry["main_cars"] = ctx.feet(main_in), ctx.cars(main_in)
        entry["house_tracks"] = _house_tracks(ctx, st.id, st.railroad, per_id[st.id] > 1)
        out.append(entry)
    for tt in ctx.of_kind("team_track"):
        if tt.str_field("station") is None:
            mp = ctx.model.branch_mp.get(tt.id)
            out.append(_station_entry(ctx, "team_track", tt.id, tt.str_field("name"),
                                      tt.railroad, tt.subdivision, mp, mp, mp,
                                      False, tt.id in switchbacks))
    return sorted(out, key=lambda s: (s["railroad"] or "", s["subdivision"] or "",
                                      _mp_key(s["mp"]), s["id"]))


def _yard_limits(ctx: _Context) -> list[dict]:
    zones = [{
        "id": z.id, "railroad": z.railroad, "subdivision": z.subdivision,
        "mp_start": _round(z.mp_start), "mp_end": _round(z.mp_end), "source": z.source,
    } for z in ctx.model.yard_limits]
    return sorted(zones, key=lambda z: (z["railroad"] or "", z["subdivision"] or "",
                                        z["mp_start"], z["id"]))


def _industries(ctx: _Context, kind: str) -> list[dict]:
    """industries[] (kind industry) or team_tracks[] (kind team_track)."""
    switchbacks = {n.str_field("serves") for n in ctx.of_kind("switchback")}
    out = []
    for n in ctx.of_kind(kind):
        spots = n.fields.get("car_spots")
        _track, length = ctx.usable_in(n)
        out.append({
            "id": n.id, "name": n.str_field("name"), "station": n.str_field("station"),
            "branch_mp": _round(ctx.model.branch_mp.get(n.id)),
            "switchback": n.id in switchbacks,
            "car_spots": spots if isinstance(spots, int) and not isinstance(spots, bool)
            else CAR_SPOTS_DEFAULT,
            "spur_capacity_cars": ctx.cars(length) if length is not None else None,
            "car_types": n.str_list_field("car_types"),
            "loads_in": n.str_list_field("loads_in"),
            "loads_out": n.str_list_field("loads_out"),
        })
    return sorted(out, key=lambda i: (_mp_key(i["branch_mp"]), i["id"]))


def _yards(ctx: _Context) -> list[dict]:
    """yards[]: the yard tracks of each location, by the location's id."""
    by_id: dict[str, list[dict]] = {}
    for n in ctx.of_kind("yard_track"):
        track_id, length = ctx.usable_in(n)
        by_id.setdefault(n.id, []).append({
            "label": n.str_field("label"), "function": n.str_field("function"),
            "railroads": n.str_list_field("railroads"), "track_id": track_id,
            "length_ft": ctx.feet(length) if length is not None else None,
            "capacity_cars": ctx.cars(length) if length is not None else None,
        })
    return [{"id": ident, "kind": ctx.location_kind.get(ident),
             "tracks": sorted(tracks, key=lambda t: t["label"] or "")}
            for ident, tracks in sorted(by_id.items())]


def _switchbacks(ctx: _Context) -> list[dict]:
    """switchbacks[]: each switchback note's tail track (where the note
    sits) and the main switch(es) acting as the switchback switch: the
    note's ``switches``, else the switches its tail leaves (spec 4.9)."""
    main = _main_layers(ctx.layout)
    out = []
    for n in ctx.of_kind("switchback"):
        tail = ctx.snapped.get(n.note_id)
        switches = n.fields.get("switches")
        if isinstance(switches, list) and switches:
            ids = [sw for sw in switches if isinstance(sw, int)]
        elif tail is not None:
            ids = [t.id for t in main_turnouts_from(ctx.layout, tail, main)]
        else:
            ids = []
        _track, length = ctx.usable_in(n)
        out.append({
            "id": n.id, "serves": n.str_field("serves"), "switch_ids": ids,
            "track_id": tail.id if tail is not None else None,
            "tail_ft": ctx.feet(length) if length is not None else None,
        })
    return sorted(out, key=lambda sb: sb["id"])


def _given_car_spots(note: ConventionNote) -> int | None:
    """A facility's or storage track's car_spots: only as the note gives
    it, no default (spec 4.8, v1.1)."""
    spots = note.fields.get("car_spots")
    return spots if isinstance(spots, int) and not isinstance(spots, bool) else None


def _parked(ctx: _Context, note: ConventionNote) -> dict:
    """The track, length, capacity and car_spots of a facility or storage
    note."""
    track_id, length = ctx.usable_in(note)
    return {
        "track_id": track_id,
        "length_ft": ctx.feet(length) if length is not None else None,
        "capacity_cars": ctx.cars(length) if length is not None else None,
        "car_spots": _given_car_spots(note),
    }


def _storage(ctx: _Context) -> list[dict]:
    """storage[]: stand-alone car storage tracks (spec 4.8)."""
    out = [{"id": n.id, "name": n.str_field("name"), **_parked(ctx, n)}
           for n in ctx.of_kind("storage")]
    return sorted(out, key=lambda st: st["id"])


def _facilities(ctx: _Context) -> list[dict]:
    out = [{"id": n.id, "name": n.str_field("name"), "type": n.str_field("type"),
            "location": n.str_field("location"), **_parked(ctx, n)}
           for n in ctx.of_kind("facility")]
    return sorted(out, key=lambda f: f["id"])


def _segments(ctx: _Context, stations: list[dict]) -> list[dict]:
    """segments[]: main-track length between consecutive stations[] (by MP)
    on each subdivision, from their MPs."""
    out = []
    for line in ctx.lines:
        if line.scale_in is None:
            continue
        on_line = [s for s in stations if (s["railroad"], s["subdivision"]) == line.key()
                   and s["mp"] is not None]
        for a, b in zip(on_line, on_line[1:]):
            mp_a = _exact_mp(ctx, a)
            mp_b = _exact_mp(ctx, b)
            out.append({
                "from_station": a["id"], "to_station": b["id"],
                "railroad": line.railroad, "subdivision": line.subdivision,
                "length_ft": ctx.feet(abs(mp_b - mp_a) * line.scale_in),
            })
    return out


def _exact_mp(ctx: _Context, station: dict) -> float:
    """A stations[] entry's unrounded MP, so segment lengths don't carry
    the rounding of the MPs."""
    if station["kind"] == "team_track":
        value = ctx.model.branch_mp.get(station["id"])
    else:
        value = next((s.mp for s in ctx.model.stations
                      if s.id == station["id"] and s.railroad == station["railroad"]
                      and s.subdivision == station["subdivision"]), None)
    return value if value is not None else station["mp"]


def build_export_v2(layout: Layout, issues: list[Issue] | None = None) -> dict:
    """The layout_data.json v2 structure for a parsed layout.

    ``issues`` is the convention check's result when the caller already has
    it; otherwise the check runs here.
    """
    if issues is None:
        issues = check_conventions(layout)
    ctx = _Context(layout)
    stations = _stations(ctx)
    return {
        "schema_version": SCHEMA_VERSION,
        "conventions_version": CONVENTIONS_VERSION,
        "generated": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "layout": layout.title1 or None,
        "scale": layout.scale or None,
        "layout_units": "in",
        "railroads": _railroads(ctx),
        "stations": stations,
        "yard_limits": _yard_limits(ctx),
        "industries": _industries(ctx, "industry"),
        "team_tracks": _industries(ctx, "team_track"),
        "yards": _yards(ctx),
        "storage": _storage(ctx),
        "switchbacks": _switchbacks(ctx),
        "facilities": _facilities(ctx),
        "segments": _segments(ctx, stations),
        "issues": [i.as_dict() for i in issues],
    }
