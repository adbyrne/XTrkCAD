"""Track geometry: turnout definitions, pieces, connections, .xtc records.

Angles are XTrkCAD's: compass degrees, 0 = north (+y), 90 = east (+x). An
endpoint's angle points out of the track at that end.
"""

import math
from pathlib import Path

import pytest

from xtrkcad_mcp.parser import parse_file
from xtrkcad_mcp.trackgeom import (
    Curve,
    Straight,
    TrackModel,
    Turnout,
    find_turnouts,
    params_dirs,
    read_turnout_defs,
)

PECO = "PECO Code 83 #6"


@pytest.fixture(scope="module")
def peco():
    return find_turnouts(PECO, "HO")


def _close(p, q, tol=1e-3):
    return math.dist(p[:2], q[:2]) < tol


def _angle_close(a, b, tol=1e-3):
    return abs((a - b + 180) % 360 - 180) < tol


# -- turnout definitions ------------------------------------------------------------

def test_params_dirs_include_the_repo_params():
    assert any((d / "HO-Peco-Code83.xtp").exists() for d in params_dirs())


def test_find_peco_number_6_both_hands(peco):
    assert "Left Hand" in peco.left.title and "#6" in peco.left.title
    assert "Right Hand" in peco.right.title and "#6" in peco.right.title


def test_peco_geometry(peco):
    for defn, sign in ((peco.left, 1), (peco.right, -1)):
        assert defn.length == pytest.approx(9.192913)
        assert defn.frog_angle == pytest.approx(9.5)
        assert defn.diverge_offset == pytest.approx(sign * 1.0)


def test_unknown_turnout_names_what_it_looked_for():
    with pytest.raises(LookupError, match="No Such #6"):
        find_turnouts("No Such #6", "HO")


def test_read_turnout_defs_keeps_segments():
    path = next(d / "HO-Peco-Code83.xtp" for d in params_dirs()
                if (d / "HO-Peco-Code83.xtp").exists())
    defs = {d.title: d for d in read_turnout_defs(path)}
    left = next(d for t, d in defs.items() if "#6 Left Hand Turnout" in t)
    assert [ln.split()[0] for ln in left.paths] == ["P", "P"]
    assert [ln.split()[0] for ln in left.segments] == ["S", "S", "C", "S"]


# -- pieces --------------------------------------------------------------------

def test_turnout_placed_like_xtrkcad(peco):
    """The NYE file's turnout 447 sits at (110.81, 95.06), angle 190.10; its
    legs end 7.9 in along local x. The same transform for the Peco: local x
    runs toward compass angle+90, local y toward angle."""
    t = Turnout(1, 0, peco.left, (110.809507, 95.064965), 190.104653)
    ends = t.ends()
    assert _close(ends[0], (110.809507, 95.064965))
    assert _angle_close(ends[0][2], 100.104653)
    heading = math.radians(280.104653)
    expect = (110.809507 + 9.192913 * math.sin(heading),
              95.064965 + 9.192913 * math.cos(heading))
    assert _close(ends[1], expect)
    assert _angle_close(ends[1][2], 280.104653)


def test_turnout_heading_and_side(peco):
    """Points at the origin heading east: the left hand diverges north."""
    t = Turnout.at_points(1, 0, peco.left, (0.0, 0.0), heading=90.0)
    _p, straight, diverge = t.ends()
    assert _close(straight, (9.192913, 0.0))
    assert _close(diverge, (9.192913, 1.0))
    assert _angle_close(diverge[2], 80.5)


def test_straight_ends_point_outward():
    s = Straight(1, 0, (0.0, 0.0), (10.0, 0.0))
    (x0, y0, a0), (x1, y1, a1) = s.ends()
    assert (x0, y0, x1, y1) == (0.0, 0.0, 10.0, 0.0)
    assert _angle_close(a0, 270.0) and _angle_close(a1, 90.0)
    assert s.length() == pytest.approx(10.0)


def test_curve_ends_run_clockwise():
    """A quarter circle, centre (0,0), radius 20, from west (270) clockwise
    to north (0/360): XTrkCAD stores the ends in clockwise order."""
    c = Curve(1, 0, (0.0, 0.0), 20.0, 270.0, 90.0)
    (x0, y0, a0), (x1, y1, a1) = c.ends()
    assert _close((x0, y0), (-20.0, 0.0)) and _angle_close(a0, 180.0)
    assert _close((x1, y1), (0.0, 20.0)) and _angle_close(a1, 90.0)
    assert c.length() == pytest.approx(math.pi * 10.0)


# -- connections and records ---------------------------------------------------

def test_connect_matching_ends(peco):
    model = TrackModel("HO")
    a = model.add(Straight(0, 0, (0.0, 0.0), (10.0, 0.0)))
    b = model.add(Straight(0, 0, (10.0, 0.0), (20.0, 0.0)))
    model.connect_all()
    assert model.connected_to(a, 1) == (b, 0)
    assert model.connected_to(b, 0) == (a, 1)
    assert model.connected_to(a, 0) is None


def test_open_ends_listed():
    model = TrackModel("HO")
    model.add(Straight(0, 0, (0.0, 0.0), (10.0, 0.0)))
    model.connect_all()
    assert len(model.open_ends()) == 2


def test_records_parse_back(tmp_path, peco):
    """Records written into a file are read back by the parser with their
    connections."""
    model = TrackModel("HO")
    t = model.add(Turnout.at_points(0, 1, peco.right, (0.0, 0.0), heading=90.0))
    s = model.add(Straight(0, 1, t.ends()[1][:2], (30.0, 0.0)))
    model.connect_all()
    lines = model.xtc_records(first_id=10)
    text = "\n".join(["VERSION 13 5.4.0", "TITLE1 t", "SCALE HO", "ROOMSIZE 40 x 20",
                      'LAYERS 1 1 0 0 0 0 0 0 0 "L1-Main"', "LAYERS CURRENT 1", ""]
                     + lines) + "\n"
    p = tmp_path / "t.xtc"
    p.write_text(text, encoding="utf-8")
    layout = parse_file(p)
    by_id = {tr.id: tr for tr in layout.tracks}
    assert set(by_id) == {10, 11}
    assert by_id[10].kind == "TURNOUT" and by_id[11].kind == "STRAIGHT"
    assert any(ep.connected_to == 11 for ep in by_id[10].endpoints)
    assert any(ep.connected_to == 10 for ep in by_id[11].endpoints)
    assert by_id[11].length_model_inches() == pytest.approx(30.0 - 9.192913, abs=1e-3)
    assert "PECO Code 83\t#6 Right Hand Turnout" in text
