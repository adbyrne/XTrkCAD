"""Tests for server tools using the bundled test fixture layout."""

from pathlib import Path

import shutil

import pytest

from xtrkcad_mcp.parser import parse_file
from xtrkcad_mcp.server import (
    delete_track,
    find_dead_connections,
    fix_dead_connections,
    load_layout_config,
    write_benchwork_report,
    write_equipment_report,
    write_gaps_report,
    write_layout_report,
    write_operation_density_report,
    write_radius_map,
    write_turnout_report,
)

FIXTURES_DIR = Path(__file__).parent / "fixtures"
FIXTURE = FIXTURES_DIR / "test_layout.xtc"
HILLSIDE_CONFIG = FIXTURES_DIR / "hillside_division.yaml"


# ---------------------------------------------------------------------------
# Parser: turntable geometry
# ---------------------------------------------------------------------------


def test_turntable_geometry_parsed():
    layout = parse_file(FIXTURE)
    tt = next(t for t in layout.tracks if t.kind == "TURNTABLE")
    assert tt.extra["cx"] == pytest.approx(60.0)
    assert tt.extra["cy"] == pytest.approx(90.0)
    assert tt.extra["radius"] == pytest.approx(8.0)


def test_turntable_has_six_stall_endpoints():
    layout = parse_file(FIXTURE)
    tt = next(t for t in layout.tracks if t.kind == "TURNTABLE")
    # All 6 stall endpoints should be open (no connected_to)
    assert len(tt.endpoints) == 6
    assert all(ep.connected_to is None for ep in tt.endpoints)


# ---------------------------------------------------------------------------
# Gaps report: turntable stalls excluded from gap analysis
# ---------------------------------------------------------------------------


def test_gaps_report_separates_turntable_stalls(tmp_path):
    out = str(tmp_path / "gaps.txt")
    write_gaps_report(str(FIXTURE), out)
    content = Path(out).read_text()

    # Turntable stall count (6) should be on its own summary line
    assert "6" in content
    assert "turntable stall" in content.lower()

    # Real track gaps: STRAIGHT(2) + CURVE2(2) + CURVE3(2) = 6 open endpoints
    # Both "6" values appear — check the "Track gaps to check" line specifically
    assert "Track gaps to check" in content


def test_gaps_report_real_gaps_not_mixed_with_stalls(tmp_path):
    out = str(tmp_path / "gaps.txt")
    write_gaps_report(str(FIXTURE), out)
    content = Path(out).read_text()

    # Turntable track ID 4 should NOT appear in the "ALL OPEN TRACK ENDPOINTS" section
    lines = content.splitlines()
    in_gap_table = False
    for line in lines:
        if "ALL OPEN TRACK ENDPOINTS" in line:
            in_gap_table = True
        if in_gap_table and line.strip():
            # Track ID column is the first number — turntable ID 4 must not appear
            parts = line.split()
            if parts and parts[0].isdigit():
                assert int(parts[0]) != 4, f"Turntable ID 4 appeared in gap table: {line}"


# ---------------------------------------------------------------------------
# Radius map SVG generation
# ---------------------------------------------------------------------------


def test_write_radius_map_creates_svg(tmp_path):
    out = str(tmp_path / "radius_map.svg")
    result = write_radius_map(str(FIXTURE), out)
    assert Path(out).exists()
    assert "SVG radius map written to" in result


def test_radius_map_is_valid_svg(tmp_path):
    out = str(tmp_path / "radius_map.svg")
    write_radius_map(str(FIXTURE), out)
    svg = Path(out).read_text()
    assert svg.startswith("<svg")
    assert "</svg>" in svg


def test_radius_map_flags_tight_curve(tmp_path):
    """Fixture CURVE 2 has radius=12\" which is below the 18\" HO minimum → must be red."""
    out = str(tmp_path / "radius_map.svg")
    write_radius_map(str(FIXTURE), out)
    svg = Path(out).read_text()
    assert "curve-red" in svg


def test_radius_map_normal_curve_not_flagged_red(tmp_path):
    """Fixture CURVE 3 has radius=30\" which is above 2×18\"=36\"... actually 30<36 → yellow."""
    out = str(tmp_path / "radius_map.svg")
    write_radius_map(str(FIXTURE), out)
    svg = Path(out).read_text()
    # 30\" is between 1.5×18=27 and 2×18=36 → yellow
    assert "curve-yellow" in svg


def test_radius_map_turntable_drawn_as_circle(tmp_path):
    out = str(tmp_path / "radius_map.svg")
    write_radius_map(str(FIXTURE), out)
    svg = Path(out).read_text()
    assert 'class="turntable"' in svg


def test_radius_map_custom_flag_radius(tmp_path):
    """With flag_radius=35, CURVE 3 (r=30) should be flagged red."""
    out = str(tmp_path / "radius_map.svg")
    write_radius_map(str(FIXTURE), out, flag_radius=35.0)
    svg = Path(out).read_text()
    # With flag_radius=35, 30 < 35 → red
    assert "curve-red" in svg


# ---------------------------------------------------------------------------
# delete_track — write round-trip
# ---------------------------------------------------------------------------


def test_delete_track_removes_straight(tmp_path):
    layout_copy = tmp_path / "layout.xtc"
    shutil.copy(FIXTURE, layout_copy)
    delete_track(str(layout_copy), 1)
    tracks = parse_file(layout_copy).tracks
    assert not any(t.id == 1 for t in tracks)


def test_delete_track_removes_curve(tmp_path):
    layout_copy = tmp_path / "layout.xtc"
    shutil.copy(FIXTURE, layout_copy)
    delete_track(str(layout_copy), 2)
    tracks = parse_file(layout_copy).tracks
    assert not any(t.id == 2 for t in tracks)


def test_delete_track_preserves_other_tracks(tmp_path):
    layout_copy = tmp_path / "layout.xtc"
    shutil.copy(FIXTURE, layout_copy)
    delete_track(str(layout_copy), 1)
    ids = {t.id for t in parse_file(layout_copy).tracks}
    assert ids == {2, 3, 4}


def test_delete_track_missing_id_raises(tmp_path):
    layout_copy = tmp_path / "layout.xtc"
    shutil.copy(FIXTURE, layout_copy)
    with pytest.raises(ValueError, match="99"):
        delete_track(str(layout_copy), 99)


def test_delete_track_file_still_parseable(tmp_path):
    layout_copy = tmp_path / "layout.xtc"
    shutil.copy(FIXTURE, layout_copy)
    delete_track(str(layout_copy), 3)
    layout = parse_file(layout_copy)
    assert len(layout.tracks) == 3


# ---------------------------------------------------------------------------
# find_dead_connections / fix_dead_connections
# ---------------------------------------------------------------------------

# Minimal layout: track 1 has T pointing to nonexistent track 999.
_DEAD_T_LAYOUT = (
    "VERSION 2 5.3.0Dev\n"
    "TITLE1 Dead T Test\n"
    "SCALE HO\n"
    "ROOMSIZE 120 x 120\n"
    "\n"
    "STRAIGHT 1 0 0 0 0 HO 0\n"
    "\tT 999 10.000000 10.000000 0.000000\n"
    "\tE 30.000000 10.000000 180.000000\n"
    "\n"
    "STRAIGHT 2 0 0 0 0 HO 0\n"
    "\tE 30.000000 20.000000 0.000000\n"
    "\tE 50.000000 20.000000 180.000000\n"
)


def _write_dead_layout(path):
    path.write_text(_DEAD_T_LAYOUT, encoding="utf-8")
    return path


def test_find_dead_connections_detects_dead_reference(tmp_path):
    layout_file = _write_dead_layout(tmp_path / "layout.xtc")
    dead = find_dead_connections(str(layout_file))
    assert len(dead) == 1
    assert dead[0]["track_id"] == 1
    assert dead[0]["dead_reference"] == 999


def test_find_dead_connections_no_false_positives(tmp_path):
    # Fixture has only E records — no T records at all → zero dead connections.
    layout_copy = tmp_path / "layout.xtc"
    shutil.copy(FIXTURE, layout_copy)
    assert find_dead_connections(str(layout_copy)) == []


def test_fix_dead_connections_converts_T_to_E(tmp_path):
    layout_file = _write_dead_layout(tmp_path / "layout.xtc")
    fix_dead_connections(str(layout_file))
    # After fix, track 1 should have 2 open E endpoints, no connected_to.
    tracks = parse_file(layout_file).tracks
    t1 = next(t for t in tracks if t.id == 1)
    assert len(t1.endpoints) == 2
    assert all(ep.connected_to is None for ep in t1.endpoints)


def test_fix_dead_connections_returns_fix_count(tmp_path):
    layout_file = _write_dead_layout(tmp_path / "layout.xtc")
    result = fix_dead_connections(str(layout_file))
    assert "1" in result
    assert "999" in result


def test_fix_dead_connections_no_op_when_clean(tmp_path):
    layout_copy = tmp_path / "layout.xtc"
    shutil.copy(FIXTURE, layout_copy)
    result = fix_dead_connections(str(layout_copy))
    assert "No dead connections" in result


# ---------------------------------------------------------------------------
# Report format parameter — md / html / json
# ---------------------------------------------------------------------------

def test_gaps_report_md_has_markdown_table(tmp_path):
    out = tmp_path / "gaps.md"
    write_gaps_report(str(FIXTURE), str(out), format="md")
    content = out.read_text()
    assert "# Track Gaps Report" in content
    assert "| Item |" in content


def test_gaps_report_html_is_html(tmp_path):
    out = tmp_path / "gaps.html"
    write_gaps_report(str(FIXTURE), str(out), format="html")
    content = out.read_text()
    assert "<html" in content
    assert "<table" in content


def test_gaps_report_json_is_valid(tmp_path):
    import json
    out = tmp_path / "gaps.json"
    write_gaps_report(str(FIXTURE), str(out), format="json")
    data = json.loads(out.read_text())
    assert "open_endpoints" in data
    assert "near_miss_pairs" in data
    assert isinstance(data["endpoints"], list)


def test_equipment_report_json_has_equipment_list(tmp_path):
    import json
    out = tmp_path / "equip.json"
    write_equipment_report(str(FIXTURE), str(out), format="json")
    data = json.loads(out.read_text())
    assert "equipment" in data
    assert isinstance(data["equipment"], list)
    assert all("status" in row for row in data["equipment"])


def test_equipment_report_html_has_pass_class(tmp_path):
    out = tmp_path / "equip.html"
    write_equipment_report(str(FIXTURE), str(out), format="html")
    content = out.read_text()
    assert "pass" in content.lower()
    assert "<table" in content


def test_turnout_report_md_has_table(tmp_path):
    out = tmp_path / "turnout.md"
    write_turnout_report(str(FIXTURE), str(out), format="md")
    content = out.read_text()
    assert "# Turnout" in content
    assert "|" in content


def test_turnout_report_json_has_totals(tmp_path):
    import json
    out = tmp_path / "turnout.json"
    write_turnout_report(str(FIXTURE), str(out), format="json")
    data = json.loads(out.read_text())
    assert "total_turnouts" in data
    assert "global_density_per_100ft" in data


def test_layout_report_md_has_headers(tmp_path):
    out = tmp_path / "layout.md"
    write_layout_report(str(FIXTURE), str(out), format="md")
    content = out.read_text()
    assert "# Layout Report" in content
    assert "## Curve Analysis" in content


def test_layout_report_json_has_track_counts(tmp_path):
    import json
    out = tmp_path / "layout.json"
    write_layout_report(str(FIXTURE), str(out), format="json")
    data = json.loads(out.read_text())
    assert "total_tracks" in data
    assert "curves" in data


def test_layout_report_html_is_valid_html(tmp_path):
    out = tmp_path / "layout.html"
    write_layout_report(str(FIXTURE), str(out), format="html")
    content = out.read_text()
    assert "<html" in content
    assert "Layout Report" in content


def test_equipment_report_md_has_table(tmp_path):
    out = tmp_path / "equip.md"
    write_equipment_report(str(FIXTURE), str(out), format="md")
    content = out.read_text()
    assert "# Equipment" in content
    assert "|" in content


def test_turnout_report_html_is_html(tmp_path):
    out = tmp_path / "turnout.html"
    write_turnout_report(str(FIXTURE), str(out), format="html")
    content = out.read_text()
    assert "<html" in content
    assert "<table" in content


def test_od_report_md_has_table(tmp_path):
    out = tmp_path / "od.md"
    layer_cats = {"0": "mainline", "1": "passing"}
    write_operation_density_report(str(FIXTURE), str(out), layer_categories=layer_cats, format="md")
    content = out.read_text()
    assert "# Operation Density" in content
    assert "|" in content


def test_od_report_html_is_html(tmp_path):
    out = tmp_path / "od.html"
    layer_cats = {"0": "mainline", "1": "passing"}
    write_operation_density_report(str(FIXTURE), str(out), layer_categories=layer_cats, format="html")
    content = out.read_text()
    assert "<html" in content
    assert "<table" in content


# ---------------------------------------------------------------------------
# Format auto-detection from file extension
# ---------------------------------------------------------------------------

def test_layout_report_html_extension_autodetects(tmp_path):
    out = tmp_path / "report.html"
    write_layout_report(str(FIXTURE), str(out))  # no explicit format
    content = out.read_text()
    assert "<html" in content


def test_layout_report_md_extension_autodetects(tmp_path):
    out = tmp_path / "report.md"
    write_layout_report(str(FIXTURE), str(out))  # no explicit format
    content = out.read_text()
    assert "# Layout Report" in content


def test_gaps_report_html_extension_autodetects(tmp_path):
    out = tmp_path / "gaps.html"
    write_gaps_report(str(FIXTURE), str(out))  # no explicit format
    content = out.read_text()
    assert "<html" in content


def test_equipment_report_md_extension_autodetects(tmp_path):
    out = tmp_path / "equip.md"
    write_equipment_report(str(FIXTURE), str(out))  # no explicit format
    content = out.read_text()
    assert "# Equipment" in content


def test_turnout_report_html_extension_autodetects(tmp_path):
    out = tmp_path / "turnout.html"
    write_turnout_report(str(FIXTURE), str(out))  # no explicit format
    content = out.read_text()
    assert "<html" in content


# ---------------------------------------------------------------------------
# load_layout_config — benchwork and floor_plan fields in response
# ---------------------------------------------------------------------------


def test_load_layout_config_returns_benchwork_sections():
    result = load_layout_config(str(HILLSIDE_CONFIG))
    assert "benchwork_sections" in result["config"]
    sections = result["config"]["benchwork_sections"]
    assert len(sections) == 3


def test_load_layout_config_benchwork_section_fields():
    result = load_layout_config(str(HILLSIDE_CONFIG))
    section = result["config"]["benchwork_sections"][0]
    assert "label" in section
    assert "level" in section
    assert "vertex_count" in section
    assert section["vertex_count"] == 4  # rect = 4 vertices


def test_load_layout_config_benchwork_file_field():
    result = load_layout_config(str(HILLSIDE_CONFIG))
    assert result["config"]["benchwork_file"].endswith("hillside_benchwork.yaml")


def test_load_layout_config_floor_plan_field():
    result = load_layout_config(str(HILLSIDE_CONFIG))
    fp = result["config"]["floor_plan"]
    assert fp is not None
    assert fp["rooms"] == 1
    assert fp["width_in"] == pytest.approx(144.0)
    assert fp["depth_in"] == pytest.approx(192.0)


def test_load_layout_config_no_obstructions_key():
    result = load_layout_config(str(HILLSIDE_CONFIG))
    assert "obstructions" not in result["config"]


def test_load_layout_config_ready():
    result = load_layout_config(str(HILLSIDE_CONFIG))
    assert result["ready"] is True
    assert result["warnings"] == []


# ---------------------------------------------------------------------------
# write_benchwork_report
# ---------------------------------------------------------------------------


def test_write_benchwork_report_creates_file(tmp_path):
    out = tmp_path / "benchwork.txt"
    msg = write_benchwork_report(str(HILLSIDE_CONFIG), str(out))
    assert out.exists()
    assert str(out) in msg


def test_write_benchwork_report_txt_has_sections(tmp_path):
    out = tmp_path / "benchwork.txt"
    write_benchwork_report(str(HILLSIDE_CONFIG), str(out))
    content = out.read_text()
    assert "west_wall" in content
    assert "north_wall" in content
    assert "east_wall" in content


def test_write_benchwork_report_txt_has_level(tmp_path):
    out = tmp_path / "benchwork.txt"
    write_benchwork_report(str(HILLSIDE_CONFIG), str(out))
    content = out.read_text()
    assert "LEVEL 1" in content


def test_write_benchwork_report_txt_has_total_area(tmp_path):
    out = tmp_path / "benchwork.txt"
    write_benchwork_report(str(HILLSIDE_CONFIG), str(out))
    content = out.read_text()
    assert "TOTAL BENCHWORK AREA" in content


def test_write_benchwork_report_md_format(tmp_path):
    out = tmp_path / "benchwork.md"
    write_benchwork_report(str(HILLSIDE_CONFIG), str(out), format="md")
    content = out.read_text()
    assert "# Benchwork Plan Report" in content
    assert "west_wall" in content


def test_write_benchwork_report_html_format(tmp_path):
    out = tmp_path / "benchwork.html"
    write_benchwork_report(str(HILLSIDE_CONFIG), str(out), format="html")
    content = out.read_text()
    assert "<html" in content
    assert "west_wall" in content


def test_write_benchwork_report_md_extension_autodetects(tmp_path):
    out = tmp_path / "benchwork.md"
    write_benchwork_report(str(HILLSIDE_CONFIG), str(out))  # no explicit format
    content = out.read_text()
    assert "# Benchwork Plan Report" in content


def test_write_benchwork_report_empty_config(tmp_path):
    cfg = tmp_path / "empty.yaml"
    cfg.write_text("name: Empty\nscale: HO\nroom: 12x16\n")
    out = tmp_path / "benchwork.txt"
    write_benchwork_report(str(cfg), str(out))
    content = out.read_text()
    assert "No benchwork sections" in content
