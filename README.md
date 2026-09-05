# XTrkCAD (GTK3V2MAIN)

[![CI](../../actions/workflows/ci-gtk3.yml/badge.svg?branch=GTK3V2MAIN)](../../actions/workflows/ci-gtk3.yml)
[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

XTrkCAD is an open-source CAD tool for designing model railroad track layouts.
It supports a wide range of scales and manufacturers, with a rich library of
turnouts, sectional track, and flex track.

This branch is the GTK+ 3 successor to the [`main`](../../tree/main)
(GTK+ 2) branch — porting the UI layer to GTK3 while tracking the same upstream project.

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

## Goals of this Fork

- **GitHub Actions CI** — automated build and test on every push, across Linux/macOS/Windows,
  ARM64, with sanitizer and Valgrind coverage.
- **Modern toolchain support** — build fixes for GCC 15, `rsvg-convert`
  (replaces Inkscape for bitmap generation), CMocka shared library, and
  forward compatibility with CMake 4.x.
- **GTK3 migration** — porting the `wlib` UI abstraction layer off GTK+ 2.0 (deprecated,
  end-of-life) onto GTK+ 3.
- **New features** — improvements and fixes worth contributing back upstream,
  developed here first.

---

## Building

Requires CMake ≥ 3.20, GCC, GTK+ 3.0, and `rsvg-convert`.

```sh
# Fedora / RHEL
sudo dnf install gtk3-devel libcmocka-devel zlib-devel libzip-devel \
                 mxml-devel astyle librsvg2-tools

# Ubuntu 22.04+
sudo apt-get install libgtk-3-dev libcmocka-dev zlib1g-dev libzip-dev \
                     libmxml-dev librsvg2-bin gcc make cmake
```

Out-of-source build (mandatory):

```sh
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DXTRKCAD_TESTING=ON
cmake --build build --parallel $(nproc)
ctest --test-dir build --output-on-failure
```

The debug build sets `-DCMAKE_BUILD_TYPE=Debug`. The binary is `xtrkcad-<version>`
(version-qualified so successive dev builds can install side by side — see
`ProgramVersion.cmake`).

---

## License

XTrkCAD is licensed under the [GNU General Public License v2.0](app/COPYING).
Modifications in this fork are under the same license.
