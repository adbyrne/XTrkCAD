"""Fixes from the MCP review of 2026-11-20 (.claude review, findings 1, 2, 3, 7).

1. delete_track and fix_dead_connections back the layout up before rewriting it.
2. Every tool that rewrites a layout says to close it in XTrkCAD first.
3. get_stations and get_station_distances use the milepost model on a layout
   with convention notes, so they agree with the export and the report.
7. A turnout whose ends all point out from one centre (a sectional
   turntable from a parameter file) is a turntable to the gaps report.
"""

import json
import math
import sys
from pathlib import Path

import pytest

from xtrkcad_mcp import server
from xtrkcad_mcp.models import turntable_like_ids
from xtrkcad_mcp.parser import parse_file

sys.path.insert(0, str(Path(__file__).parent))
import test_tutorial as T  # noqa: E402  (the tutorial's Hillside, chapter by chapter)


# -- 1, 2: editing tools ------------------------------------------------------

LAYOUT = """VERSION 13 5.4.0
SCALE HO
ROOMSIZE 100 x 100
LAYERS 0 1 0 1 0 0 0 0 0 "Main" 1 0 0.000000 0.000000 0.000000 0.000000 0.000000
LAYERS CURRENT 0
STRAIGHT 1 0 0 0 0 HO 2 0.000000 0.000000
\tE4 0.000000 0.000000 270.000000 0 0.0 0.0 0.0 0.0 0 0 0 0.000000
\tT4 9 10.000000 0.000000 90.000000 0 0.0 0.0 0.0 0.0 0 0 0 0.000000
\tEND$SEGS
STRAIGHT 2 0 0 0 0 HO 2 0.000000 0.000000
\tE4 20.000000 0.000000 270.000000 0 0.0 0.0 0.0 0.0 0 0 0 0.000000
\tE4 30.000000 0.000000 90.000000 0 0.0 0.0 0.0 0.0 0 0 0 0.000000
\tEND$SEGS
END$TRACKS
"""


@pytest.fixture
def layout(tmp_path):
    p = tmp_path / "small.xtc"
    p.write_text(LAYOUT, encoding="utf-8")
    return p


def test_delete_track_backs_up_first(layout):
    result = server.delete_track(str(layout), 2)
    backup = layout.with_name("small_v1.xtc")
    assert backup.read_text(encoding="utf-8") == LAYOUT
    assert str(backup) in result
    assert "STRAIGHT 2 " not in layout.read_text(encoding="utf-8")


def test_fix_dead_connections_backs_up_first(layout):
    result = server.fix_dead_connections(str(layout))
    backup = layout.with_name("small_v1.xtc")
    assert backup.read_text(encoding="utf-8") == LAYOUT
    assert str(backup) in result
    assert "T4 9 " not in layout.read_text(encoding="utf-8")


def test_nothing_to_fix_writes_no_backup(layout):
    server.fix_dead_connections(str(layout))                   # fixes track 1
    server.fix_dead_connections(str(layout))                   # nothing left
    assert not layout.with_name("small_v2.xtc").exists()


EDITING_TOOLS = ["generate_layout", "generate_track_plan", "merge_benchwork",
                 "add_track_layers", "rename_layers", "delete_track",
                 "fix_dead_connections"]


@pytest.mark.parametrize("name", EDITING_TOOLS)
def test_editing_tools_say_to_close_the_layout_in_xtrkcad(name):
    doc = getattr(server, name).__doc__
    assert server.CLOSE_IN_XTRKCAD in " ".join(doc.split())


def test_delete_track_result_says_it_too(layout):
    assert server.CLOSE_IN_XTRKCAD in server.delete_track(str(layout), 2)


# -- 3: station tools on the milepost model -----------------------------------

@pytest.fixture(scope="module")
def hillside(tmp_path_factory):
    tmp = tmp_path_factory.mktemp("hillside")
    base = tmp / "hillside.xtc"
    server.generate_layout(str(T.HILLSIDE), str(base))
    server.generate_track_plan(str(T.HILLSIDE), str(base))
    T._state(base, tmp, T.CHAPTERS[-1])
    return tmp / f"ch{T.CHAPTERS[-1]}.xtc"


def test_station_distances_agree_with_the_export(hillside):
    pairs = {(d["from"], d["to"]): d for d in server.get_station_distances(str(hillside))}
    hy_al = pairs[("HY", "AL")]
    assert (hy_al["mp_distance"], hy_al["distance_proto_ft"], hy_al["source"]) == (
        3.395, 295.7, "mileposts")
    export = T.build_export_v2(parse_file(hillside))
    seg = {(s["from_station"], s["to_station"]): s["length_ft"] for s in export["segments"]}
    for (a, b), ft in seg.items():
        assert pairs[(a, b)]["distance_proto_ft"] == pytest.approx(ft, abs=0.1)
    assert all(d["reachable"] for d in pairs.values())


def test_stations_from_the_convention_notes(hillside):
    st = {s["name"]: s for s in server.get_stations(str(hillside))}
    assert sorted(st) == ["AL", "BI", "CE", "HY"]
    assert (st["AL"]["mp"], st["AL"]["station_name"], st["AL"]["source"]) == (
        3.395, "Alder", "conventions")
    assert (st["AL"]["x"], st["AL"]["y"]) == (12.0, 109.5)


def test_text_notes_keep_the_track_path(tmp_path):
    """A layout with STATION: text notes and no MP anchor: as before."""
    src = Path(__file__).parent / "fixtures" / "export_test_layout.xtc"
    out = server.get_station_distances(str(src))
    assert out and all(d.get("source") == "track path" for d in out)


# -- 7: sectional turntables ---------------------------------------------------

def _turnout(tid, ends, x=50.0, y=50.0):
    lines = [f'TURNOUT {tid} 0 0 0 0 HO 2 {x:.6f} {y:.6f} 0 0.000000 "Test\tTurntable\tT"']
    for ex, ey, a, other in ends:
        if other is None:
            lines.append(f"\tE4 {ex:.6f} {ey:.6f} {a:.6f} 0 0.0 0.0 0.0 0.0 0 0 0 0.000000")
        else:
            lines.append(f"\tT4 {other} {ex:.6f} {ey:.6f} {a:.6f} 0 0.0 0.0 0.0 0.0 0 0 0 0.000000")
    lines += ["\tD 0.000000 0.000000", "\tEND$SEGS"]
    return "\n".join(lines)


def _radial(cx, cy, r, angles, connected=()):
    """Ends on a circle, each pointing straight out from the centre
    (XTrkCAD angles: 0 = +y, clockwise)."""
    out = []
    for k, a in enumerate(angles):
        rad = math.radians(a)
        out.append((cx + r * math.sin(rad), cy + r * math.cos(rad), a,
                    connected[k] if k < len(connected) else None))
    return out


def _write(tmp_path, *records):
    p = tmp_path / "tt.xtc"
    head = LAYOUT.split("STRAIGHT 1")[0]
    p.write_text(head + "\n".join(records) + "\nEND$TRACKS\n", encoding="utf-8")
    return p


def test_half_circle_of_positions_is_a_turntable(tmp_path):
    p = _write(tmp_path, _turnout(5, _radial(50, 50, 6, [0, 22.5, 45, 67.5, 90, 112.5, 135, 157.5])))
    assert turntable_like_ids(parse_file(p)) == {5}


def test_ordinary_turnouts_are_not(tmp_path):
    three_way = [(50, 50, 180, None), (50, 59, 0, None), (49, 59, 350, None), (51, 59, 10, None)]
    p = _write(tmp_path, _turnout(6, three_way))
    assert turntable_like_ids(parse_file(p)) == set()


def test_five_ends_not_from_one_centre_are_not(tmp_path):
    ends = [(50, 50, 0, None), (60, 50, 90, None), (70, 52, 90, None),
            (50, 40, 180, None), (40, 50, 270, None)]
    p = _write(tmp_path, _turnout(7, ends))
    assert turntable_like_ids(parse_file(p)) == set()


def test_gaps_report_excludes_unused_turntable_positions(tmp_path):
    p = _write(tmp_path, _turnout(5, _radial(50, 50, 6, [0, 45, 90, 135, 180, 225, 270, 315])))
    out = tmp_path / "gaps.json"
    server.write_gaps_report(str(p), str(out), format="json")
    report = json.loads(out.read_text(encoding="utf-8"))
    assert (report["turntable_stalls"], report["open_endpoints"]) == (8, 0)
