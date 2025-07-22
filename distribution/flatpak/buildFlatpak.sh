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
Usage: $ME [ destination-directory ]

This script creates a flatpak from this source. The flatpak will end
up in the root directory of this source tree or in the optional directory
passed.

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
#   D E T E R M I N E   V E R S I O N
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
MOD_PROGRAM_VERSION=Modified${PROGRAM_VERSION}
cat <<EOF > $MOD_PROGRAM_VERSION
include($PROGRAM_VERSION)
if (XTRKCAD_SHOW_VERSION)
    # used by distribution/flatpak/buildFlatpak.sh
    message(\${XTRKCAD_VERSION})
endif()
EOF
XTRKCAD_VERSION=$(cmake -DXTRKCAD_SHOW_VERSION=1 -P $MOD_PROGRAM_VERSION 2>&1)
rm -f $MOD_PROGRAM_VERSION
if [ -z "$XTRKCAD_VERSION" ]; then
    echo "Unable to determine version; is the source correct?"
    exit 1
fi

#####################################################################
#   S E T   V A R I A B L E S
#####################################################################
WORKING_FILE=xtrkcad-source.tar.gz
FP_BUILD_DIR=workDir
FP_STATE_DIR=${FP_STATE_DIR:-$HOME/.flatpak-builder-xtrkcad}
FP_REPO=dummy_repo
FP_DEST_DIR=${1:-$PWD}
if [ ! -d "$FP_DEST_DIR" ]; then
    echo "\"$FP_DEST_DIR\" is not a directory"
    exit 1
fi
BETA_SUFFIX=""
LOCAL_SUBDIR=""
if [[ $XTRKCAD_VERSION = *[bB]eta[0-9]* ]]; then
    BETA_SUFFIX="-beta"
    #LOCAL_SUBDIR="local/"
fi
if [ $DEBUG != "no" ]; then
    XTRKCAD_VERSION=${XTRKCAD_VERSION}-debug
fi
XTRKCADPREFIX=/app
XTRKCADSHARE=share/xtrkcad${BETA_SUFFIX}
XTRKCADLIB=${XTRKCADPREFIX}/${LOCAL_SUBDIR}${XTRKCADSHARE}
XTRKCADBIN=${XTRKCADPREFIX}/${LOCAL_SUBDIR}bin/xtrkcad${BETA_SUFFIX}
FP_XTRKCAD_ORG=org.xtrkcad.xtrkcad${BETA_SUFFIX}
FP_MANIFEST=${FP_XTRKCAD_ORG}.yaml
FP_DESKTOP=${FP_XTRKCAD_ORG}.desktop
FP_META=${FP_XTRKCAD_ORG}.metainfo.xml
XDG_DATA=${XDG_DATA_HOME:-$HOME/.local/share}
INKSCAPE_IMAGE=Inkscape.AppImage
INKSCAPE_TAR="Inkscape.tar.gz"

#####################################################################
#   C L E A N U P
#####################################################################
cleanup() {
    trap - 0 1 2 3 15 21 22
    if [ "$DEBUG" = "no" ]; then
        rm -f $WORKING_FILE
        rm -rf $FP_BUILD_DIR
        find $FP_STATE_DIR -name '*build-debug' | xargs -r rm
        # removing the flatpak state dir forces fresh build
        #rm -rf $FP_STATE_DIR
        rm -rf $FP_REPO
        # manifest dynamically built, but if you want to look comment out
        rm -f $FP_MANIFEST
        rm -f $FP_DESKTOP
        rm -f $FP_META
        rm -f $INKSCAPE_IMAGE
        # removing existing inkscape tar file forces download of latest
        #rm -f $XDG_DATA/$INKSCAPE_TAR
        rm -f $MOD_PROGRAM_VERSION
    fi
}
trap cleanup 0 1 2 3 15 21 22

#####################################################################
#   S T A R T   F R E S H
#
#   Normally not run in order to take advantage of cached files
#####################################################################
start_fresh() {
    rm -f $WORKING_FILE
    rm -rf $FP_BUILD_DIR
    # removing the flatpak state dir forces fresh build
    rm -rf $FP_STATE_DIR
    rm -rf $FP_REPO
    # manifest dynamically built, but if you want to look comment out
    rm -f $FP_MANIFEST
    rm -f $FP_DESKTOP
    rm -f $FP_META
    rm -f $INKSCAPE_IMAGE
    # removing existing inkscape tar file forces download of latest
    rm -f $XDG_DATA/$INKSCAPE_TAR
    flatpak uninstall -y --user org.inkscape.Inkscape
}

#####################################################################
#   F L U S H   C A C H E
#
#   Prevent cache from growing and growing during development
#####################################################################
flush_cache() {
    # in meg
    MAX_CACHE_SIZE=1000

    if [ ! -d "$FP_STATE_DIR" ]; then
        return
    fi
    CACHE_SIZE=$(du -sm $FP_STATE_DIR | cut -f1)
    if [[ $CACHE_SIZE -ge $MAX_CACHE_SIZE ]]; then
        echo "Removing build cache .."
        echo "CACHE_SIZE=$CACHE_SIZE, MAX_CACHE_SIZE=$MAX_CACHE_SIZE"
        rm -rf $FP_STATE_DIR
        sleep 2
    fi
}

#####################################################################
#   M A K E   S O U R C E   T A R B A L L
#####################################################################
make_source_tar() {
    tar -czf ../$WORKING_FILE --exclude=.hg .
    mv ../$WORKING_FILE .
}

#####################################################################
#   G E T   A N D   E X T R A C T   I N K S C A P E
#####################################################################
get_extract_inkscape() {
    flatpak remote-add --if-not-exists --user flathub https://flathub.org/repo/flathub.flatpakrepo
    flatpak install -y --user flathub org.inkscape.Inkscape
    if [ ! -f $XDG_DATA/$INKSCAPE_TAR ]; then
        echo "creating tarball of inkscape ..."
        flatpak run --user --command=tar org.inkscape.Inkscape -czf $XDG_DATA/$INKSCAPE_TAR --exclude='*/__pycache__*' /app 2>/dev/null
    fi
}

#####################################################################
#   B U I L D   M A N I F E S T
#####################################################################
build_manifest() {
    cat <<EOF > $FP_DESKTOP
[Desktop Entry]
Name=XTrackCAD${BETA_SUFFIX} (flatpak)
Comment=Design model railroad layouts
Exec=run-xtrkcad.sh
Icon=${FP_XTRKCAD_ORG}
Terminal=false
Type=Application
Categories=Graphics
EOF
    DT="$(date '+%Y-%m-%d')"
    cat <<EOF > $FP_META
<?xml version="1.0" encoding="UTF-8"?>
<component type="desktop-application">
  <id>${FP_XTRKCAD_ORG}</id>
  <name>XTrackCAD${BETA_SUFFIX} (flatpak)</name>
  <summary>Design model railroad layouts</summary>
  <description>
    <p>XTrackCAD is a CAD (computer-aided design) program for designing model railroad layouts.</p>
  </description>
  <releases>
    <release version="${XTRKCAD_VERSION}" date="${DT}"/>
  </releases>
  <metadata_license>CC0-1.0</metadata_license>
  <project_license>GPL-3.0</project_license>
</component>
EOF
    cat <<EOF > $FP_MANIFEST
id: $FP_XTRKCAD_ORG
runtime: org.gnome.Platform
runtime-version: '48'
sdk: org.gnome.Sdk
command: /app/bin/run-xtrkcad.sh
modules:
  - name: libzip
    buildsystem: cmake
    config-opts:
      - -DBUILD_DOC=OFF
      - -DBUILD_EXAMPLES=OFF
      - -DBUILD_REGRESS=OFF
      - -DBUILD_TOOLS=OFF
      - -DBUILD_OSSFUZZ=OFF
      - -DCMAKE_INSTALL_PREFIX=/app/libzip
      - -DBUILD_SHARED_LIBS=OFF
      - -DENABLE_COMMONCRYPTO=OFF
      - -DENABLE_GNUTLS=OFF
      - -DENABLE_MBEDTLS=OFF
      - -DENABLE_OPENSSL=OFF
      - -DENABLE_WINDOWS_CRYPTO=OFF
      - -DENABLE_BZIP2=OFF
      - -DENABLE_LZMA=OFF
      - -DENABLE_ZSTD=OFF
    sources:
      - type: archive
        url: https://github.com/nih-at/libzip/releases/download/v1.8.0/libzip-1.8.0.tar.gz
        sha256: 30ee55868c0a698d3c600492f2bea4eb62c53849bcf696d21af5eb65f3f3839e
    post-install:
      - mkdir -p /app/include
      - mkdir -p /app/lib
      - ln -s /app/libzip/include/* /app/include
      - ln -s /app/libzip/lib/libzip.a /app/lib
  - name: mxml
    buildsystem: simple
    build-commands:
      - ./configure --exec_prefix=/app/mxml --prefix=/app/mxml
      - make
      - make install
      - mkdir -p /app/include
      - mkdir -p /app/lib
      - ln -s /app/mxml/include/* /app/include
      - ln -s /app/mxml/lib/* /app/lib
    sources:
      - type: archive
# v4 changes names from mxml to mxml4 whch we haven't migrated yet
        url: https://github.com/michaelrsweet/mxml/archive/refs/tags/v3.3.1.tar.gz
        sha256: 59eba16ce43765f2e2a6cf4873a58d317be801f1e929647d85da9f171e41e9ac
  - name: gtk2
    buildsystem: autotools
    build-options:
      env:
        CFLAGS: "-Wno-incompatible-pointer-types -Wno-implicit-int"
        CXXFLAGS: "-Wno-incompatible-pointer-types -Wno-implicit-int"
    sources:
      - type: archive
        url: "https://gitlab.gnome.org/GNOME/gtk/-/archive/2.24.33/gtk-2.24.33.tar.gz"
        sha256: dedfaf04952434c5e3e1ce4de373ac7474d12da2d99b0afc947ef1983df64601
    config-opts:
      - "--prefix=${XTRKCADPREFIX}"
      - "--disable-introspection"
      - "--enable-shared"
      - "--disable-static"
      - "--enable-gtk-doc-html=no"
#    make-args:
#      - "V=1"
#      - "-j 1"
  - name: inkscape
    buildsystem: simple
    sources:
      - type: file
        path: $XDG_DATA/$INKSCAPE_TAR
        dest-filename: $INKSCAPE_TAR
    build-options:
      env:
        PKG_CONFIG_PATH: "/app/lib/pkgconfig:/usr/lib/pkgconfig"
        LD_LIBRARY_PATH: "/app/lib:/usr/lib:/usr/lib/x86_64-linux-gnu"
    build-commands:
      - tar -xzf $INKSCAPE_TAR --skip-old-files
      - mkdir -p /app/inkscape
      - cp -pr --update app/* /app/inkscape 2>&1
#  - name: debug
#    buildsystem: simple
#    no-make-install: true
#    build-commands:
#      - echo "D E B U G   B E G I N"
#      - find /app -type f -o -type l
#      - echo "D E B U G   E N D"
  - name: xtrkcad
    buildsystem: simple
    sources:
      - type: inline
        dest-filename: run-xtrkcad.sh
        contents: |
          #!/bin/sh
          # flatpak sandboxing prevents browser from accessing the help so copy and adjust XTRKCADLIB
          # (perhaps someday another env variable can be used for help html file access and thus not
          #  have to copy all of XTRKCADLIB)
          #export XTRKCADLIB=${XTRKCADLIB}
          #export XTRKCADBETALIB=${XTRKCADLIB}
          mkdir -p ~/.local/${XTRKCADSHARE}
          rm -rf ~/.local/${XTRKCADSHARE}/*
          cp -pr ${XTRKCADLIB}/* ~/.local/${XTRKCADSHARE}
          export XTRKCADLIB=~/.local/${XTRKCADSHARE}
          export XTRKCADBETALIB=~/.local/${XTRKCADSHARE}
          # there is no access to host gtk3 modules
          unset GTK3_MODULES
          unset GTK_MODULES
          BOOKMARK="file://\$XTRKCADLIB xtrkcad-lib${BETA_SUFFIX}"
          FILE="\$HOME/.gtk-bookmarks"
          touch \$FILE
          if ! grep -Fx "\$BOOKMARK" "\$FILE" >/dev/null; then
            echo "\$BOOKMARK" >> "\$FILE"
          fi
          exec ${XTRKCADBIN} "\$@"
      - type: inline
        dest-filename: run-xtrkcad-debug.sh
        contents: |
          #!/bin/sh
          # flatpak sandboxing prevents browser from accessing the help so copy and adjust XTRKCADLIB
          # (perhaps someday another env variable can be used for help html file access and thus not
          #  have to copy all of XTRKCADLIB)
          #export XTRKCADLIB=${XTRKCADLIB}
          #export XTRKCADBETALIB=${XTRKCADLIB}
          mkdir -p ~/.local/${XTRKCADSHARE}
          rm -rf ~/.local/${XTRKCADSHARE}/*
          cp -pr ${XTRKCADLIB}/* ~/.local/${XTRKCADSHARE}
          export XTRKCADLIB=~/.local/${XTRKCADSHARE}
          export XTRKCADBETALIB=~/.local/${XTRKCADSHARE}
          # there is no access to host gtk3 modules
          unset GTK3_MODULES
          unset GTK_MODULES
          BOOKMARK="file://\$XTRKCADLIB xtrkcad-lib${BETA_SUFFIX}"
          FILE="\$HOME/.gtk-bookmarks"
          touch \$FILE
          if ! grep -Fx "\$BOOKMARK" "\$FILE" >/dev/null; then
            echo "\$BOOKMARK" >> "\$FILE"
          fi
          # set up for logging by day the app started
          DOW=\$(date '+%a')
          LOGFILE="\$HOME/.xtrkcad${BETA_SUFFIX}/xtrkcad_\${DOW}.log"
          LOG_ALLMODULES=" \\
              -d Bezier=1 \\
              -d block=1 \\
              -d carDlgList=1 \\
              -d carDlgState=1 \\
              -d carInvList=1 \\
              -d carList=1 \\
              -d command=1 \\
              -d control=1 \\
              -d Cornu=1 \\
              -d cornuturnoutdesigner=1 \\
              -d curve=1 \\
              -d curveSegs=1 \\
              -d dumpElev=1 \\
              -d ease=1 \\
              -d endPt=1 \\
              -d group=1 \\
              -d init=1 \\
              -d join=1 \\
              -d locale=1 \\
              -d malloc=0 \\
              -d mapsize=1 \\
              -d modify=1 \\
              -d mouse=0 \\
              -d pan=1 \\
              -d paraminput=1 \\
              -d paramlayout=1 \\
              -d params=1 \\
              -d paramupdate=1 \\
              -d playbackcursor=1 \\
              -d print=1 \\
              -d profile=1 \\
              -d readTracks=1 \\
              -d redraw=1 \\
              -d regression=1 \\
              -d scale=1 \\
              -d sensor=1 \\
              -d shortPath=1 \\
              -d signal=1 \\
              -d splitturnout=1 \\
              -d Structure=1 \\
              -d suppresscheckpaths=1 \\
              -d switchmotor=1 \\
              -d timedrawgrid=1 \\
              -d timedrawtracks=1 \\
              -d timemainredraw=1 \\
              -d timereadfile=1 \\
              -d track=1 \\
              -d trainMove=1 \\
              -d trainPlayback=1 \\
              -d traverseBezier=1 \\
              -d traverseBezierSegs=1 \\
              -d traverseCornu=1 \\
              -d traverseJoint=1 \\
              -d traverseTurnout=1 \\
              -d turnout=1 \\
              -d undo=1 \\
              -d zoom=1 \\
          "
          rm -f \$LOGFILE
          touch \$LOGFILE
          exec ${XTRKCADBIN} -v -l \$LOGFILE \$LOG_ALLMODULES "\$@"
      - type: file
        path: ./$FP_DESKTOP
        dest-filename: $FP_DESKTOP
      - type: file
        path: ./$FP_META
        dest-filename: $FP_META
      - type: file
        path: ./$WORKING_FILE
        dest-filename: $WORKING_FILE
    build-options:
      env:
        PKG_CONFIG_PATH: "/app/lib/pkgconfig:/app/libzip/lib/pkgconfig:/app/libzip/lib64/pkgconfig:/app/mxml/lib/pkgconfig:/app/inkscape/lib/pkgconfig:/usr/lib/pkgconfig"
        LD_LIBRARY_PATH: "/app/lib:/app/inkscape/lib:/app/libzip/lib:/app/libzip/lib64:/app/mxml/lib:/usr/lib:/usr/lib/x86_64-linux-gnu"
        PATH: "/app/bin:/app/inkscape/bin:/usr/bin"
    build-commands: 
      - |
        install -Dm755 run-xtrkcad-debug.sh /app/bin/run-xtrkcad-debug.sh
        install -Dm755 run-xtrkcad.sh /app/bin/run-xtrkcad.sh
        install -Dm644 $FP_DESKTOP /app/share/applications/${FP_DESKTOP}
        install -Dm644 $FP_META /app/share/metainfo/${FP_META}
        mkdir src
        mkdir build
        cd src
        tar xzf ../$WORKING_FILE
        cd ../build
        XTRKCAD_CMAKE_DEBUG="-DCMAKE_TARGET_PROPERTIES=1 -DCMAKE_FIND_DEBUG_MODE=1"
        XTRKCAD_CMAKE_DEBUG=""
        cmake -G Ninja \$XTRKCAD_CMAKE_DEBUG -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$XTRKCADPREFIX -DCMAKE_C_FLAGS="-I/app/include -Wpointer-sign" ../src
        mkdir -p /app/share/icons/hicolor/64x64/apps/
        if false; then
          # debug: flatpak ps
          #        flatpak enter <instance-id> bash
          echo -e "\\nSLEEPING for debug purposes"
          echo -e "\\nin another window, run\\n  flatpak ps\\n  flatpak enter <instance-id> bash\\n"
          echo "export PKG_CONFIG_PATH=\"/app/lib/pkgconfig:/app/libzip/lib/pkgconfig:/app/libzip/lib64/pkgconfig:/app/mxml/lib/pkgconfig:/app/inkscape/lib/pkgconfig:/usr/lib/pkgconfig\""
          echo "export LD_LIBRARY_PATH=\"/app/lib:/app/inkscape/lib:/app/libzip/lib:/app/libzip/lib64:/app/mxml/lib:/usr/lib:/usr/lib/x86_64-linux-gnu\""
          echo "export PATH=\"/app/bin:/app/inkscape/bin:/usr/bin:\\\$PATH\""
          echo "cd /run/build/xtrkcad/build  # to check build environment or build with ninja"
          echo "cd /run/build/xtrkcad/src    # the source"
          echo "# sorry no editor in the Sdk"
          sleep 3600
        else
          ninja -v
          ninja -v install
          cp $XTRKCADPREFIX/${XTRKCADSHARE}/pixmaps/xtrkcad.png /app/share/icons/hicolor/64x64/apps/${FP_XTRKCAD_ORG}.png
          #cp /run/build/xtrkcad/src/app/lib/xtrkcad.png /app/share/icons/hicolor/64x64/apps/${FP_XTRKCAD_ORG}.png
          cd ..
          # what is not needed in final flatpak
          rm -rf /app/inkscape
          rm -rf /app/include
          rm -rf /app/share/man
          rm -rf /app/lib64/cmake
          rm -rf /app/share/doc
          rm -rf /app/share/gtk-2.0/demo
          rm -f /app/lib/libmxml*
          rm -f /app/lib/pkgconfig
          rm -rf /app/mxml
          rm -rf /app/libzip
          rm -f /app/lib/libzip.a
          rm -f /app/bin/gtk*
          rm -rf /app/share/aclocal
        fi
finish-args:
  - --socket=x11
  - --socket=wayland
  - --socket=session-bus
  - --share=ipc
  - --device=all
  - --filesystem=~/.xtrkcad${BETA_SUFFIX}:create
  - --filesystem=~/.themes
  - --filesystem=~/.icons
  - --filesystem=~/.config:create
  - --filesystem=~/.gtk-bookmarks
  - --filesystem=~/.local/share:create
  - --filesystem=~/.local/${XTRKCADSHARE}:create
  - --filesystem=~/Documents:create
  - --filesystem=~/Downloads:create
  - --filesystem=~/xtrkcad:create
  - --filesystem=~/XtrackCAD:create
  - --filesystem=xdg-data/applications
  - --filesystem=xdg-data/icons
  - --talk-name=org.gnome.portal.*
  - --talk-name=org.gnome.*
  - --talk-name=org.gnome.portal.*
  - --system-talk-name=org.gnome.*
EOF
}

#####################################################################
#   B U I L D   F L A T P A K
#####################################################################
build_flatpak() {
    flatpak remote-add --if-not-exists --user flathub https://flathub.org/repo/flathub.flatpakrepo
    flatpak-builder --state-dir=$FP_STATE_DIR --force-clean --user --install --repo=$FP_REPO --install-deps-from=flathub $FP_BUILD_DIR $FP_MANIFEST
    FP_NAME=xtrkcad-${XTRKCAD_VERSION}.flatpak
    rm -f $FP_NAME
    flatpak build-bundle ${FP_REPO} $FP_NAME $FP_XTRKCAD_ORG
    if [ -f $FP_NAME ]; then
        mv $FP_NAME $FP_DEST_DIR/$FP_NAME 2>/dev/null
        echo "========================================================================"
        echo "$(ls $FP_DEST_DIR/$FP_NAME) created"
        echo
        flatpak info $FP_XTRKCAD_ORG
        echo "========================================================================"
    else
        echo "========================================================================"
        echo "problem creating $FP_NAME "
        echo "========================================================================"
    fi
}

#####################################################################
#   M A I N
#####################################################################
#start_fresh
echo "========================================================================"
echo "Making xtrkcad-${XTRKCAD_VERSION}.flatpak"
echo "========================================================================"
sleep 3
flush_cache
make_source_tar
get_extract_inkscape
build_manifest
build_flatpak
