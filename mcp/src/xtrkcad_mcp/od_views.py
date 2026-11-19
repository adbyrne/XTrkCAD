"""Op Density views from the convention notes (spec v1.1, section 9.1).

Storage, in Op Density, is how many cars can be parked on the layout while
trains still run on the main and passing tracks. Besides Fugate's
by-layer view (server.get_operation_density), the notes give two more:

- industries and yards: each industry's car spots against its track
  capacity, yard tracks, storage tracks. Industries on main or passing
  track are listed apart: a car spotted there blocks the line.
- railroad service: each facility's car_spots. Only spots a note gives
  count (no default); they are parked cars, so they count as storage.
"""

from __future__ import annotations

from typing import Callable

from .conventions import read_convention_notes, snap_note
from .export_v2 import build_export_v2
from .models import Layout

# Categories whose track Fugate's view already counts by length: a
# facility on such a track adds no spots of its own to storage.
_COUNTED_BY_LENGTH = ("storage", "staging", "passing")
_ON_MAIN = ("mainline", "passing")


def note_views(layout: Layout, category: Callable[[int], str]) -> dict | None:
    """The industries-and-yards and service views, or None when the layout
    has no convention notes. ``category`` maps a layer index to its Op
    Density category, as the by-layer view uses."""
    notes, _issues = read_convention_notes(layout)
    if not notes:
        return None
    export = build_export_v2(layout, issues=[])
    by_id = {t.id: t for t in layout.tracks}
    on_main: set[str] = set()
    for note in notes:
        if note.kind in ("industry", "team_track"):
            track, _dist = snap_note(layout, note)
            if track is not None and category(track.layer) in _ON_MAIN:
                on_main.add(note.id)
    return {"industries_and_yards": _industries_and_yards(export, on_main),
            "service": _service(export, by_id, category)}


def _industries_and_yards(export: dict, on_main: set[str]) -> dict:
    industries = []
    for kind, section in (("industry", "industries"), ("team_track", "team_tracks")):
        for ind in export[section]:
            cap = ind["spur_capacity_cars"]
            industries.append({
                "id": ind["id"], "kind": kind, "station": ind["station"],
                "car_spots": ind["car_spots"], "capacity_cars": cap,
                "unserved_cars": max(cap - ind["car_spots"], 0) if cap is not None else None,
                "on_main": ind["id"] in on_main,
            })
    industries.sort(key=lambda i: i["id"])
    yards = []
    for yard in export["yards"]:
        tracks = [{"label": t["label"], "function": t["function"],
                   "capacity_cars": t["capacity_cars"]} for t in yard["tracks"]]
        yards.append({"id": yard["id"], "kind": yard["kind"], "tracks": tracks,
                      "capacity_cars": sum(t["capacity_cars"] or 0 for t in tracks)})
    storage = [{"id": st["id"], "car_spots": st["car_spots"],
                "capacity_cars": st["capacity_cars"]} for st in export["storage"]]
    off_main = [i for i in industries if not i["on_main"]]
    return {
        "industries": industries,
        "yards": yards,
        "storage": storage,
        "totals": {
            "industry_spots": sum(i["car_spots"] for i in off_main),
            "industry_capacity_cars": sum(i["capacity_cars"] or 0 for i in off_main),
            "on_main_spots": sum(i["car_spots"] for i in industries if i["on_main"]),
            "yard_capacity_cars": sum(y["capacity_cars"] for y in yards),
            "storage_spots": sum(st["car_spots"] or 0 for st in storage),
            "storage_capacity_cars": sum(st["capacity_cars"] or 0 for st in storage),
        },
    }


def _service(export: dict, by_id: dict, category: Callable[[int], str]) -> dict:
    facilities = []
    added = 0
    for fac in export["facilities"]:
        spots = fac["car_spots"]
        facilities.append({
            "id": fac["id"], "type": fac["type"], "location": fac["location"],
            "track_id": fac["track_id"], "car_spots": spots,
            "capacity_cars": fac["capacity_cars"], "counted": spots is not None,
        })
        track = by_id.get(fac["track_id"])
        if spots is not None and (track is None
                                  or category(track.layer) not in _COUNTED_BY_LENGTH):
            added += spots
    return {
        "facilities": facilities,
        "car_spots": sum(f["car_spots"] or 0 for f in facilities),
        "storage_car_spots": added,
        "not_counted": [f["id"] for f in facilities if not f["counted"]],
    }


def _n(value) -> str:
    return "-" if value is None else str(value)


def view_sections(data: dict) -> list[tuple[str, list[tuple]]]:
    """The report sections for views 2 and 3: (heading, blocks), each block
    ("table", headers, rows) or ("text", line). Empty without notes."""
    sections: list[tuple[str, list[tuple]]] = []
    iy = data.get("industries_and_yards")
    if iy is not None:
        tot = iy["totals"]
        blocks: list[tuple] = [
            ("text", f"Industry car spots: {tot['industry_spots']} on "
                     f"{tot['industry_capacity_cars']} cars of industry track. "
                     f"Yard tracks: {tot['yard_capacity_cars']} cars."),
        ]
        if iy["industries"]:
            blocks.append(("table",
                           ["Industry", "Kind", "Station", "Car spots", "Track (cars)",
                            "Unserved (cars)", "On main/passing"],
                           [[i["id"], i["kind"], _n(i["station"]), str(i["car_spots"]),
                             _n(i["capacity_cars"]), _n(i["unserved_cars"]),
                             "yes" if i["on_main"] else ""] for i in iy["industries"]]))
        if tot["on_main_spots"]:
            blocks.append(("text", f"On main or passing track: {tot['on_main_spots']} car "
                                   "spots, not storage (a spotted car blocks the line)."))
        if iy["yards"]:
            blocks.append(("table", ["Yard", "Kind", "Tracks", "Cars"],
                           [[y["id"], _n(y["kind"]), str(len(y["tracks"])),
                             str(y["capacity_cars"])] for y in iy["yards"]]))
        if iy["storage"]:
            blocks.append(("table", ["Storage track", "Car spots", "Track (cars)"],
                           [[s["id"], _n(s["car_spots"]), _n(s["capacity_cars"])]
                            for s in iy["storage"]]))
        sections.append(("Industries and Yards", blocks))
    service = data.get("service")
    if service is not None:
        blocks = [("text", f"Car spots counted as storage: {service['storage_car_spots']} "
                           f"(of {service['car_spots']} at facilities; spots on track "
                           "already counted as storage by layer are not added twice).")]
        if service["facilities"]:
            blocks.append(("table",
                           ["Facility", "Type", "Location", "Car spots", "Track (cars)",
                            "Counted"],
                           [[f["id"], _n(f["type"]), _n(f["location"]), _n(f["car_spots"]),
                             _n(f["capacity_cars"]), "yes" if f["counted"] else "no"]
                            for f in service["facilities"]]))
        if service["not_counted"]:
            blocks.append(("text", "Not counted (no car_spots): "
                                   + ", ".join(service["not_counted"])))
        sections.append(("Railroad Service", blocks))
    return sections


def txt_table(headers: list[str], rows: list[list[str]]) -> list[str]:
    widths = [max(len(h), max((len(r[i]) for r in rows), default=0))
              for i, h in enumerate(headers)]

    def line(cells):
        return "  " + "  ".join(c.ljust(w) for c, w in zip(cells, widths)).rstrip()
    return [line(headers), line(["-" * w for w in widths])] + [line(r) for r in rows]
