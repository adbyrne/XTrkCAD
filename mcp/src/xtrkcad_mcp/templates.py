"""Template library — load, enumerate, and describe layout element templates."""

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import yaml

TEMPLATES_DIR = Path(__file__).parent / "templates"

VALID_OD_ROLES = frozenset({"storage", "staging", "passing", "connecting", "none"})
VALID_MAINLINE_CONNECTIONS = frozenset({"single", "dual", "none"})


@dataclass
class TemplateParameter:
    name: str
    type: str           # "int", "length", "float", "str"
    required: bool
    description: str = ""
    default: Any = None
    unit: str = ""
    min: float | None = None
    max: float | None = None


@dataclass
class TemplateInfo:
    id: str                         # "yard/single-ended"
    name: str
    category: str
    description: str
    file: str                       # relative path from TEMPLATES_DIR
    parameters: list[TemplateParameter] = field(default_factory=list)
    connection_points: list[str] = field(default_factory=list)
    mainline_connection: str = "single"   # "single" | "dual" | "none"
    od_role: str = "none"                 # Fugate formula term
    scales: list[str] = field(default_factory=list)   # empty = all scales
    stub: bool = True

    @property
    def xtc_path(self) -> Path:
        return TEMPLATES_DIR / self.file

    def is_valid_for_scale(self, scale: str) -> bool:
        return not self.scales or scale in self.scales


@dataclass
class TemplateLibrary:
    templates: list[TemplateInfo] = field(default_factory=list)

    def by_category(self, category: str) -> list[TemplateInfo]:
        return [t for t in self.templates if t.category == category]

    def by_id(self, template_id: str) -> TemplateInfo | None:
        return next((t for t in self.templates if t.id == template_id), None)

    def categories(self) -> list[str]:
        seen: set[str] = set()
        result = []
        for t in self.templates:
            if t.category not in seen:
                seen.add(t.category)
                result.append(t.category)
        return result

    def by_od_role(self, role: str) -> list[TemplateInfo]:
        return [t for t in self.templates if t.od_role == role]


def _parse_parameter(raw: dict) -> TemplateParameter:
    return TemplateParameter(
        name=str(raw.get("name", "")),
        type=str(raw.get("type", "str")),
        required=bool(raw.get("required", False)),
        description=str(raw.get("description", "")),
        default=raw.get("default"),
        unit=str(raw.get("unit", "")),
        min=float(raw["min"]) if "min" in raw else None,
        max=float(raw["max"]) if "max" in raw else None,
    )


def _parse_template(raw: dict) -> TemplateInfo:
    od_role = str(raw.get("od_role", "none"))
    if od_role not in VALID_OD_ROLES:
        od_role = "none"
    mainline_connection = str(raw.get("mainline_connection", "single"))
    if mainline_connection not in VALID_MAINLINE_CONNECTIONS:
        mainline_connection = "single"
    return TemplateInfo(
        id=str(raw.get("id", "")),
        name=str(raw.get("name", "")),
        category=str(raw.get("category", "")),
        description=str(raw.get("description", "")),
        file=str(raw.get("file", "")),
        parameters=[_parse_parameter(p) for p in raw.get("parameters", [])],
        connection_points=[str(c) for c in raw.get("connection_points", [])],
        mainline_connection=mainline_connection,
        od_role=od_role,
        scales=[str(s) for s in raw.get("scales", [])],
        stub=bool(raw.get("stub", True)),
    )


def load_library() -> TemplateLibrary:
    """Load the template library from templates/index.yaml."""
    index_path = TEMPLATES_DIR / "index.yaml"
    if not index_path.exists():
        return TemplateLibrary()
    with open(index_path) as f:
        raw = yaml.safe_load(f) or {}
    return TemplateLibrary(
        templates=[_parse_template(t) for t in raw.get("templates", [])]
    )
