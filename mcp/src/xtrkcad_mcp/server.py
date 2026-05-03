"""XTrkCAD MCP server — Phase 1: read-only layout tools."""

import logging
import os
from pathlib import Path

from mcp.server.fastmcp import FastMCP

from xtrkcad_mcp.parser import parse_file

logging.basicConfig(level=os.environ.get("XTRKCAD_LOG_LEVEL", "INFO"))
logger = logging.getLogger(__name__)

mcp = FastMCP("xtrkcad")

_PLANS_DIR = Path(os.environ.get("XTRKCAD_PLANS_DIR", "."))
_XTRKCAD_BINARY = os.environ.get("XTRKCAD_BINARY", "")


def _resolve_plans_dir(directory: str | None) -> Path:
    if directory:
        return Path(directory).expanduser()
    return _PLANS_DIR


def _resolve_plan(path: str) -> Path:
    p = Path(path).expanduser()
    if p.is_absolute():
        return p
    return _PLANS_DIR / p


# ---------------------------------------------------------------------------
# Tools
# ---------------------------------------------------------------------------


@mcp.tool()
def list_track_plans(directory: str = "") -> list[str]:
    """List all XTrkCAD layout files (.xtc and .xtce) in a directory.

    Args:
        directory: Path to search. Uses XTRKCAD_PLANS_DIR if empty.

    Returns:
        Sorted list of absolute file paths.
    """
    base = _resolve_plans_dir(directory)
    if not base.exists():
        return []
    files = sorted(
        str(p) for p in base.rglob("*") if p.suffix.lower() in {".xtc", ".xtce"}
    )
    return files


@mcp.tool()
def get_layout_summary(path: str) -> dict:
    """Return a summary of a layout: title, scale, room size, track counts.

    Args:
        path: Path to a .xtc or .xtce file.

    Returns:
        Dict with title1, title2, version, scale, room_width, room_height,
        track_counts (by type), total_tracks.
    """
    layout = parse_file(_resolve_plan(path))
    return {
        "title1": layout.title1,
        "title2": layout.title2,
        "version": layout.version,
        "scale": layout.scale,
        "room_width": layout.room_width,
        "room_height": layout.room_height,
        "track_counts": layout.track_counts(),
        "total_tracks": len(layout.tracks),
    }


@mcp.tool()
def get_track_objects(path: str) -> list[dict]:
    """Return all track objects in a layout with their positions and connections.

    Args:
        path: Path to a .xtc or .xtce file.

    Returns:
        List of dicts, each with id, kind, layer, endpoints (list of
        {x, y, angle, connected_to}).
    """
    layout = parse_file(_resolve_plan(path))
    result = []
    for t in layout.tracks:
        result.append(
            {
                "id": t.id,
                "kind": t.kind,
                "layer": t.layer,
                "extra": t.extra,
                "endpoints": [
                    {
                        "x": round(ep.x, 6),
                        "y": round(ep.y, 6),
                        "angle": round(ep.angle, 6),
                        "connected_to": ep.connected_to,
                    }
                    for ep in t.endpoints
                ],
            }
        )
    return result


@mcp.tool()
def find_unconnected_endpoints(path: str) -> list[dict]:
    """Find all open (unconnected) endpoints in a layout.

    Args:
        path: Path to a .xtc or .xtce file.

    Returns:
        List of dicts with track_id, x, y, angle for each unconnected endpoint.
    """
    layout = parse_file(_resolve_plan(path))
    result = []
    for track_id, ep in layout.unconnected_endpoints():
        result.append(
            {
                "track_id": track_id,
                "x": round(ep.x, 6),
                "y": round(ep.y, 6),
                "angle": round(ep.angle, 6),
            }
        )
    return result


# ---------------------------------------------------------------------------
# Resources
# ---------------------------------------------------------------------------


@mcp.resource("xtrkcad://plans")
def list_plans_resource() -> str:
    """List all layout files in XTRKCAD_PLANS_DIR."""
    files = list_track_plans("")
    if not files:
        return f"No .xtc or .xtce files found in {_PLANS_DIR}"
    return "\n".join(files)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def main() -> None:
    mcp.run()


if __name__ == "__main__":
    main()
