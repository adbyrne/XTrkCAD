"""Parser for XTrkCAD .xtc / .xtce layout files."""

from pathlib import Path

from xtrkcad_mcp.models import Endpoint, LayerInfo, Layout, NoteObject, TrackObject

TRACK_KINDS = frozenset(
    {"STRAIGHT", "CURVE", "JOINT", "TURNOUT", "TURNTABLE", "CORNU", "BEZIER", "HANDLAID"}
)

# paramVersion >= this value means NOTE content is inline (not multiline until END).
_VERSION_INLINENOTE = 12


def _parse_roomsize(value: str) -> tuple[float, float]:
    parts = value.split("x")
    if len(parts) == 2:
        try:
            return float(parts[0].strip()), float(parts[1].strip())
        except ValueError:
            pass
    return 0.0, 0.0


def _parse_layers_line(line: str, layout: Layout) -> None:
    """Parse a LAYERS record and store in layout.layers."""
    # LAYERS index visible flag visible_color ... "Name"
    # LAYERS CURRENT index
    parts = line.split(None, 3)
    if len(parts) < 2:
        return
    if parts[1] == "CURRENT":
        return
    try:
        idx = int(parts[1])
    except ValueError:
        return
    # Extract quoted name at end of line
    q = line.rfind('"')
    qstart = line.rfind('"', 0, q)
    name = line[qstart + 1:q] if q > 0 and qstart >= 0 else f"Layer {idx}"
    # Visibility is the 3rd token (index 2)
    visible = True
    if len(parts) >= 3:
        try:
            visible = int(parts[2]) != 0
        except ValueError:
            pass
    layout.layers[idx] = LayerInfo(index=idx, name=name, visible=visible)


def _first_quoted(line: str) -> str:
    """Return the content of the first double-quoted string in line, or ''."""
    q1 = line.find('"')
    if q1 < 0:
        return ""
    q2 = line.find('"', q1 + 1)
    return line[q1 + 1:q2] if q2 > q1 else ""


def _two_quoted(line: str) -> tuple[str, str]:
    """Return the contents of the first two double-quoted strings in line."""
    q1 = line.find('"')
    if q1 < 0:
        return "", ""
    q2 = line.find('"', q1 + 1)
    if q2 < 0:
        return "", ""
    first = line[q1 + 1:q2]
    rest = line[q2 + 1:]
    q3 = rest.find('"')
    if q3 < 0:
        return first, ""
    q4 = rest.find('"', q3 + 1)
    second = rest[q3 + 1:q4] if q4 > q3 else ""
    return first, second


def parse_file(path: str | Path) -> Layout:
    """Parse an XTrkCAD layout file and return a Layout object."""
    path = Path(path)
    layout = Layout()
    current_track: TrackObject | None = None
    current_note: NoteObject | None = None
    note_text_lines: list[str] = []

    with path.open(encoding="utf-8", errors="replace") as fh:
        for raw_line in fh:
            line = raw_line.rstrip("\n")

            # END terminates both tracks and old-format (pre-v12) multiline notes.
            if line.strip() == "END":
                if current_note is not None:
                    current_note.text = "\n".join(note_text_lines).strip()
                    layout.notes.append(current_note)
                    current_note = None
                    note_text_lines = []
                else:
                    current_track = None
                continue

            # Accumulate old-format multiline note body (any line before END).
            if current_note is not None:
                note_text_lines.append(line)
                continue

            if line.startswith("\t"):
                if current_track is None:
                    continue
                parts = line.strip().split()
                if not parts:
                    continue
                tag = parts[0]
                if tag == "T" and len(parts) >= 5:
                    try:
                        connected_id = int(parts[1])
                        x, y, angle = float(parts[2]), float(parts[3]), float(parts[4])
                        current_track.endpoints.append(
                            Endpoint(x=x, y=y, angle=angle, connected_to=connected_id)
                        )
                    except (ValueError, IndexError):
                        pass
                elif tag == "E" and len(parts) >= 4:
                    try:
                        x, y, angle = float(parts[1]), float(parts[2]), float(parts[3])
                        current_track.endpoints.append(
                            Endpoint(x=x, y=y, angle=angle, connected_to=None)
                        )
                    except (ValueError, IndexError):
                        pass
                continue

            parts = line.split()
            if not parts or line.startswith("#"):
                continue

            keyword = parts[0]

            if keyword == "VERSION" and len(parts) >= 2:
                try:
                    layout.param_version = int(parts[1])
                except ValueError:
                    pass
                if len(parts) >= 3:
                    layout.version = parts[2]
            elif keyword == "TITLE1":
                layout.title1 = line[len("TITLE1 "):]
            elif keyword == "TITLE2":
                layout.title2 = line[len("TITLE2 "):]
            elif keyword == "SCALE" and len(parts) >= 2:
                layout.scale = parts[1]
            elif keyword == "ROOMSIZE" and len(parts) >= 3:
                layout.room_width, layout.room_height = _parse_roomsize(
                    line[len("ROOMSIZE "):]
                )
            elif keyword == "LAYERS":
                _parse_layers_line(line, layout)
            elif keyword == "NOTE" and not line.startswith("NOTE MAIN") and len(parts) >= 9:
                # NOTE id layer 0 0 x y 0 op_or_len [content...]
                try:
                    note_id = int(parts[1])
                    note_layer = int(parts[2])
                    note_x = float(parts[5])
                    note_y = float(parts[6])
                    field8 = int(parts[8])
                except (ValueError, IndexError):
                    continue
                if layout.param_version >= _VERSION_INLINENOTE:
                    # Inline format: field8 = op; content is the first quoted string.
                    op = field8
                    if op == 0:
                        text = _first_quoted(line)
                        title = ""
                    else:
                        text, title = _two_quoted(line)
                    layout.notes.append(NoteObject(
                        id=note_id, layer=note_layer,
                        x=note_x, y=note_y,
                        op=op, text=text, title=title,
                    ))
                else:
                    # Multiline format: field8 = byte count; text on following lines.
                    current_note = NoteObject(
                        id=note_id, layer=note_layer,
                        x=note_x, y=note_y,
                        op=0, text="",
                    )
                    note_text_lines = []
            elif keyword in TRACK_KINDS and len(parts) >= 2:
                try:
                    track_id = int(parts[1])
                except ValueError:
                    continue
                layer = 0
                if len(parts) >= 3:
                    try:
                        layer = int(parts[2])
                    except ValueError:
                        pass
                extra: dict = {}
                if keyword == "CURVE":
                    # Format: CURVE id layer options 0 0 scale bits cx cy 0 radius ...
                    # Indices:   0    1   2      3   4 5  6    7   8  9 10  11
                    if len(parts) >= 12:
                        try:
                            extra["cx"] = float(parts[8])
                            extra["cy"] = float(parts[9])
                            extra["radius"] = float(parts[11])
                        except (ValueError, IndexError):
                            pass
                elif keyword == "TURNTABLE":
                    # Same field layout as CURVE: cx=parts[8], cy=parts[9], radius=parts[11]
                    if len(parts) >= 12:
                        try:
                            extra["cx"] = float(parts[8])
                            extra["cy"] = float(parts[9])
                            extra["radius"] = float(parts[11])
                        except (ValueError, IndexError):
                            pass
                elif keyword == "TURNOUT":
                    q = line.find('"')
                    if q >= 0:
                        extra["name"] = line[q:].strip('"')
                current_track = TrackObject(
                    id=track_id, kind=keyword, layer=layer, extra=extra
                )
                layout.tracks.append(current_track)

    return layout
