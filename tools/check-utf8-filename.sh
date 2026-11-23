#!/bin/sh
# Check the log of a round trip of a layout with a non-ASCII file name
# (Development Issues #92): the layout has to be opened under its own name,
# saved and loaded again without a failure, and the name must never reach GTK
# as anything but UTF-8 (window title, File > Recent), which GTK would report
# as a warning. On Windows file names were in the system code page, so a name
# with characters outside it couldn't be opened at all, and one with
# characters in it was shown wrongly.
#
# usage: check-utf8-filename.sh <log> <file name>

log=$1
name=$2
export LC_ALL=C

if [ ! -f "$log" ]; then
	echo "FAIL: no log $log"
	exit 1
fi
tr -d '\r' < "$log"
echo "---"
fail=0
if ! tr -d '\r' < "$log" | grep 'current layout "' | grep -qF "$name\""; then
	echo "FAIL: the layout wasn't opened as $name"
	fail=1
fi
if ! tr -d '\r' < "$log" | grep -q ': [1-9][0-9]* tracks$'; then
	echo "FAIL: no tracks loaded"
	fail=1
fi
if tr -d '\r' < "$log" | grep -E 'FAIL|WARNING|CRITICAL|Invalid UTF-8'; then
	echo "FAIL: see the lines above"
	fail=1
fi
[ $fail -eq 0 ] && echo "OK: $name"
exit $fail
