\page windows Windows for Devs
\tableofcontents

# Overview {#windows-overview}

XTrackCAD on Windows is the same GTK+ 3 program as on Linux and macOS, built with the MSYS2
MINGW64 toolchain. The old native Win32 backend (`app/wlib/mswlib/`) is gone. A few things still
differ on Windows at run time, and several bugs found in 2026-10 came from code that assumed
Linux behavior. This page collects those differences, describes how the Windows package is put
together, and lists the pitfalls to watch for.

For the build itself, see the \ref building "Building and Testing" page's Windows section. For
the CI install test, see the \ref package-install-test-windows "Windows install test" section of
the \ref advanced page.

# Running the program {#windows-running}

## Command-line options start with `/` {#windows-options}

On Windows, `misc.c`'s option loop uses the `getopt()` replacement in
`app/wlib/gtk3lib/win32/getopt.c`, which expects options to start with `/`, not `-`:

```
xtrkcad-5.4.0.exe /V
xtrkcad-5.4.0.exe /l C:\temp\xtclog.txt /Tdmintro,dmstrtrk
```

A `-V` is not an option on Windows: the program takes it as the name of a layout file to open,
and shows "Cannot open Track file". The options are the same as elsewhere (`/v`, `/l file`,
`/d group=level`, `/c name`, `/m`, `/V`, `/T[demo,...]`). An option's argument can be attached
(`/Tdmintro`, `/dlog=3`) or follow as the next word (`/l file`). Before Development Issues #45,
an attached argument hung the program at startup; `getopttest` covers both forms.

## Files and directories {#windows-files}

| What | Where | Code |
|------|-------|------|
| Data (params, demos, examples, help HTML) | `<install root>\share\<app name>`, e.g. `C:\XTrackCAD\share\xtrkcad-5.4.0` | `wGetAppLibDir()`, `win32/w32paths.c` |
| Work directory (preferences, log, recent files) | `%APPDATA%\<app name>` | `wGetAppWorkDir()` |
| Preferences | `<work directory>\<app name>.ini`, or `<name>.ini` with `/c name` | `wpref.c` |
| Log | `<work directory>\xtclog.txt`, or the file given with `/l` | `lprintf.c` |

`<install root>` is the directory above `bin\`. `g_win32_get_package_installation_directory_of_module()`
already drops a trailing `bin`, so don't append `..` to it (Development Issues #41).
`<app name>` is the binary name without `.exe` (`XTRKCAD_BIN`), so several versioned builds can
be installed side by side without sharing data or preferences. A release build's `<app name>` is
plain `xtrkcad` (\ref release-defaults "Defaults for Release").

The work directory can be moved with an `xtrkcad0.ini` file in the data directory:

```
[workdir]
path=D:\XTrackCAD-work
```

`path=installdir` uses the data directory itself, which suits a portable install on a USB stick.

## The log goes to a file {#windows-log}

`lprintf()` writes to stdout everywhere except Windows, where it writes to `xtclog.txt` in the
work directory (`#ifdef WINDOWS` in `lprintf.c`). The program is a GUI-subsystem executable, so
there's no console for stdout anyway. Use `/l <file>` to put the log where you want it, for
example when reading `Regression Tests Pass` after a `/T` run.

GLib's and GTK's own warnings, criticals and errors go there too, once the log is open
(`wSetLogCallBack()` in wlib's `main.c`, registered by `LogOpen()`). They still go to stderr as
well. Linux and macOS do the same when `-l` names a log file; without it the log is stdout, so
the messages are only on stderr. A program that closes at startup without a word, with exit code `0xC0000409`, has usually
had a `g_error()` from GTK, and its text is now the last line of the log.

## Help {#windows-help}

Help topics are `file:///<data directory>/html/<topic>.html` (`browserhelp.c`), opened with
`ShellExecute()` in the user's default browser (`opendocument.c`). If Help doesn't open, see
\ref help-troubleshooting.

## Harmless messages {#windows-harmless}

`GLib-GIO-WARNING: win32 session dbus binary not found` on stderr is expected. GTK looks for a
session D-Bus at startup, there is none on Windows, and nothing in XTrackCAD needs it.

# The Windows package {#windows-package}

`release.yml`'s `package-windows` job builds in MSYS2 MINGW64 on `windows-2025` and publishes
`xtrkcad-<version>-windows-x64.zip`. Users unpack it anywhere and run `bin\xtrkcad-<version>.exe`,
or `bin\xtrkcad.exe` in a release (\ref release-defaults "Defaults for Release"). The job installs
MSYS2's own `git` so that `ProgramVersion.cmake` sees the commit hash and the release tag; on a
tag it fails unless configure reports the unversioned install name. Before Development Issues #48
MSYS2 had no git, so Windows builds had no hash and couldn't recognise a release tag.
The ZIP must be complete on its own, because users don't have MSYS2:

| Path in the ZIP | Contents | Where it comes from |
|-----------------|----------|---------------------|
| `bin\` | the exe and every MinGW64 DLL it or a pixbuf loader needs (48 in 2026-10) | `ntldd -R`, converted with `cygpath` |
| `lib\gdk-pixbuf-2.0\<ver>\` | image loaders and `loaders.cache` | copied from `/mingw64` |
| `share\glib-2.0\schemas\` | `gschemas.compiled` (GTK3 needs it on Windows) | copied from `/mingw64` |
| `share\icons\hicolor\` | GTK's fallback icon theme | copied from `/mingw64` |
| `share\<app name>\` | params, demos, examples, help | `cmake --install` |

Until Development Issues #42 the DLL copy ran through `xargs`, which dropped the backslashes in
`ntldd`'s Windows paths. Every copy failed, `|| true` hid it, and no ZIP ever shipped a DLL.
The ZIP only worked on machines that had MSYS2 on `PATH`. The step now runs with
`set -eo pipefail` and fails if `ntldd` finds nothing.

**NSIS installer:** `xtrkcad-setup-<version>-x64.exe`, built on version tags and, otherwise,
only when `release.yml` is dispatched with `include_nsis`. `cmake --install` doesn't collect the
MinGW DLLs, so `cpack -G NSIS` also gets the ZIP's staging directory through
`CPACK_INSTALLED_DIRECTORIES`, and the installer holds the same files as the ZIP. It installs
for all users under `Program Files` (`CPACK_NSIS_INSTALL_ROOT` is `$PROGRAMFILES64`), adds a
Start menu folder and associates `.xtc`/`.xtce` with the program. A release installs into
`Program Files\XTrkCAD` under the uninstall key `XTrkCAD`, so installing the next release offers
to uninstall the previous one first and then replaces it (\ref release-defaults "Defaults for Release").
Before Development Issues #49 the installer had no DLLs, wasn't built on tags, and its Start
menu shortcut pointed at `bin\xtrkcad.exe` even when the binary was versioned.

**Toolchain:** CI builds with MSYS2 MINGW64 and pins MinGW GCC (`.github/actions/msys2-pin-gcc`).
MSYS2 has deprecated MINGW64. Moving CI and the release to UCRT64 is tracked as Development
Issues #34.

# Testing on Windows {#windows-testing}

The CI install test (`tools/package-install-test-windows.ps1`) is the reference for running the
package the way a user does. To try a ZIP by hand:

- Unpack it into a directory whose name contains a space.
- Start it from a shell whose `PATH` holds only `C:\Windows\System32`, `C:\Windows` and
  `C:\Windows\System32\Wbem`. Otherwise a DLL the ZIP forgot can still be found in MSYS2, Git
  or Strawberry Perl, which are on `PATH` on most developer machines and on the CI runners.
- Exit code `0xC0000135` (`-1073741515`) means a DLL is missing; `0xC0000139` means a DLL lacks an
  entry point (usually a version mismatch).
- A run that never ends is usually waiting on a modal dialog: a notice about a bad option or a
  missing file. Give every scripted run a timeout. The CI test also saves a screenshot of a
  timed-out run, which shows the dialog.

The regression demo suite doesn't run on Windows in CI (it needs Xvfb). A `/l log /T...` run of
the installed program is the closest equivalent.

# Pitfalls {#windows-pitfalls}

These all caused real Windows-only failures:

- **Paths that aren't file system paths.** `MakeFullpath()` (`paths.c`) joins with `FILE_SEP_CHAR`,
  which is `\` on Windows. GResource paths (compiled-in icons and fonts) and URLs always use `/`.
  Building the layer-button font's resource path with `MakeFullpath()` aborted every Windows
  build at startup (Development Issues #44). See the bug patterns list on the \ref advanced page.
- **Assuming the Linux install layout.** Data, work and log locations come from
  `win32/w32paths.c` and `lprintf.c`, not `unix/ixpaths.c` (Development Issues #41).
- **Assuming `-` options.** Scripts and documentation that start XTrackCAD on Windows must use `/`.
- **`int` vs `long`.** Windows is LLP64: `long` is 32 bits, the same as `int`, while Linux and
  macOS are LP64 with a 64-bit `long`. A `long *` / `int *` mix-up that is harmless on Windows
  corrupts memory on the other platforms, and the reverse (a `long` assumed to hold a pointer or
  a `size_t`) breaks on Windows.
- **The `WINDOWS` macro.** Platform code tests `WINDOWS`, which `PlatformSettings.cmake` defines
  for Windows builds, or the compiler's `_WIN32`. Both are set in MSYS2 builds.
