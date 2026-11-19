"""Capacity between clearance points, for any layout.

A car standing near a turnout fouls its other leg until the two tracks are
the clearance apart (13 ft between centres, scaled). Capacity is a track's
length less the fouled part at each turnout it leaves: what it holds. The
check and the v2 export use it; generate_track_plan builds to it, so on the
generated Hillside layout the two agree.
"""

from pathlib import Path

import pytest

from xtrkcad_mcp.clearance import clear_usable_length, default_clearance_in, fouled_length
from xtrkcad_mcp.parser import parse_file
from xtrkcad_mcp.server import generate_layout, generate_track_plan

FIXTURES_DIR = Path(__file__).parent / "fixtures"
HILLSIDE = FIXTURES_DIR / "hillside_division.yaml"


@pytest.fixture(scope="module")
def hillside(tmp_path_factory):
    out = tmp_path_factory.mktemp("clearance") / "hillside.xtc"
    generate_layout(str(HILLSIDE), str(out))
    result = generate_track_plan(str(HILLSIDE), str(out))
    return parse_file(out), {f["id"]: f for f in result["features"]}, out


def _track_at(layout, x, y, layer):
    from xtrkcad_mcp.conventions import track_distance
    best = min((t for t in layout.tracks if t.layer == layer and t.kind != "DRAW"),
               key=lambda t: track_distance(t, x, y))
    return best


def test_default_clearance_is_13_ft_scaled():
    assert default_clearance_in("HO") == pytest.approx(13 * 12 / 87.1)
    assert default_clearance_in("N") == pytest.approx(13 * 12 / 160.0)


def test_spur_capacity_matches_the_plan(hillside):
    """Alder's feed mill spur was built to hold 18 in (3 cars)."""
    layout, features, _path = hillside
    spur = _track_at(layout, 10.0, 115.0, 3)            # L1-Storage
    assert clear_usable_length(layout, spur.id) == pytest.approx(
        features["AL_FEED"]["capacity_in"], abs=0.3)


def test_yard_track_capacities_match_the_plan(hillside):
    layout, features, _path = hillside
    plan = [t["capacity_in"] for t in features["HY"]["tracks"]]
    got = [clear_usable_length(layout, _track_at(layout, x, 20.0, 3).id)
           for x in (12.0, 14.0, 16.0)]
    assert got == pytest.approx(plan, abs=0.3)


def test_track_beyond_the_points_is_not_fouled(hillside):
    """The main beyond a turnout's points is shared by both routes: nothing
    to clear there."""
    layout, _features, _path = hillside
    main = _track_at(layout, 12.0, 74.0, 1)         # just north of the yard throat
    throat = next(t for t in layout.tracks if t.kind == "TURNOUT"
                  and any(ep.connected_to == main.id for ep in t.endpoints))
    assert fouled_length(layout, throat, main, default_clearance_in("HO")) == 0.0


def test_fouled_length_grows_with_the_clearance(hillside):
    layout, _features, _path = hillside
    spur = _track_at(layout, 10.0, 115.0, 3)
    tight = clear_usable_length(layout, spur.id, clearance=1.2)
    wide = clear_usable_length(layout, spur.id, clearance=2.0)
    assert tight > wide


def test_an_unconnected_track_is_all_usable(tmp_path):
    p = tmp_path / "t.xtc"
    p.write_text("VERSION 13 5.4.0\nSCALE HO\nROOMSIZE 40 x 20\n"
                 'LAYERS 0 1 0 0 0 0 0 0 0 "L1-Storage"\n'
                 "STRAIGHT 1 0 0 0 0 HO 0\n\tE 0.0 0.0 270.0\n\tE 30.0 0.0 90.0\n\tEND$SEGS\n")
    layout = parse_file(p)
    assert clear_usable_length(layout, 1) == pytest.approx(30.0)


# -- the export and the check use it -------------------------------------------------

def test_export_siding_and_spurs_match_the_plan(hillside, tmp_path):
    """The export's figures for the generated plan are what it was built to
    hold. The export needs a few convention notes: MP zero, two stations and
    their industries (placed as the tutorial places them)."""
    import json

    from xtrkcad_mcp.export_v2 import build_export_v2
    from xtrkcad_mcp.stations import model_in_to_proto_ft
    layout, features, path = hillside
    notes = [
        {"layer": "L1-Main", "at": [12.0, 70.66],
         "note": {"kind": "reference", "id": "MP_ZERO", "mp": 0, "mp_scale": 12}},
        {"layer": "L1-Main", "at": [12.0, 109.5], "note": {"kind": "station", "id": "AL"}},
        {"layer": "L1-Main", "at": [72.0, 180.0], "note": {"kind": "station", "id": "BI"}},
        {"layer": "L1-Storage", "at": [10.0, 115.0],
         "note": {"kind": "industry", "id": "AL_FEED", "station": "AL", "car_spots": 2}},
        {"layer": "L1-Storage", "at": [52.0, 182.0],
         "note": {"kind": "industry", "id": "BI_LUMBER", "station": "BI", "car_spots": 3}},
    ]
    layers = {info.name: idx for idx, info in layout.layers.items()}
    lines = [f'NOTE {900 + i} {layers[n["layer"]]} 0 0 {n["at"][0]:.6f} {n["at"][1]:.6f} 0 3 '
             f'"{json.dumps(n["note"]).replace(chr(34), chr(34) * 2)}"'
             for i, n in enumerate(notes)]
    p = tmp_path / "annotated.xtc"
    p.write_text(path.read_text(encoding="utf-8") + "\n".join(lines) + "\n", encoding="utf-8")
    export = build_export_v2(parse_file(p))
    al = next(s for s in export["stations"] if s["id"] == "AL")
    assert al["siding_cars"] == features["AL"]["capacity_cars"] == 5
    assert al["siding_ft"] == pytest.approx(model_in_to_proto_ft(30.0, "HO"), rel=0.01)
    spurs = {i["id"]: i["spur_capacity_cars"] for i in export["industries"]}
    assert spurs["AL_FEED"] == features["AL_FEED"]["capacity_cars"]
    assert spurs["BI_LUMBER"] == features["BI_LUMBER"]["capacity_cars"]
