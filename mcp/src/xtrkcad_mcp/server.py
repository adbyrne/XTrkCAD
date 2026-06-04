"""XTrkCAD MCP server — layout read and write tools."""

import datetime
import logging
import math
import os
from pathlib import Path

from mcp.server.fastmcp import FastMCP

from xtrkcad_mcp.config import load_config
from xtrkcad_mcp.models import SCALE_RATIOS, cars_per_real_ft, max_to_main_label
from xtrkcad_mcp.svg_views import generate_elevation_view, generate_plan_view
from xtrkcad_mcp.parser import TRACK_KINDS, parse_file
from xtrkcad_mcp.stations import (
    compute_capacities,
    compute_distances,
    extract_stations,
    model_in_to_proto_ft,
)

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
# Tools — layout generation (config + questionnaire)
# ---------------------------------------------------------------------------


@mcp.tool()
def load_layout_config(config_path: str) -> dict:
    """Parse and validate a layout YAML config file.

    Returns a summary of the layout to be generated, or a list of missing
    required fields with the questions to ask the user. Use this as the first
    step before generating a layout.

    Args:
        config_path: Path to the YAML config file.

    Returns:
        Dict with:
          ready    — True if all required fields are present
          summary  — human-readable plan (populated when ready=True)
          missing  — list of {field, question} for fields still needed
          warnings — list of validation warnings
          config   — parsed config values (name, scale, room, placements, ...)
    """
    result = load_config(config_path)
    cfg = result.config
    fp = cfg.floor_plan
    return {
        "ready": result.ready,
        "summary": result.summary,
        "missing": [{"field": f, "question": q} for f, q in result.missing],
        "warnings": result.warnings,
        "config": {
            "name": cfg.name,
            "scale": cfg.scale,
            "room_width_ft": cfg.room_width_ft,
            "room_depth_ft": cfg.room_depth_ft,
            "mainline": cfg.mainline,
            "curve_radius_in": cfg.curve_radius_in,
            "switch_size": cfg.switch_size,
            "levels": cfg.levels,
            "level_separation_in": cfg.level_separation_in,
            "level_break_row": cfg.level_break_row,
            "grid_size_ft": cfg.grid_size_ft,
            "benchwork_file": cfg.benchwork_file,
            "benchwork_sections": [
                {
                    "label": bw.label,
                    "level": bw.level,
                    "vertex_count": len(bw.vertices),
                }
                for bw in cfg.benchwork_sections
            ],
            "floor_plan_file": cfg.floor_plan_file,
            "floor_plan": {
                "width_in": fp.total_width_in,
                "depth_in": fp.total_depth_in,
                "rooms": len(fp.rooms),
                "doors": len(fp.doors),
                "partitions": len(fp.partitions),
                "restricted": len(fp.restricted),
            } if fp else None,
            "placements": [
                {
                    "range": pl.cell_range,
                    "type": pl.element_type,
                    "params": pl.params,
                }
                for pl in cfg.placements
            ],
        },
    }


@mcp.tool()
def list_templates(category: str = "") -> list[dict]:
    """List available layout element templates from the template library.

    Args:
        category: Filter by category (yard, staging, station, mainline, helix,
                  siding). Empty returns all categories.

    Returns:
        List of template summaries with id, name, category, description,
        mainline_connection, od_role, and stub flag.
    """
    from xtrkcad_mcp.templates import load_library
    lib = load_library()
    items = lib.by_category(category) if category else lib.templates
    return [
        {
            "id": t.id,
            "name": t.name,
            "category": t.category,
            "description": t.description,
            "mainline_connection": t.mainline_connection,
            "od_role": t.od_role,
            "stub": t.stub,
        }
        for t in items
    ]


@mcp.tool()
def get_template_info(template_id: str) -> dict:
    """Get full details for a template including parameters and OD role.

    Args:
        template_id: Template identifier (e.g. 'yard/single-ended').

    Returns:
        Full template metadata with parameters and connection points,
        or an error dict if not found.
    """
    from xtrkcad_mcp.templates import load_library
    lib = load_library()
    t = lib.by_id(template_id)
    if t is None:
        return {
            "error": f"template {template_id!r} not found",
            "available": [x.id for x in lib.templates],
        }
    return {
        "id": t.id,
        "name": t.name,
        "category": t.category,
        "description": t.description,
        "file": t.file,
        "mainline_connection": t.mainline_connection,
        "od_role": t.od_role,
        "scales": t.scales or ["all"],
        "stub": t.stub,
        "connection_points": t.connection_points,
        "parameters": [
            {
                "name": p.name,
                "type": p.type,
                "required": p.required,
                "description": p.description,
                "default": p.default,
                "unit": p.unit,
                "min": p.min,
                "max": p.max,
            }
            for p in t.parameters
        ],
    }


@mcp.tool()
def generate_layout(config_path: str, output_path: str = "") -> dict:
    """Generate an initial .xtc layout file from a YAML config.

    If the config is missing required fields (name, scale, room) the tool
    returns a questionnaire — a list of fields to prompt the user for —
    instead of generating. Fix the config and call again.

    Any existing .xtc at output_path is versioned (copied to _v1.xtc, _v2.xtc,
    etc.) before overwriting.

    Args:
        config_path: Path to the YAML layout config file.
        output_path: Path for the .xtc output. Defaults to the config
                     directory, named after the layout (e.g. my_layout.xtc).

    Returns:
        Dict with output_path, backup_path, placed list, skipped list,
        warnings, and config summary — or missing/warnings if config incomplete.
    """
    from xtrkcad_mcp.config import load_config
    from xtrkcad_mcp.generator import default_output_path, generate
    from pathlib import Path

    cfg_result = load_config(config_path)

    if not cfg_result.ready:
        return {
            "ready": False,
            "missing": [{"field": f, "question": q} for f, q in cfg_result.missing],
            "warnings": cfg_result.warnings,
            "summary": "Config incomplete — answer the missing fields and try again.",
        }

    config = cfg_result.config
    cfg_path = Path(config_path).expanduser()
    out = Path(output_path).expanduser() if output_path else default_output_path(config, cfg_path)

    result = generate(config, out)

    return {
        "ready": True,
        "output_path": str(result.output_path),
        "backup_path": str(result.backup_path) if result.backup_path else None,
        "placed": [
            {
                "template_id": p.template_id,
                "grid_range": p.grid_range,
                "origin_x_in": round(p.x, 2),
                "origin_y_in": round(p.y, 2),
                "track_ids": p.track_ids,
            }
            for p in result.placed
        ],
        "skipped": result.skipped,
        "warnings": result.warnings,
        "summary": cfg_result.summary,
    }


# ---------------------------------------------------------------------------
# Tools — benchwork plan
# ---------------------------------------------------------------------------


def _poly_bbox(vertices: list) -> tuple[float, float, float, float]:
    """Return (min_x, min_y, max_x, max_y) of a vertex list."""
    xs = [v[0] for v in vertices]
    ys = [v[1] for v in vertices]
    return min(xs), min(ys), max(xs), max(ys)


def _poly_area(vertices: list) -> float:
    """Shoelace formula — signed area magnitude of a closed polygon."""
    n = len(vertices)
    if n < 3:
        return 0.0
    area = sum(
        vertices[i][0] * vertices[(i + 1) % n][1]
        - vertices[(i + 1) % n][0] * vertices[i][1]
        for i in range(n)
    )
    return abs(area) / 2.0


@mcp.tool()
def write_benchwork_report(
    config_path: str,
    output_path: str,
    format: str = "txt",
) -> str:
    """Write a benchwork plan report from a layout YAML config.

    Summarises every benchwork section by level: label, bounding box,
    approximate area, and vertex count.  Useful for reviewing the physical
    benchwork plan before generating a layout.

    Args:
        config_path: Path to the YAML layout config (or benchwork_file path).
        output_path: Destination path for the report.
        format: Output format — "txt" (default), "md", or "html".

    Returns:
        Confirmation message with the absolute path written.
    """
    cfg_result = load_config(config_path)
    cfg = cfg_result.config
    sections = cfg.benchwork_sections

    # Group by level
    by_level: dict[int, list] = {}
    for bw in sections:
        by_level.setdefault(bw.level, []).append(bw)

    title = f"Benchwork Plan Report: {cfg.name}" if cfg.name else "Benchwork Plan Report"

    # Build per-section rows: (label, level, bbox, area_sqin, area_sqft)
    def _section_row(bw):
        if bw.vertices:
            x0, y0, x1, y1 = _poly_bbox(bw.vertices)
            area_sqin = _poly_area(bw.vertices)
        else:
            x0 = y0 = x1 = y1 = 0.0
            area_sqin = 0.0
        area_sqft = area_sqin / 144.0
        return {
            "label": bw.label,
            "level": bw.level,
            "x0": x0, "y0": y0, "x1": x1, "y1": y1,
            "area_sqin": area_sqin,
            "area_sqft": area_sqft,
        }

    rows = [_section_row(bw) for bw in sections]

    out = Path(output_path).expanduser()
    fmt = _resolve_format(output_path, format)

    if fmt == "txt":
        lines = [
            title,
            f"  Config : {config_path}",
            f"  Scale  : {cfg.scale or '—'}",
            f"  Date   : {datetime.date.today()}",
            f"  Levels : {', '.join(str(lv) for lv in sorted(by_level)) or 'none'}",
            f"  Total sections: {len(sections)}",
        ]
        if cfg_result.warnings:
            for w in cfg_result.warnings:
                lines.append(f"  WARNING: {w}")
        if not sections:
            lines += ["", "  No benchwork sections defined."]
        else:
            for lv in sorted(by_level):
                lv_rows = [r for r in rows if r["level"] == lv]
                lv_area = sum(r["area_sqft"] for r in lv_rows)
                lines += [
                    "",
                    f"LEVEL {lv}  ({len(lv_rows)} section{'s' if len(lv_rows) != 1 else ''},"
                    f" ~{lv_area:.1f} sq ft total)",
                    f"  {'Section':<24s}  {'Bbox (x0–x1) × (y0–y1) in':32s}  {'Sqin':>8s}  {'Sqft':>6s}",
                    f"  {'-'*24}  {'-'*32}  {'-'*8}  {'-'*6}",
                ]
                for r in lv_rows:
                    bbox = f"({r['x0']:.0f}–{r['x1']:.0f}) × ({r['y0']:.0f}–{r['y1']:.0f})"
                    lines.append(
                        f"  {r['label']:<24s}  {bbox:<32s}  {r['area_sqin']:8.1f}  {r['area_sqft']:6.1f}"
                    )
            total_area = sum(r["area_sqft"] for r in rows)
            lines += [
                "",
                f"TOTAL BENCHWORK AREA: ~{total_area:.1f} sq ft",
                "",
                "Note: area is the polygon area of each section as defined; actual"
                " built area may vary.",
            ]
        out.write_text("\n".join(lines) + "\n", encoding="utf-8")

    elif fmt == "md":
        md: list[str] = [
            f"# {title}", "",
            f"**Config:** {config_path}  |  **Scale:** {cfg.scale or '—'}"
            f"  |  **Date:** {datetime.date.today()}",
            f"**Levels:** {', '.join(str(lv) for lv in sorted(by_level)) or 'none'}"
            f"  |  **Total sections:** {len(sections)}",
        ]
        if cfg_result.warnings:
            md += ["", "> **Warnings:**"]
            for w in cfg_result.warnings:
                md.append(f"> - {w}")
        if not sections:
            md += ["", "_No benchwork sections defined._"]
        else:
            for lv in sorted(by_level):
                lv_rows = [r for r in rows if r["level"] == lv]
                lv_area = sum(r["area_sqft"] for r in lv_rows)
                md += [
                    "",
                    f"## Level {lv}  (~{lv_area:.1f} sq ft)", "",
                ]
                tbl_rows = [
                    [r["label"],
                     f"({r['x0']:.0f}–{r['x1']:.0f}) × ({r['y0']:.0f}–{r['y1']:.0f})",
                     f"{r['area_sqin']:.1f}",
                     f"{r['area_sqft']:.1f}"]
                    for r in lv_rows
                ]
                md += _md_table(["Section", "Bbox (in)", "Sq in", "Sq ft"], tbl_rows)
            total_area = sum(r["area_sqft"] for r in rows)
            md += [
                "",
                f"**Total benchwork area: ~{total_area:.1f} sq ft**",
                "",
                "_Area is the polygon area of each section as defined; actual built area may vary._",
            ]
        out.write_text("\n".join(md) + "\n", encoding="utf-8")

    else:  # html
        if not sections:
            body = f"<h1>{title}</h1><p>No benchwork sections defined.</p>"
        else:
            body = (
                f"<h1>{title}</h1>"
                f"<div class='summary'>"
                f"<p><strong>Config:</strong> {config_path} &nbsp;|&nbsp; "
                f"<strong>Scale:</strong> {cfg.scale or '—'} &nbsp;|&nbsp; "
                f"<strong>Date:</strong> {datetime.date.today()}</p>"
                f"<p><strong>Levels:</strong> {', '.join(str(lv) for lv in sorted(by_level))} "
                f"&nbsp;|&nbsp; <strong>Total sections:</strong> {len(sections)}</p>"
                f"</div>"
            )
            for lv in sorted(by_level):
                lv_rows = [r for r in rows if r["level"] == lv]
                lv_area = sum(r["area_sqft"] for r in lv_rows)
                tbl = _html_table(
                    ["Section", "Bbox (in)", "Sq in", "Sq ft"],
                    [[r["label"],
                      f"({r['x0']:.0f}–{r['x1']:.0f}) × ({r['y0']:.0f}–{r['y1']:.0f})",
                      f"{r['area_sqin']:.1f}",
                      f"{r['area_sqft']:.1f}"]
                     for r in lv_rows],
                )
                body += f"<h2>Level {lv} (~{lv_area:.1f} sq ft)</h2>{tbl}"
            total_area = sum(r["area_sqft"] for r in rows)
            body += (
                f"<p><strong>Total benchwork area: ~{total_area:.1f} sq ft</strong></p>"
                f"<p><em>Area is the polygon area of each section as defined; "
                f"actual built area may vary.</em></p>"
            )
        out.write_text(_html_page(title, body), encoding="utf-8")

    return f"Benchwork report written to {out}"


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


# Maps the suffix of a standard layer name (after stripping the leading
# "L{n}-" or "L{n}H-" prefix) to a Fugate category.
_SUFFIX_TO_CATEGORY: dict[str, str] = {
    "main":       "mainline",
    "passing":    "passing",
    "storage":    "storage",
    "staging":    "staging",
    "connecting": "connecting",
    "service":    "service",
    "benchwork":  "ignore",
    "track":      "mainline",   # backward compat with pre-spec generated files
}


def _category_from_name(name: str) -> str | None:
    """Return Fugate category inferred from a standard layer name, or None."""
    import re
    # Strip optional leading "L{digits}[H]-" prefix (e.g. "L1-", "L1H-", "l2-")
    suffix = re.sub(r"^l\d+h?-", "", name.lower())
    return _SUFFIX_TO_CATEGORY.get(suffix)


def _level_from_layer_name(name: str) -> str:
    """Return the level key from a standard layer name, or '0' if unrecognised.

    Examples: 'L1-Main' → '1', 'L2-Staging' → '2', 'L1H-Connecting' → '1H'.
    Layers without an 'Ln-' prefix (Floor, custom names) → '0'.
    """
    import re
    m = re.match(r"^[Ll](\d+[Hh]?)-", name)
    return m.group(1) if m else "0"


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
        # Explicit index override takes highest precedence
        if layer_idx in idx_to_cat:
            return idx_to_cat[layer_idx]
        if layer_idx in layout.layers:
            name = layout.layers[layer_idx].name
            # Explicit name override
            if name.lower() in name_to_cat:
                return name_to_cat[name.lower()]
            # Auto-categorize from standard layer name suffix
            inferred = _category_from_name(name)
            if inferred is not None:
                return inferred
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
        Dict with room info, track by category, by_level breakdown, and
        operations sub-dict with max_cars, cars_moved_per_session,
        max_to_main_pct, max_to_main_label, and estimated_trains.
        by_level groups totals by physical level parsed from layer names
        (e.g. "L1-Main" → level "1"); layers without a standard prefix
        appear under level "0".
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
    # level_data[level_str][cat] = {"ft": float, "turnouts": int}
    level_data: dict[str, dict[str, dict]] = {}

    for t in layout.tracks:
        cat = _category(t.layer)
        if cat in {"ignore", "scenery"}:
            continue
        ft = t.length_real_feet()
        is_turnout = t.kind == "TURNOUT"

        cat_ft[cat] = cat_ft.get(cat, 0.0) + ft
        if is_turnout:
            cat_turnouts[cat] = cat_turnouts.get(cat, 0) + 1

        layer_name = layout.layers[t.layer].name if t.layer in layout.layers else ""
        lv = _level_from_layer_name(layer_name)
        lv_cats = level_data.setdefault(lv, {})
        lv_entry = lv_cats.setdefault(cat, {"ft": 0.0, "turnouts": 0})
        lv_entry["ft"] += ft
        if is_turnout:
            lv_entry["turnouts"] += 1

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

    def _sort_level_key(lv: str) -> tuple:
        """Sort level keys numerically: '1' < '1H' < '2' < '0' (unassigned last)."""
        if lv == "0":
            return (9999, "")
        import re
        m = re.match(r"(\d+)([Hh]?)", lv)
        return (int(m.group(1)), m.group(2).upper()) if m else (9998, lv)

    by_level_out: dict[str, dict] = {}
    for lv in sorted(level_data, key=_sort_level_key):
        lv_cats = level_data[lv]
        lv_ft = sum(e["ft"] for e in lv_cats.values())
        lv_turnouts = sum(e["turnouts"] for e in lv_cats.values())
        lv_cats_out = {}
        for cat in sorted(lv_cats):
            e = lv_cats[cat]
            lv_cats_out[cat] = {
                "length_real_ft": round(e["ft"], 1),
                "car_capacity": int(e["ft"] * cpf),
                "turnouts": e["turnouts"],
            }
        by_level_out[lv] = {
            "length_real_ft": round(lv_ft, 1),
            "car_capacity": int(lv_ft * cpf),
            "turnouts": lv_turnouts,
            "by_category": lv_cats_out,
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
        "by_level": by_level_out,
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
    format: str = "txt",
) -> str:
    """Write a Fugate Operation Density report.

    Runs get_operation_density and formats the result as a human-readable
    report in Fugate's summary style.

    Args:
        path: Path to the source .xtc or .xtce file.
        output_path: Destination path for the report.
        layer_categories: Same as get_operation_density.
        car_length_model_in: Same as get_operation_density.
        train_length_cars: Same as get_operation_density.
        format: Output format — "txt" (default), "md", or "html".

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
        "TRACK BY LEVEL",
    ]
    for lv, lv_info in data["by_level"].items():
        lv_label = f"Level {lv}" if lv != "0" else "Unassigned"
        lines.append(
            f"  {lv_label} — {lv_info['length_real_ft']:.1f} ft"
            f"  {lv_info['car_capacity']} cars"
            f"  {lv_info['turnouts']} turnouts"
        )
        for cat in cat_order:
            if cat in lv_info["by_category"]:
                ci = lv_info["by_category"][cat]
                lines.append(
                    f"    {cat:12s}: {ci['length_real_ft']:7.1f} ft"
                    f"  {ci['car_capacity']:4d} cars"
                    f"  {ci['turnouts']:3d} turnouts"
                )
        for cat, ci in sorted(lv_info["by_category"].items()):
            if cat not in cat_order:
                lines.append(
                    f"    {cat:12s}: {ci['length_real_ft']:7.1f} ft"
                    f"  {ci['car_capacity']:4d} cars"
                    f"  {ci['turnouts']:3d} turnouts"
                )
    lines += [
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
    format = _resolve_format(output_path, format)
    if format == "txt":
        out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    else:
        # Rebuild as MD/HTML
        cat_order = ["mainline", "passing", "storage", "staging", "connecting", "service"]
        ordered_cats = [c for c in cat_order if c in data["by_category"]]
        ordered_cats += [c for c in sorted(data["by_category"]) if c not in ordered_cats]
        tbl_rows = [
            [cat,
             f"{data['by_category'][cat]['length_real_ft']:.1f}",
             str(data['by_category'][cat]['car_capacity']),
             str(data['by_category'][cat]['turnouts'])]
            for cat in ordered_cats
        ]
        tbl_rows.append(["**TOTAL**",
                         f"**{totals['length_real_ft']:.1f}**",
                         f"**{totals['car_capacity']}**",
                         f"**{totals['turnout_count']}**"])
        tbl_hdrs = ["Category", "Track (ft)", "Cars", "Turnouts"]

        title = f"Operation Density Report: {data['title']}"
        # Build per-level flat table rows: Level | Category | ft | Cars | Turnouts
        lv_tbl_rows = []
        for lv, lv_info in data["by_level"].items():
            lv_label = f"Level {lv}" if lv != "0" else "Unassigned"
            for cat in [c for c in cat_order if c in lv_info["by_category"]] + \
                       [c for c in sorted(lv_info["by_category"]) if c not in cat_order]:
                ci = lv_info["by_category"][cat]
                lv_tbl_rows.append([
                    lv_label, cat,
                    f"{ci['length_real_ft']:.1f}",
                    str(ci["car_capacity"]),
                    str(ci["turnouts"]),
                ])
            lv_tbl_rows.append([
                f"**{lv_label} Total**", "",
                f"**{lv_info['length_real_ft']:.1f}**",
                f"**{lv_info['car_capacity']}**",
                f"**{lv_info['turnouts']}**",
            ])
        lv_tbl_hdrs = ["Level", "Category", "Track (ft)", "Cars", "Turnouts"]

        if format == "md":
            md: list[str] = [
                f"# {title}", "",
                f"**Scale:** {data['scale']}  |  "
                f"**Room:** {room['width_ft']}′ × {room['height_ft']}′ = {room['area_sqft']:.0f} sq ft  |  "
                f"**Date:** {datetime.date.today()}", "",
                "## Track by Category", "",
            ]
            md += _md_table(tbl_hdrs, tbl_rows)
            md += ["", "## Track by Level", ""]
            md += _md_table(lv_tbl_hdrs, lv_tbl_rows)
            md += [
                "", "## Operation Density (Joe Fugate, MRH Oct 2014)", "",
                f"| Metric | Value |",
                f"|---|---|",
                f"| Max cars | **{ops['max_cars']}** |",
                f"| Cars moved / session | **{ops['cars_moved_per_session']}** |",
                f"| Max-to-main ratio | **{ops['max_to_main_pct']:.1f}%** → {ops['max_to_main_label']} |",
                f"| Estimated trains | {ops['estimated_trains']} (@ {ops['assumed_train_length_cars']} cars) |",
                "", f"> {data['note']}",
            ]
            out.write_text("\n".join(md) + "\n", encoding="utf-8")
        else:  # html
            html_tbl = _html_table(tbl_hdrs, tbl_rows)
            metrics = _html_table(
                ["Metric", "Value"],
                [["Max cars", f"<strong>{ops['max_cars']}</strong>"],
                 ["Cars moved / session", f"<strong>{ops['cars_moved_per_session']}</strong>"],
                 ["Max-to-main ratio",
                  f"<strong>{ops['max_to_main_pct']:.1f}%</strong> → {ops['max_to_main_label']}"],
                 ["Estimated trains",
                  f"{ops['estimated_trains']} (@ {ops['assumed_train_length_cars']} cars)"]],
            )
            lv_html_tbl = _html_table(lv_tbl_hdrs, lv_tbl_rows)
            body = (
                f"<h1>{title}</h1>"
                f"<p><strong>Scale:</strong> {data['scale']} &nbsp;|&nbsp; "
                f"<strong>Room:</strong> {room['width_ft']}′ × {room['height_ft']}′ = "
                f"{room['area_sqft']:.0f} sq ft &nbsp;|&nbsp; "
                f"<strong>Date:</strong> {datetime.date.today()}</p>"
                f"<h2>Track by Category</h2>{html_tbl}"
                f"<h2>Track by Level</h2>{lv_html_tbl}"
                f"<h2>Operation Density (Joe Fugate, MRH Oct 2014)</h2>{metrics}"
                f"<p><em>{data['note']}</em></p>"
            )
            out.write_text(_html_page(title, body), encoding="utf-8")
    return f"Report written to {out}"


# ---------------------------------------------------------------------------
# Layer management tools
# ---------------------------------------------------------------------------


@mcp.tool()
def add_track_layers(path: str, levels: int | None = None) -> dict:
    """Add standard Fugate track-type layers to an existing .xtc layout file.

    Injects Ln-Main, Ln-Passing, Ln-Storage, Ln-Staging, Ln-Connecting, and
    Ln-Service layers for each physical level. Safe to run at any stage of
    track design — existing track objects and layer IDs are never modified.
    Layers already present by name are skipped.

    New layer IDs are assigned starting from max_existing_id + 1, so there
    is no risk of collision with hand-built or previously edited files.

    Args:
        path: Path to the .xtc layout file (absolute or relative to
              XTRKCAD_PLANS_DIR).
        levels: Number of physical levels to add layers for. If omitted,
                detected automatically from the highest Ln- prefix found
                in existing layer names (minimum 1).

    Returns:
        Dict with output_path, backup_path, added (layer names inserted),
        and already_present (layer names that were already there).
    """
    import re
    from xtrkcad_mcp.generator import _TRACK_COLORS, _TRACK_TYPES, _version_backup

    p = _resolve_plan(path)
    layout = parse_file(p)

    # Detect level count from existing Ln-* names if not provided
    if levels is None:
        max_lv = 0
        for layer in layout.layers.values():
            m = re.match(r"^[Ll](\d+)", layer.name)
            if m:
                max_lv = max(max_lv, int(m.group(1)))
        levels = max(max_lv, 1)

    existing_names = {layer.name for layer in layout.layers.values()}
    max_id = max(layout.layers.keys()) if layout.layers else 0

    # Classify each standard layer as missing or present
    to_add: list[tuple[int, str]] = []   # (level, name)
    already_present: list[str] = []
    for lv in range(1, levels + 1):
        for t_name in _TRACK_TYPES:
            name = f"L{lv}-{t_name}"
            if name in existing_names:
                already_present.append(name)
            else:
                to_add.append((lv, name))

    if not to_add:
        return {
            "output_path": str(p),
            "backup_path": None,
            "added": [],
            "already_present": already_present,
        }

    # Build new LAYERS lines; IDs are strictly additive
    def _lv_color(lv: int) -> int:
        return _TRACK_COLORS[lv - 1] if lv <= len(_TRACK_COLORS) else 0

    new_layer_lines: list[str] = []
    added_names: list[str] = []
    next_id = max_id + 1
    for lv, name in to_add:
        new_layer_lines.append(
            f'LAYERS {next_id} 1 0 1 {_lv_color(lv)} 0 0 0 0 "{name}" '
            f'1 0 0.000000 0.000000 0.000000 0.000000 0.000000'
        )
        added_names.append(name)
        next_id += 1

    # Insert new LAYERS lines just before LAYERS CURRENT
    file_lines = p.read_text(encoding="utf-8", errors="replace").splitlines()
    insert_at = next(
        (i for i, ln in enumerate(file_lines) if ln.strip().startswith("LAYERS CURRENT")),
        None,
    )
    if insert_at is None:
        # Fallback: after the last existing LAYERS line
        insert_at = next(
            (i + 1 for i in range(len(file_lines) - 1, -1, -1)
             if file_lines[i].startswith("LAYERS")),
            len(file_lines),
        )

    updated = file_lines[:insert_at] + new_layer_lines + file_lines[insert_at:]
    backup = _version_backup(p)
    p.write_text("\n".join(updated) + "\n", encoding="utf-8")

    return {
        "output_path": str(p),
        "backup_path": str(backup) if backup else None,
        "added": added_names,
        "already_present": already_present,
    }


@mcp.tool()
def rename_layers(path: str, mapping: dict) -> dict:
    """Rename layers in an existing .xtc layout file by name.

    Renames the name string of matching LAYERS records in-place. Layer IDs
    and all track object references are unchanged — only the display name is
    updated. After renaming to standard Fugate names (e.g. 'L1-Staging'),
    get_operation_density will auto-categorize them without needing an
    explicit layer_categories argument.

    Args:
        path: Path to the .xtc layout file (absolute or relative to
              XTRKCAD_PLANS_DIR).
        mapping: Dict of {old_name: new_name} pairs.
                 Example: {"My Yard": "L1-Staging", "Upper Main": "L2-Main"}

    Returns:
        Dict with output_path, backup_path, renamed (list of old→new pairs
        that were applied), and not_found (old names not present in the file).
    """
    from xtrkcad_mcp.generator import _version_backup

    p = _resolve_plan(path)
    layout = parse_file(p)

    # Build lookup: existing layer name → layer ID
    name_to_id = {layer.name: layer.index for layer in layout.layers.values()}

    renamed: list[dict] = []
    not_found: list[str] = []
    for old_name, new_name in mapping.items():
        if old_name in name_to_id:
            renamed.append({"from": old_name, "to": new_name, "id": name_to_id[old_name]})
        else:
            not_found.append(old_name)

    if not renamed:
        return {
            "output_path": str(p),
            "backup_path": None,
            "renamed": [],
            "not_found": not_found,
        }

    # Apply renames to raw file lines — replace quoted name on each LAYERS line
    file_lines = p.read_text(encoding="utf-8", errors="replace").splitlines()
    rename_map = {r["from"]: r["to"] for r in renamed}
    updated_lines: list[str] = []
    for line in file_lines:
        if line.startswith("LAYERS") and not line.split()[1:2] == ["CURRENT"]:
            # Extract quoted name
            q2 = line.rfind('"')
            q1 = line.rfind('"', 0, q2)
            if q1 >= 0 and q2 > q1:
                name_in_line = line[q1 + 1:q2]
                if name_in_line in rename_map:
                    line = line[:q1 + 1] + rename_map[name_in_line] + line[q2:]
        updated_lines.append(line)

    backup = _version_backup(p)
    p.write_text("\n".join(updated_lines) + "\n", encoding="utf-8")

    return {
        "output_path": str(p),
        "backup_path": str(backup) if backup else None,
        "renamed": renamed,
        "not_found": not_found,
    }


# ---------------------------------------------------------------------------
# Report format helpers
# ---------------------------------------------------------------------------

_EXT_FORMAT = {".html": "html", ".htm": "html", ".md": "md", ".json": "json"}


def _resolve_format(output_path: str, explicit: str) -> str:
    """Return explicit format if given (non-default), else infer from file extension."""
    if explicit != "txt":
        return explicit
    suffix = Path(output_path).suffix.lower()
    return _EXT_FORMAT.get(suffix, "txt")


def _md_table(headers: list[str], rows: list[list[str]]) -> list[str]:
    """Build a GitHub-flavoured Markdown table, returning one line per row."""
    widths = [
        max(len(h), max((len(r[i]) for r in rows), default=0))
        for i, h in enumerate(headers)
    ]
    def row_line(cells: list[str]) -> str:
        return "| " + " | ".join(c.ljust(w) for c, w in zip(cells, widths)) + " |"
    sep = ["-" * w for w in widths]
    return [row_line(headers), row_line(sep)] + [row_line(r) for r in rows]


def _html_table(headers: list[str], rows: list[list[str]],
                row_class_fn=None) -> str:
    """Build an HTML table string. row_class_fn(row) → optional CSS class for <tr>."""
    ths = "".join(f"<th>{h}</th>" for h in headers)
    body_rows = []
    for r in rows:
        cls = f' class="{row_class_fn(r)}"' if row_class_fn else ""
        tds = "".join(f"<td>{c}</td>" for c in r)
        body_rows.append(f"<tr{cls}>{tds}</tr>")
    return f"<table><thead><tr>{ths}</tr></thead><tbody>{''.join(body_rows)}</tbody></table>"


def _html_page(title: str, body: str) -> str:
    """Wrap body HTML in a minimal styled standalone page."""
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>{title}</title>
<style>
body{{font-family:sans-serif;max-width:960px;margin:2em auto;color:#222}}
h1{{border-bottom:2px solid #555;padding-bottom:.3em}}
h2{{border-bottom:1px solid #ccc;padding-bottom:.2em;margin-top:1.6em}}
table{{border-collapse:collapse;width:100%;margin:.8em 0}}
th,td{{border:1px solid #ccc;padding:5px 10px;text-align:left}}
th{{background:#f4f4f4;font-weight:bold}}
tr.pass td:last-child{{color:#267326;font-weight:bold}}
tr.marginal td:last-child{{color:#7a5200;font-weight:bold}}
tr.fail td:last-child{{color:#cc2200;font-weight:bold}}
.summary{{background:#f9f9f9;border:1px solid #ddd;padding:.8em 1.2em;
          border-radius:4px;margin:1em 0}}
p{{margin:.4em 0}}
</style>
</head>
<body>
{body}
</body>
</html>"""


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
    format: str = "txt",
) -> str:
    """Write a full layout summary report.

    Covers layout header, track counts by type, track lengths per layer,
    curve radius analysis, and (if layer_categories is given) Operation Density.

    Args:
        path: Path to the source .xtc or .xtce file.
        output_path: Destination path for the report.
        layer_categories: Optional layer→category mapping (same as
            get_operation_density). Required for the OD section.
        format: Output format — "txt" (default), "md", or "html".

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
    format = _resolve_format(output_path, format)
    if format == "txt":
        out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    else:
        cpf = cars_per_real_ft(scale)
        count_rows = [[k, str(v)] for k, v in sorted(layout.track_counts().items())]
        count_rows.append(["**TOTAL**", f"**{len(layout.tracks)}**"])
        layer_data = []
        for idx, ft in sorted(by_layer.items()):
            name = layout.layers[idx].name if idx in layout.layers else f"Layer {idx}"
            layer_data.append({"layer": idx, "name": name, "feet": round(ft, 1),
                                "cars": int(ft * cpf), "turnouts": to_by_layer.get(idx, 0)})
        radii = layout.curve_radii()
        curve_data = {"count": len(radii), "min": round(min(radii), 1) if radii else None,
                      "max": round(max(radii), 1) if radii else None,
                      "mean": round(sum(radii) / len(radii), 1) if radii else None}
        od_data = (get_operation_density(path, layer_categories)["operations"]
                   if layer_categories else None)
        title = f"Layout Report: {layout.title1}"
        if format == "json":
            import json
            out.write_text(json.dumps({
                "title": layout.title1, "title2": layout.title2,
                "scale": scale, "version": layout.version,
                "room": {"width_ft": round(room_w, 1), "height_ft": round(room_h, 1),
                         "area_sqft": round(room_w * room_h, 0)},
                "date": str(datetime.date.today()),
                "track_counts": layout.track_counts(),
                "total_tracks": len(layout.tracks),
                "layers": layer_data,
                "total_ft": round(total_ft, 1),
                "total_cars": int(total_ft * cpf),
                "curves": curve_data,
                "operation_density": od_data,
            }, indent=2), encoding="utf-8")
        elif format == "md":
            layer_rows = [[str(d["layer"]), d["name"], f"{d['feet']:.1f}",
                           str(d["cars"]), str(d["turnouts"])] for d in layer_data]
            layer_rows.append(["—", "**TOTAL**", f"**{total_ft:.1f}**",
                                f"**{int(total_ft * cpf)}**",
                                f"**{sum(to_by_layer.values())}**"])
            md = [f"# {title}", ""]
            if layout.title2:
                md += [f"*{layout.title2}*", ""]
            md += [
                f"| Field | Value |", f"|---|---|",
                f"| Scale | {scale} |",
                f"| Room | {room_w:.1f}′ × {room_h:.1f}′ = {room_w * room_h:.0f} sq ft |",
                f"| Date | {datetime.date.today()} |",
                "", "## Track Counts by Type", "",
            ]
            md += _md_table(["Type", "Count"], count_rows)
            md += ["", "## Track Lengths by Layer", ""]
            md += _md_table(["Layer", "Name", "Feet", "Cars", "Turnouts"], layer_rows)
            md += ["", "## Curve Analysis", ""]
            if not radii:
                md.append("No curves found.")
            else:
                md += [f"- Curves: {curve_data['count']}",
                       f"- Min radius: {curve_data['min']}\"",
                       f"- Max radius: {curve_data['max']}\"",
                       f"- Mean radius: {curve_data['mean']}\""]
            if od_data:
                md += ["", "## Operation Density (Joe Fugate, MRH Oct 2014)", "",
                       f"| Metric | Value |", f"|---|---|",
                       f"| Max cars | **{od_data['max_cars']}** |",
                       f"| Cars moved / session | **{od_data['cars_moved_per_session']}** |",
                       f"| Max-to-main | **{od_data['max_to_main_pct']:.1f}%** → "
                       f"{od_data['max_to_main_label']} |"]
            out.write_text("\n".join(md) + "\n", encoding="utf-8")
        else:  # html
            ct_tbl = _html_table(["Type", "Count"], [[k, str(v)]
                                  for k, v in sorted(layout.track_counts().items())])
            ly_tbl = _html_table(
                ["Layer", "Name", "Feet", "Cars", "Turnouts"],
                [[str(d["layer"]), d["name"], f"{d['feet']:.1f}",
                  str(d["cars"]), str(d["turnouts"])] for d in layer_data],
            )
            curve_html = "<p>No curves found.</p>" if not radii else (
                f"<p>Curves: {curve_data['count']} &nbsp;|&nbsp; "
                f"Min: {curve_data['min']}\" &nbsp;|&nbsp; "
                f"Max: {curve_data['max']}\" &nbsp;|&nbsp; "
                f"Mean: {curve_data['mean']}\"</p>"
            )
            body = (
                f"<h1>{title}</h1>"
                + (f"<p><em>{layout.title2}</em></p>" if layout.title2 else "")
                + f"<div class='summary'>"
                f"<p><strong>Scale:</strong> {scale} &nbsp;|&nbsp; "
                f"<strong>Room:</strong> {room_w:.1f}′ × {room_h:.1f}′ = "
                f"{room_w * room_h:.0f} sq ft &nbsp;|&nbsp; "
                f"<strong>Date:</strong> {datetime.date.today()}</p></div>"
                f"<h2>Track Counts</h2>{ct_tbl}"
                f"<h2>Track Lengths by Layer</h2>{ly_tbl}"
                f"<h2>Curve Analysis</h2>{curve_html}"
            )
            if od_data:
                od_tbl = _html_table(
                    ["Metric", "Value"],
                    [["Max cars", f"<strong>{od_data['max_cars']}</strong>"],
                     ["Cars moved / session",
                      f"<strong>{od_data['cars_moved_per_session']}</strong>"],
                     ["Max-to-main",
                      f"<strong>{od_data['max_to_main_pct']:.1f}%</strong> → "
                      f"{od_data['max_to_main_label']}"]],
                )
                body += f"<h2>Operation Density</h2>{od_tbl}"
            out.write_text(_html_page(title, body), encoding="utf-8")
    return f"Report written to {out}"


@mcp.tool()
def write_equipment_report(path: str, output_path: str,
                            format: str = "txt") -> str:
    """Write an equipment suitability report based on the layout's minimum curve radius.

    Compares the minimum curve radius against published minimums for common
    equipment classes. Shows PASS / MARGINAL (within 2\" of minimum) / FAIL.

    Args:
        path: Path to the source .xtc or .xtce file.
        output_path: Destination path for the report.
        format: Output format — "txt" (default), "md", "html", or "json".

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
    format = _resolve_format(output_path, format)
    if format == "txt":
        out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    else:
        # Build equipment rows for structured formats
        if min_r is None:
            eq_rows = []
        else:
            eq_rows = []
            for name, ho_thresh in _EQUIPMENT_THRESHOLDS:
                thresh = ho_thresh * sf
                if min_r >= thresh:
                    status = "PASS"
                elif min_r >= thresh - 2.0 * sf:
                    status = "MARGINAL"
                else:
                    status = "FAIL"
                eq_rows.append({"name": name, "min_radius_in": round(thresh, 1),
                                 "status": status})
        title = f"Equipment Suitability Report: {layout.title1}"
        if format == "json":
            import json
            out.write_text(json.dumps({
                "title": layout.title1, "scale": scale,
                "date": str(datetime.date.today()),
                "min_curve_radius_in": min_r,
                "equipment": eq_rows,
                "note": f"Thresholds scaled from HO (factor {sf:.3f} for {scale}). "
                        "MARGINAL = within 2\" scaled of minimum.",
            }, indent=2), encoding="utf-8")
        elif format == "md":
            md = [f"# {title}", "",
                  f"**Scale:** {scale}  |  **Date:** {datetime.date.today()}"]
            if min_r is None:
                md += ["", "No usable curves found."]
            else:
                md += ["", f"**Minimum curve radius:** {min_r:.1f}\"  "
                           "(curves < 9\" excluded as decorative)", ""]
                md += _md_table(
                    ["Equipment Class", "Min Radius", "Status"],
                    [[r["name"], f"{r['min_radius_in']:.1f}\"", r["status"]]
                     for r in eq_rows],
                )
                md += ["", f"*Thresholds scaled from HO (factor {sf:.3f} for {scale}). "
                           "MARGINAL = within 2\" scaled of minimum; test with your models.*"]
            out.write_text("\n".join(md) + "\n", encoding="utf-8")
        else:  # html
            def _row_cls(row):
                return row[2].lower()
            if min_r is None:
                tbl_html = "<p>No usable curves found.</p>"
            else:
                tbl_html = _html_table(
                    ["Equipment Class", "Min Radius", "Status"],
                    [[r["name"], f"{r['min_radius_in']:.1f}\"", r["status"]]
                     for r in eq_rows],
                    row_class_fn=lambda r: r[2].lower(),
                )
            body = (
                f"<h1>{title}</h1>"
                f"<p><strong>Scale:</strong> {scale} &nbsp;|&nbsp; "
                f"<strong>Date:</strong> {datetime.date.today()}</p>"
            )
            if min_r is not None:
                body += (
                    f"<p><strong>Minimum curve radius:</strong> {min_r:.1f}\" "
                    f"(curves &lt; 9\" excluded as decorative)</p>"
                    f"<h2>Equipment Compatibility</h2>{tbl_html}"
                    f"<p><em>Thresholds scaled from HO (factor {sf:.3f} for {scale}). "
                    f"MARGINAL = within 2\" scaled of minimum; test with your specific models.</em></p>"
                )
            out.write_text(_html_page(title, body), encoding="utf-8")
    return f"Report written to {out}"


@mcp.tool()
def write_turnout_report(path: str, output_path: str,
                          format: str = "txt") -> str:
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

    has_heavy = any(
        (to_by_layer.get(idx, 0) / ft * 100.0) > 10.0
        for idx, ft in by_layer.items() if ft > 0
    )
    if has_heavy:
        lines += ["", "*** Layers with density > 10/100 ft are switching-heavy (possible staging yard)."]

    out = Path(output_path).expanduser()
    format = _resolve_format(output_path, format)
    if format == "txt":
        out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    else:
        layer_rows = []
        for idx, ft in sorted(by_layer.items()):
            name = layout.layers[idx].name if idx in layout.layers else f"Layer {idx}"
            turnouts = to_by_layer.get(idx, 0)
            density = (turnouts / ft * 100.0) if ft > 0 else 0.0
            flag = "⚠" if density > 10.0 else ""
            layer_rows.append([str(idx), name, f"{ft:.1f}", str(turnouts),
                                f"{density:.1f}", flag])
        partial_rows = [{"id": t.id, "endpoints": len(t.endpoints), "layer": t.layer}
                        for t in layout.tracks if t.kind == "TURNOUT" and len(t.endpoints) < 3]
        title = f"Turnout Report: {layout.title1}"
        if format == "json":
            import json
            out.write_text(json.dumps({
                "title": layout.title1, "scale": layout.scale,
                "date": str(datetime.date.today()),
                "total_turnouts": total_turnouts,
                "total_track_ft": round(total_ft, 1),
                "global_density_per_100ft": round(global_density, 1),
                "by_layer": [{"layer": int(r[0]), "name": r[1], "feet": float(r[2]),
                               "turnouts": int(r[3]), "density_per_100ft": float(r[4])}
                              for r in layer_rows],
                "partial_turnouts": partial_rows,
            }, indent=2), encoding="utf-8")
        elif format == "md":
            md = [f"# {title}", "",
                  f"**Scale:** {layout.scale}  |  **Date:** {datetime.date.today()}", "",
                  "## Overall", "",
                  f"- Total turnouts: **{total_turnouts}**",
                  f"- Total track: **{total_ft:.1f} ft**",
                  f"- Density: **{global_density:.1f}** per 100 ft", "",
                  "## By Layer", ""]
            md += _md_table(["Layer", "Name", "Feet", "Turnouts", "Density/100ft", ""],
                             layer_rows)
            md += ["", "## Partial Turnouts (fewer than 3 endpoints)", ""]
            if partial_rows:
                for p in partial_rows:
                    md.append(f"- Track ID {p['id']}: {p['endpoints']} endpoint(s), layer {p['layer']}")
            else:
                md.append("None found.")
            out.write_text("\n".join(md) + "\n", encoding="utf-8")
        else:  # html
            tbl = _html_table(["Layer", "Name", "Feet", "Turnouts", "Density/100ft", "Flag"],
                               layer_rows)
            body = (
                f"<h1>{title}</h1>"
                f"<p><strong>Scale:</strong> {layout.scale} &nbsp;|&nbsp; "
                f"<strong>Date:</strong> {datetime.date.today()}</p>"
                f"<div class='summary'>"
                f"<p><strong>Total turnouts:</strong> {total_turnouts}</p>"
                f"<p><strong>Total track:</strong> {total_ft:.1f} ft</p>"
                f"<p><strong>Density:</strong> {global_density:.1f} per 100 ft</p></div>"
                f"<h2>By Layer</h2>{tbl}"
            )
            if partial_rows:
                items = "".join(f"<li>Track ID {p['id']}: {p['endpoints']} endpoint(s), "
                                f"layer {p['layer']}</li>" for p in partial_rows)
                body += f"<h2>Partial Turnouts</h2><ul>{items}</ul>"
            out.write_text(_html_page(title, body), encoding="utf-8")
    return f"Report written to {out}"


@mcp.tool()
def write_gaps_report(path: str, output_path: str,
                       format: str = "txt") -> str:
    """Write an open endpoints (track gaps) report.

    Lists every unconnected endpoint with its track ID, position, and angle.
    Groups pairs of endpoints within 0.1 model inches of each other as
    near misses (should likely be connected).

    Turntable stall endpoints are intentionally open and are reported
    separately — they do not appear in the gap analysis.

    Args:
        path: Path to the source .xtc or .xtce file.
        output_path: Destination path for the report.
        format: Output format — "txt" (default), "md", "html", or "json".

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
    format = _resolve_format(output_path, format)
    if format == "txt":
        out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    else:
        title = f"Track Gaps Report: {layout.title1}"
        nm_list = [{"track_a": open_eps[i]["track_id"],
                    "track_b": open_eps[j]["track_id"],
                    "x_a": round(open_eps[i]["x"], 3),
                    "y_a": round(open_eps[i]["y"], 3),
                    "x_b": round(open_eps[j]["x"], 3),
                    "y_b": round(open_eps[j]["y"], 3),
                    "gap_in": round(math.hypot(open_eps[j]["x"] - open_eps[i]["x"],
                                               open_eps[j]["y"] - open_eps[i]["y"]), 4)}
                   for i, j in pairs]
        ep_list = [{"track_id": ep["track_id"], "x": round(ep["x"], 3),
                    "y": round(ep["y"], 3), "angle": round(ep["angle"], 2),
                    "near_miss": i in near_miss_indices}
                   for i, ep in enumerate(open_eps)]
        if format == "json":
            import json
            out.write_text(json.dumps({
                "title": layout.title1, "scale": layout.scale,
                "date": str(datetime.date.today()),
                "turntable_stalls": len(tt_stalls),
                "open_endpoints": len(open_eps),
                "near_miss_pairs": len(pairs),
                "isolated_open_ends": len(isolated),
                "near_misses": nm_list,
                "endpoints": ep_list,
            }, indent=2), encoding="utf-8")
        elif format == "md":
            md = [f"# {title}", "",
                  f"**Scale:** {layout.scale}  |  **Date:** {datetime.date.today()}", "",
                  "## Summary", "",
                  f"| Item | Count |", f"|---|---|",
                  f"| Turntable stalls (open by design) | {len(tt_stalls)} |",
                  f"| Track gaps to check | **{len(open_eps)}** |",
                  f"| Near-miss pairs (should connect) | {len(pairs)} |",
                  f"| Isolated open ends | {len(isolated)} |"]
            if nm_list:
                md += ["", "## Near-Miss Pairs", ""]
                md += _md_table(
                    ["Track A", "Track B", "Gap (\")"],
                    [[str(r["track_a"]), str(r["track_b"]), f"{r['gap_in']:.4f}"]
                     for r in nm_list],
                )
            if ep_list:
                md += ["", "## All Open Endpoints", ""]
                md += _md_table(
                    ["Track", "X", "Y", "Angle", "Note"],
                    [[str(r["track_id"]), f"{r['x']:.3f}", f"{r['y']:.3f}",
                      f"{r['angle']:.2f}", "near-miss" if r["near_miss"] else ""]
                     for r in ep_list],
                )
            out.write_text("\n".join(md) + "\n", encoding="utf-8")
        else:  # html
            summary_tbl = _html_table(
                ["Item", "Count"],
                [["Turntable stalls (open by design)", str(len(tt_stalls))],
                 ["Track gaps to check", f"<strong>{len(open_eps)}</strong>"],
                 ["Near-miss pairs (should connect)", str(len(pairs))],
                 ["Isolated open ends", str(len(isolated))]],
            )
            body = (
                f"<h1>{title}</h1>"
                f"<p><strong>Scale:</strong> {layout.scale} &nbsp;|&nbsp; "
                f"<strong>Date:</strong> {datetime.date.today()}</p>"
                f"<h2>Summary</h2>{summary_tbl}"
            )
            if nm_list:
                nm_tbl = _html_table(
                    ["Track A", "Track B", "Gap (\")"],
                    [[str(r["track_a"]), str(r["track_b"]), f"{r['gap_in']:.4f}"]
                     for r in nm_list],
                )
                body += f"<h2>Near-Miss Pairs</h2>{nm_tbl}"
            if ep_list:
                ep_tbl = _html_table(
                    ["Track", "X", "Y", "Angle", "Note"],
                    [[str(r["track_id"]), f"{r['x']:.3f}", f"{r['y']:.3f}",
                      f"{r['angle']:.2f}", "near-miss" if r["near_miss"] else ""]
                     for r in ep_list],
                )
                body += f"<h2>All Open Endpoints</h2>{ep_tbl}"
            out.write_text(_html_page(title, body), encoding="utf-8")
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


@mcp.tool()
def find_dead_connections(path: str) -> list[dict]:
    """Find T (connected) endpoint records that reference a nonexistent track ID.

    These arise when a track is deleted while other tracks still hold T records
    pointing to it. Returns one entry per dangling endpoint.

    Args:
        path: Path to the .xtc layout file (absolute or relative to XTRKCAD_PLANS_DIR).
    """
    p = _resolve_plan(path)
    layout = parse_file(p)
    valid_ids = {t.id for t in layout.tracks}
    dead = []
    for track in layout.tracks:
        for ep in track.endpoints:
            if ep.connected_to is not None and ep.connected_to not in valid_ids:
                dead.append({
                    "track_id": track.id,
                    "dead_reference": ep.connected_to,
                    "x": ep.x,
                    "y": ep.y,
                    "angle": ep.angle,
                })
    return dead


def _fix_dead_t_lines(lines: list[str], dead_ids: set[int]) -> tuple[list[str], int]:
    """Replace T sub-records referencing dead_ids with E (open endpoint) records."""
    result = []
    count = 0
    for line in lines:
        if line.startswith("\t"):
            parts = line.strip().split()
            if parts and parts[0] == "T" and len(parts) >= 5:
                try:
                    if int(parts[1]) in dead_ids:
                        result.append(f"\tE {parts[2]} {parts[3]} {parts[4]}")
                        count += 1
                        continue
                except (ValueError, IndexError):
                    pass
        result.append(line)
    return result, count


@mcp.tool()
def fix_dead_connections(path: str) -> str:
    """Convert dangling T endpoint records to open E endpoints in place.

    After deleting a track, any track that was connected to it retains a T
    record pointing to the now-missing ID. This tool finds those records and
    rewrites them as open E endpoints so the gaps show up correctly.

    Args:
        path: Path to the .xtc layout file (absolute or relative to XTRKCAD_PLANS_DIR).
    """
    p = _resolve_plan(path)
    layout = parse_file(p)
    valid_ids = {t.id for t in layout.tracks}
    dead_ids: set[int] = set()
    for track in layout.tracks:
        for ep in track.endpoints:
            if ep.connected_to is not None and ep.connected_to not in valid_ids:
                dead_ids.add(ep.connected_to)
    if not dead_ids:
        return "No dead connections found"
    lines = p.read_text(encoding="utf-8", errors="replace").splitlines()
    updated, count = _fix_dead_t_lines(lines, dead_ids)
    p.write_text("\n".join(updated) + "\n", encoding="utf-8")
    return f"Fixed {count} dead endpoint(s) referencing deleted track ID(s): {sorted(dead_ids)}"


# ---------------------------------------------------------------------------
# Station distance tools
# ---------------------------------------------------------------------------


@mcp.tool()
def get_stations(path: str) -> list[dict]:
    """List all stations identified by STATION: NOTE objects in the layout.

    Stations are marked by placing a text NOTE in XTrkCAD whose text begins
    with 'STATION:' (case-insensitive), e.g. 'STATION: Springfield'.
    Each station is snapped to its nearest track endpoint for graph routing.

    Args:
        path: Path to the .xtc layout file (absolute or relative to XTRKCAD_PLANS_DIR).
    """
    p = _resolve_plan(path)
    layout = parse_file(p)
    stations = extract_stations(layout)
    return [
        {
            "name": s.name,
            "note_id": s.note_id,
            "x": round(s.x, 4),
            "y": round(s.y, 4),
            "nearest_track_id": s.nearest_ep[0],
            "nearest_ep_idx": s.nearest_ep[1],
            "snap_distance_in": round(s.snap_dist, 4),
        }
        for s in stations
    ]


@mcp.tool()
def get_station_distances(
    path: str,
    layer_categories: dict | None = None,
    route_layer_names: list[str] | None = None,
    exclude_turnouts: bool = False,
) -> list[dict]:
    """Compute track-graph distances between all STATION: notes in the layout.

    Distances are the shortest path along connected track, weighted by track
    length.  Results are returned in both model inches and prototypical feet.

    Layer filtering (optional):
      Provide layer_categories (e.g. {"0": "mainline", "1": "connecting"}) and
      route_layer_names (e.g. ["mainline", "connecting"]) to restrict the path
      to specific track categories.  Useful for mainline-only routing.

    Turnout handling:
      Set exclude_turnouts=True to treat TURNOUT lengths as zero — the switch
      acts as a free connection rather than adding distance.  Use this when
      measuring siding/storage capacity so the switch throat is not counted.

    Args:
        path: Path to the .xtc layout file.
        layer_categories: Optional map of layer index → category name.
        route_layer_names: Optional list of category names to restrict routing.
        exclude_turnouts: If True, TURNOUT track lengths are excluded from totals.
    """
    p = _resolve_plan(path)
    layout = parse_file(p)
    results = compute_distances(
        layout,
        layer_categories=layer_categories,
        route_layer_names=route_layer_names,
        exclude_turnouts=exclude_turnouts,
    )
    return [
        {
            "from": r.from_station,
            "to": r.to_station,
            "distance_model_in": round(r.distance_model_in, 2),
            "distance_proto_ft": round(r.distance_proto_ft, 1),
            "reachable": r.reachable,
        }
        for r in results
    ]


@mcp.tool()
def write_station_distance_report(
    path: str,
    output_path: str,
    layer_categories: dict | None = None,
    route_layer_names: list[str] | None = None,
    exclude_turnouts: bool = False,
    format: str = "md",
) -> str:
    """Write a station distance report to a file.

    Reports the shortest track-graph distance between every pair of STATION:
    notes in both model inches and prototypical feet.

    Supports layer filtering and turnout exclusion — see get_station_distances
    for parameter details.

    Args:
        path: Path to the .xtc layout file.
        output_path: Where to write the report (.md or .json).
        layer_categories: Optional map of layer index → category name.
        route_layer_names: Optional list of category names to restrict routing.
        exclude_turnouts: If True, TURNOUT lengths are excluded from totals.
        format: Output format — "md" (default) or "json".
    """
    p = _resolve_plan(path)
    layout = parse_file(p)
    fmt = _resolve_format(output_path, format)
    stations = extract_stations(layout)
    results = compute_distances(
        layout,
        layer_categories=layer_categories,
        route_layer_names=route_layer_names,
        exclude_turnouts=exclude_turnouts,
    )
    out = Path(output_path)

    scale = layout.scale or "?"
    title = layout.title1 or p.stem
    filter_note = ""
    if route_layer_names:
        filter_note = f" (layers: {', '.join(route_layer_names)})"
    if exclude_turnouts:
        filter_note += " (turnouts excluded)"

    if fmt == "json":
        import json
        payload = {
            "layout": title,
            "scale": scale,
            "route_filter": route_layer_names or "all",
            "exclude_turnouts": exclude_turnouts,
            "stations": [s.name for s in stations],
            "distances": [
                {
                    "from": r.from_station,
                    "to": r.to_station,
                    "distance_model_in": round(r.distance_model_in, 2),
                    "distance_proto_ft": round(r.distance_proto_ft, 1),
                    "reachable": r.reachable,
                }
                for r in results
            ],
        }
        out.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        return f"Wrote JSON station distance report to {out} ({len(results)} pairs)"

    # Markdown
    lines: list[str] = [
        f"# Station Distance Report — {title} ({scale}){filter_note}",
        "",
    ]
    if not stations:
        lines.append(
            "_No stations found. Add NOTE objects with text beginning 'STATION: <name>'._"
        )
    elif not results:
        lines.append("_Only one station found — need at least two to compute distances._")
    else:
        headers = ["From", "To", "Model (in)", f"Proto (ft) [{scale}]", "Reachable"]
        rows = [
            [
                r.from_station,
                r.to_station,
                f"{r.distance_model_in:.2f}" if r.reachable else "—",
                f"{r.distance_proto_ft:.0f}" if r.reachable else "—",
                "yes" if r.reachable else "no",
            ]
            for r in results
        ]
        lines.extend(_md_table(headers, rows))
        lines.append("")
        lines.append(
            f"_Stations: {', '.join(s.name for s in stations)}_"
        )

    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return f"Wrote station distance report to {out} ({len(stations)} station(s), {len(results)} pair(s))"


@mcp.tool()
def get_siding_capacities(path: str) -> list[dict]:
    """Report usable track length and car capacity for every SIDING:/STORAGE: note.

    Capacity markers use NOTE objects whose text begins with 'SIDING:' or
    'STORAGE:' (case-insensitive), e.g. 'SIDING: Springfield Freight House'.

    The measurement walks connected non-TURNOUT tracks (STRAIGHT, CURVE, etc.)
    from the nearest endpoint, so switch/turnout geometry is excluded.  The
    Fugate cars-per-real-foot formula converts usable length to whole car count.

    Args:
        path: Path to the .xtc layout file (absolute or relative to XTRKCAD_PLANS_DIR).

    Returns a list of dicts with keys:
        name            — marker label (text after 'SIDING:' / 'STORAGE:')
        kind            — "siding" or "storage"
        nearest_track_id
        length_model_in — usable length in model inches (turnouts excluded)
        length_real_ft  — prototypical feet (scale-dependent)
        max_cars        — whole cars that fit (Fugate formula, no partial)
    """
    p = _resolve_plan(path)
    layout = parse_file(p)
    results = compute_capacities(layout)
    return [
        {
            "name": r.name,
            "kind": r.kind,
            "nearest_track_id": r.nearest_track_id,
            "length_model_in": round(r.length_model_in, 2),
            "length_real_ft": round(r.length_real_ft, 1),
            "max_cars": r.max_cars,
        }
        for r in results
    ]


# ---------------------------------------------------------------------------
# Tools — SVG visual views
# ---------------------------------------------------------------------------


@mcp.tool()
def write_plan_view(
    config_path: str,
    output_path: str,
    level: int = 0,
) -> str:
    """Generate a top-down SVG plan view from a layout config.

    Draws the room outline, floor plan features (restricted zones, door
    clearances, partitions), and benchwork sections.  Each benchwork section
    is labelled with its short name and elevation.

    Args:
        config_path: Path to the YAML layout config (or benchwork_file path).
        output_path: Destination .svg path.
        level: Deck level to show — 0 = all levels, 1 = Level 1 only, etc.

    Returns:
        Absolute path of the written SVG file.
    """
    return generate_plan_view(config_path, output_path, level=level)


@mcp.tool()
def write_elevation_view(
    config_path: str,
    output_path: str,
    wall: str = "west",
    levels: list[int] | None = None,
) -> str:
    """Generate a wall-profile elevation SVG from a layout config.

    Shows benchwork cross-sections as seen from outside looking in:
    the along-wall axis is horizontal, height above floor is vertical.
    Bars are drawn at each section's elevation with its thickness shown.

    Args:
        config_path: Path to the YAML layout config.
        output_path: Destination .svg path.
        wall: Which wall to profile — "west", "south", "east", or "north".
        levels: List of deck levels to include, e.g. [1, 2].  None = all.

    Returns:
        Absolute path of the written SVG file.
    """
    return generate_elevation_view(config_path, output_path, wall=wall, levels=levels)


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
