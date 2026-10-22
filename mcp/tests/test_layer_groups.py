"""Layer Groups (LAYERGROUP records, file VERSION 13): parsing, the
get_layer_groups tool, the layers/groups filter on report tools, and
op-density categories taken from group names.

layer_groups.xtc holds two railroads in one file: NYE on layers 0-2
(main 96in, siding 48in, yard 24in) and C&O on layers 3-4 (main 48in,
fiddle yard 36in). Groups: Mainline (0, 3), Passing (1), Storage (2),
Staging (4), NYE (0-2) and C&O (3-4).
"""

from pathlib import Path

import pytest

from xtrkcad_mcp import server
from xtrkcad_mcp.parser import parse_file

FIXTURES_DIR = Path(__file__).parent / "fixtures"
GROUPS_FIXTURE = str(FIXTURES_DIR / "layer_groups.xtc")


def _write_variant(tmp_path, old, new):
    text = Path(GROUPS_FIXTURE).read_text().replace(old, new)
    path = tmp_path / "variant.xtc"
    path.write_text(text)
    return str(path)


# -- parsing ---------------------------------------------------------------

def test_groups_parsed_in_file_order():
    layout = parse_file(GROUPS_FIXTURE)
    names = [g.name for g in layout.layer_groups]
    assert names == ["Mainline", "Passing", "Storage", "Staging", "NYE", "C&O"]


def test_group_members_are_layer_indexes():
    # The file stores members as layer index + 1; the parser gives indexes.
    groups = {g.name: g.layers for g in parse_file(GROUPS_FIXTURE).layer_groups}
    assert groups["Mainline"] == [0, 3]
    assert groups["NYE"] == [0, 1, 2]
    assert groups["C&O"] == [3, 4]


def test_groups_ignored_before_version_13(tmp_path):
    # XTrkCAD itself ignores LAYERGROUP records in older files.
    path = _write_variant(tmp_path, "VERSION 13 5.4.0", "VERSION 12 5.2.0")
    assert parse_file(path).layer_groups == []


def test_group_defined_out_of_order_is_skipped(tmp_path):
    # XTrkCAD only accepts DEFINE indexes in sequence (0, 1, 2, ...).
    path = _write_variant(tmp_path, 'LAYERGROUP DEFINE 1 "Passing"',
                          'LAYERGROUP DEFINE 9 "Passing"')
    names = [g.name for g in parse_file(path).layer_groups]
    assert "Passing" not in names


# -- tools -----------------------------------------------------------------

def test_get_layer_groups_tool():
    groups = server.get_layer_groups(GROUPS_FIXTURE)
    co = next(g for g in groups if g["name"] == "C&O")
    assert co["layers"] == [{"index": 3, "name": "CO-Main"},
                            {"index": 4, "name": "CO-Fiddle"}]


def test_layout_summary_lists_groups():
    summary = server.get_layout_summary(GROUPS_FIXTURE)
    assert summary["layer_group_count"] == 6
    assert [g["name"] for g in summary["layer_groups"]][:2] == ["Mainline", "Passing"]


@pytest.mark.parametrize("kwargs, total_in", [
    ({}, 252.0),
    ({"groups": ["NYE"]}, 168.0),
    ({"groups": ["C&O"]}, 84.0),
    ({"layers": ["CO-Main"]}, 48.0),
    ({"layers": ["3"]}, 48.0),
    ({"layers": ["NYE-Yard"], "groups": ["C&O"]}, 108.0),
])
def test_track_lengths_filtered(kwargs, total_in):
    assert server.get_track_lengths(GROUPS_FIXTURE, **kwargs)["total_model_in"] == total_in


def test_unknown_group_names_the_available_ones():
    with pytest.raises(ValueError, match="C&O"):
        server.get_track_lengths(GROUPS_FIXTURE, groups=["Nope"])


def test_unknown_layer_is_an_error():
    with pytest.raises(ValueError, match="Nope"):
        server.get_track_lengths(GROUPS_FIXTURE, layers=["Nope"])


def test_stations_scoped_to_group():
    names = {s["name"] for s in server.get_stations(GROUPS_FIXTURE, groups=["C&O"])}
    assert names == {"CA", "CB"}


def test_station_distances_scoped_to_group():
    pairs = server.get_station_distances(GROUPS_FIXTURE, groups=["NYE"])
    assert [(p["from"], p["to"]) for p in pairs] == [("NA", "NB")]


def test_report_scoped_to_group(tmp_path):
    out = tmp_path / "co.md"
    server.write_layout_report(GROUPS_FIXTURE, str(out), groups=["C&O"])
    text = out.read_text()
    assert "CO-Main" in text
    assert "NYE-Main" not in text


# -- op-density categories from groups -------------------------------------

def test_op_density_categories_from_group_names():
    by_cat = server.get_operation_density(GROUPS_FIXTURE)["by_category"]
    assert by_cat["mainline"]["length_real_ft"] == 12.0   # NYE-Main + CO-Main
    assert by_cat["passing"]["length_real_ft"] == 4.0
    assert by_cat["storage"]["length_real_ft"] == 2.0
    assert by_cat["staging"]["length_real_ft"] == 3.0


def test_op_density_explicit_categories_override_groups():
    by_cat = server.get_operation_density(
        GROUPS_FIXTURE, layer_categories={"CO-Fiddle": "storage"})["by_category"]
    assert "staging" not in by_cat
    assert by_cat["storage"]["length_real_ft"] == 5.0


def test_op_density_scoped_to_one_railroad():
    by_cat = server.get_operation_density(GROUPS_FIXTURE, groups=["C&O"])["by_category"]
    assert by_cat["mainline"]["length_real_ft"] == 4.0
    assert by_cat["staging"]["length_real_ft"] == 3.0
    assert "passing" not in by_cat
