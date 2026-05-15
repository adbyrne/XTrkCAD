"""Station distance and siding/storage capacity calculations for XTrkCAD layouts.

NOTE conventions
----------------
STATION: <name>  — mainline station; used for inter-station routing distances.
SIDING: <name>   — siding track group; used for car-capacity measurement.
STORAGE: <name>  — storage/yard track group; used for car-capacity measurement.

For routing distances, Dijkstra runs on the endpoint-level track graph.
For capacity, a BFS walks connected non-TURNOUT tracks from the NOTE's nearest
endpoint, summing their lengths.  TURNOUT (switch) track length is excluded
so the count reflects only the usable straight/curve track.
"""

import heapq
import math
from dataclasses import dataclass, field

from xtrkcad_mcp.models import Layout, NoteObject, SCALE_RATIOS, cars_per_real_ft

STATION_PREFIX = "STATION:"
SIDING_PREFIX = "SIDING:"
STORAGE_PREFIX = "STORAGE:"
_INF = float("inf")


@dataclass
class Station:
    """A STATION: note snapped to its nearest track endpoint."""
    name: str
    note_id: int
    x: float           # NOTE position x (model inches)
    y: float           # NOTE position y (model inches)
    nearest_ep: tuple[int, int]   # (track_id, ep_idx)
    snap_dist: float              # distance from NOTE to nearest endpoint


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
        name = text[len(STATION_PREFIX):].strip()
        if not name:
            continue

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
    name: str
    kind: str                    # "siding" or "storage"
    note_id: int
    nearest_track_id: int
    length_model_in: float       # total usable track length (model inches, turnouts excluded)
    length_real_ft: float        # prototypical feet (scale-dependent, for display)
    max_cars: int                # whole cars (Fugate formula: model feet × cars/model-ft)


def _note_prefix(text: str) -> tuple[str, str] | None:
    """Return (kind, name) if the NOTE text matches SIDING: or STORAGE:, else None."""
    upper = text.strip().upper()
    if upper.startswith(SIDING_PREFIX):
        return "siding", text.strip()[len(SIDING_PREFIX):].strip()
    if upper.startswith(STORAGE_PREFIX):
        return "storage", text.strip()[len(STORAGE_PREFIX):].strip()
    return None


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


def compute_capacities(layout: Layout) -> list[CapacityResult]:
    """Compute usable track length and car capacity for every SIDING:/STORAGE: note.

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

        nearest_id = _snap_to_track(layout, note.x, note.y)
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

    return results
