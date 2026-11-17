"""check_conventions: the Operating Data Conventions v1 checks.

Each test builds a tiny layout inline (see _layout) and asserts rule ids,
never message text. conventions_clean.xtc is a valid layout with every note
kind: it must give no errors and no warnings.
"""

import json
from pathlib import Path

import pytest

from xtrkcad_mcp import server
from xtrkcad_mcp.conventions import (
    CONVENTIONS_VERSION,
    check_conventions,
    read_convention_notes,
)
from xtrkcad_mcp.parser import parse_file

FIXTURES_DIR = Path(__file__).parent / "fixtures"
CLEAN_FIXTURE = FIXTURES_DIR / "conventions_clean.xtc"

_DEFAULT_LAYERS = ["L1-Main", "L1-Passing", "L1-Service"]


def _xtc_quote(text: str) -> str:
    return '"' + text.replace('"', '""') + '"'


def _layout(tmp_path, notes, layers=None, groups=None, name="t.xtc"):
    """Write and parse a layout.

    notes: list of (layer, note) where note is a dict (written as a JSON
    Note) or a str (raw JSON Note body, e.g. invalid JSON). A tuple
    (layer, str, "text") writes a plain text note instead.
    layers: layer names, index = position.
    groups: list of (name, [layer indexes]).
    Each layer gets one straight track so it is a track layer.
    """
    layers = layers if layers is not None else _DEFAULT_LAYERS
    lines = ["VERSION 13 5.4.0", "TITLE1 conventions test", "SCALE HO",
             "ROOMSIZE 200 x 100", ""]
    for idx, lname in enumerate(layers):
        lines.append(f'LAYERS {idx} 1 0 0 0 0 0 0 0 "{lname}"')
    lines.append("LAYERS CURRENT 0")
    for gidx, (gname, members) in enumerate(groups or []):
        lines.append(f'LAYERGROUP DEFINE {gidx} "{gname}"')
        lines.append(f'LAYERGROUP MEMBERS {gidx} '
                     f'"{";".join(str(m + 1) for m in members)}"')
    lines.append("")
    for nid, item in enumerate(notes, start=100):
        layer, body = item[0], item[1]
        op = 0 if len(item) > 2 and item[2] == "text" else 3
        if isinstance(body, dict):
            body = json.dumps(body)
        lines.append(f"NOTE {nid} {layer} 0 0 {nid - 100}.000000 "
                     f"{layer * 20}.000000 0 {op} {_xtc_quote(body)}")
    lines.append("")
    for tid, _ in enumerate(layers):
        y = tid * 20
        lines += [f"STRAIGHT {tid + 1} {tid} 0 0 0 HO 0",
                  f"\tE 0.000000 {y}.000000 270.000000",
                  f"\tE 100.000000 {y}.000000 90.000000",
                  "\tEND$SEGS", ""]
    path = tmp_path / name
    path.write_text("\n".join(lines))
    return parse_file(path)


def _rules(issues, severity=None):
    return [i.rule for i in issues if severity is None or i.severity == severity]


def _check(tmp_path, notes, **kw):
    return check_conventions(_layout(tmp_path, notes, **kw))


ST_WP = (0, {"kind": "station", "id": "WP"})
ST_XP = (0, {"kind": "station", "id": "XP"})


# -- which notes count -------------------------------------------------------

def test_clean_fixture_has_no_errors_or_warnings():
    issues = check_conventions(parse_file(CLEAN_FIXTURE))
    assert _rules(issues, "error") == []
    assert _rules(issues, "warning") == []


def test_json_note_without_kind_is_ignored(tmp_path):
    notes, issues = read_convention_notes(
        _layout(tmp_path, [(0, {"todo": "paint the bridge"})]))
    assert notes == [] and issues == []


def test_plain_text_note_is_ignored(tmp_path):
    assert _check(tmp_path, [(0, "fix the fascia", "text")]) == []


def test_invalid_json_with_kind(tmp_path):
    issues = _check(tmp_path, [(0, '{"kind": "station", "id": "WP",}')])
    assert _rules(issues) == ["NOTE_INVALID_JSON"]


def test_invalid_json_without_kind_is_ignored(tmp_path):
    assert _check(tmp_path, [(0, "{not json at all")]) == []


def test_text_prefix_note_is_info(tmp_path):
    issues = _check(tmp_path, [(0, "STATION: WP !TERM", "text")])
    assert _rules(issues) == ["NOTE_TEXT_PREFIX"]
    assert issues[0].severity == "info"


def test_unknown_kind(tmp_path):
    issues = _check(tmp_path, [(0, {"kind": "stationn", "id": "WP"})])
    assert _rules(issues) == ["NOTE_UNKNOWN_KIND"]


@pytest.mark.parametrize("note", [
    {"kind": "station"},
    {"kind": "station", "id": ""},
    {"kind": "station", "id": 7},
])
def test_missing_id(tmp_path, note):
    assert _rules(_check(tmp_path, [(0, note)])) == ["NOTE_MISSING_ID"]


@pytest.mark.parametrize("note", [
    {"kind": "station", "id": "WP", "terminus": "yes"},
    {"kind": "station", "id": "WP", "mp_at_note": 1},
    {"kind": "reference", "id": "MP_ZERO", "mp": "0", "mp_scale": 12},
    {"kind": "reference", "id": "MP_ZERO", "mp": 0, "mp_scale": -1},
    {"kind": "industry", "id": "QM1", "car_spots": 0},
    {"kind": "industry", "id": "QM1", "car_spots": 2.5},
    {"kind": "industry", "id": "QM1", "car_types": "HM"},
    {"kind": "industry", "id": "QM1", "loads_out": [1]},
    {"kind": "station", "id": "WP", "name": 5},
])
def test_field_type(tmp_path, note):
    assert "NOTE_FIELD_TYPE" in _rules(_check(tmp_path, [(0, note)]))


def test_read_notes_gives_fields(tmp_path):
    notes, _ = read_convention_notes(_layout(tmp_path, [
        (1, {"kind": "industry", "id": "QM1", "car_spots": 3})]))
    assert len(notes) == 1
    n = notes[0]
    assert (n.kind, n.id, n.layer, n.fields["car_spots"]) == ("industry", "QM1", 1, 3)


# -- ids ---------------------------------------------------------------------

def test_location_ids_share_one_namespace(tmp_path):
    issues = _check(tmp_path, [ST_WP, (2, {"kind": "industry", "id": "WP"})])
    assert "ID_DUPLICATE_LOCATION" in _rules(issues)


def test_staging_shares_location_namespace(tmp_path):
    issues = _check(tmp_path, [(2, {"kind": "industry", "id": "E"}),
                               (2, {"kind": "staging", "id": "E"})])
    assert "ID_DUPLICATE_LOCATION" in _rules(issues)


def test_duplicate_reference(tmp_path):
    ref = (0, {"kind": "reference", "id": "MC_EXIT"})
    assert "ID_DUPLICATE" in _rules(_check(tmp_path, [ref, ref]))


def test_same_id_in_two_kinds_outside_locations_is_fine(tmp_path):
    # Non-location kinds have their own namespaces.
    issues = _check(tmp_path, [ST_WP, (2, {"kind": "house_track", "id": "WP"}),
                               (2, {"kind": "storage", "id": "WP"})])
    assert _rules(issues, "error") == []


def test_yard_limit_and_yard_track_group_by_id(tmp_path):
    issues = _check(tmp_path, [
        ST_WP,
        (0, {"kind": "yard_limit", "id": "WP"}),
        (0, {"kind": "yard_limit", "id": "WP"}),
        (2, {"kind": "yard_track", "id": "WP", "label": "T1", "function": "lead"}),
        (2, {"kind": "yard_track", "id": "WP", "label": "T2", "function": "lead"}),
    ])
    assert _rules(issues, "error") == []


# -- joint station -----------------------------------------------------------

_TWO_RR_LAYERS = ["L1-NYE-Main", "L2-CO-Main", "L1-Service"]
_TWO_RR_GROUPS = [("Mainline", [0, 1]), ("Service", [2]),
                  ("NYE", [0, 2]), ("C&O", [1])]
_TWO_RR_NOTES = [
    (0, {"kind": "railroad", "id": "NYE", "group": "NYE"}),
    (0, {"kind": "railroad", "id": "CO", "group": "C&O"}),
]


def test_joint_station_in_two_subdivisions_is_fine(tmp_path):
    issues = _check(tmp_path, _TWO_RR_NOTES + [
        (0, {"kind": "station", "id": "WP", "name": "Williamsport"}),
        (1, {"kind": "station", "id": "WP"}),
    ], layers=_TWO_RR_LAYERS, groups=_TWO_RR_GROUPS)
    assert _rules(issues, "error") == []


def test_joint_station_ownership_from_groups(tmp_path):
    layout = _layout(tmp_path, _TWO_RR_NOTES + [
        (0, {"kind": "station", "id": "WP"}),
        (1, {"kind": "station", "id": "WP"}),
    ], layers=_TWO_RR_LAYERS, groups=_TWO_RR_GROUPS)
    notes, _ = read_convention_notes(layout)
    owners = sorted(n.railroad for n in notes if n.kind == "station")
    assert owners == ["CO", "NYE"]


def test_joint_station_ownership_from_field(tmp_path):
    # No groups: the C&O note says its railroad itself.
    layout = _layout(tmp_path, _TWO_RR_NOTES[:1] + [
        (0, {"kind": "railroad", "id": "CO"}),
        (0, {"kind": "station", "id": "WP"}),
        (0, {"kind": "station", "id": "WP", "railroad": "CO"}),
    ], layers=_TWO_RR_LAYERS, groups=[("NYE", [0, 2])])
    issues = check_conventions(layout)
    assert "STATION_JOINT_SAME_SUBDIVISION" not in _rules(issues)


def test_two_station_notes_in_one_subdivision(tmp_path):
    issues = _check(tmp_path, [ST_WP, ST_WP])
    assert "STATION_JOINT_SAME_SUBDIVISION" in _rules(issues)
    assert "ID_DUPLICATE_LOCATION" not in _rules(issues)


def test_joint_station_name_mismatch(tmp_path):
    issues = _check(tmp_path, _TWO_RR_NOTES + [
        (0, {"kind": "station", "id": "WP", "name": "Williamsport"}),
        (1, {"kind": "station", "id": "WP", "name": "Williamsport Jct"}),
    ], layers=_TWO_RR_LAYERS, groups=_TWO_RR_GROUPS)
    assert _rules(issues, "warning") == ["STATION_JOINT_NAME_MISMATCH"]


def test_joint_station_name_on_one_note_is_fine(tmp_path):
    issues = _check(tmp_path, _TWO_RR_NOTES + [
        (0, {"kind": "station", "id": "WP", "name": "Williamsport"}),
        (1, {"kind": "station", "id": "WP"}),
    ], layers=_TWO_RR_LAYERS, groups=_TWO_RR_GROUPS)
    assert "STATION_JOINT_NAME_MISMATCH" not in _rules(issues)


# -- references --------------------------------------------------------------

def test_unknown_railroad_with_no_railroad_notes(tmp_path):
    issues = _check(tmp_path, [(0, {"kind": "station", "id": "WP", "railroad": "CO"})])
    assert _rules(issues) == ["REF_UNKNOWN_RAILROAD"]


def test_unknown_railroad(tmp_path):
    issues = _check(tmp_path, [(0, {"kind": "railroad", "id": "NYE"}),
                               (0, {"kind": "station", "id": "WP", "railroad": "CO"})])
    assert _rules(issues) == ["REF_UNKNOWN_RAILROAD"]


def test_subdivision_unknown_railroad(tmp_path):
    issues = _check(tmp_path, [(0, {"kind": "railroad", "id": "NYE"}),
                               (0, {"kind": "subdivision", "id": "NLS", "railroad": "X"})])
    assert "REF_UNKNOWN_RAILROAD" in _rules(issues)


def test_unknown_subdivision(tmp_path):
    issues = _check(tmp_path, [(0, {"kind": "station", "id": "WP", "subdivision": "NLS"})])
    assert _rules(issues) == ["REF_UNKNOWN_SUBDIVISION"]


def test_subdivision_needs_railroad_when_several(tmp_path):
    issues = _check(tmp_path, [(0, {"kind": "railroad", "id": "NYE"}),
                               (0, {"kind": "railroad", "id": "CO"}),
                               (0, {"kind": "subdivision", "id": "NLS"})])
    assert "SUBDIVISION_NO_RAILROAD" in _rules(issues)


def test_subdivision_railroad_defaults_with_one_railroad(tmp_path):
    issues = _check(tmp_path, [(0, {"kind": "railroad", "id": "NYE"}),
                               (0, {"kind": "subdivision", "id": "NLS"})])
    assert _rules(issues, "error") == []


@pytest.mark.parametrize("note", [
    (2, {"kind": "industry", "id": "QM1", "station": "XX"}),
    (2, {"kind": "yard_track", "id": "XX", "label": "T1", "function": "lead"}),
    (2, {"kind": "house_track", "id": "XX"}),
    (2, {"kind": "facility", "id": "F", "type": "coal", "location": "XX"}),
    (2, {"kind": "switchback", "id": "S", "serves": "XX"}),
])
def test_unknown_location(tmp_path, note):
    assert "REF_UNKNOWN_LOCATION" in _rules(_check(tmp_path, [ST_WP, note]))


def test_house_track_at_staging_is_unknown_location(tmp_path):
    # A house track belongs to a station or an industry.
    issues = _check(tmp_path, [(2, {"kind": "staging", "id": "E"}),
                               (2, {"kind": "house_track", "id": "E"})])
    assert "REF_UNKNOWN_LOCATION" in _rules(issues)


def test_industry_station_must_be_station(tmp_path):
    issues = _check(tmp_path, [(2, {"kind": "staging", "id": "E"}),
                               (2, {"kind": "industry", "id": "QM1", "station": "E"})])
    assert "INDUSTRY_STATION_NOT_STATION" in _rules(issues)


def test_station_ref_tag_unknown(tmp_path):
    issues = _check(tmp_path, [(0, {"kind": "station", "id": "WP", "ref_tag": "NOPE"})])
    assert _rules(issues) == ["REF_UNKNOWN_REFERENCE"]


def test_station_ref_tag_known(tmp_path):
    issues = _check(tmp_path, [(0, {"kind": "reference", "id": "MC_EXIT"}),
                               (0, {"kind": "station", "id": "WP", "ref_tag": "MC_EXIT"})])
    assert issues == []


# -- yard tracks -------------------------------------------------------------

def test_yard_track_missing_label(tmp_path):
    issues = _check(tmp_path, [ST_WP, (2, {"kind": "yard_track", "id": "WP",
                                           "function": "lead"})])
    assert _rules(issues) == ["YARD_TRACK_MISSING_LABEL"]


def test_yard_track_duplicate_label(tmp_path):
    t = (2, {"kind": "yard_track", "id": "WP", "label": "T1", "function": "lead"})
    assert _rules(_check(tmp_path, [ST_WP, t, t])) == ["YARD_TRACK_DUPLICATE_LABEL"]


def test_same_label_in_two_yards_is_fine(tmp_path):
    issues = _check(tmp_path, [
        ST_WP, ST_XP,
        (2, {"kind": "yard_track", "id": "WP", "label": "T1", "function": "lead"}),
        (2, {"kind": "yard_track", "id": "XP", "label": "T1", "function": "lead"})])
    assert issues == []


def test_yard_track_no_function_is_info(tmp_path):
    issues = _check(tmp_path, [ST_WP, (2, {"kind": "yard_track", "id": "WP", "label": "T1"})])
    assert _rules(issues) == ["YARD_TRACK_NO_FUNCTION"]
    assert issues[0].severity == "info"


@pytest.mark.parametrize("loc, function, ok", [
    ({"kind": "station", "id": "L"}, "classification", True),
    ({"kind": "station", "id": "L"}, "loading", False),
    ({"kind": "industry", "id": "L"}, "loading", True),
    ({"kind": "industry", "id": "L"}, "classification", False),
    ({"kind": "staging", "id": "L"}, "staging", True),
    ({"kind": "staging", "id": "L"}, "caboose", True),   # staging takes yard functions
    ({"kind": "staging", "id": "L"}, "loading", False),
    ({"kind": "station", "id": "L"}, "staging", False),
    ({"kind": "station", "id": "L"}, "nonsense", False),
])
def test_yard_track_function_by_yard_kind(tmp_path, loc, function, ok):
    issues = _check(tmp_path, [
        (2, loc), (2, {"kind": "yard_track", "id": "L", "label": "T", "function": function})])
    assert ("YARD_TRACK_BAD_FUNCTION" not in _rules(issues)) == ok


def test_interchange_needs_two_railroads(tmp_path):
    rr = [(0, {"kind": "railroad", "id": "NYE"}), (0, {"kind": "railroad", "id": "CO"})]
    base = rr + [ST_WP]
    one = (2, {"kind": "yard_track", "id": "WP", "label": "IC",
               "function": "interchange", "railroads": ["NYE"]})
    none = (2, {"kind": "yard_track", "id": "WP", "label": "IC",
                "function": "interchange"})
    two = (2, {"kind": "yard_track", "id": "WP", "label": "IC",
               "function": "interchange", "railroads": ["NYE", "CO"]})
    assert "INTERCHANGE_RAILROADS" in _rules(_check(tmp_path, base + [one]))
    assert "INTERCHANGE_RAILROADS" in _rules(_check(tmp_path, base + [none]))
    assert _rules(_check(tmp_path, base + [two]), "error") == []


def test_interchange_unknown_railroad(tmp_path):
    issues = _check(tmp_path, [
        (0, {"kind": "railroad", "id": "NYE"}), ST_WP,
        (2, {"kind": "yard_track", "id": "WP", "label": "IC",
             "function": "interchange", "railroads": ["NYE", "PRR"]})])
    assert "REF_UNKNOWN_RAILROAD" in _rules(issues)


def test_staging_interchange_track_is_fine(tmp_path):
    issues = _check(tmp_path, [
        (0, {"kind": "railroad", "id": "NYE"}), (0, {"kind": "railroad", "id": "CO"}),
        (2, {"kind": "staging", "id": "E", "railroad": "CO"}),
        (2, {"kind": "yard_track", "id": "E", "label": "IC",
             "function": "interchange", "railroads": ["NYE", "CO"]})])
    assert _rules(issues, "error") == []


# -- industries, facilities, references --------------------------------------

def test_industry_car_spots_default_is_info(tmp_path):
    issues = _check(tmp_path, [(2, {"kind": "industry", "id": "QM1"})])
    assert _rules(issues) == ["INDUSTRY_CAR_SPOTS_DEFAULT"]
    assert issues[0].severity == "info"


def test_industry_unknown_car_type(tmp_path):
    issues = _check(tmp_path, [(2, {"kind": "industry", "id": "QM1", "car_spots": 2,
                                    "car_types": ["HM", "ZZ"]})])
    assert _rules(issues) == ["INDUSTRY_UNKNOWN_CAR_TYPE"]
    assert issues[0].severity == "warning"


@pytest.mark.parametrize("note, ok", [
    ({"kind": "facility", "id": "F", "type": "coal"}, True),
    ({"kind": "facility", "id": "F", "type": "coaling"}, False),
    ({"kind": "facility", "id": "F"}, False),
])
def test_facility_type(tmp_path, note, ok):
    assert ("FACILITY_BAD_TYPE" not in _rules(_check(tmp_path, [(2, note)]))) == ok


@pytest.mark.parametrize("unit, ok", [("in", True), ("ft", True), ("cm", True),
                                      ("m", True), ("mi", False), ("", False)])
def test_reference_mp_unit(tmp_path, unit, ok):
    issues = _check(tmp_path, [(0, {"kind": "reference", "id": "MP_ZERO", "mp": 0,
                                    "mp_scale": 12, "mp_unit": unit})])
    assert ("REFERENCE_BAD_UNIT" not in _rules(issues)) == ok


def test_unknown_fields_are_kept_and_ignored(tmp_path):
    layout = _layout(tmp_path, [(0, {"kind": "station", "id": "WP", "iot_code": "W"})])
    notes, issues = read_convention_notes(layout)
    assert issues == [] and notes[0].fields["iot_code"] == "W"


# -- ordering and tool -------------------------------------------------------

def test_issues_sorted_by_severity(tmp_path):
    issues = _check(tmp_path, [
        (2, {"kind": "industry", "id": "QM1", "car_types": ["ZZ"]}),   # info + warning
        (0, {"kind": "bogus", "id": "B"}),                             # error
    ])
    sev = [i.severity for i in issues]
    assert sev == sorted(sev, key=["error", "warning", "info"].index)


def test_issue_carries_note_position(tmp_path):
    issues = _check(tmp_path, [(0, {"kind": "bogus", "id": "B"})])
    d = issues[0].as_dict()
    assert d["rule"] == "NOTE_UNKNOWN_KIND" and d["note_id"] == 100
    assert d["layer"] == 0 and (d["x"], d["y"]) == (0.0, 0.0)


def test_tool_shape(tmp_path):
    _layout(tmp_path, [(0, {"kind": "bogus", "id": "B"}),
                       (2, {"kind": "industry", "id": "QM1"})])
    result = server.check_conventions(str(tmp_path / "t.xtc"))
    assert result["conventions_version"] == CONVENTIONS_VERSION == 1
    assert result["counts"] == {"error": 1, "warning": 0, "info": 1}
    assert [i["rule"] for i in result["issues"]] == [
        "NOTE_UNKNOWN_KIND", "INDUSTRY_CAR_SPOTS_DEFAULT"]


def test_tool_clean_fixture():
    result = server.check_conventions(str(CLEAN_FIXTURE))
    assert result["counts"]["error"] == 0 and result["counts"]["warning"] == 0
