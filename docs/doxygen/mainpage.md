# Developer Documentation

Model railroad layout design and operation software.

XTrkCAD lets modelers design track plans on a scaled grid, generate accurate turnout templates
for printing, produce parts lists, and generate operating documentation for a finished layout.
This is the developer-facing documentation, generated from source comments with
[Doxygen](https://www.doxygen.nl/) — it documents the code, not how to use the application.
Everything here has been checked against the current `GTK3V2MAIN` source rather than assumed —
see inline notes where something changed since it was first written up.

**This is a separate Doxygen project from the User Guide.** The application's own in-app help
(`Help > Contents`, F1 command-context help, every dialog's own `Help` button) is a *different*,
independently-built Doxygen config — `app/doc/Doxyfile.in`, source in `app/doc/*.dox`, the
`help-html` CMake target — with its own INPUT set that doesn't overlap this one's (`app/bin/`,
`app/wlib/gtk3lib/`, this file). See \ref ci-tooling-overview "CI tooling overview" in
`advanced.md` for how `help-links-check` verifies every runtime help link actually resolves in
that separate build.

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
two files most of the rest of the application builds on. Coding conventions and error-handling
patterns follow below; building and testing locally on each platform is covered on the
\subpage building "Building and Testing" page — or use the search box or the file/class browser
in the sidebar to jump straight to a specific symbol instead of reading top to bottom.

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

See the \subpage building "Building and Testing" page for local build steps per platform,
CI details, tool-version pins, and advanced checks (sanitizers, valgrind, the regression
demo-playback suite, debug logging).

See the \subpage advanced page for setting up CI on your own GitHub fork and the process/lessons
behind this project's static-analysis triage work.
