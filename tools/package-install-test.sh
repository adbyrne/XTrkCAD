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
#   - a system-wide .desktop entry exists (warning only, see Development
#     Issues #39)
#   - removal deletes the binary and the data directory
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
	apt-get install -y -q xvfb xauth desktop-file-utils || exit 1
	;;
*.rpm)
	dnf -y -q install xorg-x11-server-Xvfb xauth desktop-file-utils \
		util-linux || exit 1
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
# -V needs a display until Development Issues #40 is fixed.
GOT=$(xvfb-run -a "$BIN" -V 2>/dev/null | sed -n 's/^Version: //p')
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
DESKTOP=$(find /usr/share/applications -name "*$NAME*.desktop" 2>/dev/null)
if [ -z "$DESKTOP" ]; then
	warn "no system-wide .desktop entry in /usr/share/applications, so XTrackCAD is missing from the menu (Development Issues #39)"
else
	echo "$DESKTOP"
	for f in $DESKTOP; do
		desktop-file-validate "$f" || fail "$f does not validate"
	done
fi

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

echo
if [ $FAIL -ne 0 ]; then
	echo "FAILED"
	exit 1
fi
echo "PASSED"
