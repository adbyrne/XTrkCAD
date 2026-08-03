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

`release.yml`'s `package-docs` and release-packaging jobs are deliberately scoped to pushes on
`GTK3V2MAIN`/`main` specifically (see that workflow's own `on:` block) — they won't fire on an
arbitrary feature branch on your fork. That's expected: those two jobs build release artifacts
for the project maintainer's own release process, not per-developer CI, so there's nothing to set
up for them here.

# CI tooling overview {#ci-tooling-overview}

This branch's CI is `.github/workflows/ci-gtk3.yml` (`ci.yml` covers the GTK2 `%main` branch,
`codeql.yml` and `release.yml` are separate workflows — see below). Read the workflow file
directly for exact command lines; this is a map of what each job checks and whether it can fail
your run, not a copy of it.

## Gating jobs (a failure here fails the run)

- **`doxygen`** — builds this documentation with the pinned Doxygen 1.15.0 and `WARN_AS_ERROR`;
  any warning (a broken `\ref`, a stale `@param`, an unresolved anchor) fails the job.
- **`c-tests-linux`, `c-tests-arm64`, `c-tests-linux-clang`, `c-tests-macos`,
  `c-tests-windows-msys2`, `c-tests-sanitizers`, `c-tests-valgrind`** — build plus `ctest` across
  compilers (GCC, Clang, AppleClang, MinGW GCC), platforms, and architectures, plus ASan/UBSan and
  Valgrind runs (`PreferenceTest` excluded from both — see
  \ref advanced-optional-local-checks "Advanced/optional local checks" in the
  \ref index "Developer Documentation" page for why). All pass `-LE regression` so the demo
  regression suite (see `regression-gtk3` below) isn't picked up here too.
- **`regression-gtk3`** — replays the full demo suite (`RegressionTestAll()`, the `-T` flag) in
  one process via the `RegressionSuite` `ctest` target (see
  \ref advanced-optional-local-checks "Advanced/optional local checks" in the
  \ref index "Developer Documentation" page for the full `XTRKCAD_REGRESSION_TESTING` writeup).
  Kept out of `c-tests-linux` rather than folded in, since its ~9-minute single-process playback
  baseline would slow down that job's fast unit-test feedback loop.
- **`cppcheck`** — cppcheck's default check set only, `--error-exitcode=1`.
- **`astyle-check`** — AStyle 3.6.13 (built from source, pinned — an unpinned version once
  produced different formatting for identical input, SF #638) dry-run conformance to
  `app/lib/astylerc`, scoped to `app/bin`/`app/wlib` only. Vendored code
  (`app/wlib/gtk3lib/wrapbox/`, `app/tools/halibut/`) and `unittest/` directories are excluded by
  design; `app/bin/cars/` is a temporary exclusion for in-progress work (see the job's own
  comments and `bug-tracker.md` for current status).
- **`fuzz-getargs`** — a bounded-time libFuzzer regression run of the `fuzz_getargs` harness (not
  continuous fuzzing; catches regressions in previously-fixed `GetArgs()` bugs like SF #645).

## Report-only jobs (upload an artifact, never fail the run)

- **`cppcheck-deep`** — cppcheck's full check set, beyond the gating job's defaults.
- **`clang-tidy`** — a conservative `bugprone-*`/`performance-*`/`portability-*` check-set.
- **`codespell`** — scans comments, strings, and identifiers in `app/bin`/`app/wlib` for common
  misspellings.
- **`astyle-check-macos`, `astyle-check-windows`** — compare each platform's from-source-built
  astyle output against Linux's, to catch astyle itself behaving differently per OS (not code
  formatting drift — that's what the gating `astyle-check` job is for).
- **`compiler-warnings`, `compiler-warnings-macos`, `compiler-warnings-windows`** — captures
  `-Wall -Wextra` build output as an artifact on each platform.
- **`coverage`** — generates an lcov HTML report and uploads it as an artifact.

Every report-only job's findings land as a downloadable artifact on the workflow run's summary
page (Actions tab → the run → Artifacts); each job's own "Upload findings"/"Upload warning log"
step names it. **CodeQL** is a separate workflow (`codeql.yml`) — it runs on push/PR and a weekly
Monday cron, and its results show up in the repo's Security tab rather than as an artifact.

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

The large-volume cppcheck/clang-tidy categories that Phase 13 sampled and decided *not* to
bulk-suppress (`variableScope`, `bugprone-narrowing-conversions`, and others) are deliberately
left visible in the `cppcheck-deep`/`clang-tidy` report-only artifacts instead — see
\ref code-quality-process-and-patterns "Code quality: process and patterns" below for why.

# Packaging and versioning {#packaging-and-versioning}

`ProgramVersion.cmake` defines `XTRKCAD_VERSION` from three plain numbers
(`XTRKCAD_MAJOR_VERSION.XTRKCAD_MINOR_VERSION.XTRKCAD_RELEASE_VERSION`, plus an optional
`XTRKCAD_VERSION_MODIFIER` suffix like `Dev`), then — on this branch only — extends it with two
more layers so that different builds of GTK3V2MAIN can coexist on one machine instead of silently
overwriting each other.

## Per-version identity (SF #673)

`XTRKCAD_BIN` (`CMakeLists.txt`), set to `xtrkcad-${XTRKCAD_VERSION}`, is the single source of
truth for a build's on-disk identity: the installed binary name, the `.deb`/`.rpm`/NSIS package
name, the `.desktop` filename, the share/gettext-domain directory name, and (via the same
`XTRKCAD_VERSION` string reused in `app/bin/custom.c`) the runtime work/prefs directory. Before
this, all of those were the fixed literal `xtrkcad`/`xtrkcad-beta` on every branch, so installing
a GTK3V2MAIN 5.4.0 build over an existing GTK2 5.3.x install looked like an in-place upgrade to
the package manager and silently clobbered it. Embedding the version means two different point
releases now install and run side by side.

## Dev-build hash suffix

Distinct point releases weren't the whole problem: every ordinary push to GTK3V2MAIN mainline
between tagged releases still carries the *same* `XTRKCAD_VERSION` (there's no version bump
per-commit), so successive dev builds would still collide with each other under the scheme above.
`ProgramVersion.cmake` closes that gap by appending a fourth, dot-separated component — a short
VCS hash of the checked-out commit, e.g. `5.4.0.8b67f185` — to any build that isn't the exact
commit a `v<version>` tag points at. The check works from either a git or an Hg checkout (detected
by the presence of `.git`/`.hg` under `CMAKE_SOURCE_DIR`), since this file is shared by both
source trees (see the \ref index "Developer Documentation" page's note on SourceForge Hg vs. the
GitHub git mirror).

A tagged release build — `git tag v5.4.1 && git push origin v5.4.1`, see `release.yml`'s
`github-release` job — resolves to the plain three-part version, no hash. The release workflow is
otherwise unchanged: after tagging, bump `XTRKCAD_RELEASE_VERSION` (and `XTRKCAD_VERSION_MODIFIER`
if used) for the next dev cycle in the same commit or a prompt follow-up — every build after that
picks the hash suffix back up automatically, with no separate manual toggle needed per commit.

# Code quality: process and patterns {#code-quality-process-and-patterns}

CI runs several static-analysis tools on this branch (AStyle formatting, CodeQL, `cppcheck-deep`,
clang-tidy, a `fuzz_getargs` libFuzzer harness), some gating and some report-only, plus a
warning-count ratchet that locks in categories already brought to zero so they can't silently
regress. Working through their findings across many sessions converged on a process, and turned up
several defect classes that kept recurring — both captured here for whoever picks up the next
batch of findings.

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
- **The branch/ticket pipeline for a fix found this way:** SF ticket → Hg bug branch (stacked on
  any unmerged prior work it depends on) → git PR → CI green → merge → a review window (roughly
  2 days, case by case) → Hg merge. A CI-configuration-only change with no application code (a new
  report-only job, a workflow tweak) can skip the Hg bug branch entirely, since Hg has nothing to
  carry — but it still gets an SF ticket.

## Bug patterns found in this codebase

Real defect classes this project's history has actually hit, not a hypothetical checklist — each
one below was found by the static-analysis process above and confirmed by reading the code, not
inferred from a tool's message alone.

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
