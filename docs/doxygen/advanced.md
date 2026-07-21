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
