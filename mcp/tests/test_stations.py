"""Tests for station NOTE parsing, routing distances, and siding capacity."""

import json
import math
from pathlib import Path

import pytest

from xtrkcad_mcp.models import NoteObject
from xtrkcad_mcp.parser import parse_file
from xtrkcad_mcp.stations import (
    compute_capacities,
    compute_distances,
    extract_stations,
    model_in_to_proto_ft,
)

FIXTURES_DIR = Path(__file__).parent / "fixtures"
STATION_FIXTURE = FIXTURES_DIR / "station_layout.xtc"
EXAMPLES_DIR = Path(__file__).parent.parent.parent / "app" / "lib" / "examples"


def examples_available() -> bool:
    return EXAMPLES_DIR.is_dir()


# ---------------------------------------------------------------------------
# model_in_to_proto_ft
# ---------------------------------------------------------------------------

def test_proto_ft_ho():
    result = model_in_to_proto_ft(87.1, "HO")
    assert result == pytest.approx(87.1 * 87.1 / 12, rel=1e-4)


def test_proto_ft_n():
    result = model_in_to_proto_ft(160.0, "N")
    assert result == pytest.approx(160.0 * 160.0 / 12, rel=1e-4)


def test_proto_ft_unknown_scale_defaults_ho():
    result = model_in_to_proto_ft(87.1, "XYZ")
    assert result == pytest.approx(87.1 * 87.1 / 12, rel=1e-4)


# ---------------------------------------------------------------------------
# Parser — NOTE object extraction
# ---------------------------------------------------------------------------

def test_parser_reads_station_notes():
    layout = parse_file(STATION_FIXTURE)
    assert layout.param_version == 12
    note_ids = {n.id for n in layout.notes}
    # 10=STATION:Alpha, 11=STATION:Beta, 12=non-station, 13=STATION:Gamma
    # 20=SIDING:FreightHouse, 21=STORAGE:CarBarn
    assert {10, 11, 12, 13, 20, 21}.issubset(note_ids)


def test_parser_note_op_and_text():
    layout = parse_file(STATION_FIXTURE)
    notes = {n.id: n for n in layout.notes}
    assert notes[10].op == 0
    assert notes[10].text == "STATION: Alpha"
    assert notes[10].x == pytest.approx(0.0)
    assert notes[10].y == pytest.approx(0.0)


def test_parser_non_station_note_preserved():
    layout = parse_file(STATION_FIXTURE)
    notes = {n.id: n for n in layout.notes}
    assert notes[12].text == "not a station note"


@pytest.mark.skipif(not examples_available(), reason="example layouts not found")
def test_parser_reads_old_format_notes_cascade():
    """cascade.xtc is paramVersion 10 — notes use the multiline format."""
    layout = parse_file(EXAMPLES_DIR / "cascade.xtc")
    assert layout.param_version == 10
    assert len(layout.notes) == 2
    # Both notes should have non-empty text
    for note in layout.notes:
        assert note.op == 0
        assert len(note.text) > 5


# ---------------------------------------------------------------------------
# extract_stations
# ---------------------------------------------------------------------------

def test_extract_stations_finds_three():
    layout = parse_file(STATION_FIXTURE)
    stations = extract_stations(layout)
    names = {s.name for s in stations}
    assert names == {"Alpha", "Beta", "Gamma"}


def test_extract_stations_ignores_non_station_notes():
    layout = parse_file(STATION_FIXTURE)
    stations = extract_stations(layout)
    # Note 12 has text "not a station note" — must not appear
    assert all(s.name != "not a station note" for s in stations)


def test_extract_stations_snap_alpha():
    layout = parse_file(STATION_FIXTURE)
    stations = {s.name: s for s in extract_stations(layout)}
    alpha = stations["Alpha"]
    # Alpha NOTE is at (0,0); track 1 ep1 is at (0,0)
    assert alpha.nearest_ep[0] == 1   # track id
    assert alpha.snap_dist == pytest.approx(0.0)


def test_extract_stations_snap_beta():
    layout = parse_file(STATION_FIXTURE)
    stations = {s.name: s for s in extract_stations(layout)}
    beta = stations["Beta"]
    # Beta NOTE is at (80,0); track 4 ep1 is at (80,0)
    assert beta.nearest_ep[0] == 4
    assert beta.snap_dist == pytest.approx(0.0)


def test_extract_stations_snap_gamma():
    layout = parse_file(STATION_FIXTURE)
    stations = {s.name: s for s in extract_stations(layout)}
    gamma = stations["Gamma"]
    # Gamma NOTE is at (115,0); track 5 ep1 is at (115,0)
    assert gamma.nearest_ep[0] == 5
    assert gamma.snap_dist == pytest.approx(0.0)


def test_extract_stations_empty_when_no_notes():
    layout = parse_file(STATION_FIXTURE)
    layout.notes = []
    assert extract_stations(layout) == []


# ---------------------------------------------------------------------------
# compute_distances — full track graph
# ---------------------------------------------------------------------------

def test_alpha_beta_distance():
    layout = parse_file(STATION_FIXTURE)
    results = compute_distances(layout)
    pair = next((r for r in results if {r.from_station, r.to_station} == {"Alpha", "Beta"}), None)
    assert pair is not None
    assert pair.reachable
    # 4 tracks × 20in each = 80in
    assert pair.distance_model_in == pytest.approx(80.0)


def test_alpha_beta_proto_ft():
    layout = parse_file(STATION_FIXTURE)
    results = compute_distances(layout)
    pair = next(r for r in results if {r.from_station, r.to_station} == {"Alpha", "Beta"})
    expected = 80.0 * 87.1 / 12.0
    assert pair.distance_proto_ft == pytest.approx(expected, rel=1e-3)


def test_gamma_unreachable_from_alpha():
    layout = parse_file(STATION_FIXTURE)
    results = compute_distances(layout)
    pair = next(r for r in results if {r.from_station, r.to_station} == {"Alpha", "Gamma"})
    assert not pair.reachable


def test_gamma_unreachable_from_beta():
    layout = parse_file(STATION_FIXTURE)
    results = compute_distances(layout)
    pair = next(r for r in results if {r.from_station, r.to_station} == {"Beta", "Gamma"})
    assert not pair.reachable


def test_result_count():
    layout = parse_file(STATION_FIXTURE)
    results = compute_distances(layout)
    # 3 stations → C(3,2) = 3 pairs
    assert len(results) == 3


# ---------------------------------------------------------------------------
# compute_distances — layer filtering
# ---------------------------------------------------------------------------

def test_mainline_only_excludes_track3():
    """Track 3 is on layer 1 (siding). Mainline-only routing cannot connect Alpha to Beta."""
    layout = parse_file(STATION_FIXTURE)
    results = compute_distances(
        layout,
        layer_categories={"0": "mainline", "1": "siding"},
        route_layer_names=["mainline"],
    )
    pair = next(r for r in results if {r.from_station, r.to_station} == {"Alpha", "Beta"})
    assert not pair.reachable


def test_mainline_and_siding_layers_reaches_beta():
    """Including siding layer restores the path Alpha→Beta."""
    layout = parse_file(STATION_FIXTURE)
    results = compute_distances(
        layout,
        layer_categories={"0": "mainline", "1": "siding"},
        route_layer_names=["mainline", "siding"],
    )
    pair = next(r for r in results if {r.from_station, r.to_station} == {"Alpha", "Beta"})
    assert pair.reachable
    assert pair.distance_model_in == pytest.approx(80.0)


# ---------------------------------------------------------------------------
# MCP tool round-trip — write_station_distance_report
# ---------------------------------------------------------------------------

def test_write_md_report(tmp_path):
    from xtrkcad_mcp.stations import compute_distances, extract_stations

    layout = parse_file(STATION_FIXTURE)
    stations = extract_stations(layout)
    results = compute_distances(layout)
    out = tmp_path / "distances.md"

    # Call the write logic via the server tool
    import os
    os.environ["XTRKCAD_PLANS_DIR"] = str(FIXTURES_DIR)
    from xtrkcad_mcp.server import write_station_distance_report
    msg = write_station_distance_report(str(STATION_FIXTURE), str(out))
    assert out.exists()
    text = out.read_text()
    assert "Alpha" in text
    assert "Beta" in text
    assert "Station Distance Report" in text


def test_write_json_report(tmp_path):
    import os
    os.environ["XTRKCAD_PLANS_DIR"] = str(FIXTURES_DIR)
    from xtrkcad_mcp.server import write_station_distance_report
    out = tmp_path / "distances.json"
    write_station_distance_report(str(STATION_FIXTURE), str(out), format="json")
    assert out.exists()
    payload = json.loads(out.read_text())
    assert "distances" in payload
    assert "stations" in payload
    assert "Alpha" in payload["stations"]
    assert any(
        d["from"] in {"Alpha", "Beta"} and d["reachable"]
        for d in payload["distances"]
    )


def test_get_stations_tool():
    import os
    os.environ["XTRKCAD_PLANS_DIR"] = str(FIXTURES_DIR)
    from xtrkcad_mcp.server import get_stations
    result = get_stations(str(STATION_FIXTURE))
    names = {r["name"] for r in result}
    assert names == {"Alpha", "Beta", "Gamma"}


def test_get_station_distances_tool():
    import os
    os.environ["XTRKCAD_PLANS_DIR"] = str(FIXTURES_DIR)
    from xtrkcad_mcp.server import get_station_distances
    result = get_station_distances(str(STATION_FIXTURE))
    pair = next(r for r in result if {r["from"], r["to"]} == {"Alpha", "Beta"})
    assert pair["reachable"]
    assert pair["distance_model_in"] == pytest.approx(80.0, abs=0.01)


# ---------------------------------------------------------------------------
# compute_capacities — siding / storage
# ---------------------------------------------------------------------------

def test_capacity_finds_siding_and_storage():
    layout = parse_file(STATION_FIXTURE)
    results = compute_capacities(layout)
    names = {r.name for r in results}
    assert "Freight House" in names
    assert "Car Barn" in names


def test_capacity_kinds():
    layout = parse_file(STATION_FIXTURE)
    results = {r.name: r for r in compute_capacities(layout)}
    assert results["Freight House"].kind == "siding"
    assert results["Car Barn"].kind == "storage"


def test_freight_house_excludes_turnout():
    """Tracks 6+7 are 15in each = 30in. Track 8 is TURNOUT — excluded."""
    layout = parse_file(STATION_FIXTURE)
    results = {r.name: r for r in compute_capacities(layout)}
    fh = results["Freight House"]
    assert fh.length_model_in == pytest.approx(30.0)


def test_car_barn_length():
    """Tracks 9+10 are 10in + 15in = 25in total (no TURNOUT boundary)."""
    layout = parse_file(STATION_FIXTURE)
    results = {r.name: r for r in compute_capacities(layout)}
    cb = results["Car Barn"]
    assert cb.length_model_in == pytest.approx(25.0)


def test_max_cars_uses_model_feet_not_proto_feet():
    """HO: 2 cars per model foot. 30 model in = 2.5 model ft → 5 cars."""
    layout = parse_file(STATION_FIXTURE)
    results = {r.name: r for r in compute_capacities(layout)}
    fh = results["Freight House"]
    # 30 model in / 12 = 2.5 model ft × 2.0 cars/model-ft = 5
    assert fh.max_cars == 5


def test_station_notes_not_in_capacities():
    """STATION: notes must not appear in capacity results."""
    layout = parse_file(STATION_FIXTURE)
    results = compute_capacities(layout)
    names = {r.name for r in results}
    assert "Alpha" not in names
    assert "Beta" not in names


def test_get_siding_capacities_tool():
    import os
    os.environ["XTRKCAD_PLANS_DIR"] = str(FIXTURES_DIR)
    from xtrkcad_mcp.server import get_siding_capacities
    result = get_siding_capacities(str(STATION_FIXTURE))
    names = {r["name"] for r in result}
    assert "Freight House" in names
    assert "Car Barn" in names
    fh = next(r for r in result if r["name"] == "Freight House")
    assert fh["max_cars"] == 5
    assert fh["length_model_in"] == pytest.approx(30.0, abs=0.01)
