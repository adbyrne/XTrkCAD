"""generate_track_plan: a track plan laid along benchwork shelves.

Hillside, level 1, track 12 in from the wall, 20 in radius: the main runs
north up the west shelf (x=12), east along the north shelf (y=180) and
south down the east shelf (x=132). Corners: centres (32,160) and (112,160).
HO: a car is 6 in, tracks are 2 in apart; a Peco #6 diverges 1 in at 9.5
degrees over 9.19 in.
"""

import math
from pathlib import Path

import pytest
import yaml

from xtrkcad_mcp.config import load_config
from xtrkcad_mcp.track_plan import (
    PlanError,
    build_track_plan,
    clearance_point,
    offset_run_length,
    parse_track_plan,
    plan_route,
)
from xtrkcad_mcp.trackgeom import Curve, Straight, Turnout, find_turnouts

FIXTURES_DIR = Path(__file__).parent / "fixtures"
HILLSIDE = FIXTURES_DIR / "hillside_division.yaml"
LAYERS = {"L1-Main": 1, "L1-Passing": 2, "L1-Storage": 3, "L1-Staging": 4,
          "L1-Connecting": 5, "L1-Service": 6}


@pytest.fixture(scope="module")
def hillside():
    return load_config(HILLSIDE).config


def _spec(hillside, **plan):
    base = {"level": 1, "curve_radius": "20in", "turnout": "PECO Code 83 #6",
            "track_offset": "12in", "route": ["west_wall", "north_wall", "east_wall"]}
    base.update(plan)
    return parse_track_plan(base, hillside)


def _build(hillside, **plan):
    return build_track_plan(_spec(hillside, **plan), hillside, LAYERS)


# -- parsing ----------------------------------------------------------------------

def test_lengths_in_cars_inches_feet(hillside):
    spec = _spec(hillside, stations=[{"id": "AL", "shelf": "west_wall", "siding": "5 cars"}],
                 yard={"id": "HY", "tracks": 3, "track_length": "2.5ft"})
    assert spec.stations[0].siding_in == pytest.approx(30.0)
    assert spec.yard.track_length_in == pytest.approx(30.0)
    assert spec.track_offset_in == pytest.approx(12.0)


def test_unknown_key_is_an_error(hillside):
    with pytest.raises(PlanError, match="stationz"):
        _spec(hillside, stationz=[])


def test_unknown_shelf_is_an_error(hillside):
    with pytest.raises(PlanError, match="south_wall"):
        _spec(hillside, route=["south_wall"])


def test_shelf_on_another_level_is_an_error(hillside):
    with pytest.raises(PlanError, match="north_wall_l2"):
        _spec(hillside, route=["west_wall", "north_wall_l2"])


# -- the route ---------------------------------------------------------------------

def test_route_along_the_shelves(hillside):
    route = plan_route(_spec(hillside), hillside)
    x, y, h = route.point_at(0.0)
    assert (x, y, h) == pytest.approx((12.0, 1.0, 0.0))
    x, y, h = route.point_at(route.length)
    assert (x, y) == pytest.approx((132.0, 73.0))
    assert h == pytest.approx(180.0)
    corners = [c for c in route.corners]
    assert [c.center for c in corners] == [pytest.approx((32.0, 160.0)),
                                           pytest.approx((112.0, 160.0))]
    quarter = math.pi * 20.0 / 2
    assert route.length == pytest.approx((160 - 1) + quarter + 80 + quarter + (160 - 73))


def test_shelf_runs_and_wall_side(hillside):
    route = plan_route(_spec(hillside), hillside)
    west = route.run("west_wall")
    assert (west.s0, west.s1) == pytest.approx((0.0, 159.0))
    assert west.wall_side == "left"          # heading north, the west wall is on the left
    assert route.run("east_wall").wall_side == "left"


def test_offset_run_length_for_ho():
    peco = find_turnouts("PECO Code 83 #6", "HO")
    # 9.193 + 4.40 cos 9.5 + 20 sin 9.5
    assert offset_run_length(peco.left, 2.0, 20.0) == pytest.approx(16.83, abs=0.01)


# -- clearance points -----------------------------------------------------------------

def test_clearance_point_of_a_peco_number_6():
    """Where the diverging track is 13 ft (prototype) from the straight one,
    1.79 in in HO: on the straight past the turnout, 13.9 in from the points
    (the frog is 7.1 in from them)."""
    peco = find_turnouts("PECO Code 83 #6", "HO")
    for hand in (peco.left, peco.right):
        x = clearance_point(hand, 13 * 12 / 87.1, 2.0, 20.0)
        assert x == pytest.approx(13.9, abs=0.1)


def test_clearance_inside_the_turnout():
    """A clearance smaller than the turnout's own offset (1 in) is reached
    on the turnout's diverging route."""
    peco = find_turnouts("PECO Code 83 #6", "HO")
    assert clearance_point(peco.left, 0.65, 2.0, 20.0) == pytest.approx(7.1, abs=0.05)


def test_clearance_at_full_spacing_is_where_tracks_run_parallel():
    peco = find_turnouts("PECO Code 83 #6", "HO")
    assert clearance_point(peco.left, 2.0, 2.0, 20.0) == pytest.approx(
        offset_run_length(peco.left, 2.0, 20.0), abs=1e-6)


def test_clearance_default_is_13_ft_scaled(hillside):
    assert _spec(hillside).clearance_in == pytest.approx(13 * 12 / 87.1)
    assert _spec(hillside, clearance="2in").clearance_in == pytest.approx(2.0)


# -- what gets built -----------------------------------------------------------------

def _pieces(result, kind=None, layer=None):
    return [p for p in result.model.pieces
            if (kind is None or isinstance(p, kind)) and (layer is None or p.layer == layer)]


def test_bare_main_is_straights_and_two_curves(hillside):
    result = _build(hillside)
    assert result.errors == []
    assert len(_pieces(result, Curve)) == 2
    assert {p.layer for p in result.model.pieces} == {1}
    assert len(result.model.open_ends()) == 2         # the two ends of the line


def test_passing_siding(hillside):
    result = _build(hillside, stations=[{"id": "AL", "shelf": "west_wall",
                                         "siding": "5 cars", "siding_side": "room"}])
    assert result.errors == []
    turnouts = _pieces(result, Turnout)
    assert len(turnouts) == 2 and {t.layer for t in turnouts} == {1}
    siding = _pieces(result, layer=2)
    straights = [p for p in siding if isinstance(p, Straight)]
    clear = max(straights, key=lambda s: s.length())
    assert {round(clear.p0[0], 6), round(clear.p1[0], 6)} == {14.0}   # 2 in toward the room
    assert len(result.model.open_ends()) == 2
    station = result.feature("AL")
    # "5 cars" is between the clearance points; the straight part is shorter
    # by the curved track beyond each clearance point.
    assert station["capacity_in"] == pytest.approx(30.0, abs=1e-6)
    assert station["capacity_cars"] == 5
    tail = station["run_in"] - station["clearance_in"]
    assert station["clear_in"] == pytest.approx(30.0 - 2 * tail, abs=0.1)   # along the main vs the track
    assert clear.length() == pytest.approx(station["clear_in"], abs=0.01)
    assert station["clearance_in"] == pytest.approx(13.9, abs=0.1)


def test_wider_clearance_lengthens_the_siding(hillside):
    """At a 2 in clearance the clearance point is where the tracks run
    parallel, so the whole 30 in is straight."""
    result = _build(hillside, clearance="2in",
                    stations=[{"id": "AL", "shelf": "west_wall", "siding": "5 cars"}])
    assert result.feature("AL")["clear_in"] == pytest.approx(30.0, abs=1e-6)


def test_spur_ends_open_with_its_length(hillside):
    result = _build(hillside, stations=[{
        "id": "AL", "shelf": "west_wall", "siding": "5 cars",
        "spurs": [{"id": "AL_FEED", "length": "3 cars", "side": "wall",
                   "points_toward": "start"}]}])
    assert result.errors == []
    spur = result.feature("AL_FEED")
    assert spur["capacity_in"] == pytest.approx(18.0)
    assert spur["capacity_cars"] == 3
    assert spur["clear_in"] == pytest.approx(
        18.0 - (spur["run_in"] - spur["clearance_in"]), abs=0.1)
    assert spur["layer"] == 3
    ends = result.model.open_ends()
    assert len(ends) == 3
    spur_end = [e for _p, _i, e in ends if abs(e[0] - 10.0) < 1e-6]
    assert len(spur_end) == 1 and spur_end[0][2] == pytest.approx(0.0)   # points north


def test_facility_spur_on_service_layer(hillside):
    result = _build(hillside, stations=[{
        "id": "CE", "shelf": "east_wall", "siding": "4 cars",
        "spurs": [{"id": "CE_COAL", "kind": "facility", "type": "coal",
                   "length": "3 cars", "side": "wall", "points_toward": "start"}]}])
    assert result.errors == []
    assert result.feature("CE_COAL")["layer"] == 6


def test_spur_beside_a_siding_on_its_own_side_runs_end_to_end(hillside):
    """On the siding's side a spur can't overlap it: the 5-car siding (30 in
    between clearance points, 57.8 in switch to switch) plus 2 in plus the
    spur (31.9 in) is more than the shelf's 87 in."""
    result = _build(hillside, stations=[{
        "id": "CE", "shelf": "east_wall", "siding": "5 cars",
        "spurs": [{"id": "CE_COAL", "kind": "facility", "type": "coal",
                   "length": "3 cars", "side": "room", "points_toward": "start"}]}])
    assert result.model is None
    assert result.errors == ["station CE: needs 91.8 in on shelf east_wall, 87.0 in free there"]


def test_yard_at_route_start(hillside):
    result = _build(hillside, yard={"id": "HY", "tracks": 3, "track_length": "6 cars",
                                    "side": "room"})
    assert result.errors == []
    yard = result.feature("HY")
    assert len(yard["tracks"]) == 3
    caps = [t["capacity_in"] for t in yard["tracks"]]
    assert min(caps) == pytest.approx(36.0, abs=1e-6)     # the last two tracks
    assert all(t["capacity_cars"] >= 6 for t in yard["tracks"])
    bumpers = sorted(round(e[0], 6) for _p, _i, e in result.model.open_ends()
                     if abs(e[1] - 1.0) < 1e-6)
    assert bumpers == [12.0, 14.0, 16.0]


def test_terminus_with_tail(hillside):
    result = _build(hillside, stations=[{"id": "CE", "shelf": "east_wall",
                                         "siding": "4 cars", "terminus": True,
                                         "tail": "2 cars"}])
    assert result.errors == []
    cedar = result.feature("CE")
    assert cedar["terminus"] is True
    assert cedar["tail_in"] == pytest.approx(12.0, abs=1e-6)
    # The runaround's far switch is 12 in plus its own length short of the line's end.
    assert len(cedar["switches"]) == 2


def test_station_that_does_not_fit_says_by_how_much(hillside):
    result = _build(hillside, stations=[{"id": "BI", "shelf": "north_wall",
                                         "siding": "12 cars"}])
    assert result.model is None
    assert len(result.errors) == 1
    msg = result.errors[0]
    assert "BI" in msg and "north_wall" in msg and " in" in msg


def test_track_beyond_the_shelf_edge_is_an_error(hillside):
    result = _build(hillside, yard={"id": "HY", "tracks": 8, "track_length": "2 cars",
                                    "side": "room"})
    assert result.model is None
    assert any("HY" in e and "shelf" in e for e in result.errors)


# -- the Hillside plan ------------------------------------------------------------------

def test_hillside_track_plan_file_builds(hillside):
    raw = yaml.safe_load((FIXTURES_DIR / "hillside_track_plan.yaml").read_text())
    result = build_track_plan(parse_track_plan(raw, hillside), hillside, LAYERS)
    assert result.errors == []
    ids = {f["id"] for f in result.features}
    assert {"HY", "AL", "BI", "CE"} <= ids
    # Every open end is a bumper: 3 yard tracks, the spurs, Cedar's tail.
    spurs = [f for f in result.features if f["kind"] in ("industry", "team_track",
                                                         "facility")]
    assert len(result.model.open_ends()) == 3 + len(spurs) + 1


def test_spurs_ending_face_to_face_keep_their_distance(hillside):
    result = _build(hillside, stations=[{
        "id": "AL", "shelf": "west_wall", "siding": "5 cars",
        "spurs": [{"id": "A", "length": "3 cars", "side": "wall", "points_toward": "start"},
                  {"id": "B", "length": "2 cars", "side": "wall", "points_toward": "end"}]}])
    assert result.errors == []
    gap = abs(result.feature("B")["end"][1] - result.feature("A")["end"][1])
    assert gap >= 2.0 - 1e-6


def test_spur_shorter_than_its_curve_is_an_error(hillside):
    """Between its clearance point and where it runs parallel a spur has
    about 2.9 in of curve: it can't be shorter than that."""
    result = _build(hillside, stations=[{
        "id": "AL", "shelf": "west_wall", "siding": "5 cars",
        "spurs": [{"id": "A", "length": "2in", "side": "wall", "points_toward": "start"}]}])
    assert result.model is None
    assert "A" in result.errors[0] and "clearance" in result.errors[0]


# -- the tool -------------------------------------------------------------------------

def _benchwork_file(tmp_path):
    """Hillside from generate_layout: benchwork and layers, no track yet."""
    from xtrkcad_mcp.server import generate_layout
    out = tmp_path / "hillside.xtc"
    assert generate_layout(str(HILLSIDE), str(out))["ready"] is True
    return out


def test_config_names_the_track_plan_file(hillside):
    assert hillside.track_plan_file.endswith("hillside_track_plan.yaml")
    assert hillside.track_plan["route"] == ["west_wall", "north_wall", "east_wall"]


def test_generate_track_plan_writes_connected_track(tmp_path):
    from xtrkcad_mcp.parser import parse_file
    from xtrkcad_mcp.server import generate_track_plan
    layout = _benchwork_file(tmp_path)
    result = generate_track_plan(str(HILLSIDE), str(layout))
    assert result["written"] is True, result
    assert result["backup_path"]                  # the benchwork-only file is kept
    parsed = parse_file(layout)
    tracks = [t for t in parsed.tracks if t.kind in ("STRAIGHT", "CURVE", "TURNOUT")]
    assert len(tracks) == result["pieces"] == 61
    open_ends = [ep for t in tracks for ep in t.endpoints if ep.connected_to is None]
    assert len(open_ends) == result["open_ends"] == 8
    by_id = {t.id: t for t in parsed.tracks}
    for t in tracks:                             # every link points back
        for ep in t.endpoints:
            if ep.connected_to is not None:
                assert any(e.connected_to == t.id for e in by_id[ep.connected_to].endpoints)
    assert {f["id"] for f in result["features"]} >= {"HY", "AL", "BI", "CE", "CE_COAL"}


def test_generate_track_plan_new_ids_follow_the_file(tmp_path):
    from xtrkcad_mcp.parser import parse_file
    from xtrkcad_mcp.server import generate_track_plan
    layout = _benchwork_file(tmp_path)
    before = layout.read_text(encoding="utf-8")
    ids = [int(ln.split()[1]) for ln in before.splitlines()
           if ln.split() and ln.split()[0] in ("DRAW", "NOTE") and ln.split()[1].isdigit()]
    generate_track_plan(str(HILLSIDE), str(layout))
    tracks = parse_file(layout).tracks
    assert min(t.id for t in tracks if t.kind != "DRAW") > max(ids)


def test_generate_track_plan_refuses_a_file_with_track(tmp_path):
    from xtrkcad_mcp.server import generate_track_plan
    layout = _benchwork_file(tmp_path)
    assert generate_track_plan(str(HILLSIDE), str(layout))["written"] is True
    again = generate_track_plan(str(HILLSIDE), str(layout))
    assert again["written"] is False
    assert "already has" in again["errors"][0]


def test_generate_track_plan_returns_fit_errors(tmp_path):
    from xtrkcad_mcp.server import generate_track_plan
    layout = _benchwork_file(tmp_path)
    plan = yaml.safe_load((FIXTURES_DIR / "hillside_track_plan.yaml").read_text())
    plan["stations"][1]["siding"] = "12 cars"
    (tmp_path / "plan.yaml").write_text(yaml.safe_dump(plan))
    cfg = yaml.safe_load(HILLSIDE.read_text())
    cfg.update(track_plan_file=str(tmp_path / "plan.yaml"),
               floor_plan_file=str(FIXTURES_DIR / cfg["floor_plan_file"]),
               benchwork_file=str(FIXTURES_DIR / cfg["benchwork_file"]))
    (tmp_path / "cfg.yaml").write_text(yaml.safe_dump(cfg))
    before = layout.read_text(encoding="utf-8")
    result = generate_track_plan(str(tmp_path / "cfg.yaml"), str(layout))
    assert result["written"] is False
    assert "BI" in result["errors"][0]
    assert layout.read_text(encoding="utf-8") == before


def test_generate_track_plan_without_a_plan(tmp_path):
    from xtrkcad_mcp.server import generate_track_plan
    cfg = tmp_path / "cfg.yaml"
    cfg.write_text("name: T\nscale: HO\nroom: 10x10ft\n")
    result = generate_track_plan(str(cfg), str(tmp_path / "x.xtc"))
    assert result["written"] is False and "track_plan" in result["errors"][0]


def test_notes_option_not_yet_available(hillside):
    with pytest.raises(PlanError, match="notes"):
        _spec(hillside, notes=True)
