"""Tests for the layout generator (Phase D)."""

import pytest
from pathlib import Path

from xtrkcad_mcp.config import load_config
from xtrkcad_mcp.generator import (
    GenerationResult,
    _col_to_x_in,
    _find_template_id,
    _in_bounds,
    _range_origin,
    _scale_factor,
    _version_backup,
    default_output_path,
    generate,
)
from xtrkcad_mcp.parser import parse_file
from xtrkcad_mcp.templates import load_library

FIXTURES_DIR = Path(__file__).parent / "fixtures"
FULL_CONFIG = FIXTURES_DIR / "test_layout_config.yaml"
FLOOR_PLAN_CONFIG = FIXTURES_DIR / "hillside_division.yaml"


# ---------------------------------------------------------------------------
# Coordinate helpers
# ---------------------------------------------------------------------------

def test_col_to_x_a():
    assert _col_to_x_in("A", 48.0) == pytest.approx(0.0)

def test_col_to_x_b():
    assert _col_to_x_in("B", 48.0) == pytest.approx(48.0)

def test_col_to_x_c():
    assert _col_to_x_in("C", 48.0) == pytest.approx(96.0)

def test_col_to_x_case_insensitive():
    assert _col_to_x_in("a", 48.0) == _col_to_x_in("A", 48.0)


def test_range_origin_row1_is_top(tmp_path):
    # Row 1 origin y should equal room height (top of room)
    from xtrkcad_mcp.config import GridPlacement
    pl = GridPlacement("A", 1, "A", 1, "yard", "", "A1=yard")
    x, y = _range_origin(pl, 48.0, 192.0)
    assert x == pytest.approx(0.0)
    assert y == pytest.approx(192.0)   # top of 16ft room

def test_range_origin_row3():
    from xtrkcad_mcp.config import GridPlacement
    pl = GridPlacement("A", 3, "A", 3, "yard", "", "A3=yard")
    x, y = _range_origin(pl, 48.0, 192.0)
    assert y == pytest.approx(192.0 - 2 * 48.0)   # 96.0

def test_range_origin_col_c():
    from xtrkcad_mcp.config import GridPlacement
    pl = GridPlacement("C", 1, "C", 1, "yard", "", "C1=yard")
    x, y = _range_origin(pl, 48.0, 192.0)
    assert x == pytest.approx(96.0)


def test_scale_factor_ho():
    assert _scale_factor("HO") == pytest.approx(1.0)

def test_scale_factor_n():
    # 87.1 / 160 ≈ 0.544
    assert _scale_factor("N") == pytest.approx(87.1 / 160.0)

def test_scale_factor_o():
    assert _scale_factor("O") == pytest.approx(87.1 / 48.0)

def test_scale_factor_unknown():
    assert _scale_factor("XX") == pytest.approx(1.0)


# ---------------------------------------------------------------------------
# Bounds checking
# ---------------------------------------------------------------------------

def test_in_bounds_simple():
    from xtrkcad_mcp.config import GridPlacement
    # A1 single cell in 12×16 ft room, 4ft cells
    pl = GridPlacement("A", 1, "A", 1, "yard", "", "")
    assert _in_bounds(pl, 48.0, 144.0, 192.0) is True

def test_out_of_bounds_column():
    from xtrkcad_mcp.config import GridPlacement
    # Column D = x_end = 4*48 = 192 > 144 (12ft room)
    pl = GridPlacement("D", 1, "D", 1, "yard", "", "")
    assert _in_bounds(pl, 48.0, 144.0, 192.0) is False

def test_out_of_bounds_row():
    from xtrkcad_mcp.config import GridPlacement
    # Row 5 bottom = 192 - 5*48 = -48 < 0
    pl = GridPlacement("A", 5, "A", 5, "yard", "", "")
    assert _in_bounds(pl, 48.0, 144.0, 192.0) is False


# ---------------------------------------------------------------------------
# Template resolution
# ---------------------------------------------------------------------------

def test_find_template_single_ended():
    from xtrkcad_mcp.config import GridPlacement
    lib = load_library()
    pl = GridPlacement("A", 1, "A", 1, "yard", "single-ended, 3 tracks", "")
    assert _find_template_id(pl, lib) == "yard/single-ended"

def test_find_template_double_ended():
    from xtrkcad_mcp.config import GridPlacement
    lib = load_library()
    pl = GridPlacement("A", 1, "A", 1, "yard", "double-ended, 4 tracks", "")
    assert _find_template_id(pl, lib) == "yard/double-ended"

def test_find_template_no_params_fallback():
    from xtrkcad_mcp.config import GridPlacement
    lib = load_library()
    pl = GridPlacement("B", 1, "B", 8, "mainline", "", "")
    tid = _find_template_id(pl, lib)
    assert tid is not None
    assert tid.startswith("mainline/")

def test_find_template_passing_siding_hyphen_space():
    from xtrkcad_mcp.config import GridPlacement
    lib = load_library()
    # params uses "passing siding" (space), template id uses "passing-siding" (hyphen)
    pl = GridPlacement("E", 4, "E", 4, "station", "passing siding, 100ft", "")
    assert _find_template_id(pl, lib) == "station/passing-siding"

def test_find_template_unknown_type():
    from xtrkcad_mcp.config import GridPlacement
    lib = load_library()
    pl = GridPlacement("A", 1, "A", 1, "turntable", "", "")
    assert _find_template_id(pl, lib) is None


# ---------------------------------------------------------------------------
# Version backup
# ---------------------------------------------------------------------------

def test_version_backup_no_existing(tmp_path):
    path = tmp_path / "layout.xtc"
    assert _version_backup(path) is None

def test_version_backup_creates_v1(tmp_path):
    path = tmp_path / "layout.xtc"
    path.write_text("original")
    backup = _version_backup(path)
    assert backup is not None
    assert backup.name == "layout_v1.xtc"
    assert backup.read_text() == "original"

def test_version_backup_increments(tmp_path):
    path = tmp_path / "layout.xtc"
    path.write_text("v0")
    (tmp_path / "layout_v1.xtc").write_text("v1")
    backup = _version_backup(path)
    assert backup.name == "layout_v2.xtc"


# ---------------------------------------------------------------------------
# default_output_path
# ---------------------------------------------------------------------------

def test_default_output_path_simple(tmp_path):
    from xtrkcad_mcp.config import LayoutConfig
    config = LayoutConfig(name="My Layout")
    cfg_path = tmp_path / "layout.yaml"
    out = default_output_path(config, cfg_path)
    assert out == tmp_path / "my_layout.xtc"

def test_default_output_path_spaces(tmp_path):
    from xtrkcad_mcp.config import LayoutConfig
    config = LayoutConfig(name="Ridgeline Division")
    cfg_path = tmp_path / "layout.yaml"
    out = default_output_path(config, cfg_path)
    assert out.name == "ridgeline_division.xtc"


# ---------------------------------------------------------------------------
# generate — basic output
# ---------------------------------------------------------------------------

@pytest.fixture
def simple_config(tmp_path):
    """Minimal config with two placements, all within a 12×16 ft room."""
    cfg = tmp_path / "layout.yaml"
    cfg.write_text(
        "name: Test Layout\n"
        "scale: HO\n"
        "room: 12x16\n"
        "curve_radius: 22in\n"
        "grid:\n"
        "  - A1=yard=single-ended, 2 tracks, 40ft\n"
        "  - A3=staging=single-ended, 3 tracks, 60ft\n"
    )
    return cfg


def test_generate_creates_file(simple_config, tmp_path):
    result = load_config(simple_config)
    out = tmp_path / "test_layout.xtc"
    gen = generate(result.config, out)
    assert out.exists()
    assert gen.output_path == out


def test_generate_no_backup_when_no_existing(simple_config, tmp_path):
    result = load_config(simple_config)
    out = tmp_path / "test_layout.xtc"
    gen = generate(result.config, out)
    assert gen.backup_path is None


def test_generate_backup_on_overwrite(simple_config, tmp_path):
    result = load_config(simple_config)
    out = tmp_path / "test_layout.xtc"
    out.write_text("old content")
    gen = generate(result.config, out)
    assert gen.backup_path is not None
    assert gen.backup_path.read_text() == "old content"


def test_generate_output_parseable(simple_config, tmp_path):
    result = load_config(simple_config)
    out = tmp_path / "test_layout.xtc"
    generate(result.config, out)
    layout = parse_file(out)
    assert layout.scale == "HO"
    assert layout.title1 == "Test Layout"


def test_generate_roomsize_correct(simple_config, tmp_path):
    result = load_config(simple_config)
    out = tmp_path / "test_layout.xtc"
    generate(result.config, out)
    layout = parse_file(out)
    assert layout.room_width == pytest.approx(144.0)   # 12 ft
    assert layout.room_height == pytest.approx(192.0)  # 16 ft


def test_generate_places_two_templates(simple_config, tmp_path):
    result = load_config(simple_config)
    out = tmp_path / "test_layout.xtc"
    gen = generate(result.config, out)
    assert len(gen.placed) == 2


def test_generate_output_has_tracks(simple_config, tmp_path):
    result = load_config(simple_config)
    out = tmp_path / "test_layout.xtc"
    generate(result.config, out)
    layout = parse_file(out)
    assert len(layout.tracks) >= 2


def test_generate_track_ids_are_positive(simple_config, tmp_path):
    # TABLEEDGEs (room boundary) occupy IDs 1-4; placed template tracks start at 5+.
    result = load_config(simple_config)
    out = tmp_path / "test_layout.xtc"
    gen = generate(result.config, out)
    all_ids = [tid for p in gen.placed for tid in p.track_ids]
    assert all(tid > 0 for tid in all_ids)


def test_generate_track_ids_sequential(simple_config, tmp_path):
    # Placed template track IDs must be consecutive (no gaps), though they no
    # longer start at 1 — room TABLEEDGE objects consume the first 4 IDs.
    result = load_config(simple_config)
    out = tmp_path / "test_layout.xtc"
    gen = generate(result.config, out)
    all_ids = sorted(tid for p in gen.placed for tid in p.track_ids)
    assert all_ids == list(range(all_ids[0], all_ids[0] + len(all_ids)))


# ---------------------------------------------------------------------------
# generate — coordinate placement
# ---------------------------------------------------------------------------

def test_generate_a1_origin_is_top_left(simple_config, tmp_path):
    """A1 in a 4ft grid, 16ft room → origin at (0, 192)."""
    result = load_config(simple_config)
    out = tmp_path / "test_layout.xtc"
    gen = generate(result.config, out)
    a1 = next(p for p in gen.placed if p.grid_range == "A1")
    assert a1.x == pytest.approx(0.0)
    assert a1.y == pytest.approx(192.0)   # top of 16ft room


def test_generate_a3_origin(simple_config, tmp_path):
    """A3 in 4ft grid → x=0, y=192-2*48=96."""
    result = load_config(simple_config)
    out = tmp_path / "test_layout.xtc"
    gen = generate(result.config, out)
    a3 = next(p for p in gen.placed if p.grid_range == "A3")
    assert a3.x == pytest.approx(0.0)
    assert a3.y == pytest.approx(96.0)


def test_generate_endpoints_translated(simple_config, tmp_path):
    """Parsed track endpoints should reflect the translated origin."""
    result = load_config(simple_config)
    out = tmp_path / "test_layout.xtc"
    gen = generate(result.config, out)
    layout = parse_file(out)
    # A1 origin (0, 192); stub E1 at (0, 0) → translated to (0, 192)
    a1_track = layout.tracks[0]
    ep0 = a1_track.endpoints[0]
    assert ep0.x == pytest.approx(0.0)
    assert ep0.y == pytest.approx(192.0)


# ---------------------------------------------------------------------------
# generate — scale factor
# ---------------------------------------------------------------------------

def test_generate_n_scale_coords_smaller(tmp_path):
    """N-scale layout should have smaller track coords than HO for same grid."""
    cfg_ho = tmp_path / "ho.yaml"
    cfg_n = tmp_path / "n.yaml"
    cfg_ho.write_text("name: HO Test\nscale: HO\nroom: 12x16\ngrid:\n  - A1=yard=single-ended\n")
    cfg_n.write_text("name: N Test\nscale: N\nroom: 12x16\ngrid:\n  - A1=yard=single-ended\n")

    out_ho = tmp_path / "ho.xtc"
    out_n = tmp_path / "n.xtc"
    gen_ho = generate(load_config(cfg_ho).config, out_ho)
    gen_n = generate(load_config(cfg_n).config, out_n)

    layout_ho = parse_file(out_ho)
    layout_n = parse_file(out_n)

    # A1 origin is the same in real inches (same room), but template tracks
    # in N are scaled down vs HO
    ho_span = abs(layout_ho.tracks[0].endpoints[1].x - layout_ho.tracks[0].endpoints[0].x)
    n_span = abs(layout_n.tracks[0].endpoints[1].x - layout_n.tracks[0].endpoints[0].x)
    assert n_span < ho_span   # N track is physically shorter on same layout


# ---------------------------------------------------------------------------
# generate — warnings for out-of-bounds
# ---------------------------------------------------------------------------

def test_generate_warns_out_of_bounds(tmp_path):
    cfg = tmp_path / "layout.yaml"
    # G2 = column G (7th) = 6*48=288 in, room is 12ft=144 in → outside
    cfg.write_text(
        "name: OOB Test\nscale: HO\nroom: 12x16\ngrid:\n  - G2=yard=single-ended\n"
    )
    result = load_config(cfg)
    out = tmp_path / "oob.xtc"
    gen = generate(result.config, out)
    assert any("outside room" in w for w in gen.warnings)


def test_generate_out_of_bounds_still_placed(tmp_path):
    """Out-of-bounds placements are warned but not skipped."""
    cfg = tmp_path / "layout.yaml"
    cfg.write_text(
        "name: OOB\nscale: HO\nroom: 12x16\ngrid:\n  - G2=yard=single-ended\n"
    )
    result = load_config(cfg)
    out = tmp_path / "oob.xtc"
    gen = generate(result.config, out)
    assert len(gen.placed) == 1


# ---------------------------------------------------------------------------
# generate — unknown template type is skipped
# ---------------------------------------------------------------------------

def test_generate_unknown_element_type_skipped(tmp_path):
    cfg = tmp_path / "layout.yaml"
    # "turntable" is not in VALID_ELEMENT_TYPES so load_config warns; generator skips
    cfg.write_text(
        "name: T\nscale: HO\nroom: 12x16\ngrid:\n  - A1=yard=single-ended\n"
    )
    result = load_config(cfg)
    out = tmp_path / "t.xtc"
    gen = generate(result.config, out)
    assert gen.skipped == []   # single-ended yard should resolve fine


# ---------------------------------------------------------------------------
# generate — layer scheme
# ---------------------------------------------------------------------------

def test_layer_scheme_floor_always_present(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: HO\nroom: 12x16\ngrid: []\n")
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    content = out.read_text()
    assert "LAYERS 0" in content
    assert '"Floor"' in content
    assert "LAYERS CURRENT 1" in content


def test_layer_scheme_level1_always_emitted(tmp_path):
    # 6 track types (1-6) + L1-Benchwork (7) always emitted for a 1-level config
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: HO\nroom: 12x16\ngrid: []\n")
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    content = out.read_text()
    for name in ["L1-Main", "L1-Passing", "L1-Storage", "L1-Staging", "L1-Connecting", "L1-Service"]:
        assert f'"{name}"' in content
    assert '"L1-Benchwork"' in content
    assert "LAYERS 7" in content   # L1-Benchwork


def test_layer_scheme_two_levels(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: HO\nroom: 12x16\nlevels: 2\ngrid: []\n")
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    content = out.read_text()
    for name in ["L1-Main", "L1-Benchwork", "L2-Main", "L2-Benchwork"]:
        assert f'"{name}"' in content
    assert "LAYERS 7" in content    # L1-Benchwork
    assert "LAYERS 8" in content    # L2-Main
    assert "LAYERS 14" in content   # L2-Benchwork


def test_layer_scheme_track_colors(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: HO\nroom: 12x16\nlevels: 2\ngrid: []\n")
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    content = out.read_text()
    # L1-Main (layer 1) = black (0), L2-Main (layer 8) = dark-red (11534336)
    assert "LAYERS 1 1 0 1 0 " in content
    assert "LAYERS 8 1 0 1 11534336 " in content


def test_layer_scheme_benchwork_colors(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: HO\nroom: 12x16\nlevels: 2\ngrid: []\n")
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    content = out.read_text()
    # L1-Benchwork (layer 7) = light green (9498256), L2-Benchwork (layer 14) = light blue (11393254)
    assert "LAYERS 7 1 0 1 9498256 " in content
    assert "LAYERS 14 1 0 1 11393254 " in content


def test_track_types_filters_layers(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text(
        "name: T\nscale: HO\nroom: 12x16\ngrid: []\n"
        "track_types: [Main, Storage]\n"
    )
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    content = out.read_text()
    assert '"L1-Main"' in content
    assert '"L1-Storage"' in content
    assert '"L1-Passing"' not in content
    assert '"L1-Staging"' not in content
    assert '"L1-Connecting"' not in content
    assert '"L1-Service"' not in content
    assert '"L1-Benchwork"' in content   # benchwork always emitted


def test_track_types_default_emits_all_six(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: HO\nroom: 12x16\ngrid: []\n")
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    content = out.read_text()
    for name in ["L1-Main", "L1-Passing", "L1-Storage", "L1-Staging", "L1-Connecting", "L1-Service"]:
        assert f'"{name}"' in content


def test_distinct_track_colors_assigns_different_colors(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text(
        "name: T\nscale: HO\nroom: 12x16\ngrid: []\n"
        "distinct_track_colors: true\n"
    )
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    content = out.read_text()
    # L1-Main = black (0), L1-Passing = teal (32896) — must differ
    assert "LAYERS 1 1 0 1 0 " in content        # Main: black
    assert "LAYERS 2 1 0 1 32896 " in content    # Passing: teal


def test_distinct_track_colors_default_false_uses_level_color(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: HO\nroom: 12x16\ngrid: []\n")
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    content = out.read_text()
    # All L1 track types should have the same color (black = 0)
    for layer_id in range(1, 7):
        assert f"LAYERS {layer_id} 1 0 1 0 " in content


def test_track_types_invalid_entry_warns(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text(
        "name: T\nscale: HO\nroom: 12x16\ngrid: []\n"
        "track_types: [Main, NotAType]\n"
    )
    result = load_config(cfg)
    assert any("unknown track_types" in w for w in result.warnings)
    assert result.config.track_types == ["Main"]


def test_room_tableedges_always_present(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: HO\nroom: 12x16\ngrid: []\n")
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    tableedges = [
        ln for ln in out.read_text().splitlines()
        if ln.startswith("TABLEEDGE")
    ]
    assert len(tableedges) == 4


# ---------------------------------------------------------------------------
# generate — benchwork sections
# ---------------------------------------------------------------------------

def _bw_xtc(tmp_path, sections_yaml: str, levels: int = 1) -> str:
    cfg = tmp_path / "layout.yaml"
    cfg.write_text(
        f"name: T\nscale: HO\nroom: 12x16\nlevels: {levels}\n"
        f"benchwork:\n{sections_yaml}\ngrid: []\n"
    )
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    return out.read_text()


def test_benchwork_section_draws_on_benchwork_layer(tmp_path):
    content = _bw_xtc(
        tmp_path,
        "  - label: west\n    level: 1\n    shape: rect\n"
        "    x: 0in\n    y: 0in\n    width: 24in\n    length: 144in\n",
    )
    # L1-Benchwork = layer 7
    layer7_draws = [ln for ln in content.splitlines()
                    if ln.startswith("DRAW") and ln.split()[2] == "7"]
    assert len(layer7_draws) == 1


def test_benchwork_section_level2_on_layer14(tmp_path):
    content = _bw_xtc(
        tmp_path,
        "  - label: upper\n    level: 2\n    shape: rect\n"
        "    x: 0in\n    y: 0in\n    width: 24in\n    length: 144in\n",
        levels=2,
    )
    # L2-Benchwork = layer 14
    layer14_draws = [ln for ln in content.splitlines()
                     if ln.startswith("DRAW") and ln.split()[2] == "14"]
    assert len(layer14_draws) == 1


def test_benchwork_section_uses_benchwork_color(tmp_path):
    content = _bw_xtc(
        tmp_path,
        "  - label: shelf\n    level: 1\n    shape: rect\n"
        "    x: 0in\n    y: 0in\n    width: 24in\n    length: 144in\n",
    )
    # L1-Benchwork color = light green (9498256)
    assert "F4 9498256" in content


def test_benchwork_section_filpoly_vertex_count(tmp_path):
    content = _bw_xtc(
        tmp_path,
        "  - label: shelf\n    level: 1\n    shape: rect\n"
        "    x: 0in\n    y: 0in\n    width: 24in\n    length: 144in\n",
    )
    # rect → 4 vertices; L1-Benchwork color = light green (9498256)
    assert "F4 9498256 0.000000 4 0" in content


def test_benchwork_arc_section_generates(tmp_path):
    content = _bw_xtc(
        tmp_path,
        "  - label: arc\n    level: 1\n    shape: arc\n"
        "    cx: 144in\n    cy: 96in\n    radius: 48in\n    width: 24in\n"
        "    start_angle: 0\n    sweep_angle: 90\n",
    )
    filpolys = [ln for ln in content.splitlines() if ln.strip().startswith("F4")]
    assert len(filpolys) == 1
    # 90°/2° = 45 steps → 46 points per ring → 92 total vertices
    assert "F4 9498256 0.000000 92 0" in content


def test_benchwork_spline_section_generates(tmp_path):
    content = _bw_xtc(
        tmp_path,
        "  - label: strip\n    level: 1\n    shape: spline\n    width: 6in\n"
        "    points:\n      - [0, 48]\n      - [72, 48]\n      - [96, 72]\n",
    )
    filpolys = [ln for ln in content.splitlines() if ln.strip().startswith("F4")]
    assert len(filpolys) == 1
    # 3 points → 3+3 = 6 vertices
    assert "F4 9498256 0.000000 6 0" in content


def test_benchwork_sections_no_layer0_draws(tmp_path):
    content = _bw_xtc(
        tmp_path,
        "  - label: shelf\n    level: 1\n    shape: rect\n"
        "    x: 0in\n    y: 0in\n    width: 24in\n    length: 144in\n",
    )
    layer0 = [ln for ln in content.splitlines()
               if ln.startswith("DRAW") and ln.split()[2] == "0"]
    assert len(layer0) == 0


def test_benchwork_sections_generates_cleanly(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text(
        "name: T\nscale: HO\nroom: 12x16\nlevels: 2\n"
        "benchwork:\n"
        "  - label: west\n    level: 1\n    shape: rect\n"
        "    x: 0in\n    y: 0in\n    width: 24in\n    length: 144in\n"
        "  - label: upper\n    level: 2\n    shape: rotated_rect\n"
        "    cx: 72in\n    cy: 96in\n    width: 24in\n    length: 48in\n    rotation: 45\n"
        "grid: []\n"
    )
    out = tmp_path / "t.xtc"
    gen = generate(load_config(cfg).config, out)
    assert out.exists()
    assert gen.warnings == []


def test_grid_level_tag_selects_track_layer(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text(
        "name: T\nscale: HO\nroom: 12x16\nlevels: 2\n"
        "grid:\n  - A1@2=yard=single-ended\n"
    )
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    content = out.read_text()
    # L2-Main = layer 8; template track headers should use layer 8
    track_lines = [
        ln for ln in content.splitlines()
        if ln.startswith(("STRAIGHT", "CURVE", "JOINT")) and ln.split()[2] == "8"
    ]
    assert len(track_lines) >= 1


# ---------------------------------------------------------------------------
# generate — floor plan rendering (Hillside Division fixture)
# ---------------------------------------------------------------------------

@pytest.fixture
def floor_plan_result(tmp_path):
    """Generate from the Hillside Division floor-plan config."""
    result = load_config(FLOOR_PLAN_CONFIG)
    out = tmp_path / "hillside.xtc"
    generate(result.config, out)
    return out.read_text()


def _layer0_draws(content: str) -> list[str]:
    return [ln for ln in content.splitlines() if ln.startswith("DRAW") and ln.split()[2] == "0"]


def test_floor_plan_layer0_draw_count(floor_plan_result):
    # 1 restricted + 1 partition + 1 inward door clearance
    assert len(_layer0_draws(floor_plan_result)) == 3

def _layer0_filpolys(content: str) -> list[str]:
    """Return F4 lines that belong to layer-0 DRAW blocks (floor plan features)."""
    result = []
    in_layer0 = False
    for ln in content.splitlines():
        if ln.startswith("DRAW"):
            in_layer0 = ln.split()[2] == "0"
        elif ln.startswith("END"):
            in_layer0 = False
        elif in_layer0 and ln.strip().startswith("F4"):
            result.append(ln)
    return result


def test_floor_plan_filpoly_count(floor_plan_result):
    # 1 restricted + 1 partition + 1 inward door clearance = 3 layer-0 F4 polys
    assert len(_layer0_filpolys(floor_plan_result)) == 3

def test_floor_plan_restricted_vertex(floor_plan_result):
    # furnace_corner: x=96, y=0 → first vertex
    assert "96.000000 0.000000 0" in floor_plan_result

def test_floor_plan_partition_vertex(floor_plan_result):
    # alcove_wall: x0=48, y0=156, vertical → SW corner of polygon
    assert "48.000000 156.000000 0" in floor_plan_result

def test_floor_plan_door_clearance_vertex(floor_plan_result):
    # east door inward: wall_x=144, clearance=36 → x0=108, y0=36
    assert "108.000000 36.000000 0" in floor_plan_result

def test_floor_plan_door_clearance_light_color(floor_plan_result):
    # door clearance uses RESTRICTION_COLOR (12632256), not PARTITION_COLOR
    assert "F4 12632256" in floor_plan_result

def test_floor_plan_swing_none_no_clearance(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text(
        "name: T\nscale: HO\n"
        "floor_plan:\n"
        "  rooms:\n"
        "    - name: main\n"
        "      x: 0in\n      y: 0in\n      width: 144in\n      depth: 192in\n"
        "  doors:\n"
        "    - room: main\n      wall: east\n      from: 0in\n"
        "      width: 36in\n      swing: none\n      clearance: 36in\n"
        "grid: []\n"
    )
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    assert len(_layer0_draws(out.read_text())) == 0

def test_floor_plan_no_extra_draws_without_floor_plan(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: HO\nroom: 12x16\ngrid: []\n")
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    assert len(_layer0_draws(out.read_text())) == 0

def test_floor_plan_generates_cleanly(tmp_path):
    result = load_config(FLOOR_PLAN_CONFIG)
    out = tmp_path / "hillside.xtc"
    gen = generate(result.config, out)
    assert out.exists()
    assert gen.warnings == []

def test_floor_plan_restricted_uses_light_color(floor_plan_result):
    # Restricted zones should use RESTRICTION_COLOR (12632256 = light gray)
    assert "F4 12632256" in floor_plan_result

def test_floor_plan_partition_uses_medium_color(floor_plan_result):
    # Partitions should use PARTITION_COLOR (8421504 = medium gray)
    assert "F4 8421504" in floor_plan_result

def test_floor_plan_polygon_restricted(tmp_path):
    """Triangle restricted zone renders with correct vertex count."""
    cfg = tmp_path / "layout.yaml"
    cfg.write_text(
        "name: T\nscale: HO\n"
        "floor_plan:\n"
        "  rooms:\n"
        "    - name: main\n"
        "      x: 0in\n      y: 0in\n      width: 144in\n      depth: 192in\n"
        "  restricted:\n"
        "    - label: triangle_zone\n"
        "      vertices:\n"
        "        - [163, 176]\n"
        "        - [214, 176]\n"
        "        - [214, 100]\n"
        "      reason: access\n"
        "grid: []\n"
    )
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    content = out.read_text()
    # Triangle has 3 vertices → F4 ... 3 0
    assert "F4 12632256 0.000000 3 0" in content
    assert "163.000000 176.000000 0" in content

def test_floor_plan_partition_splits_around_door(tmp_path):
    """Partition adjacent to a door opening splits into 2 DRAW segments."""
    cfg = tmp_path / "layout.yaml"
    cfg.write_text(
        "name: T\nscale: HO\n"
        "floor_plan:\n"
        "  rooms:\n"
        "    - name: left\n"
        "      x: 0in\n      y: 0in\n      width: 48in\n      depth: 96in\n"
        "  doors:\n"
        "    - room: left\n      wall: east\n      from: 24in\n"
        "      width: 32in\n      swing: none\n      clearance: 32in\n"
        "  partitions:\n"
        "    - label: center_wall\n"
        "      x0: 48in\n      y0: 0in\n      x1: 48in\n      y1: 96in\n"
        "      thickness: 6in\n"
        "grid: []\n"
    )
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    layer0 = _layer0_draws(out.read_text())
    # One partition → split into 2 segments around the 32in door opening
    assert len(layer0) == 2
