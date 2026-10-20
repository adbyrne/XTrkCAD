#!/usr/bin/env bash
# Inventory every cppcheck-suppress and clang-tidy NOLINT(NEXTLINE) comment
# under app/bin, classifying cppcheck-suppress reasons as boilerplate
# (generic "confirmed via broad sampling" text from a prior bulk-approval
# pass), specific (individualized reasoning), or unreasoned (no reason found
# at all). A boilerplate suppress is the one most likely to be hiding a
# fully-removable case -- see SF #749's ccornu.c ep discovery. Output is
# bracketed-category lines compatible with format-findings-report.sh's
# "bracketed" input format:
#   file:line: <reason or note> [category]
#
# A reason is looked for in three places, in order: (1) inline after "--" on
# the suppress line itself (the prevailing convention, e.g.
# "// cppcheck-suppress shadowVariable -- reason"); (2) a same-line trailing
# block comment after the suppress comment closes (e.g.
# "/* cppcheck-suppress uninitvar */ /* reason */"); (3) one or more // or
# /* */ comment lines immediately following the suppress line, up to the
# first line of actual code (e.g. dlayer.c's arrayIndexOutOfBoundsCond pair,
# which explain the suppress on the next two lines instead of inline).
# Found 2026-08-31: the original inline-only check misclassified about half
# of the suppresses lacking an inline "--" as "(no reason given)" when they
# had a perfectly good reason one or two lines below -- caught by the user
# spot-checking the published report against dlayer.c's source.
#
# Found 2026-09-02, once app/wlib/gtk3lib was added as a root: the category
# regex didn't handle cppcheck's block form (`cppcheck-suppress-begin CAT`
# / `cppcheck-suppress-end CAT`, used in drawcairo.c) -- the `-begin`/`-end`
# suffix broke the `[[:space:]]+` match right after "cppcheck-suppress",
# so the whole unmatched line fell through as a garbled category string.
# Fixed by making that suffix optional in the regex.
#
# Usage: scan-suppressions.sh [source-root ...] > suppressions.txt
#   One or more source-roots, each scanned non-recursively (*.c/*.h directly
#   under it, matching the same non-recursive-glob convention the cppcheck
#   CI jobs use to stay out of vendored/test subdirs like wrapbox/, win32/,
#   unittest/ by construction). Defaults to app/bin alone if none given.
#   app/wlib/gtk3lib (+ its unix/ subdir) added 2026-09-02, once cppcheck's
#   own scope widened there and picked up real suppress comments (8 lines
#   across drawcairo.c/togglegroup.c/stickytogglebutton.h) that this
#   inventory had been silently missing.

set -euo pipefail

if [ "$#" -eq 0 ]; then
	ROOTS=(app/bin)
else
	ROOTS=("$@")
fi

BOILERPLATE_RE='confirmed via broad sampling this session|freshly declared and consumed within each iteration, never escapes the loop'

# Scans consecutive // or /* */ comment lines starting at $2 in file $1,
# stopping at the first line that isn't a comment (including EOF). Prints
# the concatenated, marker-stripped text, or nothing if none found.
trailing_comment_reason() {
	local f="$1" n="$2" reason="" trimmed piece
	while true; do
		trimmed=$(sed -n "${n}p" "$f" | sed -E 's/^[[:space:]]*//')
		if echo "$trimmed" | grep -qE '^//'; then
			piece=$(echo "$trimmed" | sed -E 's~^//[[:space:]]*~~')
		elif echo "$trimmed" | grep -qE '^/\*'; then
			piece=$(echo "$trimmed" | sed -E 's~^/\*[[:space:]]*~~; s~[[:space:]]*\*/[[:space:]]*$~~')
		else
			break
		fi
		if [ -z "$reason" ]; then
			reason="$piece"
		else
			reason="$reason $piece"
		fi
		n=$((n+1))
	done
	printf '%s' "$reason"
}

shopt -s nullglob
for ROOT in "${ROOTS[@]}"; do
for f in "$ROOT"/*.c "$ROOT"/*.h; do
	[ -f "$f" ] || continue
	grep -n "cppcheck-suppress\|NOLINTNEXTLINE(\|NOLINT(" "$f" 2>/dev/null | while IFS=: read -r lineno content; do
		if echo "$content" | grep -q "cppcheck-suppress"; then
			cat=$(echo "$content" | sed -E 's/.*cppcheck-suppress(-begin|-end)?[[:space:]]+([A-Za-z0-9_]+).*/\2/')
			if echo "$content" | grep -q -- '--'; then
				reason=$(echo "$content" | sed -E 's/.*--[[:space:]]*//')
			elif echo "$content" | grep -qE '\*/[[:space:]]*/\*'; then
				reason=$(echo "$content" | sed -E 's~.*/\*[[:space:]]*~~; s~[[:space:]]*\*/[[:space:]]*$~~')
			else
				reason=$(trailing_comment_reason "$f" "$((lineno+1))")
			fi
			if [ -n "$reason" ]; then
				if echo "$reason" | grep -qE "$BOILERPLATE_RE"; then
					printf '%s:%s: %s [%s-boilerplate]\n' "$f" "$lineno" "$reason" "$cat"
				else
					printf '%s:%s: %s [%s-specific]\n' "$f" "$lineno" "$reason" "$cat"
				fi
			else
				printf '%s:%s: (no reason given) [%s-unreasoned]\n' "$f" "$lineno" "$cat"
			fi
		else
			check=$(echo "$content" | sed -E 's/.*NOLINT(NEXTLINE)?\(([A-Za-z0-9_.,-]+)\).*/\2/')
			printf '%s:%s: clang-tidy suppress (no reason comment) [%s]\n' "$f" "$lineno" "$check"
		fi
	done || true
done
done
