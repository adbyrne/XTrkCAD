# layout_data.json, schema version 2

`export_layout_data(path)` writes this file (`format="v2"`, the default). It is the general,
versioned export of a layout's operating data under the Operating Data Conventions, v1. Its
first consumer is an operations program (IOTtrains), but nothing in it is specific to one.

Everything comes from the layout file itself: the convention JSON Notes (each with a `kind`),
the Layer Groups and the track geometry. No YAML is read; `stations.yaml` is retired, and the
v2 export ignores the `stations_yaml` argument with a notice.

## Writing it

- The convention check (`check_conventions`) runs first. While it reports errors, nothing is
  written: the tool returns `written: false` and the errors. `force=True` writes the file
  anyway.
- The file always carries the check's full result in `issues`, warnings and info included.
- `format="v1"` writes the previous format, unchanged, until the consumer imports v2.

Tool result: `{written, output, counts: {error, warning, info}, errors, notices}`.

## Units and conventions

- Lengths are prototype feet (`_ft`), from layout inches by the layout's scale ratio.
- Mileposts are in MP units; `mp_scale` and `mp_unit` give layout length per MP unit.
- Car counts (`_cars`, `capacity_cars`, `spur_capacity_cars`) are whole cars by the scale's
  cars per model foot (HO: 2), as the capacity reports count them.
- Usable length is what a track holds: the connected non-turnout track a note marks, between
  its **clearance points**. Near a turnout a car fouls the turnout's other leg until the two
  tracks are 13 ft apart (prototype, between centres; scaled), so that part doesn't count. The
  track beyond a turnout's points is shared by both routes and isn't reduced. Switch throats
  are left out. A siding also counts the turnouts on the siding itself (spurs leaving it), since
  cars stand on their through route; the main between a siding's switches stops at the
  clearance point near each switch too. A track measured within a thousandth of a car of a
  whole number of cars counts as that many.
- Numbers are rounded to 3 decimals. A value that can't be worked out is `null`; the reason is
  in `issues`.
- Lists are sorted: stations, yard limits and segments by railroad, subdivision, then MP;
  industries and team tracks by `branch_mp`; the rest by id.

## Top level

| Field | Meaning |
|---|---|
| `schema_version` | `2` |
| `conventions_version` | Version of the Operating Data Conventions the layout was checked against (`1`) |
| `generated` | UTC time the file was written (ISO 8601) |
| `layout` | Layout title (`TITLE1`) |
| `scale` | Layout scale (`HO`, `N`, ...) |
| `layout_units` | Unit of the layout's own coordinates: `in` |
| `railroads` | Railroads, their subdivisions and MP anchors |
| `stations` | Stations, with their MPs, sidings and house tracks |
| `yard_limits` | Yard-limit zones |
| `industries` | Industries |
| `team_tracks` | Team tracks |
| `yards` | Yard tracks, grouped by the location they belong to |
| `storage` | Stand-alone car storage tracks |
| `switchbacks` | Switchbacks |
| `facilities` | Facilities (coal, water, ...) |
| `segments` | Main-track lengths between consecutive stations |
| `issues` | The convention check's result |

## railroads[]

`id`, `name`, `reporting_marks`, and `subdivisions[]`. A railroad without `subdivision` notes has
one subdivision with `id` and `name` `null`.

subdivisions[]: `id`, `name`, `mp_scale`, `mp_unit` (`in`, `ft`, `cm`, `m`), and `anchors[]`:
`id` (the reference note's id), `mp`, `track_id`. Anchors are sorted by MP.

## stations[]

One entry per `station` note. A joint station (one id on several subdivisions) has one entry per
subdivision, each with that subdivision's MP. A team track that belongs to no station is also
listed here, with `kind` `team_track` and its switch's MP.

| Field | Meaning |
|---|---|
| `id`, `name` | Location id and name |
| `kind` | `station` or `team_track` |
| `railroad`, `subdivision` | Owner of this entry |
| `mp` | The station's MP |
| `mp_entry`, `mp_exit` | The station's limits: its siding's switches, its switchback switch, or its terminus switch; both equal `mp` for a station without limits |
| `terminus` | Trains reverse here |
| `switchback` | A `switchback` note serves this station |
| `siding_ft`, `siding_cars` | The passing track joining two main switches, at least one of them the station's (a terminus's runaround starts at its terminus switch) |
| `main_ft`, `main_cars` | The main track on the shortest path between that siding's switches |
| `house_tracks[]` | `label`, `track_id`, `length_ft`, `capacity_cars` |

## yard_limits[]

`id`, `railroad`, `subdivision`, `mp_start`, `mp_end`, `source`. By default a station's zone is
its own limits (`source` `station`); `yard_limit` boards with the station's id move the zone's
ends out to them (`source` `board`). Boards whose id is no station make a zone of their own.

## industries[] and team_tracks[]

`id`, `name`, `station` (`null` for an on-line one), `branch_mp` (MP of the switch its track
leaves the main by), `switchback` (a switchback serves it), `car_spots` (from the note; default
2), `spur_capacity_cars` (what its track holds), `car_types`, `loads_in`, `loads_out` (lists,
empty when not given).

## yards[]

`id` (the location), `kind` (the location's kind: `station`, `industry`, `staging`,
`team_track`), and `tracks[]`: `label`, `function`, `railroads`, `track_id`, `length_ft`,
`capacity_cars`.

## switchbacks[]

`id`, `serves` (the location using it), `switch_ids` (the note's `switches`, else the main
switches its tail track leaves), `track_id` (the tail track the note sits on), `tail_ft`.

## storage[]

`id`, `name`, `track_id`, `length_ft`, `capacity_cars`, and `car_spots`: the cars parked
there on purpose, as the note gives it, or `null` (no default; conventions v1.1).

## facilities[]

`id`, `name`, `type`, `location`, `track_id`, `length_ft`, `capacity_cars`, and
`car_spots` as for `storage[]` (v1.1). A coal trestle or repair track with `car_spots`
holds parked cars; a turntable or water tank usually has none.

## segments[]

`from_station`, `to_station`, `railroad`, `subdivision`, `length_ft`: between consecutive
`stations[]` entries on each subdivision, by MP.

## issues[]

`severity` (`error`, `warning`, `info`), `rule` (stable id), `message`, `hint`, `note_id`,
`layer`, `group`, `track_id`, `x`, `y`. Match `rule`, not the message text.

## Mapping from v1

| v1 | v2 |
|---|---|
| `stations[].station_id` | `stations[].id` |
| `milepost_entry`, `milepost_exit` | `mp_entry`, `mp_exit` |
| (none) | `mp`: the station's MP. v1 users of `milepost_entry` as the station MP should review this |
| `siding_length_ft`, `siding_length_cars` | `siding_ft`, `siding_cars` |
| `main_length_ft`, `main_length_cars` | `main_ft`, `main_cars` |
| `house_track_ft`, `house_track_cars` | `house_tracks[]` (one per house track) |
| `industries[].industry_id` | `industries[].id` |
| `connected_station` | `station` |
| `branch_milepost` | `branch_mp` |
| `car_spots` (from spur length) | `car_spots` (from the note) and `spur_capacity_cars` (from length) |
| `yard_tracks[]` | `yards[].tracks[]` |
| `mp_scale` (top level) | `railroads[].subdivisions[].mp_scale` |
| `warnings` | `issues` |
