"""A simple timetable from the layout's operating data and an operations file.

Tutorial-level (operating-conventions tutorial, chapter 13): the layout
gives the stations, their MPs, the yard-limit zones and the car spots (the
v2 export); a small operations file gives what the layout can't: the fast
clock, track speeds, how long a stop takes, and the trains. An operations
program (IOTtrains) draws on far more; this is not its input format.

Operations file, schema_version 1 (YAML or JSON)::

    schema_version: 1
    clock: {ratio: 3}                        # fast clock, 3:1
    speed: {max_mph: 30, yard_limit_mph: 15}
    dwell: {station_min: 10, per_car_spot: 5}
    trains:
      - {id: "1", name: Hillside Local, departs: "07:00",
         route: [HY, CE, HY], work: [AL, BI]}

A train runs along the main from each station of its route to the next.
Speeds are in MPs per hour: the MP is the timetable's mile. Inside a
yard-limit zone it runs at ``yard_limit_mph``. It stops where its route
turns, and the first time it reaches a station in ``work``; a stop takes
``station_min`` plus ``per_car_spot`` for each car spot of the industries
and team tracks switched from that station, if the train works it there.
Elsewhere it passes without stopping.
"""

from __future__ import annotations

import re

from xtrkcad_mcp.models import SCALE_RATIOS

SCHEMA_VERSION = 1
_UNIT_IN = {"in": 1.0, "ft": 12.0, "cm": 1 / 2.54, "m": 100 / 2.54}


class TimetableError(ValueError):
    """The operations file can't be used with this layout."""


def _positive(section: dict, key: str, where: str, default=None) -> float:
    value = section.get(key, default)
    if not isinstance(value, (int, float)) or isinstance(value, bool) or value <= 0:
        raise TimetableError(f"{where}.{key} must be more than 0")
    return float(value)


def _not_negative(section: dict, key: str, where: str, default) -> float:
    value = section.get(key, default)
    if not isinstance(value, (int, float)) or isinstance(value, bool) or value < 0:
        raise TimetableError(f"{where}.{key} must be 0 or more")
    return float(value)


def _clock(minutes: float) -> str:
    m = int(round(minutes)) % (24 * 60)
    return f"{m // 60:02d}:{m % 60:02d}"


def _merge(zones: list[tuple[float, float]]) -> list[tuple[float, float]]:
    out: list[list[float]] = []
    for a, b in sorted((min(a, b), max(a, b)) for a, b in zones):
        if out and a <= out[-1][1]:
            out[-1][1] = max(out[-1][1], b)
        else:
            out.append([a, b])
    return [(a, b) for a, b in out]


def _run_minutes(a: float, b: float, zones, max_mph: float, yl_mph: float) -> float:
    lo, hi = min(a, b), max(a, b)
    slow = sum(max(0.0, min(hi, z1) - max(lo, z0)) for z0, z1 in zones)
    return 60.0 * ((hi - lo - slow) / max_mph + slow / yl_mph)


def _model_mph(export, line, mph: float, ratio: float) -> float | None:
    """The scale speed a train must really run at to keep the timetable:
    ``mph`` MPs per fast hour, on a ``ratio``:1 clock, at the line's MP scale."""
    rr = next((r for r in export.get("railroads", []) if r.get("id") == line[0]), None)
    sub = next((s for s in (rr or {}).get("subdivisions", []) if s.get("id") == line[1]), None)
    if not sub or not sub.get("mp_scale") or sub.get("mp_unit") not in _UNIT_IN:
        return None
    mp_in = float(sub["mp_scale"]) * _UNIT_IN[sub["mp_unit"]]
    scale = SCALE_RATIOS.get(str(export.get("scale") or ""), 87.1)
    return round(mph * ratio * mp_in * scale / 63360.0, 1)


def build_timetable(export: dict, ops: dict) -> dict:
    """The timetable for ``ops`` on the layout whose v2 export is ``export``.

    Returns {"trains": [...], "table": markdown}. Each train has its stops
    in running order (station, leg, arrive, depart, stop: a pass has
    arrive None and depart the passing time), its running time in
    fast-clock and real minutes, and ``model_mph``: the scale speeds a model
    must really run at to keep the timetable. Raises TimetableError for a file it can't use.
    """
    if ops.get("schema_version") != SCHEMA_VERSION:
        raise TimetableError(f"schema_version must be {SCHEMA_VERSION}")
    ratio = _positive(ops.get("clock") or {}, "ratio", "clock")
    speed = ops.get("speed") or {}
    max_mph = _positive(speed, "max_mph", "speed")
    yl_mph = _positive(speed, "yard_limit_mph", "speed", default=max_mph)
    dwell = ops.get("dwell") or {}
    station_min = _not_negative(dwell, "station_min", "dwell", 10)
    per_spot = _not_negative(dwell, "per_car_spot", "dwell", 5)
    trains = ops.get("trains")
    if not isinstance(trains, list) or not trains:
        raise TimetableError("trains: give at least one train")

    spots: dict[str, int] = {}
    for loc in export.get("industries", []) + export.get("team_tracks", []):
        if loc.get("station"):
            spots[loc["station"]] = spots.get(loc["station"], 0) + int(loc.get("car_spots") or 0)

    results, columns, rows = [], [], {}
    for train in trains:
        results.append(_train(train, export, spots, max_mph, yl_mph, station_min, per_spot,
                              ratio, columns, rows))
    return {"trains": results, "table": _table(columns, rows)}


def _train(train, export, spots, max_mph, yl_mph, station_min, per_spot, ratio,
           columns, rows) -> dict:
    tid = str(train.get("id", "")).strip()
    if not tid:
        raise TimetableError("a train has no id")
    who = f"train {tid}"
    departs = train.get("departs")
    m = re.fullmatch(r"(\d{1,2}):(\d{2})", str(departs or ""))
    if not m or int(m.group(1)) > 23 or int(m.group(2)) > 59:
        raise TimetableError(f"{who}: departs must be HH:MM")
    route = [str(s) for s in train.get("route") or []]
    if len(route) < 2:
        raise TimetableError(f"{who}: route needs at least 2 stations")

    known = {s["id"] for s in export.get("stations", [])}
    for sid in route:
        if sid not in known:
            raise TimetableError(f"{who}: route: unknown station {sid!r}")
    first = next(s for s in export["stations"] if s["id"] == route[0])
    line = (first.get("railroad"), first.get("subdivision"))
    on_line = {s["id"]: s for s in export["stations"]
               if (s.get("railroad"), s.get("subdivision")) == line}
    for sid in route:
        if sid not in on_line:
            raise TimetableError(f"{who}: route: {sid!r} isn't on the same line as {route[0]!r}")
    zones = _merge([(z["mp_start"], z["mp_end"]) for z in export.get("yard_limits", [])
                    if (z.get("railroad"), z.get("subdivision")) == line])

    legs = []
    for a, b in zip(route, route[1:]):
        lo, hi = sorted((on_line[a]["mp"], on_line[b]["mp"]))
        passed = sorted((s for s in on_line.values()
                         if lo < s["mp"] < hi and s["id"] not in (a, b)),
                        key=lambda s: s["mp"], reverse=on_line[b]["mp"] < on_line[a]["mp"])
        legs.append([on_line[a]] + passed + [on_line[b]])
    reached = {s["id"] for leg in legs for s in leg}
    work = [str(s) for s in train.get("work") or []]
    for sid in work:
        if sid not in reached:
            raise TimetableError(f"{who}: works {sid!r}, which isn't on its route")

    labels = ([tid] if len(legs) == 1 else
              [f"{tid} out", f"{tid} back"] if len(legs) == 2 else
              [f"{tid} leg {n}" for n in range(1, len(legs) + 1)])
    columns.extend(labels)

    t = int(m.group(1)) * 60 + int(m.group(2))
    start, worked, stops = t, set(), []

    def cell(station, label, text):
        row = rows.setdefault(station["id"], {"station": station, "cells": {}})
        row["cells"][label] = text

    for n, (leg, label) in enumerate(zip(legs, labels)):
        if n == 0:
            stops.append({"station": leg[0]["id"], "leg": 1, "arrive": None,
                          "depart": _clock(t), "stop": True})
            cell(leg[0], label, _clock(t))
        else:
            cell(leg[0], label, stops[-1]["depart"])
        for k, st in enumerate(leg[1:], start=1):
            t += _run_minutes(leg[k - 1]["mp"], st["mp"], zones, max_mph, yl_mph)
            end_of_leg = k == len(leg) - 1
            works = st["id"] in work and st["id"] not in worked
            if end_of_leg and n == len(legs) - 1:
                stops.append({"station": st["id"], "leg": n + 1, "arrive": _clock(t),
                              "depart": None, "stop": True})
                cell(st, label, _clock(t))
            elif end_of_leg or works:
                arrive = t
                t += station_min + (per_spot * spots.get(st["id"], 0) if works else 0)
                if works:
                    worked.add(st["id"])
                stops.append({"station": st["id"], "leg": n + 1, "arrive": _clock(arrive),
                              "depart": _clock(t), "stop": True})
                cell(st, label, _clock(arrive) if end_of_leg
                     else f"{_clock(arrive)} – {_clock(t)}")
            else:
                stops.append({"station": st["id"], "leg": n + 1, "arrive": None,
                              "depart": _clock(t), "stop": False})
                cell(st, label, _clock(t))

    fast = int(round(t - start))
    return {"id": tid, "name": train.get("name"), "stops": stops,
            "fast_minutes": fast, "real_minutes": round(fast / ratio, 1),
            "model_mph": {"max": _model_mph(export, line, max_mph, ratio),
                          "yard_limit": _model_mph(export, line, yl_mph, ratio)}}


def _table(columns: list[str], rows: dict) -> str:
    out = ["| Station | MP | " + " | ".join(columns) + " |",
           "|---|---|" + "---|" * len(columns)]
    for row in sorted(rows.values(), key=lambda r: r["station"]["mp"]):
        st = row["station"]
        name = f"{st['id']} {st['name']}" if st.get("name") else st["id"]
        out.append(f"| {name} | {st['mp']} | "
                   + " | ".join(row["cells"].get(c, "") for c in columns) + " |")
    return "\n".join(out)
