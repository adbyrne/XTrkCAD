"""Tests for station NOTE parsing, routing distances, and siding capacity."""

import json
import math
from pathlib import Path

import pytest

from xtrkcad_mcp.models import NoteObject
from xtrkcad_mcp.parser import parse_file
from xtrkcad_mcp.stations import (
    build_layout_export,
    compute_capacities,
    compute_distances,
    compute_yard_tracks,
    extract_reference_points,
    extract_stations,
    list_annotated_segments,
    load_stations_config,
    model_in_to_proto_ft,
    compute_mileposts,
    validate_layout_annotations,
)

FIXTURES_DIR = Path(__file__).parent / "fixtures"
STATION_FIXTURE = FIXTURES_DIR / "station_layout.xtc"
EXPORT_FIXTURE = FIXTURES_DIR / "export_test_layout.xtc"
EXPORT_STATIONS = FIXTURES_DIR / "export_test_stations.yaml"
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
    station_names = {s["name"] for s in payload["stations"]}
    assert "Alpha" in station_names
    assert any(
        d["from"] in {"Alpha", "Beta"} and d["reachable"]
        for d in payload["distances"]
    )
    # JSON distances include MP columns
    assert "from_mp_ft" in payload["distances"][0]
    assert "to_mp_ft" in payload["distances"][0]


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
    assert "FreightHouse" in names
    assert "CarBarn" in names


def test_capacity_kinds():
    layout = parse_file(STATION_FIXTURE)
    results = {r.name: r for r in compute_capacities(layout)}
    assert results["FreightHouse"].kind == "siding"
    assert results["CarBarn"].kind == "storage"


def test_freight_house_excludes_turnout():
    """Tracks 6+7 are 15in each = 30in. Track 8 is TURNOUT — excluded."""
    layout = parse_file(STATION_FIXTURE)
    results = {r.name: r for r in compute_capacities(layout)}
    fh = results["FreightHouse"]
    assert fh.length_model_in == pytest.approx(30.0)


def test_freight_house_description():
    """FreightHouse note has description text after the ID token."""
    layout = parse_file(STATION_FIXTURE)
    results = {r.name: r for r in compute_capacities(layout)}
    assert results["FreightHouse"].description == "Freight house siding"


def test_car_barn_length():
    """Tracks 9+10 are 10in + 15in = 25in total (no TURNOUT boundary)."""
    layout = parse_file(STATION_FIXTURE)
    results = {r.name: r for r in compute_capacities(layout)}
    cb = results["CarBarn"]
    assert cb.length_model_in == pytest.approx(25.0)


def test_max_cars_uses_model_feet_not_proto_feet():
    """HO: 2 cars per model foot. 30 model in = 2.5 model ft → 5 cars."""
    layout = parse_file(STATION_FIXTURE)
    results = {r.name: r for r in compute_capacities(layout)}
    fh = results["FreightHouse"]
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
    assert "FreightHouse" in names
    assert "CarBarn" in names
    fh = next(r for r in result if r["name"] == "FreightHouse")
    assert fh["max_cars"] == 5
    assert fh["length_model_in"] == pytest.approx(30.0, abs=0.01)


# ---------------------------------------------------------------------------
# INDUSTRY: note — compute_capacities includes industry spurs
# ---------------------------------------------------------------------------

def test_industry_note_in_capacities():
    """INDUSTRY: notes should be picked up by compute_capacities."""
    layout = parse_file(EXPORT_FIXTURE)
    results = compute_capacities(layout)
    names = {r.name: r for r in results}
    assert "KIEL" in names
    assert names["KIEL"].kind == "industry"


def test_industry_length():
    """KIEL spur: tracks 9+10 = 15in + 15in = 30in total."""
    layout = parse_file(EXPORT_FIXTURE)
    results = {r.name: r for r in compute_capacities(layout)}
    assert results["KIEL"].length_model_in == pytest.approx(30.0, abs=0.1)


# ---------------------------------------------------------------------------
# Multi-industry spur — shared spur slot-based capacity
# ---------------------------------------------------------------------------

def _make_shared_spur_layout():
    """Three STRAIGHT tracks forming a 60-inch spur; TURNOUT at the throat end.

    track 1:  E(0,0)  → T(20,0)   dead-end (buffer stop) at x=0
    track 2: T(20,0)  → T(40,0)
    track 3: T(40,0)  → T(60,0)   connects to TURNOUT (track 4)
    track 4: TURNOUT  T(60,0) → E(80,0)  — throat, excluded from spur BFS

    COAL_A at (5, 0)  — 5 model inches from dead-end
    COAL_B at (35, 0) — 35 model inches from dead-end
    """
    from xtrkcad_mcp.models import Endpoint, Layout, NoteObject, TrackObject
    t1 = TrackObject(id=1, kind="STRAIGHT", layer=0, endpoints=[
        Endpoint(x=0,  y=0, angle=0,   connected_to=None),
        Endpoint(x=20, y=0, angle=180, connected_to=2),
    ])
    t2 = TrackObject(id=2, kind="STRAIGHT", layer=0, endpoints=[
        Endpoint(x=20, y=0, angle=0,   connected_to=1),
        Endpoint(x=40, y=0, angle=180, connected_to=3),
    ])
    t3 = TrackObject(id=3, kind="STRAIGHT", layer=0, endpoints=[
        Endpoint(x=40, y=0, angle=0,   connected_to=2),
        Endpoint(x=60, y=0, angle=180, connected_to=4),
    ])
    t4 = TrackObject(id=4, kind="TURNOUT", layer=0, endpoints=[
        Endpoint(x=60, y=0, angle=0,   connected_to=3),
        Endpoint(x=80, y=0, angle=180, connected_to=None),
    ])
    note_a = NoteObject(id=1, layer=0, x=5,  y=0, op=0, text="INDUSTRY: COAL_A")
    note_b = NoteObject(id=2, layer=0, x=35, y=0, op=0, text="INDUSTRY: COAL_B")
    return Layout(scale="HO", tracks=[t1, t2, t3, t4], notes=[note_a, note_b])


def test_multi_industry_both_present():
    """Both industries on a shared spur appear in compute_capacities output."""
    caps = {r.name: r for r in compute_capacities(_make_shared_spur_layout())}
    assert "COAL_A" in caps
    assert "COAL_B" in caps


def test_multi_industry_slot_width():
    """Multi-industry spur: each industry gets one threshold-slot (12 model inches)."""
    caps = {r.name: r for r in compute_capacities(_make_shared_spur_layout())}
    assert caps["COAL_A"].length_model_in == pytest.approx(12.0, abs=0.1)
    assert caps["COAL_B"].length_model_in == pytest.approx(12.0, abs=0.1)


def test_multi_industry_car_count():
    """HO scale, 12-inch slot → 1 model foot → 2 cars (Fugate)."""
    caps = {r.name: r for r in compute_capacities(_make_shared_spur_layout())}
    assert caps["COAL_A"].max_cars == 2
    assert caps["COAL_B"].max_cars == 2


def test_multi_industry_kind():
    """Kind remains 'industry' for all multi-spur entries."""
    for r in compute_capacities(_make_shared_spur_layout()):
        assert r.kind == "industry"


def test_multi_industry_ordering():
    """COAL_A (5 in from dead-end) is returned before COAL_B (35 in from dead-end)."""
    industry = [r for r in compute_capacities(_make_shared_spur_layout())
                if r.kind == "industry"]
    names = [r.name for r in industry]
    assert names.index("COAL_A") < names.index("COAL_B")


def test_single_industry_unchanged_by_grouping():
    """A spur with one industry still reports the full BFS length, not a slot."""
    layout = parse_file(EXPORT_FIXTURE)
    results = {r.name: r for r in compute_capacities(layout)}
    # KIEL spur is 30 model inches; single industry so full length is returned.
    assert results["KIEL"].length_model_in == pytest.approx(30.0, abs=0.1)


# ---------------------------------------------------------------------------
# load_stations_config
# ---------------------------------------------------------------------------

def test_load_stations_config_fields():
    config = load_stations_config(EXPORT_STATIONS)
    assert config.layout == "export_test"
    assert config.mp_scale == pytest.approx(1.0)
    assert len(config.stations) == 3


def test_load_stations_config_ids():
    config = load_stations_config(EXPORT_STATIONS)
    ids = [s.id for s in config.stations]
    assert "WP" in ids
    assert "KIEL" in ids
    assert "XP" in ids


def test_load_stations_config_types():
    config = load_stations_config(EXPORT_STATIONS)
    by_id = {s.id: s for s in config.stations}
    assert "station" in by_id["WP"].types
    assert "industry" in by_id["KIEL"].types
    assert by_id["KIEL"].switchback is False


def test_load_stations_config_sequence():
    config = load_stations_config(EXPORT_STATIONS)
    seqs = {s.id: s.sequence for s in config.stations}
    assert seqs["WP"] == 0
    assert seqs["KIEL"] == 1
    assert seqs["XP"] == 2


# ---------------------------------------------------------------------------
# extract_reference_points
# ---------------------------------------------------------------------------

def test_extract_reference_points_finds_mp_zero():
    layout = parse_file(EXPORT_FIXTURE)
    refs = extract_reference_points(layout)
    names = {r.name for r in refs}
    assert "MP_ZERO" in names


def test_extract_reference_mp_zero_snaps_to_origin():
    """REFERENCE: MP_ZERO note is at (0,0) — should snap to track 1 ep at (0,0)."""
    layout = parse_file(EXPORT_FIXTURE)
    refs = {r.name: r for r in extract_reference_points(layout)}
    mp_zero = refs["MP_ZERO"]
    assert mp_zero.snap_dist == pytest.approx(0.0, abs=0.01)
    assert mp_zero.nearest_ep[0] == 1   # track 1


def test_extract_reference_no_tracks():
    layout = parse_file(EXPORT_FIXTURE)
    layout.tracks = []
    assert extract_reference_points(layout) == []


# ---------------------------------------------------------------------------
# compute_mileposts
# ---------------------------------------------------------------------------

def test_compute_mileposts_wp_is_zero():
    """WP STATION: note at (0,2) snaps to (0,0) — same endpoint as MP_ZERO — MP=0."""
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    refs = {r.name: r for r in extract_reference_points(layout)}
    results = {r.station_id: r for r in compute_mileposts(layout, config, refs["MP_ZERO"])}
    assert "WP" in results
    assert results["WP"].reachable
    assert results["WP"].milepost == pytest.approx(0.0, abs=0.1)


def test_compute_mileposts_xp_distance():
    """XP is 120 model inches from WP. Proto ft = 120 * 87.1 / 12."""
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    refs = {r.name: r for r in extract_reference_points(layout)}
    results = {r.station_id: r for r in compute_mileposts(layout, config, refs["MP_ZERO"])}
    expected_mp = 120.0 * 87.1 / 12.0   # mp_scale=1.0, so MP == proto_ft
    assert results["XP"].milepost == pytest.approx(expected_mp, rel=1e-3)


def test_compute_mileposts_unreachable():
    """Siding tracks (7,8) are not connected to mainline — no STATION: note on them,
    but WP and XP are on the mainline and should be reachable from each other."""
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    refs = {r.name: r for r in extract_reference_points(layout)}
    results = compute_mileposts(layout, config, refs["MP_ZERO"])
    # Both WP and XP are on the connected mainline chain
    by_id = {r.station_id: r for r in results}
    assert by_id["WP"].reachable
    assert by_id["XP"].reachable


# ---------------------------------------------------------------------------
# list_annotated_segments
# ---------------------------------------------------------------------------

def test_list_annotated_segments_count():
    """Fixture has 6 annotations: REFERENCE:MP_ZERO, STATION:WP, STATION:XP,
    SIDING:XP, INDUSTRY:KIEL, YARD_TRACK:WP LEAD."""
    layout = parse_file(EXPORT_FIXTURE)
    segs = list_annotated_segments(layout)
    assert len(segs) == 6


def test_list_annotated_segments_types():
    layout = parse_file(EXPORT_FIXTURE)
    segs = list_annotated_segments(layout)
    types = {s.annotation_type for s in segs}
    assert "station" in types
    assert "siding" in types
    assert "industry" in types
    assert "reference" in types
    assert "yard_track" in types


def test_list_annotated_segments_ids():
    layout = parse_file(EXPORT_FIXTURE)
    segs = {s.annotation_id: s for s in list_annotated_segments(layout)}
    assert "WP" in segs
    assert "XP" in segs
    assert "KIEL" in segs
    assert "MP_ZERO" in segs
    assert "WP LEAD" in segs   # YARD_TRACK combined id


def test_list_annotated_segments_source_is_note():
    layout = parse_file(EXPORT_FIXTURE)
    segs = list_annotated_segments(layout)
    assert all(s.annotation_source == "note" for s in segs)


def test_list_annotated_segments_layer_names():
    layout = parse_file(EXPORT_FIXTURE)
    segs = {s.annotation_id: s for s in list_annotated_segments(layout)}
    assert segs["WP"].nearest_track_layer_name == "L1-Main"
    assert segs["XP"].nearest_track_layer_name == "L1-Passing"


# ---------------------------------------------------------------------------
# validate_layout_annotations
# ---------------------------------------------------------------------------

def test_validate_clean_layout_no_errors():
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    issues = validate_layout_annotations(layout, config)
    errors = [i for i in issues if i.severity == "error"]
    assert errors == [], f"Unexpected errors: {errors}"


def test_validate_missing_reference_is_error():
    layout = parse_file(EXPORT_FIXTURE)
    layout.notes = [n for n in layout.notes if "REFERENCE" not in n.text]
    config = load_stations_config(EXPORT_STATIONS)
    issues = validate_layout_annotations(layout, config)
    codes = {i.code for i in issues if i.severity == "error"}
    assert "MISSING_REFERENCE" in codes


def test_validate_missing_station_note_is_warning():
    layout = parse_file(EXPORT_FIXTURE)
    layout.notes = [n for n in layout.notes if "STATION: XP" not in n.text]
    config = load_stations_config(EXPORT_STATIONS)
    issues = validate_layout_annotations(layout, config)
    codes = {i.code for i in issues}
    assert "MISSING_STATION_NOTE" in codes


def test_validate_missing_industry_note_is_error():
    layout = parse_file(EXPORT_FIXTURE)
    layout.notes = [n for n in layout.notes if "INDUSTRY: KIEL" not in n.text]
    config = load_stations_config(EXPORT_STATIONS)
    issues = validate_layout_annotations(layout, config)
    codes = {i.code for i in issues if i.severity == "error"}
    assert "MISSING_INDUSTRY_NOTE" in codes


# ---------------------------------------------------------------------------
# build_layout_export
# ---------------------------------------------------------------------------

def test_build_layout_export_structure():
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    data = build_layout_export(layout, config)
    assert "stations" in data
    assert "industries" in data
    assert "segments" in data
    assert "generated" in data
    assert "warnings" in data


def test_build_layout_export_station_ids():
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    data = build_layout_export(layout, config)
    ids = {s["station_id"] for s in data["stations"]}
    assert "WP" in ids
    assert "XP" in ids


def test_build_layout_export_industry_ids():
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    data = build_layout_export(layout, config)
    ids = {i["industry_id"] for i in data["industries"]}
    assert "KIEL" in ids


def test_build_layout_export_wp_milepost_is_zero():
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    data = build_layout_export(layout, config)
    wp = next(s for s in data["stations"] if s["station_id"] == "WP")
    assert wp["milepost_entry"] == pytest.approx(0.0, abs=0.1)


def test_build_layout_export_xp_milepost():
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    data = build_layout_export(layout, config)
    xp = next(s for s in data["stations"] if s["station_id"] == "XP")
    expected = 120.0 * 87.1 / 12.0
    assert xp["milepost_entry"] == pytest.approx(expected, rel=1e-2)


def test_build_layout_export_xp_siding_length():
    """XP siding: tracks 7+8 = 20in + 20in = 40in. Proto ft = 40*87.1/12."""
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    data = build_layout_export(layout, config)
    xp = next(s for s in data["stations"] if s["station_id"] == "XP")
    expected_ft = 40.0 * 87.1 / 12.0
    assert xp["siding_length_ft"] == pytest.approx(expected_ft, rel=1e-2)


def test_build_layout_export_segments():
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    data = build_layout_export(layout, config)
    # Station sequence: WP(0) → XP(2); KIEL is an industry, not in segments
    froms = {s["from_station"] for s in data["segments"]}
    assert "WP" in froms


def test_build_layout_export_no_warnings_clean_layout():
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    data = build_layout_export(layout, config)
    assert data["warnings"] == []


# ---------------------------------------------------------------------------
# MCP tool round-trips — list_labeled_segments, validate_layout, export_layout_data
# ---------------------------------------------------------------------------

def test_list_labeled_segments_tool():
    import os
    os.environ["XTRKCAD_PLANS_DIR"] = str(FIXTURES_DIR)
    from xtrkcad_mcp.server import list_labeled_segments
    result = list_labeled_segments(str(EXPORT_FIXTURE))
    ids = {r["annotation_id"] for r in result}
    assert "WP" in ids
    assert "XP" in ids
    assert "KIEL" in ids
    assert "MP_ZERO" in ids
    assert "WP LEAD" in ids


def test_validate_layout_tool_clean():
    import os
    os.environ["XTRKCAD_PLANS_DIR"] = str(FIXTURES_DIR)
    from xtrkcad_mcp.server import validate_layout
    result = validate_layout(str(EXPORT_FIXTURE), str(EXPORT_STATIONS))
    assert result["error_count"] == 0


def test_export_layout_data_tool(tmp_path):
    import os
    os.environ["XTRKCAD_PLANS_DIR"] = str(FIXTURES_DIR)
    from xtrkcad_mcp.server import export_layout_data
    out = tmp_path / "layout_data.json"
    result = export_layout_data(str(EXPORT_FIXTURE), str(EXPORT_STATIONS), str(out))
    assert out.exists()
    assert result["stations"] == 2   # WP and XP
    assert result["industries"] == 1  # KIEL
    assert result["warnings"] == []
    import json
    data = json.loads(out.read_text())
    assert data["layout"] == "export_test"
    assert data["scale"] == "HO"


# ---------------------------------------------------------------------------
# SIDING: optional description
# ---------------------------------------------------------------------------

def test_siding_description_parsed():
    """SIDING: XP North arrival → id=XP, description='North arrival'."""
    layout = parse_file(EXPORT_FIXTURE)
    results = {r.name: r for r in compute_capacities(layout)}
    assert "XP" in results
    assert results["XP"].description == "North arrival"


def test_siding_no_description_empty():
    """FreightHouse note has description; CarBarn also. Verify via _note_prefix directly."""
    from xtrkcad_mcp.stations import _note_prefix
    result = _note_prefix("SIDING: BB")
    assert result is not None
    kind, name, desc = result
    assert name == "BB"
    assert desc == ""


# ---------------------------------------------------------------------------
# INDUSTRY: description and car_spots export
# ---------------------------------------------------------------------------

def test_industry_description_parsed():
    """INDUSTRY: KIEL Coal dealer → description='Coal dealer'."""
    layout = parse_file(EXPORT_FIXTURE)
    results = {r.name: r for r in compute_capacities(layout)}
    assert results["KIEL"].description == "Coal dealer"


def test_industry_car_spots_in_export():
    """KIEL has spots: 2 in stations.yaml → car_spots=2 in export JSON."""
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    data = build_layout_export(layout, config)
    kiel = next(i for i in data["industries"] if i["industry_id"] == "KIEL")
    assert kiel["car_spots"] == 2


def test_industry_spur_description_in_export():
    """KIEL spur description should appear in export industries list."""
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    data = build_layout_export(layout, config)
    kiel = next(i for i in data["industries"] if i["industry_id"] == "KIEL")
    assert kiel["spur_description"] == "Coal dealer"


def test_stations_siding_description_in_export():
    """XP siding description should appear in export stations list."""
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    data = build_layout_export(layout, config)
    xp = next(s for s in data["stations"] if s["station_id"] == "XP")
    assert xp["siding_description"] == "North arrival"


# ---------------------------------------------------------------------------
# compute_yard_tracks
# ---------------------------------------------------------------------------

def test_compute_yard_tracks_finds_wp_lead():
    """YARD_TRACK: WP LEAD note in fixture → one result with yard_id=WP, label=LEAD."""
    layout = parse_file(EXPORT_FIXTURE)
    results = compute_yard_tracks(layout)
    assert len(results) == 1
    yt = results[0]
    assert yt.yard_id == "WP"
    assert yt.label == "LEAD"


def test_compute_yard_tracks_length():
    """YARD_TRACK note at x=10 snaps to track 1 (x=0..20); BFS covers main 1-6 = 120in."""
    layout = parse_file(EXPORT_FIXTURE)
    results = compute_yard_tracks(layout)
    yt = results[0]
    assert yt.length_model_in == pytest.approx(120.0, abs=0.5)


def test_yard_tracks_in_export():
    """yard_tracks array should appear in export JSON with WP/LEAD entry."""
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    data = build_layout_export(layout, config)
    assert "yard_tracks" in data
    yts = {(r["yard_id"], r["label"]) for r in data["yard_tracks"]}
    assert ("WP", "LEAD") in yts


def test_load_stations_config_spots():
    """spots field from YAML should be loaded into StationEntry."""
    config = load_stations_config(EXPORT_STATIONS)
    kiel = next(s for s in config.stations if s.id == "KIEL")
    assert kiel.spots == 2


def test_load_stations_config_spots_none_for_station():
    """Station entries without spots: should default to None."""
    config = load_stations_config(EXPORT_STATIONS)
    wp = next(s for s in config.stations if s.id == "WP")
    assert wp.spots is None


# ---------------------------------------------------------------------------
# @MP_ZERO ref_tag — station adopts reference endpoint for routing
# ---------------------------------------------------------------------------

def test_extract_stations_parses_ref_tag():
    """'STATION: WP @MP_ZERO' should produce name='WP' and ref_tag='MP_ZERO'."""
    layout = parse_file(EXPORT_FIXTURE)
    # Temporarily patch the WP note text to include @MP_ZERO
    from xtrkcad_mcp.stations import extract_stations
    for note in layout.notes:
        if note.text == "STATION: WP":
            note.text = "STATION: WP @MP_ZERO"
    stations = {s.name: s for s in extract_stations(layout)}
    assert "WP" in stations
    assert stations["WP"].ref_tag == "MP_ZERO"


def test_extract_stations_no_ref_tag_by_default():
    """Plain 'STATION: WP' should have ref_tag=None."""
    layout = parse_file(EXPORT_FIXTURE)
    stations = {s.name: s for s in extract_stations(layout)}
    assert stations["WP"].ref_tag is None


def test_resolve_station_endpoints_uses_reference_ep():
    """Station with @MP_ZERO should get the reference point's nearest_ep."""
    from xtrkcad_mcp.stations import extract_stations, extract_reference_points, _resolve_station_endpoints
    layout = parse_file(EXPORT_FIXTURE)
    for note in layout.notes:
        if note.text == "STATION: WP":
            note.text = "STATION: WP @MP_ZERO"
    stations = {s.name: s for s in _resolve_station_endpoints(extract_stations(layout), layout)}
    refs = {r.name: r for r in extract_reference_points(layout)}
    assert stations["WP"].nearest_ep == refs["MP_ZERO"].nearest_ep


def test_distance_report_ordering_by_milepost(tmp_path):
    """Stations in distance report should appear in milepost order (WP first)."""
    import os
    os.environ["XTRKCAD_PLANS_DIR"] = str(FIXTURES_DIR)
    from xtrkcad_mcp.server import write_station_distance_report
    # Patch WP note to @MP_ZERO
    import copy
    layout = parse_file(EXPORT_FIXTURE)
    for note in layout.notes:
        if note.text == "STATION: WP":
            note.text = "STATION: WP @MP_ZERO"

    out = tmp_path / "distances.md"
    # Write using the original unmodified fixture — WP won't have @MP_ZERO but
    # ordering should still put WP first since it's near MP_ZERO
    write_station_distance_report(str(EXPORT_FIXTURE), str(out))
    content = out.read_text()
    # WP should appear in the first data row (nearest to MP_ZERO)
    lines = [l for l in content.splitlines() if "|" in l and "From" not in l and "---" not in l]
    assert lines[0].split("|")[1].strip() == "WP"


def test_distance_report_mp_columns_present(tmp_path):
    """Distance report markdown should include MP column headers."""
    import os
    os.environ["XTRKCAD_PLANS_DIR"] = str(FIXTURES_DIR)
    from xtrkcad_mcp.server import write_station_distance_report
    out = tmp_path / "distances.md"
    write_station_distance_report(str(EXPORT_FIXTURE), str(out))
    content = out.read_text()
    assert "MP (ft)" in content
