# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Session startup — SSH agent check

The SourceForge Hg remote (`hg.code.sf.net`) requires the passphrase-protected key
`~/.ssh/id_ed25519`. Non-interactive SSH (the weekly sync timer, or any `hg pull`/`push` run by
Claude) cannot supply the passphrase, so the key must already be loaded in the user's
`ssh-agent`. Check at the start of every session by comparing fingerprints — `ssh-add -l` prints
each loaded key's *comment* field (which may not be the filename; e.g. this key's comment is the
user's email, not `id_ed25519`), so grepping for the filename string gives a false "NOT LOADED":

```sh
ssh-add -l | grep -qF "$(ssh-keygen -lf ~/.ssh/id_ed25519.pub | awk '{print $2}')" || echo "NOT LOADED"
```

If not loaded, ask the user to run (via `!` so it's interactive and can prompt for the
passphrase): `ssh-add ~/.ssh/id_ed25519`. Don't attempt `hg pull`/`push` against the SF remote
until this succeeds — it will otherwise fail with `Permission denied (publickey,keyboard-interactive)`,
which looks like an SF-side credential problem but usually isn't (see incident 2026-07-11 below).

## Session startup — upstream sync check

**At the start of every session**, do a live, read-only check against SF — don't just trust the
`sf-sync-pending` file (see below for why). This costs a few seconds and never mutates anything:

```sh
hg -R /home/abyrne/XTrkCAD/xtrkcad-hg incoming -b GTK3V2MAIN -b default \
    --template '{rev}:{node|short} {branch} {author|person}: {desc|firstline}\n'
```

(Exit code 1 with "no changes found" means nothing pending — that's success, not an error. Use
`-b BRANCH` flags, not a `-r "branch(...) or branch(...)"` revset — `hg incoming -r` resolves its
argument against the *remote* peer, which doesn't support revset function syntax and aborts with
`abort: unknown revision`; confirmed 2026-08-29.) Also
still check the pending file, since it can carry a manual-merge note the live check above won't
show (e.g. a patch the sync script couldn't auto-apply, saved under `.claude/patches/`):

```sh
cat /home/abyrne/XTrkCAD/.claude/sf-sync-pending   # non-empty = a prior run left something for you
```

**Why both, not just the pending file:** the weekly `xtrkcad-sf-sync` timer (Monday 08:00) is the
only thing that writes that file, and it has repeatedly failed silently between runs — 24
`hg pull` failures logged in `.claude/sf-sync.log` as of 2026-08-29, almost all
`Could not resolve hostname hg.code.sf.net` (DNS not up yet at boot, despite
`network-online.target`). A failed run doesn't retry until the following Monday and doesn't touch
the pending file, so a whole week of upstream commits (including straight-to-`GTK3V2MAIN`
merges from other contributors, e.g. Martin Fischer) can land invisibly — confirmed 2026-08-29
when a live check found six unflagged `GTK3V2MAIN` merge/topic-branch commits (SF #719, #721,
#729/#730) that the pending file said nothing about. The live check above is the fix: it asks SF
directly every session instead of trusting a periodic job's last output.

**Also check the second hop — the GTK3V2MAIN working copy vs. its own local mirror.**
`xtrkcad-hg-gtk3v2main` (where GTK3V2MAIN edits actually happen) pulls from `xtrkcad-hg` locally,
not from SF directly, so the SF-vs-`xtrkcad-hg` check above can come back completely clean while
`xtrkcad-hg-gtk3v2main` is still behind `xtrkcad-hg` itself. Confirmed 2026-09-03: `xtrkcad-hg`
was byte-for-byte synced with SF (a merge had already landed in both), but
`xtrkcad-hg-gtk3v2main` was 2 changesets behind `xtrkcad-hg` — this read as "SF is broken/has an
unreconciled conflict" until traced back to a stale working copy. Check every session,
unconditionally (not just when the SF check above finds something):

```sh
hg -R /home/abyrne/XTrkCAD/xtrkcad-hg-gtk3v2main incoming -b GTK3V2MAIN \
    --template '{rev}:{node|short} {branch} {author|person}: {desc|firstline}\n'
```

If it reports anything, just pull and update immediately — no need to ask first. This is a
same-machine, local-to-local sync of content that's already vetted (either it's the user's own
prior work pushed up from this same working copy, or upstream SF content the user already
approved pulling into `xtrkcad-hg` via the check above), so there's nothing new here to review:

```sh
hg -R /home/abyrne/XTrkCAD/xtrkcad-hg-gtk3v2main status   # must be clean before pulling
hg -R /home/abyrne/XTrkCAD/xtrkcad-hg-gtk3v2main pull
hg -R /home/abyrne/XTrkCAD/xtrkcad-hg-gtk3v2main update GTK3V2MAIN
```

If `status` isn't clean, stop and reconcile the in-progress work first rather than pulling on top
of it.

If either of the two checks above (SF vs. `xtrkcad-hg`, pending file) surfaces something, tell the user which branches have new SF changes and ask:
**"Upstream SF changes are pending — want me to compile and test them locally, then push to
GitHub CI?"**

If yes, run the following workflow (note step 0, needed only when the live check above found
things the pending file didn't already know about):

### 0 — Pull if the live check found changes the pending file didn't

```sh
hg -R xtrkcad-hg pull
hg -R xtrkcad-hg-gtk3v2main pull
```

Then continue at step 1. (If `sf-sync-pending` already covered it — the normal case — the Hg
working copies are already up to date and this step is a no-op.)

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

**A large FAIL isn't necessarily a new problem.** Files belonging to git-merged-but-not-yet-Hg-merged
`needs-review` tickets (bug-tracker.md's SF Bugs table — e.g. #732/#733/#734 as of 2026-08-29) will
show up here every time until each clears its own review window; that's expected, temporary drift,
not something to add to `sync-exceptions.txt` (which is for *permanent* deliberate divergences).
Before reconciling or exception-listing anything, check whether the file is already explained by an
open `needs-review` row — confirmed useful 2026-08-29 when a 67-file FAIL turned out to be ~44
already-tracked pending-review files + 14 already-decided-against glade retirement + only 2
genuinely new gaps (Ken Shaffer's flatpak/doc-branding work, untouched by git since Aug 10/25).
Categorize file-by-file with `hg log -r "file('path')"` vs `git log -- path` (compare dates/authors,
not just presence of a diff) rather than assuming every FAIL entry needs action.

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
cmake -B build-gtk3v2main -S xtrkcad-hg-gtk3v2main -G Ninja -DCMAKE_BUILD_TYPE=Debug -DXTRKCAD_TESTING=ON -DXTRKCAD_APPEND_SRC_HASH=ON
cmake --build build-gtk3v2main
```

Useful CMake options:
- `-DXTRKCAD_TESTING=ON` — build unit tests (auto-enabled when CMocka is found)
- `-DXTRKCAD_USE_DOXYGEN=ON` — generate Doxygen internals docs
- `-DXTRKCAD_CREATE_SVG=1` — SVG export (requires MiniXML; found automatically)
- `-DXTRKCAD_APPEND_SRC_HASH=ON` — GTK3V2MAIN only: also give untagged dev builds'
  *filenames/install paths* (binary name, work/prefs dir, packages) their own short
  VCS hash, instead of each new dev build overwriting the previous one in place.
  (The internal/compiled-in version string always carries the hash regardless of
  this flag — see the Version section's "dev-build hash suffix" note.) Defaults
  OFF (matches the historic single-overwriting dev install) except under CI,
  where it defaults ON automatically. Always pass it explicitly for local
  GTK3V2MAIN builds, as shown above.

## Tests

Unit tests use [CMocka](https://cmocka.org/). Test sources: `app/bin/unittest/` and `app/dynstring/unittest/`.

- **default branch**: 9 tests pass (`ctest --test-dir build`) — corrected 2026-08-29, was
  documented as 8 (a stale count; verified via `ctest -N`, includes `DXFOutputTest`)
- **GTK3V2MAIN branch**: 64 tests pass (`ctest --test-dir build-gtk3v2main`) — corrected
  2026-08-29, was badly stale at "9 tests (adds `PreferenceTest`)"; the suite grew substantially
  since that note was written (`RegressionSuite`, `HelpLinksCheck`, `CarsTest`/`CarRegistryTest`/
  `CarEditLogicTest`, etc. — verified via repeated `ctest --output-on-failure` runs this session)

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
- `listxtp.c` — creates a contents list of all parameter files
- `dirent.c`/`dirent.h` — vendored public-domain Win32 POSIX-`dirent` shim (Kevlin Henney,
  1997/2003), Windows-only build of `listxtp`
- `mkimage1`/`mkimage3` — shell scripts converting `.png` bitmaps to `.image1`/`.image3`

(`cnvdsgn.c` and `bdf2xtp.c` actually live in `app/bin/`, not here — corrected 2026-08-28;
`pngtoxpm` doesn't exist anywhere in the current tree.)

## Version

Current development version: **5.3.2Dev** (`ProgramVersion.cmake`) on `default`/`main`
(GTK2). Binary named `xtrkcad-beta` when `XTRKCAD_VERSION_MODIFIER` matches `^Beta`,
`xtrkcad` otherwise.

**GTK3V2MAIN branch only:** version is **5.4.0** (GTK3 successor release; no modifier),
tracked independently from the GTK2 default branch's 5.3.x line. To cut a release:
`git tag v5.4.0 && git push origin v5.4.0` — GitHub Actions (`release.yml`) automatically
builds packages and creates a draft GitHub Release.

**GTK3V2MAIN dev-build hash suffix:** `ProgramVersion.cmake` (SF #712) produces two version
strings on every build that is *not* the exact commit a `v<version>` tag points at. Detected
automatically at configure time from either a git or an Hg checkout (this file's own build
applies either way). A tagged release build drops the suffix from both and keeps the plain
three-part version — there is no distinction between the two on a release.

- `XTRKCAD_VERSION` — always gets a fourth, dot-separated short-VCS-hash component (e.g.
  `5.4.0.8b67f185`). Compiled into the binary (`xtrkcad-config.h`'s `XTRKCAD_VERSION` macro) and
  used for internal, non-filesystem values: the About/`--version` string and misc.c's
  stored-preference "is this a new build since last run" check.
- `XTRKCAD_FILE_VERSION` — only gets the hash suffix when the `XTRKCAD_APPEND_SRC_HASH` CMake
  option (see Build section above) is on. Used everywhere the version becomes part of a
  filename or path: `XTRKCAD_BIN` (installed binary name — this is what disambiguates successive
  dev builds of the same nominal version installed side by side, on top of #673's per-version
  package/binary naming), the runtime work/prefs directory (`custom.c`'s `sProdNameLower`), and
  every CPack/packaging filename, install directory, and bundle-identity field under
  `distribution/`.

The option surprised a developer expecting every historic dev build to overwrite the same
plain-named install rather than accumulate new ones — hence the option defaulting OFF (so a
local build's filename/install path stays fixed across commits, and each new build just
overwrites the last, matching that historic expectation) except under CI (GitHub Actions'
built-in `CI` env var), where it defaults ON so concurrent jobs and artifacts never collide.
Always pass `-DXTRKCAD_APPEND_SRC_HASH=ON` for a local GTK3V2MAIN dev build that should also get
disambiguated filenames/installs (see Build section above for the flag). After tagging a
release, bump `XTRKCAD_RELEASE_VERSION` (and `XTRKCAD_VERSION_MODIFIER`, if used) for the next
dev cycle — every subsequent untagged build picks up the hash again automatically (in
`XTRKCAD_VERSION` always, in `XTRKCAD_FILE_VERSION` only when the option is on), no further
manual step needed.

**To build one specific past commit:** `git checkout <hash>` (or `hg update -r <hash>`) in the
source tree, then explicitly reconfigure — `cmake -B build-gtk3v2main -S xtrkcad-hg-gtk3v2main`
— before `cmake --build`. Don't rely on `cmake --build` alone to notice the checkout: CMake only
re-runs its configure step automatically when a file it tracks (`CMakeLists.txt`,
`ProgramVersion.cmake`) actually changed on disk, so switching commits that don't touch those
files leaves the *previous* commit's hash silently baked into both `XTRKCAD_VERSION` and
`XTRKCAD_FILE_VERSION` (confirmed empirically: a `cmake --build`-only rebuild reused the stale
hash from the prior configure). Add `-DXTRKCAD_APPEND_SRC_HASH=ON` too if this build should get
its own filename/install path instead of overwriting your regular dev build.

## File Formats

- `.xtc` / `.xtce` — native layout files (`.xtce` adds background-image support, v5.2+)
- `.xtp` — parameter files (turnout and equipment definitions)
- `.xtr` — demo/replay scripts
