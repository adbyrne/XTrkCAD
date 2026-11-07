#!/bin/sh
# Check what survives loading roundtrip-badline.xtc (Development Issues #75):
# the two good tracks, at y=5 and y=15, and not the one with a bad number, at
# y=10. Its loader messages are checked by RegressionNotice.badline itself.
#
# Usage: check-roundtrip-badline.sh <saved.xtc>
saved=$1
ends=$(tr -d '\r' < "$saved" | sed -n 's/^[[:space:]]*[ET]4* [0-9]* *\([0-9.]*\) \([0-9.]*\) .*/\2/p' | sort -n -u | tr '\n' ' ')
case "$ends" in
"5.000000 15.000000 ") echo "OK: the good tracks survived, the bad one didn't" ;;
*) echo "FAIL: track end Y values in the saved file: $ends (expected 5 and 15 only)"; exit 1 ;;
esac
