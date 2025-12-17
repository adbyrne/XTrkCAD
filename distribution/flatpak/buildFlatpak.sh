#!/usr/bin/env bash

ME=$(basename $0)

# WARNING: changing this to yes will leave files/directories present in the source
#          tree which are not currently part of the .hgignore file
DEBUG=no

#####################################################################
#   U S A G E
#####################################################################
usage() {
  cat <<EOF
Usage: $ME

This script creates a flatpak from this source. The flatpak will end
up in the build directory of root of this source tree.

The current directory should either be the root directory of the source
or the source distribution/flatpak directory.

Pre-requisities:
    flatpak
    flatpak-builder
    xdg-desktop-portal-gtk (log out or reboot after install)

Known issues:
  - still uses gtk2
EOF
  exit 1
}
if [ "$1" = "-h" ]; then
  usage
fi

#####################################################################
#   G E T   T O   R O O T   O F   S O U R C E
#####################################################################
PROGRAM_VERSION=ProgramVersion.cmake
if [ ! -f $PROGRAM_VERSION ]; then
  cd ../..
fi
if [ ! -f $PROGRAM_VERSION ]; then
  echo
  echo "ERROR: not in the correct directory"
  echo
  usage
  exit 1
fi
#####################################################################
#   C R E A T E   C H E C K   B U I L D   E N V
#####################################################################
HERE=`pwd`
mkdir -p build
cd build || exit 1
rm -f *flatpak
command -v flatpak >/dev/null 2>&1 || { echo "Missing flatpak tool"; exit 1; }
command -v flatpak-builder >/dev/null 2>&1 || { echo "Missing flatpak-builder tool"; exit 1; }
#####################################################################
#   B U I L D
#####################################################################
NCPU=$(lscpu -rp=CPU | tail -1)
NCPU=$((NCPU + 1))
MAXLOAD=$((NCPU * 2))
if command -v ninja >/dev/null 2>&1 && [ ! -s Makefile ] ; then
    cmake -G Ninja -B . -S $HERE
    ninja -j${NCPU} -l${MAXLOAD} flatpak
else
    cmake -B . -S $HERE
    make -j${NCPU} -l${MAXLOAD} flatpak
fi
