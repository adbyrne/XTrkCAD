#!/bin/sh
# Check the files XTrackCAD's -Texport: test writes (LayoutExportTest(), fileio.c).
#
# Usage: tools/check-export-test.sh <directory holding export.pdf/.svg/.dxf/.png>
#
# The layout is app/bin/unittest/testfiles/export-test.xtc: a straight track, a
# line and a text of size 288, which prints at 18 pt at the default print scale
# of 1:16. Needs pdfinfo and pdftotext (poppler-utils).

DIR=$1
RC=0
fail() {
	echo "FAIL: $1"
	RC=1
}

PDF="$DIR/export.pdf"
SVG="$DIR/export.svg"
DXF="$DIR/export.dxf"
PNG="$DIR/export.png"

# PDF: one page at least, holding the text at its point size. A font's box is
# taller than its point size by a factor that depends on the font (1.36 for
# DejaVu). Pango's default 96 dpi instead of 72 makes it 1.33 times that, which
# is what the upper limit is there to catch (Development Issues #62).
[ -s "$PDF" ] || fail "no PDF"
PAGES=$(pdfinfo "$PDF" | awk '/^Pages:/ {print $2}')
[ "${PAGES:-0}" -ge 1 ] || fail "PDF has no pages"
RATIO=$(pdftotext -bbox "$PDF" - | awk -F'"' '/>CHECKTEXT</ {printf "%.3f", ($8 - $4) / 18.0; exit}')
if [ -z "$RATIO" ]; then
	fail "CHECKTEXT isn't in the PDF"
else
	echo "PDF text height is $RATIO x its point size"
	awk -v r="$RATIO" 'BEGIN {exit !(r >= 1.0 && r <= 1.6)}' ||
		fail "printed text is the wrong size: $RATIO x its point size"
fi

# SVG: the text and something for each drawn object
if [ -f "$SVG" ]; then
	grep -q CHECKTEXT "$SVG" || fail "CHECKTEXT isn't in the SVG"
	[ "$(grep -c '<line\|<path\|<polyline' "$SVG")" -ge 3 ] || fail "SVG has too few lines"
else
	echo "no SVG, assuming a build without SVG support"
fi

# DXF: the text, line entities, and a proper end
[ -s "$DXF" ] || fail "no DXF"
grep -q CHECKTEXT "$DXF" || fail "CHECKTEXT isn't in the DXF"
[ "$(grep -c '^LINE' "$DXF")" -ge 3 ] || fail "DXF has too few lines"
[ "$(tail -n 2 "$DXF" | grep -c EOF)" -eq 1 ] || fail "DXF doesn't end with EOF"

# PNG: the signature, and a width and height in its header
[ -s "$PNG" ] || fail "no PNG"
[ "$(od -An -tx1 -N4 "$PNG" | tr -d ' ')" = 89504e47 ] || fail "export.png isn't a PNG"
WIDTH=$(od -An -tu1 -j16 -N4 "$PNG" | awk '{print $1*16777216 + $2*65536 + $3*256 + $4}')
HEIGHT=$(od -An -tu1 -j20 -N4 "$PNG" | awk '{print $1*16777216 + $2*65536 + $3*256 + $4}')
echo "PNG is $WIDTH x $HEIGHT"
[ "${WIDTH:-0}" -gt 0 ] && [ "${HEIGHT:-0}" -gt 0 ] || fail "PNG has no size"

[ $RC -eq 0 ] && echo "export files OK"
exit $RC
