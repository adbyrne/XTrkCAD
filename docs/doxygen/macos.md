\page macos macOS for Devs
\tableofcontents

# Overview {#macos-overview}

XTrackCAD on macOS is the GTK+ 3 program built against Homebrew's GTK, for Apple Silicon (arm64)
and Intel (x86_64). The released package is a Homebrew-dependent archive, not a self-contained
app bundle. This page covers how that package works, where the program keeps its files on macOS,
and what to know when testing or debugging there.

For the build itself, see the \ref building "Building and Testing" page's macOS section. For the
CI install test, see the \ref package-install-test-macos "macOS install test" section of the
\ref advanced page.

# The macOS package {#macos-package}

`packages-macos.yml`'s `package-macos` job (called by `release.yml`) builds on `macos-26` (arm64) and `macos-26-intel` (x86_64)
and publishes `xtrkcad-<version>-macos-<arch>.tar.gz`: the `cmake --install` tree plus
`README-INSTALL.txt` (from `distribution/osx/README-INSTALL.txt.in`). The binary links Homebrew's
GTK3 and its dependencies by absolute path, so users must install the prerequisites first:

```sh
brew install gtk+3 libzip librsvg
mkdir -p ~/Applications/XTrkCAD
tar -xzf xtrkcad-<version>-macos-arm64.tar.gz -C ~/Applications/XTrkCAD
~/Applications/XTrkCAD/bin/xtrkcad-<version>   # bin/xtrkcad in a release archive
```

The library paths are Homebrew's default prefix for the architecture the archive was built on:
`/opt/homebrew` on Apple Silicon, `/usr/local` on Intel. `otool -L bin/xtrkcad-<version>` lists
them. An archive works only with Homebrew in that default place. The binary name carries the
version (`XTRKCAD_BIN`, since #52), and the README's commands are generated from it.

The archive isn't signed or notarized. `README-INSTALL.txt` explains how to get past the
Gatekeeper warning on first start.

**Self-contained DMG:** `release.yml` can build one with `gtk-mac-bundler`
(`distribution/osx/gtk-bundle/`), only when dispatched with `include_dmg`, and the step is
`continue-on-error`. It is unsigned, and the archive is the package to use.

# Files and directories {#macos-files}

macOS uses the Unix path code, `app/wlib/gtk3lib/unix/ixpaths.c`:

| What | Where |
|------|-------|
| Data (params, demos, examples, help HTML) | `../share/<app name>` next to the binary, found from the executable's own path |
| Work directory (preferences, recent files) | `~/.<app name>` |
| Preferences | `<work directory>/<app name>.ini` (`-c name` gives `<name>.ini`) |
| Log | stdout, or the file given with `-l` |

Options start with `-`, as on Linux.

`GetProgramDir()` gets the executable's path from `_NSGetExecutablePath()` and resolves symbolic
links with `realpath()`, as `/proc/self/exe` does on Linux. Before Development Issues #43 it
read only `/proc/self/exe`, which macOS doesn't have, so the data directory was looked up
relative to the *current directory*. The archive then worked only when started from its own
`bin/` or when unpacked into `/usr/local`.

Help topics are `file:///<data directory>/html/<topic>.html` (`browserhelp.c`), opened in the
default browser (`opendocument.c`). The builds use `-DXTRKCAD_USE_APPLEHELP=OFF
-DXTRKCAD_USE_BROWSER=ON`. `osxhelp.c` and `helphelper` are the older Apple Help Viewer path,
which these builds don't use. If Help doesn't open, see \ref help-troubleshooting.

# Testing and debugging on macOS {#macos-testing}

The CI install test (`tools/package-install-test-macos.sh`) is the reference for running the
archive the way a user does: follow the README, unpack into a directory whose name contains a
space, and start the binary by its full path from `$HOME`, not from `bin/`.

Some things are different from Linux:

- **No `timeout(1)`.** Bound scripted runs with
  `perl -e 'alarm shift; exec @ARGV' <seconds> <command>`. A run killed this way exits with 142
  (128 + SIGALRM).
- **`/bin/sh` is bash 3.2.** It can't parse a `case` statement inside `$(...)`: the first
  unbalanced `)` of a pattern ends the substitution, and the shell reports a syntax error at the
  `;;`. Put such code in a function and call that from `$(...)`.
  `bash --posix -n` in a `bash:3.2` container reproduces the error on Linux.
- **Crash reports.** A crash writes `~/Library/Logs/DiagnosticReports/<binary>-<date>.ips`
  a few seconds after the process dies. Take the newest file for *your* binary; other programs
  crash too. The file is a one-line JSON header followed by a JSON body. The body's
  `faultingThread` indexes `threads`, and each frame has a `symbol` and an `imageOffset`. With a
  `-g` build, `lldb --batch -o 'target create <binary>' -o 'image lookup -v -a <0x100000000 +
  imageOffset>'` gives the source line, reading the debug info from the build's object files.
- **lldb can hide a crash.** lldb turns off address space layout randomization by default. With
  it off, `0x100000000` is the start of the executable and readable, so a corrupted pointer with
  that value doesn't fault.
- **Release builds use `-O3`.** CMake's Release type adds `CMAKE_C_FLAGS_RELEASE` (`-O3 -DNDEBUG`)
  after `CMAKE_C_FLAGS`, so an `-O` level passed in `CMAKE_C_FLAGS` has no effect. Override
  `CMAKE_C_FLAGS_RELEASE` to compare optimization levels.

## Example: the Intel-only crash {#macos-intel-crash}

Development Issues #46 shows how far a bug can hide. The x86_64 archive crashed on every demo
run, in `MoveCursor()`, right after `TempRedraw()` returned with a corrupted callee-saved
register. It didn't crash on arm64, Linux or Windows, at `-O1`/`-O2`, in a Debug + ASan build,
with `-fstack-protector-all`, or under lldb. An `-O3` UBSan build (`-fsanitize=undefined
-fsanitize-recover=all`) found it: `wDrawDelayUpdate()` passed the `int` fields of a
`cairo_rectangle_int_t` to `wDrawGetSize()` cast to `wWinPix_t *` (`long`), and the 8-byte store
wrote 4 bytes past the rectangle on the stack. When a crash depends on the optimization level,
try UBSan in recover mode at the failing level before anything else.
