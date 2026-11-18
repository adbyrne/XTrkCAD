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

from .conventions import (CONVENTIONS_VERSION, ConventionNote, Issue,
                          check_conventions, read_convention_notes,
                          resolve_ownership, snap_note)
from .mileposts import Line, MilepostModel, _lines, _setup_line
from .models import Layout, TrackObject

SCHEMA_VERSION = 2


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


def _stations(ctx: _Context) -> list[dict]:
    """stations[]: one entry per station note, so a joint station (one id
    on several subdivisions, spec 4.3) has one entry per subdivision."""
    names = {n.note_id: n.str_field("name") for n in ctx.of_kind("station")}
    switchbacks = {n.str_field("serves") for n in ctx.of_kind("switchback")}
    out = []
    for st in ctx.model.stations:
        out.append({
            "id": st.id,
            "name": names.get(st.note_id),
            "railroad": st.railroad,
            "subdivision": st.subdivision,
            "mp": _round(st.mp),
            "mp_entry": _round(st.mp_entry),
            "mp_exit": _round(st.mp_exit),
            "terminus": st.terminus,
            "switchback": st.id in switchbacks,
            "siding_ft": None,
            "siding_cars": None,
            "main_ft": None,
            "main_cars": None,
            "house_tracks": [],
        })
    return sorted(out, key=lambda s: (s["railroad"] or "", s["subdivision"] or "",
                                      _mp_key(s["mp"]), s["id"]))


def _yard_limits(ctx: _Context) -> list[dict]:
    zones = [{
        "id": z.id, "railroad": z.railroad, "subdivision": z.subdivision,
        "mp_start": _round(z.mp_start), "mp_end": _round(z.mp_end), "source": z.source,
    } for z in ctx.model.yard_limits]
    return sorted(zones, key=lambda z: (z["railroad"] or "", z["subdivision"] or "",
                                        z["mp_start"], z["id"]))


def build_export_v2(layout: Layout, issues: list[Issue] | None = None) -> dict:
    """The layout_data.json v2 structure for a parsed layout.

    ``issues`` is the convention check's result when the caller already has
    it; otherwise the check runs here.
    """
    if issues is None:
        issues = check_conventions(layout)
    ctx = _Context(layout)
    return {
        "schema_version": SCHEMA_VERSION,
        "conventions_version": CONVENTIONS_VERSION,
        "generated": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "layout": layout.title1 or None,
        "scale": layout.scale or None,
        "layout_units": "in",
        "railroads": _railroads(ctx),
        "stations": _stations(ctx),
        "yard_limits": _yard_limits(ctx),
        "issues": [i.as_dict() for i in issues],
    }
