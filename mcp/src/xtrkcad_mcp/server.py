"""XTrkCAD MCP server — layout read and write tools."""

import datetime
import logging
import math
import os
from pathlib import Path

from mcp.server.fastmcp import FastMCP

from xtrkcad_mcp.models import SCALE_RATIOS, cars_per_real_ft, max_to_main_label
from xtrkcad_mcp.parser import TRACK_KINDS, parse_file

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
# Report tools — write_layout_report, write_equipment_report,
#                write_turnout_report, write_gaps_report
# ---------------------------------------------------------------------------

# Equipment minimum curve radii in HO model inches.
# Scaled to other gauges via SCALE_RATIOS: threshold × (HO_ratio / scale_ratio).
_EQUIPMENT_THRESHOLDS: list[tuple[str, float]] = [
    ("Short freight car (< 40 ft)",              15.0),
    ("Standard freight car (40–50 ft)",          18.0),
    ("Long freight car (55–60 ft flatcar)",      22.0),
    ("4-axle diesel (GP/RS-type)",               18.0),
    ("6-axle diesel (SD40/ES44)",                22.0),
    ("Small/medium steam (2-6-0, 2-8-0, 4-6-0)", 18.0),
    ("Medium steam (4-6-2, 2-8-2, 4-6-4)",      22.0),
    ("Large steam (4-8-4, 2-10-4, 4-8-2)",      24.0),
    ("Articulated steam (2-8-8-2, 4-8-8-4)",    28.0),
    ("Standard passenger car (85 ft)",           22.0),
    ("Long passenger car (Superliner)",          28.0),
]


def _near_miss_pairs(
    open_eps: list[dict], threshold: float = 0.1
) -> list[tuple[int, int]]:
    pairs = []
    for i in range(len(open_eps)):
        for j in range(i + 1, len(open_eps)):
            a, b = open_eps[i], open_eps[j]
            if math.hypot(b["x"] - a["x"], b["y"] - a["y"]) <= threshold:
                pairs.append((i, j))
    return pairs


@mcp.tool()
def write_layout_report(
    path: str,
    output_path: str,
    layer_categories: dict | None = None,
) -> str:
    """Write a full layout summary report to a text file.

    Covers layout header, track counts by type, track lengths per layer,
    curve radius analysis, and (if layer_categories is given) Operation Density.

    Args:
        path: Path to the source .xtc or .xtce file.
        output_path: Destination path for the .txt report.
        layer_categories: Optional layer→category mapping (same as
            get_operation_density). Required for the OD section.

    Returns:
        Confirmation message with the absolute path written.
    """
    layout = parse_file(_resolve_plan(path))
    scale = layout.scale
    cpf = cars_per_real_ft(scale)
    total_ft = layout.total_length_real_feet()
    room_w = layout.room_width / 12.0
    room_h = layout.room_height / 12.0
    radii = layout.curve_radii()
    by_layer = layout.length_by_layer()
    to_by_layer = layout.turnouts_by_layer()

    lines: list[str] = [
        "LAYOUT REPORT",
        f"  Title  : {layout.title1}",
    ]
    if layout.title2:
        lines.append(f"  Subtitle: {layout.title2}")
    lines += [
        f"  Scale  : {scale}",
        f"  Room   : {room_w:.1f}' × {room_h:.1f}' = {room_w * room_h:.0f} sq ft",
        f"  Date   : {datetime.date.today()}",
        "",
        "TRACK COUNTS BY TYPE",
    ]
    for kind, count in sorted(layout.track_counts().items()):
        lines.append(f"  {kind:12s}: {count:4d}")
    lines.append(f"  {'TOTAL':12s}: {len(layout.tracks):4d}")

    lines += [
        "",
        "TRACK LENGTHS BY LAYER",
        f"  {'Lyr':>3s}  {'Name':16s}  {'Feet':>8s}  {'Cars':>6s}  {'T/O':>5s}",
        f"  {'---':>3s}  {'-'*16}  {'-'*8}  {'-'*6}  {'-'*5}",
    ]
    for idx, ft in sorted(by_layer.items()):
        name = layout.layers[idx].name if idx in layout.layers else f"Layer {idx}"
        cars = int(ft * cpf)
        turnouts = to_by_layer.get(idx, 0)
        lines.append(f"  {idx:>3d}  {name:16s}  {ft:8.1f}  {cars:6d}  {turnouts:5d}")
    lines.append(
        f"  {'':>3s}  {'TOTAL':16s}  {total_ft:8.1f}"
        f"  {int(total_ft * cpf):6d}  {sum(to_by_layer.values()):5d}"
    )

    lines += ["", "CURVE ANALYSIS"]
    if not radii:
        lines.append("  No curves found.")
    else:
        min_r = min(radii)
        max_r = max(radii)
        mean_r = sum(radii) / len(radii)
        lines += [
            f"  Curves : {len(radii)}",
            f"  Min    : {min_r:.1f}\" model",
            f"  Max    : {max_r:.1f}\" model",
            f"  Mean   : {mean_r:.1f}\" model",
            "  Distribution:",
        ]
        for label, lo, hi in [
            ("< 9\"",   0.0,  9.0),
            ("9–18\"",  9.0, 18.0),
            ("18–24\"", 18.0, 24.0),
            ("24–36\"", 24.0, 36.0),
            ("> 36\"",  36.0, 9999.0),
        ]:
            n = sum(1 for r in radii if lo <= r < hi)
            if n:
                lines.append(f"    {label:8s}: {n:4d}")
        ho_ratio = SCALE_RATIOS.get("HO", 87.1)
        sr = SCALE_RATIOS.get(scale, 87.1)
        tight = 22.0 * ho_ratio / sr
        if min_r < tight:
            lines.append(
                f"  NOTE: minimum {min_r:.1f}\" is below tight-running threshold"
                f" {tight:.1f}\" ({scale}). Some equipment may not operate reliably."
            )

    if layer_categories is not None:
        od = get_operation_density(path, layer_categories)
        ops = od["operations"]
        lines += ["", "OPERATION DENSITY (Joe Fugate, MRH Oct 2014)"]
        cat_order = ["mainline", "passing", "storage", "staging", "connecting", "service"]
        shown: set[str] = set()
        for cat in cat_order:
            if cat in od["by_category"]:
                info = od["by_category"][cat]
                lines.append(
                    f"  {cat:12s}: {info['length_real_ft']:7.1f} ft"
                    f"  {info['car_capacity']:4d} cars"
                    f"  {info['turnouts']:3d} turnouts"
                )
                shown.add(cat)
        for cat, info in sorted(od["by_category"].items()):
            if cat not in shown:
                lines.append(
                    f"  {cat:12s}: {info['length_real_ft']:7.1f} ft"
                    f"  {info['car_capacity']:4d} cars"
                    f"  {info['turnouts']:3d} turnouts"
                )
        lines += [
            f"  Max cars    : {ops['max_cars']}",
            f"  Cars moved  : {ops['cars_moved_per_session']}",
            f"  Max-to-main : {ops['max_to_main_pct']:.1f}%  → {ops['max_to_main_label']}",
            f"  Est. trains : {ops['estimated_trains']} (@ {ops['assumed_train_length_cars']} cars)",
        ]

    out = Path(output_path).expanduser()
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return f"Report written to {out}"


@mcp.tool()
def write_equipment_report(path: str, output_path: str) -> str:
    """Write an equipment suitability report based on the layout's minimum curve radius.

    Compares the minimum curve radius against published minimums for common
    equipment classes. Shows PASS / MARGINAL (within 2\" of minimum) / FAIL.

    Args:
        path: Path to the source .xtc or .xtce file.
        output_path: Destination path for the .txt report.

    Returns:
        Confirmation message with the absolute path written.
    """
    layout = parse_file(_resolve_plan(path))
    scale = layout.scale
    radii = [r for r in layout.curve_radii() if r >= 9.0]  # skip decorative curves
    min_r = min(radii) if radii else None

    ho_ratio = SCALE_RATIOS.get("HO", 87.1)
    sr = SCALE_RATIOS.get(scale, 87.1)
    sf = ho_ratio / sr

    lines: list[str] = [
        "EQUIPMENT SUITABILITY REPORT",
        f"  Layout : {layout.title1}",
        f"  Scale  : {scale}",
        f"  Date   : {datetime.date.today()}",
    ]
    if min_r is None:
        lines.append("  No usable curves found — cannot assess equipment suitability.")
    else:
        lines += [
            f"  Min curve radius: {min_r:.1f}\" model  "
            f"(curves < 9\" excluded as likely decorative)",
            "",
            "EQUIPMENT COMPATIBILITY",
            f"  {'Equipment Class':44s}  {'Min Radius':>10s}  {'Status':>10s}",
            f"  {'-'*44}  {'-'*10}  {'-'*10}",
        ]
        for name, ho_thresh in _EQUIPMENT_THRESHOLDS:
            thresh = ho_thresh * sf
            if min_r >= thresh:
                status = "PASS"
            elif min_r >= thresh - 2.0 * sf:
                status = "MARGINAL"
            else:
                status = "FAIL"
            lines.append(
                f"  {name:44s}  {thresh:>8.1f}\"  {status:>10s}"
            )
        lines += [
            "",
            f"Thresholds scaled from HO reference (factor {sf:.3f} for {scale}).",
            "MARGINAL = within 2\" (scaled) of minimum; test with your specific models.",
        ]

    out = Path(output_path).expanduser()
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return f"Report written to {out}"


@mcp.tool()
def write_turnout_report(path: str, output_path: str) -> str:
    """Write a turnout density report to a text file.

    Reports total turnouts, density (turnouts per 100 ft of track), and a
    per-layer breakdown. Flags layers with density > 10/100 ft as switching-heavy.
    Also lists any turnout objects with fewer than 3 endpoints.

    Args:
        path: Path to the source .xtc or .xtce file.
        output_path: Destination path for the .txt report.

    Returns:
        Confirmation message with the absolute path written.
    """
    layout = parse_file(_resolve_plan(path))
    total_ft = layout.total_length_real_feet()
    total_turnouts = sum(1 for t in layout.tracks if t.kind == "TURNOUT")
    by_layer = layout.length_by_layer()
    to_by_layer = layout.turnouts_by_layer()

    global_density = (total_turnouts / total_ft * 100.0) if total_ft > 0 else 0.0

    lines: list[str] = [
        "TURNOUT DENSITY REPORT",
        f"  Layout  : {layout.title1}",
        f"  Scale   : {layout.scale}",
        f"  Date    : {datetime.date.today()}",
        "",
        "OVERALL",
        f"  Total turnouts: {total_turnouts}",
        f"  Total track   : {total_ft:.1f} ft",
        f"  Density       : {global_density:.1f} per 100 ft",
        "",
        "BY LAYER",
        f"  {'Lyr':>3s}  {'Name':16s}  {'Feet':>8s}  {'T/O':>5s}  {'Density/100ft':>14s}  {'Flag':4s}",
        f"  {'---':>3s}  {'-'*16}  {'-'*8}  {'-'*5}  {'-'*14}  {'-'*4}",
    ]
    for idx, ft in sorted(by_layer.items()):
        name = layout.layers[idx].name if idx in layout.layers else f"Layer {idx}"
        turnouts = to_by_layer.get(idx, 0)
        density = (turnouts / ft * 100.0) if ft > 0 else 0.0
        flag = "***" if density > 10.0 else ""
        lines.append(
            f"  {idx:>3d}  {name:16s}  {ft:8.1f}  {turnouts:5d}"
            f"  {density:14.1f}  {flag}"
        )

    # Turnouts with < 3 endpoints
    partial = [t for t in layout.tracks if t.kind == "TURNOUT" and len(t.endpoints) < 3]
    lines += ["", "PARTIAL TURNOUTS (fewer than 3 endpoints — may be broken/incomplete)"]
    if partial:
        for t in partial:
            lines.append(f"  ID {t.id}: {len(t.endpoints)} endpoint(s), layer {t.layer}")
    else:
        lines.append("  None found.")

    if any(
        (to_by_layer.get(idx, 0) / ft * 100.0) > 10.0
        for idx, ft in by_layer.items() if ft > 0
    ):
        lines += [
            "",
            "*** Layers with density > 10/100 ft are switching-heavy (possible staging yard).",
        ]

    out = Path(output_path).expanduser()
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return f"Report written to {out}"


@mcp.tool()
def write_gaps_report(path: str, output_path: str) -> str:
    """Write an open endpoints (track gaps) report to a text file.

    Lists every unconnected endpoint with its track ID, position, and angle.
    Groups pairs of endpoints within 0.1 model inches of each other as
    near misses (should likely be connected).

    Turntable stall endpoints are intentionally open and are reported
    separately — they do not appear in the gap analysis.

    Args:
        path: Path to the source .xtc or .xtce file.
        output_path: Destination path for the .txt report.

    Returns:
        Confirmation message with the absolute path written.
    """
    layout = parse_file(_resolve_plan(path))
    all_open = [
        {"track_id": tid, "x": ep.x, "y": ep.y, "angle": ep.angle}
        for tid, ep in layout.unconnected_endpoints()
    ]

    turntable_ids = {t.id for t in layout.tracks if t.kind == "TURNTABLE"}
    open_eps = [ep for ep in all_open if ep["track_id"] not in turntable_ids]
    tt_stalls = [ep for ep in all_open if ep["track_id"] in turntable_ids]

    pairs = _near_miss_pairs(open_eps)
    near_miss_indices: set[int] = set()
    for i, j in pairs:
        near_miss_indices.add(i)
        near_miss_indices.add(j)
    isolated = [i for i in range(len(open_eps)) if i not in near_miss_indices]

    tt_ids_str = (
        ", ".join(str(i) for i in sorted({ep["track_id"] for ep in tt_stalls}))
        if tt_stalls else "none"
    )

    lines: list[str] = [
        "TRACK GAPS REPORT",
        f"  Layout : {layout.title1}",
        f"  Scale  : {layout.scale}",
        f"  Date   : {datetime.date.today()}",
        "",
        "SUMMARY",
        f"  Turntable stalls (open by design) : {len(tt_stalls):4d}  (track IDs: {tt_ids_str})",
        f"  Track gaps to check               : {len(open_eps):4d}",
        f"  Near-miss pairs                   : {len(pairs):4d}  (endpoints within 0.1\" — should connect)",
        f"  Isolated open ends                : {len(isolated):4d}",
    ]

    if pairs:
        lines += ["", "NEAR-MISS PAIRS (should likely be connected)"]
        for i, j in pairs:
            a, b = open_eps[i], open_eps[j]
            dist = math.hypot(b["x"] - a["x"], b["y"] - a["y"])
            lines.append(
                f"  Track {a['track_id']:4d}  ({a['x']:8.3f}, {a['y']:8.3f})"
                f"  ↔  Track {b['track_id']:4d}  ({b['x']:8.3f}, {b['y']:8.3f})"
                f"  gap={dist:.4f}\""
            )

    if open_eps:
        lines += ["", "ALL OPEN TRACK ENDPOINTS"]
        lines.append(
            f"  {'Track':>5s}  {'X':>10s}  {'Y':>10s}  {'Angle':>8s}  {'Note':5s}"
        )
        lines.append(
            f"  {'-----':>5s}  {'-'*10}  {'-'*10}  {'-'*8}  {'-'*10}"
        )
        for i, ep in enumerate(open_eps):
            note = "near-miss" if i in near_miss_indices else ""
            lines.append(
                f"  {ep['track_id']:>5d}  {ep['x']:10.3f}  {ep['y']:10.3f}"
                f"  {ep['angle']:8.2f}  {note}"
            )

    out = Path(output_path).expanduser()
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return f"Report written to {out}"


# ---------------------------------------------------------------------------
# SVG radius map
# ---------------------------------------------------------------------------


def _arc_polyline(
    cx: float, cy: float, r: float,
    x1: float, y1: float, x2: float, y2: float,
    arc_deg: float, n: int = 24,
) -> list[tuple[float, float]]:
    """Sample n+1 points on the circular arc from (x1,y1) to (x2,y2)."""
    if arc_deg < 0.01 or r <= 0:
        return [(x1, y1), (x2, y2)]
    a_start = math.atan2(y1 - cy, x1 - cx)
    arc_rad = math.radians(arc_deg)

    def sample(direction: float) -> list[tuple[float, float]]:
        return [
            (cx + r * math.cos(a_start + direction * arc_rad * i / n),
             cy + r * math.sin(a_start + direction * arc_rad * i / n))
            for i in range(n + 1)
        ]

    pts_ccw = sample(1.0)
    pts_cw = sample(-1.0)

    def dist_end(pts: list) -> float:
        ex, ey = pts[-1]
        return math.hypot(ex - x2, ey - y2)

    return pts_ccw if dist_end(pts_ccw) <= dist_end(pts_cw) else pts_cw


@mcp.tool()
def write_radius_map(
    path: str,
    output_path: str,
    flag_radius: float | None = None,
) -> str:
    """Generate an SVG map of the layout with curves color-coded by radius.

    Useful for visually identifying curves that may be too tight for
    reliable equipment operation.

    Color coding (model inches, scaled to layout's gauge):
      Red    — below flag_radius (default 18\" HO-equivalent): likely problematic
      Orange — flag_radius to 1.5×: tight but operable for most stock
      Yellow — 1.5× to 2×flag_radius: marginal for long cars/steam
      Gray   — above 2×flag_radius: normal

    Problem curves (below 2×flag_radius) are labeled with track ID and radius.

    Args:
        path: Path to the source .xtc or .xtce file.
        output_path: Destination path for the .svg file.
        flag_radius: Model-inch threshold for red flagging. Defaults to
            18\" HO scaled to the layout's scale.

    Returns:
        Confirmation message with the absolute path written.
    """
    layout = parse_file(_resolve_plan(path))
    scale = layout.scale

    ho_ratio = SCALE_RATIOS.get("HO", 87.1)
    sr = SCALE_RATIOS.get(scale, 87.1)
    if flag_radius is None:
        flag_radius = 18.0 * ho_ratio / sr

    thresh_orange = flag_radius * 1.5
    thresh_yellow = flag_radius * 2.0

    def curve_class(r: float) -> str:
        if r < flag_radius:
            return "curve-red"
        if r < thresh_orange:
            return "curve-orange"
        if r < thresh_yellow:
            return "curve-yellow"
        return "curve-normal"

    room_w = layout.room_width or 200.0
    room_h = layout.room_height or 200.0
    px_scale = 1200.0 / max(room_w, room_h)
    svg_w = int(room_w * px_scale)
    svg_h = int(room_h * px_scale)

    def tx(x: float) -> str:
        return f"{x * px_scale:.1f}"

    def ty(y: float) -> str:
        return f"{(room_h - y) * px_scale:.1f}"

    def txf(x: float) -> float:
        return x * px_scale

    def tyf(y: float) -> float:
        return (room_h - y) * px_scale

    elements: list[str] = []

    for t in layout.tracks:
        eps = t.endpoints
        if len(eps) < 2:
            continue

        if t.kind in {"STRAIGHT", "JOINT"}:
            elements.append(
                f'<line x1="{tx(eps[0].x)}" y1="{ty(eps[0].y)}"'
                f' x2="{tx(eps[1].x)}" y2="{ty(eps[1].y)}" class="straight"/>'
            )

        elif t.kind == "CURVE":
            cx_ = t.extra.get("cx")
            cy_ = t.extra.get("cy")
            r = t.extra.get("radius", 0.0)
            if cx_ is None or r <= 0:
                continue
            arc_deg = ((eps[1].angle - eps[0].angle + 180.0) % 360.0 + 360.0) % 360.0
            pts = _arc_polyline(cx_, cy_, r, eps[0].x, eps[0].y, eps[1].x, eps[1].y, arc_deg)
            # Y-flip each sampled point
            svg_pts = " ".join(f"{txf(px):.1f},{tyf(py):.1f}" for px, py in pts)
            cls = curve_class(r)
            elements.append(f'<polyline points="{svg_pts}" class="{cls}"/>')
            if r < thresh_yellow:
                mid = pts[len(pts) // 2]
                elements.append(
                    f'<text x="{txf(mid[0]):.1f}" y="{tyf(mid[1]):.1f}"'
                    f' class="label">{t.id}: {r:.1f}"</text>'
                )

        elif t.kind == "TURNOUT":
            x1s, y1s = tx(eps[0].x), ty(eps[0].y)
            x2s, y2s = tx(eps[1].x), ty(eps[1].y)
            elements.append(
                f'<line x1="{x1s}" y1="{y1s}" x2="{x2s}" y2="{y2s}" class="turnout"/>'
            )
            if len(eps) >= 3:
                elements.append(
                    f'<line x1="{x1s}" y1="{y1s}"'
                    f' x2="{tx(eps[2].x)}" y2="{ty(eps[2].y)}" class="turnout"/>'
                )

        elif t.kind == "TURNTABLE":
            cx_ = t.extra.get("cx")
            cy_ = t.extra.get("cy")
            r = t.extra.get("radius", 0.0)
            if cx_ is None or r <= 0:
                continue
            elements.append(
                f'<circle cx="{tx(cx_)}" cy="{ty(cy_)}" r="{r * px_scale:.1f}"'
                f' class="turntable"/>'
            )

        else:
            # CORNU, BEZIER, etc. — draw as straight line between first two endpoints
            elements.append(
                f'<line x1="{tx(eps[0].x)}" y1="{ty(eps[0].y)}"'
                f' x2="{tx(eps[1].x)}" y2="{ty(eps[1].y)}" class="straight"/>'
            )

    # Legend
    leg_x, leg_y = 10, svg_h - 80
    legend = [
        f'<rect x="{leg_x}" y="{leg_y}" width="16" height="6" fill="#cc0000"/>',
        f'<text x="{leg_x + 22}" y="{leg_y + 7}" class="legend">Radius &lt; {flag_radius:.1f}" (problem)</text>',
        f'<rect x="{leg_x}" y="{leg_y + 14}" width="16" height="6" fill="#ff8800"/>',
        f'<text x="{leg_x + 22}" y="{leg_y + 21}" class="legend">Radius {flag_radius:.1f}–{thresh_orange:.1f}" (tight)</text>',
        f'<rect x="{leg_x}" y="{leg_y + 28}" width="16" height="6" fill="#ccaa00"/>',
        f'<text x="{leg_x + 22}" y="{leg_y + 35}" class="legend">Radius {thresh_orange:.1f}–{thresh_yellow:.1f}" (marginal)</text>',
        f'<rect x="{leg_x}" y="{leg_y + 42}" width="16" height="6" fill="#888"/>',
        f'<text x="{leg_x + 22}" y="{leg_y + 49}" class="legend">Normal (≥ {thresh_yellow:.1f}")</text>',
    ]

    css = """\
<style>
  .straight     { stroke: #444; stroke-width: 2; fill: none; }
  .curve-normal { stroke: #888; stroke-width: 2; fill: none; }
  .curve-yellow { stroke: #ccaa00; stroke-width: 3; fill: none; }
  .curve-orange { stroke: #ff8800; stroke-width: 3; fill: none; }
  .curve-red    { stroke: #cc0000; stroke-width: 3; fill: none; }
  .turnout      { stroke: #444; stroke-width: 2; fill: none; }
  .turntable    { stroke: #666; stroke-width: 2; fill: #ddd; fill-opacity: 0.5; }
  .label        { font-size: 10px; fill: #000; font-family: monospace; }
  .legend       { font-size: 11px; fill: #333; font-family: sans-serif; }
</style>"""

    title = layout.title1 or "Layout"
    svg_lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{svg_w}" height="{svg_h}"'
        f' viewBox="0 0 {svg_w} {svg_h}">',
        css,
        f'<rect width="{svg_w}" height="{svg_h}" fill="white"/>',
        f'<rect x="0" y="0" width="{svg_w}" height="{svg_h}" fill="none" stroke="#ccc" stroke-width="1"/>',
        f'<text x="{svg_w // 2}" y="16" text-anchor="middle" font-size="14"'
        f' font-family="sans-serif">{title} — Curve Radius Map</text>',
    ] + elements + legend + ["</svg>"]

    out = Path(output_path).expanduser()
    out.write_text("\n".join(svg_lines) + "\n", encoding="utf-8")
    return f"SVG radius map written to {out}"


# ---------------------------------------------------------------------------
# Tools — write operations
# ---------------------------------------------------------------------------


def _delete_track_block(lines: list[str], track_id: int) -> list[str]:
    """Return lines with the block for track_id removed.

    Raises ValueError if track_id is not found.
    """
    start = None
    for i, line in enumerate(lines):
        parts = line.split()
        if parts and parts[0] in TRACK_KINDS:
            try:
                if int(parts[1]) == track_id:
                    start = i
                    break
            except (ValueError, IndexError):
                pass
    if start is None:
        raise ValueError(f"Track ID {track_id} not found in layout")
    end = start + 1
    while end < len(lines) and lines[end].startswith("\t"):
        end += 1
    return lines[:start] + lines[end:]


@mcp.tool()
def delete_track(path: str, track_id: int) -> str:
    """Delete a track object from a layout file by its numeric ID.

    Rewrites the file in place. Use this to remove drawing errors or
    phantom tracks that should not exist.

    Args:
        path: Path to the .xtc layout file (absolute or relative to XTRKCAD_PLANS_DIR).
        track_id: Numeric track ID to remove (visible in XTrkCAD Properties dialog).
    """
    p = _resolve_plan(path)
    lines = p.read_text(encoding="utf-8", errors="replace").splitlines()
    updated = _delete_track_block(lines, track_id)
    p.write_text("\n".join(updated) + "\n", encoding="utf-8")
    return f"Track {track_id} deleted from {p}"


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
