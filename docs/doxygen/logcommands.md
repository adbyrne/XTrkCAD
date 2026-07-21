\page log Log Commands

Catalog of the built-in per-module debug-logging categories, each set by name via
`-d <category>=<level>` on the command line — see
\ref running-with-debug-logging "Running with debug logging" in the
\ref index "Developer Documentation" page for the full flag syntax and how to add a new category
here. Every entry below is generated from an `@logcmd` doc comment next to that
category's registration in the source, so this list stays in sync with the code; the entry shows
the `category=n` syntax to pass on the command line, the file that registers it, and — where
written — a short note on what it actually logs. A category with no note is usually
self-explanatory from its name and the file it lives in; follow the file link to the source for
specifics.
