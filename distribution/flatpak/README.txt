If not done already, install flatpak and flatpak-builder (if not part of flatpak).

BLDDIR is your build directory,
    not part of SRCDIR
SRCDIR is your source directory
    SRCDIR must be a mercurial tree, cloned and updated to the version desired

cd $BLDDIR
cmake [-G Ninja] -DCMAKE_BUILD_TYPE=Debug $SRCDIR

    If you choose Ninja because it's really fast, note that you won't see build progress
    due to its buffer handling while running several parallel build steps.

# not necessary if you want just a flatpak
make            # or "ninja -j1"
make package    # or "ninja package"

# build and create flatpak for testing (NOT for release, use github for flatpak release)
make flatpak    # or "ninja flatpak" with minimal progress output
    This will build the .flatpak file named by version and changeset ready to be
    locally tested. A directory holding state is updated as builds progress, so that
    the next time, a build won't take as long.

    If flatpak-builder errors out showing

      "the state dir is not on the same filesystem as the target dir"

    then repeat the cmake but add

      -DFLATPAK_STATE_DIR=/path/to/.flatpak-builder-xtrkcad/

    where the path is on the same filesystem as BLDDIR. Similar to docker,
    not everything has to be rebuilt, so the state dir caches builds that
    haven't changed.

    The manifest used is similar to the official build manifest except for
    how the source is retrieved.

# RUN (pick one depending on what was built)
flatpak run io.sourceforge.xtrkcad_fork.xtrkcad
flatpak run io.sourceforge.xtrkcad_fork.xtrkcad-beta
flatpak run io.sourceforge.xtrkcad_fork.xtrkcad-gtk

# Misc
- paths in xtrkcad.rc need to change to flatpak xtrkcad lib location
  otherwise the Parameter Files dialog will be blank
- xtrkcad.rc can be renamed and let flatpak re-create
  folks would need to re-add params and set options
- paths in xtrkcad.rc ought to be relative and code which references them
  should then prepend with the XTRKCAD*LIB environment; that way folks
  can keep their xtrkcad.rc file as is, but there will be pains in the transition
- ./distribution/flatpak/buildFlatpak.sh will create build directory and build flatpak
