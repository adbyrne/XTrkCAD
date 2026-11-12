#!/bin/sh
# Check the reports RegressionParamCheck.reports saved
# (app/bin/unittest/testfiles/regress-paramcheck-reports.xtr) against
# app/bin/unittest/testfiles/reports-expected/. Line 4 of a report is the date,
# so it's left out of the comparison.
#
# Usage: tools/check-reports-test.sh <expected dir> <directory holding the reports>

EXPECTED=$1
DIR=$2
RC=0
for exp in "$EXPECTED"/report-*.txt; do
	name=$(basename "$exp")
	if [ ! -f "$DIR/$name" ]; then
		echo "FAIL: $name wasn't saved"
		RC=1
	# Compared without carriage returns (Windows writes CRLF) and without diff,
	# which the Windows runners' shell doesn't have; the diff is shown where
	# there is one.
	elif [ "$(sed 4d "$DIR/$name" | tr -d '\r')" != "$(tr -d '\r' < "$exp")" ]; then
		echo "FAIL: $name differs from the expected report"
		if command -v diff > /dev/null 2>&1; then
			sed 4d "$DIR/$name" | tr -d '\r' | diff -u "$exp" -
		fi
		RC=1
	fi
done
[ $RC -eq 0 ] && echo "reports OK"
exit $RC
