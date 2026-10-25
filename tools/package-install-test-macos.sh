#!/bin/sh
# Install-test the XTrackCAD macOS archive (.tar.gz) on a clean system.
#
# Usage: tools/package-install-test-macos.sh <package.tar.gz>
#
# The macOS counterpart of tools/package-install-test.sh; release.yml's
# install-test-macos jobs run it on fresh macos runners. The archive needs
# Homebrew's GTK3, so the script follows README-INSTALL.txt: brew install
# the prerequisites, unpack the archive (into a directory whose name has a
# space in it), and start the binary by its full path from the home
# directory, as a user would. It checks:
#   - exactly one bin/xtrkcad-<version> (or bin/xtrkcad), and every library it links
#     exists once the README's prerequisites are installed
#   - README-INSTALL.txt names the binary that's in the archive
#   - -V prints the version in the package file name
#   - share/<binary name> holds params, demos, examples and the help HTML
#   - the binary plays demos from the unpacked data
#
# Hard failures exit 1. Warnings are printed as GitHub Actions ::warning::
# annotations and don't change the exit status.

set -u

PKG=${1:?usage: $0 <package.tar.gz>}
FAIL=0

fail() { echo "::error::$*"; FAIL=1; }
warn() { echo "::warning::$*"; }
step() { echo; echo "=== $*"; }

# macOS has no timeout(1); a modal dialog would otherwise hang the job.
run_timeout() {
	secs=$1
	shift
	perl -e 'alarm shift; exec @ARGV or die "exec: $!"' "$secs" "$@"
}

PKG=$(cd "$(dirname "$PKG")" && pwd)/$(basename "$PKG")
WORK=$(mktemp -d)
ROOT="$WORK/XTrkCAD install test"
mkdir -p "$ROOT"

step "Install the README's prerequisites"
# README-INSTALL.txt: brew install gtk+3 libzip librsvg
HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_CLEANUP=1 \
	brew install gtk+3 libzip librsvg || { fail "brew install failed"; exit 1; }

step "Unpack $(basename "$PKG")"
tar -xzf "$PKG" -C "$ROOT" || { fail "tar -xzf failed"; exit 1; }
echo "$ROOT"

step "Binary"
# xtrkcad-<version>, or plain xtrkcad for an unversioned release build
# (XTRKCAD_VERSIONED_INSTALL).
set --
for f in "$ROOT"/bin/xtrkcad "$ROOT"/bin/xtrkcad-*; do
	[ -e "$f" ] && set -- "$@" "$f"
done
if [ $# -ne 1 ] || [ ! -x "$1" ]; then
	fail "expected one bin/xtrkcad or bin/xtrkcad-*, found: $*"
	exit 1
fi
BIN=$1
NAME=$(basename "$BIN")
echo "$BIN"
# Every linked library outside the OS must exist (Homebrew's, by absolute
# path). @rpath/@loader_path entries would have to be inside the archive.
# A function, because macOS's /bin/sh (bash 3.2) can't parse a case
# statement inside $(...).
missing_libs() {
	otool -L "$1" | tail -n +2 | awk '{print $1}' |
		while read -r lib; do
			case "$lib" in
			/usr/lib/* | /System/*) ;;
			/*) [ -e "$lib" ] || echo "$lib" ;;
			*) echo "$lib (not an absolute path)" ;;
			esac
		done
}
MISSING=$(missing_libs "$BIN")
[ -z "$MISSING" ] || fail "libraries not found: $MISSING"

README="$ROOT/README-INSTALL.txt"
if [ ! -f "$README" ]; then
	fail "README-INSTALL.txt is missing"
elif ! grep -q "bin/$NAME\$" "$README"; then
	fail "README-INSTALL.txt doesn't tell users to run bin/$NAME:"
	grep -n 'bin/' "$README"
fi

# Start from the home directory, not bin/: the program must find its data
# from its own location, not the current directory.
cd "$HOME" || exit 1

# xtrkcad-<version>-macos-<arch>.tar.gz
WANT=$(basename "$PKG" | sed -n 's/^xtrkcad-\(.*\)-macos-[a-z0-9_]*\.tar\.gz$/\1/p')
GOT=$(run_timeout 60 "$BIN" -V 2>/dev/null | sed -n 's/^Version: //p')
echo "-V: $GOT (package file says $WANT)"
[ -n "$GOT" ] || fail "$BIN -V printed no version"
# The binary's version carries a build hash the archive name may not have.
case "$GOT" in
"$WANT" | "$WANT".*) ;;
*) [ -z "$WANT" ] || fail "-V reports $GOT, package file name says $WANT" ;;
esac

step "Data directory"
SHARE="$ROOT/share/$NAME"
echo "$SHARE"
for d in params demos examples html; do
	n=$(find "$SHARE/$d" -type f 2>/dev/null | wc -l | tr -d ' ')
	echo "$d: $n files"
	[ "$n" -gt 0 ] || fail "$SHARE/$d is missing or empty"
done
for f in xtrkcad.xtq html/index.html; do
	[ -f "$SHARE/$f" ] || fail "$SHARE/$f is missing"
done

step "Play demos from the unpacked data"
# A throwaway HOME, so the run starts from a first-run state like a new
# user's. dmstrtrk carries REGRESSION checks; dmintro exercises a full
# demo with dialogs.
TESTHOME=$(mktemp -d)
OUT=$(HOME=$TESTHOME run_timeout 600 "$BIN" -Tdmintro,dmstrtrk 2>&1)
RC=$?
echo "$OUT" | tail -20
[ $RC -eq 0 ] || fail "demo run exited $RC"
echo "$OUT" | grep -q '^Regression Tests Pass' ||
	fail "demo run did not report 'Regression Tests Pass'"

# A crash (exit 128 + signal; 142 is our own SIGALRM timeout): show where
# from the crash report macOS writes, then rerun a few times to see how
# often it happens. The release binary has no debug info, but its function
# names are in the report.
show_crash_report() {
	# ReportCrash writes the report a moment after the process dies.
	sleep 10
	IPS=$(ls -t "$HOME"/Library/Logs/DiagnosticReports/"$NAME"*.ips 2>/dev/null | head -1)
	if [ -z "$IPS" ]; then
		echo "no crash report for $NAME"
		return
	fi
	echo "--- $IPS: crashing thread"
	# An .ips file is a one-line JSON header followed by a JSON body.
	python3 - "$IPS" <<'PY'
import json, sys
text = open(sys.argv[1]).read()
body = json.loads(text[text.index("\n") + 1:])
print("exception:", body.get("exception"))
images = body.get("usedImages", [])
thread = body["threads"][body["faultingThread"]]
for i, f in enumerate(thread.get("frames", [])[:40]):
    img = images[f["imageIndex"]].get("name", "?") if "imageIndex" in f else "?"
    print("%2d %-28s %s +%s" % (i, img, f.get("symbol", "?"),
                               f.get("symbolLocation", f.get("imageOffset"))))
PY
}
if [ $RC -gt 128 ] && [ $RC -ne 142 ]; then
	step "Crash details"
	show_crash_report
	CRASHES=1
	for i in 2 3 4 5; do
		TESTHOME=$(mktemp -d)
		HOME=$TESTHOME run_timeout 600 "$BIN" -Tdmintro,dmstrtrk >/dev/null 2>&1
		R=$?
		echo "demo run $i: exit $R"
		if [ $R -gt 128 ] && [ $R -ne 142 ]; then
			CRASHES=$((CRASHES + 1))
			show_crash_report
		fi
	done
	echo "crashed $CRASHES of 5 runs"
fi

rm -rf "$WORK" "$TESTHOME"
echo
if [ $FAIL -ne 0 ]; then
	echo "FAILED"
	exit 1
fi
echo "PASSED"
