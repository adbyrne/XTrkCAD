# Developer Documentation

Model railroad layout design and operation software.

XTrkCAD lets modelers design track plans on a scaled grid, generate accurate turnout templates
for printing, produce parts lists, and generate operating documentation for a finished layout.
This is the developer-facing documentation, generated from source comments with
[Doxygen](https://www.doxygen.nl/) — it documents the code, not how to use the application.
Everything here has been checked against the current `GTK3V2MAIN` source rather than assumed —
see inline notes where something changed since it was first written up.

\tableofcontents

## Source layout

XTrkCAD is a C application built from several libraries that link into one executable:

- **`app/bin/`** — the core application (~105K lines of C). Nearly all application logic lives
  here: track objects (`track.c`/`track.h`), drawing and geometry (`draw.c`, `drawgeom.c`),
  commands (the `c*.c` files — `ccurve.c`, `cturnout.c`, `cselect.c`, `cgroup.c`, and others),
  file I/O (`fileio.c`, `paramfile.c` — the native `.xtc`/`.xtce` format), DXF import/export,
  undo/redo (`cundo.c`), scale definitions (`scale.c`), and car/consist management (`cars/`).
- **`app/wlib/`** — the UI abstraction layer. Application code calls `wlib`'s platform-neutral
  API and never the underlying toolkit directly. On this branch (`GTK3V2MAIN`) the backend is
  `gtk3lib/` (GTK+ 3).
- **`app/cornu/`** — Spiro/Cornu easement-curve calculations.
- **`app/dynstring/`** — a safe, growable C string type used throughout the codebase.
- **`app/cJSON/`** — an embedded JSON library, used for structured data such as car definitions.
- **`app/lib/`** — runtime resources: parameter files, example layouts, demo scripts.

## Where to start reading

`track.c`/`track.h` (the base track type) and `draw.c` (the drawing and geometry core) are the
two files most of the rest of the application builds on. Coding conventions, error-handling
patterns, and how to build and test locally on each platform follow below — or use the search box
or the file/class browser in the sidebar to jump straight to a specific symbol instead of reading
top to bottom.

## Source naming convention

Source files loosely follow a naming convention, not strictly applied everywhere:

- **`c<object>.c`** — the code and data structures for creating, manipulating, and deleting an
  object (similar to a class file). Example: `ccurve.c`.
- **`d<object>.c`** — the dialog(s) for creating, changing, and deleting the object, where one
  exists.
- **`t<object>.c`** — drawing the object. Example: `tcurve.c` (curved track has no dialog, so
  there's no corresponding `d`-file).

## Casts

Casts are generally avoided; there's no need to cast to or from `void *` directly. A couple of
macro pairs exist for the cases that do need one:

- **`I2VP(val)`** / **`VP2L(val)`** (`common.h`) — round-trip an integral value through a `void *`
  callback context and back. Used when a callback needs to carry a small integer instead of a
  real pointer: `I2VP()` when creating the control, `VP2L()` when reading the value back out in
  the callback.

Downcasting a wide numeric type to a narrower one (including float-to-integer) is one of the few
other places a cast is legitimate — the narrowing should be deliberate and visible at the call
site, not implicit.

## Error handling

Four categories, each with a different mechanism:

- **User input errors** — invalid dialog entries, invalid object selection. `ErrorMessage()`
  writes to the info bar with an optional beep (recoverable by moving the mouse or picking a
  different object); `NoticeMessage()` pops a dialog requiring a button press, used when a choice
  is needed (e.g. confirming a delete). Both take a `MSG_*` identifier as their first argument,
  which ties into `app/help/messages.in` — each entry there defines a user-facing title, an
  optional `ALT` (the title with `printf` placeholders already filled in), and a `HELP` block
  with a longer explanation; this file is what drives Help → Recent Messages. `"Ptr == NULL"` is
  not a useful user-facing error message — explain the problem and the fix in the user's terms.
- **File input errors** — corrupt or unrecognized-feature files. `InputError()` (declared in
  `app/bin/getargs.c`) offers the option of aborting the rest of the file load. Layout files carry
  a version number specifically so that a non-backwards-compatible format change is caught
  cleanly rather than silently misparsed.
- **Logic errors** — null pointer checks, index range checks, "can't happen" conditions.
  `CHECKMSG(cond, msg)` calls `AbortProg()` if `cond` is false, which prompts to save the current
  layout before aborting (and skips straight to abort without asking again if a second logic
  error fires during that save — see `AbortProg()` in `misc.c`). These indicate a bug in the code,
  not something the user did wrong; the message doesn't need to be translated.
- **System/environmental errors** — out-of-memory and similar. Exit as soon as possible rather
  than trying to continue in a state that can't be trusted.

## `GetArgs()` extensibility

Most track-segment and endpoint records are read with `GetArgs()` (`app/bin/getargs.c`), using a
format-code string similar to `scanf()`. Two format codes make the on-disk format
forward/backward-compatible without a version bump for every small addition:

- **`Y`/`Z`/`X`** — read nothing from the input; the corresponding argument is just set to a
  default value. Used to fill in a field that an *older* file format doesn't carry.
- **`c`** — instead of consuming a value, stores a pointer to whatever text is left unparsed on
  the line into the corresponding `char **` argument (or `NULL` if nothing remains). A caller can
  then check that pointer and, if non-`NULL`, run a second `GetArgs()` over it to read fields a
  *newer* file format added. An old build simply never looks at the leftover text and ignores the
  addition; a new build reading an old file gets `NULL` and skips the optional second read.

`GetEndPtArg()` in `app/bin/trkendpt.c` is a good current example — it always reads a
`"pfc"` (position, angle, then "whatever's left") for every endpoint, then only reads the next
chunk (`"lpc"`, elevation-related fields) if the caller indicates the file may have the newer
"improved ends" data:

```c
if ( !GetArgs( cp, "pfc", &e->pos, &e->angle, &cp) ) { /* ... */ }
if (bImprovedEnds) {
    if (!GetArgs( cp, "lpc", &option, &e->elev.doff, &cp )) { /* ... */ }
    /* ... */
}
```

## `wlib` interaction

`wlib` (`app/wlib/`) packages the interface between the core application and the underlying
windowing toolkit. All application code goes through `wlib`'s platform-neutral API — never GTK
directly. On this branch (`GTK3V2MAIN`) the only backend is `app/wlib/gtk3lib/` (GTK+ 3); a
Win32 backend also exists in the source tree for the Windows build (see `app/wlib/mswlib/`).

`wlib` calls create and interact with window objects — `wButton`, `wEntry`, `wDraw`, and others,
collectively `wControl` (`app/wlib/include/wlib.h`). Most of these notify application code of
events (a button press, text entry) via a callback function pointer supplied when the object is
created, along with a `void *` context value — see the cast macros above for the common pattern
of packing a small integer into that context instead of a real pointer.

## Building and testing

Not sure what's already installed? Run `tools/check-build-deps.sh` first (Linux, macOS, and
Windows via an MSYS2 MINGW64 shell all work — it detects which platform it's on). It's read-only
— it reports what's missing and the exact command to install it, without installing or changing
anything itself.

The commands below are what CI actually runs, with the CI-only bits called out — a local desktop
build doesn't need everything CI needs. The full, authoritative CI configuration lives in
`.github/workflows/` (`ci-gtk3.yml` for this branch, plus `ci.yml` for the GTK2 `main` branch,
`codeql.yml`, and `release.yml`) — worth reading directly if you have a GitHub account and want
more detail than the summary below, e.g. exact job-by-job dependency lists or the release
packaging steps. If you're working purely from the SourceForge/Hg checkout, these files are
still present in this tree even though Mercurial itself doesn't run them — GitHub Actions only
executes off a `git` mirror, e.g. `github.com/$GITHUB_USER/XTrkCAD` (see
[Running CI on your own GitHub fork](#running-ci-on-your-own-github-fork) below for how to set
one up).

### Linux (x86_64 and ARM64)

```sh
sudo apt-get install -y cmake ninja-build libgtk-3-dev libcmocka-dev zlib1g-dev libzip-dev \
    libmxml-dev librsvg2-bin gettext
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DXTRKCAD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

CI adds `xvfb` and runs tests under `xvfb-run` because its runners have no display; a normal
desktop session doesn't need that wrapper. The ARM64 and Clang (`-DCMAKE_C_COMPILER=clang`)
variants use the identical steps otherwise.

### macOS

```sh
brew install ninja gtk+3 cmocka libzip librsvg
GTK_PREFIX=$(brew --prefix gtk+3)
export PKG_CONFIG_PATH="$GTK_PREFIX/lib/pkgconfig:$(brew --prefix)/lib/pkgconfig"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DXTRKCAD_TESTING=ON \
    -DCMAKE_PREFIX_PATH="$GTK_PREFIX;$(brew --prefix)" \
    -DCMAKE_EXE_LINKER_FLAGS="$(pkg-config --libs-only-L gtk+-3.0)" \
    -DXTRKCAD_USE_APPLEHELP=OFF -DXTRKCAD_USE_BROWSER=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### Windows (MSYS2/MinGW64)

Run from an MSYS2 MINGW64 shell. This is GTK3V2MAIN's only supported Windows path — the older
MSVC/`mswlib` build was removed from this branch (SF r6508); `app/wlib/mswlib/` in the source
tree is historical/`default`-branch code, not what this branch links against.

```sh
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
    mingw-w64-x86_64-pkg-config mingw-w64-x86_64-gtk3 mingw-w64-x86_64-zlib \
    mingw-w64-x86_64-libzip mingw-w64-x86_64-mxml mingw-w64-x86_64-librsvg \
    mingw-w64-x86_64-gettext mingw-w64-x86_64-cmocka
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DXTRKCAD_TESTING=ON -DXTRKCAD_USE_GETTEXT=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### Advanced/optional local checks

Not part of the everyday build/test loop, but available if you're chasing a specific class of
bug:

- **Sanitizers** — `-DXTRKCAD_SANITIZE=ON -DCMAKE_C_COMPILER=clang` builds with ASan/UBSan.
  `PreferenceTest` is excluded when running under it (GTK3's own internal allocations trip
  ASan's leak detector; that test still runs normally elsewhere).
- **Valgrind** — `ctest -T memcheck --overwrite MemoryCheckCommand=$(which valgrind)`.
  `PreferenceTest` is excluded here too, for the same GTK3-internals reason.

### Running with debug logging (`-d`) {#running-with-debug-logging}

`ctest` verifies unit-testable logic; it can't confirm an interactive-only fix (a dialog, a draw
path, a mouse-driven command) actually behaves correctly. For that, run the real app with its
built-in per-module debug logging: `-d <category>=<level>` (repeat the flag once per category;
`-d <category>` alone defaults to level 1), optionally with `-l <file>` to redirect output to a
file instead of stdout. There's no `-d help`/discovery flag — see the \ref log "Log Commands"
page for the full catalog of category names and which file registers each one. That page is
generated from `@logcmd` doc comments next to each category's registration in the source (the
same `\xrefitem`-based mechanism Doxygen uses for its own Todo/Bug/Deprecated lists), so it stays
in sync with the code; if a category you need isn't listed there, its registration is missing the
annotation, not necessarily missing outright — grep `app/bin/` for `LogFindIndex("<category>")`
to confirm either way.

**Adding a new category**, e.g. `foo` in `cfoo.c`:

```
/** @logcmd @showrefby `foo=n` `cfoo.c` — what this actually logs, if not obvious from the name */
static int log_foo = 0;
```

Keep the `category=n` syntax and the filename each in their own backtick-span — that's what keeps
the generated \ref log "Log Commands" list readable as a table-like scan instead of a run-on
sentence; the free-text description after the em dash is optional and can be left off entirely for
a self-explanatory category.

then, in whichever function already runs once at startup for that module (an `Init*()` the app
calls on launch — see `InitTrkStraight()` in `tstraigh.c` for the simplest example), register it:

```c
log_foo = LogFindIndex( "foo" );
```

If the module has no such single init point (a plain utility file with several independent entry
points, not an object with a lifecycle), use a one-time lazy-init guard at the top of the entry
point instead — see `log_zipInitted` in `archive.c`. Either way, wrap the actual debug output in
`LOG( log_foo, <level>, ( "format...\n", args... ) )`; the macro is a no-op until someone passes
`-d foo=<level>` on the command line. The `@logcmd` comment is what makes the category show up on
the \ref log "Log Commands" page — the `LogFindIndex()` call is what makes `-d foo=N` actually do
anything; a category needs both; either one alone would recreate one of the four dead/undocumented
categories fixed in the same session that finished this catalog.

**A plain `cmake --build` isn't enough to run the result interactively.** At startup the binary
locates its resource directory — `xtrkcad.xtq`, `xtrkcad.tip`, the `icons16/24/32.gresource`
files, param files, locale `.mo` files — via `wGetAppLibDir()`
(`app/wlib/gtk3lib/unix/ixpaths.c`), which checks the `XTRKCADGTKLIB` env var first, then
`<binary-dir>/../share/xtrkcad-gtk` relative to the running executable, then falls back to
`/usr/share`/`/usr/local/share`. None of that layout exists in a raw out-of-source build
directory — those files are placed only by CMake's `install()` rules, not by `cmake --build` — so
running `build-gtk3v2main/app/bin/xtrkcad` directly fails to find them. Do a real (scratch)
install first:

```sh
cmake --build build-gtk3v2main
cmake --install build-gtk3v2main --prefix build-gtk3v2main/install
build-gtk3v2main/install/bin/xtrkcad -d join=3
```

With `--prefix` placed one level above `bin/` like this, the `../share/xtrkcad-gtk` relative
lookup finds everything automatically. If the scratch prefix lives somewhere else instead (e.g.
`/tmp`), point `XTRKCADGTKLIB` at the installed `share/xtrkcad-gtk` directory explicitly:

```sh
cmake --install build-gtk3v2main --prefix /tmp/xtrkcad-scratch
XTRKCADGTKLIB=/tmp/xtrkcad-scratch/share/xtrkcad-gtk /tmp/xtrkcad-scratch/bin/xtrkcad -d join=3
```

### Tool versions

Doxygen and AStyle are explicitly version-pinned in CI; everything else uses whatever the CI
runner image currently provides. The AStyle pin exists specifically because an unpinned version
once produced different formatting output for identical input (SF #638) — if you're chasing a
formatting or doc-generation mismatch against CI, matching these two matters more than matching
anything else in this table.

| Tool | Pin status | Currently tested (as of 2026-07-18 — unpinned rows drift as CI runner images update) |
|------|-----------|------------------------------------------|
| Doxygen | pinned | 1.15.0, all platforms |
| AStyle | pinned | 3.6.13, all platforms (built from source, not the OS package) |
| CMake | floor/ceiling only (`3.20`–`4.0`) | 3.28.3 (Linux), 4.3.3 (Windows/MSYS2) |
| cppcheck | unpinned | 2.13.0 (Ubuntu 24.04) |
| clang-tidy | unpinned | 18.0 (Ubuntu 24.04) |
| GCC (Linux) | unpinned | 13.3.0 (Ubuntu 24.04.4 LTS) |
| Clang (Linux) | unpinned | 18.0 (Ubuntu 24.04) |
| AppleClang (macOS) | unpinned | 21.0.0.21000101 (macOS 26.4) |
| MinGW GCC (Windows) | unpinned | 16.1.0 (via MSYS2) |

To reproduce the pinned Doxygen build locally rather than whatever version your package manager
provides:

```sh
curl -sL -o doxygen.tar.gz \
    "https://github.com/doxygen/doxygen/releases/download/Release_1_15_0/doxygen-1.15.0.linux.bin.tar.gz"
tar xzf doxygen.tar.gz
export PATH="$PWD/doxygen-1.15.0/bin:$PATH"
```

AStyle isn't available as a prebuilt binary from upstream, so CI builds it from source rather
than trusting the OS package (that's the whole point of the pin — see the note above). Reproduce
it the same way:

```sh
ASTYLE_VERSION=3.6.13
curl -sL -o astyle.tar.bz2 \
    "https://sourceforge.net/projects/astyle/files/astyle/astyle%20${ASTYLE_VERSION%.*}/astyle-${ASTYLE_VERSION}.tar.bz2/download"
tar xjf astyle.tar.bz2
make -C "astyle-${ASTYLE_VERSION}/build/gcc" -j"$(nproc)"
export PATH="$PWD/astyle-${ASTYLE_VERSION}/build/gcc/bin:$PATH"
```

## Running CI on your own GitHub fork {#running-ci-on-your-own-github-fork}

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

## Code quality: process and patterns {#code-quality-process-and-patterns}

CI runs several static-analysis tools on this branch (AStyle formatting, CodeQL, `cppcheck-deep`,
clang-tidy, a `fuzz_getargs` libFuzzer harness), some gating and some report-only, plus a
warning-count ratchet that locks in categories already brought to zero so they can't silently
regress. Working through their findings across many sessions converged on a process, and turned up
several defect classes that kept recurring — both captured here for whoever picks up the next
batch of findings.

### Static-analysis workflow

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

### Bug patterns found in this codebase

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
  logging" above), not just by inspection.
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
