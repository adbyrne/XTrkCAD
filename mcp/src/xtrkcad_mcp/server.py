"""XTrkCAD MCP server — Phase 1: read-only layout tools."""

import logging
import os
from pathlib import Path

from mcp.server.fastmcp import FastMCP

from xtrkcad_mcp.models import SCALE_RATIOS, cars_per_real_ft, max_to_main_label
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
# Tools — file discovery
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
    return sorted(
        str(p) for p in base.rglob("*") if p.suffix.lower() in {".xtc", ".xtce"}
    )


# ---------------------------------------------------------------------------
# Tools — layout summary
# ---------------------------------------------------------------------------


@mcp.tool()
def get_layout_summary(path: str) -> dict:
    """Return a summary of a layout: title, scale, room size, track counts.

    Args:
        path: Path to a .xtc or .xtce file.

    Returns:
        Dict with title1, title2, version, scale, room_width, room_height,
        track_counts (by type), total_tracks, layers.
    """
    layout = parse_file(_resolve_plan(path))
    return {
        "title1": layout.title1,
        "title2": layout.title2,
        "version": layout.version,
        "scale": layout.scale,
        "room_width_inches": layout.room_width,
        "room_height_inches": layout.room_height,
        "room_width_feet": round(layout.room_width / 12, 2),
        "room_height_feet": round(layout.room_height / 12, 2),
        "room_area_sqft": round((layout.room_width / 12) * (layout.room_height / 12), 1),
        "track_counts": layout.track_counts(),
        "total_tracks": len(layout.tracks),
        "layers": {
            str(idx): {"name": li.name, "visible": li.visible}
            for idx, li in sorted(layout.layers.items())
        },
    }


@mcp.tool()
def get_track_objects(path: str) -> list[dict]:
    """Return all track objects in a layout with positions, connections, and lengths.

    Args:
        path: Path to a .xtc or .xtce file.

    Returns:
        List of dicts, each with id, kind, layer, length_model_in, length_real_ft,
        and endpoints.
    """
    layout = parse_file(_resolve_plan(path))
    result = []
    for t in layout.tracks:
        result.append(
            {
                "id": t.id,
                "kind": t.kind,
                "layer": t.layer,
                "length_model_in": round(t.length_model_inches(), 4),
                "length_real_ft": round(t.length_real_feet(), 4),
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
        List of dicts with track_id, x, y, angle for each open endpoint.
    """
    layout = parse_file(_resolve_plan(path))
    return [
        {
            "track_id": track_id,
            "x": round(ep.x, 6),
            "y": round(ep.y, 6),
            "angle": round(ep.angle, 6),
        }
        for track_id, ep in layout.unconnected_endpoints()
    ]


# ---------------------------------------------------------------------------
# Tools — track lengths and geometry
# ---------------------------------------------------------------------------


@mcp.tool()
def get_track_lengths(path: str) -> dict:
    """Return track lengths broken down by layer, in real feet and model inches.

    Args:
        path: Path to a .xtc or .xtce file.

    Returns:
        Dict with total lengths and a per-layer breakdown including layer name,
        total feet, turnout count, and car capacity.
    """
    layout = parse_file(_resolve_plan(path))
    total_ft = layout.total_length_real_feet()
    cpf = cars_per_real_ft(layout.scale)
    car_len_in = 12.0 / cpf

    by_layer = layout.length_by_layer()
    turnouts_by_layer = layout.turnouts_by_layer()

    layer_breakdown = {}
    for idx, ft in sorted(by_layer.items()):
        name = layout.layers[idx].name if idx in layout.layers else f"Layer {idx}"
        layer_breakdown[str(idx)] = {
            "name": name,
            "length_real_ft": round(ft, 1),
            "length_model_in": round(ft * 12, 1),
            "turnout_count": turnouts_by_layer.get(idx, 0),
            "car_capacity": int(ft * cpf),
        }

    return {
        "scale": layout.scale,
        "car_length_model_in": round(car_len_in, 2),
        "cars_per_real_ft": round(cpf, 2),
        "total_real_ft": round(total_ft, 1),
        "total_model_in": round(total_ft * 12, 1),
        "total_car_capacity": int(total_ft * cpf),
        "total_turnouts": sum(turnouts_by_layer.values()),
        "by_layer": layer_breakdown,
    }


@mcp.tool()
def get_curve_stats(path: str) -> dict:
    """Return curve radius statistics for a layout.

    Minimum radius is critical for knowing which locomotives and passenger cars
    will operate reliably.

    Args:
        path: Path to a .xtc or .xtce file.

    Returns:
        Dict with min_radius, max_radius, mean_radius (all in model inches and
        real inches), curve count, and a histogram of radius ranges.
    """
    layout = parse_file(_resolve_plan(path))
    radii = layout.curve_radii()
    if not radii:
        return {"curve_count": 0, "min_radius_in": None, "max_radius_in": None}

    min_r = min(radii)
    max_r = max(radii)
    mean_r = sum(radii) / len(radii)

    # Bucket into ranges (model inches)
    buckets: dict[str, int] = {}
    for r in radii:
        if r < 12:
            key = "< 12in"
        elif r < 18:
            key = "12–18in"
        elif r < 24:
            key = "18–24in"
        elif r < 36:
            key = "24–36in"
        else:
            key = "> 36in"
        buckets[key] = buckets.get(key, 0) + 1

    return {
        "scale": layout.scale,
        "curve_count": len(radii),
        "min_radius_model_in": round(min_r, 2),
        "max_radius_model_in": round(max_r, 2),
        "mean_radius_model_in": round(mean_r, 2),
        "radius_distribution": buckets,
    }


# ---------------------------------------------------------------------------
# Tools — Operation Density (Joe Fugate methodology, MRH Oct 2014)
# ---------------------------------------------------------------------------


def _build_category_fn(
    layer_categories: dict | None,
    layout,
) -> "callable[[int], str]":
    name_to_cat: dict[str, str] = {}
    idx_to_cat: dict[int, str] = {}
    if layer_categories:
        for key, cat in layer_categories.items():
            try:
                idx_to_cat[int(key)] = cat.lower()
            except ValueError:
                name_to_cat[key.lower()] = cat.lower()

    def _category(layer_idx: int) -> str:
        if layer_idx in idx_to_cat:
            return idx_to_cat[layer_idx]
        if layer_idx in layout.layers:
            name = layout.layers[layer_idx].name.lower()
            if name in name_to_cat:
                return name_to_cat[name]
        return "mainline"

    return _category


@mcp.tool()
def get_operation_density(
    path: str,
    layer_categories: dict | None = None,
    car_length_model_in: float | None = None,
    train_length_cars: int = 6,
) -> dict:
    """Compute Operation Density statistics using Joe Fugate's methodology (MRH Oct 2014).

    Formulas:
      max_cars   = 0.8 × (storage_cars + staging_cars + passing_cars / 2)
      cars_moved = 0.4 × (staging_cars × 2 + passing_cars + connecting_cars)
      max_to_main_pct = max_cars / mainline_cars × 100

    Args:
        path: Path to a .xtc or .xtce file.
        layer_categories: Optional dict mapping layer index (as string) or layer
            name to a category. Valid categories: "mainline", "staging",
            "storage", "service", "passing", "connecting", "scenery", "ignore".
            Uncategorized layers default to "mainline".
            Example: {"0": "mainline", "1": "staging", "4": "storage"}.
        car_length_model_in: Override car length in model inches. Defaults to
            Fugate's published factor for the layout's scale (HO=6in, N=3in, etc.)
        train_length_cars: Assumed train length in cars for estimating train count
            (default 6). Does not affect max_cars or cars_moved formulas.

    Returns:
        Dict with room info, track by category, and operations sub-dict with
        max_cars, cars_moved_per_session, max_to_main_pct, max_to_main_label,
        and estimated_trains.
    """
    layout = parse_file(_resolve_plan(path))
    scale = layout.scale

    cpf = cars_per_real_ft(scale)
    if car_length_model_in is not None:
        cpf = 12.0 / car_length_model_in
    else:
        car_length_model_in = 12.0 / cpf

    _category = _build_category_fn(layer_categories, layout)

    cat_ft: dict[str, float] = {}
    cat_turnouts: dict[str, int] = {}
    for t in layout.tracks:
        cat = _category(t.layer)
        if cat in {"ignore", "scenery"}:
            continue
        cat_ft[cat] = cat_ft.get(cat, 0.0) + t.length_real_feet()
        if t.kind == "TURNOUT":
            cat_turnouts[cat] = cat_turnouts.get(cat, 0) + 1

    def _cars(category: str) -> float:
        return cat_ft.get(category, 0.0) * cpf

    mainline_cars = _cars("mainline")
    passing_cars = _cars("passing")
    storage_cars = _cars("storage")
    staging_cars = _cars("staging")
    connecting_cars = _cars("connecting")

    # Fugate formulas
    max_cars = int(0.8 * (storage_cars + staging_cars + passing_cars / 2.0))
    cars_moved = int(0.4 * (staging_cars * 2.0 + passing_cars + connecting_cars))

    max_to_main_pct = (max_cars / mainline_cars * 100.0) if mainline_cars > 0 else 0.0
    label = max_to_main_label(max_to_main_pct)
    trains = round(cars_moved / max(train_length_cars, 1), 1)

    total_ft = sum(cat_ft.values())
    total_turnouts = sum(cat_turnouts.values())

    room_w_ft = layout.room_width / 12.0
    room_h_ft = layout.room_height / 12.0

    categories_out = {}
    for cat in sorted(cat_ft):
        ft = cat_ft[cat]
        categories_out[cat] = {
            "length_real_ft": round(ft, 1),
            "car_capacity": int(ft * cpf),
            "turnouts": cat_turnouts.get(cat, 0),
        }

    return {
        "title": layout.title1,
        "scale": scale,
        "car_length_model_in": round(car_length_model_in, 2),
        "cars_per_real_ft": round(cpf, 2),
        "room": {
            "width_ft": round(room_w_ft, 1),
            "height_ft": round(room_h_ft, 1),
            "area_sqft": round(room_w_ft * room_h_ft, 0),
        },
        "totals": {
            "length_real_ft": round(total_ft, 1),
            "car_capacity": int(total_ft * cpf),
            "turnout_count": total_turnouts,
        },
        "by_category": categories_out,
        "operations": {
            "max_cars": max_cars,
            "cars_moved_per_session": cars_moved,
            "max_to_main_pct": round(max_to_main_pct, 1),
            "max_to_main_label": label,
            "assumed_train_length_cars": train_length_cars,
            "estimated_trains": trains,
        },
        "note": (
            "Methodology: Joe Fugate, MRH Oct 2014 'Layout Design Assessment'. "
            "max_cars=0.8×(storage+staging+passing/2 cars). "
            "cars_moved=0.4×(staging×2+passing+connecting cars). "
            "Assign layer_categories to get meaningful category splits."
        ),
    }


@mcp.tool()
def write_operation_density_report(
    path: str,
    output_path: str,
    layer_categories: dict | None = None,
    car_length_model_in: float | None = None,
    train_length_cars: int = 6,
) -> str:
    """Write a Fugate Operation Density report to a text file.

    Runs get_operation_density and formats the result as a human-readable
    text report in Fugate's summary style.

    Args:
        path: Path to the source .xtc or .xtce file.
        output_path: Destination path for the .txt report.
        layer_categories: Same as get_operation_density.
        car_length_model_in: Same as get_operation_density.
        train_length_cars: Same as get_operation_density.

    Returns:
        Confirmation message with the absolute path written.
    """
    data = get_operation_density(path, layer_categories, car_length_model_in, train_length_cars)
    ops = data["operations"]
    room = data["room"]
    totals = data["totals"]

    lines: list[str] = [
        "OPERATION DENSITY REPORT",
        f"  Layout : {data['title']}",
        f"  Scale  : {data['scale']}",
        f"  Room   : {room['width_ft']}' × {room['height_ft']}' = {room['area_sqft']:.0f} sq ft",
        f"  Car len: {data['car_length_model_in']}\" model  ({data['cars_per_real_ft']} cars/real ft)",
        "",
        "TRACK BY CATEGORY",
    ]

    cat_order = ["mainline", "passing", "storage", "staging", "connecting", "service"]
    shown: set[str] = set()
    for cat in cat_order:
        if cat in data["by_category"]:
            info = data["by_category"][cat]
            lines.append(
                f"  {cat:12s}: {info['length_real_ft']:7.1f} ft"
                f"  {info['car_capacity']:4d} cars"
                f"  {info['turnouts']:3d} turnouts"
            )
            shown.add(cat)
    for cat, info in sorted(data["by_category"].items()):
        if cat not in shown:
            lines.append(
                f"  {cat:12s}: {info['length_real_ft']:7.1f} ft"
                f"  {info['car_capacity']:4d} cars"
                f"  {info['turnouts']:3d} turnouts"
            )
    lines += [
        f"  {'TOTAL':12s}: {totals['length_real_ft']:7.1f} ft"
        f"  {totals['car_capacity']:4d} cars"
        f"  {totals['turnout_count']:3d} turnouts",
        "",
        "OPERATION DENSITY (Joe Fugate, MRH Oct 2014)",
        f"  Max cars (0.8×storage+staging+passing/2):  {ops['max_cars']}",
        f"  Cars moved/session (0.4×staging×2+pass+conn): {ops['cars_moved_per_session']}",
        f"  Max-to-main ratio: {ops['max_to_main_pct']:.1f}%  → {ops['max_to_main_label']}",
        f"  Est. trains (@ {ops['assumed_train_length_cars']} cars/train): {ops['estimated_trains']}",
        "",
        f"Note: {data['note']}",
    ]

    out = Path(output_path).expanduser()
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return f"Report written to {out}"


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
