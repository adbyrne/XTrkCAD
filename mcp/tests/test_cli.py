"""xtrkcad-report command line."""

import sys
from pathlib import Path

import pytest

from xtrkcad_mcp.cli import report_main

GROUPS_FIXTURE = str(Path(__file__).parent / "fixtures" / "layer_groups.xtc")


def _run(monkeypatch, *args):
    monkeypatch.setattr(sys, "argv", ["xtrkcad-report", *args])
    report_main()


def test_report_for_one_group(monkeypatch, tmp_path):
    out = tmp_path / "co.md"
    _run(monkeypatch, GROUPS_FIXTURE, "-g", "C&O", "-f", "md", "-o", str(out))
    text = out.read_text()
    assert "CO-Main" in text
    assert "NYE-Main" not in text


def test_unknown_group_exits_with_error(monkeypatch, tmp_path, capsys):
    with pytest.raises(SystemExit) as exc:
        _run(monkeypatch, GROUPS_FIXTURE, "-g", "Nope", "-o", str(tmp_path / "x.md"))
    assert exc.value.code == 1
    assert "Nope" in capsys.readouterr().err
