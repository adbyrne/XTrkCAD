#!/bin/sh
# Install-test one XTrackCAD Linux package (.deb or .rpm) on a clean system.
#
# Usage: tools/package-install-test.sh <package-file>
#
# Run as root in a fresh container of the distribution the package is for,
# e.g. locally:
#
#   podman run --rm -v "$PWD:/src:ro,z" ubuntu:26.04 \
#       sh /src/tools/package-install-test.sh /src/xtrkcad-setup-...deb
#
# Fedora needs --privileged: its gdk-pixbuf runs image loaders in a bwrap
# sandbox (glycin), which can't start in an unprivileged container, and GTK
# then aborts on its first icon.
#
# release.yml's install-test jobs run it the same way. It installs the
# package with the distribution's own package manager (so missing or
# misnamed dependencies fail here, as they would for a user), then checks:
#   - the binary is on PATH, all its libraries resolve, -V prints the
#     version from the package file name
#   - the data directory the app computes (wGetAppLibDir(): ../share/<bin>
#     next to the binary) holds params, demos, examples and the help HTML
#   - the installed binary plays demos from the installed data under Xvfb
#   - the system-wide menu entry, icons and MIME type are installed, valid
#     and registered by the distribution's triggers (Development Issues #39)
#   - removal deletes the binary, the data directory and the desktop files
#
# Hard failures exit 1. Warnings are printed as GitHub Actions ::warning::
# annotations and don't change the exit status.

set -u

PKG=${1:?usage: $0 <package-file>}
FAIL=0

fail() { echo "::error::$*"; FAIL=1; }
warn() { echo "::warning::$*"; }
step() { echo; echo "=== $*"; }

# Package managers drop privileges to read the file (apt's _apt sandbox),
# so work on a world-readable copy rather than a bind-mounted original.
WORK=$(mktemp -d)
cp "$PKG" "$WORK/" || exit 1
PKG="$WORK/$(basename "$PKG")"
chmod 755 "$WORK"
chmod 644 "$PKG"

# The test's own tools. Installed before the package so that what the
# package pulls in is only what it declares itself.
step "Install test tools"
case "$PKG" in
*.deb)
	export DEBIAN_FRONTEND=noninteractive
	apt-get update -q || exit 1
	apt-get install -y -q xvfb xauth desktop-file-utils \
		shared-mime-info || exit 1
	;;
*.rpm)
	dnf -y -q install xorg-x11-server-Xvfb xauth desktop-file-utils \
		shared-mime-info util-linux || exit 1
	;;
*)
	echo "unknown package type: $PKG"
	exit 1
	;;
esac
# xvfb-run on Fedora ships in xorg-x11-server-Xvfb; check it's really there.
command -v xvfb-run >/dev/null || { echo "xvfb-run not found"; exit 1; }

step "Install $(basename "$PKG")"
case "$PKG" in
*.deb)
	NAME=$(dpkg-deb -f "$PKG" Package)
	apt-get install -y -q "$PKG" || { fail "apt-get install failed"; exit 1; }
	;;
*.rpm)
	NAME=$(rpm -qp --qf '%{NAME}' "$PKG")
	dnf -y install "$PKG" || { fail "dnf install failed"; exit 1; }
	;;
esac
echo "installed package: $NAME"

step "Binary"
# The package name is the binary name (XTRKCAD_BIN, lower-cased for Debian;
# it's all lower case today).
BIN=$(command -v "$NAME") || { fail "$NAME is not on PATH"; exit 1; }
echo "$BIN"
MISSING=$(ldd "$BIN" | grep 'not found')
[ -z "$MISSING" ] || fail "unresolved libraries: $MISSING"

# xtrkcad-setup-<version>-1.<arch>[-<suffix>].<ext>
WANT=$(basename "$PKG" | sed -n 's/^xtrkcad-setup-\(.*\)-[0-9]*\.[^.-]*\(-[a-z0-9]*\)\{0,1\}\.[a-z]*$/\1/p')
# Without a display: -V is handled before GTK starts (Development Issues #40).
GOT=$(env -u DISPLAY -u WAYLAND_DISPLAY "$BIN" -V 2>/dev/null | sed -n 's/^Version: //p')
echo "-V: $GOT (package file says $WANT)"
[ -n "$GOT" ] || fail "$BIN -V printed no version"
[ -z "$WANT" ] || [ "$GOT" = "$WANT" ] ||
	fail "-V reports $GOT, package file name says $WANT"

step "Data directory"
SHARE=$(dirname "$(dirname "$(readlink -f "$BIN")")")/share/$NAME
echo "$SHARE"
for d in params demos examples html; do
	n=$(find "$SHARE/$d" -type f 2>/dev/null | wc -l)
	echo "$d: $n files"
	[ "$n" -gt 0 ] || fail "$SHARE/$d is missing or empty"
done
for f in xtrkcad.xtq icons16.gresource icons24.gresource icons32.gresource; do
	[ -f "$SHARE/$f" ] || fail "$SHARE/$f is missing"
done
# Help opens file://<share>/html/<topic>.html (browserhelp.c).
[ -f "$SHARE/html/index.html" ] || fail "$SHARE/html/index.html is missing"

step "Play demos from the installed data"
# A throwaway HOME, so the run starts from a first-run state like a new
# user's. dmstrtrk carries REGRESSION checks; dmintro exercises a full
# demo with dialogs.
HOME=$(mktemp -d)
export HOME
OUT=$(timeout 600 xvfb-run -a "$BIN" -Tdmintro,dmstrtrk 2>&1)
RC=$?
echo "$OUT" | tail -20
[ $RC -eq 0 ] || fail "demo run exited $RC"
echo "$OUT" | grep -q '^Regression Tests Pass' ||
	fail "demo run did not report 'Regression Tests Pass'"

step "Desktop integration"
# The test tools above include desktop-file-utils and shared-mime-info, so
# their triggers ran when the package was installed, as on a desktop system.
DESKTOP=/usr/share/applications/$NAME.desktop
MIMEXML=/usr/share/mime/packages/$NAME.xml
ICONS="/usr/share/icons/hicolor/scalable/apps/$NAME.svg /usr/share/icons/hicolor/64x64/apps/$NAME.png"
for f in $DESKTOP $MIMEXML $ICONS; do
	[ -f "$f" ] || fail "$f is missing"
done
if [ -f "$DESKTOP" ]; then
	desktop-file-validate "$DESKTOP" || fail "$DESKTOP does not validate"
	grep -q "^Exec=$NAME " "$DESKTOP" || fail "$DESKTOP does not run $NAME"
	grep -q "^Icon=$NAME\$" "$DESKTOP" || fail "$DESKTOP does not use icon $NAME"
fi
# update-mime-database writes one file per known type.
[ -f /usr/share/mime/application/x-xtrkcad.xml ] ||
	fail "application/x-xtrkcad is not in the MIME database"
# update-desktop-database maps each type to the entries that open it.
grep -q "^application/x-xtrkcad=.*$NAME.desktop" \
	/usr/share/applications/mimeinfo.cache 2>/dev/null ||
	fail "$NAME.desktop is not registered for application/x-xtrkcad"
# Nothing per user: the old postinst wrote into the installing user's HOME.
[ ! -e /root/.local/share/applications/$NAME.desktop ] ||
	fail "per-user desktop entry created in /root"

step "Remove"
case "$PKG" in
*.deb) apt-get purge -y -q "$NAME" || fail "apt-get purge failed" ;;
*.rpm) dnf -y remove "$NAME" || fail "dnf remove failed" ;;
esac
[ ! -e "$BIN" ] || fail "$BIN is still there after removal"
[ ! -e "$SHARE" ] || {
	fail "$SHARE is still there after removal:"
	find "$SHARE" | head -20
}
for f in $DESKTOP $MIMEXML $ICONS; do
	[ ! -e "$f" ] || fail "$f is still there after removal"
done
[ ! -f /usr/share/mime/application/x-xtrkcad.xml ] ||
	fail "application/x-xtrkcad is still in the MIME database after removal"

echo
if [ $FAIL -ne 0 ]; then
	echo "FAILED"
	exit 1
fi
echo "PASSED"
