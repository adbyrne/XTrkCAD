#!/bin/sh
# Check that a saved layout has the same text segments as its source, in the
# same order. A save that writes the wrong text doesn't crash, and it can still
# reload and save the same way (Development Issues #73).
#
# usage: check-roundtrip-text.sh <source.xtc> <saved.xtc>

texts() {
	tr -d '\r' < "$1" | sed -n 's/^[[:space:]]*Z .*\("[^"]*"\).*$/\1/p'
}

expected=$(texts "$1")
actual=$(texts "$2")
if [ -z "$expected" ] || [ "$expected" != "$actual" ]; then
	echo "FAIL: text segments differ"
	echo "--- $1"
	echo "$expected"
	echo "--- $2"
	echo "$actual"
	exit 1
fi
echo "OK: text segments match"
