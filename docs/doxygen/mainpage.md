# Developer Documentation

Model railroad layout design and operation software.

XTrkCAD lets modelers design track plans on a scaled grid, generate accurate turnout templates
for printing, produce parts lists, and generate operating documentation for a finished layout.
This is the developer-facing documentation, generated from source comments with
[Doxygen](https://www.doxygen.nl/) — it documents the code, not how to use the application.
Everything here has been checked against the current `default` branch source rather than assumed —
see inline notes where something changed since it was first written up.

**This is a separate Doxygen project from the User Guide.** The application's own in-app help
(`Help > Contents`, F1 command-context help, every dialog's own `Help` button) is a *different*,
independently-built Doxygen config — `app/doc/Doxyfile.in`, source in `app/doc/*.dox`, the
`help-html` CMake target — with its own INPUT set that doesn't overlap this one's (`app/bin/`,
`app/wlib/gtk3lib/`, this file). See \ref ci-tooling-overview "CI tooling overview" in
`advanced.md` for how `help-links-check` verifies every runtime help link actually resolves in
that separate build.

\tableofcontents

# Source layout

In this docuement **$SRCDIR** refers the root of your source tree.  **$BINDIR** refers to the base of your build tree.

XTrkCAD is a C application built from several libraries that link into one executable:

- **`app/bin/`** — the core application (~105K lines of C). Nearly all application logic lives
  here: track objects (`track.c`/`track.h`), drawing and geometry (`draw.c`, `drawgeom.c`),
  commands (the `c*.c` files — `ccurve.c`, `cturnout.c`, `cselect.c`, `cgroup.c`, and others),
  file I/O (`fileio.c`, `paramfile.c` — the native `.xtc`/`.xtce` format), DXF import/export,
  undo/redo (`cundo.c`), scale definitions (`scale.c`), and car/consist management (`cars/`).
- **`app/wlib/`** — the UI abstraction layer. Application code calls `wlib`'s platform-neutral
  API and never the underlying toolkit directly. The backend is `gtk3lib/` (GTK+ 3).
- **`app/cornu/`** — Spiro/Cornu easement-curve calculations.
- **`app/dynstring/`** — a safe, growable C string type used throughout the codebase.
- **`app/cJSON/`** — an embedded JSON library, used for structured data such as car definitions.
- **`app/lib/`** — runtime resources: parameter files, example layouts, demo scripts.

# Where to start reading

`track.c`/`track.h` (the base track type) and `draw.c` (the drawing and geometry core) are the
two files most of the rest of the application builds on. Coding conventions and error-handling
patterns follow below; building and testing locally on each platform is covered on the
\ref building "Building and Testing" page — or use the search box or the file/class browser
in the sidebar to jump straight to a specific symbol instead of reading top to bottom.

# Source naming convention

Source files loosely follow a naming convention, not strictly applied everywhere:

- **`c<object>.c`** — the code and data structures for creating, manipulating, and deleting an
  object (similar to a class file). Example: `ccurve.c`.
- **`d<object>.c`** — the dialog(s) for creating, changing, and deleting the object, where one
  exists.
- **`t<object>.c`** — drawing the object. Example: `tcurve.c` (curved track has no dialog, so
  there's no corresponding `d`-file).

# Casts

Casts are generally avoided; there's no need to cast to or from `void *` directly. A couple of
macro pairs exist for the cases that do need one:

- **`I2VP(val)`** / **`VP2L(val)`** (`common.h`) — round-trip an integral value through a `void *`
  callback context and back. Used when a callback needs to carry a small integer instead of a
  real pointer: `I2VP()` when creating the control, `VP2L()` when reading the value back out in
  the callback.

  A common use is retrieving a list index or a button index.
  When the WLib object is created, I2VP(index) is used to convert the index to a float to match the required parameter type.  When the object is select or clicked, its callback uses VP2L(fIndex) to get the integer value

- Downcasting a wide numeric type to a narrower one (including float-to-integer) is one of the few
other places a cast is legitimate — the narrowing should be deliberate and visible at the call
site, not implicit.

- Object abstraction for track objects.
XTrkCad is writen in "C" but had adopted some "C++" concepts.
The track base object (`trackx.h` / `track_t` ) contains values common to all object:
object list, index, type, bounding box, etc.

  It also contains 'extraData' and `extraSize` which contains object specific data which are accessed by this code fragment:
```c
	const struct extraDataCompound_t * xx =
        GET_EXTRA_DATA(trk, T_STRAIGHT, extraDataStraight_t)
```
  This returns a pointer to the object's track-specific data.  The extraData's type is checked. 

- CAST_AWAY_CONST: In a few places we assign a const string to a non-const string. The CAST_AWAY_CONST define which simply evaluates to (char*), serves as a marker to indicate clean-up is required.

# Error / Notification handling

Five categories, each with a different mechanism:

- **User input errors and notifications** — invalid object selection or trying to perform invalid operations.
  `ErrorMessage()` writes to the info bar with an optional beep (recoverable by moving the mouse or picking a different object);
  `NoticeMessage()` displays a dialog requiring a button press, used when a choice
  is needed (e.g. confirming a delete).
  Both functions return the user to a valid state.

  `"Ptr == NULL"` is not a useful user-facing error message — explain the problem and the fix in the user's terms.

  Usually, the first parameter to `ErrorMessage()` or `NoticeMessage()` is a define prefixed by **MSG**.  These defines are created when the file `$SRCDIR/app/help/messages.in` is processed which creates `$BLDDIR/app/help/messages.h`.  This is used to provide support for `Help|Recent Messages` which describes messages in greater detail and extra provide information on resolving the issue.
The file $SRCDIR/app/help/messages.in contains the following for each messge:

```c


# fiie.c
MESSAGE MSG_message_name
A user centric title of the issue
ALT
Title with printf formatting strings replaced (optional)
HELP
Detailed descriiption of the issue and its resolution
END

```
  This is processed to create
  - $BINDIR/app/help/messages.h  // defines for MSG_ERROR etc
  - $BINDIR/app/help/messages.but // Help file inputs for all messages 
  - $BINDIR/app/doc/html/MSG_*.html // Contents for ‘Help|Recent Messages’

  As errors or notices are encountered they are added to the ‘Help/Recent  Messages’ list which can direct the user to additional help.  If your error does not fit this model (user-centric description and resolution) then you should consider alternatives: InfoMessage(), Logic Error or LOG().
  
- **Dialog Parameter Errors** — dialog items frequently have coinstraints on acceptable values.
  Numeric items must be well formed and may be restricted to positive numbers, or string values cannot be empty.

  The winData parameter in the paramData_t struct can specify a constraint by passing a paramIntegerRange_t or paramFloatRange_t.

  These constraints are validated when the parameter values are changes.  If validation fails, the failing item is highlighted and the dialog's OK button is disabled.

- **File input errors** — corrupt or unrecognized-feature files. `InputError()` (declared in
  `app/bin/getargs.c`) offers the option of aborting the rest of the file load. Layout files carry
  a version number specifically so that a non-backwards-compatible format change is caught
  cleanly rather than silently misparsed.

-  **Logic errors** — null pointer checks, index range checks, "can't happen" conditions.
  `CHECKMSG(cond, msg)` calls `AbortProg()` if `cond` is false, which prompts to save the current
  layout before aborting (and skips straight to abort without asking again if a second logic
  error fires during that save — see `AbortProg()` in `misc.c`). These indicate a bug in the code,
  not something the user did wrong; the message doesn't need to be translated.

- **System/environmental errors** — out-of-memory and similar. Exit as soon as possible rather
  than trying to continue in a state that can't be trusted.

# <tt>%GetArgs()</tt> extensibility

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

# `wlib` interaction

`wlib` (`app/wlib/`) packages the interface between the core application and the underlying
windowing toolkit. All application code goes through `wlib`'s platform-neutral API — never GTK
directly. The only backend is `app/wlib/gtk3lib/` (GTK+ 3), on Linux, macOS and Windows alike;
the old Win32 backend (`app/wlib/mswlib/`) has been removed.

`wlib` calls create and interact with window objects — `wButton`, `wEntry`, `wDraw`, and others,
collectively `wControl` (`app/wlib/include/wlib.h`). Most of these notify application code of
events (a button press, text entry) via a callback function pointer supplied when the object is
created, along with a `void *` context value — see the cast macros above for the common pattern
of packing a small integer into that context instead of a real pointer.

# Layer-Groups

See the \subpage understanding-layer-groups page for how named, user-managed Layer Groups (SF
#222) work internally and the two reusable scope patterns (selection vs. filter) every consumer
of them is built from.

See the \subpage creating-a-report "Creating a New Report" page for how to add a new entry to
the `Reports` menu (SF #217) -- the shared viewer, the two report shapes, and a step-by-step
recipe with references into the current `reports.c`/`reports.h` source.

See the \subpage dialogs-and-layout-loading "Dialogs and Layout Loading" page for what happens
to open dialogs when a new layout is loaded, and what a new dialog has to declare (reload,
close, or neither) so it never writes stale data into the new layout.

See the \subpage building "Building and Testing" page for local build steps per platform,
the CMake presets, CI details, tool-version pins, and advanced checks (sanitizers, valgrind, the regression
demo-playback suite, debug logging).

See the \subpage help-troubleshooting page for what to check when Help doesn't open a
build's help pages, on any platform, and how the install tests check it.

See the \subpage development-process page for how Mercurial branches and the SF
trackers are used during development: release, feature, and bugfix branches, and where bugs
and feature requests are filed.

See the \subpage advanced page for setting up CI on your own GitHub fork and the process/lessons
behind this project's static-analysis triage work.

# Source Control
XTrkCad source code is managed using Mercurial on SourceForge.  See the Fetch/Build wikka page (https://xtrkcad-fork.sourceforge.net/Wikka/BuildNotes).

# Branches
Normal development is handled in the ‘default’ branch.

It is recommended that any non-trivial work occur in a sub-branch from ‘default’.   Once the new work is complete and stable, the sub-branch is merged back into ‘default’.  Merging from the ‘default’ branch should be done frequencely to keep the new code in sync, especially just before merging back to ‘default’

Make your changes and Commit them to your new branch.  This will create the new branch.  Further changes are made in the new branch.

When done,  Close your branch, switch to Default and Merge from the new branch.  Closing first avoids dangling heads.

# Releases
Releasing a new version of XTrackCad, once the code is stable, feature complete and free of serious bugs:
    - Create in a new branch (vX.Y.Z) which is branched from default
    - Update $SRCDIR/ProgramVersion.cmake, in the default branch, with the next 3rd level version number, by incrementing XTRKCAD_RELEASE_VERSION. XTRKCAD_VERSION_MODIFIER will be “Dev”.
    - Update $SRCDIR/ProgramVersion.cmake, in the new branch with the new version number. XTRKCAD_VERSION_MODIFIER will be “Beta1”
    - Handle new or modified param files.
    - All platforms build and upload their beta install packages as described in https://xtrkcad-fork.sourceforge.net/Wikka/BuildNotes 
    - Source tar balls are created and uploaded.
    - Update CHANGELOG.md (see below) , Readme.md in $SRCDIR/app/lib and upload.
    - Update the XTrackCad Manual (see $SRCDIR/app/doc/mk-xtrkcad-manual-html) and upload.
    - Update the 'XTrackCAD Version' field with the new version at the SourceForge Bug Field Managment  page
    - Announce the new release on the XTrackCad mailing litst.
This process (except for the first two actions) is repeated for subsequent Betas, until all issues are resolved or defered.  XTRKCAD_VERSION_MODIFIER is incremented: Beta2, Beta3, ... 
For General Availability, XTRKCAD_VERSION_MODIFIER will be “GA” and GA install packages are built and uploaded.
Critical bugs discovered after GA are fixed in Hot Site releases with XTRKCAD_VERSION_MODIFIER set to “HS1”, “HS2”,...

The same branch is used for all releases.  A tag is created for each build: vX.Y.Z-B1..., vX.Y.Z-GA, vX.Y.Z-HS1...

**Version string sequence**

Version numbers are for illustration.
The choice of release level is made when the first Beta is released

    - 1st level: major UI, technological or organizational changes
    - 2nd level: significant new functionality, major redesign
    - 3rd level: bug fixes, minor functionality
This shows the sequence for a minor (3rd level) release.

| Event	| Rlse Version| Beta Version | Dev Version |
|:---|:---|:---|:---|
| Normal dev, between releases | 5.6.2GA | N/A | 5.6.3Dev |
| Start Release - Beta1 | 5.6.2GA | 5.6.3Beta1 | 5.6.4Dev |
| Beta2 | 5.6.2GA | 5.6.3Beta2 | 5.6.4Dev |
| Release | 5.6.3 | 5.6.3GA	N/A	| 5.6.4Dev |
| Hot Site | 5.6.3HS1 | | 5.6.4Dev |

This shows the sequence for a 2nd level release.  The sequence for a 1st level release is an exercise for the reader
 

| Event	| Rlse Version| Beta Version | Dev Version |
|:---|:---|:---|:---|
| Normal dev, between releases | 5.6.2GA | N/A | 5.6.3Dev |
| Start Release - Beta1 | 5.6.2GA | 5.7.0Beta1 | 5.7.1Dev |
| Beta2 | 5.6.2GA | 5.7.0Beta2 | 5.7.1Dev |
| Release – 5.6.3 | 5.7.0GA | N/A | 5.7.1Dev |
| Hot Site | 5.7.0HS1 | N/A | 5.7.1Dev |

There should be minimal changes in the Beta2+ or Hot Site releases.
There should be no changes between the last Beta and the GA release.
These releases are for bug fixes, not new features.
 
Fixes in Beta and Hot Site releases are duplicated in the default branch.  It is recommended that these fixes be made in the Release branch and merged into the default branch.
The merge may complain about a conflict in ProgramVersion.cmake.  The VERSION values should be as described above.

We only support the last released version.

# CHANGELOG.md
This file contains all changes for each release.  It is produced by:
```c
hg log -b default --date "2024-04-15 to 2025-04-16" --template "* {desc}\n" 
2024-04-15 is the date of the last release 
2025-04-16 is the date when the release branch (V5.3.1) was created. 
```

Additional changes in the release branch can be logged with 

```c
hg log -b v5.3.1 --template "* {desc}\n" 
```

The resulting log was edited manually as commit comments aren't always easy to understand. 
The result is prepended to CHANGELOG.md

From then on the release branch v5.3.1 should only receive bugfixes for the release. 

# Text Files
All text files (Params, Demos, Layout,...) use the UTF-8 character set and Unix-style line terminators (Single New Line, no Carriage Return).


# Parameter File Maintenance
## Independence of Parameters in Layout files
When a Turnout or Structure is place on the layout, all of its information is copied to layout file.  There is no dependency on the Parameter file.

If a change is made to Parameter file, such as adding art-work to a *Turnout* definition, the existing copies of that definition can be updated by the Manage/Update Turnouts and Structures command.

##   CONTENTS Label

Each Parameter file must have a least one CONTENTS line. The first is used as the CONTENTS Label.  Any others as used as described below.
Parameter files are distinquished by their CONTENTS Label.  A map from CONTENTS Label to file name is maintained.  This map is implemented by Preferences entries:
Parameter File Map.Kato Unitrack HO Scale: ../params/HO-Kato.xtp
If the current XTrkCad library directory has changed, then these entries are updated to the new location.
These entries accumulate, they are never purged.  But unreferenced CONTENTS Labels are ignoreed.
The active Parameter file list is implemented by Preferences entries:
```c
Parameter File Names.File1: Kato Unitrack HO Scale
Parameter File Names.File2:
```

The list is terminated by an empty value
## Adding / Deleting Parameter files
A new Parameter file is added to the parms/ directory and is available when the user accesses the Parameter library.
A deleted Paramter is removed from the params/ directory. If the user had loaded that Parameter file, then an error will be raised saying the Parameter file can not be found and will be moved from the Parameter file list.
## Changing CONTENTS Label and/or FileName
Occansionally you may need to rename a parameter file or change its CONTENTS Label. This can lead to missing file errors.
For example, we have a paramter file XYZ.xtp with CONTENTS Label
```c
CONTENTS XYZ Parameters
```

We want to rename this file to HO-XYZ.xtp and change its CONTENTS Label

```c
 CONTENTS HO XYZ Parameters
```

You need to do 2 things when you submit an updated file;

1 Add the old CONTENTS line to the new param file (HO-XYZ.xtp). Note: the first line is the new Contents Label

```c
CONTENTS HO XYZ Parameters
CONTENTS XYZ Parameters
```

2. Update xtrkcad.upd with the current date and append the name of the new param file

```c
...
20220108
...
HO-XYZ.xtp
```

If you are just updating the Contents Label, you only need to do step 1
If you are just changing the file name, you only need to do step 2.

# Pretty Printing
Code is formatted (pretty-printed) using the astyle tool with the config file astylerc or AStyle.cfg found in $SRCDIR/app/lib.

It is important to be able to separate pretty-print changes and functional changes since pretty-print changes will have no effect on functionality.
It is recommended that code files in $SRCDIR/app/bin (*.c *..h) be pretty-printed prior to making functional  changes. This should affect no files if developers are consistent in pretty-printing their changes.  If there are pretty-print changes, they should be checked-in before any other code chages. This allows us to separate pretty-print changes from functional changes.

Prior to check-in of functional changes you should pretty-print the code files, which should only affect your changes

# DynString
```c
  size_t DynStringSize(DynString *s)
  DynStringsGet the current length of the string.
  Allocate memory for a string of the desired length.
  DynString *DynStringMalloc(DynString *s, size_t size)
  Try to compact string memory by reallocating a buffer large enough for the current string content.
  void DynStringRealloc(DynString *s)
  Clear the dynamic string.
  void DynStringClear(DynString *s)
  Clear the dynamic string contents without changing memory allocation.
  void DynStringReset(DynString *s)
  Resize the string for a minimum number of bytes.
  void DynStringResize(DynString *s, size_t size)
  Free the previously allocated string.
  void DynStringFree(DynString *s)
  Create a newly allocated copy of the passed dynamic string.
  DynString *DynStringDupStr(DynString *s2, DynString *s)
  Copy the memory from the source string into the dest string.
  void DynStringCpyStr(DynString *dest, DynString *src)
  Return the content of the dynamic string as a \0 terminated C string.
  char *DynStringToCStr(DynString *s)
  Concatenate a number of bytes from the source string to the dynamic string.
  void DynStringNCatCStr(DynString *s, size_t len, const char *str)
  Concatenate a zero-terminated source string to the dynamic string.
  void DynStringCatCStr(DynString *s, const char *str)
  Concatenate a dynamic string to another dynamic string.
  void DynStringCatStr(DynString *s, const DynString *s2)
  Concatenate a variable number zero terminated strings to the dynamic string.
  void DynStringCatCStrs(DynString *s, ...)
  Concatenate a variable number of dynamic string to another dynamic string.
  void DynStringCatStrs(DynString *s1, ...)
  Return a formatted dynamic string.
  void DynStringPrintf(DynString *s, const char *fmt, ...)
```
## Example from layout.c with additional notes:
```c
/**
 * @brief Set the Layout background full path.
 * @param fileName The Layout background full path string.
*/
void SetLayoutBackGroundFullPath(const char *fileName)
{
	if (fileName && fileName[0]) {
// Make sure there is a string of length > 0 before clearing it
		if (DynStringSize(&thisLayout.backgroundFileName)) {
if (strcmp(DynStringToCStr(&thisLayout.backgroundFileName),fileName) == 0) {
				return;
			}
			DynStringClear(&thisLayout.backgroundFileName);
		}
		// Allocate memory for the string and add it to the cleared DynString
		DynStringMalloc(&thisLayout.backgroundFileName, strlen(fileName) + 1);
		DynStringCatCStr(&thisLayout.backgroundFileName, fileName);
	} else {
		// If there is not a filename, set it to an empty string
		DynStringClear(&thisLayout.backgroundFileName);
		DynStringMalloc(&thisLayout.backgroundFileName, 1);
		DynStringCatCStr(&thisLayout.backgroundFileName, "");
	}
}

```

# Suggested Topics
## Undo Processing
## Draw Model
## Headers
## ifdef WINDOWS

