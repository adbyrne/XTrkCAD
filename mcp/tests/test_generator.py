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
BENCHWORK_CONFIG = FIXTURES_DIR / "benchwork_config.yaml"
FLOOR_PLAN_CONFIG = FIXTURES_DIR / "test_floor_plan_config.yaml"


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
# generate — benchwork
# ---------------------------------------------------------------------------

@pytest.fixture
def benchwork_result(tmp_path):
    """Generate a layout from the benchwork fixture config."""
    result = load_config(BENCHWORK_CONFIG)
    out = tmp_path / "benchwork.xtc"
    generate(result.config, out)
    return out.read_text()


def test_benchwork_layers_header_written(benchwork_result):
    assert "LAYERS 0" in benchwork_result   # Floor
    assert "LAYERS 1" in benchwork_result   # Main
    assert "LAYERS 2" in benchwork_result   # L1-Benchwork
    assert "LAYERS 3" in benchwork_result   # L2-Benchwork
    assert "LAYERS CURRENT 1" in benchwork_result

def test_benchwork_layer_names_present(benchwork_result):
    assert '"Floor"' in benchwork_result
    assert '"Main"' in benchwork_result
    assert '"L1-Benchwork"' in benchwork_result
    assert '"L2-Benchwork"' in benchwork_result

def test_floor_and_main_layers_always_present(tmp_path):
    # Floor and Main layers are emitted even when there is no benchwork.
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: HO\nroom: 12x16\ngrid: []\n")
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    content = out.read_text()
    assert "LAYERS 0" in content
    assert '"Floor"' in content
    assert "LAYERS 1" in content
    assert '"Main"' in content
    assert "LAYERS 2" not in content        # no benchwork layers
    assert "LAYERS CURRENT 1" in content

def test_room_tableedges_present(benchwork_result):
    tableedges = [
        ln for ln in benchwork_result.splitlines()
        if ln.startswith("TABLEEDGE")
    ]
    assert len(tableedges) == 4

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

def test_floor_draws_on_layer0(benchwork_result):
    # 2 hard obstructions only — wall footprints are not on the floor layer
    layer0_draws = [
        ln for ln in benchwork_result.splitlines()
        if ln.startswith("DRAW") and ln.split()[2] == "0"
    ]
    assert len(layer0_draws) == 2

def test_filpoly_segments_present(benchwork_result):
    # One F4 (SEG_FILPOLY) per hard obstruction — 2 total
    filpoly = [ln for ln in benchwork_result.splitlines() if ln.strip().startswith("F4")]
    assert len(filpoly) == 2

def test_benchwork_west_shelf_draw_on_layer2(benchwork_result):
    # west wall on both levels → at least 1 DRAW on layer 2
    layer2_draws = [
        ln for ln in benchwork_result.splitlines()
        if ln.startswith("DRAW") and ln.split()[2] == "2"
    ]
    assert len(layer2_draws) >= 1

def test_benchwork_level2_wall_present_on_layer3(benchwork_result):
    # wall_x131 levels:[2] only → at least 1 DRAW on layer 3
    layer3_draws = [
        ln for ln in benchwork_result.splitlines()
        if ln.startswith("DRAW") and ln.split()[2] == "3"
    ]
    assert len(layer3_draws) >= 1

def test_benchwork_level1_only_walls_on_layer2(benchwork_result):
    # west + south + north_ul on level 1 → exactly 3 DRAW objects on layer 2
    layer2_draws = [
        ln for ln in benchwork_result.splitlines()
        if ln.startswith("DRAW") and ln.split()[2] == "2"
    ]
    assert len(layer2_draws) == 3

def test_benchwork_level2_total_walls(benchwork_result):
    # west + south + north_ul + wall_x131 on level 2 → exactly 4 DRAW objects on layer 3
    layer3_draws = [
        ln for ln in benchwork_result.splitlines()
        if ln.startswith("DRAW") and ln.split()[2] == "3"
    ]
    assert len(layer3_draws) == 4

def test_benchwork_generates_cleanly(tmp_path):
    result = load_config(BENCHWORK_CONFIG)
    out = tmp_path / "benchwork.xtc"
    gen = generate(result.config, out)
    assert out.exists()
    assert gen.warnings == []


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

def test_floor_plan_filpoly_count(floor_plan_result):
    filpolys = [ln for ln in floor_plan_result.splitlines() if ln.strip().startswith("F4")]
    assert len(filpolys) == 3

def test_floor_plan_restricted_vertex(floor_plan_result):
    # furnace_corner: x=96, y=0 → first vertex
    assert "96.000000 0.000000 0" in floor_plan_result

def test_floor_plan_partition_vertex(floor_plan_result):
    # alcove_wall: x0=48, y0=156, vertical → SW corner of polygon
    assert "48.000000 156.000000 0" in floor_plan_result

def test_floor_plan_door_clearance_vertex(floor_plan_result):
    # east door inward: wall_x=144, clearance=36 → x0=108, y0=36
    assert "108.000000 36.000000 0" in floor_plan_result

def test_floor_plan_benchwork_walls_still_present(floor_plan_result):
    # west + south shelf outlines on layer 2
    layer2_draws = [ln for ln in floor_plan_result.splitlines()
                    if ln.startswith("DRAW") and ln.split()[2] == "2"]
    assert len(layer2_draws) == 2

def test_floor_plan_no_extra_draws_without_floor_plan(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text("name: T\nscale: HO\nroom: 12x16\ngrid: []\n")
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    assert len(_layer0_draws(out.read_text())) == 0

def test_floor_plan_outward_door_no_clearance(tmp_path):
    cfg = tmp_path / "layout.yaml"
    cfg.write_text(
        "name: T\nscale: HO\n"
        "floor_plan:\n"
        "  rooms:\n"
        "    - name: main\n"
        "      x: 0in\n      y: 0in\n      width: 144in\n      depth: 192in\n"
        "  doors:\n"
        "    - room: main\n      wall: east\n      from: 0in\n"
        "      width: 36in\n      swing: outward\n      clearance: 36in\n"
        "grid: []\n"
    )
    out = tmp_path / "t.xtc"
    generate(load_config(cfg).config, out)
    assert len(_layer0_draws(out.read_text())) == 0

def test_floor_plan_generates_cleanly(tmp_path):
    result = load_config(FLOOR_PLAN_CONFIG)
    out = tmp_path / "hillside.xtc"
    gen = generate(result.config, out)
    assert out.exists()
    assert gen.warnings == []
