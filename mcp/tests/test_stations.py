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
    # 30=STATION:Alpha, 11=STATION:Beta, 12=non-station, 13=STATION:Gamma
    # 20=STORAGE:FreightHouse, 21=STORAGE:CarBarn
    assert {30, 11, 12, 13, 20, 21}.issubset(note_ids)


def test_parser_note_op_and_text():
    layout = parse_file(STATION_FIXTURE)
    notes = {n.id: n for n in layout.notes}
    assert notes[30].op == 0
    assert notes[30].text == "STATION: Alpha"
    assert notes[30].x == pytest.approx(0.0)
    assert notes[30].y == pytest.approx(0.0)


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
    assert results["Freight House"].kind == "storage"
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


# ---------------------------------------------------------------------------
# compute_capacities — layer-based inference fallback
# ---------------------------------------------------------------------------

def test_layer_inference_finds_passing_capacity():
    """XP passing track capacity comes from the L1-Passing layer (no note needed)."""
    layout = parse_file(EXPORT_FIXTURE)
    results = {r.name: r for r in compute_capacities(layout)}
    assert "XP" in results
    # Tracks 7+8 = 20in + 20in = 40in
    assert results["XP"].length_model_in == pytest.approx(40.0, abs=0.1)


def test_layer_inference_kind_is_passing_for_passing_layer():
    layout = parse_file(EXPORT_FIXTURE)
    results = {r.name: r for r in compute_capacities(layout)}
    assert results["XP"].kind == "passing"


def test_layer_inference_note_id_is_minus_one():
    """Layer-inferred results carry note_id=-1 to distinguish them from explicit notes."""
    layout = parse_file(EXPORT_FIXTURE)
    results = {r.name: r for r in compute_capacities(layout)}
    assert results["XP"].note_id == -1


def test_station_notes_not_in_capacities_no_passing_layers():
    """station_layout.xtc has no standard passing-suffix layers → no inference fires."""
    layout = parse_file(STATION_FIXTURE)
    results = compute_capacities(layout)
    names = {r.name for r in results}
    assert "Alpha" not in names
    assert "Beta" not in names


def test_layer_inference_car_count():
    """40 model in / 12 = 3.33 model ft × 2 cars/ft = 6 cars (HO)."""
    layout = parse_file(EXPORT_FIXTURE)
    results = {r.name: r for r in compute_capacities(layout)}
    assert results["XP"].max_cars == 6


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
# load_stations_config
# ---------------------------------------------------------------------------

def test_load_stations_config_fields():
    config = load_stations_config(EXPORT_STATIONS)
    assert config.layout == "export_test"
    assert config.mp_scale == pytest.approx(1.0)
    assert len(config.stations) == 2


def test_load_stations_config_ids():
    config = load_stations_config(EXPORT_STATIONS)
    ids = [s.id for s in config.stations]
    assert "WP" in ids
    assert "XP" in ids


def test_load_stations_config_types():
    config = load_stations_config(EXPORT_STATIONS)
    by_id = {s.id: s for s in config.stations}
    assert "station" in by_id["WP"].types
    assert "station" in by_id["XP"].types


def test_load_stations_config_sequence():
    config = load_stations_config(EXPORT_STATIONS)
    seqs = {s.id: s.sequence for s in config.stations}
    assert seqs["WP"] == 0
    assert seqs["XP"] == 1


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
    """XP is 120 model inches from WP. mp_scale=1.0 (layout in/MP) → 120 MPs."""
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    refs = {r.name: r for r in extract_reference_points(layout)}
    results = {r.station_id: r for r in compute_mileposts(layout, config, refs["MP_ZERO"])}
    expected_mp = 120.0 / 1.0   # d (layout in) / mp_scale (layout in/MP)
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
    """Fixture has 4 annotations: REFERENCE:MP_ZERO, STATION:WP, STATION:XP,
    INDUSTRY:KIEL Kiel Co.  Passing track capacity comes from L1-Passing layer, not a note."""
    layout = parse_file(EXPORT_FIXTURE)
    segs = list_annotated_segments(layout)
    assert len(segs) == 4


def test_list_annotated_segments_types():
    layout = parse_file(EXPORT_FIXTURE)
    segs = list_annotated_segments(layout)
    types = {s.annotation_type for s in segs}
    assert "station" in types
    assert "industry" in types
    assert "reference" in types
    assert "siding" not in types


def test_list_annotated_segments_ids():
    layout = parse_file(EXPORT_FIXTURE)
    segs = {s.annotation_id: s for s in list_annotated_segments(layout)}
    assert "WP" in segs
    assert "XP" in segs
    assert "KIEL" in segs
    assert "MP_ZERO" in segs


def test_list_annotated_segments_source_is_note():
    layout = parse_file(EXPORT_FIXTURE)
    segs = list_annotated_segments(layout)
    assert all(s.annotation_source == "note" for s in segs)


def test_list_annotated_segments_layer_names():
    layout = parse_file(EXPORT_FIXTURE)
    segs = {s.annotation_id: s for s in list_annotated_segments(layout)}
    assert segs["WP"].nearest_track_layer_name == "L1-Main"
    assert segs["XP"].nearest_track_layer_name == "L1-Main"


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


def test_validate_industry_note_parsed_from_layout():
    """Industries are note-driven: INDUSTRY: KIEL Kiel Co → annotation_id='KIEL'."""
    layout = parse_file(EXPORT_FIXTURE)
    segs = list_annotated_segments(layout)
    ind = next((s for s in segs if s.annotation_type == "industry"), None)
    assert ind is not None
    assert ind.annotation_id == "KIEL"
    assert ind.annotation_name == "Kiel Co"
    assert ind.annotation_within is None


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


def test_build_layout_export_station_has_main_length_fields():
    """Every station dict must contain main_length_ft and main_length_cars keys."""
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    data = build_layout_export(layout, config)
    for s in data["stations"]:
        assert "main_length_ft" in s, f"{s['station_id']} missing main_length_ft"
        assert "main_length_cars" in s, f"{s['station_id']} missing main_length_cars"


def test_build_layout_export_wp_main_length_not_null():
    """WP has a STATION: note and the fixture has an L1-Main layer → main_length non-null."""
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    data = build_layout_export(layout, config)
    wp = next(s for s in data["stations"] if s["station_id"] == "WP")
    assert wp["main_length_ft"] is not None
    assert wp["main_length_ft"] > 0


def test_build_layout_export_no_main_siding_warning_when_unbounded():
    """Export fixture main has no turnouts → component is unbounded → no mismatch warning.

    This is the expected behaviour: when no switch bounds the main at a station
    (e.g. a simple point-to-point with no intermediate turnouts on the main)
    the comparison against siding length would be meaningless, so no warning fires.
    """
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    data = build_layout_export(layout, config)
    mismatch_warnings = [w for w in data["warnings"] if "differ by" in w]
    assert mismatch_warnings == []


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
    expected = 120.0 / 1.0   # d (layout in) / mp_scale (layout in/MP) = 120 MPs
    assert xp["milepost_entry"] == pytest.approx(expected, rel=1e-2)


def test_terminus_station_exit_is_null():
    """terminus: true → milepost_exit is null (no through exit)."""
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    wp_entry = next(e for e in config.stations if e.id == "WP")
    wp_entry.terminus = True
    data = build_layout_export(layout, config)
    wp = next(s for s in data["stations"] if s["station_id"] == "WP")
    assert wp["terminus"] is True
    assert wp["milepost_exit"] is None


def test_terminus_in_station_output():
    """terminus field appears in every station dict."""
    layout = parse_file(EXPORT_FIXTURE)
    config = load_stations_config(EXPORT_STATIONS)
    data = build_layout_export(layout, config)
    for s in data["stations"]:
        assert "terminus" in s


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
    # Station sequence: WP(0) → XP(1); KIEL is an industry, not in segments
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
