# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Session startup — SSH agent check

The SourceForge Hg remote (`hg.code.sf.net`) requires the passphrase-protected key
`~/.ssh/id_ed25519`. Non-interactive SSH (the weekly sync timer, or any `hg pull`/`push` run by
Claude) cannot supply the passphrase, so the key must already be loaded in the user's
`ssh-agent`. Check at the start of every session:

```sh
ssh-add -l | grep -q id_ed25519 || echo "NOT LOADED"
```

If not loaded, ask the user to run (via `!` so it's interactive and can prompt for the
passphrase): `ssh-add ~/.ssh/id_ed25519`. Don't attempt `hg pull`/`push` against the SF remote
until this succeeds — it will otherwise fail with `Permission denied (publickey,keyboard-interactive)`,
which looks like an SF-side credential problem but usually isn't (see incident 2026-07-11 below).

## Session startup — upstream sync check

**At the start of every session**, check for the pending upstream-sync notification:

```sh
cat /home/abyrne/XTrkCAD/.claude/sf-sync-pending   # non-empty = upstream changes waiting
```

If the file is non-empty, tell the user which branches have new SF changes (the file lists the
git test-branch names) and ask: **"Upstream SF changes are pending — want me to compile and
test them locally, then push to GitHub CI?"**

If yes, run the following workflow:

### 1 — Local compile (Linux GTK3)

The Hg working copies are already updated by the weekly sync script. Just rebuild:

```sh
cmake --build build-gtk3v2main 2>&1 | tee /tmp/build-gtk3v2main.log
ctest --test-dir build-gtk3v2main --output-on-failure
```

For default-branch changes (rare) use `build/` instead.

### 2 — Fix any compile errors

- Edit files in `xtrkcad-hg-gtk3v2main/` (the Hg source for GTK3V2MAIN).
- Rebuild until clean and all tests pass.
- Keep fixes minimal — only what's needed to compile against the upstream changes.

### 3 — Carry fixes into the git test branch and push

The sync script already pushed a `test/upstream-{branch}-YYYY-MM-DD` branch to GitHub with the
raw upstream diff applied. If the local build needed fixes, apply them to that branch too:

```sh
# Get a diff of what was fixed in Hg
hg -R xtrkcad-hg-gtk3v2main diff > /tmp/local-fixes.diff

# Apply to the git test branch
BRANCH=$(grep -oP "test/upstream-\S+" /home/abyrne/XTrkCAD/.claude/sf-sync-pending | head -1)
git -C xtrkcad-git checkout "$BRANCH"
git -C xtrkcad-git apply /tmp/local-fixes.diff
git -C xtrkcad-git add -u
git -C xtrkcad-git commit -m "fix: local compile fixes for upstream SF sync"
git -C xtrkcad-git push
```

If the build was already clean (no fixes needed), skip step 3 — CI is already running on the
branch that the sync script pushed.

### 4 — Verify the git tree actually matches Hg before declaring the sync done

**Never update `bug-tracker.md`'s "Hg Branches" table to `✅ rNNNN — synced` based on the diff
having applied cleanly.** A clean `git apply` only proves *that patch's* hunks landed — it says
nothing about files a *previous* sync silently failed to bring forward. That gap is exactly what
caused the 2026-07-09 incident: git's GTK3V2MAIN quietly fell behind Hg in the gtk3lib
control-struct/basic-draw family and several `app/bin` files for weeks while the tracker kept
claiming full sync.

Run the verification script instead:

```sh
xtrkcad-verify-sync xtrkcad-hg-gtk3v2main GTK3V2MAIN xtrkcad-git-gtk3 GTK3V2MAIN
# for the default/GTK2 branch: xtrkcad-verify-sync xtrkcad-hg default xtrkcad-git main
```

**Always pass the Hg branch name explicitly (as shown above), never bare `tip`.** In a
multi-branch Hg repo, `tip` means the single most-recently-committed revision *repo-wide*, not
"the tip of the branch you're comparing" — if another contributor's unrelated branch (their own
feature work, e.g. `KensTest`/`newtrainrun`) happens to be more recently committed, `tip` silently
resolves there instead, producing a huge bogus diff that looks like real drift but isn't (this is
exactly what inflated the 2026-08-14 `default`-branch check to "375 unexpected differences" — the
real number was 2). The script now refuses to compare across a branch mismatch and exits with an
error explaining the fix, but avoid tripping it in the first place by never using `tip`.

It archives both trees and diffs every file under `app/`, filtered against the maintained
allowlist at `.claude/sync-exceptions.txt`. `PASS` means the git tree is byte-for-byte identical
to Hg except for documented, deliberate exceptions — only then is it safe to write the `✅ rNNNN`
line. On `FAIL`, either reconcile the listed files or, if the difference is genuinely intentional
(e.g. a feature port that's blocked, or local test coverage never contributed upstream), add it
to `sync-exceptions.txt` with a reason comment — don't silently ignore the failure.

### 5 — Clear the pending file

```sh
rm /home/abyrne/XTrkCAD/.claude/sf-sync-pending
```

### Notes
- The test branch is for integration testing only — do **not** merge it into `main`/`GTK3V2MAIN`
  until CI is green and the changes are reviewed.
- If the patch required manual conflict resolution (the sync script saved a `.diff` to
  `.claude/patches/` instead of creating a branch), handle that first before compiling, and run
  step 4's verification afterward — this is exactly the case that most often leaves files behind.

---

## Project Layout (`/home/abyrne/XTrkCAD/`)

```
.claude/                   Development working files — plans, notes, sf-sync-pending (not committed)
docs/                      Project documentation not required in any repo
xtrkcad-hg/                Hg working copy — default branch, remote = SourceForge
xtrkcad-hg-gtk3v2main/     Hg working copy — GTK3V2MAIN branch, cloned from xtrkcad-hg
xtrkcad-git/               Git repo — main branch (GTK2), pushed to github.com/adbyrne/XTrkCAD
xtrkcad-git-gtk3/          Git worktree — GTK3V2MAIN branch (same .git as xtrkcad-git)
build/                     Out-of-source CMake build for default branch (source: xtrkcad-hg)
build-gtk3v2main/          Out-of-source CMake build for GTK3V2MAIN branch (source: xtrkcad-hg-gtk3v2main)
CLAUDE.md                  This file — committed into xtrkcad-git (main) and xtrkcad-git-gtk3 (GTK3V2MAIN)
```

## SourceForge / Mercurial remote

Both Hg repos push via SSH. The HTTP endpoint returns 400 errors and is not used.

- **SourceForge username**: `adbyrne1905`
- **SSH key**: `~/.ssh/id_ed25519` (registered at SF Account → SSH Settings)
- `xtrkcad-hg` remote: `ssh://adbyrne1905@hg.code.sf.net/p/xtrkcad-fork/xtrkcad`
- `xtrkcad-hg-gtk3v2main` remote: `xtrkcad-hg` (local) — push there first, then push `xtrkcad-hg` to SF

Push sequence:
```sh
hg -R xtrkcad-hg-gtk3v2main push          # GTK3V2MAIN → local default repo
hg -R xtrkcad-hg pull                     # pull any upstream SF changes
hg -R xtrkcad-hg merge && hg -R xtrkcad-hg commit -m "merge: ..."   # if needed
hg -R xtrkcad-hg push                     # default → SourceForge
```

## Build

Requires CMake ≥ 3.20 and `rsvg-convert` (replaces Inkscape for SVG→PNG; no D-Bus needed). Out-of-source builds are mandatory.

- **default branch** requires GTK+ 2.0 (`gtk2-devel`)
- **GTK3V2MAIN branch** requires GTK+ 3.0 (`gtk3-devel`)

```sh
# default branch (GTK2)
cmake -B build -S xtrkcad-hg -G Ninja -DCMAKE_BUILD_TYPE=Debug -DXTRKCAD_TESTING=ON
cmake --build build

# GTK3V2MAIN branch (GTK3)
cmake -B build-gtk3v2main -S xtrkcad-hg-gtk3v2main -G Ninja -DCMAKE_BUILD_TYPE=Debug -DXTRKCAD_TESTING=ON
cmake --build build-gtk3v2main
```

Useful CMake options:
- `-DXTRKCAD_TESTING=ON` — build unit tests (auto-enabled when CMocka is found)
- `-DXTRKCAD_USE_DOXYGEN=ON` — generate Doxygen internals docs
- `-DXTRKCAD_CREATE_SVG=1` — SVG export (requires MiniXML; found automatically)

## Tests

Unit tests use [CMocka](https://cmocka.org/). Test sources: `app/bin/unittest/` and `app/dynstring/unittest/`.

- **default branch**: 8 tests pass (`ctest --test-dir build`)
- **GTK3V2MAIN branch**: 9 tests pass (adds `PreferenceTest` from GTK3 wlib)

```sh
ctest --test-dir build                  # default branch
ctest --test-dir build-gtk3v2main       # GTK3V2MAIN branch
ctest --test-dir build -R pathstest     # run a single test by name
ctest --test-dir build --output-on-failure
```

**GTK3V2MAIN-specific note**: `common.h` on that branch includes `dynarray.h` from
`app/bin/include/` — any new test target that pulls in `common.h` must add
`${CMAKE_CURRENT_SOURCE_DIR}/../include` to its `target_include_directories`.

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

Current development version: **5.3.2Dev** (`ProgramVersion.cmake`) on `default`/`main`
(GTK2). Binary named `xtrkcad-beta` when `XTRKCAD_VERSION_MODIFIER` matches `^Beta`,
`xtrkcad` otherwise.

**GTK3V2MAIN branch only:** version is **5.4.0** (GTK3 successor release; no modifier),
tracked independently from the GTK2 default branch's 5.3.x line. To cut a release:
`git tag v5.4.0 && git push origin v5.4.0` — GitHub Actions (`release.yml`) automatically
builds packages and creates a draft GitHub Release.

## File Formats

- `.xtc` / `.xtce` — native layout files (`.xtce` adds background-image support, v5.2+)
- `.xtp` — parameter files (turnout and equipment definitions)
- `.xtr` — demo/replay scripts
