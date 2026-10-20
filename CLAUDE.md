# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

**Since 2026-10-20 there is one development line.** Upstream (Martin Fischer) merged
`GTK3V2MAIN` into Hg `default` at r7748 ("Make GTK3 branch the default") and closed
`GTK3V2MAIN` at r7749. Git followed: `main` is the GTK3 line (PR #240), and the last GTK2 state of
`main` is the `gtk2-final` tag. GTK2 is no longer our concern, in CI or anywhere else (user,
2026-10-20). Hg `v5.3.1` is Martin's GTK2 hotfix branch. Anything below that still says
`GTK3V2MAIN` is history, not a branch to use.

## Session startup — SSH agent check

The SourceForge Hg remote (`hg.code.sf.net`) requires the passphrase-protected key
`~/.ssh/id_ed25519`. Non-interactive SSH (any `hg pull`/`push` run by Claude) cannot supply the
passphrase, so the key must already be loaded in the user's `ssh-agent`. Check at the start of
every session by comparing fingerprints — `ssh-add -l` prints each loaded key's *comment* field
(which may not be the filename; e.g. this key's comment is the user's email, not `id_ed25519`), so
grepping for the filename string gives a false "NOT LOADED":

```sh
ssh-add -l | grep -qF "$(ssh-keygen -lf ~/.ssh/id_ed25519.pub | awk '{print $2}')" || echo "NOT LOADED"
```

If not loaded, ask the user to run (via `!` so it's interactive and can prompt for the
passphrase): `ssh-add ~/.ssh/id_ed25519`. Don't attempt `hg pull`/`push` against the SF remote
until this succeeds — it will otherwise fail with `Permission denied (publickey,keyboard-interactive)`,
which looks like an SF-side credential problem but usually isn't (incident 2026-07-11).

## Session startup — upstream sync check

**At the start of every session**, do a live, read-only check against SF:

```sh
hg -R /home/abyrne/XTrkCAD/xtrkcad-hg incoming -b default \
    --template '{rev}:{node|short} {branch} {author|person}: {desc|firstline}\n'
```

(Exit code 1 with "no changes found" means nothing pending — that's success, not an error. Use
`-b BRANCH`, not a `-r "branch(...)"` revset — `hg incoming -r` resolves its argument against the
*remote* peer, which doesn't support revset function syntax and aborts with
`abort: unknown revision`; confirmed 2026-08-29.) Leave out `-b` to also see other contributors'
topic branches (e.g. Martin's `fix-…-gtk3`), which is useful context but not something to sync.

**`incoming` can't see a working copy that was pulled but never updated.** It only lists
changesets the repo doesn't have yet. Confirmed 2026-10-05 and again 2026-10-20 (`xtrkcad-hg` sat
on old GTK2 r7675 with 1368 pulled changesets pending). Also check the working copy is at the
`default` tip:

```sh
H=/home/abyrne/XTrkCAD/xtrkcad-hg
hg -R $H summary | head -3        # branch: must be default unless you're on a topic branch on purpose
[ "$(hg -R $H id -i -r default)" = "$(hg -R $H id -i)" ] || echo "WORKING COPY NOT AT default TIP (or has uncommitted changes)"
```

If it's behind and `hg -R $H status` is clean, `hg -R $H pull && hg -R $H update default`.

**Hg `default` → git `main` is synced on GitHub, not locally.**
`.github/workflows/hg-sync-default.yml` runs daily at 10:23 UTC. It clones Hg `default`
anonymously from SF's read-only URL and diffs from the last synced node, which is the newest
`Hg-Node:` commit trailer on `main` (PR #240's merge commit carries r7752). It applies the diff
to a `test/upstream-default-*` branch and runs `ci-gtk3.yml` there. If CI passes, it
fast-forwards `main`, runs `main`'s CI, and deletes the test branch together with its runs. If
the patch won't apply, CI fails, or `main` moved in the meantime, it opens a GitHub issue titled
`Hg sync (default): …` and leaves `main` alone. At session start, run
`gh issue list --repo adbyrne/XTrkCAD --search "Hg sync in:title"`. A `default` hit from the
`incoming` check above is something the workflow will pick up, not local work. Run it by hand
with `gh workflow run hg-sync-default.yml`; for testing, the inputs are `dry_run`, `promote`,
`from_node`, `to_node`, and `target_branch` (a scratch branch instead of `main`).

**The local `xtrkcad-sf-sync` timer is retired** (disabled 2026-10-20). It only handled
`GTK3V2MAIN`, which no longer exists. `.claude/sf-sync-pending` is no longer written; ignore it.

### When the sync workflow opens an issue

The usual cause is an upstream change colliding with a git-only file or hunk (astyle fixes, git
CI descriptions in the dev guide, held features). Handle it by hand:

```sh
hg -R xtrkcad-hg pull && hg -R xtrkcad-hg update default
hg -R xtrkcad-hg diff --git -r <last-synced-node> -r default \
    > .claude/patches/upstream-default-r<A>-r<B>.diff
git -C xtrkcad-git checkout -b test/upstream-default-YYYY-MM-DD origin/main
git -C xtrkcad-git apply --reject ../.claude/patches/upstream-default-r<A>-r<B>.diff
```

Use `--reject`, not `--3way`. Hg diffs don't carry git blob IDs, so `--3way` always fails with
"repository lacks the necessary blob" and falls back anyway. Resolve each `*.rej` hunk by hand,
keep any git-only change it collides with, and end the commit message with an
`Hg-Node: <full node>` trailer so the workflow resumes from there. Open the PR with
`--base main`.

**A clean Hg build doesn't prove the git branch builds.** Git can carry git-only code that an
upstream API change breaks even though Hg compiles fine. This happened 2026-10-05
(`PDO_DLGRESIZE` removed) and 2026-10-18 (`F_BLOCK` removed). Before pushing, build in a scratch
dir and run `tools/astyle-check.sh`, the same script as `ci-gtk3.yml`'s `astyle-check` gate:

```sh
cmake -B <scratch>/build-git -S xtrkcad-git -G Ninja -DCMAKE_BUILD_TYPE=Debug -DXTRKCAD_TESTING=ON
cmake --build <scratch>/build-git -- -k 0
ctest --test-dir <scratch>/build-git -LE regression
(cd xtrkcad-git && tools/astyle-check.sh)
```

### Verify the git tree actually matches Hg before declaring a sync done

**Never update `bug-tracker.md`'s "Hg Branches" table to `✅ rNNNN — synced` because a diff
applied cleanly.** A clean `git apply` only proves *that patch's* hunks landed. It says nothing
about files a *previous* sync failed to bring forward (2026-07-09 incident: git fell behind Hg for
weeks while the tracker claimed full sync). Run the verification script:

```sh
xtrkcad-verify-sync xtrkcad-hg default xtrkcad-git main          # the defaults; can be omitted
xtrkcad-verify-sync xtrkcad-hg "" xtrkcad-git feature-layergroups-jsonnote   # a feature branch
```

**Never pass bare `tip` as the Hg revision.** In a multi-branch Hg repo `tip` is the newest
changeset repo-wide, which can be another contributor's branch (`KensTest` inflated a 2026-08-14
check to 375 bogus differences). The script maps git `main` to Hg `default`, and any other git
ref to the Hg branch of the same name or that name plus `-gtk3` (Hg topic branches still carry
the suffix). It refuses to compare across a branch mismatch. It also diffs the **local** git ref,
so fast-forward it first (`git -C xtrkcad-git pull --ff-only`).

It archives both trees and diffs the **whole tree** (since 2026-10-19; `--app-only` gives the old
`app/`-only scope), labelling each entry `differs`, `hg-only` or `git-only`. Results are filtered
against `.claude/sync-exceptions-<git-ref>.txt` if it exists, otherwise
`.claude/sync-exceptions.txt` (used for `main` and feature branches; the old GTK2 list is
archived as `sync-exceptions-gtk2-final.txt.archived`). Entries are paths or bash patterns
(`.github/*`). The script also lists exceptions that matched nothing; remove those unless the
exceptions file's comment says the entry only applies to some refs. `PASS` means the git tree is
byte-for-byte identical to Hg except for documented, deliberate exceptions — only then is it safe
to write the `✅ rNNNN` line. On `FAIL`, reconcile the listed files, or, if the difference is
genuinely intentional and permanent, add it to the exceptions file with a reason comment.
Categorize file by file with `hg log -r "file('path')"` vs `git log -- path` (compare dates and
authors, not just presence of a diff) rather than assuming every FAIL entry needs action.

### Notes
- Test branches are for integration testing only — do **not** merge into `main` until CI is
  green and the changes are reviewed.
- **Merging into git `main` triggers `release.yml`**, which republishes the `latest-main`
  prerelease packages and redeploys GitHub Pages. PR CI never runs `release.yml`, so check that
  the post-merge Release run finishes green. Its packaging jobs use rolling OS packages that can
  break with no change on our side (2026-10-05: MSYS2 doxygen 1.18.0-3 crashed
  `package-windows`, fixed by pinning 1.16.1-4). When a PR changes `release.yml`, validate it
  before merging with `gh workflow run release.yml --ref <branch>`. Any workflow change also
  needs a matching dev-guide update (`docs/doxygen/building.md`, `advanced.md`).

---

## Project Layout (`/home/abyrne/XTrkCAD/`)

```
.claude/                   Development working files — plans, notes, patches, sync exceptions (not committed)
docs/                      Project documentation not required in any repo
xtrkcad-hg/                Hg working copy — default branch (GTK3) and our topic branches, remote = SourceForge
xtrkcad-git/               Git repo — main branch (GTK3), pushed to github.com/adbyrne/XTrkCAD
build/                     Out-of-source CMake build (source: xtrkcad-hg)
CLAUDE.md                  This file — also committed into xtrkcad-git (main)
```

Retired 2026-10-20, kept until the switch has settled (delete only with the user's OK):
`xtrkcad-hg-gtk3v2main/` (clone of the closed GTK3V2MAIN branch), `xtrkcad-git-gtk3/` (git
worktree of the frozen git `GTK3V2MAIN` branch) and `build-gtk3v2main/`.

## SourceForge / Mercurial remote

`xtrkcad-hg` pushes via SSH. The HTTP endpoint returns 400 errors and is not used.

- **SourceForge username**: `adbyrne1905`
- **SSH key**: `~/.ssh/id_ed25519` (registered at SF Account → SSH Settings)
- `xtrkcad-hg` remote: `ssh://adbyrne1905@hg.code.sf.net/p/xtrkcad-fork/xtrkcad`

Push sequence:
```sh
hg -R xtrkcad-hg pull                          # pick up upstream SF changes first
hg -R xtrkcad-hg merge && hg -R xtrkcad-hg commit -m "merge: ..."   # if needed
hg -R xtrkcad-hg push -b <branch>              # always scope with -b; bare push sends every local branch
```

## Hg development process (adopted 2026-10-05)

Follows Martin Fischer's *Development Process Specification* (dev guide page
`docs/doxygen/development-process.md`, `\page development-process`).

**Tracking — where an issue gets written down:**
- **SF "Development Issues" tracker** (`https://sourceforge.net/p/xtrkcad-fork/gtk3issues/`,
  REST mount point `gtk3issues`): **every new ticket we create goes here**, whether it's a
  feature, a bug we found, or a broad-impact fix. It's only visible to signed-in project members,
  so anonymous REST calls and the public tool list don't show it.
  `~/.config/xtrkcad-dev/sf-ticket.py` hardcodes `TRACKER = "bugs"`; override it when creating
  tickets (see the SF API memory).
- SF `bugs` tracker: **user-submitted bugs only**. Don't file our own tickets there.
- SF `feature-requests` tracker: user-submitted ideas, not ours.
- Problems found while developing a feature are fixed on its branch without a ticket. Review
  requests, and fixes we're unsure of, go to the **dev mailing list** (drafts in
  `.claude/devml-drafts/`).

**Branches** (`default` is the release branch for 5.4.0):
- **Localized fix** (one or a few places — missing init, small logic fix): commit directly on
  `default`, no branch.
- **Broad-impact fix** (wider implications, e.g. a FormAPI change): `fix-<NNN>-<desc>-gtk3`.
- **Feature**: `feature-<NNN>-<desc>-gtk3`, branched from `default`; every commit for the
  feature stays on it until it's verified complete.
- Housekeeping: `chore-<desc>-gtk3`.
- `<NNN>` is the Development Issues ticket number when one exists (file it first); leave it out
  otherwise. The `-gtk3` suffix stays: Martin still uses it after the switch
  (`fix-toolbar-button-reset-gtk3`, 2026-10-20). Git branch names drop it.
- Once verified, **we merge the branch into `default` ourselves, then close it**
  (`hg update <branch> && hg commit --close-branch -m "close <branch>"`), then push. Exception:
  if we're unsure of a fix, ask for review on the dev mailing list before merging.

**Unchanged:** git first — git PR, CI green, PR merged, and only then commit/merge in Hg and push
to SF (`hg push -b <branch>`). Holds the user sets explicitly (e.g. the Layer Groups / JSON Note
dev-ML review hold) stay in force until the user lifts them.

**Held feature (2026-10-20):** Hg `feature-layergroups-jsonnote-gtk3` (r7754 merges `default`
r7752; not pushed to SF yet) ↔ git `feature-layergroups-jsonnote`, draft PR #241. It isn't on
git `main`. Don't merge either side until upstream merges it in Hg.

## Build

Requires CMake ≥ 3.20, GTK+ 3.0 (`gtk3-devel`) and `rsvg-convert` (replaces Inkscape for
SVG→PNG; no D-Bus needed). Out-of-source builds are mandatory.

```sh
cmake -B build -S xtrkcad-hg -G Ninja -DCMAKE_BUILD_TYPE=Debug -DXTRKCAD_TESTING=ON -DXTRKCAD_APPEND_SRC_HASH=ON
cmake --build build
```

Useful CMake options:
- `-DXTRKCAD_TESTING=ON` — build unit tests (auto-enabled when CMocka is found)
- `-DXTRKCAD_USE_DOXYGEN=ON` — generate Doxygen internals docs
- `-DXTRKCAD_CREATE_SVG=1` — SVG export (requires MiniXML; found automatically)
- `-DXTRKCAD_APPEND_SRC_HASH=ON` — also give untagged dev builds' *filenames/install paths*
  (binary name, work/prefs dir, packages) their own short VCS hash, instead of each new dev
  build overwriting the previous one in place. (The internal/compiled-in version string always
  carries the hash regardless of this flag — see the Version section's "dev-build hash suffix"
  note.) Defaults OFF (matches the historic single-overwriting dev install) except under CI,
  where it defaults ON automatically. Always pass it explicitly for local builds, as shown above.

## Tests

Unit tests use [CMocka](https://cmocka.org/). Test sources: `app/bin/unittest/` and
`app/dynstring/unittest/`. Hg `default` has 75 tests, 17 of them outside the `regression` label
(r7752, `ctest -N`, 2026-10-20). The feature branch has 79: Layer Groups and JSON Note add 4.
The full run including the regression demo-playback suite takes ~10 minutes; run it in the
background.

```sh
ctest --test-dir build                       # everything, incl. regression
ctest --test-dir build -LE regression        # unit tests only
ctest --test-dir build -R pathstest          # run a single test by name
ctest --test-dir build --output-on-failure
```

`common.h` includes `dynarray.h` from `app/bin/include/` — any new test target that pulls in
`common.h` must add `${CMAKE_CURRENT_SOURCE_DIR}/../include` to its `target_include_directories`.

## Doxygen

```sh
cmake -B build -S xtrkcad-hg -DXTRKCAD_USE_DOXYGEN=ON
cmake --build build --target docs-doxygen   # note: NOT --target doc
# Output: build/docs/doxygen/html/
```

## Code Formatting

AStyle enforces style. Config: `app/lib/astylerc` (1TBS, tab=8, max 80 cols, C mode). In git,
run `tools/astyle-check.sh` from the repo root — the same script as CI's `astyle-check` gate,
with the same vendored-code exclusions.

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
- **Reports**: `reports.c/h` — the `Reports` menu (SF #217)

### `app/wlib/` — UI abstraction layer (`xtrkcad-wlib`)

Platform UI toolkit wrapper. The only backend is `gtk3lib/` (GTK+ 3) on Linux, macOS and
Windows alike; the old Win32 backend `mswlib/` has been removed. All application code calls
`wlib` APIs — never GTK directly.

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

(`cnvdsgn.c` lives in `app/bin/`, not here. `bdf2xtp.c` was removed as orphaned dead code —
SF #735.)

### `mcp/` — MCP server (git only)

Python package for layout analysis and report generation; CI in `ci-mcp.yml`. Not in Hg.

## Version

Current version: **5.4.0** (`ProgramVersion.cmake`, no modifier). To cut a release:
`git tag v5.4.0 && git push origin v5.4.0` — GitHub Actions (`release.yml`) automatically builds
packages and creates a draft GitHub Release. Every push to `main` also refreshes the rolling
`latest-main` prerelease.

**Dev-build hash suffix:** `ProgramVersion.cmake` (SF #712) produces two version strings on every
build that is *not* the exact commit a `v<version>` tag points at. Detected automatically at
configure time from either a git or an Hg checkout. A tagged release build drops the suffix from
both and keeps the plain three-part version.

- `XTRKCAD_VERSION` — always gets a fourth, dot-separated short-VCS-hash component (e.g.
  `5.4.0.8b67f185`). Compiled into the binary (`xtrkcad-config.h`'s `XTRKCAD_VERSION` macro) and
  used for internal, non-filesystem values: the About/`--version` string and misc.c's
  stored-preference "is this a new build since last run" check.
- `XTRKCAD_FILE_VERSION` — only gets the hash suffix when the `XTRKCAD_APPEND_SRC_HASH` CMake
  option is on. Used everywhere the version becomes part of a filename or path: `XTRKCAD_BIN`
  (installed binary name), the runtime work/prefs directory (`custom.c`'s `sProdNameLower`), and
  every CPack/packaging filename, install directory, and bundle-identity field under
  `distribution/`.

The option defaults OFF locally (a local build's filename/install path stays fixed across
commits, and each new build overwrites the last) and ON under CI (GitHub Actions' built-in `CI`
env var), so concurrent jobs and artifacts never collide. After tagging a release, bump
`XTRKCAD_RELEASE_VERSION` (and `XTRKCAD_VERSION_MODIFIER`, if used) for the next dev cycle.

**To build one specific past commit:** `git checkout <hash>` (or `hg update -r <hash>`) in the
source tree, then explicitly reconfigure — `cmake -B build -S xtrkcad-hg` — before
`cmake --build`. CMake only re-runs its configure step when a file it tracks (`CMakeLists.txt`,
`ProgramVersion.cmake`) changed on disk, so a `cmake --build`-only rebuild after switching
commits silently keeps the previous commit's hash.

## File Formats

- `.xtc` / `.xtce` — native layout files (`.xtce` adds background-image support, v5.2+)
- `.xtp` — parameter files (turnout and equipment definitions)
- `.xtr` — demo/replay scripts
