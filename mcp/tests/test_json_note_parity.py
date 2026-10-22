"""A layout annotated with JSON Notes must give the same results as the same
layout annotated with the older text-prefix notes (STATION:, INDUSTRY:, ...).

annotations_text.xtc and annotations_json.xtc share their tracks and carry
the same annotations, one as text notes and one as JSON Notes, covering every
annotation kind: station, storage, industry, house_track, yard_track and
reference.
"""

import json
from pathlib import Path

import pytest

from xtrkcad_mcp import server

FIXTURES_DIR = Path(__file__).parent / "fixtures"
TEXT_FIXTURE = str(FIXTURES_DIR / "annotations_text.xtc")
JSON_FIXTURE = str(FIXTURES_DIR / "annotations_json.xtc")

TOOLS = [
    "get_stations",
    "get_station_distances",
    "get_siding_capacities",
    "list_labeled_segments",
    "validate_layout",
]


def _normalized(result):
    """JSON text of a tool result with the fixture name made the same for both.

    Only the file name is swapped, not the full path: in JSON text a Windows
    path's backslashes are escaped, so it wouldn't match the plain path."""
    return json.dumps(result, sort_keys=True, default=str).replace(
        "annotations_json", "annotations_text")


@pytest.mark.parametrize("tool", TOOLS)
def test_json_notes_match_text_notes(tool):
    fn = getattr(server, tool)
    text_result = fn(TEXT_FIXTURE)
    json_result = fn(JSON_FIXTURE)
    assert text_result, f"{tool} found nothing in the text-note fixture"
    assert _normalized(json_result) == _normalized(text_result)


def test_every_annotation_kind_is_found_in_json_notes():
    kinds = {seg["annotation_type"] for seg in server.list_labeled_segments(JSON_FIXTURE)}
    assert {"station", "storage", "industry", "house_track",
            "yard_track", "reference"} <= kinds


def test_json_reference_mp_scale_is_used(tmp_path):
    out_text = tmp_path / "text.md"
    out_json = tmp_path / "json.md"
    server.write_station_distance_report(TEXT_FIXTURE, str(out_text))
    server.write_station_distance_report(JSON_FIXTURE, str(out_json))
    assert out_json.read_text() == out_text.read_text().replace(
        "annotations_text", "annotations_json")
