"""Every file a tool takes or writes is relative to XTRKCAD_PLANS_DIR.

The tutorial calls tools with bare file names ("hillside_division.yaml",
"hillside_division.xtc", "hillside_operations.yaml"). With the plans folder
set, those are that folder's files, inputs and outputs alike, whatever the
server's own working directory is (a desktop app's isn't obvious).
"""

import os
import shutil
from pathlib import Path

import pytest

from xtrkcad_mcp import server

FIXTURES = Path(__file__).parent / "fixtures"
HILLSIDE_FILES = ["hillside_division.yaml", "hillside_floor_plan.yaml",
                  "hillside_benchwork.yaml", "hillside_track_plan.yaml"]


@pytest.fixture
def plans(tmp_path, monkeypatch):
    folder = tmp_path / "Layouts"
    folder.mkdir()
    for f in HILLSIDE_FILES:
        shutil.copy(FIXTURES / f, folder / f)
    shutil.copy(FIXTURES / "tutorial" / "hillside_operations.yaml", folder)
    elsewhere = tmp_path / "elsewhere"
    elsewhere.mkdir()
    monkeypatch.chdir(elsewhere)                 # the server's working directory
    monkeypatch.setattr(server, "_PLANS_DIR", folder)
    return folder


def test_config_files_are_found_in_the_plans_folder(plans):
    assert server.load_layout_config("hillside_division.yaml")["ready"] is True
    result = server.generate_layout("hillside_division.yaml", "hillside_division.xtc")
    assert Path(result["output_path"]) == plans / "hillside_division.xtc"
    assert server.generate_track_plan("hillside_division.yaml",
                                      "hillside_division.xtc")["written"] is True


def test_outputs_land_in_the_plans_folder(plans):
    server.generate_layout("hillside_division.yaml", "hillside_division.xtc")
    server.write_plan_view("hillside_division.yaml", "plan.svg", level=1)
    server.write_gaps_report("hillside_division.xtc", "gaps.txt")
    server.write_benchwork_report("hillside_division.yaml", "bench.txt")
    for name in ("plan.svg", "gaps.txt", "bench.txt"):
        assert (plans / name).exists(), name
    assert os.listdir(".") == []                 # nothing in the working directory


def test_operations_file_in_the_plans_folder(plans):
    server.generate_layout("hillside_division.yaml", "hillside_division.xtc")
    server.generate_track_plan("hillside_division.yaml", "hillside_division.xtc")
    result = server.make_timetable("hillside_division.xtc", "hillside_operations.yaml",
                                   "timetable.md")
    # No notes yet: the check's error stops it, but the file was found and read.
    assert result["made"] is False and result["errors"]
    assert not (plans / "timetable.md").exists()
