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
