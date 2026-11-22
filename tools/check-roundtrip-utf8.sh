#!/bin/sh
# Check that a saved layout keeps its non-ASCII text byte for byte: every line
# of the source with a non-ASCII byte has to come back unchanged and in the
# same order. The source is UTF-8, so the saved file is too: its other lines
# are ASCII. On Windows text is converted to the system code page when it's
# read and back to UTF-8 when it's written (UTFCONVERT), which loses every
# character the code page doesn't have (Development Issues #83).
#
# usage: check-roundtrip-utf8.sh <source.xtc> <saved.xtc>

nonascii() {
	tr -d '\r' < "$1" | LC_ALL=C grep '[^ -~	]'
}

expected=$(nonascii "$1")
actual=$(nonascii "$2")
if [ -z "$expected" ] || [ "$expected" != "$actual" ]; then
	echo "FAIL: non-ASCII lines differ"
	echo "--- $1"
	echo "$expected"
	echo "--- $2"
	echo "$actual"
	exit 1
fi
echo "OK: non-ASCII lines match"
