# File Menu {#fileM}

![File menu](mfile.png)

The **File Menu** shows file oriented commands for loading and saving layout files.

**File Types** The main two filetypes that XTrackCAD uses are normal Layout Files
(\*.xtc) and Archive Files (\*.xtce). The internals of both formats are described in
the XtrkCAD Wiki at [Wiki FileFormats](http://www.xtrkcad.org/Wikka/FileFormats).

**Layout File** The layout file is named with a _\*.xtc_ extension and is a
text-based description file of all the objects used in the layout plus some
information about the options currently in use.

**Archive** An archive is named with a _\*.xtce_ extension, is a zipped collection of:

- a _manifest_ JSON file that lists the contents,
- the .xtc layout file, and
- any other content listed in the manifest -- such as referenced image files.
  Initially the only such content is the background image which is added to the
  archive if it is present.

- **Exit** - Exits _XTrackCAD_. You will be asked to confirm your choice if there
  are unsaved changes.

- ![](bexport.png) **Export** - Exports objects to a file in _XTrackCAD_ (*.xti)
  format. If no objects are selected, all objects in visible layers are exported.
  If objects are selected they are exported. The exported file can then be
  imported into another layout design. Refer to the Import command listed below.

- ![](bexportbmap.png) **Export to Bitmap** - Creates a bitmap file
  (\ref cmdOutputbitmap "Export to Bitmap") of the layout. The bitmap can be
  saved in either JPEG or PNG format.

- ![](bnew.png) **New** - Clears the current layout. In case there are any
  unsaved changes on the current plan, a warning pop-up will be displayed and
  you'll have the option to cancel the operation.

- **Save As** - This command lets you make a copy of the track plan you are
  currently working on as an file (.xtc) or an archive (.xtce). It differs from
  the regular Save command. Save stores your data back into the folder
  (directory) it originally came from in the same filetype. "Save As" lets you
  give your plan a different name and/or put it in a different folder on your
  hard disk and change its filetype using the selection box at the bottom of the
  list of files or by hardcoding the extensions .xtc or .xtce (for an archive).

@note This is a proof-of-concept conversion covering a representative subset of
`filem.but`'s File Menu entries, not the full original page (see the halibut
original for the complete list). Written to validate the markup conversion and
the topic-anchor-preserving build pipeline, part of the SF #220-adjacent Halibut
&rarr; Doxygen investigation.
