#!/usr/bin/env bash
# Renders the same demo(s) from two git refs and diffs the resulting images.
# A text diff of REGRESSION-block track coordinates can miss a change that's
# obvious in a picture -- SF #667/#26: a fixture regen silently dropped a
# join's connector tracks, and the numeric diff alone didn't make that
# visible. Useful for join/curve/geometry-touching changes specifically, not
# a replacement for the normal ctest-based regression suite.
#
# Usage: tools/regression-visual-diff.sh <before-ref> <after-ref> <demo.xtr> [<demo2.xtr> ...]
#
# Builds each ref in its own git worktree + scratch install + scratch $HOME
# (mirroring app/bin/RegressionTests.cmake's own isolation), runs it with
# -d visualdiff=1 -T<demos>, then pairs up same-named PNGs and diffs them
# with ImageMagick. Results land in ./regression-visual-diff-<timestamp>/ in
# the current directory -- not cleaned up automatically, since that's the
# whole point of running this.
#
# Requires: cmake, ninja, xvfb-run, ImageMagick (compare/convert or magick),
# and this branch's normal build deps (see tools/check-build-deps.sh). See
# docs/doxygen/mainpage.md's "Advanced/optional local checks" section for
# background on -d visualdiff=1.

set -uo pipefail

if [ "$#" -lt 3 ]; then
    echo "Usage: $0 <before-ref> <after-ref> <demo.xtr> [<demo2.xtr> ...]" >&2
    exit 1
fi

BEFORE_REF="$1"
AFTER_REF="$2"
shift 2
DEMOS=("$@")

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SCRATCH="$(mktemp -d)"
OUT_DIR="$(pwd)/regression-visual-diff-$(date +%Y%m%d-%H%M%S)"
WORKTREES=()

cleanup() {
    for wt in "${WORKTREES[@]:-}"; do
        [ -n "$wt" ] && git -C "$REPO_ROOT" worktree remove --force "$wt" 2>/dev/null
    done
    rm -rf "$SCRATCH"
}
trap cleanup EXIT

if command -v magick >/dev/null 2>&1; then
    IM_COMPARE=(magick compare)
    IM_CONVERT=(magick convert)
else
    IM_COMPARE=(compare)
    IM_CONVERT=(convert)
fi

# Renders $1 (a git ref) into its own worktree/build/install/scratch-home,
# labeled $2 ("before"/"after"). Prints one absolute PNG path per line on
# stdout; all other output goes to stderr so callers can safely capture just
# the paths.
render_ref() {
    local ref="$1" label="$2"
    local worktree="$SCRATCH/worktree-$label"
    local build="$SCRATCH/build-$label"
    local install="$SCRATCH/install-$label"
    local home="$SCRATCH/home-$label"
    local bin demo_args

    echo "=== $label: $ref ===" >&2
    git -C "$REPO_ROOT" worktree add --detach "$worktree" "$ref" >&2
    WORKTREES+=("$worktree")
    cmake -B "$build" -S "$worktree" -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug -DXTRKCAD_TESTING=ON -DXTRKCAD_REGRESSION_TESTING=ON >&2
    cmake --build "$build" >&2
    cmake --install "$build" --prefix "$install" >&2
    mkdir -p "$home"
    bin=$(find "$install" -maxdepth 3 -type f -name "xtrkcad-*" | head -1)
    if [ -z "$bin" ]; then
        echo "ERROR: no installed xtrkcad binary found under $install" >&2
        return 1
    fi
    demo_args=$(IFS=,; echo "${DEMOS[*]}")
    # Regression CHECK failures are expected/normal here (that's the whole
    # point of an "after" run against a stale fixture) -- don't let a
    # non-zero exit from a real REGRESSION FAIL abort the script.
    xvfb-run -a -- env HOME="$home" "$bin" -d visualdiff=1 -T"$demo_args" >&2 || true
    find "$home" -path "*/visualdiff/*.png"
}

read_lines() {
    # mapfile is a bash-4+ builtin; macOS ships bash 3.2 (c-tests-macos
    # builds on it), so use a portable read loop instead.
    local var_name="$1" line
    local -a lines=()
    while IFS= read -r line; do
        lines+=("$line")
    done
    eval "$var_name=(\"\${lines[@]}\")"
}

echo "Rendering 'before' ($BEFORE_REF)..." >&2
read_lines BEFORE_PNGS < <(render_ref "$BEFORE_REF" before)
echo "Rendering 'after' ($AFTER_REF)..." >&2
read_lines AFTER_PNGS < <(render_ref "$AFTER_REF" after)

if [ "${#BEFORE_PNGS[@]}" -eq 0 ] || [ "${#AFTER_PNGS[@]}" -eq 0 ]; then
    echo "ERROR: no visualdiff PNGs produced for one or both refs -- check the demo name(s) and stderr output above." >&2
    exit 1
fi

mkdir -p "$OUT_DIR"
MATCHED=0
for before_png in "${BEFORE_PNGS[@]}"; do
    name=$(basename "$before_png")
    after_png=""
    for candidate in "${AFTER_PNGS[@]}"; do
        if [ "$(basename "$candidate")" = "$name" ]; then
            after_png="$candidate"
            break
        fi
    done
    if [ -z "$after_png" ]; then
        echo "SKIP: $name has no 'after' match (before-only REGRESSION block/line-number shift?)" >&2
        continue
    fi
    MATCHED=$((MATCHED + 1))
    ae=$("${IM_COMPARE[@]}" -metric AE "$before_png" "$after_png" "$OUT_DIR/$name" 2>&1 || true)
    "${IM_CONVERT[@]}" +append "$before_png" "$after_png" "$OUT_DIR/sidebyside-$name"
    echo "$name: pixel-diff(AE)=$ae"
done

for after_png in "${AFTER_PNGS[@]}"; do
    name=$(basename "$after_png")
    found=0
    for before_png in "${BEFORE_PNGS[@]}"; do
        [ "$(basename "$before_png")" = "$name" ] && found=1 && break
    done
    [ "$found" -eq 0 ] && echo "SKIP: $name has no 'before' match" >&2
done

echo
echo "$MATCHED matched pair(s). Results in: $OUT_DIR"
echo "  <name>.png            -- ImageMagick AE diff (highlighted differences)"
echo "  sidebyside-<name>.png -- before | after, side by side"
