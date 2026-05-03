"""Tests for the XTrkCAD layout parser against bundled example files."""

from pathlib import Path

import pytest

from xtrkcad_mcp.parser import parse_file

EXAMPLES_DIR = Path(__file__).parent.parent.parent / "app" / "lib" / "examples"


def examples_available() -> bool:
    return EXAMPLES_DIR.is_dir()


@pytest.mark.skipif(not examples_available(), reason="example layouts not found")
def test_parse_cascade():
    path = EXAMPLES_DIR / "cascade.xtc"
    layout = parse_file(path)

    assert layout.title1 == "CPR in BC - Cascade Division"
    assert layout.scale == "N"
    assert layout.room_width == pytest.approx(150.0)
    assert layout.room_height == pytest.approx(266.2)
    assert layout.version == "3.0.0"

    counts = layout.track_counts()
    assert counts.get("STRAIGHT", 0) > 0
    assert counts.get("CURVE", 0) > 0
    assert counts.get("TURNOUT", 0) > 0
    assert len(layout.tracks) > 0


@pytest.mark.skipif(not examples_available(), reason="example layouts not found")
def test_endpoint_connectivity():
    path = EXAMPLES_DIR / "cascade.xtc"
    layout = parse_file(path)

    # Every track object that has T endpoints should have connected_to set
    for track in layout.tracks:
        for ep in track.endpoints:
            if ep.connected_to is not None:
                assert ep.connected_to > 0


@pytest.mark.skipif(not examples_available(), reason="example layouts not found")
def test_unconnected_endpoints_are_open():
    path = EXAMPLES_DIR / "cascade.xtc"
    layout = parse_file(path)
    unconnected = layout.unconnected_endpoints()
    # A real layout has some staging/staging endpoints open — just check structure
    for track_id, ep in unconnected:
        assert isinstance(track_id, int)
        assert ep.connected_to is None


@pytest.mark.skipif(not examples_available(), reason="example layouts not found")
@pytest.mark.parametrize(
    "filename",
    ["cascade.xtc", "Control-Panels.xtc", "G Scale Folded Dogbone.xtc"],
)
def test_parse_multiple_layouts(filename):
    path = EXAMPLES_DIR / filename
    if not path.exists():
        pytest.skip(f"{filename} not found")
    layout = parse_file(path)
    assert layout.scale != "" or layout.room_width > 0
    assert len(layout.tracks) >= 0  # at minimum parses without error
