"""Layout config parser — YAML config file → validated LayoutConfig."""

import math
import re
from dataclasses import dataclass, field
from pathlib import Path

import yaml

# Minimum curve radius in model inches, by scale
SCALE_MIN_RADIUS_IN: dict[str, float] = {
    "Z": 7.0, "N": 9.75, "TT": 12.0, "HO": 18.0,
    "S": 24.0, "O": 36.0, "G": 48.0, "I": 32.0,
    "Nn3": 9.75, "HOn3": 18.0, "On3": 36.0, "Sn3": 24.0,
}

KNOWN_SCALES = frozenset(SCALE_MIN_RADIUS_IN)

VALID_ELEMENT_TYPES = frozenset({
    "yard", "staging", "mainline", "station", "helix", "siding", "module",
})

_CELL_RE = re.compile(r'^([A-Z]+)(\d+)$')
_RANGE_RE = re.compile(r'^([A-Z]+)(\d+)-([A-Z]+)(\d+)$')


def _default_grid_cell_ft(radius_in: float) -> float:
    """Grid cell = 2 × curve diameter rounded up to nearest foot, min 3."""
    return max(3.0, math.ceil((2.0 * radius_in) / 12.0))


def _parse_cell_or_range(s: str) -> tuple[str, int, str, int]:
    """'A3' → (A,3,A,3);  'A3-C5' → (A,3,C,5)."""
    m = _RANGE_RE.match(s)
    if m:
        return m.group(1), int(m.group(2)), m.group(3), int(m.group(4))
    m = _CELL_RE.match(s)
    if m:
        col, row = m.group(1), int(m.group(2))
        return col, row, col, row
    raise ValueError(f"invalid cell/range {s!r} — expected A3 or A3-C5")


def _parse_room(s: str) -> tuple[float, float]:
    """'12x16' or '12 x 16' → (width_ft, depth_ft)."""
    s = str(s).strip().replace("×", "x").replace(" ", "")
    parts = s.lower().split("x")
    if len(parts) != 2:
        raise ValueError(f"invalid room size {s!r} — expected WxD in feet")
    try:
        return float(parts[0]), float(parts[1])
    except ValueError:
        raise ValueError(f"invalid room size {s!r} — values must be numbers")


def _parse_length_in(s: str) -> float:
    """'22in', '22\"', '18.5ft', '18.5' → model inches."""
    s = str(s).strip().lower()
    if s.endswith("in") or s.endswith('"'):
        return float(s.rstrip('in"').strip())
    if s.endswith("ft") or s.endswith("'"):
        return float(s.rstrip("ft'").strip()) * 12.0
    return float(s)


@dataclass
class GridPlacement:
    col_start: str
    row_start: int
    col_end: str
    row_end: int
    element_type: str
    params: str
    raw: str = ""

    @property
    def cell_range(self) -> str:
        if self.col_start == self.col_end and self.row_start == self.row_end:
            return f"{self.col_start}{self.row_start}"
        return f"{self.col_start}{self.row_start}-{self.col_end}{self.row_end}"


@dataclass
class LayoutConfig:
    # Required
    name: str = ""
    scale: str = ""
    room_width_ft: float = 0.0
    room_depth_ft: float = 0.0

    # Optional — 0.0 / "" = use scale default
    mainline: str = "single"
    curve_radius_in: float = 0.0
    switch_size: str = "#6"
    levels: int = 1
    level_separation_in: float = 18.0
    level_break_row: str = ""
    grid_size_ft: float = 0.0

    obstructions: list[str] = field(default_factory=list)
    placements: list[GridPlacement] = field(default_factory=list)


@dataclass
class ConfigResult:
    config: LayoutConfig
    missing: list[tuple[str, str]]  # (field_name, question_text)
    warnings: list[str]
    summary: str
    ready: bool                     # True when no required fields are missing


def _parse_grid_entry(s: str) -> GridPlacement:
    """'A3-C5=yard=3 tracks, single-ended' → GridPlacement."""
    parts = s.split("=", 2)
    if len(parts) < 2:
        raise ValueError(f"need at least cell=type, got {s!r}")
    cell_part = parts[0].strip().upper()
    element_type = parts[1].strip().lower()
    params = parts[2].strip() if len(parts) > 2 else ""
    if element_type not in VALID_ELEMENT_TYPES:
        raise ValueError(
            f"unknown element type {element_type!r}; "
            f"valid: {', '.join(sorted(VALID_ELEMENT_TYPES))}"
        )
    col_s, row_s, col_e, row_e = _parse_cell_or_range(cell_part)
    return GridPlacement(col_s, row_s, col_e, row_e, element_type, params, raw=s)


def load_config(path: str | Path) -> ConfigResult:
    """Parse a YAML layout config file. Always returns a ConfigResult."""
    p = Path(path).expanduser()
    warnings: list[str] = []
    config = LayoutConfig()

    if not p.exists():
        warnings.append(f"config file not found: {p}")
        return ConfigResult(
            config=config,
            missing=_required_missing(config),
            warnings=warnings,
            summary="",
            ready=False,
        )

    try:
        with open(p) as f:
            raw = yaml.safe_load(f) or {}
    except yaml.YAMLError as e:
        warnings.append(f"YAML parse error: {e}")
        return ConfigResult(
            config=config,
            missing=_required_missing(config),
            warnings=warnings,
            summary="",
            ready=False,
        )

    if not isinstance(raw, dict):
        warnings.append("config file must be a YAML mapping at the top level")
        return ConfigResult(
            config=config,
            missing=_required_missing(config),
            warnings=warnings,
            summary="",
            ready=False,
        )

    # --- Required fields ---
    if "name" in raw:
        config.name = str(raw["name"]).strip()
    if "scale" in raw:
        config.scale = str(raw["scale"]).strip()
        if config.scale not in KNOWN_SCALES:
            warnings.append(
                f"unrecognised scale {config.scale!r}; "
                f"known scales: {', '.join(sorted(KNOWN_SCALES))}"
            )
    if "room" in raw:
        try:
            config.room_width_ft, config.room_depth_ft = _parse_room(str(raw["room"]))
        except ValueError as e:
            warnings.append(str(e))

    # --- Optional fields ---
    if "mainline" in raw:
        v = str(raw["mainline"]).strip().lower()
        if v in {"single", "dual"}:
            config.mainline = v
        else:
            warnings.append(f"mainline must be 'single' or 'dual'; got {v!r}, using 'single'")

    if "curve_radius" in raw:
        try:
            config.curve_radius_in = _parse_length_in(str(raw["curve_radius"]))
        except ValueError:
            warnings.append(f"invalid curve_radius {raw['curve_radius']!r}")

    if "switch_size" in raw:
        config.switch_size = str(raw["switch_size"]).strip()

    if "levels" in raw:
        try:
            config.levels = int(raw["levels"])
        except (TypeError, ValueError):
            warnings.append(f"invalid levels {raw['levels']!r}, using 1")

    if "level_separation" in raw:
        try:
            config.level_separation_in = _parse_length_in(str(raw["level_separation"]))
        except ValueError:
            warnings.append(f"invalid level_separation {raw['level_separation']!r}")

    if "level_break" in raw:
        config.level_break_row = str(raw["level_break"]).strip().upper()

    if "grid_size" in raw:
        try:
            config.grid_size_ft = _parse_length_in(str(raw["grid_size"]))
        except ValueError:
            warnings.append(f"invalid grid_size {raw['grid_size']!r}")

    # --- Obstructions ---
    for obs in raw.get("obstructions", []):
        config.obstructions.append(str(obs).strip())

    # --- Grid placements ---
    for entry in raw.get("grid", []):
        try:
            config.placements.append(_parse_grid_entry(str(entry).strip()))
        except ValueError as e:
            warnings.append(f"skipping grid entry {entry!r}: {e}")

    # --- Fill scale-dependent defaults ---
    if config.scale in SCALE_MIN_RADIUS_IN:
        if config.curve_radius_in <= 0.0:
            config.curve_radius_in = SCALE_MIN_RADIUS_IN[config.scale]
        if config.grid_size_ft <= 0.0:
            config.grid_size_ft = _default_grid_cell_ft(config.curve_radius_in)

    missing = _required_missing(config)
    ready = len(missing) == 0
    summary = _build_summary(config) if ready else ""
    return ConfigResult(
        config=config, missing=missing, warnings=warnings,
        summary=summary, ready=ready,
    )


def _required_missing(config: LayoutConfig) -> list[tuple[str, str]]:
    missing = []
    if not config.name:
        missing.append(("name", "Layout name?"))
    if not config.scale:
        missing.append(("scale", "Scale? (HO, N, O, S, Z, G, ...)"))
    if config.room_width_ft <= 0 or config.room_depth_ft <= 0:
        missing.append(("room", "Room size in feet? (e.g. 12x16)"))
    return missing


def _build_summary(config: LayoutConfig) -> str:
    lines = [
        f"Layout:  {config.name}",
        f"Scale:   {config.scale}  |  Room: {config.room_width_ft:.0f}×{config.room_depth_ft:.0f} ft",
        f"Main:    {config.mainline}  |  Levels: {config.levels}"
        + (f"  (separation: {config.level_separation_in:.0f} in)" if config.levels > 1 else ""),
        f"Radius:  {config.curve_radius_in:.1f} in  |  Switch: {config.switch_size}"
        f"  |  Grid cell: {config.grid_size_ft:.0f} ft",
    ]
    if config.obstructions:
        lines.append(f"Obstructions: {', '.join(config.obstructions)}")
    if config.placements:
        lines.append(f"Elements ({len(config.placements)}):")
        for pl in config.placements:
            param_str = f"  [{pl.params}]" if pl.params else ""
            lines.append(f"  {pl.cell_range} → {pl.element_type}{param_str}")
    return "\n".join(lines)
