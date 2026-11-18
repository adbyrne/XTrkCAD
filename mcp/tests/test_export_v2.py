"""export_layout_data v2 (Operating Data Conventions spec v1, section 8).

conventions_clean.xtc has every note kind on a straight main from x=0 to
x=240 at 12 layout inches per MP, so the expected values below are worked
out by hand from its coordinates.
"""

import json
from pathlib import Path

import pytest

from xtrkcad_mcp import server
from xtrkcad_mcp.export_v2 import SCHEMA_VERSION, build_export_v2
from xtrkcad_mcp.parser import parse_file

FIXTURES_DIR = Path(__file__).parent / "fixtures"
CLEAN_FIXTURE = FIXTURES_DIR / "conventions_clean.xtc"


@pytest.fixture(scope="module")
def clean():
    return build_export_v2(parse_file(CLEAN_FIXTURE))


def _with_error(tmp_path):
    """The clean fixture plus a note of an unknown kind (an error)."""
    text = CLEAN_FIXTURE.read_text(encoding="utf-8")
    bad = ('NOTE 199 0 0 0 50.000000 0.000000 0 3 '
           '"{""kind"": ""stationn"", ""id"": ""ZZ""}"\n')
    p = tmp_path / "with_error.xtc"
    p.write_text(text.replace("\nSTRAIGHT 1 ", "\n" + bad + "\nSTRAIGHT 1 ", 1),
                 encoding="utf-8")
    return p


# -- phase 1: header, railroads, issues, refusal -----------------------------

def test_header(clean):
    assert clean["schema_version"] == SCHEMA_VERSION == 2
    assert clean["conventions_version"] == 1
    assert clean["layout"] == "Conventions clean fixture"
    assert clean["scale"] == "HO"
    assert clean["layout_units"] == "in"
    assert isinstance(clean["generated"], str) and clean["generated"]


def test_railroads_and_subdivisions(clean):
    assert clean["railroads"] == [{
        "id": "NYE", "name": "New York & Eastern", "reporting_marks": "NYE",
        "subdivisions": [{
            "id": "NLS", "name": "Northern Lights Subdivision",
            "mp_scale": 12, "mp_unit": "in",
            "anchors": [{"id": "MP_ZERO", "mp": 0.0, "track_id": 1}],
        }],
    }]


def test_railroad_without_subdivisions_has_one_unnamed(tmp_path):
    text = CLEAN_FIXTURE.read_text(encoding="utf-8")
    sub = next(ln for ln in text.splitlines() if '""subdivision""' in ln)
    p = tmp_path / "no_sub.xtc"
    p.write_text(text.replace(sub + "\n", ""), encoding="utf-8")
    rr = build_export_v2(parse_file(p))["railroads"][0]
    assert [(s["id"], s["name"]) for s in rr["subdivisions"]] == [(None, None)]
    assert rr["subdivisions"][0]["anchors"][0]["id"] == "MP_ZERO"


def test_issues_are_the_convention_check(clean):
    from xtrkcad_mcp.conventions import check_conventions
    expected = [i.as_dict() for i in check_conventions(parse_file(CLEAN_FIXTURE))]
    assert clean["issues"] == expected
    assert not [i for i in clean["issues"] if i["severity"] in ("error", "warning")]


def test_tool_writes_v2_by_default(tmp_path):
    out = tmp_path / "layout_data.json"
    result = server.export_layout_data(str(CLEAN_FIXTURE), str(out))
    assert result["written"] is True and result["output"] == str(out)
    data = json.loads(out.read_text(encoding="utf-8"))
    assert data["schema_version"] == 2
    assert result["counts"] == {"error": 0, "warning": 0,
                                "info": len(data["issues"])}


def test_tool_refuses_on_errors(tmp_path):
    out = tmp_path / "layout_data.json"
    result = server.export_layout_data(str(_with_error(tmp_path)), str(out))
    assert result["written"] is False
    assert not out.exists()
    assert result["counts"]["error"] >= 1
    assert "NOTE_UNKNOWN_KIND" in {e["rule"] for e in result["errors"]}


def test_tool_force_writes_with_errors(tmp_path):
    out = tmp_path / "layout_data.json"
    result = server.export_layout_data(str(_with_error(tmp_path)), str(out), force=True)
    assert result["written"] is True
    data = json.loads(out.read_text(encoding="utf-8"))
    assert "NOTE_UNKNOWN_KIND" in {i["rule"] for i in data["issues"]}


def test_tool_v1_format_unchanged(tmp_path):
    out = tmp_path / "layout_data.json"
    result = server.export_layout_data(str(CLEAN_FIXTURE), str(out), format="v1")
    data = json.loads(out.read_text(encoding="utf-8"))
    assert "schema_version" not in data and "yard_tracks" in data
    assert result["output"] == str(out)


def test_tool_v2_ignores_stations_yaml(tmp_path):
    out = tmp_path / "layout_data.json"
    result = server.export_layout_data(str(CLEAN_FIXTURE), str(out),
                                       stations_yaml="whatever.yaml")
    assert result["written"] is True
    assert any("stations_yaml" in n for n in result["notices"])


def test_tool_rejects_unknown_format(tmp_path):
    with pytest.raises(ValueError):
        server.export_layout_data(str(CLEAN_FIXTURE), str(tmp_path / "x.json"),
                                  format="v3")


# -- phase 2: stations and yard limits ----------------------------------------
# WP: terminus at MP_ZERO (x=0). XP: limits from its switchback switch,
# turnout 5 at x=190 = MP 15.833. Boards: WP x=5, 40; XP x=145, 195.

_STATION_KEYS = {"id", "kind", "name", "railroad", "subdivision", "mp", "mp_entry", "mp_exit",
                 "terminus", "switchback", "siding_ft", "siding_cars", "main_ft",
                 "main_cars", "house_tracks"}


def _stations(data):
    return {s["id"]: s for s in data["stations"]}


def test_stations_fields(clean):
    assert all(set(s) == _STATION_KEYS for s in clean["stations"])


def test_stations_mileposts(clean):
    st = _stations(clean)
    assert (st["WP"]["mp"], st["WP"]["mp_entry"], st["WP"]["mp_exit"]) == (0.0, 0.0, 0.0)
    assert st["XP"]["mp"] == st["XP"]["mp_entry"] == st["XP"]["mp_exit"] == 15.833
    # By MP; MILLER_TEAM is a team track with no station (MP 8.333).
    assert [s["id"] for s in clean["stations"]] == ["WP", "MILLER_TEAM", "XP"]


def test_stations_names_and_owners(clean):
    st = _stations(clean)
    assert st["WP"]["name"] == "Williamsport" and st["XP"]["name"] == "Xina Pass"
    assert {(s["railroad"], s["subdivision"]) for s in clean["stations"]} == {("NYE", "NLS")}


def test_stations_terminus_and_switchback(clean):
    st = _stations(clean)
    assert (st["WP"]["terminus"], st["WP"]["switchback"]) == (True, False)
    # A switchback note (XP_SWB, serves XP) makes XP a switchback station.
    assert (st["XP"]["terminus"], st["XP"]["switchback"]) == (False, True)


def test_yard_limits(clean):
    assert clean["yard_limits"] == [
        {"id": "WP", "railroad": "NYE", "subdivision": "NLS",
         "mp_start": 0.0, "mp_end": 3.333, "source": "board"},
        {"id": "XP", "railroad": "NYE", "subdivision": "NLS",
         "mp_start": 12.083, "mp_end": 16.25, "source": "board"},
    ]


def test_joint_station_has_one_entry_per_subdivision(tmp_path):
    # Spec 4.3: WP on two railroads' mains, one station note on each, same id.
    text = CLEAN_FIXTURE.read_text(encoding="utf-8")
    text = text.replace('LAYERS CURRENT 0\n',
                        'LAYERS 4 1 0 0 0 0 0 0 0 "L2-CO-Main"\nLAYERS CURRENT 0\n')
    text = text.replace('LAYERGROUP MEMBERS 4 "1;2;3;4"\n',
                        'LAYERGROUP MEMBERS 4 "1;2;3;4"\n'
                        'LAYERGROUP DEFINE 5 "C&O"\nLAYERGROUP MEMBERS 5 "5"\n')
    text = text.replace('LAYERGROUP MEMBERS 0 "1"', 'LAYERGROUP MEMBERS 0 "1;5"')
    notes = (
        'NOTE 130 4 0 0 0.000000 0.000000 0 3 "{""kind"": ""railroad"", ""id"": ""CO"", '
        '""name"": ""Chesapeake & Ohio"", ""group"": ""C&O""}"\n'
        'NOTE 131 4 0 0 0.000000 70.000000 0 3 "{""kind"": ""reference"", ""id"": ""CO_ZERO"", '
        '""mp"": 200, ""mp_scale"": 12}"\n'
        'NOTE 132 4 0 0 60.000000 70.000000 0 3 "{""kind"": ""station"", ""id"": ""WP"", '
        '""name"": ""Williamsport""}"\n'
        '\nSTRAIGHT 40 4 0 0 0 HO 0\n\tE 0.000000 70.000000 270.000000\n'
        '\tE 240.000000 70.000000 90.000000\n\tEND$SEGS\n')
    text = text.replace("\nSTRAIGHT 1 ", "\n" + notes + "\nSTRAIGHT 1 ", 1)
    p = tmp_path / "joint.xtc"
    p.write_text(text, encoding="utf-8")
    data = build_export_v2(parse_file(p))
    assert not [i for i in data["issues"] if i["severity"] != "info"]
    wp = [s for s in data["stations"] if s["id"] == "WP"]
    assert {(s["railroad"], s["subdivision"], s["mp"]) for s in wp} == {
        ("NYE", "NLS", 0.0), ("CO", None, 205.0)}


# -- phase 3: lengths and capacities ------------------------------------------
# Usable length: the connected non-turnout tracks (switch throats excluded),
# as the capacity reports measure. HO: 87.1 to 1, 2 cars per model foot.
# Clean fixture: the Service track (3) is 240 in; MILLER_TEAM's spur (9) is
# (108,-5)-(140,-15); the Staging track (4) is 240 in.

from xtrkcad_mcp.models import cars_per_real_ft  # noqa: E402
from xtrkcad_mcp.stations import model_in_to_proto_ft  # noqa: E402

from .test_conventions import LINE, ZERO, _layout, _st  # noqa: E402

SPUR_9_IN = (32 ** 2 + 10 ** 2) ** 0.5


def _ft(inches):
    return round(model_in_to_proto_ft(inches, "HO"), 3)


def _cars(inches):
    return int(inches / 12.0 * cars_per_real_ft("HO"))


def test_house_tracks(clean):
    xp = _stations(clean)["XP"]
    assert xp["house_tracks"] == [
        {"label": None, "track_id": 3, "length_ft": _ft(240), "capacity_cars": _cars(240)}]
    assert _stations(clean)["WP"]["house_tracks"] == []


def test_industries(clean):
    assert clean["industries"] == [{
        "id": "QM1", "name": "Quilly Mine #1", "station": "XP", "branch_mp": None,
        "switchback": False, "car_spots": 2, "spur_capacity_cars": _cars(240),
        "car_types": ["HM"], "loads_in": ["empties"], "loads_out": ["coal"],
    }]


def test_team_tracks(clean):
    tt = {t["id"]: t for t in clean["team_tracks"]}
    assert set(tt) == {"XP_TEAM", "MILLER_TEAM"}
    assert tt["XP_TEAM"]["station"] == "XP" and tt["XP_TEAM"]["car_spots"] == 3
    m = tt["MILLER_TEAM"]
    assert (m["station"], m["branch_mp"], m["car_spots"]) == (None, 8.333, 2)
    assert m["spur_capacity_cars"] == _cars(SPUR_9_IN)
    assert (m["car_types"], m["loads_in"], m["loads_out"]) == ([], [], [])


def test_team_track_without_station_is_a_station(clean):
    # Spec 8: kind team_track, mp = its branch_mp.
    miller = _stations(clean)["MILLER_TEAM"]
    assert miller["kind"] == "team_track" and miller["name"] == "Miller team track"
    assert miller["mp"] == miller["mp_entry"] == miller["mp_exit"] == 8.333
    assert {s["kind"] for s in clean["stations"] if s["id"] != "MILLER_TEAM"} == {"station"}
    assert "XP_TEAM" not in _stations(clean)


def test_yards(clean):
    yards = {y["id"]: y for y in clean["yards"]}
    assert {i: y["kind"] for i, y in yards.items()} == {
        "EAST": "staging", "QM1": "industry", "WP": "station"}
    assert yards["EAST"]["tracks"] == [{
        "label": "S1", "function": "staging", "railroads": [], "track_id": 4,
        "length_ft": _ft(240), "capacity_cars": _cars(240)}]
    assert [t["label"] for t in yards["WP"]["tracks"]] == ["Track 2"]
    assert yards["QM1"]["tracks"][0]["function"] == "loading"


def test_siding_and_main_lengths(tmp_path):
    # LINE: switches 2 (x=60) and 5 (x=180); passing track 4 and main track
    # 3 both run 68..172 between the switch throats.
    data = build_export_v2(_layout(tmp_path, [ZERO, _st("XP", 100)], tracks=LINE))
    xp = _stations(data)["XP"]
    assert (xp["mp_entry"], xp["mp_exit"]) == (5.0, 15.0)
    assert (xp["siding_ft"], xp["siding_cars"]) == (_ft(104), _cars(104))
    assert (xp["main_ft"], xp["main_cars"]) == (_ft(104), _cars(104))


def test_no_siding_no_lengths(clean):
    for st in clean["stations"]:
        assert st["siding_ft"] is None and st["siding_cars"] is None
        assert st["main_ft"] is None and st["main_cars"] is None


def test_terminus_runaround_siding(tmp_path):
    # Like NYE's HC: the terminus switch (5, where MP_ZERO sits) is also the
    # first switch of a runaround beyond it (passing track 4, to switch 2).
    zero = (0, {"kind": "reference", "id": "MP_ZERO", "mp": 0, "mp_scale": 12}, None, 176, 0)
    notes = [zero, _st("WP", 100, terminus=True, ref_tag="MP_ZERO"), _st("SK", 228)]
    wp = _stations(build_export_v2(_layout(tmp_path, notes, tracks=LINE)))["WP"]
    assert wp["terminus"] is True and wp["mp"] == 0.0
    assert (wp["siding_ft"], wp["siding_cars"]) == (_ft(104), _cars(104))
    assert (wp["main_ft"], wp["main_cars"]) == (_ft(104), _cars(104))


def test_siding_counts_turnouts_on_the_siding(tmp_path):
    # A spur leaving the siding: its turnout is on the passing layer and
    # cars stand on its through route, so it counts (the main switches
    # at the siding's ends don't).
    tracks = LINE.replace(
        "STRAIGHT 4 1 0 0 0 HO 0\n\tT 2 68.000000 4.000000 260.000000\n"
        "\tT 5 172.000000 4.000000 100.000000\n",
        "STRAIGHT 4 1 0 0 0 HO 0\n\tT 2 68.000000 4.000000 260.000000\n"
        "\tT 30 110.000000 4.000000 100.000000\n\tEND$SEGS\n\n"
        "TURNOUT 30 1 0 0 0 HO 2 110.000000 4.000000 0 0.000000 \"S\t#6 Left\t\"\n"
        "\tT 4 110.000000 4.000000 280.000000\n\tT 31 118.000000 4.000000 100.000000\n"
        "\tE 118.000000 8.000000 80.000000\n\tEND$SEGS\n\n"
        "STRAIGHT 31 1 0 0 0 HO 0\n\tT 30 118.000000 4.000000 280.000000\n"
        "\tT 5 172.000000 4.000000 100.000000\n")
    assert "TURNOUT 30" in tracks
    xp = _stations(build_export_v2(_layout(tmp_path, [ZERO, _st("XP", 100)],
                                           tracks=tracks)))["XP"]
    turnout_in = 8.0   # turnout 30's through route, 110..118
    assert xp["siding_ft"] == _ft(42 + turnout_in + 54)
