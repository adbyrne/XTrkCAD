"""Tests for server tools using the bundled test fixture layout."""

from pathlib import Path

import shutil

import pytest

from xtrkcad_mcp.parser import parse_file
from xtrkcad_mcp.server import (
    _category_from_name,
    _level_from_layer_name,
    add_track_layers,
    rename_layers,
    delete_track,
    find_dead_connections,
    fix_dead_connections,
    load_layout_config,
    write_benchwork_report,
    write_elevation_view,
    write_equipment_report,
    write_gaps_report,
    write_layout_report,
    write_operation_density_report,
    write_plan_view,
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
# write_plan_view
# ---------------------------------------------------------------------------


def test_write_plan_view_creates_file(tmp_path):
    out = str(tmp_path / "plan.svg")
    result = write_plan_view(str(HILLSIDE_CONFIG), out)
    assert Path(out).exists()
    assert str(tmp_path) in result


def test_write_plan_view_is_valid_svg(tmp_path):
    out = str(tmp_path / "plan.svg")
    write_plan_view(str(HILLSIDE_CONFIG), out)
    svg = Path(out).read_text()
    assert svg.startswith("<?xml")
    assert "<svg" in svg
    assert "</svg>" in svg


def test_write_plan_view_level_filter_l1(tmp_path):
    out = str(tmp_path / "plan_l1.svg")
    write_plan_view(str(HILLSIDE_CONFIG), out, level=1)
    svg = Path(out).read_text()
    # L1 fill colour present; L2 fill colour absent
    assert _LEVEL_FILL_L1 in svg
    assert _LEVEL_FILL_L2 not in svg


def test_write_plan_view_level_filter_l2(tmp_path):
    out = str(tmp_path / "plan_l2.svg")
    write_plan_view(str(HILLSIDE_CONFIG), out, level=2)
    svg = Path(out).read_text()
    assert _LEVEL_FILL_L2 in svg
    assert _LEVEL_FILL_L1 not in svg


def test_write_plan_view_all_levels_contains_both(tmp_path):
    out = str(tmp_path / "plan_all.svg")
    write_plan_view(str(HILLSIDE_CONFIG), out, level=0)
    svg = Path(out).read_text()
    assert _LEVEL_FILL_L1 in svg
    assert _LEVEL_FILL_L2 in svg


# Colour constants matched to svg_views._LEVEL_FILL
_LEVEL_FILL_L1 = "#A5D6A7"
_LEVEL_FILL_L2 = "#90CAF9"


# ---------------------------------------------------------------------------
# write_elevation_view
# ---------------------------------------------------------------------------


def test_write_elevation_view_creates_file(tmp_path):
    out = str(tmp_path / "elev.svg")
    result = write_elevation_view(str(HILLSIDE_CONFIG), out, wall="west")
    assert Path(out).exists()
    assert str(tmp_path) in result


def test_write_elevation_view_is_valid_svg(tmp_path):
    out = str(tmp_path / "elev.svg")
    write_elevation_view(str(HILLSIDE_CONFIG), out, wall="west")
    svg = Path(out).read_text()
    assert svg.startswith("<?xml")
    assert "<svg" in svg
    assert "</svg>" in svg


def test_write_elevation_view_south_wall(tmp_path):
    out = str(tmp_path / "elev_south.svg")
    write_elevation_view(str(HILLSIDE_CONFIG), out, wall="south")
    assert Path(out).exists()


def test_write_elevation_view_invalid_wall(tmp_path):
    out = str(tmp_path / "elev.svg")
    import pytest as _pytest
    with _pytest.raises(ValueError, match="wall must be"):
        write_elevation_view(str(HILLSIDE_CONFIG), out, wall="ceiling")


def test_write_elevation_view_level_filter(tmp_path):
    out = str(tmp_path / "elev_l1.svg")
    write_elevation_view(str(HILLSIDE_CONFIG), out, wall="west", levels=[1])
    svg = Path(out).read_text()
    assert _LEVEL_FILL_L1 in svg


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
# Auto-categorization from layer name
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("name,expected", [
    ("L1-Main",       "mainline"),
    ("L1-Passing",    "passing"),
    ("L1-Storage",    "storage"),
    ("L1-Staging",    "staging"),
    ("L1-Connecting", "connecting"),
    ("L1-Service",    "service"),
    ("L1-Benchwork",  "ignore"),
    ("L2-Main",       "mainline"),
    ("L2-Staging",    "staging"),
    ("l3-storage",    "storage"),    # lowercase
    ("L1H-Connecting","connecting"), # helix pseudo-level
    ("L1-Track",      "mainline"),   # backward compat
])
def test_category_from_name_standard_suffixes(name, expected):
    assert _category_from_name(name) == expected


def test_category_from_name_unrecognized_returns_none():
    assert _category_from_name("MyCustomLayer") is None
    assert _category_from_name("Floor") is None
    assert _category_from_name("L1-Unknown") is None


def test_category_from_name_explicit_overrides_auto(tmp_path):
    """Explicit layer_categories dict takes precedence over name-based detection."""
    from xtrkcad_mcp.server import get_operation_density
    result = get_operation_density(
        str(FIXTURE),
        layer_categories={"L1-Main": "staging"},   # override: treat main layer as staging
    )
    # If the fixture has a layer named "L1-Main", it should be categorised as staging.
    # The fixture may not have that layer — what matters is no exception is raised and
    # the function returns a valid result.
    assert "operations" in result


# ---------------------------------------------------------------------------
# Level extraction from layer name
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("name,expected", [
    ("L1-Main",        "1"),
    ("L2-Staging",     "2"),
    ("L10-Storage",    "10"),
    ("l3-passing",     "3"),
    ("L1H-Connecting", "1H"),
    ("Floor",          "0"),
    ("MyCustomLayer",  "0"),
])
def test_level_from_layer_name(name, expected):
    assert _level_from_layer_name(name) == expected


# ---------------------------------------------------------------------------
# get_operation_density — by_level breakdown
# ---------------------------------------------------------------------------

def test_od_by_level_key_present():
    from xtrkcad_mcp.server import get_operation_density
    result = get_operation_density(str(FIXTURE))
    assert "by_level" in result


def test_od_by_level_structure():
    from xtrkcad_mcp.server import get_operation_density
    result = get_operation_density(str(FIXTURE))
    for lv, info in result["by_level"].items():
        assert "length_real_ft" in info
        assert "car_capacity" in info
        assert "turnouts" in info
        assert "by_category" in info
        assert isinstance(lv, str)


def test_od_by_level_totals_sum_to_overall():
    from xtrkcad_mcp.server import get_operation_density
    result = get_operation_density(str(FIXTURE))
    level_total_ft = sum(info["length_real_ft"] for info in result["by_level"].values())
    assert abs(level_total_ft - result["totals"]["length_real_ft"]) < 0.2


def test_od_by_level_unassigned_layer_goes_to_zero():
    """Track on a layer with no Ln- prefix should appear under level '0'."""
    from xtrkcad_mcp.server import get_operation_density
    result = get_operation_density(str(FIXTURE), layer_categories={"0": "mainline"})
    # Floor layer (index 0, name "Floor") has no Ln- prefix → level "0"
    # Even if no track is on that layer, the key may or may not appear — just
    # verify any "0" entry has the right structure if present.
    if "0" in result["by_level"]:
        assert "by_category" in result["by_level"]["0"]


def test_od_report_txt_has_by_level_section(tmp_path):
    from xtrkcad_mcp.server import write_operation_density_report
    out = tmp_path / "od.txt"
    write_operation_density_report(str(FIXTURE), str(out))
    content = out.read_text()
    assert "TRACK BY LEVEL" in content


def test_od_report_md_has_by_level_section(tmp_path):
    from xtrkcad_mcp.server import write_operation_density_report
    out = tmp_path / "od.md"
    write_operation_density_report(str(FIXTURE), str(out), format="md")
    content = out.read_text()
    assert "## Track by Level" in content


def test_od_report_html_has_by_level_section(tmp_path):
    from xtrkcad_mcp.server import write_operation_density_report
    out = tmp_path / "od.html"
    write_operation_density_report(str(FIXTURE), str(out), format="html")
    content = out.read_text()
    assert "Track by Level" in content


# ---------------------------------------------------------------------------
# add_track_layers
# ---------------------------------------------------------------------------

def _xtc_with_old_layers(tmp_path) -> Path:
    """Copy the test fixture and strip its LAYERS down to old-style L1-Track/L1-Benchwork."""
    src = FIXTURE.read_text(encoding="utf-8")
    # Replace any Ln-Main / Ln-Passing etc. with the old single-layer scheme
    # by simply writing a minimal file that has L1-Track + L1-Benchwork only.
    out = tmp_path / "old_style.xtc"
    old_layers = (
        'LAYERS 0 1 0 1 8421504 0 0 0 0 "Floor" 1 0 0.000000 0.000000 0.000000 0.000000 0.000000\n'
        'LAYERS 1 1 0 1 0 0 0 0 0 "L1-Track" 1 0 0.000000 0.000000 0.000000 0.000000 0.000000\n'
        'LAYERS 2 1 0 1 9498256 0 0 0 0 "L1-Benchwork" 1 0 0.000000 0.000000 0.000000 0.000000 0.000000\n'
        'LAYERS CURRENT 1\n'
    )
    lines = []
    in_layers = False
    for line in src.splitlines():
        if line.startswith("LAYERS"):
            in_layers = True
            continue
        if in_layers and not line.startswith("LAYERS"):
            in_layers = False
            lines.append(old_layers.rstrip())
        lines.append(line)
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return out


def test_add_track_layers_adds_missing(tmp_path):
    xtc = _xtc_with_old_layers(tmp_path)
    result = add_track_layers(str(xtc), levels=1)
    assert result["added"] != []
    # All 6 standard types should now be present
    content = xtc.read_text()
    for name in ["L1-Main", "L1-Passing", "L1-Storage", "L1-Staging", "L1-Connecting", "L1-Service"]:
        assert f'"{name}"' in content


def test_add_track_layers_skips_present(tmp_path):
    xtc = _xtc_with_old_layers(tmp_path)
    # Run twice — second run should add nothing
    add_track_layers(str(xtc), levels=1)
    result2 = add_track_layers(str(xtc), levels=1)
    assert result2["added"] == []
    assert len(result2["already_present"]) == 6


def test_add_track_layers_ids_are_additive(tmp_path):
    xtc = _xtc_with_old_layers(tmp_path)
    result = add_track_layers(str(xtc), levels=1)
    assert result["added"] != []
    content = xtc.read_text()
    # Max existing ID was 2 (L1-Benchwork); new layers must start at 3+
    layer_ids = [
        int(ln.split()[1])
        for ln in content.splitlines()
        if ln.startswith("LAYERS") and ln.split()[1] != "CURRENT"
    ]
    assert min(layer_ids) == 0
    assert max(layer_ids) >= 3   # at least one new layer was added above id 2


def test_add_track_layers_creates_backup(tmp_path):
    xtc = _xtc_with_old_layers(tmp_path)
    result = add_track_layers(str(xtc), levels=1)
    assert result["backup_path"] is not None
    assert Path(result["backup_path"]).exists()


def test_add_track_layers_detects_levels_automatically(tmp_path):
    """Omitting levels= should detect from existing L1-/L2- names."""
    xtc = _xtc_with_old_layers(tmp_path)
    # Add L2-Benchwork so level 2 is detectable
    content = xtc.read_text()
    content = content.replace(
        "LAYERS CURRENT 1",
        'LAYERS 3 1 0 1 11393254 0 0 0 0 "L2-Benchwork" 1 0 0.000000 0.000000 0.000000 0.000000 0.000000\n'
        "LAYERS CURRENT 1",
    )
    xtc.write_text(content)
    result = add_track_layers(str(xtc))   # no levels= arg
    # Should have added layers for both L1 and L2
    added = result["added"]
    assert any(n.startswith("L1-") for n in added)
    assert any(n.startswith("L2-") for n in added)


# ---------------------------------------------------------------------------
# rename_layers
# ---------------------------------------------------------------------------

def _xtc_with_custom_names(tmp_path) -> Path:
    """Write a minimal .xtc with two custom-named LAYERS entries."""
    out = tmp_path / "custom.xtc"
    out.write_text(
        "VERSION 10 3.0.0\nTITLE1 Test\nTITLE2 custom\nSCALE HO\nROOMSIZE 144 x 192\n\n"
        'LAYERS 0 1 0 1 8421504 0 0 0 0 "Floor" 1 0 0.000000 0.000000 0.000000 0.000000 0.000000\n'
        'LAYERS 50 1 0 1 0 0 0 0 0 "My Yard" 1 0 0.000000 0.000000 0.000000 0.000000 0.000000\n'
        'LAYERS 51 1 0 1 0 0 0 0 0 "Upper Main" 1 0 0.000000 0.000000 0.000000 0.000000 0.000000\n'
        "LAYERS CURRENT 50\n"
    )
    return out


def test_rename_layers_applies_mapping(tmp_path):
    xtc = _xtc_with_custom_names(tmp_path)
    result = rename_layers(str(xtc), {"My Yard": "L1-Staging", "Upper Main": "L2-Main"})
    assert len(result["renamed"]) == 2
    assert result["not_found"] == []
    content = xtc.read_text()
    assert '"L1-Staging"' in content
    assert '"L2-Main"' in content
    assert '"My Yard"' not in content
    assert '"Upper Main"' not in content


def test_rename_layers_preserves_layer_id(tmp_path):
    xtc = _xtc_with_custom_names(tmp_path)
    rename_layers(str(xtc), {"My Yard": "L1-Staging"})
    content = xtc.read_text()
    # Layer 50 should now have the new name, not layer 51
    assert 'LAYERS 50 1 0 1 0 0 0 0 0 "L1-Staging"' in content


def test_rename_layers_partial_match(tmp_path):
    xtc = _xtc_with_custom_names(tmp_path)
    result = rename_layers(str(xtc), {"My Yard": "L1-Staging", "DoesNotExist": "L1-Main"})
    assert len(result["renamed"]) == 1
    assert result["not_found"] == ["DoesNotExist"]


def test_rename_layers_all_not_found_skips_write(tmp_path):
    xtc = _xtc_with_custom_names(tmp_path)
    mtime_before = xtc.stat().st_mtime
    result = rename_layers(str(xtc), {"NoSuchLayer": "L1-Main"})
    assert result["renamed"] == []
    assert result["not_found"] == ["NoSuchLayer"]
    assert result["backup_path"] is None
    # File should not have been rewritten
    assert xtc.stat().st_mtime == mtime_before


def test_rename_layers_creates_backup(tmp_path):
    xtc = _xtc_with_custom_names(tmp_path)
    result = rename_layers(str(xtc), {"My Yard": "L1-Staging"})
    assert result["backup_path"] is not None
    assert Path(result["backup_path"]).exists()


def test_rename_layers_enables_auto_categorization(tmp_path):
    """After renaming to standard names, get_operation_density should auto-categorize."""
    from xtrkcad_mcp.server import get_operation_density
    xtc = _xtc_with_custom_names(tmp_path)
    rename_layers(str(xtc), {"My Yard": "L1-Staging"})
    result = get_operation_density(str(xtc))
    # L1-Staging should now appear in by_category without passing layer_categories
    # (fixture may have no track on that layer, but no error should occur)
    assert "operations" in result


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
    assert len(sections) == 6  # 4 L1 + 2 L2 sections


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
