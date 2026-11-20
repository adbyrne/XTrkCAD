"""The operating-conventions tutorial's Hillside layout, chapter by chapter.

The tutorial (mcp/docs/operating-conventions-tutorial.md) adds notes to the
Hillside layout from the layout config guide and quotes what the tools
report. fixtures/tutorial/hillside_notes.yaml holds those notes and the
mistakes the tutorial shows; this test builds each state and checks the
check's findings and the export values the tutorial quotes, so the text
can't drift from the tools.
"""

import json
import re
from pathlib import Path

import pytest
import yaml

from xtrkcad_mcp.conventions import check_conventions
from xtrkcad_mcp.export_v2 import build_export_v2
from xtrkcad_mcp.parser import parse_file
from xtrkcad_mcp.server import generate_layout, generate_track_plan

FIXTURES_DIR = Path(__file__).parent / "fixtures"
HILLSIDE = FIXTURES_DIR / "hillside_division.yaml"
PLAN = yaml.safe_load((FIXTURES_DIR / "tutorial" / "hillside_notes.yaml").read_text())
CHAPTERS = sorted(PLAN["chapters"])


@pytest.fixture(scope="module")
def base(tmp_path_factory):
    """Hillside with its track, before any notes (the end of Part 1)."""
    out = tmp_path_factory.mktemp("tutorial") / "hillside.xtc"
    generate_layout(str(HILLSIDE), str(out))
    assert generate_track_plan(str(HILLSIDE), str(out))["written"] is True
    return out


def _layers(path):
    return {info.name: idx for idx, info in parse_file(path).layers.items()}


def _note_line(nid, layer, at, note):
    body = json.dumps(note).replace('"', '""')
    return f'NOTE {nid} {layer} 0 0 {at[0]:.6f} {at[1]:.6f} 0 3 "{body}"'


def _notes_until(chapter):
    return [n for ch in CHAPTERS if ch <= chapter for n in PLAN["chapters"][ch]["notes"]]


def _state(base, tmp_path, chapter, add=None, drop=None, groups=None):
    """The layout after ``chapter``, with an example's changes."""
    layers = _layers(base)
    notes = [n for n in _notes_until(chapter)
             if not drop or not all(n["note"].get(k) == v for k, v in drop.items())]
    if add:
        notes.append(add)
    text = base.read_text(encoding="utf-8")
    if groups:
        for name, members in groups.items():
            gidx = re.search(rf'^LAYERGROUP DEFINE (\d+) "{re.escape(name)}"$', text, re.M)
            ids = ";".join(str(layers[m] + 1) for m in members)       # 1-based
            text = re.sub(rf'^LAYERGROUP MEMBERS {gidx.group(1)} ".*"$',
                          f'LAYERGROUP MEMBERS {gidx.group(1)} "{ids}"', text, flags=re.M)
    lines = [_note_line(900 + i, layers[n["layer"]], n["at"], n["note"])
             for i, n in enumerate(notes)]
    path = tmp_path / f"ch{chapter}.xtc"
    path.write_text(text + "\n".join(lines) + "\n", encoding="utf-8")
    return parse_file(path)


def test_part_1_leaves_one_error(base, tmp_path):
    """Chapter 2's starting point: track, Layer Groups, no notes."""
    issues = check_conventions(parse_file(base))
    assert [(i.severity, i.rule) for i in issues] == [("error", "MP_NO_ANCHOR")]


@pytest.mark.parametrize("chapter", CHAPTERS)
def test_each_chapter_leaves_the_layout_clean(base, tmp_path, chapter):
    issues = check_conventions(_state(base, tmp_path, chapter))
    assert [(i.severity, i.rule, i.message) for i in issues] == []


@pytest.mark.parametrize("example", PLAN["examples"], ids=lambda e: e["name"])
def test_examples_give_the_findings_shown(base, tmp_path, example):
    layout = _state(base, tmp_path, example["chapter"], example.get("add"),
                    example.get("drop"), example.get("groups"))
    assert [i.rule for i in check_conventions(layout)] == example["expect"]


@pytest.fixture(scope="module")
def export(base, tmp_path_factory):
    return build_export_v2(_state(base, tmp_path_factory.mktemp("export"), CHAPTERS[-1]))


def _station(export, ident):
    return next(s for s in export["stations"] if s["id"] == ident)


def test_quoted_station_mileposts(export):
    """Chapter 5's table."""
    assert [(s["id"], s["mp"]) for s in export["stations"]] == [
        ("HY", 0.0), ("AL", 3.375), ("BI", 13.643), ("CE", 21.521)]
    assert (_station(export, "AL")["mp_entry"], _station(export, "AL")["mp_exit"]) == (
        0.963, 5.786)
    assert _station(export, "AL")["siding_cars"] == 5
    assert _station(export, "CE")["terminus"] is True


def test_quoted_yard_limits(export):
    """Chapter 6: Cedar's board moves its zone's start out to MP 20.261."""
    ce = next(z for z in export["yard_limits"] if z["id"] == "CE")
    assert (ce["mp_start"], ce["mp_end"], ce["source"]) == (20.261, 21.521, "board")


def test_quoted_capacities(export):
    """Chapters 7-9 (chapter 8: Track 2 falls just short of six cars)."""
    spots = {i["id"]: (i["car_spots"], i["spur_capacity_cars"])
             for i in export["industries"] + export["team_tracks"]}
    assert spots == {"AL_FEED": (2, 3), "AL_TEAM": (2, 2), "BI_LUMBER": (3, 4)}
    hy = next(y for y in export["yards"] if y["id"] == "HY")
    assert [(t["label"], t["capacity_cars"]) for t in hy["tracks"]] == [
        ("Track 1", 8), ("Track 2", 5), ("Track 3", 6)]
    coal = export["facilities"][0]
    assert (coal["id"], coal["car_spots"], coal["capacity_cars"]) == ("CE_COAL", 3, 3)


def test_quoted_railroad(base, tmp_path, export):
    """Chapter 11: no railroad until its note, then HD everywhere."""
    before = build_export_v2(_state(base, tmp_path, 9))
    assert before["railroads"] == [] and _station(before, "AL")["railroad"] is None
    assert [r["id"] for r in export["railroads"]] == ["HD"]
    assert {s["railroad"] for s in export["stations"] + export["yard_limits"]} == {"HD"}


def test_quoted_segments(export):
    """Chapter 12's table."""
    assert [(s["from_station"], s["to_station"], s["length_ft"])
            for s in export["segments"]] == [
        ("HY", "AL", 293.932), ("AL", "BI", 894.367), ("BI", "CE", 686.211)]


def test_quoted_export_refusal(base, tmp_path):
    """Chapter 12: an error stops the export, and the result says why."""
    from xtrkcad_mcp.server import export_layout_data
    _state(base, tmp_path, CHAPTERS[-1], add={
        "layer": "L1-Passing", "at": [14.0, 109.5],
        "note": {"kind": "yard_limit", "id": "AL"}})
    out = tmp_path / "layout_data.json"
    result = export_layout_data(str(tmp_path / f"ch{CHAPTERS[-1]}.xtc"), str(out))
    assert result["written"] is False and not out.exists()
    assert result["counts"] == {"error": 1, "warning": 0, "info": 0}
    assert [(e["rule"], e["message"]) for e in result["errors"]] == [
        ("YARD_LIMIT_NOT_ON_MAIN", "Yard-limit board AL is not on main track")]
    assert result["notices"] == [
        "Not written: fix the errors (check_conventions lists them all), or pass force=True."]


def test_quoted_timetable(base, tmp_path):
    """Chapter 13: the Hillside Local, out to Cedar and back."""
    from xtrkcad_mcp.server import make_timetable
    _state(base, tmp_path, CHAPTERS[-1])
    out = tmp_path / "timetable.md"
    r = make_timetable(str(tmp_path / f"ch{CHAPTERS[-1]}.xtc"),
                       str(FIXTURES_DIR / "tutorial" / "hillside_operations.yaml"), str(out))
    assert r["made"] is True
    assert r["table"].splitlines() == [
        "| Station | MP | 1 out | 1 back |",
        "|---|---|---|---|",
        "| HY Hillside | 0.0 | 07:00 | 10:15 |",
        "| AL Alder | 3.375 | 07:12 – 07:42 | 10:03 |",
        "| BI Birch | 13.643 | 08:12 – 08:37 | 09:33 |",
        "| CE Cedar | 21.521 | 09:00 | 09:10 |"]
    assert r["table"] in TUTORIAL.read_text(encoding="utf-8")
    t = r["trains"][0]
    assert (t["fast_minutes"], t["real_minutes"]) == (195, 65.0)
    assert t["model_mph"] == {"max": 1.5, "yard_limit": 0.7}
    assert out.read_text(encoding="utf-8") == r["table"] + "\n"
    # The MP that would make 30 mph look like 30 mph on the 3:1 clock: about 20 ft.
    assert round(30 * 5280 * 12 / 87.1 / (30 * 3) / 12, 1) == 20.2


def test_timetable_waits_for_a_clean_check(base, tmp_path):
    from xtrkcad_mcp.server import make_timetable
    _state(base, tmp_path, CHAPTERS[-1], add={
        "layer": "L1-Passing", "at": [14.0, 109.5],
        "note": {"kind": "yard_limit", "id": "AL"}})
    r = make_timetable(str(tmp_path / f"ch{CHAPTERS[-1]}.xtc"),
                       str(FIXTURES_DIR / "tutorial" / "hillside_operations.yaml"))
    assert r["made"] is False and [e["rule"] for e in r["errors"]] == ["YARD_LIMIT_NOT_ON_MAIN"]


TUTORIAL = Path(__file__).parents[1] / "docs" / "operating-conventions-tutorial.md"
_FINDING = re.compile(r"^(error|warning|info) ([A-Z_]+)$")


def _quoted_findings():
    """Each finding quoted in the tutorial's code blocks: (severity, rule,
    message, hint or None). A hint may wrap onto indented lines."""
    out, block, lines = [], False, TUTORIAL.read_text(encoding="utf-8").splitlines()
    for n, line in enumerate(lines):
        if line.strip().startswith("```"):
            block = not block
            continue
        m = _FINDING.match(line.strip()) if block else None
        if not m:
            continue
        message = lines[n + 1].strip()
        hint, k = None, n + 2
        if lines[k].strip().startswith("hint: "):
            parts = [lines[k].strip()[len("hint: "):]]
            k += 1
            while lines[k].startswith("  ") and not lines[k].strip().startswith("hint:") \
                    and not _FINDING.match(lines[k].strip()) \
                    and not lines[k].strip().startswith("```"):
                parts.append(lines[k].strip())
                k += 1
            hint = " ".join(parts)
        out.append((m.group(1), m.group(2), message, hint))
    return out


def test_tutorial_quotes_real_findings(base, tmp_path):
    """Every finding the tutorial quotes is one the check really gives, in one
    of the chapter states or examples, word for word."""
    actual = {(i.severity, i.rule, i.message, i.hint)
              for i in check_conventions(parse_file(base))}
    for ex in PLAN["examples"]:
        layout = _state(base, tmp_path, ex["chapter"], ex.get("add"), ex.get("drop"),
                        ex.get("groups"))
        actual |= {(i.severity, i.rule, i.message, i.hint) for i in check_conventions(layout)}
    quoted = _quoted_findings()
    assert len(quoted) >= 6
    for sev, rule, message, hint in quoted:
        hits = [a for a in actual if a[:3] == (sev, rule, message)]
        assert hits, f"quoted {sev} {rule} {message!r} is not what the check says"
        if hint is not None:
            assert any(a[3] == hint for a in hits), f"{rule}: hint {hint!r} differs"


DVRR = Path(__file__).parents[2] / "app" / "lib" / "examples" / \
    "DVRR v3.0 E Stroudsburg - Pt Jervis.xtc"


@pytest.mark.skipif(not DVRR.exists(), reason="XTrkCAD's example layouts aren't here")
def test_appendix_your_own_layout(tmp_path):
    """The appendix's DVRR steps 2 and 3: what the check finds on a layout
    with no conventions, and the layers add_track_layers makes."""
    from xtrkcad_mcp.server import add_track_layers
    dvrr = tmp_path / "dvrr.xtc"
    dvrr.write_bytes(DVRR.read_bytes())
    issues = check_conventions(parse_file(dvrr))
    assert {(i.severity, i.rule) for i in issues} == {
        ("error", "MP_NO_ANCHOR"), ("warning", "CATEGORY_LAYER_NONE")}
    added = add_track_layers(str(dvrr), levels=1)["added"]
    assert added == ["L1-Main", "L1-Passing", "L1-Storage", "L1-Staging",
                     "L1-Connecting", "L1-Service"]
