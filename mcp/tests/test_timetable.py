"""make_timetable: a simple timetable from the v2 export and an operations file.

The numbers below are worked by hand: A at MP 0, B at MP 10, C at MP 20, a
yard-limit zone MP 8-12 around B; 30 mph, 15 mph in yard limits; 10 minutes
a stop plus 5 a car spot. A to B: 8 MP at 30 (16 min) + 2 at 15 (8) = 24.
"""

import pytest

from xtrkcad_mcp.timetable import TimetableError, build_timetable


def _export(zones=((8.0, 12.0),)):
    st = [("A", "Ash", 0.0), ("B", "Box", 10.0), ("C", "Cove", 20.0)]
    return {
        "scale": "HO",
        "railroads": [{"id": "RR", "subdivisions": [{"id": None, "mp_unit": "in",
                                                      "mp_scale": 12}]}],
        "stations": [{"id": i, "name": n, "railroad": "RR", "subdivision": None, "mp": mp}
                     for i, n, mp in st],
        "yard_limits": [{"id": f"Z{k}", "railroad": "RR", "subdivision": None,
                         "mp_start": a, "mp_end": b} for k, (a, b) in enumerate(zones)],
        "industries": [{"id": "B_MILL", "station": "B", "car_spots": 2}],
        "team_tracks": [{"id": "C_TEAM", "station": "C", "car_spots": 3}],
    }


def _ops(**train):
    t = {"id": "1", "name": "Local", "departs": "07:00", "route": ["A", "C", "A"],
         "work": ["B"]}
    t.update(train)
    return {"schema_version": 1, "clock": {"ratio": 3},
            "speed": {"max_mph": 30, "yard_limit_mph": 15},
            "dwell": {"station_min": 10, "per_car_spot": 5},
            "trains": [t]}


def _times(result):
    return [(s["station"], s["arrive"], s["depart"]) for s in result["trains"][0]["stops"]]


def test_out_and_back_by_hand():
    r = build_timetable(_export(), _ops())
    assert _times(r) == [
        ("A", None, "07:00"),
        ("B", "07:24", "07:44"),      # works B: 10 + 2 spots x 5
        ("C", "08:08", "08:18"),      # turns at C: 10, no work there
        ("B", None, "08:42"),         # passes B on the way back
        ("A", "09:06", None),
    ]
    t = r["trains"][0]
    assert (t["fast_minutes"], t["real_minutes"]) == (126, 42.0)


def test_model_speed_the_timetable_implies():
    """30 MP an hour on a 3:1 clock is 90 MP an hour real; at 12 in a MP
    that's 0.3 in/s, 26.1 in/s in HO (87.1): 1.5 mph. 15 mph: 0.7."""
    t = build_timetable(_export(), _ops())["trains"][0]
    assert t["model_mph"] == {"max": 1.5, "yard_limit": 0.7}


def test_work_at_the_turnaround_counts_its_spots():
    r = build_timetable(_export(), _ops(work=["B", "C"]))
    assert _times(r)[2] == ("C", "08:08", "08:33")        # 10 + 3 x 5


def test_without_yard_limits_all_at_track_speed():
    r = build_timetable(_export(zones=()), _ops(work=[]))
    assert _times(r) == [("A", None, "07:00"), ("B", None, "07:20"),
                         ("C", "07:40", "07:50"), ("B", None, "08:10"), ("A", "08:30", None)]


def test_one_way_train():
    r = build_timetable(_export(zones=()), _ops(route=["A", "C"], work=[]))
    assert _times(r) == [("A", None, "07:00"), ("B", None, "07:20"), ("C", "07:40", None)]


def test_timetable_table_lists_stations_by_mp():
    r = build_timetable(_export(), _ops())
    lines = r["table"].splitlines()
    assert lines[0] == "| Station | MP | 1 out | 1 back |"
    assert lines[2] == "| A Ash | 0.0 | 07:00 | 09:06 |"
    assert lines[3] == "| B Box | 10.0 | 07:24 – 07:44 | 08:42 |"
    assert lines[4] == "| C Cove | 20.0 | 08:08 | 08:18 |"


@pytest.mark.parametrize("change, message", [
    ({"schema_version": 2}, "schema_version must be 1"),
    ({"speed": {"max_mph": 0, "yard_limit_mph": 15}}, "speed.max_mph must be more than 0"),
])
def test_bad_operations_file(change, message):
    ops = _ops()
    ops.update(change)
    with pytest.raises(TimetableError, match=message):
        build_timetable(_export(), ops)


@pytest.mark.parametrize("train, message", [
    ({"route": ["A", "X"]}, "train 1: route: unknown station 'X'"),
    ({"route": ["A"]}, "train 1: route needs at least 2 stations"),
    ({"work": ["Q"]}, "train 1: works 'Q', which isn't on its route"),
    ({"departs": "7 am"}, "train 1: departs must be HH:MM"),
])
def test_bad_train(train, message):
    with pytest.raises(TimetableError, match=message):
        build_timetable(_export(), _ops(**train))
