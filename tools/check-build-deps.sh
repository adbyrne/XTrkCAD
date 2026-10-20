#!/usr/bin/env bash
# Reports which tools needed to build XTrkCAD are present on this machine,
# with the exact install command for anything missing. Read-only: this
# script never installs or modifies anything itself.
#
# Usage: tools/check-build-deps.sh
#
# See docs/doxygen/devguide.md's "Building and testing" section for the
# full build/test commands once everything here is green.

set -uo pipefail

MISSING=0

# --- OS detection -----------------------------------------------------

case "$(uname -s)" in
    Linux)        OS=linux ;;
    Darwin)       OS=macos ;;
    MINGW*|MSYS*) OS=windows ;;
    *)
        echo "Unrecognized OS: $(uname -s)" >&2
        echo "This script supports Linux, macOS, and Windows (via an MSYS2 MINGW64 shell)." >&2
        exit 1
        ;;
esac

echo "Detected platform: $OS"
echo

# --- helpers ------------------------------------------------------------

# find_compiler: echoes the first working C compiler command name found.
find_compiler() {
    for c in cc gcc clang; do
        if command -v "$c" >/dev/null 2>&1; then
            echo "$c"
            return 0
        fi
    done
    return 1
}

check_cmd() {
    # check_cmd <command> <description> <install-hint>
    local cmd="$1" desc="$2" hint="$3"
    if command -v "$cmd" >/dev/null 2>&1; then
        local ver
        ver=$("$cmd" --version 2>&1 | head -1)
        printf "  [OK]      %-14s %s\n" "$cmd" "($ver)"
    else
        printf "  [MISSING] %-14s %s\n" "$cmd" "$desc"
        echo "            install: $hint"
        MISSING=$((MISSING + 1))
    fi
}

check_pkg() {
    # check_pkg <pkg-config-name> <description> <install-hint>
    local pkg="$1" desc="$2" hint="$3"
    if ! command -v pkg-config >/dev/null 2>&1; then
        printf "  [SKIP]    %-14s pkg-config itself is missing, can't check this one\n" "$pkg"
        return
    fi
    if pkg-config --exists "$pkg" 2>/dev/null; then
        local ver
        ver=$(pkg-config --modversion "$pkg" 2>/dev/null)
        printf "  [OK]      %-14s (%s)\n" "$pkg" "$ver"
    else
        printf "  [MISSING] %-14s %s\n" "$pkg" "$desc"
        echo "            install: $hint"
        MISSING=$((MISSING + 1))
    fi
}

check_header() {
    # check_header <header.h> <description> <install-hint>
    # Fallback for libraries with no pkg-config file (e.g. MiniXML) --
    # tries to actually compile a one-line #include, which works
    # regardless of what the library happens to be packaged as.
    local hdr="$1" desc="$2" hint="$3"
    local cc
    cc=$(find_compiler) || { echo "  [SKIP]    $hdr          no C compiler found yet, can't check"; return; }
    if echo "#include <$hdr>" | "$cc" -E -x c - >/dev/null 2>&1; then
        printf "  [OK]      %-14s header found\n" "$hdr"
    else
        printf "  [MISSING] %-14s %s\n" "$hdr" "$desc"
        echo "            install: $hint"
        MISSING=$((MISSING + 1))
    fi
}

# --- core build tools -----------------------------------------------------

echo "Core build tools:"

case "$OS" in
    linux)
        check_cmd cmake         "build system generator (need >= 3.20)" "sudo apt-get install cmake"
        check_cmd ninja         "build tool"                            "sudo apt-get install ninja-build"
        check_cmd "$(find_compiler || echo cc)" "C compiler"            "sudo apt-get install build-essential"
        check_pkg gtk+-3.0      "GTK+ 3 (UI toolkit)"                   "sudo apt-get install libgtk-3-dev"
        check_pkg zlib          "compression library"                  "sudo apt-get install zlib1g-dev"
        check_pkg libzip        "zip archive library"                  "sudo apt-get install libzip-dev"
        check_header mxml.h     "MiniXML (needed for SVG export)"       "sudo apt-get install libmxml-dev"
        check_cmd rsvg-convert  "SVG -> PNG conversion"                 "sudo apt-get install librsvg2-bin"
        check_cmd msgfmt        "gettext (translations)"                "sudo apt-get install gettext"
        check_pkg cmocka        "unit test framework (optional -- only needed for -DXTRKCAD_TESTING=ON)" "sudo apt-get install libcmocka-dev"
        ;;
    macos)
        check_cmd cmake         "build system generator (need >= 3.20)" "brew install cmake"
        check_cmd ninja         "build tool"                            "brew install ninja"
        check_cmd "$(find_compiler || echo cc)" "C compiler (Xcode Command Line Tools)" "xcode-select --install"
        check_pkg gtk+-3.0      "GTK+ 3 (UI toolkit)"                   "brew install gtk+3"
        check_pkg libzip        "zip archive library"                  "brew install libzip"
        check_cmd rsvg-convert  "SVG -> PNG conversion"                 "brew install librsvg"
        check_pkg cmocka        "unit test framework (optional)"       "brew install cmocka"
        echo
        echo "  Note: after 'brew install gtk+3', CMake also needs PKG_CONFIG_PATH set to"
        echo "  find it -- see devguide.md's macOS build section for the exact export."
        ;;
    windows)
        check_cmd cmake         "build system generator (need >= 3.20)" "pacman -S mingw-w64-x86_64-cmake"
        check_cmd ninja         "build tool"                            "pacman -S mingw-w64-x86_64-ninja"
        check_cmd "$(find_compiler || echo gcc)" "C compiler"           "pacman -S mingw-w64-x86_64-gcc"
        check_pkg gtk+-3.0      "GTK+ 3 (UI toolkit)"                   "pacman -S mingw-w64-x86_64-gtk3"
        check_pkg libzip        "zip archive library"                  "pacman -S mingw-w64-x86_64-libzip"
        check_header mxml.h     "MiniXML (needed for SVG export)"       "pacman -S mingw-w64-x86_64-mxml"
        check_cmd rsvg-convert  "SVG -> PNG conversion"                 "pacman -S mingw-w64-x86_64-librsvg"
        check_pkg cmocka        "unit test framework (optional)"       "pacman -S mingw-w64-x86_64-cmocka"
        echo
        echo "  Note: this script (and the actual build) must be run from an MSYS2 MINGW64"
        echo "  shell, not plain cmd.exe or PowerShell."
        ;;
esac

# --- optional docs/formatting tools ---------------------------------------

echo
echo "Optional (only needed for building docs or checking code formatting, not"
echo "for building the app itself):"

if command -v doxygen >/dev/null 2>&1; then
    DOXY_VER=$(doxygen --version 2>&1 | awk '{print $1}')
    if [ "$DOXY_VER" = "1.15.0" ]; then
        printf "  [OK]      doxygen        (%s, matches CI's pin)\n" "$DOXY_VER"
    else
        printf "  [WARN]    doxygen        (%s -- CI is pinned to 1.15.0, docs output may differ)\n" "$DOXY_VER"
    fi
else
    echo "  [--]      doxygen        not found -- only needed with -DXTRKCAD_USE_DOXYGEN=ON"
fi

if command -v astyle >/dev/null 2>&1; then
    ASTYLE_VER=$(astyle --version 2>&1 | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' | head -1)
    if [ "$ASTYLE_VER" = "3.6.13" ]; then
        printf "  [OK]      astyle         (%s, matches CI's pin)\n" "$ASTYLE_VER"
    else
        printf "  [WARN]    astyle         (%s -- CI is pinned to 3.6.13 built from source; an\n" "$ASTYLE_VER"
        echo "            unpinned astyle has caused real formatting mismatches before, see SF #638)"
    fi
else
    echo "  [--]      astyle         not found -- only needed for code formatting, see devguide.md"
fi

# --- summary ----------------------------------------------------------

echo
if [ "$MISSING" -eq 0 ]; then
    echo "All required tools found. See devguide.md's Building and testing section for"
    echo "the exact commands for $OS."
else
    echo "$MISSING required tool(s) missing -- install the ones listed above, then re-run this script."
    exit 1
fi
