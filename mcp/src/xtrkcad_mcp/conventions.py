"""Operating Data Conventions v1: reading convention notes and checking them.

The conventions say how a layout file records its railroad as built, in a
form an operating plan can use (spec: mcp/docs/operating-conventions.md once
stable). A fact tied to a place on the layout is a JSON Note there, with a
``kind`` and an ``id``; check_conventions() reports every rule the layout
breaks as an Issue with a stable rule id.

This module covers the note and id rules (phase 1) and the Layer Group rules
(phase 2). Placement and milepost rules follow in later phases.
"""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass, field

from .models import Layout, NoteObject, category_from_layer_name

CONVENTIONS_VERSION = 1

KINDS = (
    "railroad", "subdivision", "reference", "station", "yard_limit",
    "industry", "staging", "yard_track", "house_track", "storage",
    "facility", "switchback",
)
LOCATION_KINDS = ("station", "industry", "staging")

# Kinds whose ids are unique within the kind. Location kinds share one
# namespace (checked separately); yard_limit and yard_track group notes by id.
_UNIQUE_ID_KINDS = (
    "railroad", "subdivision", "reference", "house_track", "storage",
    "facility", "switchback",
)

YARD_FUNCTIONS = (
    "arrival", "departure", "arrival_departure", "classification",
    "runaround", "lead", "interchange", "caboose", "rip", "storage",
    "engine", "team",
)
INDUSTRY_FUNCTIONS = ("loading", "empties", "loads", "service")
STAGING_FUNCTIONS = ("staging",) + YARD_FUNCTIONS
_FUNCTIONS_BY_YARD_KIND = {
    "station": YARD_FUNCTIONS,
    "industry": INDUSTRY_FUNCTIONS,
    "staging": STAGING_FUNCTIONS,
}

FACILITY_TYPES = (
    "ice", "engine_service", "turntable", "fuel", "water", "sand", "coal",
    "ash", "scale", "car_repair",
)
MP_UNITS = ("in", "ft", "cm", "m")

# AAR mechanical designations (spec 7). Extra codes come from the equipment
# source once one is given.
AAR_CAR_TYPES = frozenset({
    # box cars
    "XM", "XA", "XAP", "XAR", "XF", "XI", "XL", "XLI", "XMI", "XP", "XR",
    # refrigerator cars
    "RA", "RB", "RBL", "RP", "RPL", "RS",
    # stock cars
    "SM", "SA", "SC",
    # flat cars
    "FA", "FB", "FC", "FD", "FL", "FM", "FMS", "FW",
    # gondolas
    "GA", "GB", "GD", "GH", "GS", "GT",
    # hoppers and covered hoppers
    "HD", "HFA", "HK", "HM", "HMA", "HT", "HTS", "LO", "LP",
    # tank cars
    "TA", "TG", "TM", "TP",
    # caboose, maintenance of way, passenger
    "NE", "MW", "PA", "PB", "PO", "PS", "PD", "BH",
})

# Reserved Layer Group names that give their layers a category (spec 3.1),
# matched case-insensitively. Values are the category keys the reports use.
CATEGORY_GROUPS = {
    "mainline": "mainline", "passing": "passing", "staging": "staging",
    "storage": "storage", "service": "service", "connecting": "connecting",
    "scenery": "scenery", "ignore": "ignore",
}
# Categories whose layers belong to no railroad.
_UNOWNED_CATEGORIES = ("scenery", "ignore")

_SEVERITY_ORDER = ("error", "warning", "info")

# v0 text-prefix notes (spec 9, compatibility).
_TEXT_PREFIXES = (
    "STATION:", "STORAGE:", "INDUSTRY:", "REFERENCE:", "YARD_TRACK:",
    "HOUSE_TRACK:",
)

_NOTE_OP_TEXT = 0
_NOTE_OP_JSON = 3

# Field types, by field name (the same name means the same thing in every
# kind). Unknown fields are kept and never checked (spec 4.0).
_STR_FIELDS = (
    "name", "railroad", "subdivision", "ref_tag", "station", "label",
    "function", "location", "serves", "type", "group", "reporting_marks",
    "mp_unit",
)
_BOOL_FIELDS = ("terminus", "mp_at_note")
_STR_LIST_FIELDS = ("car_types", "loads_in", "loads_out", "railroads")


@dataclass
class Issue:
    """One broken rule. ``rule`` is the stable id that tests and docs match."""
    severity: str          # "error" | "warning" | "info"
    rule: str
    message: str
    hint: str = ""
    note_id: int | None = None
    layer: int | None = None
    group: str | None = None
    track_id: int | None = None
    x: float | None = None
    y: float | None = None

    def as_dict(self) -> dict:
        return asdict(self)


@dataclass
class ConventionNote:
    """A JSON Note with a known ``kind`` and an ``id``.

    railroad and subdivision are resolved by resolve_ownership(): the note's
    own field, else the railroad/subdivision whose Layer Group holds the
    note's layer, else the layout's only one, else None.
    """
    note_id: int
    layer: int
    x: float
    y: float
    kind: str
    id: str
    fields: dict = field(default_factory=dict)
    railroad: str | None = None
    subdivision: str | None = None

    def str_field(self, key: str) -> str | None:
        """fields[key] when it is a non-empty string, else None."""
        value = self.fields.get(key)
        return value if isinstance(value, str) and value else None

    def str_list_field(self, key: str) -> list[str]:
        value = self.fields.get(key)
        if not isinstance(value, list):
            return []
        return [v for v in value if isinstance(v, str)]


def _note_issue(severity: str, rule: str, message: str, note: NoteObject | ConventionNote,
                hint: str = "") -> Issue:
    note_id = note.id if isinstance(note, NoteObject) else note.note_id
    return Issue(severity, rule, message, hint=hint, note_id=note_id,
                 layer=note.layer, x=note.x, y=note.y)


def _is_number(value) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _field_type_problems(fields: dict) -> list[str]:
    """The known fields in ``fields`` whose value has the wrong type."""
    bad = []
    for key in _STR_FIELDS:
        if key in fields and not isinstance(fields[key], str):
            bad.append(f"{key} must be a string")
    for key in _BOOL_FIELDS:
        if key in fields and not isinstance(fields[key], bool):
            bad.append(f"{key} must be true or false")
    for key in _STR_LIST_FIELDS:
        if key in fields:
            value = fields[key]
            if not isinstance(value, list) or not all(isinstance(v, str) for v in value):
                bad.append(f"{key} must be a list of strings")
    if "mp" in fields and not _is_number(fields["mp"]):
        bad.append("mp must be a number")
    if "mp_scale" in fields and not (_is_number(fields["mp_scale"]) and fields["mp_scale"] > 0):
        bad.append("mp_scale must be a number above 0")
    if "car_spots" in fields:
        value = fields["car_spots"]
        if not (isinstance(value, int) and not isinstance(value, bool) and value > 0):
            bad.append("car_spots must be a whole number above 0")
    if "switches" in fields:
        value = fields["switches"]
        if not isinstance(value, list) or not all(
                isinstance(v, int) and not isinstance(v, bool) for v in value):
            bad.append("switches must be a list of track ids")
    return bad


def read_convention_notes(layout: Layout) -> tuple[list[ConventionNote], list[Issue]]:
    """Read the layout's convention notes.

    Returns the notes, with ownership resolved, and the issues found while
    reading them: invalid JSON, an unknown kind, a missing id, wrong field
    types, v0 text-prefix notes. Notes with an unknown kind or no id are
    reported and left out of the result.
    """
    notes: list[ConventionNote] = []
    issues: list[Issue] = []
    for raw in layout.notes:
        if raw.op == _NOTE_OP_TEXT:
            if raw.text.strip().upper().startswith(_TEXT_PREFIXES):
                issues.append(_note_issue(
                    "info", "NOTE_TEXT_PREFIX",
                    f"Text-prefix note {raw.text.strip()[:40]!r} (conventions v0)",
                    raw, hint="Upgrade it to a JSON Note with apply_conventions."))
            continue
        if raw.op != _NOTE_OP_JSON:
            continue
        try:
            obj = json.loads(raw.text)
        except (json.JSONDecodeError, TypeError):
            obj = None
        if not isinstance(obj, dict):
            if '"kind"' in raw.text:
                issues.append(_note_issue(
                    "error", "NOTE_INVALID_JSON",
                    "JSON Note with a \"kind\" is not a valid JSON object", raw,
                    hint="Fix the JSON in the note's Properties."))
            continue
        if "kind" not in obj:
            continue        # some other JSON Note, not ours
        kind = obj["kind"]
        if kind not in KINDS:
            issues.append(_note_issue(
                "error", "NOTE_UNKNOWN_KIND", f"Unknown kind {kind!r}", raw,
                hint="Kinds: " + ", ".join(KINDS)))
            continue
        ident = obj.get("id")
        if not isinstance(ident, str) or not ident.strip():
            issues.append(_note_issue(
                "error", "NOTE_MISSING_ID", f"{kind} note without an id", raw,
                hint="Every convention note needs a non-empty string id."))
            continue
        for problem in _field_type_problems(obj):
            issues.append(_note_issue(
                "error", "NOTE_FIELD_TYPE", f"{kind} {ident}: {problem}", raw))
        notes.append(ConventionNote(
            note_id=raw.id, layer=raw.layer, x=raw.x, y=raw.y, kind=kind,
            id=ident.strip(), fields=obj))
    resolve_ownership(layout, notes)
    return notes, issues


def _group_layers(layout: Layout) -> dict[str, set[int]]:
    """Layer Group name (lower case) -> its layers. Names match like the
    layers/groups filter on the report tools: case-insensitively."""
    groups: dict[str, set[int]] = {}
    for g in layout.layer_groups:
        groups.setdefault(g.name.lower(), set()).update(g.layers)
    return groups


def track_layers(layout: Layout) -> set[int]:
    """Layers holding at least one track object."""
    return {t.layer for t in layout.tracks}


def layer_categories(layout: Layout) -> dict[int, tuple[str, str]]:
    """Each layer's category and where it came from (spec 3.1).

    Returns layer index -> (category, source); source is "group" (the first
    category Layer Group holding the layer), "name" (the L<n>-Suffix layer
    name) or "default" (mainline).
    """
    by_group: dict[int, str] = {}
    for g in layout.layer_groups:
        cat = CATEGORY_GROUPS.get(g.name.lower())
        if cat is None:
            continue
        for idx in g.layers:
            by_group.setdefault(idx, cat)
    result: dict[int, tuple[str, str]] = {}
    for idx, info in layout.layers.items():
        if idx in by_group:
            result[idx] = (by_group[idx], "group")
            continue
        inferred = category_from_layer_name(info.name)
        result[idx] = (inferred, "name") if inferred else ("mainline", "default")
    return result


def resolve_ownership(layout: Layout, notes: list[ConventionNote]) -> None:
    """Set each note's railroad and subdivision (see ConventionNote)."""
    groups = _group_layers(layout)
    railroads = [n for n in notes if n.kind == "railroad"]
    subdivisions = [n for n in notes if n.kind == "subdivision"]
    only_railroad = railroads[0].id if len(railroads) == 1 else None

    def _owner_by_group(owners: list[ConventionNote], layer: int) -> str | None:
        hits = {o.id for o in owners
                if layer in groups.get((o.str_field("group") or "").lower(), set())}
        return hits.pop() if len(hits) == 1 else None

    for sub in subdivisions:
        sub.railroad = sub.str_field("railroad") or only_railroad
    sub_railroad = {s.id: s.railroad for s in subdivisions}

    for note in notes:
        if note.kind in ("railroad", "subdivision"):
            continue
        note.railroad = (note.str_field("railroad")
                         or _owner_by_group(railroads, note.layer)
                         or only_railroad)
        sub = note.str_field("subdivision") or _owner_by_group(subdivisions, note.layer)
        if sub is None:
            candidates = [s.id for s in subdivisions
                          if note.railroad is None or s.railroad in (None, note.railroad)]
            sub = candidates[0] if len(candidates) == 1 else None
        note.subdivision = sub
        if note.railroad is None and sub is not None:
            note.railroad = sub_railroad.get(sub)


def _check_ids(notes: list[ConventionNote], issues: list[Issue]) -> dict[str, str]:
    """Duplicate-id rules. Returns location id -> its kind."""
    locations: dict[str, list[ConventionNote]] = {}
    for note in notes:
        if note.kind in LOCATION_KINDS:
            locations.setdefault(note.id, []).append(note)

    location_kind: dict[str, str] = {}
    for loc_id, group in locations.items():
        first = group[0]
        location_kind[loc_id] = first.kind
        if all(n.kind == "station" for n in group):
            _check_joint_station(group, issues)
            continue
        for extra in group[1:]:
            issues.append(_note_issue(
                "error", "ID_DUPLICATE_LOCATION",
                f"Location id {loc_id!r} is used by a {first.kind} and a {extra.kind}"
                if extra.kind != first.kind else
                f"Two {extra.kind} notes with id {loc_id!r}",
                extra, hint="Stations, industries and staging yards share one id namespace."))

    for kind in _UNIQUE_ID_KINDS:
        seen: dict[str, ConventionNote] = {}
        for note in notes:
            if note.kind != kind:
                continue
            if note.id in seen:
                issues.append(_note_issue(
                    "error", "ID_DUPLICATE", f"Two {kind} notes with id {note.id!r}", note))
            else:
                seen[note.id] = note
    return location_kind


def _check_joint_station(group: list[ConventionNote], issues: list[Issue]) -> None:
    """Station notes sharing an id: one per subdivision (spec 4.3)."""
    if len(group) < 2:
        return
    seen: set[tuple[str | None, str | None]] = set()
    for note in group:
        key = (note.railroad, note.subdivision)
        if key in seen:
            issues.append(_note_issue(
                "error", "STATION_JOINT_SAME_SUBDIVISION",
                f"Two station notes for {note.id!r} in one subdivision", note,
                hint="A joint station has one station note per subdivision; "
                     "check the note's layer groups or its railroad/subdivision."))
        seen.add(key)
    names = {n.str_field("name") for n in group} - {None}
    if len(names) > 1:
        issues.append(_note_issue(
            "warning", "STATION_JOINT_NAME_MISMATCH",
            f"Station {group[0].id!r} has different names: {', '.join(sorted(names))}",
            group[0], hint="Give the name on one note, or the same on all."))


def _check_references(notes: list[ConventionNote], location_kind: dict[str, str],
                      issues: list[Issue]) -> None:
    railroad_ids = {n.id for n in notes if n.kind == "railroad"}
    subdivision_ids = {n.id for n in notes if n.kind == "subdivision"}
    reference_ids = {n.id for n in notes if n.kind == "reference"}

    def unknown_railroad(note: ConventionNote, rr: str) -> None:
        hint = ("Add a railroad note for it." if not railroad_ids
                else "Railroads: " + ", ".join(sorted(railroad_ids)))
        issues.append(_note_issue(
            "error", "REF_UNKNOWN_RAILROAD",
            f"{note.kind} {note.id}: unknown railroad {rr!r}", note, hint=hint))

    def unknown_location(note: ConventionNote, key: str, loc: str, kinds: str) -> None:
        issues.append(_note_issue(
            "error", "REF_UNKNOWN_LOCATION",
            f"{note.kind} {note.id}: {key} {loc!r} is no {kinds}", note))

    for note in notes:
        rr = note.str_field("railroad")
        if rr is not None and note.kind != "railroad" and rr not in railroad_ids:
            unknown_railroad(note, rr)
        sub = note.str_field("subdivision")
        if sub is not None and note.kind != "subdivision" and sub not in subdivision_ids:
            issues.append(_note_issue(
                "error", "REF_UNKNOWN_SUBDIVISION",
                f"{note.kind} {note.id}: unknown subdivision {sub!r}", note))
        for listed in note.str_list_field("railroads"):
            if listed not in railroad_ids:
                unknown_railroad(note, listed)

        if note.kind == "subdivision" and rr is None and len(railroad_ids) > 1:
            issues.append(_note_issue(
                "error", "SUBDIVISION_NO_RAILROAD",
                f"Subdivision {note.id!r} doesn't say its railroad", note,
                hint="With more than one railroad, a subdivision needs \"railroad\"."))
        elif note.kind == "station":
            tag = note.str_field("ref_tag")
            if tag is not None and tag not in reference_ids:
                issues.append(_note_issue(
                    "error", "REF_UNKNOWN_REFERENCE",
                    f"Station {note.id}: ref_tag {tag!r} names no reference note", note))
        elif note.kind == "industry":
            station = note.str_field("station")
            if station is not None:
                if station not in location_kind:
                    unknown_location(note, "station", station, "location")
                elif location_kind[station] != "station":
                    issues.append(_note_issue(
                        "error", "INDUSTRY_STATION_NOT_STATION",
                        f"Industry {note.id}: {station!r} is a "
                        f"{location_kind[station]}, not a station", note))
        elif note.kind == "yard_track":
            if note.id not in location_kind:
                unknown_location(note, "id", note.id, "station, industry or staging yard")
        elif note.kind == "house_track":
            if location_kind.get(note.id) not in ("station", "industry"):
                unknown_location(note, "id", note.id, "station or industry")
        elif note.kind == "facility":
            loc = note.str_field("location")
            if loc is not None and loc not in location_kind:
                unknown_location(note, "location", loc, "location")
        elif note.kind == "switchback":
            serves = note.str_field("serves")
            if serves is not None and serves not in location_kind:
                unknown_location(note, "serves", serves, "location")


def _check_yard_tracks(notes: list[ConventionNote], location_kind: dict[str, str],
                       issues: list[Issue]) -> None:
    railroad_ids = {n.id for n in notes if n.kind == "railroad"}
    labels: set[tuple[str, str]] = set()
    all_functions = set(YARD_FUNCTIONS) | set(INDUSTRY_FUNCTIONS) | set(STAGING_FUNCTIONS)
    for note in notes:
        if note.kind != "yard_track":
            continue
        label = note.str_field("label")
        name = f"{note.id} {label}" if label else note.id
        if label is None:
            issues.append(_note_issue(
                "error", "YARD_TRACK_MISSING_LABEL",
                f"Yard track of {note.id!r} without a label", note))
        elif (note.id, label) in labels:
            issues.append(_note_issue(
                "error", "YARD_TRACK_DUPLICATE_LABEL",
                f"Two tracks labelled {label!r} in {note.id!r}", note))
        else:
            labels.add((note.id, label))

        function = note.str_field("function")
        if function is None:
            issues.append(_note_issue(
                "info", "YARD_TRACK_NO_FUNCTION",
                f"Yard track {name} has no function",
                note))
            continue
        yard_kind = location_kind.get(note.id)
        allowed = _FUNCTIONS_BY_YARD_KIND.get(yard_kind, tuple(all_functions))
        if function not in allowed:
            issues.append(_note_issue(
                "error", "YARD_TRACK_BAD_FUNCTION",
                f"Yard track {name}: function {function!r} "
                f"is not one for a {yard_kind or 'yard'}", note,
                hint="Functions: " + ", ".join(sorted(allowed))))
        if function == "interchange":
            known = set(note.str_list_field("railroads")) & railroad_ids
            if len(known) < 2:
                issues.append(_note_issue(
                    "error", "INTERCHANGE_RAILROADS",
                    f"Interchange track {name} needs the 2 or more "
                    f"railroads that exchange cars there", note,
                    hint='Add "railroads": ["A", "B"] naming railroad notes.'))


def _check_kind_fields(notes: list[ConventionNote], issues: list[Issue]) -> None:
    for note in notes:
        if note.kind == "industry":
            if "car_spots" not in note.fields:
                issues.append(_note_issue(
                    "info", "INDUSTRY_CAR_SPOTS_DEFAULT",
                    f"Industry {note.id}: car_spots not given, 2 used", note))
            unknown = [c for c in note.str_list_field("car_types") if c not in AAR_CAR_TYPES]
            if unknown:
                issues.append(_note_issue(
                    "warning", "INDUSTRY_UNKNOWN_CAR_TYPE",
                    f"Industry {note.id}: unknown car type(s) {', '.join(unknown)}", note,
                    hint="Use AAR mechanical designations (XM, HM, TM, ...)."))
        elif note.kind == "facility":
            ftype = note.fields.get("type")
            if ftype not in FACILITY_TYPES:
                issues.append(_note_issue(
                    "error", "FACILITY_BAD_TYPE",
                    f"Facility {note.id}: type {ftype!r} is not a facility type", note,
                    hint="Types: " + ", ".join(FACILITY_TYPES)))
        elif note.kind == "reference":
            unit = note.fields.get("mp_unit")
            if isinstance(unit, str) and unit not in MP_UNITS:
                issues.append(_note_issue(
                    "error", "REFERENCE_BAD_UNIT",
                    f"Reference {note.id}: mp_unit {unit!r} is not one of "
                    f"{', '.join(MP_UNITS)}", note))


def _layer_name(layout: Layout, layer: int) -> str:
    return layout.layers[layer].name if layer in layout.layers else f"Layer {layer}"


def _layer_issue(severity: str, rule: str, message: str, layer: int,
                 hint: str = "", group: str | None = None) -> Issue:
    return Issue(severity, rule, message, hint=hint, layer=layer, group=group)


def _check_groups(layout: Layout, notes: list[ConventionNote], issues: list[Issue]) -> None:
    """Category, railroad and subdivision Layer Group rules (spec 3.1, 3.2)."""
    with_track = track_layers(layout)

    category_groups: dict[int, list[str]] = {}
    for g in layout.layer_groups:
        if g.name.lower() in CATEGORY_GROUPS:
            for idx in g.layers:
                category_groups.setdefault(idx, []).append(g.name)
    for idx, names in sorted(category_groups.items()):
        if len(names) > 1:
            issues.append(_layer_issue(
                "error", "CATEGORY_LAYER_IN_TWO",
                f"Layer {_layer_name(layout, idx)} is in category groups "
                + " and ".join(names), idx, group=names[1],
                hint="A layer has one category; take it out of all but one group."))

    categories = layer_categories(layout)
    for idx in sorted(with_track):
        if idx in category_groups:
            continue
        cat, source = categories.get(idx, ("mainline", "default"))
        how = "from its name" if source == "name" else "by default"
        issues.append(_layer_issue(
            "warning", "CATEGORY_LAYER_NONE",
            f"Track layer {_layer_name(layout, idx)} is in no category group; {cat} {how}",
            idx,
            hint="Add it to one of: " + ", ".join(n.capitalize() for n in CATEGORY_GROUPS)))

    groups = _group_layers(layout)
    for kind, rule in (("railroad", "RAILROAD_LAYER_IN_TWO"),
                       ("subdivision", "SUBDIVISION_LAYER_IN_TWO")):
        owners: dict[int, list[str]] = {}
        seen_groups: set[str] = set()
        for note in notes:
            if note.kind != kind:
                continue
            gname = note.str_field("group")
            if gname is None:
                continue
            if gname.lower() not in groups:
                issues.append(Issue(
                    "error", "GROUP_UNKNOWN",
                    f"{kind} {note.id}: Layer Group {gname!r} doesn't exist",
                    hint="Groups: " + (", ".join(g.name for g in layout.layer_groups) or "none"),
                    note_id=note.note_id, layer=note.layer, x=note.x, y=note.y,
                    group=gname))
                continue
            if kind == "subdivision" and gname.lower() in seen_groups:
                continue        # subdivisions may share one group (spec 4.1)
            seen_groups.add(gname.lower())
            for idx in groups[gname.lower()]:
                owners.setdefault(idx, []).append(note.id)
        for idx, ids in sorted(owners.items()):
            if len(ids) > 1:
                issues.append(_layer_issue(
                    "error", rule,
                    f"Layer {_layer_name(layout, idx)} is in the groups of "
                    f"{kind}s {' and '.join(ids)}", idx))
        if kind == "railroad" and sum(1 for n in notes if n.kind == "railroad") > 1:
            for idx in sorted(with_track):
                if idx in owners or categories.get(idx, ("",))[0] in _UNOWNED_CATEGORIES:
                    continue
                issues.append(_layer_issue(
                    "warning", "RAILROAD_LAYER_NONE",
                    f"Track layer {_layer_name(layout, idx)} belongs to no railroad", idx,
                    hint="Add it to a railroad's Layer Group (the railroad note's \"group\")."))


def sort_issues(issues: list[Issue]) -> list[Issue]:
    return sorted(issues, key=lambda i: (
        _SEVERITY_ORDER.index(i.severity), i.rule,
        i.note_id if i.note_id is not None else -1,
        i.layer if i.layer is not None else -1))


def check_conventions(layout: Layout) -> list[Issue]:
    """Every convention rule the layout breaks, errors first."""
    notes, issues = read_convention_notes(layout)
    location_kind = _check_ids(notes, issues)
    _check_references(notes, location_kind, issues)
    _check_yard_tracks(notes, location_kind, issues)
    _check_kind_fields(notes, issues)
    _check_groups(layout, notes, issues)
    return sort_issues(issues)
