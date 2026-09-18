#!/usr/bin/env python3
"""Verify every runtime wHelp() topic XTrkCAD can actually open resolves to a
real page in the built User Guide (the help-html CMake target).

Covers the two link families found while investigating SF #729/#730:

  - Command-context help (F1): app/wlib/gtk3lib/help.c's DoHelpMenu() opens
    GetCurCommandName(), which only ever returns a helpKey registered via
    AddMenuButton() in app/bin/*.c (traced through menu.c's AddMenuButton ->
    AddCommand wrapper chain -- AddToolbarButton's helpStr is tooltip text
    only, not reachable from GetCurCommandName()).

  - Per-dialog Help button: app/bin/form/dialog.c's FormCreateDialog()
    builds its "id_help" button's context via GetDialogHelpTopic() (SF
    #730), given a paramGroup_t's own name (app/bin/*.c). That function
    returns either a small alias-table hit, or -- the long-standing default,
    unchanged from before #730 -- "cmd" + the name with its first letter
    capitalized (e.g. "block" -> "cmdBlock"). dialog->name itself is never
    touched: it separately keys window-position prefs and every field-level
    tooltip ("<dialog>-<field>" in tooltip.c), so renaming it to fix a
    broken Help link would silently break both of those.

    NOTE: app/wlib/gtk3lib/dialog.c ALSO has a GTK_RESPONSE_HELP case that
    looks like a second Help-button path, but it is entirely dead code --
    wWinDialogCreate() (the only place a dialog created this way gets built)
    never connects a "response" signal to anything, so that switch statement
    is never reached regardless of what it contains. (Its containing
    function is additionally broken by an unrelated unclosed `/**` comment
    that swallows the whole function body -- found and left as-is, tracked
    separately, since fixing it wouldn't make the function reachable.)
    FormCreateDialog()'s direct per-button "clicked" handler (ButtonHelp) is
    the actual live mechanism for every paramGroup_t dialog's Help button.

Used both as a CTest test (app/doc/CMakeLists.txt, when Doxygen is found)
and by CI's help-links-check job -- one source of truth for both, instead of
the two ever drifting apart.

A known-gap allowlist (app/doc/known_missing_dialog_help.txt, one bare
dialog name per line, `#`-comments allowed) lets SF #730 land in verified
batches without holding every dialog for the last one -- same pattern SF
#729/PR #128 used while its own content fix was still pending. Only dialog
names can go in it (command-context keys are already 100% fixed, zero
exceptions expected there); remove an entry the moment its dialog gets
aliased or a real page, so the gate keeps covering everything it can.

Usage: check_help_links.py <source-root> <built-html-dir>
"""
import glob
import os
import re
import sys


def extract_call_args(text, func_name):
    """Yield each top-level, comma-split argument list for every call to
    func_name(...) in text, matching balanced parens across newlines --
    a plain per-line regex misses any call whose args span multiple lines
    (found the hard way: SF #729's first-draft check silently skipped
    cmdStructureHotBar this way)."""
    for m in re.finditer(re.escape(func_name) + r"\s*\(([^;]*?)\)", text, re.S):
        parts, depth, cur = [], 0, ""
        for ch in m.group(1):
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
            if ch == "," and depth == 0:
                parts.append(cur)
                cur = ""
            else:
                cur += ch
        parts.append(cur)
        yield parts


def literal_string(arg):
    m = re.match(r'^\s*"([A-Za-z0-9_]+)"', arg)
    return m.group(1) if m else None


def command_context_keys(bin_dir):
    keys = set()
    for path in glob.glob(os.path.join(bin_dir, "*.c")):
        text = open(path).read()
        for args in extract_call_args(text, "AddMenuButton"):
            if len(args) >= 3:
                key = literal_string(args[2])
                if key:
                    keys.add(key)
    return keys


def dialog_names(bin_dir):
    names = set()
    for path in glob.glob(os.path.join(bin_dir, "*.c")):
        text = open(path).read()
        for m in re.finditer(
                r'paramGroup_t\s+\w+\s*=\s*\{\s*"([A-Za-z0-9_]+)"', text):
            names.add(m.group(1))
    return names


def default_dialog_topic(name):
    """Mirrors GetDialogHelpTopic()'s (app/bin/form/dialog.c) fallback
    exactly: "cmd" + name with its first letter capitalized."""
    return "cmd" + name[:1].upper() + name[1:]


def dialog_help_aliases(dialog_c_path):
    """Parse the { "dialogName", "page" } entries out of
    app/bin/form/dialog.c's dialogHelpAliases[] table (the sentinel
    { NULL, NULL } row is skipped since neither field is a quoted string)."""
    text = open(dialog_c_path).read()
    m = re.search(r"dialogHelpAliases\[\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if not m:
        sys.exit("could not find dialogHelpAliases[] in " + dialog_c_path)
    aliases = {}
    for pair in re.finditer(r'\{\s*"([A-Za-z0-9_]+)"\s*,\s*"([A-Za-z0-9_]+)"\s*\}', m.group(1)):
        aliases[pair.group(1)] = pair.group(2)
    return aliases


def known_missing_dialogs(doc_dir):
    path = os.path.join(doc_dir, "known_missing_dialog_help.txt")
    if not os.path.isfile(path):
        return set()
    names = set()
    for line in open(path):
        line = line.split("#", 1)[0].strip()
        if line:
            names.add(line)
    return names


def main():
    if len(sys.argv) != 3:
        sys.exit(f"usage: {sys.argv[0]} <source-root> <built-html-dir>")
    source_root, html_dir = sys.argv[1], sys.argv[2]
    bin_dir = os.path.join(source_root, "app", "bin")
    doc_dir = os.path.join(source_root, "app", "doc")

    topics = command_context_keys(bin_dir)
    topics.add("index")

    aliases = dialog_help_aliases(os.path.join(bin_dir, "form", "dialog.c"))
    dialogs = dialog_names(bin_dir)
    known_missing = known_missing_dialogs(doc_dir)
    dialog_topics = {name: aliases.get(name, default_dialog_topic(name)) for name in dialogs}

    missing = sorted(t for t in topics if not os.path.isfile(os.path.join(html_dir, f"{t}.html")))
    missing += sorted(
        page for name, page in dialog_topics.items()
        if name not in known_missing and not os.path.isfile(os.path.join(html_dir, f"{page}.html")))
    stale_known_missing = sorted(
        name for name in known_missing
        if name in dialog_topics and os.path.isfile(os.path.join(html_dir, f"{dialog_topics[name]}.html")))

    if missing:
        print(f"::error::wHelp() topics missing from the built User Guide: {' '.join(missing)}")
        print("Each AddMenuButton() helpKey needs a matching '\\page <key>' in app/doc/*.dox. "
              "Each paramGroup_t dialog name needs its default 'cmd'+Capitalize(name) page to "
              "exist, or an entry in app/bin/form/dialog.c's dialogHelpAliases[] table pointing "
              "at an existing page, or (if genuinely not yet scheduled) an entry in "
              "app/doc/known_missing_dialog_help.txt.")
        sys.exit(1)

    if stale_known_missing:
        print(f"::error::app/doc/known_missing_dialog_help.txt lists {' '.join(stale_known_missing)} "
              "as still missing, but they now resolve -- remove them so the gate covers this again.")
        sys.exit(1)

    print(f"All {len(topics) + len(dialog_topics)} wHelp() topics resolve to a real User Guide "
          f"page or a tracked known gap ({len(dialogs)} dialog names, {len(aliases)} aliased, "
          f"{len(known_missing)} tracked as still-open in known_missing_dialog_help.txt).")


if __name__ == "__main__":
    main()
