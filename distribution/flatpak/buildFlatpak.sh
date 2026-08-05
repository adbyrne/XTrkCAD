#!/usr/bin/env bash

ME=$(basename $0)

# WARNING: changing this to yes will leave files/directories present in the source
#          tree which are not currently part of the .hgignore file
DEBUG=no

# uncomment to start fresh (presumes -DFLATPAK_STATE_DIR not passed)
#rm -rf $HOME/.flatpak-builder-xtrkcad

#####################################################################
#   U S A G E
#####################################################################
usage() {
	cat <<EOF
Usage: $ME [-d]

This script creates a flatpak from this source. The flatpak will end
up in the build directory of root of this source tree.

The current directory should either be the root directory of the source
or the source distribution/flatpak directory.

Pre-requisities:
    flatpak
    flatpak-builder
    xdg-desktop-portal-gtk (log out or reboot after install)

Options:
  -d    do a debug build
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

HERE=$(pwd)
BUILD_TYPE=""
cleanup() {
	trap - 0 1 2 3 15 21 22
	if [ -n "$BUILD_TYPE" ]; then
		hg revert -C $HERE/app/bin/smalldlg.c
	fi
}

trap cleanup 0 1 2 3 15 21 22

#####################################################################
#   C R E A T E   C H E C K   B U I L D   E N V
#####################################################################
mkdir -p build
cd build || exit 1
rm -f *flatpak
command -v flatpak >/dev/null 2>&1 || {
	echo "Missing flatpak tool"
	exit 1
}
command -v flatpak-builder >/dev/null 2>&1 || {
	echo "Missing flatpak-builder tool"
	exit 1
}
#####################################################################
#   D E B U G   B U I L D
#####################################################################
if [ "$1" = "-d" ]; then
	SF_VERSION=$(hg branch)
	CHANGESETREV=$(hg log -r . -T '{rev}')
	CHANGESET=$(hg log -r . -T '{shortest(node,12)}')
	sed -i "s/^#define DESCRIPTION .*/#define DESCRIPTION N_(\"Debug XtrackCad build from $SF_VERSION (changeset $CHANGESETREV:$CHANGESET).\")/" ../app/bin/smalldlg.c
	BUILD_TYPE="-DCMAKE_BUILD_TYPE=Debug"
fi

#####################################################################
#   B U I L D
#####################################################################
NCPU=$(lscpu -rp=CPU | tail -1)
NCPU=$((NCPU + 1))
MAXLOAD=$((NCPU * 2))
if command -v ninja >/dev/null 2>&1 && [ ! -s Makefile ]; then
	cmake $DEBUG_FLATPAK $BUILD_TYPE -DXTRKCAD_BUILD_FLATPAK=1 -G Ninja -B . -S $HERE
	ninja -j${NCPU} -l${MAXLOAD} flatpak || exit 1
else
	cmake $DEBUG_FLATPAK $BUILD_TYPE -DXTRKCAD_BUILD_FLATPAK=1 -B . -S $HERE
	make -j${NCPU} -l${MAXLOAD} flatpak || exit 1
fi
