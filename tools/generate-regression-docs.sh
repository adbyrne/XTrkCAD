#!/usr/bin/env bash
# Generate docs/doxygen/regression-tests.md and its screenshots.
#
# Builds the current working tree, runs every demo once with
# -d visualdiff=1 -T under Xvfb + an isolated $HOME, collects each demo's
# end-of-playback ("final") screenshot, and emits one page section per demo:
# the demo's own title (from app/lib/xtrkcad.xtq) and its introductory
# MESSAGE text (from the demo .xtr file itself) as the description, plus the
# screenshot. A few demos (pure dialog/mouse-interaction ones with no track
# content, e.g. dmdialog.xtr) have nothing to screenshot -- those get a note
# instead of a broken image reference, not an error.
#
# Some demos may warrant additional mid-script screenshots later -- the
# -d visualdiff=1 mechanism already captures one per REGRESSION block too
# (see tools/regression-visual-diff.sh); this generator only wires up the
# "final view" for now, per the initial scope.
#
# Usage: tools/generate-regression-docs.sh [--jobs N] [--keep-scratch]
#
# See docs/doxygen/mainpage.md's "Advanced/optional local checks" section for
# background on -d visualdiff=1.

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
XTQ="$REPO_ROOT/app/lib/xtrkcad.xtq"
DEMOS_DIR="$REPO_ROOT/app/lib/demos"
IMAGES_DIR="$REPO_ROOT/docs/doxygen/images/regression-tests"
PAGE_PATH="$REPO_ROOT/docs/doxygen/regression-tests.md"

JOBS=4
KEEP_SCRATCH=0
while [ "$#" -gt 0 ]; do
    case "$1" in
        --jobs) JOBS="$2"; shift 2 ;;
        --keep-scratch) KEEP_SCRATCH=1; shift ;;
        *) echo "Usage: $0 [--jobs N] [--keep-scratch]" >&2; exit 1 ;;
    esac
done

SCRATCH="$(mktemp -d)"
cleanup() {
    if [ "$KEEP_SCRATCH" -eq 1 ]; then
        echo "Scratch kept at: $SCRATCH"
    else
        rm -rf "$SCRATCH"
    fi
}
trap cleanup EXIT

BUILD="$SCRATCH/build"
INSTALL="$SCRATCH/install"
HOME_SCRATCH="$SCRATCH/home"
mkdir -p "$HOME_SCRATCH"

echo "Building..." >&2
cmake -B "$BUILD" -S "$REPO_ROOT" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DXTRKCAD_TESTING=ON -DXTRKCAD_REGRESSION_TESTING=ON >&2
cmake --build "$BUILD" --parallel "$JOBS" >&2
cmake --install "$BUILD" --prefix "$INSTALL" >&2

BIN=$(find "$INSTALL" -type f -name "xtrkcad-*" | head -1)
if [ -z "$BIN" ]; then
    echo "ERROR: no installed xtrkcad binary found under $INSTALL" >&2
    exit 1
fi

echo "Running full demo suite with -d visualdiff=1..." >&2
# REGRESSION FAILs are expected/don't matter here -- this only cares about
# the screenshots, not the text comparison.
xvfb-run -a -- env HOME="$HOME_SCRATCH" "$BIN" -d visualdiff=1 -T >&2 || true

mkdir -p "$IMAGES_DIR"

extract_description() {
    # First MESSAGE...END block's prose, reflowed into blank-line-separated
    # paragraphs; escapes backslashes (Doxygen's command prefix) and angle
    # brackets (demo prose references UI labels like "<Describe>", which
    # Doxygen's markdown-to-XML pass otherwise reads as an unknown HTML tag).
    awk '
        /^MESSAGE/ { p=1; next }
        p && /^END/ { exit }
        p {
            line = $0
            # Cosmetic divider lines ("___..." or "===...", leading or
            # trailing) -- drop wherever they appear, not just the first
            # line. No backreferences: not all awk regex engines support
            # them, so match the two divider characters actually seen
            # rather than "any repeated character".
            if (line ~ /^_+$/ || line ~ /^=+$/) next
            if (line == "") {
                if (para != "") { print para "\n"; para="" }
            } else {
                para = (para=="") ? line : para " " line
            }
        }
        END { if (para != "") print para }
    ' "$1" | sed -e 's/\\/\\\\/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g'
}

cat > "$PAGE_PATH" <<'HEADER'
\page regression-tests Regression Test Demos

One entry per demo-playback regression test (`RegressionTestAll()`, the `-T`
flag -- see \ref advanced-optional-local-checks "Advanced/optional local
checks"). The description is the demo's own introductory `MESSAGE` text; the
screenshot is the layout's final state after the whole script runs. Regenerate
both via `tools/generate-regression-docs.sh`. Some demos may warrant additional
mid-script screenshots later -- `-d visualdiff=1` already captures one per
`REGRESSION` block too, see \ref advanced-optional-local-checks.

HEADER

DEMO_COUNT=0
MISSING_COUNT=0
MISSING_LIST=()

while IFS=$'\t' read -r title file; do
    [ -z "$file" ] && continue
    DEMO_COUNT=$((DEMO_COUNT + 1))
    stem="${file%.xtr}"
    png_name="${stem}-final.png"
    src_png=$(find "$HOME_SCRATCH" -path "*/visualdiff/$png_name" | head -1)

    {
        # # (not ##): Doxygen errors "found subsection command outside of
        # section context" if a page's first heading is ## with an explicit
        # {#anchor} and no preceding # heading (confirmed empirically -- see
        # advanced.md's own devguide note on the same quirk).
        echo "# $title (\`$file\`) {#demo-$stem}"
        echo
        extract_description "$DEMOS_DIR/$file"
        echo
        if [ -n "$src_png" ]; then
            cp "$src_png" "$IMAGES_DIR/$png_name"
            echo "\\image html $png_name \"$title -- final state\""
        else
            MISSING_COUNT=$((MISSING_COUNT + 1))
            MISSING_LIST+=("$file")
            echo "*(no track content in this demo -- nothing to screenshot)*"
        fi
        echo
    } >> "$PAGE_PATH"
# Title-then-tab-then-filename, one DEMO line at a time; awk (not sed's \t,
# a GNU extension BSD/macOS sed doesn't support) for portability.
done < <(grep '^DEMO ' "$XTQ" | awk -F'"' '{
    rest = $3
    sub(/^ +/, "", rest)
    split(rest, a, " ")
    print $2 "\t" a[1]
}')

echo "Wrote ${PAGE_PATH#"$REPO_ROOT"/} ($DEMO_COUNT demos, $MISSING_COUNT with no track content)"
if [ "$MISSING_COUNT" -gt 0 ]; then
    echo "No screenshot (no track content): ${MISSING_LIST[*]}"
fi
