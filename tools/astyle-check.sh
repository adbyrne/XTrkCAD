#!/usr/bin/env bash
# The astyle conformance check: does every tracked app/bin, app/wlib,
# app/help and app/tools file match app/lib/astylerc? Shared by ci-gtk3.yml's
# astyle-check (the gate) and its macOS/Windows comparison jobs, and the same
# command a developer runs locally before pushing. ASTYLE_VERSION below is
# the one place the pinned astyle version is set (see SF #638 for why it's
# pinned, and ci-gtk3.yml's astyle-check comment for the exclusion list).
#
# Usage: tools/astyle-check.sh [--build] [--warn-only <platform>]
#   (run from the repository root)
#   --build      Download and build the pinned astyle into ./astyle-<version>/
#                and use that (CI does this; locally, use your own astyle).
#   --warn-only  Report differences as a GitHub warning naming <platform>
#                instead of failing.
#   ASTYLE=/path/to/astyle overrides the binary when --build isn't given.

set -euo pipefail

ASTYLE_VERSION=3.6.13
ASTYLE=${ASTYLE:-astyle}
BUILD=0
WARN_PLATFORM=""

while [ $# -gt 0 ]; do
	case "$1" in
	--build) BUILD=1 ;;
	--warn-only) WARN_PLATFORM="$2"; shift ;;
	*) echo "usage: $0 [--build] [--warn-only <platform>]" >&2; exit 2 ;;
	esac
	shift
done

if [ "$BUILD" = 1 ]; then
	curl -sL -o astyle.tar.bz2 "https://sourceforge.net/projects/astyle/files/astyle/astyle%20${ASTYLE_VERSION%.*}/astyle-${ASTYLE_VERSION}.tar.bz2/download"
	tar xjf astyle.tar.bz2
	make -C "astyle-${ASTYLE_VERSION}/build/gcc" -j"$(getconf _NPROCESSORS_ONLN)"
	ASTYLE="$PWD/astyle-${ASTYLE_VERSION}/build/gcc/bin/astyle"
fi

VERSION_LINE=$("$ASTYLE" --version)
echo "$VERSION_LINE"
case "$VERSION_LINE" in
*"$ASTYLE_VERSION"*) ;;
*) echo "::warning::astyle is not the pinned $ASTYLE_VERSION; results may differ from CI (SF #638)." ;;
esac

OUTPUT=$("$ASTYLE" --options=app/lib/astylerc --dry-run -Q --recursive --ignore-exclude-errors \
	--exclude=wrapbox \
	--exclude=unittest \
	--exclude=dirent.c \
	--exclude=dirent.h \
	"app/bin/*.c" "app/bin/*.h" "app/wlib/*.c" "app/wlib/*.h" "app/help/*.c" "app/help/*.h" "app/tools/*.c" "app/tools/*.h" 2>&1 | grep '^Formatted' || true)

if [ -n "$OUTPUT" ]; then
	echo "$OUTPUT"
	if [ -n "$WARN_PLATFORM" ]; then
		echo "::warning::$WARN_PLATFORM astyle disagrees with the pinned Linux astyle-check gate on the files above."
		exit 0
	fi
	echo "::error::Non-conformant files found. Run: astyle --options=app/lib/astylerc <file>"
	exit 1
fi
if [ -n "$WARN_PLATFORM" ]; then
	echo "$WARN_PLATFORM astyle agrees with the Linux gate: all files conform."
else
	echo "All files conform to app/lib/astylerc."
fi
