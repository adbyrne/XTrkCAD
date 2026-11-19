# Operating your layout: a tutorial

Part 2 of the Hillside tutorial. [Part 1, the layout config guide](layout-config-guide.md),
built the Hillside Division: a 12×16 ft room, shelves around three walls, and a track plan
laid by `generate_track_plan`. This part makes that layout **operable**: it records the
railroad as built (where the stations are, which tracks are industries and yards, how far it is
from one place to the next) so a check can find mistakes and an operations program can use it.

The rules are the [Operating Data Conventions](operating-conventions.md); section numbers in
brackets, like [4.3], point into them. Every example below was run through the tools, and the
results are quoted as the tools give them; the test suite rebuilds each chapter's layout and
checks those results (`mcp/tests/test_tutorial.py`), so this page can't drift from the tools.

**Who this is for:** you've drawn a layout in XTrkCAD and know layers and notes. You don't need
to know JSON Notes, Layer Groups or the MCP tools yet.

**How to read the examples.** Each step shows what you'd ask your assistant (Claude, with the
XTrkCAD MCP server connected), the tool call that answers it, and what comes back:

> *Check my layout's operating data.*

```
check_conventions("hillside_division.xtc")
```

You can also call the tools yourself; the prompt is just the plain-language version.

---

## 1. What this is for

A layout drawing says where the track goes. Operating it needs more: that this siding belongs to
Alder, that the spur behind it is a feed mill taking two cars, that Cedar is the end of the line
20.8 miles out. The conventions record those facts **in the layout file itself**, so they move
with the drawing and are never typed twice.

They record the railroad **as built**. Trains, schedules, waybills and crews are the operations
system's job; it reads the layout's data and refers to places by their `id` [1].

The flow:

1. Draw the layout in XTrkCAD (Part 1 did that for Hillside).
2. Add the operating data: JSON Notes on the track, Layer Groups for the layers.
3. `check_conventions` finds what's missing or wrong.
4. `export_layout_data` writes `layout_data.json` for an operations program
   ([schema](layout_data_v2.md)).

Three rules of thumb run through all of it:

- **A fact sits where it is on the layout.** A station's note goes on the station's main track;
  an industry's on its spur. The note's position is part of its data.
- **A set of layers with one meaning is a Layer Group.** "These layers are the main line" is a
  group named `Mainline`.
- **Nothing is typed twice.** Lengths, car capacities and mileposts are computed from the track;
  notes never hold them.

## 2. Before you start

**The layout.** Start from Part 1's result: the Hillside layout with its track.

> *Generate the Hillside layout and lay its track plan.*

```
generate_layout("hillside_division.yaml", "hillside_division.xtc")
generate_track_plan("hillside_division.yaml", "hillside_division.xtc")
```

*[Screenshot: the Hillside layout in XTrkCAD, Level 1.]*

**Layer names.** Hillside's layers are named `L1-Main`, `L1-Passing`, `L1-Storage` and so on:
the `L1-` prefix is the level (deck), the rest says what kind of track the layer holds [3.3].

**JSON Notes.** A JSON Note is an XTrkCAD note whose text is one JSON object, like
`{"kind": "station", "id": "AL"}`. Place one with **Draw > Note > JSON Note** (or the Notes
button on the toolbar): click where it goes, then type the object into the dialog's **JSON**
field. Every note the conventions read has a `kind` (what it is) and an `id` (its short name);
the other fields depend on the kind [4.0].

*[Screenshot: the JSON Note dialog with a station note.]*

**Position is data.** A note belongs to the nearest track **on its own layer**: put a station's
note on an `L1-Main` layer, on the main. A note that lands beside its track is reported:

```
warning NOTE_FAR_FROM_TRACK
  station BI: 5.0 in from the nearest track (44)
  hint: It is on track 43 of layer L1-Passing: move the note to that layer.
```

That note was put at y=175 instead of on the main at y=180, near Birch's siding.

**Reading the check.** `check_conventions` lists every rule the layout breaks, errors first.
Each finding has a severity, a rule id (stable: match it, not the wording), a message, often a
hint, and the note, layer and track it's about [1, 9]:

- **error**: something the export can't work around; it refuses until it's fixed (or forced);
- **warning**: reported, results still produced;
- **info**: a default was used, or something is worth a look.

## 3. Categories: telling the main from the rest

Every track layer has a **category**: main line, passing track, storage, staging, connecting
track, service, scenery or ignored. A category is a Layer Group with one of those reserved names:
`Mainline`, `Passing`, `Staging`, `Storage`, `Service`, `Connecting`, `Scenery`, `Ignore` [3.1].

Hillside already has them: `generate_layout` makes one per track type, holding that type's
layer on each level (**Manage > Layer Groups…** shows them):

| Group | Layers |
|---|---|
| `Mainline` | L1-Main, L2-Main |
| `Passing` | L1-Passing, L2-Passing |
| `Storage` | L1-Storage, L2-Storage |
| `Staging` | L1-Staging, L2-Staging |
| `Connecting` | L1-Connecting, L2-Connecting |
| `Service` | L1-Service, L2-Service |

*[Screenshot: the Layer Groups dialog.]*

(The `Level 1` and `Level 2` groups are for showing and printing one deck; they aren't
categories.)

Main track is what everything else is measured along, so the category matters. Two mistakes
the check catches:

- **A layer in two category groups** is an error: a layer has one category.

  ```
  error CATEGORY_LAYER_IN_TWO
    Layer L1-Main is in category groups Mainline and Passing
    hint: A layer has one category; take it out of all but one group.
  ```

- **A track layer in no category group** is a warning. Its category then comes from its name
  (`L1-Main` is main line), but say it with a group.

  ```
  warning CATEGORY_LAYER_NONE
    Track layer L1-Main is in no category group; mainline from its name
    hint: Add it to one of: Mainline, Passing, Staging, Storage, Service, Connecting, Scenery, Ignore
  ```

Run the check on Hillside now and it finds one thing, the subject of the next chapter:

> *Check my layout's operating data.*

```
error MP_NO_ANCHOR
  The layout has main track but no MP anchor
  hint: Add a reference note with "mp" (0 at MP zero) on its main track.
```

## 4. Mileposts

A **milepost** (MP) is a position along the main line. Timetables, train orders and the
operations program all locate places by MP, so the layout needs MPs. You don't type them in:
you fix MP zero, say how much layout length is one mile, and the tools count the rest along the
main [5.1].

**MP zero** is a `reference` note with `"mp": 0`, placed on the main track at that point. It
also carries the **MP scale**: how much layout one MP is [4.2]. Hillside puts MP zero at the yard
throat, the first switch the main meets, and counts one MP per foot:

```json
{"kind": "reference", "id": "MP_ZERO", "mp": 0, "mp_scale": 12, "mp_unit": "in"}
```

Place it on `L1-Main` at the yard throat switch (x 12, y 70.7).

*[Screenshot: the MP_ZERO note on the yard throat switch.]*

- `mp_scale` and `mp_unit` (`in`, `ft`, `cm` or `m`) say how long one MP is: 12 in here. Pick
  whatever makes your timetable read well; a big layout might use 24 in.
- A note with an `mp` is an **anchor**: the track at the note *is* that milepost. MPs then grow
  along the main away from zero.
- A second anchor further along resets the count, if you want a place to have an exact MP: the
  count carries on from the anchor's value.
- A railroad that has no zero on the layout (a line passing through, from staging to staging)
  uses a start anchor and an end anchor instead [5.1].

Check again, and the layout is clean: 0 errors, 0 warnings, 0 info. The MPs are there, but
nothing is placed along the main yet; that's the stations' job.

## 5. Stations

A **station** is a named place on the main: a timetable stop. Its note goes on the main track at
the station [4.3]. Where the MP of a station comes from depends on its track [5.2]:

- **A station with a passing siding** has the siding's two switches as its limits, and its MP is
  the midpoint of the main between them.
- **A terminus**, the end of the line, has no exit: its MP is the switch where the terminus starts.
- **A station without a siding** takes the MP at its note.

Hillside has four. Alder and Birch are pass-through stations with sidings:

```json
{"kind": "station", "id": "AL", "name": "Alder"}
{"kind": "station", "id": "BI", "name": "Birch"}
```

Place Alder's on `L1-Main` between its two switches (x 12, y 109.5), and Birch's on the north
wall's main (x 72, y 180).

**Hillside, the yard**, is the line's home terminal at MP zero. Its note sits on a yard track,
off the main, so it says where it joins the main with `ref_tag`: the named point MP_ZERO.
`terminus` says no train runs beyond it.

```json
{"kind": "station", "id": "HY", "name": "Hillside", "terminus": true, "ref_tag": "MP_ZERO"}
```

Place it on `L1-Storage`, on yard track 1 (x 12, y 30).

**Cedar** is the other end of the line. A terminus needs to know where it starts: a `terminus`
note on the main-track switch where the terminus begins [4.10]. For Cedar that's the first
switch of its runaround:

```json
{"kind": "station", "id": "CE", "name": "Cedar", "terminus": true}
{"kind": "terminus", "id": "CE"}
```

Place the station note on the main between Cedar's switches (x 132, y 114) and the terminus note
on the runaround's north switch (x 132, y 139). Leave the terminus note out and the check asks:

```
warning TERMINUS_NO_START
  Terminus station CE: where does the terminus start?
  hint: Add a terminus note on the main-track switch where it starts (or ref_tag the MP zero
  anchor, if the terminus starts there).
```

Hillside's terminus needs no such note: it starts at MP zero, which its `ref_tag` names.

A station note must sit on main track. One placed on the passing siding instead is reported
twice: it isn't on the main, and the MP count can't reach it along the main:

```
warning MP_UNREACHABLE
  Station AL2 can't be reached along the main from MP_ZERO
warning STATION_NOT_ON_MAIN
  Station AL2 is not on main track
  hint: Place a station note on the main at the station.
```

With the four notes in place the check is clean again, and the export now has the stations,
their MPs and their sidings:

> *Export my layout's operating data.*

```
export_layout_data("hillside_division.xtc")
```

| Station | MP | Limits (MP) | Siding | Main between the switches |
|---|---|---|---|---|
| HY Hillside | 0.0 | terminus | | |
| AL Alder | 3.24 | 0.587 – 5.892 | 7 cars | 6 cars |
| BI Birch | 13.396 | 10.744 – 16.049 | 7 cars | 6 cars |
| CE Cedar | 20.792 | terminus | 6 cars | 5 cars |

The track plan asked for 5-car sidings at Alder and Birch: that's the straight part, clear of
the curves. The export counts the whole passing track from switch to switch, curves included,
so it gives 7. Both are right; they measure different things.

*(Chapters 6–12 follow: yard limits, industries, yards, facilities, switchbacks, more than one
railroad, and exporting.)*
