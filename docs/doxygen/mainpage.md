# Developer Documentation

Model railroad layout design and operation software.

XTrkCAD lets modelers design track plans on a scaled grid, generate accurate turnout templates
for printing, produce parts lists, and generate operating documentation for a finished layout.
This is the developer-facing documentation, generated from source comments with
[Doxygen](https://www.doxygen.nl/) — it documents the code, not how to use the application.

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
two files most of the rest of the application builds on.

Use the search box or the file/class browser in the sidebar to navigate from here.
