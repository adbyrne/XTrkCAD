\page building Building and Testing

Not sure what's already installed? Run `tools/check-build-deps.sh` first (Linux, macOS, and
Windows via an MSYS2 MINGW64 shell all work — it detects which platform it's on). It's read-only
— it reports what's missing and the exact command to install it, without installing or changing
anything itself.

The commands below are what CI actually runs, with the CI-only bits called out — a local desktop
build doesn't need everything CI needs. The full, authoritative CI configuration lives in
`.github/workflows/` (`ci-gtk3.yml` for the main CI, `ci-gtk3-preview.yml` for its
next-toolchain preview, plus `codeql.yml`, `release.yml`, and
`hg-sync-default.yml`) — worth reading directly if you have a GitHub account and want
more detail than the summary below, e.g. exact job-by-job dependency lists or the release
packaging steps. If you're working purely from the SourceForge/Hg checkout, these files are
still present in this tree even though Mercurial itself doesn't run them — GitHub Actions only
executes off a `git` mirror, e.g. `github.com/$GITHUB_USER/XTrkCAD` (see
\ref running-ci-on-your-own-github-fork "Running CI on your own GitHub fork" in the
\ref advanced "Advanced Topics" page for how to set one up).

\tableofcontents

# Linux (x86_64 and ARM64) {#linux-build-test}

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

This plain `ctest` also picks up the demo-playback regression tests: `XTRKCAD_REGRESSION_TESTING`
auto-enables on Linux whenever `DISPLAY` is set (any normal desktop session) or `xvfb-run` is
found, with no extra flag needed. See the
\ref advanced-optional-local-checks "Regression demo-playback suite" note below for a known
false-failure case (gtk3issues #21) this can surface.

For flatpak information, see \subpage flatpak.

# macOS

For the Homebrew-dependent package, file locations and debugging on macOS, see \subpage macos.

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

# Windows (MSYS2/MinGW64)

For how Windows differs at run time (options start with `/`, file locations, the ZIP's
contents, pitfalls), see \subpage windows.

Run from an MSYS2 MINGW64 shell. This is the supported Windows path — the older
MSVC/`mswlib` build was removed (SF r6508) and `app/wlib/mswlib/` is gone from the source tree.
An experimental MSVC build on the GTK3 backend is described further down.

```sh
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
    mingw-w64-x86_64-pkg-config mingw-w64-x86_64-gtk3 mingw-w64-x86_64-zlib \
    mingw-w64-x86_64-libzip mingw-w64-x86_64-mxml mingw-w64-x86_64-librsvg \
    mingw-w64-x86_64-gettext mingw-w64-x86_64-cmocka
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DXTRKCAD_TESTING=ON -DXTRKCAD_USE_GETTEXT=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Experimental: MSVC with gvsbuild {#windows-msvc-gvsbuild}

A second Windows toolchain, still being tried out: Visual Studio's compiler (`cl.exe`) against a
GTK3 tree built by [gvsbuild](https://github.com/wingtk/gvsbuild). It uses the same wlib backend as
MinGW; only the compiler and where the libraries come from differ.

1. Build GTK3 and the other libraries with gvsbuild, in its release configuration:
   `gvsbuild build gtk3 adwaita-icon-theme hicolor-icon-theme librsvg zlib`. librsvg brings
   the gdk-pixbuf loader for SVG icons, gtk3 alone doesn't. Keep GTK4 out of this output
   directory, the two clash. libzip is found either in the gvsbuild tree (add
   `libzip` to that command) or, as for the old MSVC build, in `%XTCEXTERNALROOT%\x64\libzip`
   with `include` and `lib` below it.
2. Set `GVSBUILD` to gvsbuild's output directory, usually `C:\gtk-build\gtk\x64\release`.
3. Install a current CMake (4.0 or newer, e.g. `winget install Kitware.CMake`). The CMake bundled
   with Visual Studio is usually too old for the presets, and the Developer prompt puts it first
   on the path: check with `where cmake` and put the new one in front if needed.
4. From a *x64 Native Tools Command Prompt for VS*, which puts `cl.exe` and Ninja on the path:

```bat
cmake --preset windows-gvsbuild-release
cmake --build --preset windows-gvsbuild-release
cmake --install build\windows-gvsbuild-release --prefix C:\xtrkcad-msvc
```

For a Visual Studio solution, `cmake --preset windows-gvsbuild-vs` (from any prompt with
`GVSBUILD` set) writes `build\windows-gvsbuild-vs\XTrkCAD.sln` for the newest Visual Studio
installed. It has Release and RelWithDebInfo but no Debug configuration, for the C runtime reason
below. Building its `INSTALL` project installs to `build\install-windows-gvsbuild-vs`; run and
debug the program from there, since it only finds its data files in the installed layout.

Everything comes from gvsbuild's `pkgconf.exe`: the preset points CMake at it, and the build
takes the GTK prefix from it for the DLLs and the GTK runtime data that `cmake --install` copies
(GLib schemas, gdk-pixbuf loaders with the DLLs they need, such as librsvg and libxml2, the
hicolor and Adwaita icons, `etc\gtk-3.0`). There is no
Debug preset: GTK from gvsbuild links the release C runtime, and a Debug build would link the debug
one. `windows-gvsbuild-relwithdebinfo` gives you something to debug instead. gvsbuild has no CMocka,
so these presets build no unit tests. Translations are off, as in `windows-debug`.

The source files need nothing special for MSVC: `app/bin/include/win32/msvccompat.h` is
force-included with `/FI` and covers what `cl.exe` doesn't understand (`__attribute__`,
`strcasecmp`).

Things to check in the installed tree, where failures usually show up:

- the program starts from Explorer, not only from the command prompt (no DLL missing);
- toolbar icons and PNG images show (gdk-pixbuf loaders; if not, the `loaders.cache` paths may
  point into the gvsbuild tree and need regenerating with `gdk-pixbuf-query-loaders`);
- the file and colour dialogs open (GLib schemas, icon theme). If gvsbuild left no
  `share\glib-2.0\schemas\gschemas.compiled`, create it with its `glib-compile-schemas` and
  install again.

CI builds this way in `ci-windows-msvc.yml` (advisory for now, Development Issues #60). Instead
of running gvsbuild it unpacks gvsbuild's prebuilt GTK3 bundle (`GTK3_Gvsbuild_<version>_x64.zip`
from its GitHub releases), which has everything above except libzip; the job builds libzip
against the bundle's zlib into the same tree and caches both. It then packs the installed tree
into a ZIP like `release.yml`'s and runs the same install test,
`tools/package-install-test-windows.ps1`, so the checks in the list above are automated.

# CMake presets {#cmake-presets}

With CMake 4.0 or newer, `CMakePresets.json` at the top of the source tree replaces the configure
lines above. The build itself still needs only CMake 3.20; older versions refuse the presets with a
clear error, and plain `cmake -B build ...` keeps working without them. Each preset builds in
`build/<preset>/`:

| Preset | Host | Configures |
|--------|------|------------|
| `linux-debug` | Linux | Debug, tests on |
| `linux-release` | Linux | Release, tests on |
| `linux-clang-debug` | Linux | Debug with `clang`, tests on |
| `macos-debug` | macOS | Debug with the Homebrew paths and help options from the macOS section |
| `windows-debug` | MSYS2 MINGW64 | Debug with gettext off, as in the Windows section |
| `windows-gvsbuild-release` | Windows, MSVC | Experimental: Release against gvsbuild, tests off (see \ref windows-msvc-gvsbuild) |
| `windows-gvsbuild-relwithdebinfo` | Windows, MSVC | The same with debug information |
| `windows-gvsbuild-vs` | Windows, MSVC | A Visual Studio solution with Release and RelWithDebInfo |

`cmake --list-presets=all` shows the ones that apply to your host. A workflow preset of the same
name runs configure, build and the unit tests in one command:

```sh
cmake --workflow --preset linux-debug
```

Or step by step:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug        # unit tests; linux-debug-all adds the regression demos
                                  # when they are enabled (see below)
```

The test presets leave out the `regression` label, like the CI test jobs. On macOS, run
`eval "$(brew shellenv)"` first: `macos-debug` takes its paths from `HOMEBREW_PREFIX`. Personal
presets go in `CMakeUserPresets.json`, which git and Hg ignore. CI runs the `linux-debug`,
`macos-debug` and `windows-debug` workflows, the Linux one with CMake 4.0.7 (see
\ref ci-tooling-overview "CI tooling overview").

# Dev-build filename hash suffix

Every untagged dev build's *compiled-in* version string (About dialog, `--version` CLI output)
always carries a short VCS-hash fourth component, e.g. `5.4.0.8b67f185` — but by default its
*filename/install path* (binary name, work/prefs directory, package filenames) stays the plain
three-part version, so a new dev build just overwrites the previous one in place, matching what
a developer coming from the historic three-part version expects. Add
`-DXTRKCAD_APPEND_SRC_HASH=ON` to also give the filename/install path its own hash, so successive
dev builds of the same nominal version can install side by side instead of colliding — this is
what CI does automatically (SF #712; see `ProgramVersion.cmake` for the full
`XTRKCAD_VERSION`/`XTRKCAD_FILE_VERSION` split). A tagged release build is unaffected either
way — both variables drop the hash and stay the plain three-part version. Whether the version
is part of the install name at all is a separate switch; see
\ref release-defaults "Defaults for Release".

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DXTRKCAD_TESTING=ON -DXTRKCAD_APPEND_SRC_HASH=ON
cmake --build build
```

**To build one specific past commit:** check out the exact commit first, *then* explicitly
reconfigure — don't rely on `cmake --build` alone to notice the checkout, since CMake only
re-runs its configure step when a file it tracks (`CMakeLists.txt`, `ProgramVersion.cmake`)
actually changed on disk, so switching commits that don't touch those files leaves the
*previous* commit's hash silently baked into the build.

```sh
git checkout <commit-hash>                       # or: hg update -r <commit-hash>
cmake -B build -S . -DXTRKCAD_APPEND_SRC_HASH=ON  # force the reconfigure
cmake --build build
```

# Defaults for Release {#release-defaults}

A release has to replace the previous install when a user installs it, the way XTrackCAD
releases always did. Development builds do the opposite: each one carries its version in its
install name so that several can be installed side by side. `XTRKCAD_VERSIONED_INSTALL`
(`ProgramVersion.cmake`, Development Issues #48) chooses between the two. It covers the
binary, the `.deb`/`.rpm` package name, the share directory, the desktop and MIME files, the
work/preferences directory, and on Windows the install folder, Start menu folder and registry
key.

| Value | Install name |
|-------|--------------|
| `AUTO` (default) | `OFF` on the exact commit a `v<version>` tag points at, `ON` otherwise |
| `ON` | `xtrkcad-<version>`, `~/.xtrkcad-<version>`, `XTrkCAD <version>` |
| `OFF` | `xtrkcad`, `~/.xtrkcad`, `XTrkCAD` |

`AUTO` is re-evaluated on every configure, so a build directory first configured on an untagged
commit switches to the unversioned name after you check out the tag and reconfigure. The
configure output names the result: `XTrkCAD install identity: xtrkcad (...)`.

With the defaults, the three kinds of build come out like this (`<version>` is `5.4.0` here):

| Build | `--version` / About | Package file name | Install name |
|-------|---------------------|-------------------|--------------|
| Local, untagged commit | `5.4.0.<hash>` | `xtrkcad-setup-5.4.0-...` | `xtrkcad-5.4.0` |
| CI (`latest-main`), untagged commit | `5.4.0.<hash>` | `xtrkcad-setup-5.4.0.<hash>-...` | `xtrkcad-5.4.0.<hash>` |
| Tagged release `v5.4.0` | `5.4.0` | `xtrkcad-setup-5.4.0-...` | `xtrkcad` |

Package and archive *file* names always keep the version, so downloads of different releases
can't be confused. Only the name the package installs under changes.

**Cutting a release** needs no extra option: tag the commit (`git tag v5.4.0 && git push origin
v5.4.0`) and `release.yml` builds unversioned packages from it. Configuring a tagged checkout
by hand gives the same result. On a tag, `release.yml` also:

- checks that the tag names the version in `ProgramVersion.cmake` (`check-release-tag`): `v5.4.0`
  for 5.4.0, or `v5.4.0-<suffix>` for a trial run. Any other tag fails the run;
- fails the Windows build unless configure reports the unversioned install name;
- builds the Windows NSIS installer, which ordinary pushes to `main` skip;
- drafts the GitHub Release (`github-release`) only after every package built **and** passed its
  install test on a clean system, the installer included. A suffixed tag's release is marked as
  a prerelease.

## Cutting a release {#cutting-a-release}

1. Check `ProgramVersion.cmake` has the version being released, with no modifier left over from
   development, and that `main` is green.
2. **Trial run.** Tag the release commit with a suffix:
   ```sh
   git tag v5.4.0-test1 <commit> && git push origin v5.4.0-test1
   ```
   The tag run builds and install-tests every release package, the NSIS installer included, and
   the packages install as plain `xtrkcad`. The experimental self-contained DMG is still opt-in:
   `gh workflow run release.yml --ref v5.4.0-test1 -f include_dmg=true` builds it. Check
   the draft prerelease's packages, then delete the draft release(s) and the tag
   (`gh release delete v5.4.0-test1 --cleanup-tag`).
3. **Release.** `git tag v5.4.0 <commit> && git push origin v5.4.0`. Wait for the run to finish,
   review the draft release and its generated notes, and publish it.
4. **Next cycle.** Bump `XTRKCAD_RELEASE_VERSION` (and `XTRKCAD_VERSION_MODIFIER` if used) in
   `ProgramVersion.cmake`, so development builds no longer claim the released version.

In Hg, `hg tag v5.4.0` commits the tag as a new changeset and moves the working copy onto it,
and that changeset isn't the tagged one. To build the release from Hg, `hg update v5.4.0` first.

**An unversioned build from an untagged commit**, for someone who wants a dev build to update
their current install in place:

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DXTRKCAD_VERSIONED_INSTALL=OFF
cmake --build build
```

It replaces the installed release and uses its preferences. Its `--version` still carries the
commit hash, so it can be told apart from the release.

**A versioned build from a tagged commit**, e.g. a release candidate that should install next to
the current release: add `-DXTRKCAD_VERSIONED_INSTALL=ON`.

What an unversioned install does on each platform:

- **Linux:** the `.deb`/`.rpm` package is named `xtrkcad`, so the package manager upgrades the
  installed `xtrkcad` package. The Debian version carries the epoch `1:` (SF #149), so a
  distribution's own `xtrkcad` package doesn't replace it on the next system update.
- **Windows:** the NSIS installer uses the `Program Files\XTrkCAD` folder and the `XTrkCAD`
  registry key, and offers to uninstall the previous release first
  (`CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL`); `install-test-windows-nsis` checks that path. From the
  ZIP, unpack over the previous folder; the program is `bin\xtrkcad.exe`.
- **macOS:** the archive's program is `bin/xtrkcad`; unpack it over the previous install.
- **Preferences:** an unversioned install uses `~/.xtrkcad` (`%APPDATA%\xtrkcad` on Windows),
  the same directory releases before 5.4.0 used, so upgrading keeps the user's settings.
  Versioned dev builds keep their own directories and don't touch it.

Flatpak builds don't use this option; they have their own, `XTRKCAD_FLATPAK_NOVERSION` (see
\ref flatpak "Flatpak").

# Advanced/optional local checks {#advanced-optional-local-checks}

Not part of the everyday build/test loop, but available if you're chasing a specific class of
bug:

- **Sanitizers** — `-DXTRKCAD_SANITIZE=ON -DCMAKE_C_COMPILER=clang` builds with ASan/UBSan.
  `PreferenceTest` and `MRUListTest` (SF #794) are excluded when running under it (GTK3's own
  internal allocations trip ASan's leak detector; both tests still run normally elsewhere,
  e.g. under `c-tests-linux`'s `xvfb-run`).
- **Valgrind** — `ctest -T memcheck --overwrite MemoryCheckCommand=$(which valgrind)`.
  `PreferenceTest` and `MRUListTest` are excluded here too, for the same GTK3-internals reason.
- **Regression demo-playback suite** — `-DXTRKCAD_REGRESSION_TESTING=ON` (auto-enabled on Linux
  when `xvfb-run` or an existing `DISPLAY` is found) adds ~47 per-demo `ctest` targets, one per
  entry in `app/lib/xtrkcad.xtq`'s `DEMO` list, for targeting a single demo during local
  debugging (`ctest -R Regression.dmease.xtr`), plus a `RegressionSuite` test (label
  `regression-ci`) that replays every demo in one process — what CI actually gates on, via its
  own `regression-gtk3` job (`ctest --test-dir build -L regression-ci`), rather than the 47
  individual tests, to avoid multiplying per-test process/X-session startup overhead. These tests
  need a real installed tree, not just the build tree, because `wGetAppLibDir()`
  (`app/wlib/gtk3lib/unix/ixpaths.c`) can't find `demos`/`params`/compiled icon resources in the
  build tree alone; a `RegressionInstall` `ctest` fixture runs `cmake --install` into a scratch
  prefix automatically before any regression test executes. Each regression test also gets its
  own scratch `$HOME` (XTrkCAD persists a benchwork-lumber catalog, an MRU color list, etc. to
  `$HOME/.<binary-name>/xtrkcad.rc` — the directory is version/build-hash-qualified, e.g.
  `.xtrkcad-5.4.0.<hash>`, not plain `.xtrkcad`; and two runs sharing a real `$HOME` can produce a
  false FAIL from state an earlier run left behind) and `RUN_SERIAL TRUE` (`xvfb-run -a`'s
  free-display detection isn't atomic against other concurrent `xvfb-run` instances, confirmed
  empirically: a batch of the per-demo tests under `ctest -j 4` failed 8/48, all cleanly
  reproducing as passes when rerun alone). Both together make `ctest -j N` safe for the whole
  suite from a normal desktop checkout — the cheap non-display unit tests elsewhere in the
  project parallelize freely, while regression tests still run one at a time relative to each
  other and to everything else. A `RegressionCleanHomes` fixture wipes and recreates every scratch
  `$HOME` at the *start of each ctest invocation* (not just once at CMake configure time), so a
  process killed uncleanly by e.g. `ctest`'s own `TIMEOUT` can't leave behind a stale checkpoint
  file that would otherwise make `OfferCheckpoint()` (`misc.c`) pop a blocking resume/discard
  dialog on the *next* run — invisible and unclickable under a headless Xvfb session, which looks
  exactly like a hung regression. The wipe must recreate every directory it just removed, not just
  the root: `wGetAppWorkDir()` creates only `$HOME/.<binary-name>` via a non-recursive `g_mkdir()`,
  which fails with `ENOENT` (and pops that same kind of unclickable modal) if `$HOME` itself is
  missing (see [gtk3issues #24](https://sourceforge.net/p/xtrkcad-fork/gtk3issues/24/)). On a
  `REGRESSION FAIL`, each test's `COMMAND` also cats `$HOME/.<binary-name>/xtrkcad.regress` — the
  full actual-vs-expected track dump `CheckRegressionResult()` (`track.c`) writes there — to stdout
  before exiting, so that detail is visible in `ctest --output-on-failure`/`-VV` output instead of
  sitting in a directory the next test's `RegressionCleanHomes` run is about to delete.
- **`regression-gtk3-standalone` CI job** — runs the per-demo targets individually
  (`ctest --test-dir build -L "^regression$" -LE "regression-ci"`), separately from the
  `regression-gtk3` job's single-process `RegressionSuite` run. The two setups aren't
  interchangeable: some cold-start-only bugs only reproduce standalone (a fresh process, first
  paint) and pass inside the full suite (where the same demo runs warm, after other demos have
  already drained any startup event backlog) — see
  [gtk3issues #21](https://sourceforge.net/p/xtrkcad-fork/gtk3issues/21/)'s `wDrawStart`
  reentrancy and mouse-synthesis bugs, both of which needed this standalone mode to catch.
- **`dmjnss.xtr` excluded from `RegressionSuite` on arm64** — its "join straights cornu" check
  fails deterministically on arm64 Release builds (~0.1 degree cornu-join angle divergence).
  Root cause not yet found: tried and ruled out AArch64 FMA-contraction of `spiro.c`'s
  Newton-Raphson solver (forcing `-ffp-contract=off` on the cornu library had zero effect on the
  actual failing job, so something else is going on). `app/bin/RegressionTests.cmake` excludes it
  from `RegressionSuite`'s demo list whenever `CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64"`
  (see [gtk3issues #28](https://sourceforge.net/p/xtrkcad-fork/gtk3issues/28/)) so CI can still
  gate on the rest of the suite on that architecture. Temporary — remove the exclusion once #28
  is actually root-caused and fixed, don't leave it in place indefinitely.
- **Wayland desktop: regression tests forced to X11** — `xvfb-run` only overrides `DISPLAY`, not
  `WAYLAND_DISPLAY`. On a Wayland session, GTK3's backend auto-detect then prefers the real
  compositor over the X11 display `xvfb-run` just set up, so every regression test opened real
  windows on the developer's own desktop and stole focus instead of running isolated inside the
  virtual framebuffer. `app/bin/RegressionTests.cmake` wraps every regression test's command with
  `if [ -n "$WAYLAND_DISPLAY" ]; then export GDK_BACKEND=x11; fi` — a no-op on a pure-X11 session
  or a CI runner (no `WAYLAND_DISPLAY` there), a hard backend override on a Wayland desktop.
- **Visual pre/post diffing (`-d visualdiff=1`)** — the regression suite's `REGRESSION START`/`END`
  blocks compare track coordinates as text, which can miss a change that's obvious in a picture
  (SF #667/#26: a fixture regen silently dropped a join's connector tracks, and the numeric diff
  alone didn't make that visible). `-d visualdiff=1 -T<demo>.xtr` renders a PNG at every
  `REGRESSION` block and once more at end-of-playback (`$HOME/.<binary-name>/visualdiff/`), using
  the layout's own track bounding box rather than the live window's viewport (which isn't reliable
  off-screen). `tools/regression-visual-diff.sh <before-ref> <after-ref> <demo.xtr>` builds two
  refs, runs both, and diffs the resulting images with ImageMagick — useful for join/curve/geometry
  changes specifically. The end-of-playback screenshots also feed the \ref regression-tests
  "Regression Test Demos" page, generated by `tools/generate-regression-docs.sh`.

# Running with debug logging (`-d`) {#running-with-debug-logging}

`ctest` verifies unit-testable logic; it can't confirm an interactive-only fix (a dialog, a draw
path, a mouse-driven command) actually behaves correctly. For that, run the real app with its
built-in per-module debug logging: `-d <category>=<level>` (repeat the flag once per category;
`-d <category>` alone defaults to level 1), optionally with `-l <file>` to redirect output to a
file instead of stdout. A log file also gets GLib's and GTK's warnings and errors, so it shows what
GTK reported before a failure. There's no `-d help`/discovery flag — see the \ref log "Log Commands"
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
running `build/app/bin/xtrkcad` directly fails to find them. Do a real (scratch)
install first:

```sh
cmake --build build
cmake --install build --prefix build/install
build/install/bin/xtrkcad -d join=3
```

With `--prefix` placed one level above `bin/` like this, the `../share/xtrkcad-gtk` relative
lookup finds everything automatically. If the scratch prefix lives somewhere else instead (e.g.
`/tmp`), point `XTRKCADGTKLIB` at the installed `share/xtrkcad-gtk` directory explicitly:

```sh
cmake --install build --prefix /tmp/xtrkcad-scratch
XTRKCADGTKLIB=/tmp/xtrkcad-scratch/share/xtrkcad-gtk /tmp/xtrkcad-scratch/bin/xtrkcad -d join=3
```

# Tool versions

CI pins its runner images and the compilers that gate a build. Every Linux job in
`ci-gtk3.yml`, `release.yml` and `codeql.yml` runs on `ubuntu-24.04` (or `ubuntu-24.04-arm`)
rather than `ubuntu-latest`, which GitHub moved to Ubuntu 26.04 on 2026-10-19. The one exception
is `release.yml`'s `package-linux-ubuntu`, which also builds the Ubuntu 26.04 `.deb` on
`ubuntu-26.04` (and `ubuntu-26.04-arm`), because a `.deb` built on 24.04 doesn't install on 26.04
(Development Issues #37). The Ubuntu image
fixes cppcheck, clang-tidy, GCC and Clang, so they only change when we move the runners on
purpose. macOS jobs run on `macos-26` (Apple Silicon), plus `macos-26-intel` for the Intel
package in `release.yml`, with Xcode selected through `DEVELOPER_DIR` (which fixes AppleClang), and Windows jobs run on `windows-2025` with MinGW GCC installed at an exact MSYS2
package version. Homebrew and the rest of MSYS2 (GTK, CMake, libzip and so on) can't be pinned
and still update on their own schedule. `ci-gtk3-preview.yml` runs the toolchain-sensitive jobs
on the next images ahead of a move (see \ref ci-tooling-overview "CI tooling overview"). The
`c-tests-debian` jobs and `release.yml`'s `package-linux-debian` jobs build inside
`debian:bookworm` and `debian:trixie` containers, so they get those releases' own GCC 12/14,
CMake and GTK; `package-linux-fedora` does the same in `fedora:44` and `fedora:45` containers. The AStyle pin exists specifically because an unpinned version once produced
different formatting output for identical input (SF #638) — if you're chasing a formatting or
doc-generation mismatch against CI, matching these two matters more than matching anything else
in this table.

| Tool | Pin status | Currently tested (as of 2026-10-18 — unpinned rows drift as packages update) |
|------|-----------|------------------------------------------|
| Doxygen | pinned | 1.15.0 (upstream Linux binary, `.github/actions/setup-doxygen`) for the dev-guide jobs (`ci-gtk3.yml`'s `doxygen` and `help-links-check`, `release.yml`'s `package-docs`). The packaging jobs build the User Guide with the OS package instead: apt/Homebrew (unpinned) on Linux/macOS; MSYS2 `mingw-w64-x86_64-doxygen` pinned to 1.16.1-4 in `package-windows`, since 1.18.0-3 aborts on the User Guide build (`std::system_error`, 2026-10-05) |
| ntldd | pinned | `mingw-w64-x86_64-ntldd-r19.7fb9365-4`, installed with `pacman -U` from its URL in `package-windows` (it collects the DLLs for the ZIP and installer). On 2026-10-25 MSYS2 changed that package's database entry without a version bump, and installing it through `setup-msys2` failed the signature check against the runner image's older database |
| AStyle | pinned | 3.6.13, all platforms (built from source, not the OS package; version set in `tools/astyle-check.sh`) |
| CMake | floor/ceiling only (`3.20`–`4.0`) | 3.28.3 (Linux), 4.4.x (Windows/MSYS2), Homebrew current (macOS) |
| cppcheck | pinned (runner) | 2.13.0 (Ubuntu 24.04) |
| clang-tidy | pinned (runner) | 18.0 (Ubuntu 24.04) |
| GCC (Linux) | pinned (runner) | 13.3.0 (Ubuntu 24.04.4 LTS) |
| Clang (Linux) | pinned (runner) | 18.0 (Ubuntu 24.04) |
| Runner images | pinned | `ubuntu-24.04`, `ubuntu-24.04-arm`, `ubuntu-26.04`(`-arm`) (Ubuntu 26.04 package only), `macos-26`, `macos-26-intel` (Intel package), `windows-2025` |
| AppleClang (macOS) | pinned (Xcode) | Xcode 26.6, Clang 21.0.0 (`.github/actions/select-xcode`) |
| MinGW GCC (Windows) | pinned | 16.2.0-6 (MSYS2 `gcc`/`gcc-libs`, installed with `pacman -U` by `.github/actions/msys2-pin-gcc`) |

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

Then run the same check as the `astyle-check` gate from the repository root. It warns if your
astyle isn't the pinned version:

```sh
tools/astyle-check.sh
```
