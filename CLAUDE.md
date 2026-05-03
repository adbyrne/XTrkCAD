# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Layout (`/home/abyrne/XTrkCAD/`)

```
.claude/               Development working files — plans, notes (not committed to repos)
docs/                  Project documentation not required in any repo
xtrkcad-hg/            Authoritative Hg source (r6422+), remote = SourceForge
xtrkcad-fork-xtrkcad/  Old Hg checkout — do not pull from (broken pull path), reference only
build/                 Out-of-source CMake build (gitignored)
CLAUDE.md              This file — committed into the git repo
```

## Build

Requires CMake ≥ 3.20, GTK+ 2.0, and `rsvg-convert` (replaces Inkscape for SVG→PNG; no D-Bus needed). Out-of-source builds are mandatory.

```sh
cmake -B build -S xtrkcad-hg -DCMAKE_BUILD_TYPE=Debug -DXTRKCAD_TESTING=ON
cmake --build build --parallel $(nproc)
```

Useful CMake options:
- `-DXTRKCAD_TESTING=ON` — build unit tests (auto-enabled when CMocka is found)
- `-DXTRKCAD_USE_DOXYGEN=ON` — generate Doxygen internals docs
- `-DXTRKCAD_CREATE_SVG=1` — SVG export (requires MiniXML; found automatically)

## Tests

5 unit tests using [CMocka](https://cmocka.org/). All pass on the current codebase.

```sh
ctest --test-dir build                  # run all tests
ctest --test-dir build -R pathstest     # run a single test by name
ctest --test-dir build --output-on-failure
```

Test sources: `app/bin/unittest/` and `app/dynstring/unittest/`.

## Doxygen

```sh
cmake -B build -S xtrkcad-hg -DXTRKCAD_USE_DOXYGEN=ON
cmake --build build --target docs-doxygen   # note: NOT --target doc
# Output: build/docs/doxygen/html/
```

## Code Formatting

AStyle enforces style. Config: `app/lib/astylerc` (1TBS, tab=8, max 80 cols, C mode).

```sh
astyle --options=xtrkcad-hg/app/lib/astylerc <file.c>
```

The `.editorconfig` at repo root encodes the same rules for editor integration.

## Known CMake Issues (track for CMake 4.x)

`app/bin/CMakeLists.txt` mixes keyword and non-keyword `target_link_libraries` calls on
`xtrkcad-lib` — line 284 uses `PRIVATE`, surrounding calls do not. CMake 4.0 makes
CMP0023 a hard error. Fix by adding `PRIVATE` to all non-keyword calls on that target.

## Architecture

XTrkCAD is a C application for designing model railroad layouts. Several libraries link into one executable:

### `app/bin/` — Core application (~105 K lines of C)

Built as `xtrkcad-lib`. Nearly all application logic lives here:
- **Track objects**: `track.c/h` — base track type, selection, modification
- **Drawing/geometry**: `draw.c`, `draw.h`, `drawgeom.c`
- **Commands** (`c*.c` prefix): `ccurve.c`, `cturnout.c`, `cselect.c`, `cgroup.c`, etc.
- **File I/O**: `fileio.c`, `paramfile.c` — native `.xtc`/`.xtce` format
- **DXF support**: `dxfformat.c`, `dxfimport.c`, `dxfoutput.c`
- **Undo/redo**: `cundo.c`
- **Scale definitions**: `scale.c`
- **Car/consist management**: `cars/` subdirectory

### `app/wlib/` — UI abstraction layer (`xtrkcad-wlib`)

Platform UI toolkit wrapper. Two backends:
- `gtklib/` — GTK+ 2.0 (Linux/macOS)
- `mswlib/` — Win32 (Windows)

All application code calls `wlib` APIs — never GTK/Win32 directly.

### `app/cornu/` — Spiro/Cornu curve library

Smooth easement curve calculations (`spiro.c`).

### `app/dynstring/` — Dynamic string library

Safe, growable C strings used throughout the application.

### `app/cJSON/` — Embedded JSON library

Used for serializing structured data (e.g., car definitions).

### `app/lib/` — Runtime resources

Parameter files (`params/`), example layouts (`examples/`), demo scripts (`demos/`),
AStyle config. Installed alongside the binary.

### Build tools (`app/tools/`)

Used only at build time:
- `cnvdsgn.c` — converts turnout symbol sources (`.src` → `.lin`)
- `bdf2xtp.c` — converts BDF bitmap fonts to XTP parameter files
- `pngtoxpm` — converts PNG bitmaps to XPM (Windows)

## Version

Current development version: **5.3.2Dev** (`ProgramVersion.cmake`). Binary named
`xtrkcad-beta` when version modifier is set, `xtrkcad` otherwise.

## File Formats

- `.xtc` / `.xtce` — native layout files (`.xtce` adds background-image support, v5.2+)
- `.xtp` — parameter files (turnout and equipment definitions)
- `.xtr` — demo/replay scripts
