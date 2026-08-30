#!/usr/bin/env bash
# Inventory every cppcheck-suppress and clang-tidy NOLINT(NEXTLINE) comment
# under app/bin, classifying cppcheck-suppress reasons as boilerplate
# (generic "confirmed via broad sampling" text from a prior bulk-approval
# pass), specific (individualized reasoning), or unreasoned (no "--" reason
# at all). A boilerplate suppress is the one most likely to be hiding a
# fully-removable case -- see SF #749's ccornu.c ep discovery. Output is
# bracketed-category lines compatible with format-findings-report.sh's
# "bracketed" input format:
#   file:line: <reason or note> [category]
#
# Usage: scan-suppressions.sh [source-root] > suppressions.txt
#   source-root defaults to app/bin (the only directory with any
#   cppcheck-suppress/NOLINT comments as of 2026-08-30 -- app/wlib,
#   app/help, app/tools, app/dynstring, app/cornu have none)

set -euo pipefail

ROOT="${1:-app/bin}"

BOILERPLATE_RE='confirmed via broad sampling this session|freshly declared and consumed within each iteration, never escapes the loop'

shopt -s nullglob
for f in "$ROOT"/*.c "$ROOT"/*.h; do
	[ -f "$f" ] || continue
	grep -n "cppcheck-suppress\|NOLINTNEXTLINE(\|NOLINT(" "$f" 2>/dev/null | while IFS=: read -r lineno content; do
		if echo "$content" | grep -q "cppcheck-suppress"; then
			cat=$(echo "$content" | sed -E 's/.*cppcheck-suppress[[:space:]]+([A-Za-z0-9_]+).*/\1/')
			if echo "$content" | grep -q -- '--'; then
				reason=$(echo "$content" | sed -E 's/.*--[[:space:]]*//')
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
