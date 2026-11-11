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
	elif ! sed 4d "$DIR/$name" | diff -u "$exp" - ; then
		echo "FAIL: $name differs from the expected report"
		RC=1
	fi
done
[ $RC -eq 0 ] && echo "reports OK"
exit $RC
