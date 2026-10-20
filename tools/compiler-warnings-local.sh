#!/usr/bin/env bash
# Local-only compiler-warnings report with vendored app/tools/halibut/ noise
# filtered out. ci-gtk3.yml's compiler-warnings job (and its published
# GitHub Pages version in release.yml) deliberately capture the *unfiltered*
# -Wall -Wextra output, matching every other report-only job's convention of
# showing the whole tree -- halibut is vendored and out of scope for fixes
# (see the "Vendored-code exclusion" notes elsewhere in ci-gtk3.yml), but its
# volume of pre-existing warnings can bury real findings in the app's own
# code when just eyeballing the report locally. This script is for that
# local read, not a CI change.
#
# Usage: tools/compiler-warnings-local.sh [--html <output-dir>] [<existing-warnings.txt>]
#
# With no positional argument, configures and builds a fresh out-of-source
# tree (build-compiler-warnings-local/) with the same -Wall -Wextra
# -Wno-unused-parameter flags as ci-gtk3.yml's compiler-warnings job, so the
# filtered counts are directly comparable to that job's unfiltered ones.
# Pass an existing warnings.txt (e.g. one already captured from your own
# build) to skip rebuilding.
#
# --html <dir> additionally writes a browsable HTML report via the existing
# format-findings-report.sh into <dir>/index.html, with file:line links
# pointing at the local working tree instead of a GitHub blob URL.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HTML_DIR=""
IN_FILE=""

while [ "$#" -gt 0 ]; do
	case "$1" in
	--html)
		HTML_DIR="$2"
		shift 2
		;;
	*)
		IN_FILE="$1"
		shift
		;;
	esac
done

if [ -z "$IN_FILE" ]; then
	BUILD_DIR="$REPO_ROOT/build-compiler-warnings-local"
	echo "No warnings.txt given -- building in $BUILD_DIR (-Wall -Wextra, matching ci-gtk3.yml's compiler-warnings job)" >&2
	cmake -B "$BUILD_DIR" -S "$REPO_ROOT" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DXTRKCAD_TESTING=ON \
		-DCMAKE_C_FLAGS="-Wall -Wextra -Wno-unused-parameter" >&2
	cmake --build "$BUILD_DIR" 2>&1 | tee "$BUILD_DIR/warnings.txt" >&2 || true
	IN_FILE="$BUILD_DIR/warnings.txt"
fi

FILTERED="$(mktemp)"
trap 'rm -f "$FILTERED"' EXIT
grep -v '/halibut/' "$IN_FILE" > "$FILTERED" || true

RAW_COUNT=$(grep -c ': warning:' "$IN_FILE" || echo 0)
COUNT=$(grep -c ': warning:' "$FILTERED" || echo 0)
echo "Total warnings: $COUNT (halibut/ filtered out $((RAW_COUNT - COUNT)) of $RAW_COUNT)"
echo ""
echo "=== By warning flag ==="
grep ': warning:' "$FILTERED" | grep -oE '\[-W[^]]+\]' | sort | uniq -c | sort -rn || true
echo ""
echo "=== By file (top 20) ==="
grep ': warning:' "$FILTERED" | grep -oE '^[^:]+' | sed 's|.*/||' | sort | uniq -c | sort -rn | head -20 || true

if [ -n "$HTML_DIR" ]; then
	mkdir -p "$HTML_DIR"
	"$REPO_ROOT/tools/format-findings-report.sh" "$FILTERED" "$HTML_DIR/index.html" \
		"Compiler warnings, -Wall -Wextra (halibut/ filtered, local)" bracketed \
		"file://$REPO_ROOT/"
	echo ""
	echo "HTML report: $HTML_DIR/index.html"
fi
