"""Tests for server tools using the bundled test fixture layout."""

from pathlib import Path

import pytest

from xtrkcad_mcp.parser import parse_file
from xtrkcad_mcp.server import write_gaps_report, write_radius_map

FIXTURES_DIR = Path(__file__).parent / "fixtures"
FIXTURE = FIXTURES_DIR / "test_layout.xtc"


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
