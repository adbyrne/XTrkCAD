XTrkCAD Installation — macOS
=============================

This archive contains an XTrkCAD build that requires GTK3 libraries
provided by Homebrew. If you do not have Homebrew installed, see
https://brew.sh first.

PREREQUISITES
-------------
Install the required libraries (one-time setup):

    brew install gtk+3 libzip librsvg

INSTALLATION
------------
Extract the archive to a location of your choice, for example:

    tar -xzf xtrkcad-*-macos-arm64.tar.gz -C ~/Applications/XTrkCAD

Then run:

    ~/Applications/XTrkCAD/bin/xtrkcad

GATEKEEPER WARNING
------------------
Because this build is not signed with an Apple Developer certificate,
macOS will show a security warning the first time you run it.
To bypass: right-click (or Control-click) xtrkcad and choose "Open",
then click "Open" in the dialog.

SUPPORT
-------
https://xtrkcad.org
https://github.com/adbyrne/XTrkCAD
