# XTrkCAD Development Plan

_Last updated: 2026-05-12 (rev 4 — A1/A2/A4 done; A3 N/A; A5-A6/A7 branches open)_

## Workflow

**VCS:** Git-primary. Develop in named branches (one per goal), push to GitHub, CI validates.  
When ready for upstream submission: generate Hg patch, apply to both Hg clones, push to SF.

**Branch naming:** `feature/<topic>` or `fix/<topic>`. Create from `main`; apply to `GTK3V2MAIN` as well (dual-branch policy until upstream merges GTK3V2MAIN).

**Repos:**
- Working git: `https://github.com/adbyrne/XTrkCAD` — `main` (GTK2) + `GTK3V2MAIN` (GTK3)
- Upstream Hg: `ssh://adbyrne1905@hg.code.sf.net/p/xtrkcad-fork/xtrkcad`
- SF tickets: `https://sourceforge.net/p/xtrkcad-fork/bugs/`

---

## Size key

**S** = hours to ~2 days · **M** = ~3 days to 2 weeks · **H** = weeks to months

---

## Theme A — CMake Modernization

Goal: adopt current CMake best practices, unblock CMake 4.x, improve developer experience.

| ID  | Size | Item                                                                                       | Branch                          | SF ticket | Status  |
|-----|------|--------------------------------------------------------------------------------------------|---------------------------------|-----------|---------|
| A1  | S    | `cmake_minimum_required(VERSION 3.20...4.0)` range syntax — silence policy warnings       | `fix/623-cmake-version-range`   | #623      | ✅ done  |
| A2  | S    | Replace non-keyword `target_link_libraries` with `PRIVATE/PUBLIC/INTERFACE` (CMP0023)     | `feature/624-cmake-tll-keywords`| #624      | ✅ done  |
| A3  | S    | ~~Use `ZLIB::ZLIB` target name consistently~~ — already correct, no change needed         | —                               | —         | ✅ N/A   |
| A4  | S    | Guard `add_dependencies` calls with `if(TARGET ...)` for conditional targets               | `fix/625-cmake-dep-guards`      | #625      | ✅ done  |
| A5  | S    | cmake `-P` wrapper for rsvg-convert (bitmaps) — first priority of ~15–20 bare COMMAND calls| `bug619-fix`                   | #619      | 🔧 open  |
| A6  | M    | cmake `-P` wrappers for remaining tool invocations (msgfmt, genhelp, genmessages, halibut) | `bug619-fix`                   | #619      | 🔧 open  |
| A7  | M    | `CMakePresets.json` — `debug-linux`, `debug-macos`, `debug-windows-vcpkg`, `release-linux` | `feature/626-cmake-presets`    | #626      | 🔧 open  |
| A8  | M    | `vcpkg.json` manifest mode — replace explicit `vcpkg install` step in CI                   | `feature/cmake-vcpkg`      | —         |
| A9  | M    | FetchContent fallback for cmocka and mxml (build from source when system package absent)   | `feature/cmake-fetchcontent`| —        |
| A10 | M    | Split large CMakeLists.txt files into domain-scoped includes (Configure/Platform/Build/Install/Test) | `feature/cmake-structure` | — |
| A11 | H    | Bump `cmake_minimum_required` to 4.0 — after all CMP0023/CMP0057/etc. hard errors fixed   | `feature/cmake4`           | —         |

---

## Theme B — Testing & Quality

Goal: increase confidence in correctness; catch regressions earlier; add static analysis.

| ID  | Size | Item                                                                                       | Suggested branch            | SF ticket |
|-----|------|--------------------------------------------------------------------------------------------|-----------------------------|-----------|
| B1  | S    | Remove `log_calls()` debug artifact from `mapwindow.c` (uses `backtrace`/`execinfo.h`)    | `fix/mapwindow-debug`       | —         |
| B2  | S    | clang-tidy CI gate (report-only initially; select rules; integrate with CI matrix)         | `feature/clang-tidy`        | —         |
| B3  | M    | AddressSanitizer + UBSan CMake preset and CI job (Linux/Clang)                             | `feature/sanitizers`        | #620      |
| B4  | M    | New unit tests: geometry (curve/straight intersections, easement/Cornu transitions)        | `feature/tests-geometry`    | —         |
| B5  | M    | New unit tests: fileio (`.xtc` round-trip parse → write → re-parse comparison)             | `feature/tests-fileio`      | —         |
| B6  | M    | New unit tests: scale/gauge validation (legal gauge values, DPI conversions)               | `feature/tests-scale`       | —         |
| B7  | H    | Test coverage report (lcov/gcovr) in CI; set a minimum coverage floor                     | `feature/coverage`          | —         |

---

## Theme C — MCP Server Phase 2

Goal: add write capability to the MCP server; operational density analysis; layout generation.

Phase 1 complete: 16 tools, 27 tests. Source: `xtrkcad-git/mcp/`.

| ID  | Size | Item                                                                                       | Suggested branch            | SF ticket |
|-----|------|--------------------------------------------------------------------------------------------|-----------------------------|-----------|
| C1  | S    | Reconnect NYE layout tracks 421/506 (gap left by deleted track 505)                        | `feature/mcp-nye-fix`       | —         |
| C2  | S    | Operational density formula integration (Fugate MRH Oct 2014; `docs/Layout_Design_Assessment_Formulas.pdf`) | `feature/op-density` | #217 |
| C3  | M    | MCP write tools: `add_straight_track`, `add_curved_track`, `connect_tracks`                | `feature/mcp-write-tools`   | #216      |
| C4  | M    | MCP write tools: `delete_track`, `move_track`, `set_track_layer`                           | `feature/mcp-write-tools`   | #216      |
| C5  | M    | Layout generation: produce `.xtc` snippet from a density + dimension spec                  | `feature/mcp-layout-gen`    | —         |
| C6  | H    | Interactive layout design assistant (iterative MCP loop: propose → evaluate → revise)      | `feature/mcp-assistant`     | —         |

---

## Theme D — Windows / GTK3 Migration (transitional)

**Architecture decision (Martin Fischer, 2026-05-12):** `mswlib` and `gtklib` are both transitional
abstraction layers that will be removed when the GTK3 migration is complete. Do not invest in
implementing the 58 stub functions in `mswstubs.c` — they exist only to keep the Windows CI build
linking during the transition. The long-term Windows platform target is GTK3 running natively on
Windows via the official GTK runtime (vcpkg); MSYS2/MinGW is not a target.

Current state: `mswstubs.c` — 58 stubs, all return no-ops or safe defaults. **Good enough for now.**

_Architecture details for the GTK3-direct replacement are pending clarification from Martin
(open question: what replaces wlib on Windows; official GTK runtime distribution strategy;
timeline for wlib removal from the codebase)._

| ID  | Size | Item                                                                                           | Suggested branch              | SF ticket |
|-----|------|------------------------------------------------------------------------------------------------|-------------------------------|-----------|
| D1  | S    | Maintain `mswstubs.c` as a build shim only — fix compile errors if wlib API changes, no more  | ad-hoc on `feature/*` as needed | #618    |
| D2  | M    | GTK3-on-Windows CI: switch Windows job from MSVC+wlib to GTK3 official runtime via vcpkg      | `feature/win-gtk3-runtime`    | TBD       |
| D3  | H    | Remove `mswlib/` and `gtklib/` — requires GTK3-direct wlib replacement to be in place         | `feature/wlib-removal`        | TBD       |
| D4  | H    | GTK3-direct wlib replacement — depends on upstream architecture decision from Martin           | `feature/wlib-gtk3-direct`    | TBD       |

---

## Waiting for

- **Martin Fischer (architecture clarification)** — GTK3-direct replacement for wlib on Windows;
  official GTK runtime distribution strategy; timeline for mswlib/gtklib removal. Preference:
  no MSYS2 dependency; use official GTK-for-Windows runtime + vcpkg.
- **SF patches** in `.claude/patches/` — may be superseded by landed changes; verify before resubmitting.

---

## Completed

| Item | Date |
|------|------|
| bug-618: mswlib Windows linker fixes; 58 stubs; CI all-green both branches | 2026-05-11 |
| bug-619 filed — SF ticket for cmake `-P` wrappers | 2026-05-11 |
| bug-620 filed — SF ticket for CI improvements (ARM64, Clang) | 2026-05-11 |
| feature-216 Phase 1 — MCP server, 16 tools, 27 tests pass | 2026-04-xx |
| Regression test baseline — 51 checks pass on default branch | 2026-05-11 |
