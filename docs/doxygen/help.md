\page help-troubleshooting When Help Doesn't Work
\tableofcontents

# How Help is opened {#help-how}

The help pages are the User Guide, HTML that Doxygen builds from `app/doc` (target
`help-html`). The Help menu, the F1 key and every dialog's Help button end in `wHelp()`
(`app/wlib/gtk3lib/browserhelp.c`), which turns a topic such as `cmdStraight` into

    file:///<data directory>/html/<topic>.html

and hands it to the default browser: `ShellExecute()` on Windows, `gtk_show_uri_on_window()`
on Linux and macOS (`opendocument.c`). The data directory is the folder the parameter files
come from (`wGetAppLibDir()`). If the hand-over fails, XTrackCAD shows *"Help Error - help
information cannot be found"* with the full path it tried. Note that path down first: it tells
you which of the steps below applies.

On Linux and macOS the URL has four slashes (`file:////usr/share/...`), because the data
directory is an absolute path already. A path starting with `//` means the same as `/` there,
so it works, but don't let it surprise you in the error notice.

# What to check {#help-checklist}

## 1. Was the User Guide built? {#help-built}

Without Doxygen, configure only prints

    Doxygen not found -- the User Guide (help-html target) will not be built or installed

and carries on: the build succeeds and installs everything except help. Look for that line in
your configure output, or for `<build>/app/doc/html/index.html`. If it's missing, install
Doxygen, reconfigure, build and install again:

- Windows: `winget install DimitriVanHeesch.Doxygen` (open a new prompt so it's on `PATH`), or
  `mingw-w64-x86_64-doxygen` in MSYS2.
- Linux: your distribution's `doxygen` package.
- macOS: `brew install doxygen`.

## 2. Did you run the installed program? {#help-installed}

The data directory is found from where the program runs, not from where it was built:

- Windows: `<install root>\share\<app name>`, where `<install root>` is the folder above `bin`
  (`win32/w32paths.c`), e.g. `C:\XTrackCAD\share\xtrkcad-5.4.0`. See \ref windows.
- Linux and macOS (`unix/ixpaths.c`), the first one that exists: the folder in the environment
  variable `<APP NAME>LIB` (upper case, `-` and `.` become `_`, e.g. `XTRKCAD_5_4_0LIB`), then
  `../share/<app name>` next to the binary, then `/usr/share/<app name>`, then
  `/usr/local/share/<app name>`. See \ref macos for the macOS package.

Running from the build tree finds no data directory there. On Windows the program then looks in
`<build>\app\share\...`, which doesn't exist; on Linux it may quietly use an older version
installed in `/usr/share` instead. Either way help and parameters come from the wrong place or
from nowhere. Run `cmake --install` and start the installed program.

## 3. Is the html folder in the data directory? {#help-html-folder}

The User Guide installs to `<data directory>/html`, next to `params`, `demos` and `examples`.
If the parameter files load but `html` is missing, go back to step 1: it was never built. If
`params` is missing too, it's step 2 or an incomplete install.

## 4. Does the page open by hand? {#help-by-hand}

Open `<data directory>/html/index.html` in a browser yourself, then paste the exact path from
the error notice into the address bar.

- **It doesn't open by hand:** the files are missing or unreadable (step 3, or permissions).
- **It opens by hand but not from XTrackCAD:** the hand-over to the browser is the problem.
  - Check that a default browser is set (Windows: Settings > Apps > Default apps, for `.html`).
  - On Linux, GIO picks the program by the file's MIME type, and it sees the User Guide's
    pages as `application/xhtml+xml`, not `text/html`. The default for *that* type is what
    opens; it can be an editor, or nothing, even when the browser is the default for
    `text/html`. Check with `gio mime application/xhtml+xml`; set it with
    `xdg-mime default firefox.desktop application/xhtml+xml` (or your browser's `.desktop`).
  - On macOS, the program set to open `.html` files in Finder (Get Info > Open with) is the
    one that opens.
  - On Linux, a snap or Flatpak browser may not be allowed to read `/usr/share`; try one
    installed as a normal package.

## 5. Ask on the dev mailing list {#help-report}

Include the platform, how you built and installed XTrackCAD, the path from the error notice (or
what happened instead: the browser's "file not found", another program opening, or nothing at
all), and whether step 4 opens the page by hand.

# Testing it {#help-testing}

The package install tests open Help on every Linux package (Development Issues #58):
`tools/package-install-test.sh` registers a fake browser for `text/html` and
`application/xhtml+xml` that writes down what it's given, plays
`tools/package-install-test-help.xtr` with `-T`, and checks that each topic arrived as the
installed `html/<topic>.html`. The script uses the `HELP <topic>` playback command, which opens
a topic the way the Help menu does; under `-T` a missing topic counts as a regression failure
instead of stopping at the error notice. `-T` takes the path of a playback script as well as a
demo name, so a test script doesn't have to be in the Demo menu. See \ref package-install-tests.
