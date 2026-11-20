# XTrkCAD Operating Data Conventions, version 1.1

How a layout file records its railroad as built, in JSON Notes and Layer Groups, so tools can
check it and an operating plan can use it. `check_conventions` checks a layout against these
rules; `export_layout_data` writes what they record as `layout_data.json`
([schema](layout_data_v2.md)). The MCP tools are listed in the [README](../README.md); the
[layout config guide](layout-config-guide.md) builds the Hillside example layout these
conventions are taught on.

Version 1.1 added `car_spots` on `facility` and `storage` notes (4.8), an info notice for an
industry on main or passing track (4.5), a warning for more car spots than the track holds
(4.5, 4.8), `storage[]` in the export (8) and the Op Density views (9.1). Track capacity is
counted between clearance points (2).

## 1. Scope and principles

These conventions say how a layout file (`.xtc`/`.xtce`) records its railroad **as built** in a
form an operating plan can use, and how tools check it. They are general; the Hillside Division
of the [tutorial](operating-conventions-tutorial.md) is their worked example.

Out of scope, **plan data**: trains, schedules, TO and R designations, equipment, waybills, crews,
rules. It belongs to the operations system (an operations program), which refers to layout objects by
`id`.

1. A fact tied to a place on the layout is a JSON Note placed at that place.
2. A set of layers with one meaning (category, railroad, subdivision) is a Layer Group.
3. Nothing about the layout is entered twice. Lengths, capacities and mileposts are computed,
   never typed into notes.
4. Every rule has a severity:
   - **error**: the export refuses (unless forced) and reports depending on the rule say so;
   - **warning**: reported, results still produced;
   - **info**: a default was applied or a value is worth a look.
5. Changes are additive. The spec has a version; the convention check and the export carry it.

## 2. Terms

| Term | Meaning |
|---|---|
| Layout units | the unit the layout is drawn in (XTrkCAD stores inches; designs use in, ft, cm or m) |
| Main track | track on a layer in the `Mainline` category group (3.1) |
| Location | a station, an industry, a team track or a staging yard: anything with a location id (4.0) |
| Team track | a public delivery track: shippers without a siding of their own load and unload cars there themselves (4.5a) |
| House track | the track at the railroad's freight house, where its staff handle less-than-carload freight (4.8) |
| Milepost (MP) | a position along main track, in MP units, counted from MP anchors (5) |
| MP scale | layout length per MP unit, with its unit (`12 in`, `1 ft`, `0.3 m`) |
| Railroad | an owning company (HD, the Hillside Division) |
| Subdivision | one MP line of a railroad; a railroad has one or more |
| Entry switch | a station's main-track switch closest to MP zero (lowest MP) |
| Tail track | the reversing track of a switchback (4.9) |
| Clearance point | where a car on one leg of a turnout stops fouling the other: where the two tracks are 13 ft apart, between centres (prototype, scaled to the layout) |
| Capacity | what a track holds: its length between the clearance points at its ends, in whole cars of the scale's cars per model foot (HO: 2). Track past a turnout's points, shared by both routes, isn't reduced; switch throats don't count ([schema](layout_data_v2.md), units) |

## 3. Layer Groups

### 3.1 Category groups (reserved names)
`Mainline`, `Passing`, `Staging`, `Storage`, `Service`, `Connecting`, `Scenery`, `Ignore` (Fugate).
- error: a layer in two category groups.
- warning: a passing track joined to main track without a turnout: usually a piece drawn on the
  wrong layer (XTrkCAD puts a new track on the current layer), and the station's siding can't be
  found.
- warning: a layer holding track in no category group. Its category then comes from the
  `L<n>-Suffix` layer name (`L2-VL-Staging` -> Staging), else Mainline.

### 3.2 Railroad and subdivision groups
A `railroad` or `subdivision` note (4.1) names the Layer Group holding its layers; any group name
works.
- error: a named group doesn't exist; a layer in two railroad groups; a layer in two subdivision
  groups.
- warning: a layout with more than one railroad and track layers in none of their groups.

### 3.3 Level
The `L<n>-` layer-name prefix, as today.

## 4. JSON Note kinds

### 4.0 Common rules
- Every note has `kind` and `id`. Optional on every kind: `name` (display name), `railroad`,
  `subdivision` (defaults: from the note's layer's groups, else the layout's only railroad or
  subdivision).
- **Location ids** (`station`, `industry`, `team_track`, `staging`) share one namespace and are unique across
  it, also on a layout with several railroads: a note's `railroad` says who owns it, so the id
  needn't. One namespace lets `yard_track`, `house_track` and `facility` name any location
  without saying which kind it is. Other kinds' ids are unique within their kind, except where a kind groups notes by `id`
  (`yard_limit`, `yard_track`, `house_track`, and `station` for a joint station, 4.3).
- A note's **position is data**: it snaps to the nearest track on its own layer.
  - error: no track within the snap distance;
  - warning: a large snap distance (probably on the wrong track).
- Unknown fields are kept and ignored, so plan tools may add their own.

### 4.1 `railroad`, `subdivision`
```json
{"kind": "railroad", "id": "HD", "name": "Hillside Division", "reporting_marks": "HD",
 "group": "HD"}
{"kind": "subdivision", "id": "WEST", "name": "West Subdivision", "railroad": "HD",
 "group": "HD-WEST"}
```
- Position not used.
- Both optional for a layout with one railroad with one subdivision.
- `group` is optional on a subdivision: one railroad's subdivisions may share layers.
- `group` names one Layer Group. Other groups may hold the same layers for other purposes
  (per-level groups like `Level 1`/`Level 2`, for printing), so a railroad spread over levels
  gets one group of its own (`HD`) beside them.

### 4.2 `reference`: MP anchor or named point
```json
{"kind": "reference", "id": "MP_ZERO", "mp": 0, "mp_scale": 12, "mp_unit": "in"}
{"kind": "reference", "id": "VL_START", "mp": 412.3, "railroad": "VL"}
{"kind": "reference", "id": "BI_EAST"}
```
- With `mp`: an **MP anchor**: the track at the note *is* that milepost (5).
- `mp_scale` and `mp_unit` (`in`, `ft`, `cm`, `m`; default `in`) go on one anchor per subdivision,
  normally the first.
- Without `mp`: a named point other notes refer to (`ref_tag`).
- error:
  - a subdivision with main track and no anchor;
  - anchors and no `mp_scale`;
  - an anchor not on main track;
  - two `mp_scale` values in one subdivision.

### 4.3 `station`
```json
{"kind": "station", "id": "AL", "name": "Alder"}
{"kind": "station", "id": "HY", "name": "Hillside", "terminus": true, "ref_tag": "MP_ZERO"}
{"kind": "station", "id": "DL", "name": "Dale", "mp_at_note": true}
```
- Placed on main track at the station.
- `terminus`: a line end, so there's no exit.
- `ref_tag`: route from that named point when the note sits on an isolated track. Such a note
  may sit off the main, on the station's own track (Hillside's HY: its note is on a yard track,
  and the main ends at the MP_ZERO switch, the yard throat).
- `mp_at_note`: see 5.2.

**Joint station**: one station served by more than one railroad or subdivision. It is one
location (one `id`) with one `station` note per subdivision, each placed on that subdivision's
main track. Each note gets its own MP, limits, `terminus` and `ref_tag` from its own main; yards,
industries, house tracks and facilities name the station by its one `id`.
```json
{"kind": "station", "id": "CE", "name": "Cedar", "terminus": true}
{"kind": "station", "id": "CE", "railroad": "VL"}
```
For example, Cedar as a terminus of the Hillside Division (lower level) and a station on the
Valley line's main (upper level); its interchange tracks are where the two meet.
- error: two `station` notes with one `id` in the same subdivision.
- warning: notes of one station with different `name`s (give it on one note, or the same on all).

### 4.4 `yard_limit`: a YL board (optional)
```json
{"kind": "yard_limit", "id": "CE"}
```
- Yard limits are timetable and rules data: a layout needs no YL notes, and
  a station without one is not reported.
- **Default zone:** every station's yard limits are its own limits, entry to exit switch
  (5.2). A physical board (this note) with the station's id moves the zone's end out to the
  board: Hillside's CE is a terminus at its switch, MP 21.505, with a board out on the line at
  MP 20.244, so its zone is MP 20.244-21.505. Boards
  whose id is no station make a zone of their own.
- One note per board, placed on main track.
- Boards with the same `id` bound one yard-limit zone: main track only, with special operating
  and speed rules. YL zones may be anywhere on the main. Stations usually have them at their
  switches, so local moves can work.
- Board order comes from their MPs.
- error: a board not on main track; more than 2 boards with one `id`.
- warning: 1 board, unless its id is a terminus station: only a terminus's zone runs to the end
  of the line, so trains meet its board from one side only. A pass-through or switchback station,
  where trains come from both ends, has a board at each end, or none.

### 4.5 `industry`
```json
{"kind": "industry", "id": "MINE", "name": "Ridge Mine", "station": "BI",
 "car_spots": 2, "car_types": ["HM"], "loads_in": ["empties"], "loads_out": ["coal"]}
```
- Placed on its spur.
- `station`: the station whose limits it is switched from. Leave it out for an on-line industry.
- `car_spots`: the number of spots, chosen for operations (default 2). The spur's capacity (2),
  computed from the track, is exported separately.
- `car_types`: car type codes (7).
- `loads_in`, `loads_out`: load types (7).
- error: `station` names no station.
- info: `car_spots` defaulted.
- info: the industry is on main or passing track (allowed: an industry on the main is usually a
  timetable station; IOTtrains uses it).
- warning: more `car_spots` (given or defaulted) than its track's capacity (2).
- warning: an unknown car type code.

### 4.5a `team_track`: a public delivery track
```json
{"kind": "team_track", "id": "AL_TEAM", "name": "Alder team track", "station": "AL",
 "car_spots": 3}
{"kind": "team_track", "id": "MILLER_TEAM", "name": "Miller", "car_spots": 2}
```
- A siding open to the public: merchants, farmers and small businesses without a siding of
  their own load and unload cars there themselves (also "public delivery track"). Usually near
  a station, sometimes standing alone.
- Placed on its track. Fields as `industry` (4.5): `station`, `car_spots`, `car_types`,
  `loads_in`, `loads_out`. `car_types` and loads are usually left out: being public, it takes
  any car and load.
- `station`: the station it is switched from. **Without `station` it is a timetable station
  itself** (a stand-alone team track away from any station): the export lists it in
  `stations[]`, its MP where its track leaves the main.
- A team-track yard (several tracks) uses `yard_track` notes with the industry functions.
- One track that is both the house track and the team track carries both notes.
- error: `station` names no station. info: `car_spots` defaulted; no `station` (a timetable
  station); on main or passing track (as 4.5). warning: an unknown car type code.

### 4.6 `staging`: a staging yard
```json
{"kind": "staging", "id": "VL_EAST", "name": "East Staging", "railroad": "VL"}
```
- Placed on the staging yard's lead.
- Its tracks are `yard_track` notes with this `id`. Staging is laid out like a yard: it has
  switches and may set tracks aside for one use, so its tracks take any yard `function`
  (`interchange`, `caboose`, `engine`, ...) as well as `staging`.

### 4.7 `yard_track`
```json
{"kind": "yard_track", "id": "HY", "label": "Track 2", "function": "classification"}
{"kind": "yard_track", "id": "CE", "label": "Interchange", "function": "interchange",
 "railroads": ["HD", "VL"]}
{"kind": "yard_track", "id": "MINE", "label": "Loading 1", "function": "loading"}
```
- One note per track, placed on it.
- `id`: the yard's location id: a station (HY), an industry (a mine's own tracks) or a
  staging yard.
- `label`: unique within the yard.
- `function`, one of:
  - yard: `arrival`, `departure`, `arrival_departure`, `classification`, `runaround`, `lead`,
    `interchange`, `caboose`, `rip`, `storage`, `engine` (a team track is its own kind, 4.5a);
  - industry and team track: `loading`, `empties`, `loads`, `service`, `lead` (where the
    switcher works the industry's tracks);
  - staging: `staging`, or any yard function (4.6).
- `railroads`: required for `interchange`; the railroads exchanging cars there. This replaces a
  separate interchange kind.
- error: `id` is no location id; a duplicate `label` within a yard; `interchange` without 2 known
  railroads.
- info: no `function`.

### 4.8 `house_track`, `storage`, `facility`
```json
{"kind": "house_track", "id": "BI"}
{"kind": "house_track", "id": "CE", "label": "Freight house"}
{"kind": "storage", "id": "CAR_BARN", "name": "Car barn"}
{"kind": "facility", "id": "CE_COAL", "name": "Cedar coal trestle", "type": "coal", "location": "CE",
 "car_spots": 3}
```
- `house_track`: the freight-house track (LCL freight, handled by the railroad), one note per
  track. A team track is its own kind (4.5a). `id` is a station or
  industry id; a location may have several, like an industry's tracks. `label` is optional for
  one; with several, each needs its own `label` (error otherwise).
- `storage`: a stand-alone car storage spur.
- `facility`: a servicing point, placed on its track: a railroad's own industry (turntable,
  repair track, coal trestle). `type` is one of `ice`, `engine_service`,
  `turntable`, `fuel`, `water`, `sand`, `coal`, `ash`, `scale`, `car_repair`. `location`
  (optional) is a location id.
- `car_spots` (optional, `facility` and `storage`): cars parked there on purpose. **No default**,
  for any `type`: only a note that gives it counts, and those spots count as storage in Op
  Density (9.1). A facility without it holds no cars (engines only, or not used for cars).
  - warning: more `car_spots` than its track's capacity (2).

### 4.9 `switchback`: a track configuration
```json
{"kind": "switchback", "id": "MINE_SWB", "serves": "MINE"}
{"kind": "switchback", "id": "SUMMIT_SWB", "serves": "SUMMIT", "switches": [412, 418]}
```
- Placed on the tail track. A switchback is a track configuration, not an attribute of a station
  or industry.
- `serves`: the location that uses it.
- `switches`: the main-track switch(es) that together act as the switchback switch.
  - Default: the switch the tail track leaves.
  - Usually one. Two when, for example, the passing track rejoins the main beyond the first
    switch.
- The tail track may end the main, or continue into another subdivision, an industry or yard
  tracks. It is never main track.
- error: the tail track is on a Mainline layer.
- warning: a switchback station with more than one main-track switch and no `switches`.

### 4.10 `terminus`: where a terminus starts
```json
{"kind": "terminus", "id": "CE"}
```
- `id`: the terminus station's id (the station has `terminus: true`).
- Placed on the main-track switch where the terminus starts. The track beyond the switch's
  legs is the station's own: it may stay on a Mainline layer (op density counts it as main),
  but it is not running line. MPs stop at the switch: anything on that track has the switch's
  MP.
- **Default:** a terminus station without a terminus note starts at the MP zero anchor when its
  `ref_tag` names that anchor (Hillside's HY: MP_ZERO at its yard throat). Other termini need
  the note (Hillside's CE: on the switch where its runaround starts).
- error: `id` names no station; the note isn't on a main-track switch.
- warning: the station isn't marked `terminus`; a terminus station with neither a terminus
  note nor a `ref_tag` to the zero anchor.

## 5. Mileposts

### 5.1 Along the main
- Within a subdivision, MPs increase **away from zero** (the anchor with `mp: 0`).
- MP at a point = the MP of the **last anchor passed** + the main-track distance since it /
  MP scale.
- A hard anchor **resets** the count: a calculated 10 can meet an anchor that says 40, and the
  count goes on from 40.
- A subdivision **without a zero** (a line from staging to staging, like the Valley line) has a
  start anchor where its visible trackage begins and an end anchor. MPs increase from start
  toward end: the count starts at its lowest anchor.
- Distances follow the subdivision's main track (its Mainline layers) only. A switch's MP is
  taken at its points.
- info: the difference between a calculated MP and the next anchor.

### 5.2 Stations
- **Pass-through station**: its limits are its passing siding's two switches; its MP is the
  midpoint of the main between them.
- **Terminus**: the MP of the switch where the terminus starts (4.10); its limits are that
  switch.
- **Without a passing siding**: the MP at the note's position.
- **Switchback station**: the midpoint of the main between the entry switch (its passing
  siding's lower switch, or the tail switch when it has no siding) and the tail switch.
- A station note whose track doesn't reach the main takes the MP of its `ref_tag` point.
- `mp_at_note: true`: the MP at the note's position instead, for a note placed away from the
  midpoint on purpose. It never resets the count; a hard value is a `reference` anchor.
- The export also gives `mp_entry` and `mp_exit`, so siding and reversing-lead lengths stay
  available.

## 6. Rules across kinds (summary)

| Rule | Severity |
|---|---|
| unknown `kind`, missing `id`, invalid JSON in a note that has a `kind` | error |
| reference to an unknown location, railroad, subdivision or Layer Group | error |
| duplicate ids (4.0) | error |
| note not on any track / far from its track | error / warning |
| category, railroad, subdivision group conflicts (3) | error |
| passing track joined to main track without a turnout (3.1) | warning |
| anchors and MP scale (4.2) | error |
| switchback tail on main (4.9) | error |
| defaults applied (`car_spots`, category from layer name) | info |
| industry or team track on main or passing track (4.5) | info |
| more car spots than the track holds (4.5, 4.8) | warning |
| stand-alone team track is a timetable station | info |

## 7. Vocabulary

- **Car type codes**: AAR mechanical designations (`XM`, `RS`, `SM`, `FM`, `HM`, `GB`, `TM`, ...).
  Extra codes come from the equipment source, which uses the same codes.
- **Load types**: free text, matched against the equipment source when one is given.

## 8. Export: `layout_data.json` v2

General, versioned; IOTtrains is the first consumer. Units: lengths in prototype feet unless the
field says `_in` (layout inches); MPs in MP units. Track lengths (`_ft`) and capacities (`_cars`)
are measured between clearance points (2), so a siding or spur counts less than its whole length.

```text
schema_version: 2, conventions_version: 1, generated, layout, scale, layout_units
railroads[]       id, name, reporting_marks, subdivisions[]: id, name, mp_scale, mp_unit, anchors[]
stations[]        id, name, railroad, subdivision, mp, mp_entry, mp_exit, terminus, switchback,
                  siding_ft, siding_cars, main_ft, main_cars,
                  house_tracks[]: label, track_id, length_ft, capacity_cars
                  (one entry per station and subdivision: a joint station has one per
                  subdivision, with the same id)
yard_limits[]     id, railroad, subdivision, mp_start, mp_end, source (station | board)
industries[]      id, name, station, branch_mp, switchback, car_spots, spur_capacity_cars,
                  car_types, loads_in, loads_out
team_tracks[]     the same fields as industries[]; one without station is also in stations[]
                  (kind team_track, mp = its branch_mp)
yards[]           id, kind (station|industry|staging), tracks[]: label, function, railroads,
                  track_id, length_ft, capacity_cars
storage[]         id, name, track_id, length_ft, capacity_cars, car_spots (null if not given)
switchbacks[]     id, serves, switch_ids, tail_ft
facilities[]      id, type, location, track_id, length_ft, capacity_cars,
                  car_spots (null if not given)
segments[]        from_station, to_station, subdivision, length_ft
issues[]          the convention check (9)
```

Mapping from v1: `station_id` -> `id`, `milepost_entry` -> `mp_entry`, `milepost_exit` ->
`mp_exit`, `industry_id` -> `id`, `branch_milepost` -> `branch_mp`; `yard_tracks[]` -> `yards[]`.
v1 `milepost` users (IOTtrains' timetable) now get `mp`, the station MP (5.2): a change to review
at import.

## 9. Tools (MCP)

- `check_conventions(path)`: every rule, as issues: severity, rule id, object (note, layer, group,
  track), position, fix hint.
- `apply_conventions(path, dry_run=True)` (planned, not built yet): upgrade notes in place:
  - text prefixes -> JSON, including `Yard_TRACK:` spellings;
  - `!SWB` / `switchback: true` -> a `switchback` note on the tail: the off-main track leaving a
    main turnout through its points nearest the station. One candidate is placed (info in the
    result); several are listed for the user;
  - `STORAGE:` entries that are facilities are listed for the user, not changed.

  Dry run first; backup and versioned output like the other edit tools.
- `export_layout_data`: v2; refuses on errors unless `force`; includes `issues`. Option `v1` keeps
  the current format until IOTtrains imports v2.
- Reports (Op Density, station distances, capacities) run the rules they depend on and report
  failures.

### 9.1 Op Density views

Storage, in Op Density, is how many cars can be parked on the layout while trains still run on
the main and passing tracks. The report has three views:

1. **Fugate, by layer category** (as now): Track by Category/Level and the Fugate metrics.
   Service-layer track counts only through facility `car_spots` (view 3), which are added to
   storage; the rest of a Service layer (turntable leads, engine tracks) holds no cars.
2. **Industries and yards, from the notes**: per industry and team track its `car_spots`, its
   track capacity and the difference (track that holds cars no spot serves, e.g. a long spur to a
   2-spot industry); yards by track and function; `storage` notes. Industries on main or passing
   track are listed on their own line and stay out of the storage total (a car spotted there
   blocks the line).
3. **Railroad service, from `facility` notes**: each facility's `car_spots`, counted as storage.
   A facility without `car_spots` is listed as not counted (a report notice, not a check rule:
   turntables and water tanks would otherwise all be flagged).

Compatibility: text-prefix notes and the current JSON fields are still read. `stations.yaml` is
retired; `validate_layout`/`export_layout_data` ignore it, with a notice that says so.

## 10. Open items

1. Equipment source format (CSV/ODS columns). Only the vocabulary (7) matters to this spec.
2. Station limits other than the passing siding: no case yet; revisit with one.
