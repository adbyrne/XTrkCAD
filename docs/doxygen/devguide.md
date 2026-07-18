\page devguide Developer Guide

Conventions and mechanisms that aren't obvious from the code alone, plus how to build and test
the project locally. Everything here has been checked against the current `GTK3V2MAIN` source
rather than assumed — see inline notes where something changed since it was first written up.

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

The commands below are what CI actually runs (`.github/workflows/ci-gtk3.yml`), with the CI-only
bits called out — a local desktop build doesn't need everything CI needs.

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
