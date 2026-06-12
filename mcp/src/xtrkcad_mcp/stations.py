"""Station distance and siding/storage capacity calculations for XTrkCAD layouts.

NOTE conventions
----------------
STATION: <name>    — mainline station; used for inter-station routing/milepost.
SIDING: <name>     — siding track group; used for car-capacity measurement.
STORAGE: <name>    — storage/yard track group; used for car-capacity measurement.
INDUSTRY: <name>   — industry spur; used for spur-length and branch-MP measurement.
REFERENCE: <name>  — reference point; REFERENCE: MP_ZERO marks milepost 0.

Resolution priority (first match wins):
  1. NOTE text with the prefix above
  2. (future) track label field
  3. layer name inference (e.g. L1-Passing → siding)

For routing distances, Dijkstra runs on the endpoint-level track graph.
For capacity, a BFS walks connected non-TURNOUT tracks from the NOTE's nearest
endpoint, summing their lengths.  TURNOUT (switch) track length is excluded
so the count reflects only the usable straight/curve track.

Multiple industries on one spur
---------------------------------
When two or more INDUSTRY: notes project onto the same BFS spur component, each
note is snapped to the nearest track segment (perpendicular projection rather
than nearest endpoint) and sorted by distance from the spur's dead-end (the
unconnected / buffer-stop endpoint).  Each industry is then assigned one
``_SNAP_WARN_THRESHOLD``-wide slot (default 12 model inches ≈ one car length).

Place each NOTE over the section of track that serves the industry; the system
derives the ordering automatically.  To override the car count for any industry,
set ``spots: N`` in stations.yaml.
"""

import datetime
import heapq
import math
from dataclasses import dataclass, field
from pathlib import Path

import yaml

from xtrkcad_mcp.models import LayerInfo, Layout, NoteObject, SCALE_RATIOS, cars_per_real_ft

STATION_PREFIX = "STATION:"
SIDING_PREFIX = "SIDING:"
STORAGE_PREFIX = "STORAGE:"
INDUSTRY_PREFIX = "INDUSTRY:"
REFERENCE_PREFIX = "REFERENCE:"
YARD_TRACK_PREFIX = "YARD_TRACK:"
_INF = float("inf")

_ALL_PREFIXES: list[tuple[str, str]] = [
    (STATION_PREFIX, "station"),
    (SIDING_PREFIX, "siding"),
    (STORAGE_PREFIX, "storage"),
    (INDUSTRY_PREFIX, "industry"),
    (REFERENCE_PREFIX, "reference"),
    (YARD_TRACK_PREFIX, "yard_track"),
]

_SNAP_WARN_THRESHOLD = 12.0  # model inches — warn if NOTE is more than 1 model foot from track


@dataclass
class Station:
    """A STATION: note snapped to its nearest track endpoint."""
    name: str
    note_id: int
    x: float           # NOTE position x (model inches)
    y: float           # NOTE position y (model inches)
    nearest_ep: tuple[int, int]   # (track_id, ep_idx)
    snap_dist: float              # distance from NOTE to nearest endpoint
    ref_tag: str | None = None    # @<name> token — station is co-located with REFERENCE:<name>


@dataclass
class DistanceResult:
    """Distance between two stations."""
    from_station: str
    to_station: str
    distance_model_in: float    # layout model inches
    distance_proto_ft: float    # prototypical feet (scale-dependent)
    reachable: bool


def model_in_to_proto_ft(model_in: float, scale: str) -> float:
    """Convert model inches to prototypical feet using the layout scale."""
    ratio = SCALE_RATIOS.get(scale.upper(), SCALE_RATIOS.get(scale, 87.1))
    return model_in * ratio / 12.0


def extract_stations(layout: Layout) -> list[Station]:
    """Find all STATION: text notes and snap each to the nearest track endpoint."""
    if not layout.tracks:
        return []

    stations: list[Station] = []
    for note in layout.notes:
        if note.op != 0:
            continue
        text = note.text.strip()
        if not text.upper().startswith(STATION_PREFIX):
            continue
        body = text[len(STATION_PREFIX):].strip()
        tokens = body.split()
        if not tokens:
            continue
        name = tokens[0]
        ref_tag = next((t[1:] for t in tokens[1:] if t.startswith("@")), None)

        best_track_id = -1
        best_ep_idx = 0
        best_dist = _INF
        for track in layout.tracks:
            for ep_idx, ep in enumerate(track.endpoints):
                d = math.hypot(ep.x - note.x, ep.y - note.y)
                if d < best_dist:
                    best_dist = d
                    best_track_id = track.id
                    best_ep_idx = ep_idx

        if best_track_id >= 0:
            stations.append(Station(
                name=name,
                note_id=note.id,
                x=note.x,
                y=note.y,
                nearest_ep=(best_track_id, best_ep_idx),
                snap_dist=best_dist,
                ref_tag=ref_tag,
            ))

    return stations


def _resolve_station_endpoints(
    stations: list[Station],
    layout: Layout,
) -> list[Station]:
    """Return a copy of stations with ref-tagged stations' nearest_ep replaced.

    A station annotated with '@MP_ZERO' (or any '@<ref>') adopts the graph
    endpoint of the matching REFERENCE: note so that Dijkstra can route to it
    even when the station NOTE is placed on an isolated yard track.
    """
    refs = {r.name: r for r in extract_reference_points(layout)}
    if not refs:
        return stations

    resolved: list[Station] = []
    for s in stations:
        if s.ref_tag and s.ref_tag in refs:
            ref = refs[s.ref_tag]
            resolved.append(Station(
                name=s.name,
                note_id=s.note_id,
                x=s.x,
                y=s.y,
                nearest_ep=ref.nearest_ep,
                snap_dist=s.snap_dist,
                ref_tag=s.ref_tag,
            ))
        else:
            resolved.append(s)
    return resolved


def _build_ep_graph(
    layout: Layout,
    allowed_layer_indices: set[int] | None,
    exclude_turnouts: bool,
) -> dict[tuple[int, int], list[tuple[tuple[int, int], float]]]:
    """Build an endpoint-level adjacency graph for Dijkstra.

    Nodes: (track_id, ep_idx).
    Intra-track edges: connect all endpoint pairs with weight = track length.
    Inter-track edges: follow connected_to links with weight = 0.

    allowed_layer_indices: if given, only include tracks on those layers.
    exclude_turnouts: if True, TURNOUT lengths count as 0 (connection-only).
    """
    track_by_id = {t.id: t for t in layout.tracks}
    adj: dict[tuple[int, int], list[tuple[tuple[int, int], float]]] = {}

    def add_edge(a: tuple[int, int], b: tuple[int, int], w: float) -> None:
        adj.setdefault(a, []).append((b, w))
        adj.setdefault(b, []).append((a, w))

    seen_inter: set[tuple[tuple[int, int], tuple[int, int]]] = set()

    for track in layout.tracks:
        if allowed_layer_indices is not None and track.layer not in allowed_layer_indices:
            continue
        n_ep = len(track.endpoints)
        if n_ep < 2:
            continue

        if exclude_turnouts and track.kind == "TURNOUT":
            length = 0.0
        else:
            length = track.length_model_inches()

        # Intra-track: connect all endpoint pairs
        for i in range(n_ep):
            for j in range(i + 1, n_ep):
                add_edge((track.id, i), (track.id, j), length)

        # Inter-track: follow connected_to pointers
        for ep_idx, ep in enumerate(track.endpoints):
            if ep.connected_to is None:
                continue
            neighbor = track_by_id.get(ep.connected_to)
            if neighbor is None:
                continue
            if allowed_layer_indices is not None and neighbor.layer not in allowed_layer_indices:
                continue
            # Find the endpoint on neighbor that connects back to this track
            for n_ep_idx, n_ep in enumerate(neighbor.endpoints):
                if n_ep.connected_to == track.id:
                    a = (track.id, ep_idx)
                    b = (neighbor.id, n_ep_idx)
                    key = (min(a, b), max(a, b))
                    if key not in seen_inter:
                        seen_inter.add(key)
                        add_edge(a, b, 0.0)

    return adj


def _dijkstra(
    adj: dict[tuple[int, int], list[tuple[tuple[int, int], float]]],
    start: tuple[int, int],
) -> dict[tuple[int, int], float]:
    """Single-source Dijkstra. Returns distances from start to all reachable nodes."""
    dist: dict[tuple[int, int], float] = {start: 0.0}
    heap: list[tuple[float, tuple[int, int]]] = [(0.0, start)]
    while heap:
        d, node = heapq.heappop(heap)
        if d > dist.get(node, _INF):
            continue
        for neighbor, w in adj.get(node, []):
            nd = d + w
            if nd < dist.get(neighbor, _INF):
                dist[neighbor] = nd
                heapq.heappush(heap, (nd, neighbor))
    return dist


def compute_distances(
    layout: Layout,
    layer_categories: dict | None = None,
    route_layer_names: list[str] | None = None,
    exclude_turnouts: bool = False,
) -> list[DistanceResult]:
    """Compute pairwise shortest-path distances between all STATION: notes.

    Args:
        layout: parsed layout.
        layer_categories: maps layer index (int or str) → category name,
            e.g. {0: "mainline", 1: "connecting", 4: "passing"}.
        route_layer_names: if given, restrict path to layers whose category
            name is in this list, e.g. ["mainline", "connecting"].
        exclude_turnouts: if True, TURNOUT track lengths are treated as 0
            (the turnout acts as a free connection, not adding to distance).
    """
    stations = _resolve_station_endpoints(extract_stations(layout), layout)
    if len(stations) < 2:
        return []

    # Resolve allowed layer indices from layer_categories + route_layer_names
    allowed_layers: set[int] | None = None
    if route_layer_names and layer_categories:
        norm_cats = {
            int(k) if str(k).isdigit() else k: v
            for k, v in layer_categories.items()
        }
        allowed_layers = {
            idx for idx, cat in norm_cats.items()
            if isinstance(idx, int) and cat in route_layer_names
        }

    adj = _build_ep_graph(layout, allowed_layers, exclude_turnouts)
    results: list[DistanceResult] = []

    for i, s1 in enumerate(stations):
        dist_from_s1 = _dijkstra(adj, s1.nearest_ep)
        for s2 in stations[i + 1:]:
            d = dist_from_s1.get(s2.nearest_ep, _INF)
            reachable = d < _INF
            proto_ft = model_in_to_proto_ft(d, layout.scale) if reachable else 0.0
            results.append(DistanceResult(
                from_station=s1.name,
                to_station=s2.name,
                distance_model_in=d if reachable else 0.0,
                distance_proto_ft=proto_ft,
                reachable=reachable,
            ))

    return results


# ---------------------------------------------------------------------------
# Siding / storage capacity
# ---------------------------------------------------------------------------

@dataclass
class CapacityResult:
    """Usable track length and car count for a named SIDING: or STORAGE: marker."""
    name: str                    # station/siding ID (first token after prefix)
    description: str             # optional human label (text after the ID token)
    kind: str                    # "siding" or "storage"
    note_id: int
    nearest_track_id: int
    length_model_in: float       # total usable track length (model inches, turnouts excluded)
    length_real_ft: float        # prototypical feet (scale-dependent, for display)
    max_cars: int                # whole cars (Fugate formula: model feet × cars/model-ft)


@dataclass
class YardTrackResult:
    """Capacity measurement for a YARD_TRACK: marker."""
    yard_id: str
    label: str
    note_id: int
    nearest_track_id: int
    length_model_in: float
    length_real_ft: float
    car_capacity: int


def _note_prefix(text: str) -> tuple[str, str, str] | None:
    """Return (kind, id, description) for any recognised annotation prefix.

    The ID is the first whitespace-delimited token after the prefix.
    Everything after the ID is the optional human-readable description.

    Examples:
      'SIDING: WP Platform arrival track' → ('siding', 'WP', 'Platform arrival track')
      'YARD_TRACK: WP LEAD'              → ('yard_track', 'WP', 'LEAD')
      'STATION: WP'                      → ('station', 'WP', '')
      'REFERENCE: MP_ZERO'               → ('reference', 'MP_ZERO', '')
    """
    upper = text.strip().upper()
    for prefix, kind in (
        (STATION_PREFIX, "station"),
        (SIDING_PREFIX, "siding"),
        (STORAGE_PREFIX, "storage"),
        (INDUSTRY_PREFIX, "industry"),
        (REFERENCE_PREFIX, "reference"),
        (YARD_TRACK_PREFIX, "yard_track"),
    ):
        if upper.startswith(prefix):
            body = text.strip()[len(prefix):].strip()
            parts = body.split(None, 1)
            if not parts:
                return None
            return kind, parts[0], parts[1] if len(parts) > 1 else ""
    return None


def _is_main_layer(name: str) -> bool:
    """Return True if the layer name denotes a main track layer."""
    lower = name.lower()
    # Standard: "L1-Main", "L2-Main"; legacy: "L1-Track"
    return lower.endswith("-main") or lower.endswith("-track")


def _bfs_usable_length(
    layout: Layout,
    start_track_id: int,
) -> float:
    """BFS from start_track_id along connected non-TURNOUT tracks.

    Sums the length of every reachable STRAIGHT / CURVE / JOINT / CORNU /
    BEZIER / HANDLAID track.  TURNOUT objects are treated as opaque walls:
    traversal does not enter them, so switch-throat geometry is excluded.

    Returns total usable track length in model inches.
    """
    track_by_id = {t.id: t for t in layout.tracks}
    start = track_by_id.get(start_track_id)
    if start is None:
        return 0.0

    visited: set[int] = set()
    queue: list[int] = [start_track_id]
    total = 0.0

    while queue:
        tid = queue.pop()
        if tid in visited:
            continue
        visited.add(tid)
        track = track_by_id.get(tid)
        if track is None:
            continue
        if track.kind == "TURNOUT":
            # Boundary: don't enter or measure switch track
            continue
        total += track.length_model_inches()
        for ep in track.endpoints:
            if ep.connected_to is not None and ep.connected_to not in visited:
                neighbor = track_by_id.get(ep.connected_to)
                if neighbor is not None and neighbor.kind != "TURNOUT":
                    queue.append(ep.connected_to)

    return total


def _snap_to_track(layout: Layout, x: float, y: float) -> int:
    """Return the track_id of the track whose nearest endpoint is closest to (x, y)."""
    best_id = -1
    best_dist = _INF
    for track in layout.tracks:
        for ep in track.endpoints:
            d = math.hypot(ep.x - x, ep.y - y)
            if d < best_dist:
                best_dist = d
                best_id = track.id
    return best_id


def _project_onto_segment(
    px: float, py: float,
    x0: float, y0: float, x1: float, y1: float,
) -> tuple[float, float]:
    """Project (px, py) onto segment [x0,y0]→[x1,y1].

    Returns (t, perp_dist): t ∈ [0,1] is the clamped parameter (t=0 at ep[0],
    t=1 at ep[1]) and perp_dist is the distance from (px,py) to the projection.
    """
    dx, dy = x1 - x0, y1 - y0
    seg_len_sq = dx * dx + dy * dy
    if seg_len_sq < 1e-12:
        return 0.0, math.hypot(px - x0, py - y0)
    t = ((px - x0) * dx + (py - y0) * dy) / seg_len_sq
    t = max(0.0, min(1.0, t))
    cx, cy = x0 + t * dx, y0 + t * dy
    return t, math.hypot(px - cx, py - cy)


def _snap_to_segment(layout: Layout, x: float, y: float) -> tuple[int, float, float]:
    """Find the nearest point on any track segment to (x, y).

    Returns (track_id, t, perp_dist): t ∈ [0,1] is the parameter along the
    chord ep[0]→ep[1] and perp_dist is the perpendicular distance from (x,y)
    to the projection point.  All track types are approximated via their chord
    (accurate for straights; close enough for curves for ordering purposes).
    """
    best_id = -1
    best_t = 0.0
    best_dist = _INF
    for track in layout.tracks:
        eps = track.endpoints
        if len(eps) < 2:
            continue
        t, d = _project_onto_segment(x, y, eps[0].x, eps[0].y, eps[1].x, eps[1].y)
        if d < best_dist:
            best_dist = d
            best_id = track.id
            best_t = t
    return best_id, best_t, best_dist


def _spur_bfs_component(layout: Layout, start_track_id: int) -> set[int]:
    """Return the set of non-TURNOUT track IDs reachable from start_track_id.

    Stops at TURNOUT boundaries (the switch throat), so the result contains
    exactly the usable track IDs that _bfs_usable_length sums.
    """
    track_by_id = {t.id: t for t in layout.tracks}
    start = track_by_id.get(start_track_id)
    if start is None or start.kind == "TURNOUT":
        return set()

    visited: set[int] = set()
    queue: list[int] = [start_track_id]
    while queue:
        tid = queue.pop()
        if tid in visited:
            continue
        visited.add(tid)
        track = track_by_id.get(tid)
        if track is None:
            continue
        for ep in track.endpoints:
            if ep.connected_to is not None and ep.connected_to not in visited:
                nb = track_by_id.get(ep.connected_to)
                if nb is not None and nb.kind != "TURNOUT":
                    queue.append(ep.connected_to)
    return visited


def _find_dead_end_ep(layout: Layout, spur_ids: set[int]) -> tuple[int, int] | None:
    """Return (track_id, ep_idx) of the first unconnected endpoint in the spur.

    In a correctly-wired layout this is the buffer-stop end.  Iterates over
    sorted track IDs for determinism.  Returns None for circular spurs.
    """
    track_by_id = {t.id: t for t in layout.tracks}
    for tid in sorted(spur_ids):
        track = track_by_id.get(tid)
        if track is None or track.kind == "TURNOUT":
            continue
        for ep_idx, ep in enumerate(track.endpoints):
            if ep.connected_to is None:
                return (tid, ep_idx)
    return None


def _dist_from_dead_end(
    layout: Layout,
    spur_ids: set[int],
    dead_end_ep: tuple[int, int],
    target_track_id: int,
    t_on_target: float,
) -> float:
    """Distance along the spur from dead_end_ep to the point at t_on_target on target_track.

    Runs Dijkstra on the spur's own endpoint graph (restricted to spur_ids), then
    adds the fractional remaining distance within the target segment.
    """
    track_by_id = {t.id: t for t in layout.tracks}

    adj: dict[tuple[int, int], list[tuple[tuple[int, int], float]]] = {}
    seen_inter: set[tuple] = set()

    def _add(a: tuple[int, int], b: tuple[int, int], w: float) -> None:
        adj.setdefault(a, []).append((b, w))
        adj.setdefault(b, []).append((a, w))

    for tid in spur_ids:
        track = track_by_id.get(tid)
        if track is None or track.kind == "TURNOUT":
            continue
        n_ep = len(track.endpoints)
        length = track.length_model_inches()
        for i in range(n_ep):
            for j in range(i + 1, n_ep):
                _add((tid, i), (tid, j), length)
        for ep_idx, ep in enumerate(track.endpoints):
            if ep.connected_to is None or ep.connected_to not in spur_ids:
                continue
            nb = track_by_id.get(ep.connected_to)
            if nb is None or nb.kind == "TURNOUT":
                continue
            for n_ep_idx, n_ep_obj in enumerate(nb.endpoints):
                if n_ep_obj.connected_to == tid:
                    a, b = (tid, ep_idx), (nb.id, n_ep_idx)
                    key = (min(a, b), max(a, b))
                    if key not in seen_inter:
                        seen_inter.add(key)
                        _add(a, b, 0.0)

    dist_from_dead = _dijkstra(adj, dead_end_ep)

    target = track_by_id.get(target_track_id)
    if target is None:
        return _INF
    seg_len = target.length_model_inches()
    best = _INF
    for ep_idx in range(len(target.endpoints)):
        d_ep = dist_from_dead.get((target_track_id, ep_idx), _INF)
        if d_ep >= _INF:
            continue
        # t_on_target is measured from ep[0]; distance from ep[ep_idx] to that point:
        d_point = d_ep + (t_on_target if ep_idx == 0 else (1.0 - t_on_target)) * seg_len
        best = min(best, d_point)
    return best


def compute_capacities(layout: Layout) -> list[CapacityResult]:
    """Compute usable track length and car capacity for every SIDING:/STORAGE:/INDUSTRY: note.

    SIDING / STORAGE notes: BFS walks connected non-TURNOUT tracks from the
    nearest track endpoint; Fugate formula converts length to car count.

    INDUSTRY notes — single industry on a spur: same as above (full BFS length).

    INDUSTRY notes — multiple industries on the same spur: each note is projected
    perpendicularly onto the track centreline and sorted by distance from the
    spur's dead-end endpoint (the buffer-stop / unconnected end, i.e. the end
    the switcher reaches last).  Each industry is assigned one threshold-width
    slot (_SNAP_WARN_THRESHOLD model inches); set ``spots:`` in stations.yaml to
    override the derived car count for any individual industry.
    """
    if not layout.tracks:
        return []

    scale = layout.scale or "HO"
    cpf = cars_per_real_ft(scale)
    results: list[CapacityResult] = []

    # --- SIDING / STORAGE: endpoint-snap, full BFS (unchanged behaviour) ---
    for note in layout.notes:
        if note.op != 0:
            continue
        parsed = _note_prefix(note.text)
        if parsed is None:
            continue
        kind, name, description = parsed
        if kind not in ("siding", "storage") or not name:
            continue
        nearest_id = _snap_to_track(layout, note.x, note.y)
        if nearest_id < 0:
            continue
        length_in = _bfs_usable_length(layout, nearest_id)
        model_ft = length_in / 12.0
        proto_ft = model_in_to_proto_ft(length_in, scale)
        results.append(CapacityResult(
            name=name,
            description=description,
            kind=kind,
            note_id=note.id,
            nearest_track_id=nearest_id,
            length_model_in=length_in,
            length_real_ft=proto_ft,
            max_cars=int(model_ft * cpf),
        ))

    # --- INDUSTRY: segment-snap, grouped by spur component ---
    # Each note is projected onto its nearest track segment to determine its
    # position along the spur.  Notes that land on the same BFS component are
    # treated as sharing that spur.
    industry_items: list[tuple[NoteObject, str, str, int, float, frozenset]] = []
    for note in layout.notes:
        if note.op != 0:
            continue
        parsed = _note_prefix(note.text)
        if parsed is None:
            continue
        kind, name, description = parsed
        if kind != "industry" or not name:
            continue
        track_id, t, _perp = _snap_to_segment(layout, note.x, note.y)
        if track_id < 0:
            continue
        spur_ids = frozenset(_spur_bfs_component(layout, track_id))
        if not spur_ids:
            continue  # note snapped to a TURNOUT — skip
        industry_items.append((note, name, description, track_id, t, spur_ids))

    # Group by spur component frozenset.
    spur_groups: dict[frozenset, list] = {}
    for item in industry_items:
        spur_groups.setdefault(item[5], []).append(item)

    for spur_ids, group in spur_groups.items():
        any_track = next(iter(spur_ids))
        total_in = _bfs_usable_length(layout, any_track)
        total_ft = total_in / 12.0
        total_proto = model_in_to_proto_ft(total_in, scale)

        if len(group) == 1:
            # Single industry on spur — full BFS length (existing behaviour).
            note, name, description, track_id, _t, _ = group[0]
            results.append(CapacityResult(
                name=name,
                description=description,
                kind="industry",
                note_id=note.id,
                nearest_track_id=track_id,
                length_model_in=total_in,
                length_real_ft=total_proto,
                max_cars=int(total_ft * cpf),
            ))
        else:
            # Multiple industries share the spur.  Sort by distance from the
            # dead-end so the result order reflects switcher reach sequence.
            # Each industry gets one threshold-slot width of track.
            dead_end_ep = _find_dead_end_ep(layout, set(spur_ids))
            slot_in = _SNAP_WARN_THRESHOLD          # one slot = 12 model inches
            slot_proto = model_in_to_proto_ft(slot_in, scale)
            slot_cars = max(1, int(slot_in / 12.0 * cpf))

            def _dead_end_dist(item: tuple) -> float:
                _, _, _, tid, t, _ = item
                if dead_end_ep is None:
                    return _INF
                return _dist_from_dead_end(layout, set(spur_ids), dead_end_ep, tid, t)

            for item in sorted(group, key=_dead_end_dist):
                note, name, description, track_id, _t, _ = item
                results.append(CapacityResult(
                    name=name,
                    description=description,
                    kind="industry",
                    note_id=note.id,
                    nearest_track_id=track_id,
                    length_model_in=slot_in,
                    length_real_ft=slot_proto,
                    max_cars=slot_cars,
                ))

    return results


def compute_yard_tracks(layout: Layout) -> list[YardTrackResult]:
    """Compute usable length and car capacity for every YARD_TRACK: note.

    Note format: YARD_TRACK: <yard_id> <label>
    e.g.  YARD_TRACK: WP LEAD1
          YARD_TRACK: QM1 LOADING

    BFS walks connected non-TURNOUT tracks from the nearest endpoint, same
    as compute_capacities.  Returns one YardTrackResult per note.
    """
    if not layout.tracks:
        return []

    scale = layout.scale or "HO"
    cpf = cars_per_real_ft(scale)
    results: list[YardTrackResult] = []

    for note in layout.notes:
        if note.op != 0:
            continue
        text = note.text.strip()
        if not text.upper().startswith(YARD_TRACK_PREFIX):
            continue
        body = text[len(YARD_TRACK_PREFIX):].strip()
        parts = body.split(None, 1)
        if len(parts) < 2:
            continue
        yard_id, label = parts[0], parts[1]

        nearest_id = _snap_to_track(layout, note.x, note.y)
        if nearest_id < 0:
            continue

        length_in = _bfs_usable_length(layout, nearest_id)
        model_ft = length_in / 12.0
        proto_ft = model_in_to_proto_ft(length_in, scale)
        capacity = int(model_ft * cpf)

        results.append(YardTrackResult(
            yard_id=yard_id,
            label=label,
            note_id=note.id,
            nearest_track_id=nearest_id,
            length_model_in=length_in,
            length_real_ft=proto_ft,
            car_capacity=capacity,
        ))

    return results


# ---------------------------------------------------------------------------
# Stations config (stations.yaml)
# ---------------------------------------------------------------------------


@dataclass
class StationEntry:
    """One location entry from stations.yaml."""
    id: str
    name: str
    sequence: int
    types: list[str]              # "station", "industry", "yard", "staging"
    switchback: bool = False
    within_limits_of: str | None = None
    spots: int | None = None      # industry car spots (operational); None = derive from length


@dataclass
class StationsConfig:
    """Parsed stations.yaml — the reference schema for export and validation."""
    layout: str
    mp_scale: float               # prototype feet per milepost unit (default 1.0)
    stations: list[StationEntry]


def load_stations_config(path: str | Path) -> StationsConfig:
    """Load a stations.yaml file and return a StationsConfig."""
    path = Path(path)
    with path.open(encoding="utf-8") as fh:
        data = yaml.safe_load(fh)

    entries: list[StationEntry] = []
    for s in data.get("stations", []):
        raw_spots = s.get("spots")
        entries.append(StationEntry(
            id=s["id"],
            name=s.get("name", s["id"]),
            sequence=int(s.get("sequence", 0)),
            types=list(s.get("types", ["station"])),
            switchback=bool(s.get("switchback", False)),
            within_limits_of=s.get("within_limits_of"),
            spots=int(raw_spots) if raw_spots is not None else None,
        ))

    return StationsConfig(
        layout=str(data.get("layout", "")),
        mp_scale=float(data.get("mp_scale", 1.0)),
        stations=entries,
    )


# ---------------------------------------------------------------------------
# Reference points  (REFERENCE: notes)
# ---------------------------------------------------------------------------


@dataclass
class ReferencePoint:
    """A REFERENCE: text note snapped to its nearest track endpoint."""
    name: str                          # text after "REFERENCE:" e.g. "MP_ZERO"
    note_id: int
    x: float
    y: float
    nearest_ep: tuple[int, int]        # (track_id, ep_idx)
    snap_dist: float


def extract_reference_points(layout: Layout) -> list[ReferencePoint]:
    """Find all REFERENCE: text notes and snap each to the nearest track endpoint."""
    if not layout.tracks:
        return []

    points: list[ReferencePoint] = []
    for note in layout.notes:
        if note.op != 0:
            continue
        text = note.text.strip()
        if not text.upper().startswith(REFERENCE_PREFIX):
            continue
        name = text[len(REFERENCE_PREFIX):].strip()
        if not name:
            continue

        best_id = -1
        best_ep_idx = 0
        best_dist = _INF
        for track in layout.tracks:
            for ep_idx, ep in enumerate(track.endpoints):
                d = math.hypot(ep.x - note.x, ep.y - note.y)
                if d < best_dist:
                    best_dist = d
                    best_id = track.id
                    best_ep_idx = ep_idx

        if best_id >= 0:
            points.append(ReferencePoint(
                name=name,
                note_id=note.id,
                x=note.x,
                y=note.y,
                nearest_ep=(best_id, best_ep_idx),
                snap_dist=best_dist,
            ))

    return points


# ---------------------------------------------------------------------------
# Milepost calculation
# ---------------------------------------------------------------------------


@dataclass
class MilepostResult:
    """Milepost of a STATION: note measured from the REFERENCE: MP_ZERO point."""
    station_id: str
    note_id: int
    milepost: float | None            # None when unreachable
    distance_model_in: float
    reachable: bool


def compute_mileposts(
    layout: Layout,
    stations_config: StationsConfig,
    ref: ReferencePoint,
) -> list[MilepostResult]:
    """Compute milepost for every STATION: note, measured from ref.

    Milepost = prototype_feet_from_ref / mp_scale.
    With the default mp_scale=1.0, 1 milepost unit == 1 prototype foot of track.
    """
    stations = _resolve_station_endpoints(extract_stations(layout), layout)
    if not stations:
        return []

    adj = _build_ep_graph(layout, None, False)
    dist_from_ref = _dijkstra(adj, ref.nearest_ep)

    results: list[MilepostResult] = []
    for station in stations:
        # Stations tagged @<ref_name> that match this reference are at MP 0
        if station.ref_tag and station.ref_tag == ref.name:
            results.append(MilepostResult(
                station_id=station.name,
                note_id=station.note_id,
                milepost=0.0,
                distance_model_in=0.0,
                reachable=True,
            ))
            continue
        d = dist_from_ref.get(station.nearest_ep, _INF)
        reachable = d < _INF
        if reachable:
            proto_ft = model_in_to_proto_ft(d, layout.scale)
            mp: float | None = proto_ft / stations_config.mp_scale
        else:
            mp = None
        results.append(MilepostResult(
            station_id=station.name,
            note_id=station.note_id,
            milepost=mp,
            distance_model_in=d if reachable else 0.0,
            reachable=reachable,
        ))

    return results


# ---------------------------------------------------------------------------
# Annotated segments — what's labelled in the layout
# ---------------------------------------------------------------------------


@dataclass
class AnnotatedSegment:
    """One NOTE-based annotation resolved to its nearest track."""
    annotation_id: str
    annotation_type: str       # "station" | "siding" | "storage" | "industry" | "reference"
    annotation_source: str     # "note" (only source implemented; "label"/"layer" are future)
    note_id: int
    note_text: str
    nearest_track_id: int
    nearest_track_layer: int
    nearest_track_layer_name: str
    snap_dist: float


def list_annotated_segments(layout: Layout) -> list[AnnotatedSegment]:
    """Return one AnnotatedSegment for each NOTE with a known annotation prefix.

    Scans all text NOTEs for STATION:, SIDING:, STORAGE:, INDUSTRY:, REFERENCE:
    prefixes and snaps each to the nearest track endpoint.
    """
    if not layout.tracks:
        return []

    results: list[AnnotatedSegment] = []
    track_by_id = {t.id: t for t in layout.tracks}

    for note in layout.notes:
        if note.op != 0:
            continue
        text = note.text.strip()
        upper = text.upper()

        parsed = _note_prefix(text)
        if not parsed:
            continue
        _kind, matched_id, _desc = parsed
        matched_type = _kind
        # YARD_TRACK annotation_id combines yard_id and label to stay unique
        if _kind == "yard_track" and _desc:
            matched_id = f"{matched_id} {_desc}"

        if not matched_id:
            continue

        best_id = -1
        best_ep_idx = 0
        best_dist = _INF
        for track in layout.tracks:
            for ep_idx, ep in enumerate(track.endpoints):
                d = math.hypot(ep.x - note.x, ep.y - note.y)
                if d < best_dist:
                    best_dist = d
                    best_id = track.id
                    best_ep_idx = ep_idx

        if best_id < 0:
            continue

        track = track_by_id.get(best_id)
        layer_idx = track.layer if track else 0
        layer_info = layout.layers.get(layer_idx, LayerInfo(layer_idx, f"Layer {layer_idx}"))

        results.append(AnnotatedSegment(
            annotation_id=matched_id,
            annotation_type=matched_type,
            annotation_source="note",
            note_id=note.id,
            note_text=text,
            nearest_track_id=best_id,
            nearest_track_layer=layer_idx,
            nearest_track_layer_name=layer_info.name,
            snap_dist=best_dist,
        ))

    return results


# ---------------------------------------------------------------------------
# Layout validation
# ---------------------------------------------------------------------------


@dataclass
class ValidationIssue:
    """A single annotation problem found by validate_layout_annotations."""
    severity: str             # "error" | "warning" | "info"
    code: str                 # short machine-readable code
    location_id: str | None   # station/industry id, or None for global issues
    message: str
    x: float | None = None    # layout coordinate for placement (model inches)
    y: float | None = None


def validate_layout_annotations(
    layout: Layout,
    stations_config: StationsConfig,
) -> list[ValidationIssue]:
    """Check layout NOTE annotations against the stations config.

    Checks:
      - REFERENCE: MP_ZERO note is present
      - Every station entry has a STATION: note (for milepost)
      - Every station entry has a SIDING: note (for siding capacity)
      - Every industry entry has an INDUSTRY: or SIDING: note (for spur length)
      - All found NOTEs snap close to a track (warn if far)
    """
    issues: list[ValidationIssue] = []

    segments = list_annotated_segments(layout)
    by_type: dict[str, set[str]] = {}
    for seg in segments:
        by_type.setdefault(seg.annotation_type, set()).add(seg.annotation_id)

    # REFERENCE: MP_ZERO must exist for milepost calculations
    refs = by_type.get("reference", set())
    if "MP_ZERO" not in refs:
        issues.append(ValidationIssue(
            severity="error",
            code="MISSING_REFERENCE",
            location_id=None,
            message="No 'REFERENCE: MP_ZERO' note found — milepost calculations will be null",
        ))

    station_ids = by_type.get("station", set())
    siding_ids = by_type.get("siding", set()) | by_type.get("industry", set())

    for entry in stations_config.stations:
        is_station = any(t in entry.types for t in ("station", "yard", "staging"))
        is_industry = "industry" in entry.types

        if is_station:
            if entry.id not in station_ids:
                issues.append(ValidationIssue(
                    severity="warning",
                    code="MISSING_STATION_NOTE",
                    location_id=entry.id,
                    message=(
                        f"No 'STATION: {entry.id}' note — "
                        f"milepost for '{entry.name}' will be null"
                    ),
                ))
            if entry.id not in siding_ids:
                issues.append(ValidationIssue(
                    severity="warning",
                    code="MISSING_SIDING_NOTE",
                    location_id=entry.id,
                    message=(
                        f"No 'SIDING: {entry.id}' note — "
                        f"siding capacity for '{entry.name}' will be null"
                    ),
                ))

        if is_industry:
            if entry.id not in siding_ids:
                issues.append(ValidationIssue(
                    severity="error",
                    code="MISSING_INDUSTRY_NOTE",
                    location_id=entry.id,
                    message=(
                        f"No 'INDUSTRY: {entry.id}' or 'SIDING: {entry.id}' note — "
                        f"spur length for '{entry.name}' will be null"
                    ),
                ))

    # Warn if any NOTE is far from its nearest track
    for seg in segments:
        if seg.snap_dist > _SNAP_WARN_THRESHOLD:
            # Find the note position for placement
            note_x: float | None = None
            note_y: float | None = None
            for n in layout.notes:
                if n.id == seg.note_id:
                    note_x, note_y = n.x, n.y
                    break
            issues.append(ValidationIssue(
                severity="warning",
                code="FAR_SNAP",
                location_id=seg.annotation_id,
                message=(
                    f"Note '{seg.note_text}' is {seg.snap_dist:.1f} model in from "
                    f"nearest track (threshold {_SNAP_WARN_THRESHOLD} in)"
                ),
                x=note_x, y=note_y,
            ))

    # Track-level structural checks
    main_layer_ids: set[int] = {
        idx for idx, li in layout.layers.items() if _is_main_layer(li.name)
    }
    if main_layer_ids:
        track_by_id = {t.id: t for t in layout.tracks}
        for track in layout.tracks:
            if track.layer not in main_layer_ids or track.kind == "TURNOUT":
                continue
            cx = (sum(ep.x for ep in track.endpoints) / len(track.endpoints)
                  if track.endpoints else 0.0)
            cy = (sum(ep.y for ep in track.endpoints) / len(track.endpoints)
                  if track.endpoints else 0.0)
            length_in = track.length_model_inches()

            has_main_neighbor = False
            has_nonmain_neighbor = False
            has_main_turnout_neighbor = False
            target_layer_name: str | None = None

            for ep in track.endpoints:
                if ep.connected_to is None:
                    continue
                nb = track_by_id.get(ep.connected_to)
                if nb is None:
                    continue
                if nb.layer in main_layer_ids:
                    has_main_neighbor = True
                    if nb.kind == "TURNOUT":
                        has_main_turnout_neighbor = True
                else:
                    has_nonmain_neighbor = True
                    if target_layer_name is None:
                        nb_li = layout.layers.get(nb.layer)
                        if nb_li:
                            target_layer_name = nb_li.name

            if not has_main_neighbor:
                issues.append(ValidationIssue(
                    severity="warning",
                    code="ISOLATED_MAIN_TRACK",
                    location_id=None,
                    message=(
                        f"T{track.id} ({length_in:.1f}in {track.kind}) at "
                        f"({cx:.1f}, {cy:.1f}) is on a main layer but has no "
                        f"main-layer connections — check layer assignment"
                    ),
                    x=cx, y=cy,
                ))
            elif has_main_turnout_neighbor and has_nonmain_neighbor:
                suggest = target_layer_name or "non-main layer"
                issues.append(ValidationIssue(
                    severity="warning",
                    code="LAYER_BRIDGE_TRACK",
                    location_id=None,
                    message=(
                        f"T{track.id} ({length_in:.1f}in {track.kind}) at "
                        f"({cx:.1f}, {cy:.1f}) bridges main layer and {suggest} — "
                        f"suggest moving to {suggest}"
                    ),
                    x=cx, y=cy,
                ))

    return issues


# ---------------------------------------------------------------------------
# Layout data export  (→ layout_data.json)
# ---------------------------------------------------------------------------


def build_layout_export(
    layout: Layout,
    stations_config: StationsConfig,
) -> dict:
    """Build the layout_data.json export structure.

    Returns a dict matching the schema in XTRKCAD_DATA_REQUIREMENTS.md.
    Fields that cannot be computed are null; reasons are listed in warnings[].

    Milepost approximations used here:
      - milepost_entry = MP of STATION: note (nearest switch is typically very close)
      - milepost_exit  = milepost_entry + siding_length_ft / mp_scale (non-switchback)
      - switchback exit MPs are left null (require topology analysis)
    """
    warnings: list[str] = []

    # Find REFERENCE: MP_ZERO
    ref_points = extract_reference_points(layout)
    ref = next((r for r in ref_points if r.name == "MP_ZERO"), None)
    if ref is None:
        warnings.append("No REFERENCE: MP_ZERO note — all mileposts will be null")

    # Build graph once and run Dijkstra from the reference point
    adj = _build_ep_graph(layout, None, False)
    dist_from_ref: dict[tuple[int, int], float] = {}
    if ref is not None:
        dist_from_ref = _dijkstra(adj, ref.nearest_ep)

    scale = layout.scale or "HO"

    def _ep_mp(ep: tuple[int, int]) -> float | None:
        d = dist_from_ref.get(ep, _INF)
        if d >= _INF:
            return None
        return model_in_to_proto_ft(d, scale) / stations_config.mp_scale

    # Station mileposts keyed by station_id (from STATION: notes)
    station_objects = {s.name: s for s in extract_stations(layout)}
    mp_by_id: dict[str, float | None] = {}
    for sid, sobj in station_objects.items():
        mp_by_id[sid] = _ep_mp(sobj.nearest_ep)

    # Siding / industry capacities keyed by name
    capacities = compute_capacities(layout)
    cap_by_name: dict[str, CapacityResult] = {c.name: c for c in capacities}

    # Yard track capacities
    yard_tracks = compute_yard_tracks(layout)

    # --- Stations ---
    station_entries = sorted(
        [e for e in stations_config.stations if any(t in e.types for t in ("station", "yard", "staging"))],
        key=lambda e: e.sequence,
    )
    stations_out = []
    for entry in station_entries:
        mp_entry = mp_by_id.get(entry.id)
        cap = cap_by_name.get(entry.id)
        siding_ft = cap.length_real_ft if cap else None
        siding_cars = cap.max_cars if cap else None

        if not entry.switchback and mp_entry is not None and siding_ft is not None:
            mp_exit: float | None = mp_entry + siding_ft / stations_config.mp_scale
        else:
            mp_exit = None
            if entry.switchback and mp_entry is not None:
                warnings.append(
                    f"{entry.id}: switchback — exit MP requires topology analysis, set to null"
                )

        stations_out.append({
            "station_id": entry.id,
            "milepost_entry": round(mp_entry, 3) if mp_entry is not None else None,
            "milepost_exit": round(mp_exit, 3) if mp_exit is not None else None,
            "siding_length_ft": round(siding_ft, 3) if siding_ft is not None else None,
            "siding_length_cars": siding_cars,
            "siding_description": cap.description if cap else None,
            "switchback": entry.switchback,
        })

    # --- Industries ---
    industry_entries = sorted(
        [e for e in stations_config.stations if "industry" in e.types],
        key=lambda e: e.sequence,
    )
    industries_out = []
    for entry in industry_entries:
        cap = cap_by_name.get(entry.id)
        spur_ft = cap.length_real_ft if cap else None
        spur_cars = cap.max_cars if cap else None

        # Branch milepost: nearest-endpoint MP of the industry's track
        branch_mp: float | None = None
        if cap is not None and ref is not None:
            ind_track = next((t for t in layout.tracks if t.id == cap.nearest_track_id), None)
            if ind_track:
                best = min(
                    (_ep_mp((ind_track.id, i)) for i in range(len(ind_track.endpoints))),
                    key=lambda v: v if v is not None else _INF,
                    default=None,
                )
                branch_mp = best

        industries_out.append({
            "industry_id": entry.id,
            "connected_station": entry.within_limits_of,
            "branch_milepost": round(branch_mp, 3) if branch_mp is not None else None,
            "spur_length_ft": round(spur_ft, 3) if spur_ft is not None else None,
            "spur_length_cars": spur_cars,
            "spur_description": cap.description if cap else None,
            "car_spots": entry.spots,
            "switchback": entry.switchback,
        })

    # --- Mainline segments between consecutive stations ---
    segments_out = []
    for i in range(len(station_entries) - 1):
        a = station_entries[i]
        b = station_entries[i + 1]
        mp_a = mp_by_id.get(a.id)
        mp_b = mp_by_id.get(b.id)
        length: float | None = None
        if mp_a is not None and mp_b is not None:
            length = round(abs(mp_b - mp_a) * stations_config.mp_scale, 3)
        segments_out.append({
            "from_station": a.id,
            "to_station": b.id,
            "length_ft": length,
        })

    # --- Yard tracks ---
    yard_tracks_out = [
        {
            "yard_id": yt.yard_id,
            "label": yt.label,
            "track_id": yt.nearest_track_id,
            "length_ft": round(yt.length_real_ft, 3),
            "car_capacity": yt.car_capacity,
        }
        for yt in sorted(yard_tracks, key=lambda r: (r.yard_id, r.label))
    ]

    return {
        "generated": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "layout": stations_config.layout,
        "scale": scale,
        "stations": stations_out,
        "industries": industries_out,
        "segments": segments_out,
        "yard_tracks": yard_tracks_out,
        "warnings": warnings,
    }
