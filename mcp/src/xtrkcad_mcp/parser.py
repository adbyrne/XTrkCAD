"""Parser for XTrkCAD .xtc / .xtce layout files."""

from pathlib import Path

from xtrkcad_mcp.models import Endpoint, Layout, TrackObject

# Top-level keywords that define track/non-track objects ending with END
TRACK_KINDS = frozenset(
    {"STRAIGHT", "CURVE", "JOINT", "TURNOUT", "TURNTABLE", "CORNU", "BEZIER", "HANDLAID"}
)


def _parse_roomsize(value: str) -> tuple[float, float]:
    """Parse 'W x H' string into (width, height)."""
    parts = value.split("x")
    if len(parts) == 2:
        try:
            return float(parts[0].strip()), float(parts[1].strip())
        except ValueError:
            pass
    return 0.0, 0.0


def _parse_endpoint_line(parts: list[str]) -> Endpoint:
    """Parse a T or E sub-line (already split, tag stripped)."""
    # T: connected_id x y angle [extra...]
    # E: x y angle [extra...]
    try:
        x = float(parts[0])
        y = float(parts[1])
        angle = float(parts[2])
        return Endpoint(x=x, y=y, angle=angle, connected_to=None)
    except (IndexError, ValueError):
        return Endpoint(x=0.0, y=0.0, angle=0.0, connected_to=None)


def parse_file(path: str | Path) -> Layout:
    """Parse an XTrkCAD layout file and return a Layout object."""
    path = Path(path)
    layout = Layout()
    current_track: TrackObject | None = None

    with path.open(encoding="utf-8", errors="replace") as fh:
        for raw_line in fh:
            line = raw_line.rstrip("\n")

            # Sub-line of a current track object
            if line.startswith("\t"):
                if current_track is None:
                    continue
                parts = line.strip().split()
                if not parts:
                    continue
                tag = parts[0]
                if tag == "T" and len(parts) >= 5:
                    # T connected_id x y angle [extra...]
                    try:
                        connected_id = int(parts[1])
                        ep = _parse_endpoint_line(parts[2:])
                        ep.connected_to = connected_id
                        current_track.endpoints.append(ep)
                    except (ValueError, IndexError):
                        pass
                elif tag == "E" and len(parts) >= 4:
                    # E x y angle — open endpoint
                    ep = _parse_endpoint_line(parts[1:])
                    current_track.endpoints.append(ep)
                # D, P, C etc. are turnout internal data — skip
                continue

            # End of a track object
            if line.strip() == "END":
                current_track = None
                continue

            # Top-level keyword line
            parts = line.split()
            if not parts or line.startswith("#"):
                continue

            keyword = parts[0]

            if keyword == "VERSION" and len(parts) >= 3:
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
            elif keyword in TRACK_KINDS and len(parts) >= 2:
                try:
                    track_id = int(parts[1])
                except ValueError:
                    continue
                # Layer is the 6th field (index 5) when it exists
                layer = 0
                if len(parts) >= 6:
                    try:
                        layer = int(parts[5])
                    except ValueError:
                        pass
                extra: dict = {}
                if keyword == "CURVE" and len(parts) >= 10:
                    try:
                        extra["cx"] = float(parts[8])
                        extra["cy"] = float(parts[9])
                        extra["radius"] = float(parts[11]) if len(parts) > 11 else 0.0
                    except (ValueError, IndexError):
                        pass
                elif keyword == "TURNOUT":
                    # Name is in quotes at end of line
                    q = line.find('"')
                    if q >= 0:
                        extra["name"] = line[q:].strip('"')
                current_track = TrackObject(
                    id=track_id, kind=keyword, layer=layer, extra=extra
                )
                layout.tracks.append(current_track)

    return layout
