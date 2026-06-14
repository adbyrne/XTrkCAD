"""Station distance and capacity calculations for XTrkCAD layouts.

NOTE conventions
----------------
STATION: <name>    — mainline station; used for inter-station routing/milepost.
STORAGE: <name>    — storage track (house tracks, freight spurs); car-capacity measurement.
INDUSTRY: <name>   — industry spur; used for spur-length and branch-MP measurement.
REFERENCE: <name>  — reference point; REFERENCE: MP_ZERO marks milepost 0.

Passing track capacity is NOT annotated with a note — it is derived from the layer
name: layers named L{n}-Passing are automatically measured and matched to the
nearest STATION: note.

Resolution priority (first match wins):
  1. NOTE text with the prefix above
  2. (future) track label field
  3. layer name inference (e.g. L1-Passing → passing capacity for nearest station)

For routing distances, Dijkstra runs on the endpoint-level track graph.
For capacity, a BFS walks connected non-TURNOUT tracks from the NOTE's nearest
endpoint, summing their lengths.  TURNOUT (switch) track length is excluded
so the count reflects only the usable straight/curve track.
"""

import datetime
import heapq
import math
import re
from dataclasses import dataclass, field
from pathlib import Path

import yaml

from xtrkcad_mcp.models import LayerInfo, Layout, NoteObject, SCALE_RATIOS, cars_per_real_ft

STATION_PREFIX = "STATION:"
STORAGE_PREFIX = "STORAGE:"
INDUSTRY_PREFIX = "INDUSTRY:"
REFERENCE_PREFIX = "REFERENCE:"
YARD_TRACK_PREFIX = "YARD_TRACK:"
HOUSE_TRACK_PREFIX = "HOUSE_TRACK:"
_INF = float("inf")

_ALL_PREFIXES: list[tuple[str, str]] = [
    (STATION_PREFIX, "station"),
    (STORAGE_PREFIX, "storage"),
    (INDUSTRY_PREFIX, "industry"),
    (REFERENCE_PREFIX, "reference"),
    (YARD_TRACK_PREFIX, "yard_track"),
    (HOUSE_TRACK_PREFIX, "house_track"),
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
    terminus: bool = False        # !TERM flag in note
    switchback: bool = False      # !SWB flag in note


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
        parsed = _parse_station_note(text)
        if parsed is None:
            continue
        station_id, terminus, switchback, ref_tag = parsed

        best_track_id, best_ep_idx, best_dist = _snap_to_endpoint(
            layout, note.x, note.y, note.layer,
        )

        if best_track_id >= 0:
            stations.append(Station(
                name=station_id,
                note_id=note.id,
                x=note.x,
                y=note.y,
                nearest_ep=(best_track_id, best_ep_idx),
                snap_dist=best_dist,
                ref_tag=ref_tag,
                terminus=terminus,
                switchback=switchback,
            ))

    return stations


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


def _resolve_routing_ep(
    station: "Station",
    adj: dict,
    ref_by_name: dict,
) -> tuple[int, int]:
    """Return the graph endpoint to use for routing this station.

    If the station's nearest_ep has connections to other tracks in the
    adjacency graph, use it directly.  Otherwise, if the station carries a
    @ref_tag pointing to a known REFERENCE: note, fall back to that reference
    point's nearest_ep.

    This handles terminus stations (e.g. WP @MP_ZERO) whose STATION: note
    may sit on an isolated yard stub while the mainline entry point is marked
    by a separate REFERENCE: note placed on the connected throat switch.
    """
    ep = station.nearest_ep
    track_id = ep[0]
    # An isolated track only has intra-track edges (same track_id on both ends).
    # If any neighbour is on a different track, the ep is connected.
    has_inter = any(nb[0] != track_id for nb, _ in adj.get(ep, []))
    if not has_inter and station.ref_tag:
        ref = ref_by_name.get(station.ref_tag)
        if ref is not None:
            return ref.nearest_ep
    return ep


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
    stations = extract_stations(layout)
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

    # Resolve each station's routing endpoint, applying the @ref_tag fallback
    # for stations whose nearest track is isolated (e.g. WP @MP_ZERO).
    ref_by_name = {r.name: r for r in extract_reference_points(layout)}
    routing_eps = {
        s.name: _resolve_routing_ep(s, adj, ref_by_name)
        for s in stations
    }

    results: list[DistanceResult] = []
    for i, s1 in enumerate(stations):
        dist_from_s1 = _dijkstra(adj, routing_eps[s1.name])
        for s2 in stations[i + 1:]:
            d = dist_from_s1.get(routing_eps[s2.name], _INF)
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
    """Usable track length and car count for a STORAGE:/INDUSTRY: note or L{n}-Passing layer."""
    name: str
    kind: str                    # "passing", "storage", or "industry"
    note_id: int
    nearest_track_id: int
    length_model_in: float       # total usable track length (model inches, turnouts excluded)
    length_real_ft: float        # prototypical feet (scale-dependent, for display)
    max_cars: int                # whole cars (Fugate formula: model feet × cars/model-ft)


def _parse_industry_note(text: str) -> tuple[str, str, str | None] | None:
    """Parse 'INDUSTRY: <id> <name> [@station]' → (id, name, within_station) or None.

    The id is the first token after INDUSTRY:.  An optional token starting with
    '@' anywhere in the remainder is the connected-station reference.  All other
    tokens form the display name; if no name tokens are present the id is reused.
    """
    if not text.strip().upper().startswith(INDUSTRY_PREFIX):
        return None
    rest = text.strip()[len(INDUSTRY_PREFIX):].strip()
    if not rest:
        return None
    tokens = rest.split()
    industry_id = tokens[0]
    within: str | None = None
    name_tokens: list[str] = []
    for tok in tokens[1:]:
        if tok.startswith('@'):
            within = tok[1:]
        else:
            name_tokens.append(tok)
    name = ' '.join(name_tokens) if name_tokens else industry_id
    return industry_id, name, within


def _note_prefix(text: str) -> tuple[str, str] | None:
    """Return (kind, id) if the NOTE text matches a known capacity prefix, else None."""
    upper = text.strip().upper()
    if upper.startswith(STORAGE_PREFIX):
        return "storage", text.strip()[len(STORAGE_PREFIX):].strip()
    if upper.startswith(INDUSTRY_PREFIX):
        parsed = _parse_industry_note(text)
        return ("industry", parsed[0]) if parsed else None
    if upper.startswith(HOUSE_TRACK_PREFIX):
        return "house_track", text.strip()[len(HOUSE_TRACK_PREFIX):].strip()
    return None


def _parse_station_note(
    text: str,
) -> tuple[str, bool, bool, str | None] | None:
    """Parse 'STATION: <id> [!TERM] [!SWB] [@ref]' → (id, terminus, switchback, ref_tag) or None."""
    if not text.strip().upper().startswith(STATION_PREFIX):
        return None
    rest = text.strip()[len(STATION_PREFIX):].strip()
    if not rest:
        return None
    tokens = rest.split()
    station_id = tokens[0]
    terminus = False
    switchback = False
    ref_tag: str | None = None
    for tok in tokens[1:]:
        if tok == "!TERM":
            terminus = True
        elif tok == "!SWB":
            switchback = True
        elif tok.startswith('@') and len(tok) > 1:
            ref_tag = tok[1:]
    return station_id, terminus, switchback, ref_tag


def _extract_mp_scale(layout: Layout) -> float:
    """Return mp_scale from 'REFERENCE: MP_ZERO mp_scale=N', defaulting to 12.0."""
    for note in layout.notes:
        if note.op != 0:
            continue
        text = note.text.strip()
        if not text.upper().startswith(REFERENCE_PREFIX):
            continue
        rest = text[len(REFERENCE_PREFIX):].strip().split()
        if not rest or rest[0].upper() != "MP_ZERO":
            continue
        for tok in rest[1:]:
            if tok.lower().startswith("mp_scale="):
                try:
                    return float(tok.split("=", 1)[1])
                except ValueError:
                    pass
    return 12.0


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


_LAYER_PREF_THRESHOLD = 15.0  # model inches — use layer-filtered snap only if this close


def _snap_to_endpoint(
    layout: Layout, x: float, y: float, layer: int | None = None,
) -> tuple[int, int, float]:
    """Return (track_id, ep_idx, dist) of the nearest track endpoint to (x, y).

    If layer is given, prefer tracks on that layer: use the layer-filtered result
    when it is within _LAYER_PREF_THRESHOLD model inches; otherwise fall back to
    the nearest endpoint on any layer.  This tolerates notes whose layer was not
    saved correctly in XTrkCAD (old notes may be on layer 1 regardless of intent).
    """
    def _search(tracks: list) -> tuple[int, int, float]:
        best_id, best_ep_idx, best_dist = -1, 0, _INF
        for track in tracks:
            for ep_idx, ep in enumerate(track.endpoints):
                d = math.hypot(ep.x - x, ep.y - y)
                if d < best_dist:
                    best_dist = d
                    best_id = track.id
                    best_ep_idx = ep_idx
        return best_id, best_ep_idx, best_dist

    all_result = _search(layout.tracks)

    if layer is not None:
        same_layer = [t for t in layout.tracks if t.layer == layer]
        filt_id, filt_ep_idx, filt_dist = _search(same_layer)
        if filt_id >= 0 and filt_dist <= _LAYER_PREF_THRESHOLD:
            return filt_id, filt_ep_idx, filt_dist

    return all_result


def _snap_to_track(layout: Layout, x: float, y: float, layer: int | None = None) -> int:
    """Return the track_id of the track whose nearest endpoint is closest to (x, y)."""
    best_id, _, _ = _snap_to_endpoint(layout, x, y, layer)
    return best_id


def _project_onto_segment(
    px: float, py: float,
    x0: float, y0: float, x1: float, y1: float,
) -> tuple[float, float]:
    """Project (px, py) onto segment [x0,y0]→[x1,y1].

    Returns (t, perp_dist): t ∈ [0,1] is the clamped parameter and
    perp_dist is the distance from (px,py) to the projection point.
    """
    dx, dy = x1 - x0, y1 - y0
    seg_len_sq = dx * dx + dy * dy
    if seg_len_sq < 1e-12:
        return 0.0, math.hypot(px - x0, py - y0)
    t = ((px - x0) * dx + (py - y0) * dy) / seg_len_sq
    t = max(0.0, min(1.0, t))
    cx, cy = x0 + t * dx, y0 + t * dy
    return t, math.hypot(px - cx, py - cy)


def _snap_to_segment(
    layout: Layout, x: float, y: float, layer: int | None = None,
    exclude_turnouts: bool = False,
) -> tuple[int, float, float]:
    """Find the nearest point on any track segment to (x, y).

    Returns (track_id, t, perp_dist): t ∈ [0,1] is the parameter along the
    chord ep[0]→ep[1] and perp_dist is the perpendicular distance.
    All track types are approximated via their chord (accurate for straights;
    close enough for curves for ordering purposes).

    If layer is given, restrict search to tracks on that layer; falls back to
    all tracks if nothing is found on that layer.  When exclude_turnouts is
    True, TURNOUT tracks are skipped — useful for YARD_TRACK snapping where
    snapping to a turnout boundary gives 0 usable length.
    """
    def _search(tracks: list) -> tuple[int, float, float]:
        best_id, best_t, best_dist = -1, 0.0, _INF
        for track in tracks:
            eps = track.endpoints
            if len(eps) < 2:
                continue
            t, d = _project_onto_segment(x, y, eps[0].x, eps[0].y, eps[1].x, eps[1].y)
            if d < best_dist:
                best_dist = d
                best_id = track.id
                best_t = t
        return best_id, best_t, best_dist

    candidates = [t for t in layout.tracks if not (exclude_turnouts and t.kind == "TURNOUT")]
    all_result = _search(candidates)

    if layer is not None:
        same_layer = [t for t in candidates if t.layer == layer]
        filt_id, filt_t, filt_dist = _search(same_layer)
        if filt_id >= 0 and filt_dist <= _LAYER_PREF_THRESHOLD:
            return filt_id, filt_t, filt_dist

    return all_result


def compute_capacities(layout: Layout) -> list[CapacityResult]:
    """Compute usable track length and car capacity.

    Priority:
      1. Explicit STORAGE:/INDUSTRY: note  (note_id >= 0)
      2. Layer-based fallback for STATION: notes whose nearest passing/storage
         layer component has no explicit note (note_id == -1, inferred=True)

    The BFS walks connected non-TURNOUT tracks from the nearest track endpoint,
    so switch-throat length is excluded from the measurement.  The Fugate
    cars-per-real-foot formula (scale-dependent) converts length to car count.
    """
    if not layout.tracks:
        return []

    scale = layout.scale or "HO"
    cpf = cars_per_real_ft(scale)
    results: list[CapacityResult] = []

    for note in layout.notes:
        if note.op != 0:
            continue
        parsed = _note_prefix(note.text)
        if parsed is None:
            continue
        kind, name = parsed
        if not name:
            continue

        nearest_id = _snap_to_track(layout, note.x, note.y, note.layer)
        if nearest_id < 0:
            continue

        length_in = _bfs_usable_length(layout, nearest_id)
        model_ft = length_in / 12.0        # layout/model feet — input to Fugate formula
        proto_ft = model_in_to_proto_ft(length_in, scale)  # prototype feet for display
        max_cars = int(model_ft * cpf)     # Fugate: cars per model foot, no partial cars

        results.append(CapacityResult(
            name=name,
            kind=kind,
            note_id=note.id,
            nearest_track_id=nearest_id,
            length_model_in=length_in,
            length_real_ft=proto_ft,
            max_cars=max_cars,
        ))

    # Exclude house_track: a HOUSE_TRACK: note should not suppress layer inference
    # for that station's passing siding — the two are independent.
    existing_names = {r.name for r in results if r.kind != "house_track"}
    results.extend(_infer_layer_capacities(layout, existing_names))
    return results


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

        nearest_id, _t, _perp = _snap_to_segment(
            layout, note.x, note.y, note.layer, exclude_turnouts=True,
        )
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
# Layer-based siding capacity inference
# ---------------------------------------------------------------------------

_LAYER_SIDING_SUFFIXES: frozenset[str] = frozenset({"passing", "storage", "staging"})


def _layer_siding_category(layer_name: str) -> str | None:
    """Return capacity category if layer name suffix indicates passing/storage, else None.

    Recognises the standard 'L{n}-Suffix' and 'L{n}H-Suffix' naming scheme.
    'L1-Passing' → 'passing', 'L2-Storage' → 'storage', 'Floor' → None.
    """
    suffix = re.sub(r"^l\d+h?-", "", layer_name.lower())
    return suffix if suffix in _LAYER_SIDING_SUFFIXES else None


@dataclass
class _SidingComponent:
    """A connected group of layer-restricted tracks bounded by turnouts."""
    track_ids: set[int]
    length_model_in: float
    layer_idx: int
    kind: str                          # "passing", "storage", or "main"
    endpoints: list[tuple[float, float]]
    bounded: bool = False              # True if at least one endpoint connects to a TURNOUT


def _bfs_siding_component(
    layout: Layout,
    start_track_id: int,
    layer_idx: int,
    traverse_turnouts: bool = True,
) -> tuple[set[int], float]:
    """BFS from start_track_id restricted to layer_idx.

    traverse_turnouts=True (default, used for passing/storage layers):
        TURNOUT objects on the same layer are included — switch throat geometry
        on a siding layer counts toward the siding length.  Stops only at
        connections to a DIFFERENT layer.

    traverse_turnouts=False (used for main-line layers):
        TURNOUT objects act as opaque boundaries regardless of their layer.
        This preserves station-section granularity (24 components vs 6).

    Returns (set of track IDs, total length in model inches).
    """
    track_by_id = {t.id: t for t in layout.tracks}
    start = track_by_id.get(start_track_id)
    if start is None or start.layer != layer_idx:
        return set(), 0.0
    if not traverse_turnouts and start.kind == "TURNOUT":
        return set(), 0.0

    visited: set[int] = set()
    queue: list[int] = [start_track_id]
    total = 0.0

    while queue:
        tid = queue.pop()
        if tid in visited:
            continue
        track = track_by_id.get(tid)
        if track is None or track.layer != layer_idx:
            continue
        if not traverse_turnouts and track.kind == "TURNOUT":
            continue
        visited.add(tid)
        total += track.length_model_inches()
        for ep in track.endpoints:
            if ep.connected_to is not None and ep.connected_to not in visited:
                queue.append(ep.connected_to)

    return visited, total


def _build_siding_components(layout: Layout) -> list[_SidingComponent]:
    """Find all connected components of passing/storage/staging layer tracks."""
    siding_layers: dict[int, str] = {}
    for layer_idx, layer_info in layout.layers.items():
        cat = _layer_siding_category(layer_info.name)
        if cat is not None:
            siding_layers[layer_idx] = cat

    if not siding_layers:
        return []

    track_by_id = {t.id: t for t in layout.tracks}
    seen: set[int] = set()
    components: list[_SidingComponent] = []

    for track in layout.tracks:
        if track.id in seen:
            continue
        if track.layer not in siding_layers or track.kind == "TURNOUT":
            seen.add(track.id)
            continue

        component_ids, length = _bfs_siding_component(layout, track.id, track.layer)
        seen.update(component_ids if component_ids else {track.id})
        if not component_ids:
            continue

        comp_layer = track.layer
        eps: list[tuple[float, float]] = []
        bounded = False
        for tid in component_ids:
            t = track_by_id.get(tid)
            if t:
                for ep in t.endpoints:
                    eps.append((ep.x, ep.y))
                    if ep.connected_to is not None:
                        nb = track_by_id.get(ep.connected_to)
                        if nb is not None and nb.layer != comp_layer:
                            bounded = True

        cat = siding_layers[track.layer]
        kind = "storage" if cat in {"storage", "staging"} else "passing"
        components.append(_SidingComponent(
            track_ids=component_ids,
            length_model_in=length,
            layer_idx=track.layer,
            kind=kind,
            endpoints=eps,
            bounded=bounded,
        ))

    return components


# ---------------------------------------------------------------------------
# Main-line component inference (station throat length)
# ---------------------------------------------------------------------------

_LAYER_MAIN_SUFFIXES: frozenset[str] = frozenset({"main", "track"})


def _build_main_components(layout: Layout) -> list[_SidingComponent]:
    """Find connected components of main-line layer tracks, bounded by turnouts.

    Each component is one continuous run of main-line track between switches.
    'bounded=True' means at least one end connects to a turnout, so the component
    represents an actual station section rather than the whole unbounded main.
    """
    main_layers: dict[int, str] = {}
    for layer_idx, layer_info in layout.layers.items():
        suffix = re.sub(r"^l\d+h?-", "", layer_info.name.lower())
        if suffix in _LAYER_MAIN_SUFFIXES:
            main_layers[layer_idx] = suffix

    if not main_layers:
        return []

    track_by_id = {t.id: t for t in layout.tracks}
    seen: set[int] = set()
    components: list[_SidingComponent] = []

    for track in layout.tracks:
        if track.id in seen:
            continue
        if track.layer not in main_layers or track.kind == "TURNOUT":
            seen.add(track.id)
            continue

        # traverse_turnouts=False: L2-Main TURNOUTs remain boundaries so each
        # station section stays as a separate component.
        component_ids, length = _bfs_siding_component(
            layout, track.id, track.layer, traverse_turnouts=False
        )
        seen.update(component_ids if component_ids else {track.id})
        if not component_ids:
            continue

        eps: list[tuple[float, float]] = []
        bounded = False
        for tid in component_ids:
            t = track_by_id.get(tid)
            if t:
                for ep in t.endpoints:
                    eps.append((ep.x, ep.y))
                    if ep.connected_to is not None:
                        nb = track_by_id.get(ep.connected_to)
                        if nb is not None and nb.kind == "TURNOUT":
                            bounded = True

        components.append(_SidingComponent(
            track_ids=component_ids,
            length_model_in=length,
            layer_idx=track.layer,
            kind="main",
            endpoints=eps,
            bounded=bounded,
        ))

    return components


def _infer_main_lengths(layout: Layout) -> list[CapacityResult]:
    """Infer main-line track length at each STATION: note.

    For each station, finds the nearest main-layer component (bounded by turnouts).
    Returns CapacityResult with kind='main' and note_id=-1.
    The 'bounded' flag on the source component determines whether a mismatch
    warning against the siding length is meaningful.
    """
    stations = extract_stations(layout)
    if not stations:
        return []

    components = _build_main_components(layout)
    if not components:
        return []

    scale = layout.scale or "HO"
    cpf = cars_per_real_ft(scale)
    results: list[CapacityResult] = []

    for station in stations:
        best_comp: _SidingComponent | None = None
        best_dist = _INF

        for comp in components:
            for ex, ey in comp.endpoints:
                d = math.hypot(station.x - ex, station.y - ey)
                if d < best_dist:
                    best_dist = d
                    best_comp = comp

        if best_comp is None:
            continue

        length_in = best_comp.length_model_in
        model_ft = length_in / 12.0
        proto_ft = model_in_to_proto_ft(length_in, scale)
        max_cars = int(model_ft * cpf)
        nearest_id = next(iter(best_comp.track_ids))

        results.append(CapacityResult(
            name=station.name,
            kind="main",
            note_id=-1,
            nearest_track_id=nearest_id,
            length_model_in=length_in,
            length_real_ft=proto_ft,
            max_cars=max_cars,
            # Store bounded flag via nearest_track_id sign convention would be
            # fragile — instead we post a per-station bounded lookup below.
            # The bounded value is recorded in _main_bounded_by_station (caller).
        ))

    return results


def _infer_main_lengths_with_bounded(layout: Layout) -> tuple[list[CapacityResult], dict[str, bool]]:
    """Like _infer_main_lengths but also returns a {station_name: bounded} dict.

    Uses the station's passing siding as the geographic anchor for selecting the
    correct main component.  This avoids a common misfire where the station note
    is slightly closer to a long through-main component than to the shorter
    station-section component between its two siding switches.

    Anchor selection: among passing sidings within 1.5× the nearest passing
    distance from the station note, pick the longest one (most likely to be the
    primary passing track, not a short stub at one switch).  Falls back to the
    station note position when no passing sidings are found.
    """
    stations = extract_stations(layout)
    if not stations:
        return [], {}

    main_components = _build_main_components(layout)
    if not main_components:
        return [], {}

    siding_comps = _build_siding_components(layout)
    passing_comps = [c for c in siding_comps if c.kind == "passing"]

    scale = layout.scale or "HO"
    cpf = cars_per_real_ft(scale)
    results: list[CapacityResult] = []
    bounded_map: dict[str, bool] = {}

    for station in stations:
        # Step 1: find the best passing siding to use as the main anchor.
        # Among all passing sidings, collect those within 1.5× the nearest distance.
        # Prefer the longest (most likely the primary passing, not a short stub).
        siding_candidates: list[tuple[float, _SidingComponent]] = []
        for comp in passing_comps:
            d = min(math.hypot(station.x - ex, station.y - ey) for ex, ey in comp.endpoints)
            siding_candidates.append((d, comp))

        anchor_eps: list[tuple[float, float]]
        if siding_candidates:
            min_d = min(d for d, _ in siding_candidates)
            radius = max(min_d * 1.5, 20.0)
            nearby_sidings = [(d, c) for d, c in siding_candidates if d <= radius]
            # Pick the longest siding in the candidate set
            anchor_siding = max(nearby_sidings, key=lambda x: x[1].length_model_in)[1]
            anchor_eps = anchor_siding.endpoints
        else:
            anchor_eps = [(station.x, station.y)]

        # Step 2: find the main component nearest to the siding anchor.
        # Exclude tiny stubs (often tracks on the wrong layer) shorter than
        # this threshold — real station sections are at least a few car lengths.
        _MIN_MAIN_IN = 10.0   # model inches (~73ft prototype HO)

        best_comp: _SidingComponent | None = None
        best_dist = _INF
        for comp in main_components:
            if comp.length_model_in < _MIN_MAIN_IN:
                continue
            for ex, ey in comp.endpoints:
                for ax, ay in anchor_eps:
                    d = math.hypot(ax - ex, ay - ey)
                    if d < best_dist:
                        best_dist = d
                        best_comp = comp

        if best_comp is None:
            continue

        length_in = best_comp.length_model_in
        model_ft = length_in / 12.0
        proto_ft = model_in_to_proto_ft(length_in, scale)
        max_cars = int(model_ft * cpf)
        nearest_id = next(iter(best_comp.track_ids))

        results.append(CapacityResult(
            name=station.name,
            kind="main",
            note_id=-1,
            nearest_track_id=nearest_id,
            length_model_in=length_in,
            length_real_ft=proto_ft,
            max_cars=max_cars,
        ))
        bounded_map[station.name] = best_comp.bounded

    return results, bounded_map


def _infer_layer_capacities(
    layout: Layout,
    existing_names: set[str],
) -> list[CapacityResult]:
    """Infer passing track capacity for STATION: notes via layer-based measurement.

    For each station with no explicit capacity note, finds the nearest *passing*-kind
    component (L{n}-Passing layers).  Only falls back to storage/staging if no passing
    component exists anywhere in the layout.
    """
    stations = extract_stations(layout)
    if not stations:
        return []

    components = _build_siding_components(layout)
    if not components:
        return []

    passing_comps = [c for c in components if c.kind == "passing"]
    fallback_comps = components  # storage + staging + passing, used when no passing exists

    scale = layout.scale or "HO"
    cpf = cars_per_real_ft(scale)
    results: list[CapacityResult] = []

    eligible = [s for s in stations if s.name not in existing_names]
    if not eligible:
        return []

    # Prefer passing-layer components; fall back to all if layout has none
    search = passing_comps if passing_comps else fallback_comps

    # Exclusive Voronoi assignment: each component goes to its single nearest
    # station.  Without this, multiple stations can claim the same component
    # (e.g. a terminal yard at one end of the line claiming a mid-line siding).
    comps_for_station: dict[str, list[tuple[float, _SidingComponent]]] = {
        s.name: [] for s in eligible
    }
    for comp in search:
        best_name: str | None = None
        best_dist = _INF
        for station in eligible:
            d = min(math.hypot(station.x - ex, station.y - ey) for ex, ey in comp.endpoints)
            if d < best_dist:
                best_dist = d
                best_name = station.name
        if best_name is not None:
            comps_for_station[best_name].append((best_dist, comp))

    for station in eligible:
        owned = comps_for_station.get(station.name, [])
        if not owned:
            continue

        # Among owned components within 1.5× the nearest distance, prefer the
        # longest — avoids a short stub at one switch winning over the main siding.
        min_d = min(d for d, _ in owned)
        radius = max(min_d * 1.5, 20.0)
        nearby = [(d, c) for d, c in owned if d <= radius]
        best_comp = max(nearby, key=lambda x: x[1].length_model_in)[1]

        length_in = best_comp.length_model_in
        model_ft = length_in / 12.0
        proto_ft = model_in_to_proto_ft(length_in, scale)
        max_cars = int(model_ft * cpf)
        nearest_id = next(iter(best_comp.track_ids))

        results.append(CapacityResult(
            name=station.name,
            kind=best_comp.kind,
            note_id=-1,
            nearest_track_id=nearest_id,
            length_model_in=length_in,
            length_real_ft=proto_ft,
            max_cars=max_cars,
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
    terminus: bool = False        # True for line-end stations: mp_exit is null (no through exit)
    passing_siding: bool | None = None  # False = suppress layer inference; None = auto-detect


@dataclass
class StationsConfig:
    """Parsed stations.yaml — the reference schema for export and validation."""
    layout: str
    mp_scale: float               # layout inches per milepost unit (default 12 → 1 MP = 1 ft of layout)
    stations: list[StationEntry]


def load_stations_config(path: str | Path) -> StationsConfig:
    """Load a stations.yaml file and return a StationsConfig."""
    path = Path(path)
    with path.open(encoding="utf-8") as fh:
        data = yaml.safe_load(fh)

    entries: list[StationEntry] = []
    for s in data.get("stations", []):
        entries.append(StationEntry(
            id=s["id"],
            name=s.get("name", s["id"]),
            sequence=int(s.get("sequence", 0)),
            types=list(s.get("types", ["station"])),
            switchback=bool(s.get("switchback", False)),
            terminus=bool(s.get("terminus", False)),
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
        # First token only — ignore trailing params (e.g. "mp_scale=12" in MP_ZERO note)
        rest = text[len(REFERENCE_PREFIX):].strip().split()
        name = rest[0] if rest else ""
        if not name:
            continue

        best_id, best_ep_idx, best_dist = _snap_to_endpoint(
            layout, note.x, note.y, note.layer,
        )

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

    Milepost = layout_inches_from_ref / mp_scale.
    With mp_scale=12, 1 milepost unit == 12 layout inches of track.
    """
    stations = extract_stations(layout)
    if not stations:
        return []

    adj = _build_ep_graph(layout, None, False)
    dist_from_ref = _dijkstra(adj, ref.nearest_ep)
    ref_by_name = {r.name: r for r in extract_reference_points(layout)}

    results: list[MilepostResult] = []
    for station in stations:
        ep = _resolve_routing_ep(station, adj, ref_by_name)
        d = dist_from_ref.get(ep, _INF)
        reachable = d < _INF
        mp: float | None = d / stations_config.mp_scale if reachable else None
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
    annotation_type: str       # "station" | "siding" | "storage" | "industry" | "reference" | "yard_track" | "house_track"
    annotation_source: str     # "note" (only source implemented; "label"/"layer" are future)
    note_id: int
    note_text: str
    note_layer: int
    note_layer_name: str
    nearest_track_id: int
    nearest_track_kind: str    # "STRAIGHT" | "CURVE" | "TURNOUT" | etc.
    nearest_track_layer: int
    nearest_track_layer_name: str
    snap_dist: float
    annotation_name: str | None = None      # industries: display name parsed from note
    annotation_within: str | None = None    # industries: connected station (@STATION ref)


def list_annotated_segments(layout: Layout) -> list[AnnotatedSegment]:
    """Return one AnnotatedSegment for each NOTE with a known annotation prefix.

    Scans all text NOTEs for STATION:, STORAGE:, INDUSTRY:, HOUSE_TRACK:,
    YARD_TRACK:, and REFERENCE: prefixes and snaps each to the nearest track.
    YARD_TRACK: notes use segment-projection snap (excluding turnouts) to match
    the measurement logic in compute_yard_tracks.  All others use endpoint snap.
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

        matched_type: str | None = None
        matched_id: str | None = None
        annotation_name: str | None = None
        annotation_within: str | None = None
        for prefix, atype in _ALL_PREFIXES:
            if upper.startswith(prefix):
                matched_type = atype
                if atype == "industry":
                    parsed_ind = _parse_industry_note(text)
                    if parsed_ind is None:
                        break
                    matched_id, annotation_name, annotation_within = parsed_ind
                elif atype == "station":
                    parsed_sta = _parse_station_note(text)
                    if parsed_sta is None:
                        break
                    matched_id = parsed_sta[0]  # station ID only; flags handled by extract_stations
                elif atype == "reference":
                    rest = text[len(prefix):].strip().split()
                    matched_id = rest[0] if rest else None
                else:
                    matched_id = text[len(prefix):].strip()
                break

        if not matched_type or not matched_id:
            continue

        if matched_type in ("station", "yard_track"):
            # Use segment projection: notes are placed over the track body, not
            # necessarily near an endpoint.  YARD_TRACK additionally excludes
            # turnouts so a note adjacent to a switch measures the spur, not 0 ft.
            best_id, _t, best_dist = _snap_to_segment(
                layout, note.x, note.y, note.layer,
                exclude_turnouts=(matched_type == "yard_track"),
            )
        else:
            best_id, _ep_idx, best_dist = _snap_to_endpoint(
                layout, note.x, note.y, note.layer,
            )

        if best_id < 0:
            continue

        note_layer_info = layout.layers.get(note.layer, LayerInfo(note.layer, f"Layer {note.layer}"))
        track = track_by_id.get(best_id)
        track_layer_idx = track.layer if track else 0
        track_layer_info = layout.layers.get(track_layer_idx, LayerInfo(track_layer_idx, f"Layer {track_layer_idx}"))

        results.append(AnnotatedSegment(
            annotation_id=matched_id,
            annotation_type=matched_type,
            annotation_source="note",
            note_id=note.id,
            note_text=text,
            note_layer=note.layer,
            note_layer_name=note_layer_info.name,
            nearest_track_id=best_id,
            nearest_track_kind=track.kind if track else "UNKNOWN",
            nearest_track_layer=track_layer_idx,
            nearest_track_layer_name=track_layer_info.name,
            snap_dist=best_dist,
            annotation_name=annotation_name,
            annotation_within=annotation_within,
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


def validate_layout_annotations(
    layout: Layout,
    stations_config: StationsConfig | None = None,
) -> list[ValidationIssue]:
    """Check layout NOTE annotations and track layer placement.

    Layer checks (always run, no stations_config needed):
      - ISOLATED_MAIN_TRACK: main-layer track with no main-layer neighbours
      - LAYER_BRIDGE_TRACK: main-layer track bridging to a siding/storage layer

    Annotation checks (only run when stations_config is provided):
      - REFERENCE: MP_ZERO note is present
      - Every station entry has a STATION: note (for milepost)
      - Every industry entry has an INDUSTRY: note (for spur length)
      - All found NOTEs snap close to a track (warn if far)

    Note: passing track capacity is derived from L{n}-Passing layers, not from
    notes — no MISSING_SIDING_NOTE warning is generated.
    """
    issues: list[ValidationIssue] = []

    segments = list_annotated_segments(layout)
    by_type: dict[str, set[str]] = {}
    for seg in segments:
        by_type.setdefault(seg.annotation_type, set()).add(seg.annotation_id)

    if stations_config is not None:
        # REFERENCE: MP_ZERO must exist for milepost calculations
        refs = by_type.get("reference", set())
        if "MP_ZERO" not in refs:
            issues.append(ValidationIssue(
                severity="error",
                code="MISSING_REFERENCE",
                location_id=None,
                message="No 'REFERENCE: MP_ZERO' note found — milepost calculations will be null",
            ))

    if stations_config is not None:
        station_ids = by_type.get("station", set())

        for entry in stations_config.stations:
            is_station = any(t in entry.types for t in ("station", "yard", "staging"))

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

        # Warn if any NOTE is far from its nearest track
        for seg in segments:
            if seg.snap_dist > _SNAP_WARN_THRESHOLD:
                issues.append(ValidationIssue(
                    severity="warning",
                    code="FAR_SNAP",
                    location_id=seg.annotation_id,
                    message=(
                        f"Note '{seg.note_text}' is {seg.snap_dist:.1f} model in from "
                        f"nearest track (threshold {_SNAP_WARN_THRESHOLD} in)"
                    ),
                ))

        # Warn when a STATION: note snaps to a track on a different layer.
        # Terminus stations (WP, HC) are excluded: their yards extend beyond the
        # last milepost, so the nearest Main-layer track may be far away.
        terminus_ids = {e.id for e in stations_config.stations if e.terminus}
        station_segs = {seg.annotation_id: seg for seg in segments
                        if seg.annotation_type == "station"}
        for seg in station_segs.values():
            sid = seg.annotation_id.split()[0]   # strip @ref tag if present
            if sid in terminus_ids:
                continue
            if seg.note_layer != seg.nearest_track_layer:
                note_lname = layout.layers.get(
                    seg.note_layer, LayerInfo(seg.note_layer, f"Layer {seg.note_layer}")
                ).name
                track_lname = seg.nearest_track_layer_name
                issues.append(ValidationIssue(
                    severity="warning",
                    code="STATION_NOTE_LAYER_MISMATCH",
                    location_id=sid,
                    message=(
                        f"STATION: {seg.annotation_id} note is on layer '{note_lname}' "
                        f"but snaps to T{seg.nearest_track_id} ({seg.nearest_track_kind}) "
                        f"on layer '{track_lname}' — move note over a {note_lname} track"
                    ),
                ))

    track_by_id = {t.id: t for t in layout.tracks}
    main_layer_idxs: set[int] = set()
    siding_layer_idxs: set[int] = set()
    for layer_idx, layer_info in layout.layers.items():
        suffix = re.sub(r"^l\d+h?-", "", layer_info.name.lower())
        if suffix in _LAYER_MAIN_SUFFIXES:
            main_layer_idxs.add(layer_idx)
        elif suffix in _LAYER_SIDING_SUFFIXES:
            siding_layer_idxs.add(layer_idx)

    def _track_coords(t: object) -> str:
        """Return a short '(x,y)' coordinate string for the track's midpoint."""
        eps = getattr(t, "endpoints", [])
        if not eps:
            return ""
        if len(eps) >= 2:
            cx = sum(ep.x for ep in eps) / len(eps)
            cy = sum(ep.y for ep in eps) / len(eps)
        else:
            cx, cy = eps[0].x, eps[0].y
        return f"({cx:.1f}, {cy:.1f})"

    if main_layer_idxs:
        for track in layout.tracks:
            if track.layer not in main_layer_idxs or track.kind == "TURNOUT":
                continue

            layer_info_obj = layout.layers.get(track.layer, LayerInfo(track.layer, f"Layer {track.layer}"))
            layer_name_str = layer_info_obj.name
            coords = _track_coords(track)
            length_str = f"{track.length_model_inches():.1f}in"

            neighbors = [
                track_by_id[ep.connected_to]
                for ep in track.endpoints
                if ep.connected_to is not None and ep.connected_to in track_by_id
            ]

            has_main_neighbor = any(nb.layer in main_layer_idxs for nb in neighbors)
            siding_neighbors = [nb for nb in neighbors if nb.layer in siding_layer_idxs]

            if not has_main_neighbor:
                # Completely isolated from the main layer — likely wrong layer entirely.
                if siding_neighbors:
                    nb_layers = {layout.layers.get(nb.layer, LayerInfo(nb.layer, f"Layer {nb.layer}")).name
                                 for nb in siding_neighbors}
                    suggest = f"connects to {', '.join(sorted(nb_layers))} — should probably be on that layer"
                else:
                    suggest = "may be on the wrong layer"
                issues.append(ValidationIssue(
                    severity="warning",
                    code="ISOLATED_MAIN_TRACK",
                    location_id=None,
                    message=(
                        f"Track {track.id} ({track.kind}, {length_str}) "
                        f"on {layer_name_str} at {coords}: "
                        f"no connections to other main-layer tracks — {suggest}"
                    ),
                ))
            elif siding_neighbors:
                # Has both a main-layer neighbor AND a siding-layer neighbor:
                # this is a bridge/throat connector that belongs on the siding layer.
                nb_layers = {layout.layers.get(nb.layer, LayerInfo(nb.layer, f"Layer {nb.layer}")).name
                             for nb in siding_neighbors}
                issues.append(ValidationIssue(
                    severity="warning",
                    code="LAYER_BRIDGE_TRACK",
                    location_id=None,
                    message=(
                        f"Track {track.id} ({track.kind}, {length_str}) "
                        f"on {layer_name_str} at {coords}: "
                        f"connects main-layer switch to {', '.join(sorted(nb_layers))} track — "
                        f"should be on the siding layer (placed on wrong active layer)"
                    ),
                ))

    return issues


# ---------------------------------------------------------------------------
# Layout data export  (→ layout_data.json)
# ---------------------------------------------------------------------------


def _build_config_from_notes(layout: Layout) -> StationsConfig:
    """Derive a StationsConfig entirely from the layout's STATION: notes.

    Flags in the note drive station properties:
      !TERM → terminus=True   !SWB → switchback=True
    mp_scale is read from 'REFERENCE: MP_ZERO mp_scale=N' (default 12.0).
    Station sequence is assigned in ascending milepost order.
    """
    mp_scale = _extract_mp_scale(layout)

    station_list = extract_stations(layout)

    # Quick Dijkstra pass to order stations by milepost
    ref_points = extract_reference_points(layout)
    ref = next((r for r in ref_points if r.name == "MP_ZERO"), None)
    ref_by_name = {r.name: r for r in ref_points}
    adj = _build_ep_graph(layout, None, False)
    dist_from_ref: dict[tuple[int, int], float] = {}
    if ref is not None:
        dist_from_ref = _dijkstra(adj, ref.nearest_ep)

    def _mp(s: Station) -> float:
        d = dist_from_ref.get(s.nearest_ep, _INF)
        if d < _INF:
            return d / mp_scale
        # Terminus / co-located stations use @ref for milepost (e.g. WP @MP_ZERO)
        if s.ref_tag is not None:
            linked = ref_by_name.get(s.ref_tag)
            if linked is not None:
                d_ref = dist_from_ref.get(linked.nearest_ep, _INF)
                if d_ref < _INF:
                    return d_ref / mp_scale
        return _INF

    station_list_sorted = sorted(station_list, key=_mp)

    entries = [
        StationEntry(
            id=s.name,
            name=s.name,
            sequence=idx,
            types=["station"],
            switchback=s.switchback,
            terminus=s.terminus,
        )
        for idx, s in enumerate(station_list_sorted)
    ]

    return StationsConfig(
        layout=layout.title1 or "",
        mp_scale=mp_scale,
        stations=entries,
    )


def build_layout_export(
    layout: Layout,
    stations_config: StationsConfig | None = None,
) -> dict:
    """Build the layout_data.json export structure.

    Returns a dict matching the schema in XTRKCAD_DATA_REQUIREMENTS.md.
    Fields that cannot be computed are null; reasons are listed in warnings[].

    Milepost and exit MP rules:
      - milepost_entry: MP of STATION: note via Dijkstra; or REFERENCE: point if
        Dijkstra fails and note carries @<ref> tag (e.g. WP @MP_ZERO)
      - milepost_exit (terminus): null — trains reverse out the same end, no through exit
      - milepost_exit (non-switchback): milepost_entry + siding_length / mp_scale
      - milepost_exit (switchback, no @ref): milepost_entry + main_length / mp_scale
      - milepost_exit (switchback, @ref tag + Dijkstra succeeded): MP of the named
        REFERENCE: point (e.g. STATION: MC @MC_EXIT resolves MC's exit switch)
    """
    if stations_config is None:
        stations_config = _build_config_from_notes(layout)

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
        return d / stations_config.mp_scale   # d is layout inches; mp_scale is layout in/MP

    # Station mileposts keyed by station_id (from STATION: notes)
    # exit_ref_by_id: stations whose @ref resolves the exit switch (not entry)
    ref_points_by_name = {r.name: r for r in ref_points}
    station_objects = {s.name: s for s in extract_stations(layout)}
    mp_by_id: dict[str, float | None] = {}
    exit_ref_by_id: dict[str, object] = {}
    for sid, sobj in station_objects.items():
        mp_dijkstra = _ep_mp(sobj.nearest_ep)
        if sobj.ref_tag is not None:
            linked_ref = ref_points_by_name.get(sobj.ref_tag)
            if mp_dijkstra is None:
                # Dijkstra failed — @ref resolves entry (e.g. WP @MP_ZERO)
                mp_by_id[sid] = _ep_mp(linked_ref.nearest_ep) if linked_ref else None
            else:
                # Dijkstra succeeded — @ref marks the exit switch (e.g. MC @MC_EXIT)
                mp_by_id[sid] = mp_dijkstra
                exit_ref_by_id[sid] = linked_ref
        else:
            mp_by_id[sid] = mp_dijkstra

    # All capacities keyed by name; split house-track entries separately so they
    # don't shadow passing/storage capacity for the same station ID.
    capacities = compute_capacities(layout)
    cap_by_name: dict[str, CapacityResult] = {c.name: c for c in capacities if c.kind != "house_track"}
    house_by_name: dict[str, CapacityResult] = {c.name: c for c in capacities if c.kind == "house_track"}

    # Main-line track lengths at each station (bounded components only get warnings)
    main_caps, main_bounded = _infer_main_lengths_with_bounded(layout)
    main_by_id: dict[str, CapacityResult] = {r.name: r for r in main_caps}

    _MAIN_SIDING_WARN_THRESHOLD = 0.25   # 25% difference triggers a warning

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
        siding_in = cap.length_model_in if cap else None
        siding_cars = cap.max_cars if cap else None

        main_cap = main_by_id.get(entry.id)
        main_ft = main_cap.length_real_ft if main_cap else None
        main_in = main_cap.length_model_in if main_cap else None
        main_cars = main_cap.max_cars if main_cap else None

        # Warn on significant mismatch only when main component is switch-bounded
        # (unbounded = whole continuous main line, comparison is not meaningful)
        if (siding_ft is not None and main_ft is not None
                and main_bounded.get(entry.id, False)):
            diff = abs(siding_ft - main_ft) / max(siding_ft, main_ft)
            if diff > _MAIN_SIDING_WARN_THRESHOLD:
                pct = int(diff * 100)
                warnings.append(
                    f"{entry.id}: main track ({main_ft:.0f}ft) and siding "
                    f"({siding_ft:.0f}ft) differ by {pct}% — "
                    f"verify switch placement at station limits"
                )

        exit_ref = exit_ref_by_id.get(entry.id)
        if entry.terminus:
            # Terminus: no through exit — trains reverse out the same end
            mp_exit: float | None = None
        elif exit_ref is not None:
            # Explicit exit switch via REFERENCE: note (e.g. MC @MC_EXIT)
            mp_exit = _ep_mp(exit_ref.nearest_ep)
        elif not entry.switchback and mp_entry is not None and siding_in is not None:
            mp_exit = mp_entry + siding_in / stations_config.mp_scale
        elif entry.switchback and mp_entry is not None and main_in is not None:
            # Regular switchback: exit = entry + main section length between switches
            mp_exit = mp_entry + main_in / stations_config.mp_scale
        else:
            mp_exit = None
            if entry.switchback and mp_entry is not None:
                warnings.append(
                    f"{entry.id}: switchback — exit MP could not be computed (no main length)"
                )

        house_cap = house_by_name.get(entry.id)
        stations_out.append({
            "station_id": entry.id,
            "milepost_entry": round(mp_entry, 3) if mp_entry is not None else None,
            "milepost_exit": round(mp_exit, 3) if mp_exit is not None else None,
            "siding_length_ft": round(siding_ft, 3) if siding_ft is not None else None,
            "siding_length_cars": siding_cars,
            "house_track_ft": round(house_cap.length_real_ft, 3) if house_cap else None,
            "house_track_cars": house_cap.max_cars if house_cap else None,
            "main_length_ft": round(main_ft, 3) if main_ft is not None else None,
            "main_length_cars": main_cars,
            "switchback": entry.switchback,
            "terminus": entry.terminus,
        })

    # --- Industries (derived from INDUSTRY: notes, not YAML) ---
    all_segments = list_annotated_segments(layout)
    industry_segs = [s for s in all_segments if s.annotation_type == "industry"]
    industries_out = []
    for seg in industry_segs:
        cap = cap_by_name.get(seg.annotation_id)

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
            "industry_id": seg.annotation_id,
            "name": seg.annotation_name,
            "connected_station": seg.annotation_within,
            "branch_milepost": round(branch_mp, 3) if branch_mp is not None else None,
            "car_spots": cap.max_cars if cap is not None else None,
        })

    industries_out.sort(
        key=lambda i: i["branch_milepost"] if i["branch_milepost"] is not None else _INF
    )

    # --- Mainline segments between consecutive stations ---
    segments_out = []
    for i in range(len(station_entries) - 1):
        a = station_entries[i]
        b = station_entries[i + 1]
        mp_a = mp_by_id.get(a.id)
        mp_b = mp_by_id.get(b.id)
        length: float | None = None
        if mp_a is not None and mp_b is not None:
            model_in_diff = abs(mp_b - mp_a) * stations_config.mp_scale
            length = round(model_in_to_proto_ft(model_in_diff, scale), 3)
        segments_out.append({
            "from_station": a.id,
            "to_station": b.id,
            "length_ft": length,
        })

    # --- Yard tracks ---
    yard_tracks = compute_yard_tracks(layout)
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
        "mp_scale": stations_config.mp_scale,
        "stations": stations_out,
        "industries": industries_out,
        "segments": segments_out,
        "yard_tracks": yard_tracks_out,
        "warnings": warnings,
    }
