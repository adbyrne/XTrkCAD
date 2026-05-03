# XTrkCAD

[![CI](https://github.com/adbyrne/XTrkCAD/actions/workflows/ci.yml/badge.svg)](https://github.com/adbyrne/XTrkCAD/actions/workflows/ci.yml)
[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

XTrkCAD is an open-source CAD tool for designing model railroad track layouts.
It supports a wide range of scales and manufacturers, with a rich library of
turnouts, sectional track, and flex track.

**Upstream project:** [XTrkCAD on SourceForge](https://sourceforge.net/projects/xtrkcad-fork/) —
the authoritative Mercurial repository and issue tracker live there.
This fork tracks that upstream and layers on toolchain modernization, CI/CD,
and MCP support.

---

## Goals of this Fork

- **GitHub Actions CI** — automated build and test on every push, using the
  same Ubuntu 22.04 / GTK 2.0 stack as the upstream release.
- **MCP server** — a [Model Context Protocol](https://modelcontextprotocol.io/)
  server (`mcp/`) so Claude and other AI assistants can read, create, and
  validate XTrkCAD track plans programmatically.
- **Modern toolchain support** — build fixes for GCC 15, `rsvg-convert`
  (replaces Inkscape for bitmap generation), CMocka shared library, and
  forward compatibility with CMake 4.x.
- **New features** — improvements and fixes worth contributing back upstream,
  developed here first.

---

## Building

Requires CMake ≥ 3.20, GCC, GTK+ 2.0, and `rsvg-convert`.

```sh
# Fedora / RHEL
sudo dnf install gtk2-devel libcmocka-devel zlib-devel libzip-devel \
                 mxml-devel astyle librsvg2-tools

# Ubuntu 22.04
sudo apt-get install libgtk2.0-dev libcmocka-dev zlib1g-dev libzip-dev \
                     libmxml-dev librsvg2-bin gcc make cmake
```

Out-of-source build (mandatory):

```sh
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DXTRKCAD_TESTING=ON
cmake --build build --parallel $(nproc)
ctest --test-dir build --output-on-failure
```

The debug build sets `-DCMAKE_BUILD_TYPE=Debug`. The binary is `xtrkcad`
(or `xtrkcad-beta` for development builds with a version modifier).

---

## MCP Server

The `mcp/` directory contains an MCP server that exposes XTrkCAD layout files
to Claude and other AI assistants. Phase 1 provides read-only tools; later
phases will add layout creation and DXF/SVG export.

### Tools

| Tool | Description |
|---|---|
| `list_track_plans` | Find all `.xtc` / `.xtce` files in a directory |
| `get_layout_summary` | Title, scale, room size, track type counts |
| `get_track_objects` | All tracks with positions and endpoint connections |
| `find_unconnected_endpoints` | Identify open (unconnected) track endpoints |

### Quick start

```sh
# Run the server directly (requires uv)
XTRKCAD_PLANS_DIR=/path/to/your/layouts \
  uv run --project mcp xtrkcad-mcp
```

### Claude Code configuration

Add a `.mcp.json` file in your project working directory:

```json
{
  "mcpServers": {
    "xtrkcad": {
      "command": "uv",
      "args": ["run", "--project", "/path/to/xtrkcad-git/mcp", "xtrkcad-mcp"],
      "env": {
        "XTRKCAD_PLANS_DIR": "/path/to/your/layouts"
      }
    }
  }
}
```

---

## Upstream Sync

This repo was converted from the upstream Hg repository at revision r6422.
The tag `upstream/hg-r6422` marks that point in history.

To pull new upstream commits:

```sh
# In the local Hg clone
hg pull -R /path/to/xtrkcad-hg

# Re-run fast-export, then rebase/merge into main
```

---

## License

XTrkCAD is licensed under the [GNU General Public License v2.0](app/COPYING).
Modifications in this fork are under the same license.
