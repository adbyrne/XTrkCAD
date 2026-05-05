"""Tests for the template library loader."""

from pathlib import Path

import pytest

from xtrkcad_mcp.templates import (
    TEMPLATES_DIR,
    VALID_OD_ROLES,
    TemplateInfo,
    load_library,
)
from xtrkcad_mcp.parser import parse_file


# ---------------------------------------------------------------------------
# Library loads and is complete
# ---------------------------------------------------------------------------

def test_library_loads():
    lib = load_library()
    assert len(lib.templates) > 0

def test_library_has_twelve_templates():
    lib = load_library()
    assert len(lib.templates) == 12

def test_all_template_ids_unique():
    lib = load_library()
    ids = [t.id for t in lib.templates]
    assert len(ids) == len(set(ids))

def test_all_template_files_exist():
    lib = load_library()
    missing = [t.id for t in lib.templates if not t.xtc_path.exists()]
    assert missing == [], f"Missing .xtc files: {missing}"


# ---------------------------------------------------------------------------
# Categories
# ---------------------------------------------------------------------------

def test_categories_present():
    lib = load_library()
    cats = set(lib.categories())
    assert cats == {"yard", "staging", "station", "mainline", "helix", "siding"}

def test_filter_by_category_yard():
    lib = load_library()
    yards = lib.by_category("yard")
    assert len(yards) == 3
    assert all(t.category == "yard" for t in yards)

def test_filter_by_category_staging():
    lib = load_library()
    assert len(lib.by_category("staging")) == 2

def test_filter_by_unknown_category():
    lib = load_library()
    assert lib.by_category("doesnotexist") == []


# ---------------------------------------------------------------------------
# Lookup by id
# ---------------------------------------------------------------------------

def test_lookup_by_id_found():
    lib = load_library()
    t = lib.by_id("yard/single-ended")
    assert t is not None
    assert t.name == "Single-Ended Yard"

def test_lookup_by_id_not_found():
    lib = load_library()
    assert lib.by_id("yard/nonexistent") is None

def test_lookup_helix():
    lib = load_library()
    t = lib.by_id("helix/standard")
    assert t is not None
    assert t.category == "helix"


# ---------------------------------------------------------------------------
# OD roles — coverage and validity
# ---------------------------------------------------------------------------

def test_all_od_roles_valid():
    lib = load_library()
    bad = [t.id for t in lib.templates if t.od_role not in VALID_OD_ROLES]
    assert bad == []

def test_staging_od_role():
    lib = load_library()
    for t in lib.by_category("staging"):
        assert t.od_role == "staging"

def test_yard_od_role():
    lib = load_library()
    for t in lib.by_category("yard"):
        assert t.od_role == "storage"

def test_mainline_od_role():
    lib = load_library()
    for t in lib.by_category("mainline"):
        assert t.od_role == "connecting"

def test_helix_od_role():
    lib = load_library()
    assert lib.by_id("helix/standard").od_role == "connecting"

def test_passing_siding_od_role():
    lib = load_library()
    assert lib.by_id("station/passing-siding").od_role == "passing"

def test_team_track_od_role():
    lib = load_library()
    assert lib.by_id("siding/team-track").od_role == "passing"

def test_by_od_role_storage():
    lib = load_library()
    storage = lib.by_od_role("storage")
    assert len(storage) >= 3  # 3 yards + stub station + industry siding

def test_all_four_od_roles_represented():
    lib = load_library()
    roles_present = {t.od_role for t in lib.templates} - {"none"}
    assert roles_present == {"storage", "staging", "passing", "connecting"}


# ---------------------------------------------------------------------------
# Mainline connection topology
# ---------------------------------------------------------------------------

def test_double_ended_templates_have_dual_connection():
    lib = load_library()
    # Sidings (team-track, industry) are layout-specific — excluded here
    double_ended_ids = {
        "yard/double-ended", "staging/double-ended",
        "station/passing-siding",
        "mainline/curve", "mainline/s-curve", "helix/standard",
    }
    for tid in double_ended_ids:
        t = lib.by_id(tid)
        assert t is not None
        assert t.mainline_connection == "dual", f"{tid} should be dual"

def test_single_ended_templates_have_single_connection():
    lib = load_library()
    single_ended_ids = {
        "yard/single-ended", "yard/runaround",
        "staging/single-ended", "station/stub-end", "siding/industry",
    }
    for tid in single_ended_ids:
        t = lib.by_id(tid)
        assert t is not None
        assert t.mainline_connection == "single", f"{tid} should be single"


# ---------------------------------------------------------------------------
# Connection points
# ---------------------------------------------------------------------------

def test_single_ended_has_one_connection_point():
    lib = load_library()
    t = lib.by_id("yard/single-ended")
    assert len(t.connection_points) == 1
    assert "throat" in t.connection_points

def test_double_ended_has_two_connection_points():
    lib = load_library()
    t = lib.by_id("yard/double-ended")
    assert len(t.connection_points) == 2


# ---------------------------------------------------------------------------
# Parameters
# ---------------------------------------------------------------------------

def test_yard_single_ended_has_required_params():
    lib = load_library()
    t = lib.by_id("yard/single-ended")
    param_names = {p.name for p in t.parameters}
    assert "track_count" in param_names
    assert "min_track_length" in param_names

def test_track_count_is_required_int():
    lib = load_library()
    t = lib.by_id("yard/single-ended")
    tc = next(p for p in t.parameters if p.name == "track_count")
    assert tc.required is True
    assert tc.type == "int"
    assert tc.min == 2
    assert tc.max == 12

def test_helix_grade_has_default():
    lib = load_library()
    t = lib.by_id("helix/standard")
    grade = next(p for p in t.parameters if p.name == "grade")
    assert grade.default == pytest.approx(2.0)
    assert grade.unit == "pct"

def test_min_track_length_default_60ft():
    lib = load_library()
    t = lib.by_id("yard/single-ended")
    ml = next(p for p in t.parameters if p.name == "min_track_length")
    assert ml.default == 60


# ---------------------------------------------------------------------------
# Scale validity
# ---------------------------------------------------------------------------

def test_no_scale_restriction_means_all_scales():
    lib = load_library()
    t = lib.by_id("yard/single-ended")
    assert t.scales == []
    assert t.is_valid_for_scale("HO") is True
    assert t.is_valid_for_scale("N") is True
    assert t.is_valid_for_scale("O") is True


# ---------------------------------------------------------------------------
# Stub .xtc files are parseable
# ---------------------------------------------------------------------------

def test_stub_xtc_parseable():
    """Every stub .xtc must parse without error and have at least one track."""
    lib = load_library()
    for t in lib.templates:
        layout = parse_file(t.xtc_path)
        assert len(layout.tracks) >= 1, f"{t.id}: stub .xtc has no tracks"

def test_stub_xtc_endpoints_open():
    """Stub tracks should have open (unconnected) endpoints."""
    lib = load_library()
    for t in lib.templates:
        layout = parse_file(t.xtc_path)
        for track in layout.tracks:
            for ep in track.endpoints:
                assert ep.connected_to is None, \
                    f"{t.id}: stub endpoint should be open (E), not connected (T)"
