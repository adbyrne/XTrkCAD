oThis is a work-in-progress to build a flatpak package

BLDDIR is your build directory,
SRCDIR is your source directory


Resolved issues so far:

- FreeImage and Inkscape not available as a flatpak
  FreeImage is mainly used on Win32, but also in pngtoxpm
  We disable building pngtoxpm in $SRCDIR/CMakeLists.txt

- gtk2 is included as a shared module
  See org.xtrkcad.xtrkcad.yml

- MiniXML is included as a module
  /app is used instead of /usr: FindMiniXML.cmake

- InkScape/pngtoxpm is used to convert .svg to .xpm3
  Not easy to include into flatpak, so we copy the .xpm3 from the regular build
  See make-soure-archive which creates a tar ball from xtrkcad source tree 
  appended with BLDDIR/app/bin/bitmaps

-libzip is included as a module

# Unresolved issues

- org.xtrkcad.xtrkcad.yml needs cleanup actions

- Added cmake variable FLATPAC
  CMake complains about this
	"CMake Warning:
	   Manually-specified variables were not used by the project:
	     FLATPAK"
  Controls CMakeLists.txt in SRCDIR, app/bin/bitmaps, app/tools

- app builds bnd runs but buttons are empty
$ flatpak-builder  --run workD org.xtrkcad.xtrkcad.yml xtrkcad
  see below


# DIRECTIONS:


# SEtUP: do once

# install flatpak
$ sudo apt install flatpak

# set up build environment
$ cd $BLDDIR/distribution
$ mkdir flatpak
$ cd flatpak

#install gnome sdk, runtime
# choose user, 47
$ flatpak install org.gnome.Platform
$ flatpak install org.gnome.Sdk

# initialize git (needed for building gtk2
$ git init
$ git submodule add https://github.com/flathub/shared-modules.git

# build init 
$ flatpak build-init workD org.xtrkcad.xtrkcad org.gnome.Sdk/x86_64/47 org.gnome.Platform/x86_64/47

# create symlink from source to build for org.xtrkcad.xtrkcad.yml
# builder looks for shared-modules in org.xtrkcad.xtrkcad.yml's directory instead of pwd
$ ln -sf $SRCDIR/distribution/flatpak/org.xtrkcad.yml


# BUILD

# build source tarball - you need to do this every time you change the source
$ cd SRCDIR/distribution/flatpak
$ ./make-source-archive $BLDDIR
$ edit $SRCDIR/org.xtrkcad.xtrkcad.yml: replace checksum with updated value
# return to build dir
$ cd $BLDDIR/distribution/flatpak
# build
$ flatpak-builder --force-clean workD org.xtrkcad.xtrkcad.yml


# RUN

# make a symlink so we can find xtrkcad/libdir
# should add /app to search path
$ flatpak-builder --socket=fallback-x11 --run workD org.xtrkcad.xtrkcad.yml ln -s /apt/share ../share
# ta-da
$ flatpak-builder --socket=fallback-x11 --run workD org.xtrkcad.xtrkcad.yml xtrkcad
# xtrkcad runs ok (minimal testing) but command buttons are empty
GdkPixbuf-WARNING **: 09:45:31.759: Error loading XPM image loader: Image type “xpm” is not supported
# and lots of errors
# this was a recent change to GdkPixbuf :(
https://gitlab.gnome.org/GNOME/gdk-pixbuf/-/commit/e052a112075a19fb75f1f2ff3de4c82923de13f2


# MISC:

# Bonus points - run bash (or any other command) in flatpak sandbox
$ flatpak-builder --socket=fallback-x11 --run workD org.xtrkcad.xtrkcad.yml bash


I don't know why you might need this but ...
# add repo
flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo

