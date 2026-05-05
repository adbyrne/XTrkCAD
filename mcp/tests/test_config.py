"""Tests for layout config parser."""

from pathlib import Path

import pytest

from xtrkcad_mcp.config import (
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

def test_load_full_config_obstructions():
    result = load_config(FULL_CONFIG)
    assert len(result.config.obstructions) == 2

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
