\page advanced Advanced Topics

Material for contributors doing ongoing work in this repo — setting up your own CI, and the
process/lessons behind the static-analysis triage sessions — rather than a first read. See the
\ref index "Developer Documentation" page for everything else (source layout, coding conventions,
building and testing, debug logging).

\tableofcontents

# Running CI on your own GitHub fork {#running-ci-on-your-own-github-fork}

SourceForge Mercurial is this project's canonical source — GitHub only exists here so pushes can
run the workflows in `.github/workflows/` as free, automatic CI before a change is sent upstream
as an Hg patch. There's no single shared XTrkCAD repo on GitHub for that; each developer points
Actions at a plain, empty repo they create under their own account. Throughout this guide (and in
`.github/README.md`), `$GITHUB_USER` stands for whichever GitHub username you're using this way —
substitute your own everywhere you see it, or just `export GITHUB_USER=yourname` so the commands
below work as written.

The two repositories name the release line differently: Hg `default` and git `%main` are the same
branch. This page describes the GitHub workflows, so it says `%main`; where it refers to Hg, it
says `default`.

1. Create a GitHub account if you don't already have one.
2. Create a new repository under your account — with the [`gh` CLI](https://cli.github.com/):
   ```sh
   gh repo create "$GITHUB_USER/XTrkCAD" --public --source=. --remote=github
   ```
   or via the web UI, then add the remote yourself:
   ```sh
   git remote add github "https://github.com/$GITHUB_USER/XTrkCAD.git"
   ```
3. Push a branch:
   ```sh
   git push github your-branch-name
   ```
   Actions is on by default for repos you own, so `.github/workflows/*.yml` starts running as
   soon as the push lands — no separate enable step.
4. Watch the run at `https://github.com/$GITHUB_USER/XTrkCAD/actions`, or with
   `gh run watch` if you have the `gh` CLI authenticated.

`release.yml`'s `package-docs` and release-packaging jobs only run automatically on pushes to
`%main` and on `v*.*.*` release tags (see that workflow's own `on:` block). They won't fire on
an arbitrary feature branch on your fork. That's expected: those jobs build release artifacts for
the project maintainer's own release process, not per-developer CI, so there's nothing to set up
for them here.

Pull-request CI never runs `release.yml`, so a change to that workflow, or to anything only its
packaging jobs exercise, isn't tested until it has already merged. To test it first, start the
workflow by hand on your branch:

```sh
gh workflow run release.yml --ref your-branch-name
```

This runs every packaging, install-test and report job against your branch without publishing
anything. Its inputs choose the platforms, reuse an earlier run's packages, or publish the
packages to a temporary prerelease named after the branch for someone to review; see
\ref release-workflow "release.yml".

# CI tooling overview {#ci-tooling-overview}

## Workflows at a glance {#ci-workflows-at-a-glance}

| Workflow | Runs on | Fails the run? |
|---|---|---|
| `ci-gtk3.yml` | push to `%main` or `test/**`, PR into `%main`, dispatch | yes: the gate |
| `ci-gtk3-preview.yml` | push to `%main` or `test/**`, PR into `%main` | no, advisory |
| `ci-windows-msvc.yml` | push to `%main` or `test/**`, PR into `%main`, dispatch | no, advisory while MSVC is experimental |
| `ci-mcp.yml` | the same, only when `mcp/` or the workflow changed | yes, for `mcp/` |
| `codeql.yml` | push to `%main`, PR into `%main`, weekly (Monday) | yes |
| `release.yml` | push to `%main`, `v*.*.*` tag, dispatch | see \ref release-workflow "release.yml" |
| `install-tests-only.yml` | an install-test script changes (PR into `%main`, `test/**`), dispatch | yes |
| `hg-sync-default.yml` | daily 10:23 UTC, dispatch | opens an issue instead |
| `gtk3-toolchain.yml`, `gtk3-qa-reports.yml`, `packages-linux.yml`, `packages-macos.yml`, `packages-windows.yml`, `install-tests.yml` | only when another workflow calls them | as the caller's jobs |

A push to any other branch runs nothing, and neither does a PR into a branch other than `%main`,
e.g. a feature branch's PR into another feature branch. Run CI on such a branch by hand:
`gh workflow run ci-gtk3.yml --ref <branch>`. A `test/**` branch gets the full PR set without a
PR.

The main CI is `.github/workflows/ci-gtk3.yml` (`ci-gtk3-preview.yml`,
`codeql.yml`, `release.yml` and `hg-sync-default.yml` are separate workflows — see below). Read the workflow file
directly for exact command lines; this is a map of what each job checks and whether it can fail
your run, not a copy of it.

No job definition is kept as a hand-made copy of another, because copies drifted more than once.
The shared pieces:

- **`gtk3-toolchain.yml`** (a reusable workflow) holds the toolchain-sensitive build/test jobs:
  `c-tests-linux`, `c-tests-linux-clang`, `c-tests-arm64`, `regression-gtk3`,
  `c-tests-sanitizers`, `cppcheck`, `c-tests-macos` and `c-tests-windows`. `ci-gtk3.yml` calls it
  with the pinned runners and `ci-gtk3-preview.yml` with the next ones.
- **`gtk3-qa-reports.yml`** (a reusable workflow) holds the report-only analysis jobs. `ci-gtk3.yml`
  calls it for plain-text PR artifacts, `release.yml` for the published HTML reports, and
  `ci-gtk3-preview.yml` on the next Ubuntu (Linux only: `other-platforms: false` leaves out the
  macOS and Windows warnings jobs).
- **`packages-linux.yml`, `packages-macos.yml`, `packages-windows.yml`** and
  **`install-tests.yml`** (reusable workflows) hold the packaging and install-test jobs that
  `release.yml` calls; `install-tests-only.yml` reuses `install-tests.yml`. See
  \ref release-workflow "release.yml".
- **`.github/actions/`** holds composite actions for repeated steps: `apt-deps` (the Linux package
  list), `select-xcode`, `msys2-pin-gcc`, `setup-doxygen` and `setup-cmake`. Each pinned version
  is set once, as the action's default.
- **`tools/cppcheck-gate.sh`** and **`tools/astyle-check.sh`** hold the two gate commands. Run
  them locally from the repository root.

A job from a reusable workflow shows in the checks list as `<caller job> / <job name>`, e.g.
`toolchain / c-tests-linux (ubuntu-24.04)` or `qa / clang-tidy`.

On `%main`, `hg-sync-default.yml` also mirrors SourceForge Hg `default` into git `%main` every
day. It diffs from the last synced Hg node, which is recorded as an `Hg-Node:` commit trailer,
or from a later `default` changeset when `%main` already has every change up to it. Work that
lands in git first reaches Hg only after its pull request merged, so no trailer can name its
changeset; for each file changed since the trailer, the workflow accepts the newer start if the
file matches Hg there or if Hg's diff for it reverse-applies to `%main`. It applies the result to a `test/upstream-default-*` branch. It runs `ci-gtk3.yml` there and
fast-forwards `%main` only if CI passes; a conflict or failure opens a GitHub issue instead.
It applies the diff file by file. Changes usually land in git first, so a file that already
matches Hg (its content on `%main` equals Hg's at the synced changeset) is skipped and listed in
the commit message rather than patched; a file that doesn't apply is reported as a conflict.
Its pushes use `GITHUB_TOKEN`, which never triggers push-event workflows, so it starts
`ci-gtk3.yml` through `workflow_dispatch`.

## Gating jobs (a failure here fails the run)

- **`doxygen`** — builds this documentation with the pinned Doxygen 1.15.0 and `WARN_AS_ERROR`;
  any warning (a broken `\ref`, a stale `@param`, an unresolved anchor) fails the job.
- **`c-tests-linux`, `c-tests-arm64`, `c-tests-debian`, `c-tests-linux-clang`,
  `c-tests-macos`, `c-tests-windows`, `c-tests-sanitizers`, `c-tests-valgrind`** — build
  plus `ctest` across compilers (GCC, Clang, AppleClang, MinGW GCC), platforms, and architectures, plus ASan/UBSan and
  Valgrind runs (`PreferenceTest` and `MRUListTest` excluded from both — see
  \ref advanced-optional-local-checks "Advanced/optional local checks" in the
  \ref index "Developer Documentation" page for why). All pass `-LE regression` so the demo
  regression suite (see `regression-gtk3` below) isn't picked up here too. `c-tests-debian` runs
  in `debian:bookworm` and `debian:trixie` containers on amd64 and arm64. The arm64 entries stand
  in for Raspberry Pi OS on a Pi 3/4/5, and Bookworm's older GCC 12, CMake 3.25 and GTK 3.24.38
  catch code that only builds on newer toolchains.
- **`cmake-presets`, `cmake-presets-macos`, `cmake-presets-windows`** — run each platform's
  workflow preset from `CMakePresets.json` (configure, build, unit tests): `linux-debug`,
  `macos-debug` and `windows-debug`. The Linux job uses CMake 4.0.7, installed by `setup-cmake`,
  because the presets require CMake 4.0 or newer and pinning the oldest supported CMake means the
  file can't quietly start depending on a newer one. The macOS and Windows jobs use Homebrew's and
  MSYS2's CMake, which roll and are already 4.x. See \ref cmake-presets "CMake presets".
- **`regression-gtk3`** — replays the full demo suite (`RegressionTestAll()`, the `-T` flag) in
  one process via the `RegressionSuite` `ctest` target (see
  \ref advanced-optional-local-checks "Advanced/optional local checks" in the
  \ref index "Developer Documentation" page for the full `XTRKCAD_REGRESSION_TESTING` writeup).
  Kept out of `c-tests-linux` rather than folded in, since its ~9-minute single-process playback
  baseline would slow down that job's fast unit-test feedback loop.
- **`regression-gtk3-sanitizers`** — the same `RegressionSuite` run as `regression-gtk3`, but
  ASan/UBSan-instrumented (mirrors `c-tests-sanitizers`' configure line). Closes the SF #682 gap:
  `c-tests-sanitizers` excludes regression (`-LE regression`) and `regression-gtk3` isn't
  sanitizer-instrumented, so the combination that found SF #675/#677–#680 previously only happened
  when someone ran it locally — see the "Static tools and a live sanitizer run" bullet below.
- **`cppcheck`** — cppcheck's default check set only, `--error-exitcode=1`
  (`tools/cppcheck-gate.sh`).
- **`astyle-check`** — AStyle 3.6.13 (built from source, pinned — an unpinned version once
  produced different formatting for identical input, SF #638) dry-run conformance to
  `app/lib/astylerc`, scoped to `app/bin`, `app/wlib`, `app/help`, and `app/tools`. Vendored code
  (`app/wlib/gtk3lib/wrapbox/`, the `app/tools/dirent.c`/`dirent.h` Win32 shim, and
  `app/bin/uthash.h`, an unmodified upstream uthash release) and `unittest/` directories are
  excluded by design. The command, version and exclude list live in `tools/astyle-check.sh`;
  run it locally before pushing.
- **`fuzz-getargs`** — a bounded-time libFuzzer regression run of the `fuzz_getargs` harness (not
  continuous fuzzing; catches regressions in previously-fixed `GetArgs()` bugs like SF #645).

## Report-only jobs (upload an artifact, never fail the run)

- **`cppcheck-deep`** — cppcheck's full check set, beyond the gating job's defaults.
- **`clang-tidy`** — a conservative `bugprone-*`/`performance-*`/`portability-*` check-set.
- **`codespell`** — scans comments, strings, and identifiers in `app/bin`, `app/wlib`,
  `app/help` and `app/tools` for common misspellings.
- **`astyle-check-macos`, `astyle-check-windows`** — compare each platform's from-source-built
  astyle output against Linux's, to catch astyle itself behaving differently per OS (not code
  formatting drift — that's what the gating `astyle-check` job is for). The Windows job checks out
  with `core.autocrlf` off, so astyle sees the same LF line endings as on Linux.
- **`compiler-warnings`, `compiler-warnings-macos`, `compiler-warnings-windows`** (in
  `gtk3-qa-reports.yml`, so they show as `qa / ...`) — capture `-Wall -Wextra` build output on
  Linux (GCC), macOS (AppleClang) and Windows (MSYS2 MINGW64 GCC). Each compiler sees code the
  others don't (platform-only branches) and reports different warnings, so all three are
  published as "Compiler warnings" pages on the GitHub Pages site; look there for current counts
  rather than in this guide. The macOS and Windows jobs are `continue-on-error`, so a runner
  problem doesn't hold up the Pages deploy; the site then says that report is missing.
- **`coverage`** — generates an lcov HTML report and uploads it as an artifact.

## Toolchain preview (advisory)

`ci-gtk3.yml` pins its runner images and compilers (see \ref building "Building and Testing"'s
tool-version table), so an image update can't change tool versions under us.
`ci-gtk3-preview.yml` runs on the same triggers and calls the same two reusable workflows with
the next images:

- **Ubuntu 26.04** (`ubuntu-26.04`, `ubuntu-26.04-arm`): every Linux job in
  `gtk3-toolchain.yml`, plus the QA reports except coverage. Their plain-text artifacts carry a
  `-ubuntu2604` suffix, so newer cppcheck, clang-tidy and GCC findings can be compared before the
  pin moves.
- **Xcode 27** (`c-tests-macos` on GitHub's `xcode-27` preview image): the next AppleClang.
- **Windows 11 on ARM** (`c-tests-windows` on `windows-11-arm`, MSYS2 `CLANGARM64`): a platform
  we don't ship packages for yet.

A failure there doesn't block a merge, but it does need fixing or a ticket. When a preview is
consistently clean, move the pin forward by changing `ci-gtk3.yml`'s call (or the reusable
workflow's default input).

Every report-only job's findings land as a downloadable artifact on the workflow run's summary
page (Actions tab → the run → Artifacts); each job's own "Upload findings"/"Upload warning log"
step names it. **CodeQL** is a separate workflow (`codeql.yml`) — it runs on push/PR and a weekly
Monday cron, and its results show up in the repo's Security tab rather than as an artifact.

Five of these (`coverage`, `cppcheck-deep`, `clang-tidy`, `codespell`, and `compiler-warnings` with
its macOS and Windows variants) are additionally published as permanent, browsable HTML at
`$GITHUB_USER.github.io/XTrkCAD` (GitHub Pages' default project-page URL for whichever fork's
Actions is doing the deploying), redeployed on every push to
`%main` by `release.yml`'s `qa-reports` job. That job calls `gtk3-qa-reports.yml` with
`html: true`, so the published reports come from the same job definitions as the PR artifacts. The four plain-text
tools are formatted into grouped, linkable HTML by `tools/format-findings-report.sh` — see below.

### Reproducing a report locally

Each report is just a CMake/CTest build plus one external tool, runnable outside CI the same way
any other build is. The coverage job:

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DXTRKCAD_TESTING=ON \
    -DCMAKE_C_FLAGS="--coverage" -DCMAKE_EXE_LINKER_FLAGS="--coverage"
cmake --build build
ctest --test-dir build --output-on-failure   # unit tests + full regression suite
lcov --capture --directory build --output-file build/coverage.info \
    --ignore-errors mismatch --rc branch_coverage=1
lcov --remove build/coverage.info '/usr/*' '*/build/*' '*/unittest/*' '*/tools/*' \
    --output-file build/coverage-filtered.info --ignore-errors unused --rc branch_coverage=1
genhtml build/coverage-filtered.info --output-directory coverage-html --rc branch_coverage=1
```

`cppcheck-deep`/`clang-tidy`/`codespell` each run their own analysis command straight from
their job definition in `gtk3-qa-reports.yml` — copy the `run:` step verbatim,
it needs no CI-specific setup. `compiler-warnings` is just `cmake --build` with
`-DCMAKE_C_FLAGS="-Wall -Wextra -Wno-unused-parameter"` piped through `tee`. `clang-tidy`'s own
configure step passes `-DCMAKE_C_COMPILER=clang` (SF #693) — reproducing it locally needs `clang`
itself installed, not just the `clang-tidy` binary, or `CMAKE_C_COMPILER_ID` resolves to the
system default compiler and two of `PlatformSettings.cmake`'s Clang-conditional exemptions
silently stop applying.

Once you have a plain-text findings file (cppcheck/clang-tidy/compiler-warnings output, or
codespell's), turn it into the same grouped, GitHub-linked HTML the published site shows with
`tools/format-findings-report.sh`:

```sh
tools/format-findings-report.sh <input.txt> <output.html> "<title>" <bracketed|codespell> <blob-base-url> [source-root]
```

`bracketed` handles cppcheck/clang-tidy/compiler-warning lines ending in `[category]`;
`codespell` handles its `file:line: word ==> suggestion` shape instead. `<blob-base-url>` is
prepended to each finding's `file#Lline` to build its link — pass a real GitHub blob URL (e.g.
`https://github.com/$GITHUB_USER/XTrkCAD/blob/$(git rev-parse HEAD)/`) to get working links
against your own checkout, or omit meaningful linking entirely by passing any placeholder if you
just want the grouped local HTML view. Compilers print absolute paths, so pass the checkout's
root as `[source-root]` (`"$PWD"` from the repository root; in MSYS2, `"$(cygpath -m "$PWD")"`)
to make them repository-relative; it handles Windows paths written with `/` or `\`, whose
drive colon would otherwise keep the line from parsing. The `coverage` report doesn't go through this script — `genhtml`
already produces its own linked, per-line HTML.

The gating `cppcheck` job (default/error-only severity, distinct from the report-only
`cppcheck-deep` above) runs `tools/cppcheck-gate.sh`, which holds its scope and suppression list.
Run it from the repository root to reproduce the job:

```sh
tools/cppcheck-gate.sh
```

Results depend on the cppcheck version. CI uses Ubuntu 24.04's (see \ref building "Building and
Testing"'s tool-version table). The remaining gating jobs — `astyle-check`, the
`ctest` unit/regression suite, and `docs-doxygen` itself — are just this project's everyday
build/test/format loop rather than anything CI-specific, and are already documented in full on the
\ref building "Building and Testing" page and in the project's `CLAUDE.md` Code Formatting
section; nothing to add here beyond pointing at those.

## The warning-count ratchet

`PlatformSettings.cmake` promotes specific warning categories to `-Werror=` once a phase has
brought their count to zero for owned code, so a new PR can't silently reintroduce them:
`sign-compare`, `type-limits`, `absolute-value`, `unused-but-set-parameter`, and
`implicit-fallthrough` are gated on both GCC and Clang; `cast-function-type` stays GCC-only.
That last exception is deliberate, not an oversight: Clang's equivalent
`-Wcast-function-type-strict` sub-check additionally flags ~106 instances of GLib/GTK's own
mandated generic-callback-cast idiom (`GCallback`, `GCompareFunc`, `G_DEFINE_TYPE`-generated code)
that aren't realistically fixable without abandoning GTK's own type-safety pattern — documented as
permanently-accepted non-ratchetable noise (SF #662) rather than ratcheted like the rest.
`implicit-fallthrough` reached universal (GCC+Clang) status the same way: 73 genuine sites were
standardized on `__attribute__((fallthrough))`, recognized identically by both compilers, before
the Clang-only guard was dropped (SF #663).

`incompatible-pointer-types` is an error on GCC and Clang, on every platform. Until Development
Issues #52-#54 it was suppressed, and fixing the sites turned up real bugs (a layer's frozen flag
written through the wrong type, an uninitialized result for an empty list, an icon from the
wrong set). To keep new code clean:

- **Define a callback with exactly the signature of its typedef.** A `paramChangeProc` takes
  `paramGroup_cp`; wlib's button and menu callbacks take `void *`, not `const void *`; list
  callbacks take `wIndex_t`. Don't cast a function to make it fit: a call through the wrong
  type is undefined behaviour, which Clang's `-fsanitize=function` (in the UBSan run) reports.
- **A helper that only reads a group, or writes through its controls, takes `paramGroup_cp`.**
  Writing through `pg->paramPtr` is allowed on a const group.
- **wlib control handles are all `struct control`** (`wControl_p`, `wButton_p`, `wList_p`, ...),
  so they pass to each other without casts.
- **Convert GTK and GLib types with their macros, not casts or nothing:** `GTK_WINDOW()`,
  `GTK_WIDGET()`, `GTK_TOGGLE_BUTTON()`, `G_OBJECT()`, and `G_CALLBACK()` for signal handlers.
  In debug GLib the widget macros also check the type at run time.
- **Give a pointer its real type** rather than casting it at each use (a file name is a
  `const char *`, an icon a `wIcon_p`).

The large-volume cppcheck/clang-tidy categories that Phase 13 sampled and decided *not* to
bulk-suppress (`variableScope`, `bugprone-narrowing-conversions`, and others) are deliberately
left visible in the `cppcheck-deep`/`clang-tidy` report-only artifacts instead — see
\ref code-quality-process-and-patterns "Code quality: process and patterns" below for why.

# release.yml {#release-workflow}

`release.yml` builds the packages, install-tests them on clean systems, and publishes them. What
it publishes depends on how it was started:

- **Push to `%main`:** every package and install test. `publish-latest-release` refreshes the
  rolling `latest-main` prerelease as soon as the packages are built, without waiting for the
  install tests, and `deploy-pages` redeploys GitHub Pages.
- **`v*.*.*` tag:** the same, plus the NSIS installer and its test. `github-release` drafts the
  release only once `check-release-tag` passed and every package **and** every install test
  passed (see \ref cutting-a-release "Cutting a release").
- **Dispatch** (`gh workflow run release.yml --ref <branch>`): what the inputs below choose.
  Nothing is published unless `publish_temp_release` is set.

`release.yml` itself only orchestrates and publishes. The platform work is in reusable workflows,
so each can be read on its own:

| File | Jobs | Called by |
|---|---|---|
| `packages-linux.yml` | `package-linux-ubuntu`, `package-linux-debian`, `package-linux-fedora` | `release.yml` (`packages-linux`) |
| `packages-macos.yml` | `package-macos` (arm64, x86_64), DMG with `include_dmg` | `release.yml` (`packages-macos`) |
| `packages-windows.yml` | `package-windows` (MINGW64, UCRT64: ZIP, NSIS with `include_nsis`) | `release.yml` (`packages-windows`) |
| `install-tests.yml` | `install-test-linux`, `install-test-windows`, `install-test-windows-nsis`, `install-test-macos` | `release.yml` once per platform (`install-tests-linux`, `-windows`, `-macos`); `install-tests-only.yml` |
| `gtk3-qa-reports.yml` | the analysis reports | `release.yml` (`qa-reports`), `ci-gtk3.yml` |

In the run's job list a called job shows as `<caller job> / <job>`, e.g.
`packages-windows / package-windows`. Artifacts uploaded by a called workflow belong to the
calling run, which is how the publish jobs and `packages_from_run` find them. Packages and
install tests are separate files on purpose: `publish-latest-release` waits for the packages
only, and an install test failing on one platform doesn't stop `latest-main` (it does stop
`github-release`). Each platform's install tests wait only for that platform's packages.

\image html release-flow.svg "release.yml job flow"

The diagram's source is `docs/doxygen/images/ci/release-flow.puml`. After changing it,
regenerate the committed SVG with `plantuml -tsvg docs/doxygen/images/ci/release-flow.puml`.

## Dispatch inputs {#release-dispatch-inputs}

| Input | Default | Effect |
|---|---|---|
| `linux`, `windows`, `macos` | on | Build and install-test that platform's packages. Turn a platform off to save the runners when your change only affects the others. |
| `packages_from_run` | empty | The run ID of an earlier `release.yml` run (`gh run list -w release.yml`). No packages are built: the install tests download that run's artifacts and test them with *this* branch's `tools/package-install-test*` scripts. The artifacts expire after 14 days. The report and docs jobs are skipped too. |
| `include_nsis` | off | Also build and test the Windows NSIS installer. Tags always do. |
| `include_dmg` | off | Also try the experimental self-contained macOS DMG. |
| `publish_temp_release` | off | Publish the packages to a temporary prerelease named after the branch. Delete it once the branch merges or is dropped. |

Push and tag runs ignore the inputs and always build everything. `check-inputs` rejects a
dispatch that turns every platform off, a `packages_from_run` that isn't the ID of a
`release.yml` run, and a `publish_temp_release` combined with a reduced platform set or reused
packages: a temp release always holds a fresh build of every package. It logs the source run's
branch and commit.

## Recipes {#release-recipes}

**Changed an install-test script** (`tools/package-install-test*`): nothing to do.
`install-tests-only.yml` runs on the pull request (and on `test/**` pushes) and tests the newest
successful `%main` packages with your scripts, in minutes and without building anything. The
UCRT64 Windows packages are tested only when that run has them (`install-tests.yml`'s
`windows_ucrt` input). To pick the packages, or to include the NSIS installer from a run that
built it:

```sh
gh workflow run install-tests-only.yml --ref <branch> -f packages_from_run=<run id> -f include_nsis=true
```

To test one platform only, use `release.yml`'s inputs instead:

```sh
RUN=$(gh run list -w release.yml -b main -L 1 --json databaseId --jq '.[0].databaseId')
gh workflow run release.yml --ref <branch> -f packages_from_run=$RUN -f linux=false -f macos=false
```

**Changed packaging for one platform** (e.g. `distribution/` files for Windows):

```sh
gh workflow run release.yml --ref <branch> -f linux=false -f macos=false
```

**Changed `release.yml` or a workflow it calls** (`packages-*.yml`, `install-tests.yml`): run
it with every platform on, as `%main` will, before merging
(`gh workflow run release.yml --ref <branch>`). Pull-request CI never runs it. A change to the
tag-only jobs (`check-release-tag`, `github-release`) needs a trial tag; see
\ref cutting-a-release "Cutting a release".

**Adding a platform or package:** add its job to the matching `packages-*.yml` (artifact name
`packages-<platform>-...`), its install test to `install-tests.yml` behind that platform's input,
and nothing else: the publish jobs take every artifact of the run.

**A review build for testers:** `-f publish_temp_release=true`.

# Packaging and versioning {#packaging-and-versioning}

`ProgramVersion.cmake` defines `XTRKCAD_VERSION` from three plain numbers
(`XTRKCAD_MAJOR_VERSION.XTRKCAD_MINOR_VERSION.XTRKCAD_RELEASE_VERSION`, plus an optional
`XTRKCAD_VERSION_MODIFIER` suffix like `Dev`), then extends it with two
more layers so that different builds of the `default` branch can coexist on one machine instead of silently
overwriting each other.

## Per-version identity (SF #673)

`XTRKCAD_BIN` (`CMakeLists.txt`), set to `xtrkcad-${XTRKCAD_FILE_VERSION}`, is the single source of
truth for a build's on-disk identity: the installed binary name, the `.deb`/`.rpm`/NSIS package
name, the `.desktop` filename, the share/gettext-domain directory name, and (via the same
`XTRKCAD_VERSION` string reused in `app/bin/custom.c`) the runtime work/prefs directory. Before
this, all of those were the fixed literal `xtrkcad`/`xtrkcad-beta` on every branch, so installing
a GTK3 5.4.0 build over an existing GTK2 5.3.x install looked like an in-place upgrade to
the package manager and silently clobbered it. Embedding the version means two different point
releases now install and run side by side.

That is what development builds want, but not releases: a new release should replace the
previous one. `XTRKCAD_VERSIONED_INSTALL` (Development Issues #48) drops the version from
`XTRKCAD_BIN` and the Windows install folder and registry key, giving plain `xtrkcad`. Its
default does that on tagged release builds only; see
\ref release-defaults "Defaults for Release" on the Building and Testing page.

## Dev-build hash suffix

Distinct point releases weren't the whole problem: every ordinary push to the `default` mainline
between tagged releases still carries the *same* `XTRKCAD_VERSION` (there's no version bump
per-commit), so successive dev builds would still collide with each other under the scheme above.
`ProgramVersion.cmake` closes that gap by appending a fourth, dot-separated component — a short
VCS hash of the checked-out commit, e.g. `5.4.0.8b67f185` — to any build that isn't the exact
commit a `v<version>` tag points at. The check works from either a git or an Hg checkout (detected
by the presence of `.git`/`.hg` under `CMAKE_SOURCE_DIR`), since this file is shared by both
source trees (see the \ref index "Developer Documentation" page's note on SourceForge Hg vs. the
GitHub git mirror).

A tagged release build — `git tag v5.4.1 && git push origin v5.4.1`, see `release.yml`'s
`github-release` job — resolves to the plain three-part version, no hash, and installs under the
unversioned name `xtrkcad` (\ref release-defaults "Defaults for Release"). The release workflow is
otherwise unchanged: after tagging, bump `XTRKCAD_RELEASE_VERSION` (and `XTRKCAD_VERSION_MODIFIER`
if used) for the next dev cycle in the same commit or a prompt follow-up — every build after that
picks the hash suffix back up automatically, with no separate manual toggle needed per commit.

## Linux packages and install tests {#package-install-tests}

`release.yml` builds one Linux package per supported release, each on that release, all for
amd64 and arm64:

- `.deb`: `package-linux-ubuntu` on Ubuntu 24.04 and 26.04 runners, `package-linux-debian` in
  `debian:bookworm` and `debian:trixie` containers;
- `.rpm`: `package-linux-fedora` in `fedora:44` and `fedora:45` containers.

The file name ends in the release (`-ubuntu2404.deb`, `-ubuntu2604.deb`, `-debian12.deb`,
`-debian13.deb`, `-fedora44.rpm`, `-fedora45.rpm`), because a package built on one release
declares that release's library package names or sonames and usually won't install on another.
A 24.04 `.deb` doesn't install on 26.04, for example: it depends on `libzip4t64` and 26.04 only
has `libzip5` (Development Issues #37). The RPM used to be built on Ubuntu and required
`libzip.so.4`, which no Fedora release has (#38). The Ubuntu 24.04 amd64 job also builds the
STGZ installer and the source archives.

The Fedora containers run with `--privileged`. Fedora's gdk-pixbuf loads images through
glycin, which runs each image loader in a `bwrap` sandbox; `bwrap` can't create its namespaces
in an unprivileged container, so the loader dies and GTK aborts on its first icon. That's a
property of the container, not of a real Fedora system.

The `install-test-linux` jobs then install each package in a fresh container of the release
it's for, with that release's own `apt-get` or `dnf`, and run `tools/package-install-test.sh`.
The script checks that:

- the package installs, so every dependency it declares exists on that release;
- the binary is on `PATH`, all its libraries resolve, and `-V` prints the version in the
  package file name;
- the data directory the program computes (`../share/<binary name>` next to the binary, see
  `wGetAppLibDir()`) holds `params`, `demos`, `examples` and the help HTML;
- the installed binary plays the `dmintro` and `dmstrtrk` demos from the installed data under
  Xvfb, with an empty `HOME`;
- Help opens the installed help pages: a fake browser registered in that `HOME` records what
  `tools/package-install-test-help.xtr` opens with the `HELP` playback command, and each topic
  must arrive as `<data directory>/html/<topic>.html` (Development Issues #58, see
  \ref help-testing);
- the menu entry (`share/applications/<binary name>.desktop`, which must pass
  `desktop-file-validate`), the icons (`share/icons/hicolor`) and the MIME type
  (`share/mime/packages`) are installed system-wide, and the distribution's triggers have
  registered them: `application/x-xtrkcad` is in the MIME database and `mimeinfo.cache` maps it
  to the `.desktop` file;
- the package removes cleanly, desktop files included.

The packages have no maintainer scripts. The distribution's own triggers (desktop-file-utils,
shared-mime-info, the icon cache) register the files for every user, so a package installed
from a GUI installer or a root shell gets a menu entry too. Each file carries the binary name,
so several versioned builds can be installed side by side, and a release (plain `xtrkcad`)
upgrades the previous one.

The install tests don't hold back `latest-main`: a failure shows on the run, and the packages
are published anyway. A release is different: `github-release` needs every install test, so no
release is drafted from a package that failed one (Development Issues #48). To run the same
test locally, mount the
package and the script into a container of the target release (add `--privileged` for Fedora,
and for Ubuntu 26.04 under rootless podman, whose gdk-pixbuf also uses glycin):

```sh
podman run --rm -v "$PWD:/src:ro,z" ubuntu:26.04 \
    sh /src/tools/package-install-test.sh /src/xtrkcad-setup-5.4.0-1.x86_64-ubuntu2604.deb
```

## Windows install test {#package-install-test-windows}

Background on Windows' run-time differences and the ZIP's layout: \ref windows.

The Windows packages are the ZIPs from `package-windows`, one per MSYS2 environment (MINGW64
and UCRT64, \ref windows-package). Users install one by unpacking it anywhere. The
`install-test-windows` job runs once per variant: `tools/package-install-test-windows.ps1` on a
fresh `windows-2025` runner. The script unpacks the ZIP into a directory whose name contains a
space and runs the program with only the Windows system directories on `PATH`. Without that,
a DLL the package forgot could still be found in the runner's MSYS2, Git or Perl. It checks
that:

- there's exactly one `bin\xtrkcad-<version>.exe`, and it starts: a missing DLL shows as exit
  code `0xC0000135`;
- `/V` prints the version in the ZIP's name. On Windows, options start with `/`
  (`app/wlib/gtk3lib/win32/getopt.c`), and a `-V` would be opened as a layout file;
- `share\<binary name>` holds `params`, `demos`, `examples` and the help HTML;
- the program plays the `dmintro` and `dmstrtrk` demos from the unpacked data. On Windows the
  log goes to a file rather than stdout (`lprintf.c`), so the script passes `/l` and reads
  `Regression Tests Pass` from it;
- Help opens the installed help pages (Development Issues #58, see \ref help-testing). A fake
  `.html` handler, a `.cmd` file registered under `HKCU\Software\Classes`, records what
  `ShellExecute()` gives it. The runner has a `UserChoice` default for `.html` (Edge's
  `MSEdgeHTM`), which wins over that registration and which Windows won't let a script remove,
  so in CI the script also registers the fake under `HKCU\Software\Classes\MSEdgeHTM`: HKCU's
  open command for a ProgId wins over HKLM's. It removes both afterwards, and on a developer's
  machine it leaves the `UserChoice` ProgId alone;
- it warns when running the program writes into its install directory, which is read-only
  under `Program Files`.

`package-windows` builds the ZIP by installing to a staging directory and copying in the GTK3
runtime: pixbuf loaders, GSettings schemas, the hicolor icon theme, and every DLL from its MSYS2
environment that `ntldd -R` finds for the binary and the loaders. Before this test existed, that copy failed
without anyone noticing (`xargs` dropped the backslashes in `ntldd`'s Windows paths, and
`|| true` hid the errors), so no ZIP had ever shipped a DLL. The step now fails on any error.

Every run of the program has a timeout, because a modal dialog (an unknown option, a missing
file) would otherwise hang the job. Like the Linux tests, it gates a release but not `latest-main`.
To run it on a Windows
machine:

```powershell
pwsh tools/package-install-test-windows.ps1 xtrkcad-5.4.0-windows-x64.zip
```

## Windows installer test {#package-install-test-nsis}

When the NSIS installers are built (version tags, or `include_nsis`), the
`install-test-windows-nsis` job runs once per variant: `tools/package-install-test-nsis.ps1` on a fresh
`windows-2025` runner. It does what a user does with the installer and checks the result:

- it installs silently (`/S`) into the default folder, which must be under `Program Files`,
  and leaves exactly one `XTrkCAD` uninstall entry in the registry;
- `tools/package-install-test-windows.ps1 -InstallDir` runs all of the ZIP test's checks on the
  installed tree, with the version taken from the installer's name;
- every Start menu shortcut points at a file that exists, and one starts the installed binary;
- `.xtc` and `.xtce` open with the installed binary;
- installing again over the first install, the upgrade path, answers yes to "uninstall the old
  version first" and leaves one uninstall entry and one binary;
- uninstalling removes every file, the uninstall entry, the Start menu folder and the file
  association.

The installer needs administrator rights, which a GitHub runner has. On your own machine, run it
from an administrator PowerShell on a system with no XTrackCAD installed through the installer:

```powershell
pwsh tools/package-install-test-nsis.ps1 xtrkcad-setup-5.4.0-x64.exe
```

Like the other install tests it gates a release (`github-release` needs it) but not
`latest-main`, which has no installer.

## macOS install test {#package-install-test-macos}

Background on the macOS package and on debugging there: \ref macos.

The macOS package is a Homebrew-dependent `.tar.gz` per architecture, with the binary linked
against Homebrew's GTK3 by absolute path. The `install-test-macos` jobs follow its
`README-INSTALL.txt` on fresh `macos-26` (arm64) and `macos-26-intel` (x86_64) runners with
`tools/package-install-test-macos.sh`: `brew install` the prerequisites, unpack the archive into
a directory whose name contains a space, and start the binary by its full path from `$HOME`. It
checks that:

- there's exactly one `bin/xtrkcad-<version>`, and every library it links (`otool -L`) exists;
- `README-INSTALL.txt` names that binary;
- `-V` prints the version in the archive's name;
- `share/<binary name>` holds `params`, `demos`, `examples` and the help HTML;
- the program plays the `dmintro` and `dmstrtrk` demos from the unpacked data, with an empty
  `HOME`;
- Help opens: on CI, Safari starts and shows the last help topic's page (see
  \ref help-testing).

Starting from `$HOME` matters: the program has to find its data from its own location. Before
Development Issues #43, macOS looked for it relative to the current directory. macOS has no
`timeout(1)`, so the script bounds each run with a `perl` alarm. Like the Linux tests, the
jobs gate a release but not `latest-main`. To run it on a Mac:

```sh
sh tools/package-install-test-macos.sh xtrkcad-5.4.0-macos-arm64.tar.gz
```

# Code quality: process and patterns {#code-quality-process-and-patterns}

CI runs several static-analysis tools on this branch (AStyle formatting, CodeQL, `cppcheck-deep`,
clang-tidy, a `fuzz_getargs` libFuzzer harness), some gating and some report-only, plus a
warning-count ratchet that locks in categories already brought to zero so they can't silently
regress. Working through their findings across many sessions converged on a process, and turned up
several defect classes that kept recurring — both captured here for whoever picks up the next
batch of findings.

**Goals for QA.** The aim is to reach, and hold, zero suppressed findings per category in owned
code — by fixing the underlying defect or narrowing the variable's scope, not by suppressing the
tool's complaint. A suppress comment is a last resort for a confirmed non-bug the tool can't be
taught to recognize (see the `trackGauge`/`infoSubst` entries below), never the default response
to a large finding count. The bullets right below embody this in practice; this line just says it
up front. Current status: `shadowVariable` is at zero across all of `app/bin` (SF #764/#765/#766,
2026-09-01, closing what had been the last large-volume category); `variableScope` has been
reduced to a documented minimum (2026-08-31); a for-loop index-variable sweep is the next open
category, not yet started.

## Static-analysis workflow

- **Re-verify counts against a fresh CI artifact before triaging anything.** A category's finding
  count — and even which findings exist at all — can shift between sessions purely from tool
  version drift on the CI runner image. Don't triage from a number a plan document wrote down last
  week; pull the current artifact first.
- **Sample individual findings before bulk-suppressing a category, even one that looks like pure
  style noise.** A category "looking safe" from its name is not evidence that it is: sampling
  `knownConditionTrueFalse` turned up a real copy-paste bug in `cbezier.c` (SF #664); sampling
  `shadowVariable` — a category already provisionally labeled low-risk — turned up the `cgroup.c`
  `trackCount` bug below (SF #669).
- **Verify a suppress comment against CI's *exact* tool version, not just a local one.** A local
  tool that's newer or older than CI's pinned version can fail to reproduce a finding at all,
  giving a false "verified" result — SF #667's `ctodesgn.c` `cppcheck-suppress` looked correct
  against a local cppcheck 2.17.1, but CI's pinned 2.13.0 still flagged it. Both
  `// cppcheck-suppress` and `NOLINTNEXTLINE` anchor to the physical line immediately before the
  exact line the diagnostic is attached to — for a multi-line expression that isn't necessarily
  the statement's first line, and any explanatory comment placed between the directive and the
  code silently breaks it. This recurred independently in both #667 (cppcheck) and #665
  (clang-tidy).
- **CodeQL inline suppression comments (`// codeql[query-id]`) turned out not to be durable in
  practice — prefer fixing the actual taint path instead.** They were tried first for SF #709's
  `cpp/path-injection` findings in `genmessages.c`/`genhelp.c` (same placement discipline as
  `cppcheck-suppress`/`NOLINTNEXTLINE` above: the line immediately before the flagged statement),
  verified via `gh api repos/OWNER/REPO/code-scanning/analyses`'s `results_count` for the PR's own
  `refs/pull/N/merge` entry (0 vs. a nonzero count on an earlier PR touching the same lines
  without one) — that looked like real evidence, but checking GitHub's own Security tab directly
  (`is:open rule:cpp/path-injection`) showed the alerts were still open regardless. Replaced with
  real input validation instead — a `PathHasTraversal()` check rejecting `..` path components
  before every argv-derived `fopen()`/`FOpenRestricted()` call — which a small, isolated PR (#113)
  confirmed passes CodeQL cleanly with 0 alerts. **The Security tab, not a `results_count`
  number or a PR check, is the trustworthy signal for whether an alert is genuinely gone.** Also
  worth checking every flagged location individually before assuming one fix covers a whole
  finding class — SF #709 found a second, previously undocumented pair of the identical
  `cpp/path-injection` pattern in `genhelp.c` this way, sitting right next to the already-known
  `genmessages.c` pair with no suppression or explanation at all.
- **CodeQL alerts can resurface as "new" on a large-diff PR even when nothing about the flagged
  code actually changed** — its own re-scan fallback for diffs past some size threshold
  re-evaluates more broadly than a normal incremental diff, and pre-existing, already-understood
  code can get relabeled "new" in the PR check as a result. Confirmed three times now (PR #60,
  PR #100, PR #109) on this project — PR #109 flagged `genhelp.c:177`/`198` again even though
  both lines are the exact `fopen()` calls, each already guarded by a `PathHasTraversal()` check
  5 lines above (confirmed via `git show` on the PR branch, not just eyeballing HEAD). Real input
  validation is durable against this in the sense that the underlying vulnerability class is
  actually fixed — but the PR check itself will still show a spurious "N new alerts" on a
  large-enough diff regardless, so don't expect a clean check as proof; verify the flagged lines
  directly instead, same as above.
- **Don't assume every new CodeQL alert on a PR is the known rescan false-positive pattern above
  — investigate each one on its own merits.** PR #114 (unrelated to genhelp.c/genmessages.c)
  flagged a genuinely different, real finding: `sprintf(buf, "%0.2f", someDouble)` into a fixed
  256-byte buffer in `careditdlg.c` (SF #711) — `%f` has no upper bound on output length for an
  arbitrary `double`, so this was a real (if practically edge-case, given these are car-price
  fields) fixed-buffer overflow, fixed with `snprintf(..., STR_SIZE, ...)`. The two patterns look
  identical from the PR checks list (both show as "CodeQL: fail" with a short duration) — the
  only way to tell them apart is pulling the actual annotation data
  (`gh api repos/OWNER/REPO/check-runs/ID/annotations`) and reading what line/rule it flags.
- **The branch/ticket pipeline for a fix found this way:** SF ticket → Hg bug branch (stacked on
  any unmerged prior work it depends on) → git PR → CI green → merge → a review window (roughly
  2 days, case by case) → Hg merge. A CI-configuration-only change with no application code (a new
  report-only job, a workflow tweak) can skip the Hg bug branch entirely, since Hg has nothing to
  carry — but it still gets an SF ticket.
- **A finding's reported line number is tied to the exact tree state it was pulled against — never
  reuse it against a different tree.** This project runs an Hg branch (behind) and a git branch
  (merged ahead) side by side, often for days at a time. Pulling a fresh CI artifact from the git
  side and then applying its line numbers as edit coordinates on the Hg side (or vice versa) will
  silently drift wherever the two trees' line counts have diverged — even a handful of unrelated
  `const`-correctness edits or dead-code deletions upstream shifts everything below them. SF #726
  — a 111-finding `shadowVariable`/`shadowFunction` suppress-comment sweep applied git-derived line
  numbers to the Hg tree; ~35% of the inserted comments landed one or more statements away from
  their intended target (confirmed by re-running cppcheck after the edit and finding a third of the
  findings still unsuppressed, not by the insertion script's own success output). Fixed by
  reverting the uncommitted Hg-side edits and regenerating the finding list fresh **against the Hg
  tree itself** before reapplying. Re-pull per tree, every time — a "fresh" artifact is only fresh
  for the tree it was generated against.
- **Static tools and a live sanitizer run catch different classes of bug — run both.** cppcheck/
  clang-tidy/CodeQL never execute the code; they can't catch a mismatch that only manifests at
  runtime against a specific input (a struct field one size too small, a sentinel value an
  unrelated caller forgot to check). Building with `-DXTRKCAD_SANITIZE=ON` and actually running
  the app — interactively, or better, the full demo-playback `RegressionSuite` — surfaces a
  different, complementary set of real bugs. SF #675, #677–#680 were all found exactly this way in
  one session: none of them showed up in any of `cppcheck`/`cppcheck-deep`/`clang-tidy`'s existing
  findings for the same files, because none of those tools model "this `void *` gets reinterpreted
  as a different-sized type at runtime based on a tag" or "this loop condition dereferences before
  its own bounds guard." SF #682 documented a concrete gap this exposed: CI's sanitizer job and its
  regression-suite job had never actually been run together. Closed by adding
  `regression-gtk3-sanitizers` (`ci-gtk3.yml`), which runs the full `RegressionSuite` under
  `-DXTRKCAD_SANITIZE=ON` — the same combination that found #675/#677–#680, now on every PR instead
  of only when someone thinks to run it locally. Scoped to `RegressionSuite` itself, not
  `regression-gtk3-standalone`'s ~47 per-demo cold-start tests, to keep the added CI time bounded
  (~3 minutes observed locally) against unproven additional benefit from sanitizing the standalone
  path too.

## Bug patterns found in this codebase

Real defect classes this project's history has actually hit, not a hypothetical checklist — each
one below was found by the static-analysis process above, or by the live-sanitizer/regression
combination described just above it, and confirmed by reading the code, not inferred from a
tool's message alone.

- **Variable shadowing hides an intended write to an outer/global variable.** A local variable
  sharing a global's name silently absorbs writes meant for the global. SF #669 — `cgroup.c`'s
  `UngroupCompound()` declared a local `trackCount` shadowing the global `wIndex_t trackCount`
  (`track.c`); two `trackCount--` calls meant to mirror `DeleteTrack()`'s bookkeeping decremented
  the dead local instead, so ungrouping certain compounds never decremented the app-wide track
  count.
- **`sizeof` applied to the wrong thing.** Three separate variants turned up in this codebase:
  `sizeof` on a decayed pointer instead of the array it used to be (SF #667, `dlayer.c` —
  `sizeof(layers[inx].name-1)` truncated preference-loaded layer names to 8 bytes instead of 79);
  a missing dereference level, sizing a struct instead of a pointer to one (SF #670, `track.c`);
  and a variable that looked like an array but was declared as a bare scalar (SF #669,
  `partcatalog.c`'s `stopwords`, likely collapsed to one entry during a merge conflict per
  `git blame`).
- **`&&`/`||` swapped in a condition, silently dead-coding a branch.** SF #667 — `cjoin.c`'s
  `JoinWithStraight()` used `&&` where a complementary check nearby confirmed `||` was intended,
  permanently preventing a curve-orientation adjustment from ever firing. Confirmed live via the
  app's own `-d join=3` debug logging (see \ref running-with-debug-logging "Running with debug
  logging" in the \ref index "Developer Documentation" page), not just by inspection.
- **Copy-paste debris passing stray arguments to a sibling function's signature.** Adapting a
  nearby call by search-and-replace and forgetting to trim an argument list the new call doesn't
  need. SF #664 — `cbezier.c`; SF #667 — `cgroup.c`'s
  `ErrorMessage(MSG_GROUP_NO_PATHS, _("Ok"), NULL)` carried 2 unused varargs copy-pasted from a
  sibling `NoticeMessage` call just above it.
- **Off-by-one bounds checks.** SF #647 — `cturntbl.c`'s `ReadTurntable()` checked
  `currEp > GetTrkEndPtCnt(trk)` where valid indices only run `0..count-1`; a `currEp` exactly
  equal to the count slipped through unreset and tripped an unrelated assertion much later instead
  of failing where the actual bad value was read.
- **Unbounded format-string/buffer writes on untrusted file content.** `.xtc`/`.xtp`/`.xtr` files
  are untrusted input, so a fixed-size destination buffer with no read-width limit is a real
  overflow, not a theoretical one. SF #645 — `GetArgs()`'s string-field format code had zero
  bounds checking across 14 call sites, all writing into small fixed buffers; fixed with a
  mandatory inline width (e.g. `"s9"`, mirroring `scanf("%9s", ...)`). SF #664 — `scale.c` had the
  same class of bug via an unbounded `sscanf()` on `.xtp` content.
- **A debug-log category declared and used, but never actually wired up.** A `LOG(log_x, ...)`
  call and its `static int log_x` guard variable both exist and look complete, but nothing ever
  calls `LogFindIndex()` to populate `log_x`, so the category silently does nothing no matter what
  `-d` flag is passed. Found by cross-referencing every `LogFindIndex()` call against every
  `log_*` declaration while completing the \ref log "Log Commands" catalog — `tstraigh.c`'s
  `straight`, `archive.c`'s `zip`, and `undostream.c`'s own separate `undo` registration all had
  this bug (fixed in SF #671's later commits).
- **Code reads/frees a shared record without checking whether it's still valid.** A dynamic array
  slot or list-widget row marked "deleted"/"unloaded" via its own flag, but still iterated or
  compared by code that doesn't check that flag first. SF #675 — `dprmfile.c`'s
  `CompareParameterFiles()` (a `qsort()` comparator) called `strcmp()` on a just-unloaded, freed
  parameter file's `.contents` without checking `.valid`, and `wlibListStoreClear()`
  (`liststore.c`) freed every row's context data *before* calling `gtk_list_store_clear()` — which
  can reentrantly fire a `selection-changed` signal mid-clear, whose handler
  (`wlibTreeSelectionChanged()`) then wrote into the already-freed context of a row GTK hadn't
  actually removed yet. Fixed by making the comparator group invalid entries by validity alone
  (never touching their freed fields), and by deferring the free in `wlibListStoreClear()` until
  after `gtk_list_store_clear()` fully returns. SF #676 — the same "no validity check" shape,
  independently: `GetParamFileName()`/`GetParamFileContents()` (`paramfile.c`) returned a freed,
  un-nulled pointer unconditionally, and `problemrep.c`'s `ProblemDataCollect()` (Help → Collect
  Problem Info) read it for every parameter file by index with no check.
- **A short-circuit bounds guard placed after the access it's meant to protect.** `&&` evaluates
  left to right, so `while ( *p && (p >= bufStart) )` dereferences `p` *before* checking whether
  it's still in bounds — the guard only stops the *next* iteration. SF #679 — `cgroup.c`'s
  `GroupOk()`, walking a path buffer backward; the guard's own inline comment
  (`//Add Guard for flip backwards`) shows it was added deliberately, just on the wrong side of
  `&&`. Distinct from the `&&`/`||`-swapped pattern above — the operator was already correct here,
  only the operand order was backwards.
- **A bounds check covers only the upper limit, so a negative sentinel value slips through.**
  `CHECK( ep < GetTrkEndPtCnt(trk) )` is true for *any* negative `ep`, not just valid indices — and
  this codebase uses `-1` as a real "not found" sentinel elsewhere (e.g.
  `PickUnconnectedEndPointSilent()`'s own return value). SF #678 — `track.c`'s
  `ConnectAllEndPts()` passed such a sentinel straight into `GetTrkEndPos()` with no check,
  reading before the endpoint array's allocation; fixed at the caller, and defensively at all 8
  `CHECK( ep < ... )` sites across `track.c`/`trkendpt.c` (now `CHECK( ep >= 0 && ep < ... )`).
- **A param-dialog binding declared a narrower C type than the framework actually reads/writes.**
  `app/bin/form/defaultvalues.c`'s `FormLoadDefaultValues()` reinterprets every `PD_RADIO`/
  `PD_TOGGLE`/`PD_LONG`/`PD_SCALE`/`PD_COLORLIST`/`PDO_LISTINDEX`-flagged `PD_LIST`/`PD_COMBOLIST`
  binding's `void *valueP` as `long *`, unconditionally — regardless of the bound variable's real
  declared type. A `static int`, a `BOOL_T` (`typedef int`), or a `wIndex_t`/`SCALEDESCINX_T`/
  `GAUGEINX_T` (all also `typedef int`) bound this way is a real 4-byte overflow on every
  preferences load on a 64-bit Linux build, not a theoretical one — a pre-64-bit-era assumption
  (`int`/`long` were the same width on 32-bit and still are under Windows' LLP64 model) that only
  breaks under LP64. SF #677 — 12 separate sites across `misc.c`, `dxfimport.c`, `ctext.c`,
  `dpreferences.c`, `cdraw.c`, `dcmpnd.c`, and `scale.c`, one of which (`toolbar.c`'s `iconSize`
  read) had an explicit `(long*)` cast specifically to silence the compiler warning that should
  have caught the mismatch — a cast hiding a real bug rather than the deliberate, visible
  narrowing the \ref index "Developer Documentation" page's Casts section calls for.
- **An index derived from `count - 1` where `count` can legitimately be zero.** SF #680 —
  `drawgeom.c`'s `DrawGeomMouse()` read `tempSegs(segCnt-1)` when closing a polygon/polyline via
  Tab/Enter/Space, without checking `segCnt > 0` first; pressing the close key before placing any
  point reads one element before the (just-resized-to-empty) array. Fixed by skipping the check
  entirely when `segCnt == 0` — correct behavior, not just crash-avoidance, since there's no "last
  segment" to compare against yet.
- **A Doxygen tag's platform-dependent default silently diverges across CI, invisible until a
  real cross-platform build hits it.** `CASE_SENSE_NAMES` (unset = Doxygen's own default) is
  case-sensitive on Linux, case-insensitive on Windows/macOS — so an unpinned `Doxyfile` generates
  case-preserved page filenames (`cmdBlock.html`) on Linux but lowercased ones (`cmdblock.html`)
  on Windows/macOS. SF #729/#730 — `app/doc/Doxyfile.in` (the User Guide config, separate from
  `docs/doxygen/Doxyfile.in` used to build *this* page, which already pinned it correctly) had no
  explicit value; every `ci-gtk3.yml` job that exercises it only runs on Linux, so the divergence
  was invisible until `release.yml`'s Windows/macOS packaging jobs (which only run on push, i.e.
  only *after* a merge, never during PR review) hit it for real, failing a new exact-case-matching
  CTest (`HelpLinksCheck`) with literally every topic but `index.html` (Doxygen's own
  always-generated landing page, name unaffected by this setting) reported missing — that specific
  fingerprint (one page fine, everything else broken, Linux-only passing) is what pointed at
  filename casing rather than a build-ordering or path-resolution bug. Fixed by pinning
  `CASE_SENSE_NAMES = YES` explicitly. Worth checking any Doxyfile whose output filenames are
  referenced by code (not just browsed by a human) for other unpinned platform-dependent defaults.
- **A whole source file with real-looking, `EXPORT`-marked functions that isn't actually compiled
  into anything.** `app/bin/paramwrapper.c` (header comment: "Wrapper around old Param*()
  functions to facilitate simple switch to new variants" — an apparent leftover transitional shim)
  is not listed in `app/bin/CMakeLists.txt`'s `target_sources(xtrkcad-lib ...)` at all, confirmed
  both by `grep` and by a genuine link error (`undefined reference`) after adding a new function
  there and building with `CCACHE_DISABLE=1` from a truly clean tree — an earlier build had
  silently kept reporting success because a stale ccache-cached object satisfied the link. Found
  during SF #730; see \ref ci-tooling-overview "CI tooling overview" above for the general ccache
  gotcha. If a change to `paramwrapper.c` ever seems to have no effect, this is why — verify with
  `nm <binary> | grep <symbol>` before assuming the edit landed. (`paramwrapper.c` has since been
  removed, together with `param.h`, whose live definitions moved into `form.h`; the lesson about
  unbuilt files and stale ccache objects still applies.)
- **A UI event-handler case statement that looks live but is permanently unreachable.**
  `app/wlib/gtk3lib/dialog.c` has a `GTK_RESPONSE_HELP` switch case inside a function meant to
  handle a `GtkDialog`'s "response" signal — but `wWinDialogCreate()` (the only place a dialog
  built this way gets created) never calls `g_signal_connect(dialog, "response", ...)`, so that
  function can never run regardless of what it contains. It's also independently broken by an
  unclosed `/**` comment a few lines above it that swallows the function's entire signature and
  body (through to the next real `*/`, which happens to belong to the *next* function's docblock)
  — found while tracing SF #730's real Help-button mechanism (`app/bin/form/dialog.c`'s
  `FormCreateDialog()`, which wires the Help button's click handler directly, bypassing the
  response-signal path entirely). Neither the dead code path nor the unclosed comment has been
  fixed — fixing the comment wouldn't make the function reachable, and the function has no
  purpose to reach. Documented here so a future session doesn't re-diagnose the same dead end.

- **A file-system path helper used to build a path that isn't a file-system path.**
  `MakeFullpath()` (`paths.c`) joins its parts with `FILE_SEP_CHAR`, which is `\` on Windows.
  GResource paths (`g_resources_lookup_data()`, the compiled-in icons and fonts) always use `/`,
  as do URLs. Development Issues #44 — `dlayer.c`'s `InitializeCustomFont()` built the
  layer-button font's resource path with `MakeFullpath()`. On Linux that gives
  `/fonts/xtrackcad-10.bdf`, but on Windows it gives `/\fonts/\xtrackcad-10.bdf`, the lookup
  failed, and `wFTLabelLoadFontFromResource()` aborted every Windows build at startup. Nothing
  caught it, since CI ran no Windows binary past its unit tests. The Windows ZIP install test
  (`install-test-windows` in `install-tests.yml`) found it. Build resource paths with `/` explicitly
  (`DynStringPrintf(&path, "%sfonts/%s", XTRKCAD_SYMBOLS_PATH, name)`).

## Confirmed-safe codebase conventions (not bugs, but non-obvious enough to document)

Unlike the section above, these are cases where deep verification confirmed the code is doing
exactly what it should — captured because the shape looks suspicious enough on first read (or on
a static-analysis tool's flag) that a future session would otherwise re-derive the same answer
from scratch, or worse, "fix" something that isn't broken.

- **`trackGauge` used to be deliberately re-declared as a local in every track-type-specific
  `Draw*Track` function, shadowing the module-global `DIST_T trackGauge` (`track.c`) — no longer
  true as of the shadowVariable full re-audit (SF #764/#765/#766, 2026-09-01).** The global holds
  whatever gauge is "current" for new-track creation; each `Draw*Track` function instead wants the
  *specific track it's drawing*'s own gauge, so it used to shadow the name with
  `DIST_T trackGauge = GetTrkGauge(trk);` (or, in `cmodify.c`'s `CmdModify()`, a `static` version
  persisted across a modify-command's mouse-drag lifecycle, reset to `0.0` on `C_START`) — present
  and correctly assigned-before-use in `cmodify.c`, `tcurve.c`, `track.c`, `tstraigh.c`, and all 3
  of `turnout.c`'s instances (SF #721), each individually suppressed with a
  `// cppcheck-suppress shadowVariable` since `cppcheck-deep` had no way to know these were
  different gauges by design. The audit eliminated the shadow entirely instead of continuing to
  suppress it — renamed to `trkGauge` (or `modTrackGauge` in `cmodify.c`'s static case) everywhere
  it recurred. Zero `cppcheck-suppress shadowVariable` comments remain in any of these files now.
  One rename was incomplete on first pass — two sites in `track.c`'s `DrawEndPt()` were missed and
  kept reading the now-unrelated global, a real behavior change once the shadow was gone; fixed
  separately (SF #767).
- **A `static` local persisting mouse-command state across calls, sharing a name with something
  else at file or global scope, is this codebase's standard idiom for multi-step command
  dispatch** (`CmdModify`, `CmdDraw`, and similar `C_START`/`C_MOVE`/`C_UP`-style dispatchers). The
  static survives between the separate invocations that make up one drag/click sequence and is
  reset explicitly on `C_START`; `cdraw.c`, `ccurve.c`, `ccornu.c`, and `cjoin.c` each declare their
  own independent `static BOOL_T infoSubst` for this purpose — these are unrelated, fully
  self-contained per-function statics that merely share a generic name, not a cross-function
  interaction. (`cdraw.c`'s `CmdDraw()` renamed its own copy to `drawInfoSubst` during the
  shadowVariable full re-audit, SF #765 — the file also has a separate, untouched file-scope
  `infoSubst` used elsewhere, so the file still has "an" `infoSubst`, just not the one `CmdDraw()`
  itself uses.)
- **`UndoStart()` (`cundo.c`) calls `SetFileChanged()` internally** — a caller that opens an undo
  transaction (`UndoStart()` → `UndoModify()`/`UndoNew()`/`UndoDelete()` → implicit close) does
  **not** need to call `SetFileChanged()` itself; the layout-dirty flag (`layout.c`'s
  `EXPORT wIndex_t changed`) is already marked the moment a real (non-empty) transaction begins.
  The handful of call sites that *do* call `SetFileChanged()` directly (`textnoteui.c`,
  `linknoteui.c`, `filenoteui.c`, `dlayer.c`) are redundant-but-harmless, not evidence that it's
  otherwise required. Discovered while verifying `cblock.c`'s `UpdateBlock()` — a dialog handler
  with a local `BOOL_T changed` (renamed `blockChanged` by the shadowVariable full re-audit,
  SF #765 — it shadowed the global of the same name at the time this was investigated) and *no*
  direct `SetFileChanged()` call anywhere in the file, which looked like a real "renaming a block
  doesn't mark the file dirty" bug until tracing `UndoStart()`'s own body settled it.
