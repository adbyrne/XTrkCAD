# XTrkCAD

[![CI](../../actions/workflows/ci-gtk3.yml/badge.svg?branch=main)](../../actions/workflows/ci-gtk3.yml)
[![Packages and install tests](../../actions/workflows/release.yml/badge.svg?branch=main)](../../actions/workflows/release.yml?query=branch%3Amain)
[![CodeQL](../../actions/workflows/codeql.yml/badge.svg?branch=main)](../../actions/workflows/codeql.yml)
[![Latest development build](https://img.shields.io/badge/download-latest--main-orange.svg)](../../releases/tag/latest-main)
[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

XTrkCAD is an open-source CAD tool for designing model railroad track layouts.
It supports a wide range of scales and manufacturers, with a rich library of
turnouts, sectional track, and flex track.

`main` is the GTK+ 3 line (5.4.0), mirroring SourceForge Hg `default` since upstream made the
GTK3 branch the default on 2026-10-20. The last GTK+ 2 state of `main` is tagged
[`gtk2-final`](../../tree/gtk2-final).

**Upstream project:** [XTrkCAD on SourceForge](https://sourceforge.net/projects/xtrkcad-fork/) —
the authoritative Mercurial repository and issue tracker live there.
This fork tracks that upstream and layers on toolchain modernization, CI/CD,
the GTK3 migration, and quality work (static analysis, sanitizers, coverage).

**Documentation & reports:** published via GitHub Pages on every push to this branch (Doxygen
developer docs, the user guide, and non-gating analysis reports — code coverage, cppcheck,
clang-tidy, codespell, compiler warnings) at `<owner>.github.io/<repo>`, `<owner>/<repo>` being
whichever fork's Actions did the deploying. See
[`docs/doxygen/advanced.md`](docs/doxygen/advanced.md) for how this is wired up.

---

## Downloads

Every push to `main` builds packages and publishes them as the
[`latest-main`](../../releases/tag/latest-main) prerelease, after the build and unit tests
passed. Each package is also installed and tested on a clean system of its kind (it starts,
plays demos from the installed data, and Help opens the right page); the badge above shows
whether the last run passed.

| System | File |
|--------|------|
| Ubuntu 24.04 / 26.04 | `*-ubuntu2404.deb` / `*-ubuntu2604.deb` (x86_64, aarch64) |
| Debian 12 / 13, Raspberry Pi OS 64-bit | `*-debian12.deb` / `*-debian13.deb` (x86_64, aarch64) |
| Fedora 44 / 45 | `*-fedora44.rpm` / `*-fedora45.rpm` (x86_64, aarch64) |
| Other Linux (x86_64) | `*.x86_64.sh` self-extracting archive, or build from `xtrkcad-source-*` |
| Windows 10/11 x64 | `*-windows-x64.zip`: unpack anywhere and run `bin\xtrkcad-<version>.exe`. `*-windows-x64-ucrt.zip` is the same program built against Windows' Universal C Runtime (MSYS2 UCRT64); if you're not sure which to take, use the one without `-ucrt` |
| macOS (Apple Silicon / Intel) | `*-macos-arm64.tar.gz` / `*-macos-x86_64.tar.gz`; needs Homebrew's GTK3, see `README-INSTALL.txt` inside |

These are development builds: they install as `xtrkcad-<version>` with their own settings, next
to an installed release. A tagged release installs as plain `xtrkcad`, replaces the previous
release and keeps its settings; its Windows installer (NSIS) is built for tags only.

---

## Goals of this Fork

- **GitHub Actions CI** — automated build and test on every push, across Linux/macOS/Windows,
  ARM64, with sanitizer and Valgrind coverage, and install tests of every package on clean
  systems.
- **Modern toolchain support** — build fixes for GCC 15, `rsvg-convert`
  (replaces Inkscape for bitmap generation), CMocka shared library, and
  forward compatibility with CMake 4.x.
- **GTK3 migration** — porting the `wlib` UI abstraction layer off GTK+ 2.0 (deprecated,
  end-of-life) onto GTK+ 3. Done: upstream made the GTK3 line its default branch on
  2026-10-20.
- **New features** — improvements and fixes worth contributing back upstream,
  developed here first.
- **MSVC on Windows** — an experimental Visual Studio build against gvsbuild's GTK3, next to
  the MSYS2/MinGW one (`ci-windows-msvc.yml`).

The fork also carries an MCP server for layout analysis and reports (`mcp/`, Python, see
[`mcp/README.md`](mcp/README.md)). It is git-only and not part of the upstream program.

---

## Building

Requires CMake ≥ 3.20, GCC or Clang, GTK+ 3.0, `pkg-config` and `rsvg-convert`. Doxygen builds
the User Guide; without it the build succeeds but has no help pages.

```sh
# Fedora / RHEL
sudo dnf install gtk3-devel libcmocka-devel zlib-devel libzip-devel \
                 mxml-devel librsvg2-tools doxygen pkgconf ninja-build

# Ubuntu 24.04+ / Debian 12+
sudo apt-get install libgtk-3-dev libcmocka-dev zlib1g-dev libzip-dev \
                     libmxml-dev librsvg2-bin doxygen pkg-config gcc cmake ninja-build
```

macOS (Homebrew) and Windows (MSYS2, or the experimental MSVC build) are covered in the
developer guide's Building and Testing page (`docs/doxygen/building.md`).

Out-of-source build (mandatory):

```sh
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DXTRKCAD_TESTING=ON
cmake --build build --parallel $(nproc)
ctest --test-dir build --output-on-failure
```

The debug build sets `-DCMAKE_BUILD_TYPE=Debug`. With CMake 4.0 or newer,
`cmake --workflow --preset linux-debug` does all three steps from `CMakePresets.json`; see
the developer guide's Building and Testing page for the other presets.

A build from an untagged commit is named `xtrkcad-<version>` and keeps its own settings, so it
installs next to a release; a build from a release tag is plain `xtrkcad`. Pass
`-DXTRKCAD_VERSIONED_INSTALL=OFF` or `ON` to choose (see `ProgramVersion.cmake` and the
developer guide's "Defaults for Release").

---

## License

XTrkCAD is licensed under the [GNU General Public License v2.0](app/COPYING).
Modifications in this fork are under the same license.
