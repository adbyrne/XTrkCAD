\page flatpak Flatpak for Devs
\tableofcontents

# Overview {#flatpak-for-devs}

This is a page to document changes and build requirements to generate a flatpak of XTrkCAD.

For the curious, a flatpak is a way that an app built on linux can basically run on any version of linux that can run a flatpak. They are self-contained, for the most part, meaning no additional packages need to be updated on the system but for the flatpak package. They run in a sandbox, meaning they don't have full access to everything on your host computer (XTrackCAD has full access to the home directory for updating track plans). Binaries are either built statically with libraries needed or the libraries are available in the runtime chosen. Yes, this means flatpak's are larger, however they are broken up similar to docker where only the changed parts are required to be downloaded on updates.

[XTrackCAD](https://flathub.org/en/apps/io.sourceforge.xtrkcad_fork.xtrkcad) is an offical app on [flathub](https://flathub.org/), _the repository of flatpak apps_, and so there is both an `x86_64` and `aarch64` (arm) version. Once a release is provided on sourceforge, the flathub repo will need updating of its manifest to generate a new version available for download. More on that later.

Time to get into the weeds, but if having second thoughts, head back to the \ref index "Developer Documentation".

# Requirements {#flatpak-requirements}

First, ensure you can \ref linux-build-test "build XTrackCAD" normally. This ensures you have other pre-requisites needed for builds.

To build a flatpak, the **flatpak-builder** package is needed, but to run a flatpak, you only need the **flatpak** package (which is a dependency of **flatpak-builder**).

Developers and users will need to add the **flathub** repo in order to find flatpak apps and their runtimes. Developers will also need an sdk, also available from flathub.

From time to time, the sdk and platform will change from the versions below.

Replace `--user` with `--system` if installing for every user, but you'll need to be root (not recommended).

```sh
# get flatpak-builder and flatpak (change accordingly for your distribution)
sudo apt-get install -y flatpak-builder

# Log out/in so that XDG_DATA_DIRS set with flatpak directories in the menu system

# use flatpak to point to flathub repo for install of sdk and runtime
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo

# install sdk
flatpak install -y --user flathub org.freedesktop.Sdk//25.08

# install runtime
flatpak install -y --user flathub org.freedesktop.Platform//25.08

# install flatseal (permission manager)
# (not really a requirement, but a nice-to-have)
flatpak install -y --user flathub com.github.tchx84.Flatseal

```

See [flathub's setup guide](https://flathub.org/en/setup) for your linux distribution.

# App ID {#flatpak-appid}

The flathub official XTrackCAD app id is `io.sourceforge.xtrkcad_fork.xtrkcad` (_App Id's are in reverse DNS notation_). This was chosen since https was needed for verification and also redirects weren't working well from xtrkcad.org. See the message [xtrkcad.org website and flatpak](https://xtrackcad.groups.io/g/development/topic/116473371). Also, it's 1 character shorter than using _net.sourceforge.xtrkcad_fork.xtrkcad_ since app id's have a max length of 255 characters. The _sourceforge.io_ domain is redirected to _sourceforge.net_ anyway. But it is what it is. Can it be changed? Sure, just not easily as name change request must be made to flathub.

For verification purposes, a token was provided and file created in the xtrkcad_fork project on sourceforge. I can't remember how I got the file created or whether or not admin privilege was required. You can view it [here](https://xtrkcad-fork.sourceforge.io/.well-known/org.flathub.VerifiedApps.txt). Verification shows the blue checkmark. See [XTrackCAD on Flathub](https://flathub.org/en/apps/io.sourceforge.xtrkcad_fork.xtrkcad).

# Build XTrackCAD Flatpak Locally {#flatpak-build}

Some leniency in local builds allows the app id to change and include the version so that different builds of a flatpak can be run side-by-side for comparison if desired. This won't be allowed in offical flatpak releases on flathub; the app id stays the same, and only the metainfo changes per version.

When run for the first time, `~/.xtrkcad-version_hash-fp` directories are created to contain the initialization file and options chosen. It will also contain the files created for regression tests if run. The _version_ portion uses underscores instead of periods.

## buildFlatpak.sh

This script saves you from manually entering all the commands to build the flatpak. It creates a _build_ directory, optionally modifies _About_ to note this is a test build, runs _cmake_ with **`-DXTRKCAD_BUILD_FLATPAK=1`** to configure the build system for a flatpak build, runs _ninja_ (preferable) or _make_ with the _flatpak_ target generating a <b>xtrkcad-<i>version.arch</i>.flatpak</b> file in the _build_ directory, and lastly _hg revert_ some files.

```sh
# from root of cloned repo, pick from command lines below:
#
# show help
# ./distribution/flatpak/buildFlatpak.sh -h
#
# build flatpak using workdir with version (~/.xtrkcad-ver-fp)
# ./distribution/flatpak/buildFlatpak.sh
#
# build flatpak with debug, modified About, using workdir with version
# ./distribution/flatpak/buildFlatpak.sh -d
#
# build flatpak with debug, modified About, using workdir with hash (~/.xtrkcad-ver_hash-fp)
# ./distribution/flatpak/buildFlatpak.sh -d --hash

./distribution/flatpak/buildFlatpak.sh -d
```

We'll get into the details later, but you may notice that this script never runs **[flatpak-builder](https://docs.flatpak.org/en/latest/flatpak-builder-command-reference.html)**, rather, it is run from the _flatpak_ target defined by the cmake system. **flatpak-builder** takes as an argument, a manifest file, which defines the libraries and special executables needed to build the binary. Then **[flatpak build-bundle](https://docs.flatpak.org/en/latest/flatpak-command-reference.html#flatpak-build-bundle)** takes the artifacts built and creates a flatpak package.

Note that if ninja is used, there will be a long delay in output during the build process. Be patient.

Also note that if the flatpak tools are not available, the flatpak target will not be available in the cmake system.

## Why is source modified?

Several reasons:

- to minimize patching required for official flatpak builds
- to pass flathub submission requirements
- one less thing to remember (what files to change) when nearing release

The **buildFlatpak.sh** script performs reverts so that the version changes are not stored.

However, when a new version branch is tagged or created, version changes should be saved and pushed so that source archival can be used by official flatpak builds. Run:

```sh
cmake -DXTRKCAD_BUILD_FLATPAK=1 -G "Ninja" -B build
# or if you don't have ninja installed
# cmake -DXTRKCAD_BUILD_FLATPAK=1 -B build
```

and notice `hg diff app/lib/xtrkcad.*` shows the new versions and release date.

\important Ensure version changes that are checked in do **NOT** include the hash. Periods separating the version numbers must be converted to underscores wherever the app id is used (**io.sourceforge.xtrkcad_fork.xtrkcad\***).

If the only difference is the inclusion of the hash, then no need to edit, just revert:

```sh
hg revert -C app/lib/xtrkcad.metainfo.xml
hg revert -C app/lib/xtrkcad.flatpak.desktop
```

Push the changes without the commit hash (version should only contain for example 5.4.0, and references using the app id with version changes should only contain for example xtrkcad-5_4_0, no 12-digit hexadecimal hash).

Re-tag the tree if necessary.

# Build XTrackCAD Flatpak on Flathub {#flatpak-build-flathub}

Building an app for release on flathub requires that [guidelines](https://docs.flathub.org/docs/for-app-authors/submission) be followed. This took several weeks. Once the submission is approved, a repo for the app is created and the requestor is given access. The XTrackCAD app repo is [on flathub's github](https://github.com/flathub) as repository [io.sourceforge.xtrkcad_fork.xtrkcad](https://github.com/flathub/io.sourceforge.xtrkcad_fork.xtrkcad). Now that the app has been approved, it is now in [maintenance mode](https://docs.flathub.org/docs/for-app-authors/maintenance). Other branches can be created (although certain ones are reserved).

## Create a source tar ball for test flatpak builds

\note do not copy/paste these entire steps as some steps require some eyes to check results.

```sh
# get latest
hg pull

# check on the branch desired
hg branch

# change to branch desired
hg update GTK3V2MAIN

# ensure no changes pending
hg status
hg out

# create $HOME/.tarignore to exclude files from source tar ball (here's mine)
cat <<EOF > $HOME/.tarignore
.hg
.git
.github
*gz
.DS_Store
.project
.settings.json
build
build32
.vs
_CPack_Packages
*.mo
.vscode
.cache
*.flatpak
tmp
*.out
*.patch
ken*
doit
*tar
*tar.gz
*tgz
*zip
*bak
.hgignore
.hgtags
tags
EOF

# make source archive in build directory
./distribution/flatpak/make-source-archive

# check that source contains only files required to build XTrackCAD
tar tzvf build/source*tar.gz

# update $HOME/.tarignore if necessary and re-run previous two steps

# make note of the sha256sum of the build/source*tar.gz file created

# upload the source tar ball to sourceforge into Development/Flatpak/TestSources under the File tab

```

## Update the flathub repo

```sh
# flathub xtrkcad repo (your fork of course would be different)
git clone git@github.com:flathub/io.sourceforge.xtrkcad_fork.xtrkcad.git

# master branch updates reserved for releases

# edit the yaml file and update at a minimum the source file url location and sha256

# try to build, create patches as necessary

```

### Try a build

Build in a sandbox. There won't be flatpak, flatpak-builder, nor hg.

```sh
# create build in repo
flatpak run --command=flathub-build org.flatpak.Builder --verbose --install --force-clean io.sourceforge.xtrkcad_fork.xtrkcad.yaml
META_DIR=builddir/files/share/metainfo
METAINFO=io.sourceforge.xtrkcad_fork.xtrkcad.metainfo.xml
ARCH=$(uname -m)
VERSION=$(grep "release version=" $META_DIR/$METAINFO| head -1 | sed 's/^.*version="//;s/".*//;s/ //' | tr -d '\n')
flatpak build-bundle repo xtrkcad${VERSION}.${ARCH}.flatpak

# now and then get issues with fuse
sudo umount rofiles-fuse

```

### Try lint

If you see an error regarding home filesystem access, you can ignore since an exception has already been approved by flathub (different repo) since this app does need home directory access so that a user can save their layout anywhere they want.

It looks like this:

```json
    "errors": [
        "finish-args-home-filesystem-access"
    ],
```

Other error entries need to be addressed.

Anyway, here are some linter commands to perform:

```sh
flatpak run --command=flatpak-builder-lint org.flatpak.Builder manifest io.sourceforge.xtrkcad_fork.xtrkcad.yaml
flatpak run --command=flatpak-builder-lint org.flatpak.Builder repo repo
flatpak run --command=flatpak-builder-lint org.flatpak.Builder appstream builddir/files/share/metainfo/io.sourceforge.xtrkcad_fork.xtrkcad.metainfo.xml
flatpak run --command=flatpak-builder-lint org.flatpak.Builder builddir builddir
```

### Patching

To patch, we need a directory containing original source, and a directory which at first also contains original source but will be modified. From these two directories, patch files can be created.

Inspect the source tar ball to discover its directory structure. For example:

```sh
# a released tar ball
$ tar tzf ~/Downloads/xtrkcad-source-5.3.1GA.tar.gz |head
xtrkcad-source-5.3.1GA/ProgramVersion.cmake
xtrkcad-source-5.3.1GA/CMakeLists.txt
xtrkcad-source-5.3.1GA/app/
xtrkcad-source-5.3.1GA/app/lib/
xtrkcad-source-5.3.1GA/app/lib/xtrkcad.xml
xtrkcad-source-5.3.1GA/app/lib/xdg-open
xtrkcad-source-5.3.1GA/app/lib/logo.bmp
xtrkcad-source-5.3.1GA/app/lib/CMakeLists.txt
xtrkcad-source-5.3.1GA/app/lib/xtrkcad.desktop
xtrkcad-source-5.3.1GA/app/lib/xtrkcad.upd

# vs a tar ball created by "./distribution/flatpak/make-source-archive build" (no leading directory)
$ tar tzf build/source.tar.gz  | head
./
./app/
./app/bin/
./app/bin/bitmaps/
./app/bin/bitmaps/greendot.png
./app/bin/bitmaps/svg/
./app/bin/bitmaps/svg/benchwork.svg
./app/bin/bitmaps/svg/convert-from.svg
./app/bin/bitmaps/svg/doc-new.svg
./app/bin/bitmaps/svg/map.svg
```

Depending on the structure, the extraction options are different.

```sh
# official source (has leading directory)
tar xzf xtrkcad-source-5.3.1GA.tar.gz --strip-components=1 --one-top-level=source.orig
tar xzf xtrkcad-source-5.3.1GA.tar.gz --strip-components=1 --one-top-level=source.new

# made with make-source-archive (no leading directory)
tar xzf source.tar.gz --one-top-level=source.orig
tar xzf source.tar.gz --one-top-level=source.new
```

Now you have 2 directories, _source.orig_ and _source.new_. Make changes to files only in _source.new_. Of course, feel free to name these directories anything you like.

To create the actual patch file, run for example:

```sh
diff -uNrb --no-dereference source.orig source.new > patches/my-new.patch
```

Edit the yaml file to include the patch.

Try a build.

# Locations of interest {#flatpak-locations}

```sh
# list of user flatpaks
export TAB=$(echo -e -n "\t")
flatpak list --user | grep -E "stable|master|beta" | column -d -t -s "$TAB"

ls -1AF ~/.local/share/flatpak/app

# menu system may get messed up during development
# list xtrkcad apps, remove ones not installed, log out/in
ls -1AF ~/.local/share/applications/ | grep xtrkcad
ls -1AF ~/.local/share/flatpak/exports/share/applications/ | grep xtrkcad
ls -1AF /var/lib/flatpak/exports/share/applications/ | grep xtrkcad

# clean up hidden, excluded, and included XTrackCAD or xtrkcad entries (your menu system may vary)
# look for refs to xtrkcad in <Include>, <Exclude> in <Graphics> and <.hidden> and remove
edit ~/.config/menus/applications-kmenuedit.menu
# rebuild menu
kbuildsycoca6 --noincremental
# restart menu
systemctl --user restart plasma-plasmashell.service

# menu system finds apps (.desktop files) based off XDG_DATA_DIRS env var
ls -1AF /etc/profile.d/flatpak*.sh

# if --delete-data not passed with "flatpak uninstall" leftover data may be present
ls -1AF ~/.var/app/ | grep xtrkcad | column -t
# only do this if flatpak version of xtrkcad not running
rm -rf ~/.var/app/io.sourceforge.xtrkcad_fork.xtrkcad/

# dev version of working directories to be removed if no longer needed
ls -d1AF ~/.xtrkcad* | grep -v "\.xtrkcad-fp"

# overrides, if any (remove unneeded ones)
ls -1AF ~/.local/share/flatpak/overrides/ | grep xtrkcad

```
