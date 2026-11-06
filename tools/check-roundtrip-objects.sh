#!/bin/sh
# Check a saved layout against its source: every line of the object types that
# only this fixture contains has to come back unchanged (Development Issues #75).
#
# Usage: check-roundtrip-objects.sh <source.xtc> <saved.xtc>
source_file=$1
saved_file=$2
pattern='^(BEZIER|SENSOR|CONTROL|SWITCHMOTOR|BLOCK|SIGNAL) |^[[:space:]]*(ASPECT|TRK) '

expected=$(tr -d '\r' < "$source_file" | grep -E "$pattern")
if [ -z "$expected" ]; then
    echo "FAIL: no object lines in $source_file"
    exit 1
fi
actual=$(tr -d '\r' < "$saved_file" | grep -E "$pattern")
if [ "$expected" != "$actual" ]; then
    echo "FAIL: objects differ between $source_file and $saved_file"
    echo "--- expected"
    echo "$expected"
    echo "--- actual"
    echo "$actual"
    exit 1
fi
echo "OK: $(echo "$expected" | wc -l) object lines match"
