#!/usr/bin/env bash

ME=$(basename $0)
DEBUG=no

#####################################################################
#   U S A G E
#####################################################################
usage() {
    cat <<EOF
$ME path-to-xtrkcad-rpm

This script takes an existing xtrkcad rpm file and creates a flatpak file
for portable use across linux systems which have flatpak installed.

Pre-requisities:
    flatpak
    flatpak-builder
    xdg-desktop-portal-gtk (log out or reboot after install)

Known issues:
  - still uses gtk2
  - won't work on older rpms since there is no longer support for xpm files in pixbuf
EOF
    exit 1
}
if [ $# -lt 1 ]; then
    usage
fi

#####################################################################
#   S E T   V A R I A B L E S
#####################################################################
WORKING_RPM=xtrkcad.rpm
FP_BUILD_DIR=workDir
FP_STATE_DIR=$HOME/.flatpak-builder-xtrkcad
FP_REPO=dummy_repo
BETA_SUFFIX=""
LOCAL_SUBDIR=""
SOURCE_RPM=$(basename $1)
if [[ $SOURCE_RPM = *[bB]eta[0-9]* ]]; then
    BETA_SUFFIX="-beta"
    LOCAL_SUBDIR="local/"
fi
XTRKCAD_VERSION=$(echo $SOURCE_RPM | cut -d'-' -f3)
if [ $DEBUG != "no" ]; then
    XTRKCAD_VERSION=${XTRKCAD_VERSION}-debug
fi
XTRKCADSHARE=share/xtrkcad${BETA_SUFFIX}
XTRKCADLIB=/usr/${LOCAL_SUBDIR}${XTRKCADSHARE}
XTRKCADBIN=/usr/${LOCAL_SUBDIR}bin/xtrkcad${BETA_SUFFIX}
FP_XTRKCAD_ORG=org.xtrkcad.xtrkcad${BETA_SUFFIX}
FP_MANIFEST=${FP_XTRKCAD_ORG}.yaml
FP_DESKTOP=${FP_XTRKCAD_ORG}.desktop
FP_META=${FP_XTRKCAD_ORG}.metainfo.xml

#####################################################################
#   C L E A N U P
#####################################################################
cleanup() {
    trap - 0 1 2 3 15 21 22
    if [ "$DEBUG" = "no" ]; then
        rm -f $WORKING_RPM
        rm -rf $FP_BUILD_DIR
        # removing the flatpak state dir forces fresh build
        #rm -rf $FP_STATE_DIR
        rm -rf $FP_REPO
        # manifest dynamically built, but if you want to look comment out
        rm -f $FP_MANIFEST
        rm -f $FP_DESKTOP
        rm -f $FP_META
    fi
}
trap cleanup 0 1 2 3 15 21 22

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
  - name: libgtk-x11
    buildsystem: simple
    sources:
      - type: file
        url: "https://www.rpmfind.net/linux/mageia/distrib/9/x86_64/media/core/updates/lib64gtk+-x11-2.0_0-2.24.33-5.1.mga9.x86_64.rpm"
        dest-filename: libgtk-x11.rpm
        sha256: 6a8541e8b4239850c53748f9c9275cafcd24d334aba15debb316dd94b0e02e1b
    build-commands:
      - bsdtar -xf libgtk-x11.rpm
      - mkdir -p /app/lib
      - cp -p ./usr/lib64/lib* /app/lib/
      - rm -f libgtk-x11.rpm
  - name: extractRPM
    buildsystem: simple
    sources:
      - type: file
        path: ./$WORKING_RPM
        dest-filename: $WORKING_RPM
    build-commands:
      - bsdtar -xf $WORKING_RPM
      - rm -f $WORKING_RPM
      - cp -pr ./* /app/
      - mkdir -p /app/share/icons/hicolor/64x64/apps/
      - cp .${XTRKCADLIB}/pixmaps/xtrkcad.png /app/share/icons/hicolor/64x64/apps/${FP_XTRKCAD_ORG}.png
  - name: xtrkcad
    buildsystem: simple
    sources:
      - type: script
        dest-filename: run-xtrkcad-nolog.sh
        commands:
          - mkdir -p ~/.local/${XTRKCADSHARE}
          - cp -pr --update /app${XTRKCADLIB}/* ~/.local/${XTRKCADSHARE}
          - "#export XTRKCADLIB=/app${XTRKCADLIB}"
          - "#export XTRKCADBETALIB=/app${XTRKCADLIB}"
          - export XTRKCADLIB=~/.local/${XTRKCADSHARE}
          - export XTRKCADBETALIB=~/.local/${XTRKCADSHARE}
          - BOOKMARK="file://\$XTRKCADLIB xtrkcad${BETA_SUFFIX}"
          - FILE="\$HOME/.gtk-bookmarks"
          - touch \$FILE
          - if ! grep -Fx "\$BOOKMARK" "\$FILE" >/dev/null; then
          -  echo "\$BOOKMARK" >> "\$FILE"
          - fi
          - "exec /app${XTRKCADBIN} \"\$@\""
      - type: inline
        dest-filename: run-xtrkcad.sh
        contents: |
          #!/bin/sh
          mkdir -p ~/.local/${XTRKCADSHARE}
          cp -pr --update /app${XTRKCADLIB}/* ~/.local/${XTRKCADSHARE}
          #export XTRKCADLIB=/app${XTRKCADLIB}
          #export XTRKCADBETALIB=/app${XTRKCADLIB}
          export XTRKCADLIB=~/.local/${XTRKCADSHARE}
          export XTRKCADBETALIB=~/.local/${XTRKCADSHARE}
          BOOKMARK="file://\$XTRKCADLIB xtrkcad${BETA_SUFFIX}"
          FILE="\$HOME/.gtk-bookmarks"
          touch \$FILE
          if ! grep -Fx "\$BOOKMARK" "\$FILE" >/dev/null; then
            echo "\$BOOKMARK" >> "\$FILE"
          fi
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
          exec /app${XTRKCADBIN} -v -l \$LOGFILE \$LOG_ALLMODULES \"\$@\"
      - type: file
        path: ./$FP_DESKTOP
        dest-filename: $FP_DESKTOP
      - type: file
        path: ./$FP_META
        dest-filename: $FP_META
    build-commands:
      - install -Dm755 run-xtrkcad-nolog.sh /app/bin/run-xtrkcad-nolog.sh
      - install -Dm755 run-xtrkcad.sh /app/bin/run-xtrkcad.sh
      - install -Dm644 $FP_DESKTOP /app/share/applications/${FP_DESKTOP}
      - install -Dm644 $FP_META /app/share/metainfo/${FP_META}
finish-args:
  - --socket=x11
  - --socket=wayland
  - --share=ipc
  - --device=all
  - --filesystem=~/.xtrkcad${BETA_SUFFIX}:create
  - --filesystem=~/.themes
  - --filesystem=~/.icons
  - --filesystem=~/.config:create
  - --filesystem=~/.gtk-bookmarks
  - --filesystem=~/.local/share
  - --filesystem=~/.local/${XTRKCADSHARE}
  - --filesystem=~/Documents:create
  - --filesystem=~/Downloads:create
  - --filesystem=~/xtrkcad:create
  - --filesystem=~/XtrackCAD:create
  - --filesystem=xdg-data/applications
  - --filesystem=xdg-data/icons
  - --talk-name=org.gnome.portal.OpenURI
EOF
}

#####################################################################
#   B U I L D   F L A T P A K
#####################################################################
build_flatpak() {
    flatpak remote-add --if-not-exists --user flathub https://flathub.org/repo/flathub.flatpakrepo
    flatpak-builder --state-dir=$FP_STATE_DIR --force-clean --user --install --repo=$FP_REPO --install-deps-from=flathub $FP_BUILD_DIR $FP_MANIFEST
    flatpak build-bundle ${FP_REPO} xtrkcad-${XTRKCAD_VERSION}.flatpak $FP_XTRKCAD_ORG
}

#####################################################################
#   M A I N
#####################################################################
cp -p $1 $WORKING_RPM || exit 1
build_manifest
build_flatpak
