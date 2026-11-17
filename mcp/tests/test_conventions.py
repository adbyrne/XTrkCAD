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
    layer_categories,
    read_convention_notes,
)
from xtrkcad_mcp.parser import parse_file

FIXTURES_DIR = Path(__file__).parent / "fixtures"
CLEAN_FIXTURE = FIXTURES_DIR / "conventions_clean.xtc"

_DEFAULT_LAYERS = ["L1-Main", "L1-Passing", "L1-Service"]
_DEFAULT_GROUPS = [("Mainline", [0]), ("Passing", [1]), ("Service", [2])]


def _xtc_quote(text: str) -> str:
    return '"' + text.replace('"', '""') + '"'


def _layout(tmp_path, notes, layers=None, groups=None, name="t.xtc",
            trackless=(), tracks=None):
    """Write and parse a layout.

    notes: list of (layer, note) where note is a dict (written as a JSON
    Note) or a str (raw JSON Note body, e.g. invalid JSON). A tuple
    (layer, str, "text") writes a plain text note instead.
    layers: layer names, index = position.
    groups: list of (name, [layer indexes]); with the default layers and no
    groups given, each default layer is in its category group.
    Each layer gets one straight track from (0, 20*layer) to
    (100, 20*layer), except those in trackless; tracks (a string of .xtc
    track records) replaces them all.
    """
    if layers is None:
        layers = _DEFAULT_LAYERS
        if groups is None:
            groups = _DEFAULT_GROUPS
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
        x, y = (item[3], item[4]) if len(item) > 4 else (nid - 100, layer * 20)
        lines.append(f"NOTE {nid} {layer} 0 0 {x:.6f} {y:.6f} 0 {op} {_xtc_quote(body)}")
    lines.append("")
    if tracks is not None:
        lines.append(tracks)
        trackless = range(len(layers))
    for tid, _ in enumerate(layers):
        if tid in trackless:
            continue
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


# -- phase 2: Layer Groups ---------------------------------------------------

def test_layer_categories_sources(tmp_path):
    layout = _layout(tmp_path, [], layers=["L1-Main", "L1-CO-Staging", "Helix"],
                     groups=[("Mainline", [0])])
    cats = layer_categories(layout)
    assert cats[0] == ("mainline", "group")
    assert cats[1] == ("staging", "name")
    assert cats[2] == ("mainline", "default")


def test_category_group_name_is_case_insensitive(tmp_path):
    layout = _layout(tmp_path, [], layers=["Helix"], groups=[("staging", [0])])
    assert layer_categories(layout)[0] == ("staging", "group")


def test_non_category_group_does_not_categorize(tmp_path):
    # "Main" is not a reserved name; the layer name decides.
    layout = _layout(tmp_path, [], layers=["L1-Passing"], groups=[("Main", [0])])
    assert layer_categories(layout)[0] == ("passing", "name")


def test_layer_in_two_category_groups(tmp_path):
    issues = _check(tmp_path, [], groups=_DEFAULT_GROUPS + [("Staging", [0])])
    assert _rules(issues) == ["CATEGORY_LAYER_IN_TWO"]
    assert issues[0].layer == 0


def test_track_layer_in_no_category_group(tmp_path):
    issues = _check(tmp_path, [], groups=[("Mainline", [0]), ("Passing", [1])])
    assert _rules(issues) == ["CATEGORY_LAYER_NONE"]
    assert issues[0].severity == "warning" and issues[0].layer == 2


def test_layer_without_track_needs_no_category(tmp_path):
    issues = _check(tmp_path, [], layers=["L1-Main", "Floor"],
                    groups=[("Mainline", [0])], trackless=(1,))
    assert issues == []


def test_railroad_group_unknown(tmp_path):
    issues = _check(tmp_path, [(0, {"kind": "railroad", "id": "NYE", "group": "NYEE"})])
    assert _rules(issues) == ["GROUP_UNKNOWN"]
    assert issues[0].group == "NYEE"


def test_subdivision_group_unknown(tmp_path):
    issues = _check(tmp_path, [(0, {"kind": "railroad", "id": "NYE"}),
                               (0, {"kind": "subdivision", "id": "NLS", "group": "X"})])
    assert _rules(issues) == ["GROUP_UNKNOWN"]


def test_group_name_match_is_case_insensitive(tmp_path):
    issues = _check(tmp_path, [(0, {"kind": "railroad", "id": "NYE", "group": "nye"})],
                    groups=_DEFAULT_GROUPS + [("NYE", [0, 1, 2])])
    assert issues == []


_RR2 = [(0, {"kind": "railroad", "id": "NYE", "group": "NYE"}),
        (0, {"kind": "railroad", "id": "CO", "group": "C&O"})]


def test_two_railroads_cover_all_layers(tmp_path):
    issues = _check(tmp_path, _RR2,
                    groups=_DEFAULT_GROUPS + [("NYE", [0, 2]), ("C&O", [1])])
    assert issues == []


def test_layer_in_two_railroad_groups(tmp_path):
    issues = _check(tmp_path, _RR2,
                    groups=_DEFAULT_GROUPS + [("NYE", [0, 1, 2]), ("C&O", [1])])
    assert _rules(issues) == ["RAILROAD_LAYER_IN_TWO"]
    assert issues[0].layer == 1


def test_layer_in_no_railroad_group(tmp_path):
    issues = _check(tmp_path, _RR2,
                    groups=_DEFAULT_GROUPS + [("NYE", [0]), ("C&O", [1])])
    assert _rules(issues) == ["RAILROAD_LAYER_NONE"]
    assert issues[0].severity == "warning" and issues[0].layer == 2


def test_one_railroad_needs_no_railroad_group(tmp_path):
    assert _check(tmp_path, [(0, {"kind": "railroad", "id": "NYE"})]) == []


def test_ignore_layer_needs_no_railroad(tmp_path):
    issues = _check(tmp_path, _RR2, layers=["L1-Main", "L2-Main", "Benchwork"],
                    groups=[("Mainline", [0, 1]), ("Ignore", [2]),
                            ("NYE", [0]), ("C&O", [1])])
    assert issues == []


def test_layer_in_two_subdivision_groups(tmp_path):
    issues = _check(tmp_path, [
        (0, {"kind": "railroad", "id": "NYE"}),
        (0, {"kind": "subdivision", "id": "A", "group": "SA"}),
        (0, {"kind": "subdivision", "id": "B", "group": "SB"}),
    ], groups=_DEFAULT_GROUPS + [("SA", [0, 1]), ("SB", [1, 2])])
    assert _rules(issues) == ["SUBDIVISION_LAYER_IN_TWO"]
    assert issues[0].layer == 1


def test_subdivisions_may_share_a_group(tmp_path):
    # Spec 4.1: one railroad's subdivisions may share layers.
    issues = _check(tmp_path, [
        (0, {"kind": "railroad", "id": "NYE"}),
        (0, {"kind": "subdivision", "id": "A", "group": "S"}),
        (0, {"kind": "subdivision", "id": "B", "group": "S"}),
    ], groups=_DEFAULT_GROUPS + [("S", [0, 1, 2])])
    assert issues == []


def test_layer_groups_fixture_with_railroads(tmp_path):
    text = (FIXTURES_DIR / "layer_groups.xtc").read_text().replace(
        'NOTE 50 ',
        'NOTE 60 0 0 0 0.000000 2.000000 0 3 '
        '"{""kind"": ""railroad"", ""id"": ""NYE"", ""group"": ""NYE""}"\n'
        'NOTE 61 3 0 0 0.000000 52.000000 0 3 '
        '"{""kind"": ""railroad"", ""id"": ""CO"", ""group"": ""C&O""}"\n'
        'NOTE 50 ', 1)
    path = tmp_path / "lg.xtc"
    path.write_text(text)
    issues = check_conventions(parse_file(path))
    assert _rules(issues, "error") == [] and _rules(issues, "warning") == []


def test_two_railroads_sharing_one_group(tmp_path):
    issues = _check(tmp_path, [(0, {"kind": "railroad", "id": "NYE", "group": "Both"}),
                               (0, {"kind": "railroad", "id": "CO", "group": "Both"})],
                    groups=_DEFAULT_GROUPS + [("Both", [0, 1, 2])])
    assert _rules(issues) == ["RAILROAD_LAYER_IN_TWO"] * 3


def test_braces_in_names_are_safe(tmp_path):
    issues = _check(tmp_path, [(0, {"kind": "railroad", "id": "A{x}", "group": "G{0}"}),
                               (0, {"kind": "railroad", "id": "B", "group": "G{0}"})],
                    layers=["L1-Main{1}"], groups=[("Mainline", [0]), ("G{0}", [0])])
    assert _rules(issues) == ["RAILROAD_LAYER_IN_TWO"]
    assert "L1-Main{1}" in issues[0].message


# -- phase 3: placement ------------------------------------------------------

# Main (layer 0): straight 1 (0,0)-(50,0), turnout 2 at (50,0) with its
# straight leg to (58,0) and diverging leg to (58,5), straight 3 (58,0)-(120,0).
# Spur (layer 2, Service): straight 4 (58,5)-(100,20) off the turnout.
# Isolated spur (layer 2): straight 5 (0,40)-(40,40), connected to nothing.
MAIN_AND_SPUR = """
STRAIGHT 1 0 0 0 0 HO 0
\tE 0.000000 0.000000 270.000000
\tT 2 50.000000 0.000000 90.000000
\tEND$SEGS

TURNOUT 2 0 0 0 0 HO 2 50.000000 0.000000 0 0.000000 "Test\t#6 Right\t"
\tT 1 50.000000 0.000000 270.000000
\tT 3 58.000000 0.000000 90.000000
\tT 4 58.000000 5.000000 70.000000
\tD 0.000000 0.000000
\tP "Normal" 0
\tP "Reverse" 1
\tS 0 0 0.000000 0.000000 8.000000 0.000000
\tS 0 0 0.000000 0.000000 8.000000 5.000000
\tEND$SEGS

STRAIGHT 3 0 0 0 0 HO 0
\tT 2 58.000000 0.000000 270.000000
\tE 120.000000 0.000000 90.000000
\tEND$SEGS

STRAIGHT 4 2 0 0 0 HO 0
\tT 2 58.000000 5.000000 250.000000
\tE 100.000000 20.000000 70.000000
\tEND$SEGS

STRAIGHT 5 2 0 0 0 HO 0
\tE 0.000000 40.000000 270.000000
\tE 40.000000 40.000000 90.000000
\tEND$SEGS
"""

# A closed loop of main track: a 100 x 100 square, no open end.
MAIN_LOOP = """
STRAIGHT 1 0 0 0 0 HO 0
\tT 4 0.000000 0.000000 180.000000
\tT 2 100.000000 0.000000 90.000000
\tEND$SEGS

STRAIGHT 2 0 0 0 0 HO 0
\tT 1 100.000000 0.000000 270.000000
\tT 3 100.000000 100.000000 0.000000
\tEND$SEGS

STRAIGHT 3 0 0 0 0 HO 0
\tT 2 100.000000 100.000000 180.000000
\tT 4 0.000000 100.000000 270.000000
\tEND$SEGS

STRAIGHT 4 0 0 0 0 HO 0
\tT 3 0.000000 100.000000 90.000000
\tT 1 0.000000 0.000000 0.000000
\tEND$SEGS
"""

# A quarter circle of main track: centre (0,0), radius 100, from (100,0)
# to (0,100). Its chord passes 29 in inside the arc's midpoint.
MAIN_CURVE = """
CURVE 1 0 0 0 0 HO 0 0.000000 0.000000 0 100.000000
\tE 100.000000 0.000000 0.000000
\tE 0.000000 100.000000 270.000000
\tEND$SEGS
"""


def _placed(tmp_path, notes, tracks=MAIN_AND_SPUR, **kw):
    return _check(tmp_path, notes, tracks=tracks, **kw)


def _station_at(x, y, layer=0, **fields):
    return (layer, {"kind": "station", "id": "WP", **fields}, None, x, y)


def test_note_on_its_track(tmp_path):
    assert _placed(tmp_path, [_station_at(20, 0.5)]) == []


def test_note_far_from_track(tmp_path):
    issues = _placed(tmp_path, [_station_at(20, 5)])
    assert _rules(issues) == ["NOTE_FAR_FROM_TRACK"]
    assert issues[0].severity == "warning" and issues[0].track_id == 1


def test_note_no_track_nearby(tmp_path):
    issues = _placed(tmp_path, [_station_at(20, 30)])
    assert _rules(issues) == ["NOTE_NO_TRACK"]


def test_note_only_snaps_to_its_own_layer(tmp_path):
    # On top of main track, but the note is on the Passing layer, which has none.
    issues = _placed(tmp_path, [(1, {"kind": "reference", "id": "X"}, None, 20, 0)])
    assert _rules(issues) == ["NOTE_NO_TRACK"]


def test_railroad_and_subdivision_have_no_position(tmp_path):
    issues = _placed(tmp_path, [(0, {"kind": "railroad", "id": "NYE"}, None, 500, 500),
                                (0, {"kind": "subdivision", "id": "S"}, None, 500, 500)])
    assert issues == []


def test_note_on_curve_measured_to_the_arc(tmp_path):
    issues = _placed(tmp_path, [_station_at(70.71, 70.71)], tracks=MAIN_CURVE)
    assert issues == []


def test_note_off_curve(tmp_path):
    issues = _placed(tmp_path, [_station_at(80, 80)], tracks=MAIN_CURVE)
    assert _rules(issues) == ["NOTE_FAR_FROM_TRACK"]


def test_note_on_turnout_diverging_leg(tmp_path):
    assert _placed(tmp_path, [_station_at(54, 2.5)]) == []


@pytest.mark.parametrize("note, rule", [
    ((2, {"kind": "reference", "id": "A", "mp": 0, "mp_scale": 12}, None, 80, 12.86),
     "ANCHOR_NOT_ON_MAIN"),
    ((2, {"kind": "yard_limit", "id": "WP"}, None, 80, 12.86), "YARD_LIMIT_NOT_ON_MAIN"),
    ((2, {"kind": "station", "id": "WP"}, None, 80, 12.86), "STATION_NOT_ON_MAIN"),
])
def test_not_on_main(tmp_path, note, rule):
    assert rule in _rules(_placed(tmp_path, [note]))


def test_station_not_on_main_is_warning(tmp_path):
    issues = _placed(tmp_path, [(2, {"kind": "station", "id": "WP"}, None, 80, 12.86)])
    assert [i.severity for i in issues] == ["warning"]


def test_reference_without_mp_may_be_off_main(tmp_path):
    assert _placed(tmp_path, [(2, {"kind": "reference", "id": "MC_EXIT"}, None, 80, 12.86)]) == []


def _yl(x, y=0.0, ident="WP"):
    return (0, {"kind": "yard_limit", "id": ident}, None, x, y)


def test_two_yard_limit_boards(tmp_path):
    assert _placed(tmp_path, [_yl(10), _yl(100)]) == []


def test_three_yard_limit_boards(tmp_path):
    assert "YARD_LIMIT_TOO_MANY" in _rules(_placed(tmp_path, [_yl(10), _yl(30), _yl(100)]))


def test_one_board_on_main_with_an_end(tmp_path):
    assert _placed(tmp_path, [_yl(100)]) == []


def test_one_board_on_a_loop(tmp_path):
    issues = _placed(tmp_path, [_yl(50)], tracks=MAIN_LOOP)
    assert _rules(issues) == ["YARD_LIMIT_ONE_BOARD"]
    assert issues[0].severity == "warning"


def test_one_board_at_terminus_on_a_loop(tmp_path):
    issues = _placed(tmp_path, [_yl(50), (0, {"kind": "station", "id": "WP", "terminus": True},
                                          None, 20, 0)], tracks=MAIN_LOOP)
    assert issues == []


def _swb(x, y, layer=2, **fields):
    return (layer, {"kind": "switchback", "id": "SWB", **fields}, None, x, y)


def test_switchback_default_switch_found(tmp_path):
    assert _placed(tmp_path, [_swb(80, 12.86)]) == []


def test_switchback_tail_on_main(tmp_path):
    assert "SWITCHBACK_TAIL_ON_MAIN" in _rules(_placed(tmp_path, [_swb(100, 0, layer=0)]))


def test_switchback_isolated_tail_has_no_switch(tmp_path):
    assert _rules(_placed(tmp_path, [_swb(20, 40)])) == ["SWITCHBACK_NO_SWITCH"]


def test_switchback_isolated_tail_with_switches_given(tmp_path):
    assert _placed(tmp_path, [_swb(20, 40, switches=[2])]) == []


@pytest.mark.parametrize("switches", [[1], [99], [2, 4]])
def test_switchback_bad_switch(tmp_path, switches):
    # 1 is a straight, 99 doesn't exist, 4 is the (non-main) spur.
    issues = _placed(tmp_path, [_swb(80, 12.86, switches=switches)])
    assert _rules(issues) == ["SWITCHBACK_BAD_SWITCH"]


def test_hint_names_the_layer_the_note_belongs_on(tmp_path):
    # On the spur (Service layer) but the note is on the Main layer.
    issues = _placed(tmp_path, [(0, {"kind": "reference", "id": "X"}, None, 80, 12.86)])
    assert _rules(issues) == ["NOTE_FAR_FROM_TRACK"]
    assert "L1-Service" in issues[0].hint and "4" in issues[0].hint


# -- house tracks: several per location (user 2026-11-17) --------------------

def _ht(ident="WP", **fields):
    return (2, {"kind": "house_track", "id": ident, **fields})


def test_one_house_track_needs_no_label(tmp_path):
    assert _check(tmp_path, [ST_WP, _ht()]) == []


def test_several_house_tracks_with_labels(tmp_path):
    assert _check(tmp_path, [ST_WP, _ht(label="Freight house"), _ht(label="Team")]) == []


def test_house_track_at_industry(tmp_path):
    issues = _check(tmp_path, [(2, {"kind": "industry", "id": "QM1", "car_spots": 2}),
                               _ht("QM1", label="A"), _ht("QM1", label="B")])
    assert issues == []


@pytest.mark.parametrize("a, b", [({}, {}), ({"label": "T"}, {"label": "T"}),
                                  ({}, {"label": "T"})])
def test_house_tracks_not_told_apart(tmp_path, a, b):
    # Several house tracks at one location: each needs its own label.
    issues = _check(tmp_path, [ST_WP, _ht(**a), _ht(**b)])
    assert set(_rules(issues)) == {"HOUSE_TRACK_LABEL"}
