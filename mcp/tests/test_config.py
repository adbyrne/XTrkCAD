"""Tests for layout config parser."""

from pathlib import Path

import pytest

from xtrkcad_mcp.config import (
    Benchwork,
    FloorDoor,
    FloorPartition,
    FloorPlan,
    FloorRestricted,
    FloorRoom,
    GridPlacement,
    _default_grid_cell_ft,
    _parse_cell_or_range,
    _parse_grid_entry,
    _parse_length_in,
    _parse_room,
    load_config,
)

FIXTURES_DIR = Path(__file__).parent / "fixtures"
FULL_CONFIG = FIXTURES_DIR / "test_layout_config.yaml"
FLOOR_PLAN_CONFIG = FIXTURES_DIR / "hillside_division.yaml"


# ---------------------------------------------------------------------------
# _parse_room
# ---------------------------------------------------------------------------

def test_parse_room_basic():
    assert _parse_room("12x16") == (12.0, 16.0)

def test_parse_room_spaces():
    assert _parse_room("12 x 16") == (12.0, 16.0)

def test_parse_room_unicode_times():
    assert _parse_room("12×16") == (12.0, 16.0)

def test_parse_room_float():
    w, d = _parse_room("10.5x8.5")
    assert w == pytest.approx(10.5)
    assert d == pytest.approx(8.5)

def test_parse_room_bad():
    with pytest.raises(ValueError, match="invalid room size"):
        _parse_room("12")


# ---------------------------------------------------------------------------
# _parse_length_in
# ---------------------------------------------------------------------------

def test_parse_length_inches_suffix():
    assert _parse_length_in("22in") == pytest.approx(22.0)

def test_parse_length_quote_suffix():
    assert _parse_length_in('22"') == pytest.approx(22.0)

def test_parse_length_ft_suffix():
    assert _parse_length_in("2ft") == pytest.approx(24.0)

def test_parse_length_bare_number():
    assert _parse_length_in("18.5") == pytest.approx(18.5)


# ---------------------------------------------------------------------------
# _parse_cell_or_range
# ---------------------------------------------------------------------------

def test_parse_single_cell():
    assert _parse_cell_or_range("A3") == ("A", 3, "A", 3)

def test_parse_range():
    assert _parse_cell_or_range("A3-C5") == ("A", 3, "C", 5)

def test_parse_multi_letter_col():
    assert _parse_cell_or_range("AB12") == ("AB", 12, "AB", 12)

def test_parse_bad_cell():
    with pytest.raises(ValueError, match="invalid cell/range"):
        _parse_cell_or_range("3A")


# ---------------------------------------------------------------------------
# _default_grid_cell_ft
# ---------------------------------------------------------------------------

def test_grid_cell_ho_22in():
    # 2 * 22 / 12 = 3.67 → ceil = 4
    assert _default_grid_cell_ft(22.0) == 4.0

def test_grid_cell_n_9_75in():
    # 2 * 9.75 / 12 = 1.625 → ceil = 2, but min=3 → 3
    assert _default_grid_cell_ft(9.75) == 3.0

def test_grid_cell_o_54in():
    # 2 * 54 / 12 = 9.0 → 9
    assert _default_grid_cell_ft(54.0) == 9.0


# ---------------------------------------------------------------------------
# _parse_grid_entry
# ---------------------------------------------------------------------------

def test_parse_grid_single_cell():
    p = _parse_grid_entry("E4=station=passing siding, 100ft")
    assert p.col_start == "E" and p.row_start == 4
    assert p.col_end == "E" and p.row_end == 4
    assert p.element_type == "station"
    assert p.params == "passing siding, 100ft"

def test_parse_grid_range():
    p = _parse_grid_entry("A3-C5=yard=single-ended, 3 tracks")
    assert p.col_start == "A" and p.row_start == 3
    assert p.col_end == "C" and p.row_end == 5
    assert p.element_type == "yard"

def test_parse_grid_no_params():
    p = _parse_grid_entry("B1-B8=mainline")
    assert p.element_type == "mainline"
    assert p.params == ""

def test_parse_grid_cell_range_property():
    p = _parse_grid_entry("A3-C5=yard=params")
    assert p.cell_range == "A3-C5"

def test_parse_grid_single_cell_range_property():
    p = _parse_grid_entry("E4=station")
    assert p.cell_range == "E4"

def test_parse_grid_unknown_type():
    with pytest.raises(ValueError, match="unknown element type"):
        _parse_grid_entry("A1=turntable=params")

def test_parse_grid_missing_type():
    with pytest.raises(ValueError):
        _parse_grid_entry("A1")


# ---------------------------------------------------------------------------
# load_config — full fixture
# ---------------------------------------------------------------------------

def test_load_full_config_ready():
    result = load_config(FULL_CONFIG)
    assert result.ready is True
    assert result.missing == []

def test_load_full_config_name():
    result = load_config(FULL_CONFIG)
    assert result.config.name == "Ridgeline Division"

def test_load_full_config_scale():
    result = load_config(FULL_CONFIG)
    assert result.config.scale == "HO"

def test_load_full_config_room():
    result = load_config(FULL_CONFIG)
    assert result.config.room_width_ft == pytest.approx(12.0)
    assert result.config.room_depth_ft == pytest.approx(16.0)

def test_load_full_config_mainline():
    result = load_config(FULL_CONFIG)
    assert result.config.mainline == "dual"

def test_load_full_config_radius():
    result = load_config(FULL_CONFIG)
    assert result.config.curve_radius_in == pytest.approx(22.0)

def test_load_full_config_levels():
    result = load_config(FULL_CONFIG)
    assert result.config.levels == 2
    assert result.config.level_separation_in == pytest.approx(18.0)
    assert result.config.level_break_row == "E"

def test_load_full_config_grid_cell_size():
    result = load_config(FULL_CONFIG)
    # 22in radius → 2*22/12=3.67 → ceil=4
    assert result.config.grid_size_ft == pytest.approx(4.0)

def test_load_full_config_placements():
    result = load_config(FULL_CONFIG)
    assert len(result.config.placements) == 5
    types = [p.element_type for p in result.config.placements]
    assert "yard" in types
    assert "staging" in types
    assert "helix" in types

def test_load_full_config_summary_not_empty():
    result = load_config(FULL_CONFIG)
    assert result.summary != ""
    assert "Ridgeline Division" in result.summary

def test_load_full_config_no_warnings():
    result = load_config(FULL_CONFIG)
    assert result.warnings == []


# ---------------------------------------------------------------------------
# load_config — missing required fields
# ---------------------------------------------------------------------------

def test_load_missing_file():
    result = load_config("/nonexistent/path/layout.yaml")
    assert result.ready is False
    assert any(f == "name" for f, _ in result.missing)
    assert any("not found" in w for w in result.warnings)

def test_load_missing_name(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("scale: HO\nroom: 12x16\n")
    result = load_config(cfg)
    assert result.ready is False
    assert any(f == "name" for f, _ in result.missing)

def test_load_missing_scale(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: Test\nroom: 12x16\n")
    result = load_config(cfg)
    assert result.ready is False
    assert any(f == "scale" for f, _ in result.missing)

def test_load_missing_room(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: Test\nscale: HO\n")
    result = load_config(cfg)
    assert result.ready is False
    assert any(f == "room" for f, _ in result.missing)


# ---------------------------------------------------------------------------
# load_config — defaults
# ---------------------------------------------------------------------------

def test_scale_default_radius_filled(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: N\nroom: 8x12\n")
    result = load_config(cfg)
    assert result.config.curve_radius_in == pytest.approx(9.75)

def test_scale_default_grid_size_filled(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: N\nroom: 8x12\n")
    result = load_config(cfg)
    assert result.config.grid_size_ft == pytest.approx(3.0)

def test_explicit_radius_overrides_default(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: HO\nroom: 12x16\ncurve_radius: 36in\n")
    result = load_config(cfg)
    assert result.config.curve_radius_in == pytest.approx(36.0)

def test_mainline_default_is_single(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: HO\nroom: 12x16\n")
    result = load_config(cfg)
    assert result.config.mainline == "single"


# ---------------------------------------------------------------------------
# load_config — warnings for bad values
# ---------------------------------------------------------------------------

def test_unknown_scale_warns(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: XX\nroom: 12x16\n")
    result = load_config(cfg)
    assert any("unrecognised scale" in w for w in result.warnings)

def test_bad_mainline_warns(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: HO\nroom: 12x16\nmainline: triple\n")
    result = load_config(cfg)
    assert any("mainline" in w for w in result.warnings)
    assert result.config.mainline == "single"

def test_bad_grid_entry_warns(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: HO\nroom: 12x16\ngrid:\n  - A1=badtype=x\n")
    result = load_config(cfg)
    assert any("skipping grid entry" in w for w in result.warnings)
    assert result.config.placements == []


# ---------------------------------------------------------------------------
# benchwork sections — shape parsing
# ---------------------------------------------------------------------------

def _bw_config(tmp_path, sections_yaml: str) -> "ConfigResult":
    """Helper: create a config with inline benchwork sections."""
    cfg = tmp_path / "layout.yaml"
    cfg.write_text(
        f"name: T\nscale: HO\nroom: 12x16\nbenchwork:\n{sections_yaml}\n"
    )
    return load_config(cfg)


def test_benchwork_absent_gives_empty_list(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: HO\nroom: 12x16\n")
    result = load_config(cfg)
    assert result.config.benchwork_sections == []


def test_benchwork_rect_section(tmp_path):
    result = _bw_config(
        tmp_path,
        "  - label: west_shelf\n    level: 1\n    shape: rect\n"
        "    x: 0in\n    y: 0in\n    width: 24in\n    length: 144in\n",
    )
    assert result.warnings == []
    sections = result.config.benchwork_sections
    assert len(sections) == 1
    bw = sections[0]
    assert bw.label == "west_shelf"
    assert bw.level == 1
    assert len(bw.vertices) == 4
    assert bw.vertices[0] == pytest.approx((0.0, 0.0))
    assert bw.vertices[1] == pytest.approx((24.0, 0.0))


def test_benchwork_rect_default_shape(tmp_path):
    # shape: omitted → defaults to rect
    result = _bw_config(
        tmp_path,
        "  - label: south_shelf\n    level: 1\n"
        "    x: 0in\n    y: 0in\n    width: 144in\n    length: 24in\n",
    )
    assert len(result.config.benchwork_sections) == 1


def test_benchwork_rotated_rect_section(tmp_path):
    result = _bw_config(
        tmp_path,
        "  - label: corner_shelf\n    level: 1\n    shape: rotated_rect\n"
        "    cx: 72in\n    cy: 96in\n    width: 24in\n    length: 24in\n    rotation: 45\n",
    )
    bw = result.config.benchwork_sections[0]
    assert len(bw.vertices) == 4
    # All vertices should be near (72, 96) — rotated square center
    for x, y in bw.vertices:
        assert abs(x - 72) < 25
        assert abs(y - 96) < 25


def test_benchwork_polygon_section(tmp_path):
    result = _bw_config(
        tmp_path,
        "  - label: l_shelf\n    level: 1\n    shape: polygon\n"
        "    vertices:\n      - [0, 0]\n      - [48, 0]\n      - [48, 24]\n"
        "      - [24, 24]\n      - [24, 72]\n      - [0, 72]\n",
    )
    bw = result.config.benchwork_sections[0]
    assert len(bw.vertices) == 6
    assert bw.vertices[0] == pytest.approx((0.0, 0.0))


def test_benchwork_arc_section(tmp_path):
    result = _bw_config(
        tmp_path,
        "  - label: curve_shelf\n    level: 1\n    shape: arc\n"
        "    cx: 144in\n    cy: 96in\n    radius: 48in\n    width: 24in\n"
        "    start_angle: 90\n    sweep_angle: 90\n",
    )
    assert result.warnings == []
    bw = result.config.benchwork_sections[0]
    # Arc: 45 steps (90°/2°) + 1 points per ring, two rings → at least 8 vertices
    assert len(bw.vertices) >= 8
    # Outer ring = first half of vertices, all at r_outer=60; inner at r_inner=36
    half = len(bw.vertices) // 2
    outer = bw.vertices[:half]
    for x, y in outer:
        dist = ((x - 144)**2 + (y - 96)**2) ** 0.5
        assert dist == pytest.approx(60.0, abs=0.01)


def test_benchwork_spline_section(tmp_path):
    result = _bw_config(
        tmp_path,
        "  - label: track_strip\n    level: 1\n    shape: spline\n    width: 6in\n"
        "    points:\n      - [0, 48]\n      - [72, 48]\n      - [96, 72]\n      - [96, 144]\n",
    )
    assert result.warnings == []
    bw = result.config.benchwork_sections[0]
    # 4 input points → 4 left + 4 right = 8 vertices in polygon
    assert len(bw.vertices) == 8


def test_benchwork_spline_width_offset(tmp_path):
    # Straight horizontal spline: left edge should be at y+3, right at y-3
    result = _bw_config(
        tmp_path,
        "  - label: h_strip\n    level: 1\n    shape: spline\n    width: 6in\n"
        "    points:\n      - [0, 60]\n      - [120, 60]\n",
    )
    bw = result.config.benchwork_sections[0]
    left_ys = [v[1] for v in bw.vertices[:2]]
    right_ys = [v[1] for v in bw.vertices[2:]]
    assert all(abs(y - 63.0) < 0.1 for y in left_ys)
    assert all(abs(y - 57.0) < 0.1 for y in right_ys)


def test_benchwork_level_default_is_1(tmp_path):
    result = _bw_config(
        tmp_path,
        "  - label: shelf\n    x: 0in\n    y: 0in\n    width: 24in\n    length: 144in\n",
    )
    assert result.config.benchwork_sections[0].level == 1


def test_benchwork_level_explicit(tmp_path):
    result = _bw_config(
        tmp_path,
        "  - label: upper\n    level: 2\n    x: 0in\n    y: 0in\n    width: 24in\n    length: 144in\n",
    )
    assert result.config.benchwork_sections[0].level == 2


def test_benchwork_bad_entry_warns_and_skips(tmp_path):
    result = _bw_config(
        tmp_path,
        "  - label: bad\n    level: 1\n    shape: rect\n    width: 24in\n",  # missing length
    )
    assert any("skipping benchwork" in w for w in result.warnings)
    assert result.config.benchwork_sections == []


def test_benchwork_file_key(tmp_path):
    bw_file = tmp_path / "shelves.yaml"
    bw_file.write_text(
        "- label: west\n  level: 1\n  shape: rect\n"
        "  x: 0in\n  y: 0in\n  width: 24in\n  length: 144in\n"
    )
    cfg = tmp_path / "layout.yaml"
    cfg.write_text(
        f"name: T\nscale: HO\nroom: 12x16\n"
        f"benchwork_file: {bw_file.name}\n"
    )
    result = load_config(cfg)
    assert result.warnings == []
    assert len(result.config.benchwork_sections) == 1
    assert result.config.benchwork_sections[0].label == "west"


def test_benchwork_file_missing_warns(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: HO\nroom: 12x16\nbenchwork_file: no_such.yaml\n")
    result = load_config(cfg)
    assert any("benchwork_file not found" in w for w in result.warnings)
    assert result.config.benchwork_sections == []


def test_benchwork_file_precedence_warns_when_both(tmp_path):
    bw_file = tmp_path / "shelves.yaml"
    bw_file.write_text(
        "- label: west\n  level: 1\n  x: 0in\n  y: 0in\n  width: 24in\n  length: 144in\n"
    )
    cfg = tmp_path / "layout.yaml"
    cfg.write_text(
        f"name: T\nscale: HO\nroom: 12x16\n"
        f"benchwork_file: {bw_file.name}\n"
        f"benchwork:\n  - label: inline\n    level: 1\n"
        f"    x: 0in\n    y: 0in\n    width: 24in\n    length: 144in\n"
    )
    result = load_config(cfg)
    assert any("benchwork_file takes precedence" in w for w in result.warnings)
    # Only the file-sourced section is loaded
    assert len(result.config.benchwork_sections) == 1
    assert result.config.benchwork_sections[0].label == "west"


# ---------------------------------------------------------------------------
# grid — @N level tag
# ---------------------------------------------------------------------------

def test_parse_grid_level_default_is_1():
    p = _parse_grid_entry("A1-B2=yard=single-ended")
    assert p.level == 1


def test_parse_grid_level_at_tag():
    p = _parse_grid_entry("A1-B2@2=yard=upper yard")
    assert p.level == 2
    assert p.col_start == "A"
    assert p.row_start == 1
    assert p.col_end == "B"
    assert p.row_end == 2


def test_parse_grid_level_single_cell():
    p = _parse_grid_entry("C3@3=station=passing siding")
    assert p.level == 3
    assert p.col_start == "C"


# ---------------------------------------------------------------------------
# FloorPlan / floor_plan_file (Hillside Division generic tutorial fixture)
# ---------------------------------------------------------------------------

def test_floor_plan_loads():
    result = load_config(FLOOR_PLAN_CONFIG)
    assert result.config.floor_plan is not None

def test_floor_plan_no_warnings():
    result = load_config(FLOOR_PLAN_CONFIG)
    assert result.warnings == []

def test_floor_plan_config_ready():
    result = load_config(FLOOR_PLAN_CONFIG)
    assert result.ready is True

def test_floor_plan_derives_room_width():
    result = load_config(FLOOR_PLAN_CONFIG)
    assert result.config.room_width_ft == pytest.approx(12.0)

def test_floor_plan_derives_room_depth():
    result = load_config(FLOOR_PLAN_CONFIG)
    assert result.config.room_depth_ft == pytest.approx(16.0)

def test_floor_plan_bounding_box():
    result = load_config(FLOOR_PLAN_CONFIG)
    fp = result.config.floor_plan
    assert fp.total_width_in == pytest.approx(144.0)
    assert fp.total_depth_in == pytest.approx(192.0)

def test_floor_plan_room_count():
    result = load_config(FLOOR_PLAN_CONFIG)
    assert len(result.config.floor_plan.rooms) == 1

def test_floor_plan_room_fields():
    result = load_config(FLOOR_PLAN_CONFIG)
    room = result.config.floor_plan.rooms[0]
    assert room.name == "main"
    assert room.x == pytest.approx(0.0)
    assert room.y == pytest.approx(0.0)
    assert room.width == pytest.approx(144.0)
    assert room.depth == pytest.approx(192.0)

def test_floor_plan_door_count():
    result = load_config(FLOOR_PLAN_CONFIG)
    assert len(result.config.floor_plan.doors) == 1

def test_floor_plan_door_fields():
    result = load_config(FLOOR_PLAN_CONFIG)
    door = result.config.floor_plan.doors[0]
    assert door.room == "main"
    assert door.wall == "east"
    assert door.from_in == pytest.approx(36.0)
    assert door.width_in == pytest.approx(36.0)
    assert door.swing == "inward"
    assert door.clearance_in == pytest.approx(36.0)

def test_floor_plan_partition_count():
    result = load_config(FLOOR_PLAN_CONFIG)
    assert len(result.config.floor_plan.partitions) == 1

def test_floor_plan_partition_fields():
    result = load_config(FLOOR_PLAN_CONFIG)
    p = result.config.floor_plan.partitions[0]
    assert p.label == "alcove_wall"
    assert p.x0 == pytest.approx(48.0)
    assert p.y0 == pytest.approx(156.0)
    assert p.x1 == pytest.approx(48.0)
    assert p.y1 == pytest.approx(192.0)
    assert p.thickness == pytest.approx(4.0)

def test_floor_plan_restricted_count():
    result = load_config(FLOOR_PLAN_CONFIG)
    assert len(result.config.floor_plan.restricted) == 1

def test_floor_plan_restricted_fields():
    result = load_config(FLOOR_PLAN_CONFIG)
    r = result.config.floor_plan.restricted[0]
    assert r.label == "furnace_corner"
    assert r.x == pytest.approx(96.0)
    assert r.y == pytest.approx(0.0)
    assert r.width == pytest.approx(48.0)
    assert r.depth == pytest.approx(36.0)
    assert r.reason == "HVAC unit"

def test_floor_plan_file_missing_warns(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: HO\nfloor_plan_file: no_such_file.yaml\n")
    result = load_config(cfg)
    assert any("floor_plan_file not found" in w for w in result.warnings)


# ---------------------------------------------------------------------------
# Benchwork fixture — Hillside Division (hillside_benchwork.yaml)
# ---------------------------------------------------------------------------

def test_hillside_benchwork_loads_six_sections():
    # 4 L1 wall shelves + 2 L2 upper-deck sections
    result = load_config(FLOOR_PLAN_CONFIG)
    assert len(result.config.benchwork_sections) == 6


def test_hillside_benchwork_no_warnings():
    result = load_config(FLOOR_PLAN_CONFIG)
    assert result.warnings == []


def test_hillside_benchwork_labels():
    result = load_config(FLOOR_PLAN_CONFIG)
    labels = [bw.label for bw in result.config.benchwork_sections]
    assert "west_wall" in labels
    assert "north_wall_west" in labels
    assert "north_wall_east" in labels
    assert "east_wall" in labels


def test_hillside_benchwork_levels():
    result = load_config(FLOOR_PLAN_CONFIG)
    levels = {bw.level for bw in result.config.benchwork_sections}
    assert 1 in levels
    assert 2 in levels


def test_hillside_benchwork_file_path_set():
    result = load_config(FLOOR_PLAN_CONFIG)
    assert result.config.benchwork_file.endswith("hillside_benchwork.yaml")


def test_hillside_benchwork_west_wall_vertices():
    result = load_config(FLOOR_PLAN_CONFIG)
    bw = next(b for b in result.config.benchwork_sections if b.label == "west_wall")
    xs = [v[0] for v in bw.vertices]
    ys = [v[1] for v in bw.vertices]
    assert min(xs) == pytest.approx(0.0)
    assert max(xs) == pytest.approx(24.0)
    assert min(ys) == pytest.approx(0.0)
    assert max(ys) == pytest.approx(156.0)


def test_hillside_benchwork_north_wall_west_vertices():
    result = load_config(FLOOR_PLAN_CONFIG)
    bw = next(b for b in result.config.benchwork_sections if b.label == "north_wall_west")
    xs = [v[0] for v in bw.vertices]
    ys = [v[1] for v in bw.vertices]
    assert min(xs) == pytest.approx(0.0)
    assert max(xs) == pytest.approx(48.0)
    assert min(ys) == pytest.approx(168.0)
    assert max(ys) == pytest.approx(192.0)


def test_hillside_benchwork_north_wall_east_vertices():
    result = load_config(FLOOR_PLAN_CONFIG)
    bw = next(b for b in result.config.benchwork_sections if b.label == "north_wall_east")
    xs = [v[0] for v in bw.vertices]
    ys = [v[1] for v in bw.vertices]
    assert min(xs) == pytest.approx(52.0)
    assert max(xs) == pytest.approx(144.0)
    assert min(ys) == pytest.approx(168.0)
    assert max(ys) == pytest.approx(192.0)


def test_hillside_benchwork_east_wall_vertices():
    result = load_config(FLOOR_PLAN_CONFIG)
    bw = next(b for b in result.config.benchwork_sections if b.label == "east_wall")
    xs = [v[0] for v in bw.vertices]
    ys = [v[1] for v in bw.vertices]
    assert min(xs) == pytest.approx(120.0)
    assert max(xs) == pytest.approx(144.0)
    assert min(ys) == pytest.approx(72.0)
    assert max(ys) == pytest.approx(192.0)
