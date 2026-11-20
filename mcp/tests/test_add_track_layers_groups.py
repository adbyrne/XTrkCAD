"""add_track_layers also makes the category Layer Groups (MCP review 2026-11-20).

On an existing layout the check wants every track layer in a category group
(Mainline, Passing, ...). add_track_layers adds the L<n>- layers; it now
adds the groups too, as generate_layout does, leaving groups the user
already has alone.
"""

from xtrkcad_mcp import server
from xtrkcad_mcp.parser import parse_file

HEAD = """VERSION {v} 5.4.0
SCALE HO
ROOMSIZE 100 x 100
LAYERS 0 1 0 1 0 0 0 0 0 "Main line west" 1 0 0.000000 0.000000 0.000000 0.000000 0.000000
LAYERS CURRENT 0
{groups}STRAIGHT 1 0 0 0 0 HO 2 0.000000 0.000000
\tE4 0.000000 0.000000 270.000000 0 0.0 0.0 0.0 0.0 0 0 0 0.000000
\tE4 10.000000 0.000000 90.000000 0 0.0 0.0 0.0 0.0 0 0 0 0.000000
\tEND$SEGS
END$TRACKS
"""


def _layout(tmp_path, version=13, groups=""):
    p = tmp_path / "own.xtc"
    p.write_text(HEAD.format(v=version, groups=groups), encoding="utf-8")
    return p


def _groups(p):
    layout = parse_file(p)
    names = {i: info.name for i, info in layout.layers.items()}
    return {g.name: sorted(names[m] for m in g.layers) for g in layout.layer_groups}


def test_category_groups_are_made(tmp_path):
    p = _layout(tmp_path)
    result = server.add_track_layers(str(p), levels=2)
    assert result["groups_added"] == ["Mainline", "Passing", "Storage", "Staging",
                                      "Connecting", "Service"]
    g = _groups(p)
    assert g["Mainline"] == ["L1-Main", "L2-Main"]
    assert g["Service"] == ["L1-Service", "L2-Service"]


def test_a_group_the_user_has_is_left_alone(tmp_path):
    p = _layout(tmp_path, groups='LAYERGROUP DEFINE 0 "Mainline"\nLAYERGROUP MEMBERS 0 "1"\n')
    result = server.add_track_layers(str(p), levels=1)
    assert "Mainline" not in result["groups_added"]
    assert _groups(p)["Mainline"] == ["Main line west"]
    assert _groups(p)["Passing"] == ["L1-Passing"]


def test_old_file_version_gets_no_groups(tmp_path):
    p = _layout(tmp_path, version=12)
    result = server.add_track_layers(str(p), levels=1)
    assert result["groups_added"] == []
    assert "Save it in XTrkCAD 5.4" in result["notice"]


def test_groups_added_when_the_layers_already_exist(tmp_path):
    p = _layout(tmp_path)
    server.add_track_layers(str(p), levels=1)
    text = p.read_text(encoding="utf-8")
    p.write_text("\n".join(l for l in text.splitlines()
                           if not l.startswith("LAYERGROUP")) + "\n", encoding="utf-8")
    result = server.add_track_layers(str(p), levels=1)
    assert result["added"] == [] and len(result["groups_added"]) == 6
