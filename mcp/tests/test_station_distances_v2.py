"""Station distance report from the milepost model (conventions v1).

A layout with convention notes (a railroad and stations with MPs) is
reported per railroad and subdivision: stations in MP order, and each pair's
distance as the MP difference times that subdivision's own mp_scale. A joint
station (one id on several subdivisions, spec 4.3) appears once on each.
Layouts with only STATION: text notes keep the track-graph report.

conventions_clean.xtc: railroad NYE, subdivision NLS at 12 in per MP; WP at
MP 0, team track MILLER_TEAM at MP 8.333, XP at MP 15.833.
"""

import json
from pathlib import Path

import pytest

from xtrkcad_mcp.server import write_station_distance_report
from xtrkcad_mcp.station_distances import distances_from_export, station_distances_v2
from xtrkcad_mcp.parser import parse_file
from xtrkcad_mcp.stations import model_in_to_proto_ft

FIXTURES_DIR = Path(__file__).parent / "fixtures"
CLEAN_FIXTURE = FIXTURES_DIR / "conventions_clean.xtc"
STATION_FIXTURE = FIXTURES_DIR / "station_layout.xtc"


@pytest.fixture(scope="module")
def clean():
    return station_distances_v2(parse_file(CLEAN_FIXTURE))


def _pair(sub, a, b):
    return next(p for p in sub["pairs"] if (p["from"], p["to"]) == (a, b))


def test_one_table_per_subdivision(clean):
    assert [(s["railroad"], s["subdivision"]) for s in clean["subdivisions"]] == [
        ("NYE", "NLS")]
    sub = clean["subdivisions"][0]
    assert sub["railroad_name"] == "New York & Eastern"
    assert sub["subdivision_name"] == "Northern Lights Subdivision"
    assert sub["mp_scale_in"] == 12.0


def test_stations_in_mp_order(clean):
    sub = clean["subdivisions"][0]
    assert [(s["id"], s["mp"]) for s in sub["stations"]] == [
        ("WP", 0.0), ("MILLER_TEAM", 8.333), ("XP", 15.833)]


def test_pair_distance_from_mps(clean):
    sub = clean["subdivisions"][0]
    pair = _pair(sub, "WP", "XP")
    assert pair["mp_distance"] == 15.833
    assert pair["distance_model_in"] == pytest.approx(190.0, abs=0.01)
    assert pair["distance_proto_ft"] == pytest.approx(
        round(model_in_to_proto_ft(190.0, "HO"), 1), abs=0.2)
    assert len(sub["pairs"]) == 3          # every pair once


def _export(stations, subdivisions):
    """A minimal v2 export: subdivisions as (railroad, sub id, mp_scale, mp_unit)."""
    railroads = {}
    for rr, sub, scale, unit in subdivisions:
        railroads.setdefault(rr, {"id": rr, "name": rr, "subdivisions": []})
        railroads[rr]["subdivisions"].append(
            {"id": sub, "name": None, "mp_scale": scale, "mp_unit": unit, "anchors": []})
    return {"layout": "t", "scale": "HO", "railroads": list(railroads.values()),
            "stations": [{"id": i, "kind": "station", "name": n, "railroad": rr,
                          "subdivision": sub, "mp": mp}
                         for i, n, rr, sub, mp in stations]}


def test_joint_station_once_per_subdivision():
    export = _export(
        [("WP", "Williamsport", "NYE", None, 0.0), ("XP", "Xina Pass", "NYE", None, 46.0),
         ("WP", None, "CO", None, 235.4), ("CK", "Clark", "CO", None, 240.4)],
        [("NYE", None, 12, "in"), ("CO", None, 12, "in")])
    report = distances_from_export(export)
    by_rr = {s["railroad"]: s for s in report["subdivisions"]}
    assert [s["id"] for s in by_rr["NYE"]["stations"]] == ["WP", "XP"]
    assert [s["id"] for s in by_rr["CO"]["stations"]] == ["WP", "CK"]
    # The C&O note has no name: the joint station's name comes from its other note.
    assert by_rr["CO"]["stations"][0]["name"] == "Williamsport"
    assert report["joint_stations"] == [{"id": "WP", "on": [
        {"railroad": "CO", "subdivision": None, "mp": 235.4},
        {"railroad": "NYE", "subdivision": None, "mp": 0.0}]}]
    assert _pair(by_rr["CO"], "WP", "CK")["distance_model_in"] == pytest.approx(60.0)


@pytest.mark.parametrize("unit, scale, inches", [("ft", 1, 12.0), ("cm", 2.54, 1.0),
                                                 ("m", 0.0254, 1.0)])
def test_mp_unit(unit, scale, inches):
    export = _export([("A", "A", "R", None, 0.0), ("B", "B", "R", None, 10.0)],
                     [("R", None, scale, unit)])
    sub = distances_from_export(export)["subdivisions"][0]
    assert sub["mp_scale_in"] == pytest.approx(inches)
    assert _pair(sub, "A", "B")["distance_model_in"] == pytest.approx(10 * inches)


def test_station_without_mp_is_listed_not_paired():
    export = _export([("A", "A", "R", None, 0.0), ("B", "B", "R", None, 5.0),
                      ("Z", "Z", "R", None, None)], [("R", None, 12, "in")])
    sub = distances_from_export(export)["subdivisions"][0]
    assert sub["no_mp"] == ["Z"]
    assert [(p["from"], p["to"]) for p in sub["pairs"]] == [("A", "B")]


def test_layout_without_railroad_notes_is_not_v2():
    assert station_distances_v2(parse_file(STATION_FIXTURE)) is None


# -- the report ---------------------------------------------------------------------

def test_md_report_v2(tmp_path):
    out = tmp_path / "d.md"
    msg = write_station_distance_report(str(CLEAN_FIXTURE), str(out))
    text = out.read_text(encoding="utf-8")
    assert "## New York & Eastern — Northern Lights Subdivision" in text
    assert "MP scale: 12 in per MP" in text
    assert "| WP" in text and "15.833" in text
    assert "3 station(s), 3 pair(s)" in msg


def test_json_report_v2(tmp_path):
    out = tmp_path / "d.json"
    write_station_distance_report(str(CLEAN_FIXTURE), str(out), format="json")
    payload = json.loads(out.read_text(encoding="utf-8"))
    assert payload["source"] == "mileposts"
    sub = payload["subdivisions"][0]
    assert _pair(sub, "WP", "XP")["mp_distance"] == 15.833


def test_route_options_noted_as_not_used(tmp_path):
    out = tmp_path / "d.md"
    write_station_distance_report(str(CLEAN_FIXTURE), str(out), exclude_turnouts=True)
    assert "don't apply" in out.read_text(encoding="utf-8")


def test_text_note_layout_keeps_track_graph_report(tmp_path):
    out = tmp_path / "d.md"
    write_station_distance_report(str(STATION_FIXTURE), str(out))
    text = out.read_text(encoding="utf-8")
    assert "| From" in text and "Reachable" in text
    assert "## " not in text
