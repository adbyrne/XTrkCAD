"""Op Density views from the convention notes (spec v1.1, 9.1).

View 1 (Fugate, by layer category) adds facility car_spots to storage when
the facility's track isn't already counted by length. View 2 lists
industries and yards from the notes; view 3 the railroad's own service
facilities. conventions_clean.xtc: main layer 0, passing 1, service 2,
staging 3 (by its Layer Groups); service track 3 runs (0,30)-(240,30).
"""

from pathlib import Path

from xtrkcad_mcp.server import get_operation_density, write_operation_density_report

FIXTURES_DIR = Path(__file__).parent / "fixtures"
CLEAN_FIXTURE = FIXTURES_DIR / "conventions_clean.xtc"
PLAIN_FIXTURE = FIXTURES_DIR / "test_layout.xtc"


def _variant(tmp_path, old, new, name="variant.xtc"):
    text = CLEAN_FIXTURE.read_text(encoding="utf-8")
    assert text.count(old) == 1
    p = tmp_path / name
    p.write_text(text.replace(old, new), encoding="utf-8")
    return p


def _with_coal_spots(tmp_path, spots=3):
    return _variant(tmp_path, '""location"": ""WP""}"',
                    f'""location"": ""WP"", ""car_spots"": {spots}}}"')


def _fugate_max(result, extra_storage=0):
    cats = result["by_category"]
    cpf = result["cars_per_real_ft"]

    def cars(cat):
        return cats.get(cat, {}).get("length_real_ft", 0.0) * cpf
    return int(0.8 * (cars("storage") + extra_storage + cars("staging") + cars("passing") / 2))


# -- view 3: service facilities ------------------------------------------------

def test_facility_without_car_spots_is_not_counted():
    service = get_operation_density(str(CLEAN_FIXTURE))["service"]
    assert service["facilities"] == [{
        "id": "WP_COAL", "type": "coal", "location": "WP", "track_id": 3,
        "car_spots": None, "capacity_cars": 40, "counted": False}]
    assert service["car_spots"] == 0
    assert service["not_counted"] == ["WP_COAL"]


def test_facility_car_spots_count_as_storage(tmp_path):
    result = get_operation_density(str(_with_coal_spots(tmp_path)))
    service = result["service"]
    assert service["car_spots"] == 3 and service["not_counted"] == []
    assert service["facilities"][0]["counted"] is True
    assert result["operations"]["service_car_spots"] == 3
    assert result["operations"]["max_cars"] == _fugate_max(result, extra_storage=3)


def test_facility_on_storage_track_is_not_counted_twice(tmp_path):
    """A facility on a Storage-layer track is already in storage by length."""
    p = _with_coal_spots(tmp_path)
    result = get_operation_density(str(p), layer_categories={"2": "storage"})
    assert result["service"]["car_spots"] == 3
    assert result["operations"]["service_car_spots"] == 0
    assert result["operations"]["max_cars"] == _fugate_max(result)


# -- view 2: industries and yards ------------------------------------------------

def _industry(result, ident):
    return next(i for i in result["industries_and_yards"]["industries"] if i["id"] == ident)


def test_industry_spots_and_unserved_track():
    result = get_operation_density(str(CLEAN_FIXTURE))
    qm1 = _industry(result, "QM1")
    # QM1: 2 spots on 240 in of service track (40 cars).
    assert qm1 == {"id": "QM1", "kind": "industry", "station": "XP", "car_spots": 2,
                   "capacity_cars": 40, "unserved_cars": 38, "on_main": False}
    assert _industry(result, "XP_TEAM")["kind"] == "team_track"


def test_industry_totals():
    view = get_operation_density(str(CLEAN_FIXTURE))["industries_and_yards"]
    off_main = [i for i in view["industries"] if not i["on_main"]]
    assert view["totals"]["industry_spots"] == sum(i["car_spots"] for i in off_main)
    assert view["totals"]["industry_capacity_cars"] == sum(
        i["capacity_cars"] or 0 for i in off_main)
    assert view["totals"]["on_main_spots"] == 0


def test_industry_on_main_is_its_own_line(tmp_path):
    # QM1 moved onto main track 10, (108,0)-(190,0).
    p = _variant(tmp_path, "NOTE 108 2 0 0 160.000000 30.000000",
                 "NOTE 108 0 0 0 160.000000 0.000000")
    view = get_operation_density(str(p))["industries_and_yards"]
    assert _industry({"industries_and_yards": view}, "QM1")["on_main"] is True
    assert view["totals"]["on_main_spots"] == 2
    assert "QM1" not in [i["id"] for i in view["industries"] if not i["on_main"]]


def test_yards_and_storage_from_notes():
    view = get_operation_density(str(CLEAN_FIXTURE))["industries_and_yards"]
    yards = {y["id"]: y for y in view["yards"]}
    assert yards["WP"]["tracks"] == [
        {"label": "Track 2", "function": "classification", "capacity_cars": 40}]
    assert yards["WP"]["capacity_cars"] == 40
    assert view["storage"] == [{"id": "CAR_BARN", "car_spots": None, "capacity_cars": 40}]
    assert view["totals"]["yard_capacity_cars"] == sum(y["capacity_cars"] for y in view["yards"])


# -- layouts without convention notes ------------------------------------------

def test_layout_without_notes_has_no_note_views():
    result = get_operation_density(str(PLAIN_FIXTURE))
    assert result["industries_and_yards"] is None
    assert result["service"] is None
    assert result["operations"]["service_car_spots"] == 0


# -- the report -------------------------------------------------------------------

def test_report_has_the_three_views(tmp_path):
    p = _with_coal_spots(tmp_path)
    for fmt, heads in (("md", ["## Industries and Yards", "## Railroad Service"]),
                       ("txt", ["INDUSTRIES AND YARDS", "RAILROAD SERVICE"]),
                       ("html", ["<h2>Industries and Yards</h2>",
                                 "<h2>Railroad Service</h2>"])):
        out = tmp_path / f"od.{fmt}"
        write_operation_density_report(str(p), str(out), format=fmt)
        text = out.read_text(encoding="utf-8")
        for head in heads:
            assert head in text, (fmt, head)
        assert "WP_COAL" in text and "QM1" in text


def test_report_lists_facilities_not_counted(tmp_path):
    out = tmp_path / "od.md"
    write_operation_density_report(str(CLEAN_FIXTURE), str(out), format="md")
    assert "Not counted (no car_spots): WP_COAL" in out.read_text(encoding="utf-8")


def test_report_without_notes_has_no_note_views(tmp_path):
    out = tmp_path / "od.md"
    write_operation_density_report(str(PLAIN_FIXTURE), str(out), format="md")
    text = out.read_text(encoding="utf-8")
    assert "## Industries and Yards" not in text and "## Railroad Service" not in text
