"""Station distances from the milepost model (Operating Data Conventions v1).

With convention notes on the layout, stations have MPs on their railroad's
subdivision (spec 5). The distance between two stations on one subdivision
is their MP difference times that subdivision's mp_scale: MPs are measured
along the main, so this is the main-track distance. A joint station (one id
on several subdivisions, spec 4.3) is listed once on each.

Layouts with only STATION: text notes and no MP anchors keep the
track-graph distances of stations.compute_distances.
"""

from __future__ import annotations

from .conventions import read_convention_notes
from .export_v2 import build_export_v2
from .mileposts import UNIT_INCHES
from .models import Layout
from .stations import model_in_to_proto_ft


def station_distances_v2(layout: Layout) -> dict | None:
    """The milepost-based distance report for a layout, or None when it has
    no railroad note or no station with an MP (use the track-graph report)."""
    notes, _issues = read_convention_notes(layout)
    if not any(n.kind == "railroad" for n in notes):
        return None
    export = build_export_v2(layout, issues=[])
    if not any(s["mp"] is not None for s in export["stations"]):
        return None
    return distances_from_export(export)


def _scale_in(sub: dict) -> float | None:
    scale, unit = sub.get("mp_scale"), sub.get("mp_unit") or "in"
    if not isinstance(scale, (int, float)) or scale <= 0 or unit not in UNIT_INCHES:
        return None
    return float(scale) * UNIT_INCHES[unit]


def distances_from_export(export: dict) -> dict:
    """Per railroad and subdivision: its stations by MP and every pair's
    distance. Built from a layout_data.json v2 structure."""
    names: dict[str, str] = {}
    for st in export["stations"]:
        if st.get("name") and st["id"] not in names:
            names[st["id"]] = st["name"]
    scale = export.get("scale") or "HO"
    subdivisions = []
    for rr in export["railroads"]:
        for sub in rr["subdivisions"]:
            on_sub = [s for s in export["stations"]
                      if s["railroad"] == rr["id"] and s["subdivision"] == sub["id"]]
            with_mp = sorted((s for s in on_sub if s["mp"] is not None),
                             key=lambda s: (s["mp"], s["id"]))
            scale_in = _scale_in(sub)
            pairs = []
            for i, a in enumerate(with_mp):
                for b in with_mp[i + 1:]:
                    mp_dist = round(abs(b["mp"] - a["mp"]), 3)
                    model_in = mp_dist * scale_in if scale_in is not None else None
                    pairs.append({
                        "from": a["id"], "to": b["id"], "mp_distance": mp_dist,
                        "distance_model_in": _r(model_in, 2),
                        "distance_proto_ft": _r(model_in_to_proto_ft(model_in, scale), 1)
                        if model_in is not None else None,
                    })
            subdivisions.append({
                "railroad": rr["id"], "railroad_name": rr.get("name"),
                "subdivision": sub["id"], "subdivision_name": sub.get("name"),
                "mp_scale_in": scale_in,
                "stations": [{"id": s["id"], "kind": s.get("kind", "station"),
                              "name": s.get("name") or names.get(s["id"]), "mp": s["mp"]}
                             for s in with_mp],
                "no_mp": sorted(s["id"] for s in on_sub if s["mp"] is None),
                "pairs": pairs,
            })
    places: dict[str, list[dict]] = {}
    for st in export["stations"]:
        places.setdefault(st["id"], []).append(
            {"railroad": st["railroad"], "subdivision": st["subdivision"], "mp": st["mp"]})
    joint = [{"id": ident, "on": sorted(on, key=lambda p: (p["railroad"] or "",
                                                          p["subdivision"] or ""))}
             for ident, on in sorted(places.items()) if len(on) > 1]
    return {"layout": export.get("layout"), "scale": export.get("scale"),
            "subdivisions": subdivisions, "joint_stations": joint}


def _r(value: float | None, places: int) -> float | None:
    return round(value, places) if value is not None else None


def _fmt_mp(value: float) -> str:
    return f"{value:.3f}".rstrip("0").rstrip(".") if value != int(value) else f"{value:.1f}"


def _fmt_scale(scale_in: float) -> str:
    return f"{scale_in:g}"


def markdown(report: dict, title: str, md_table, route_note: str = "") -> list[str]:
    """The report as Markdown lines; md_table(headers, rows) builds a table."""
    lines = [f"# Station Distance Report — {title} ({report['scale'] or '?'})", "",
             "Distances along the main, from each station's milepost (MP) on its "
             "subdivision.", ""]
    if route_note:
        lines += [route_note, ""]
    for sub in report["subdivisions"]:
        head = sub["railroad_name"] or sub["railroad"]
        if sub["subdivision_name"] or sub["subdivision"]:
            head += f" — {sub['subdivision_name'] or sub['subdivision']}"
        lines += [f"## {head}", ""]
        if sub["mp_scale_in"] is not None:
            lines += [f"MP scale: {_fmt_scale(sub['mp_scale_in'])} in per MP.", ""]
        else:
            lines += ["_No MP scale: distances in MP only._", ""]
        if sub["stations"]:
            lines += md_table(["Station", "Name", "MP"],
                              [[s["id"], s["name"] or "", _fmt_mp(s["mp"])]
                               for s in sub["stations"]])
            lines.append("")
        if sub["pairs"]:
            lines += md_table(
                ["From", "To", "Distance (MP)", "Model (in)", "Prototype (ft)"],
                [[p["from"], p["to"], _fmt_mp(p["mp_distance"]),
                  f"{p['distance_model_in']:.2f}" if p["distance_model_in"] is not None
                  else "—",
                  f"{p['distance_proto_ft']:.1f}" if p["distance_proto_ft"] is not None
                  else "—"] for p in sub["pairs"]])
            lines.append("")
        else:
            lines += ["_Fewer than two stations with an MP._", ""]
        if sub["no_mp"]:
            lines += [f"_No MP (not paired): {', '.join(sub['no_mp'])}_", ""]
    for joint in report["joint_stations"]:
        where = "; ".join(
            f"{p['railroad']}{' ' + p['subdivision'] if p['subdivision'] else ''} MP "
            f"{_fmt_mp(p['mp']) if p['mp'] is not None else '—'}" for p in joint["on"])
        lines.append(f"_Joint station {joint['id']}: {where}._")
    return lines


def counts(report: dict) -> tuple[int, int]:
    """(stations, pairs) over all subdivisions."""
    return (sum(len(s["stations"]) for s in report["subdivisions"]),
            sum(len(s["pairs"]) for s in report["subdivisions"]))
