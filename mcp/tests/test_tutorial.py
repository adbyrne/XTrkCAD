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
        ("HY", 0.0), ("AL", 3.24), ("BI", 13.396), ("CE", 20.792)]
    assert (_station(export, "AL")["mp_entry"], _station(export, "AL")["mp_exit"]) == (
        0.587, 5.892)
    assert _station(export, "AL")["siding_cars"] == 7
    assert _station(export, "CE")["terminus"] is True


def test_quoted_yard_limits(export):
    """Chapter 6: Cedar's board moves its zone's end out to MP 20.014."""
    ce = next(z for z in export["yard_limits"] if z["id"] == "CE")
    assert (ce["mp_start"], ce["mp_end"], ce["source"]) == (20.014, 20.792, "board")


def test_quoted_capacities(export):
    """Chapters 7-9."""
    spots = {i["id"]: (i["car_spots"], i["spur_capacity_cars"])
             for i in export["industries"] + export["team_tracks"]}
    assert spots == {"AL_FEED": (2, 4), "AL_TEAM": (2, 3), "BI_LUMBER": (3, 5)}
    hy = next(y for y in export["yards"] if y["id"] == "HY")
    assert [(t["label"], t["capacity_cars"]) for t in hy["tracks"]] == [
        ("Track 1", 10), ("Track 2", 7), ("Track 3", 7)]
    coal = export["facilities"][0]
    assert (coal["id"], coal["car_spots"], coal["capacity_cars"]) == ("CE_COAL", 3, 4)


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
