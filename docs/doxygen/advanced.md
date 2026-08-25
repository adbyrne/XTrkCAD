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
- **`regression-gtk3-sanitizers`** — the same `RegressionSuite` run as `regression-gtk3`, but
  ASan/UBSan-instrumented (mirrors `c-tests-sanitizers`' configure line). Closes the SF #682 gap:
  `c-tests-sanitizers` excludes regression (`-LE regression`) and `regression-gtk3` isn't
  sanitizer-instrumented, so the combination that found SF #675/#677–#680 previously only happened
  when someone ran it locally — see the "Static tools and a live sanitizer run" bullet below.
- **`cppcheck`** — cppcheck's default check set only, `--error-exitcode=1`.
- **`astyle-check`** — AStyle 3.6.13 (built from source, pinned — an unpinned version once
  produced different formatting for identical input, SF #638) dry-run conformance to
  `app/lib/astylerc`, scoped to `app/bin`/`app/wlib` only. Vendored code
  (`app/wlib/gtk3lib/wrapbox/`, `app/tools/halibut/`) and `unittest/` directories are excluded by
  design.
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

Five of these (`coverage`, `cppcheck-deep`, `clang-tidy`, `codespell`, `compiler-warnings`) are
additionally published as permanent, browsable HTML at
[adbyrne.github.io/XTrkCAD](https://adbyrne.github.io/XTrkCAD/), redeployed on every push to
`GTK3V2MAIN` by `release.yml`'s `package-coverage`/`package-cppcheck-deep`/`package-clang-tidy`/
`package-codespell`/`package-compiler-warnings` jobs (separate from the `ci-gtk3.yml` jobs of the
same underlying command, which stay PR-artifact-only for fast per-PR feedback). The four plain-text
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
`ci-gtk3.yml`'s (or `release.yml`'s, identical) job definition — copy the `run:` step verbatim,
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
tools/format-findings-report.sh <input.txt> <output.html> "<title>" <bracketed|codespell> <blob-base-url>
```

`bracketed` handles cppcheck/clang-tidy/compiler-warning lines ending in `[category]`;
`codespell` handles its `file:line: word ==> suggestion` shape instead. `<blob-base-url>` is
prepended to each finding's `file#Lline` to build its link — pass a real GitHub blob URL (e.g.
`https://github.com/adbyrne/XTrkCAD/blob/$(git rev-parse HEAD)/`) to get working links against
your own checkout, or omit meaningful linking entirely by passing any placeholder if you just
want the grouped local HTML view. The `coverage` report doesn't go through this script — `genhtml`
already produces its own linked, per-line HTML.

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
  `nm <binary> | grep <symbol>` before assuming the edit landed.
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

## Confirmed-safe codebase conventions (not bugs, but non-obvious enough to document)

Unlike the section above, these are cases where deep verification confirmed the code is doing
exactly what it should — captured because the shape looks suspicious enough on first read (or on
a static-analysis tool's flag) that a future session would otherwise re-derive the same answer
from scratch, or worse, "fix" something that isn't broken.

- **`trackGauge` is deliberately re-declared as a local in every track-type-specific `Draw*Track`
  function, shadowing the module-global `DIST_T trackGauge` (`track.c`).** The global holds
  whatever gauge is "current" for new-track creation; each `Draw*Track` function instead wants the
  *specific track it's drawing*'s own gauge, so it shadows the name with
  `DIST_T trackGauge = GetTrkGauge(trk);` (or, in `cmodify.c`'s `CmdModify()`, a `static` version
  persisted across a modify-command's mouse-drag lifecycle, reset to `0.0` on `C_START`). Confirmed
  present and correctly assigned-before-use in `cmodify.c`, `tcurve.c`, `track.c`, `tstraigh.c`,
  and all 3 of `turnout.c`'s instances (SF #721) — same intentional pattern everywhere it recurs,
  not a one-off. `cppcheck-deep`'s `shadowVariable` check has no way to know these are different
  gauges by design; each site carries a `// cppcheck-suppress shadowVariable` explaining this.
- **A `static` local persisting mouse-command state across calls, sharing a name with something
  else at file or global scope, is this codebase's standard idiom for multi-step command
  dispatch** (`CmdModify`, `CmdDraw`, and similar `C_START`/`C_MOVE`/`C_UP`-style dispatchers). The
  static survives between the separate invocations that make up one drag/click sequence and is
  reset explicitly on `C_START`; several files (`cdraw.c`, `ccurve.c`, `ccornu.c`, `cjoin.c`) each
  declare their own independent `static BOOL_T infoSubst` for this purpose — these are unrelated,
  fully self-contained per-function statics that merely share a generic name, not a cross-function
  interaction.
- **`UndoStart()` (`cundo.c`) calls `SetFileChanged()` internally** — a caller that opens an undo
  transaction (`UndoStart()` → `UndoModify()`/`UndoNew()`/`UndoDelete()` → implicit close) does
  **not** need to call `SetFileChanged()` itself; the layout-dirty flag (`layout.c`'s
  `EXPORT wIndex_t changed`) is already marked the moment a real (non-empty) transaction begins.
  The handful of call sites that *do* call `SetFileChanged()` directly (`textnoteui.c`,
  `linknoteui.c`, `filenoteui.c`, `dlayer.c`) are redundant-but-harmless, not evidence that it's
  otherwise required. Discovered while verifying `cblock.c`'s `UpdateBlock()` — a dialog handler
  with a local `BOOL_T changed` shadowing the global of the same name and *no* direct
  `SetFileChanged()` call anywhere in the file, which looked like a real "renaming a block doesn't
  mark the file dirty" bug until tracing `UndoStart()`'s own body settled it.
