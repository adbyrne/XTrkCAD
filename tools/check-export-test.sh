#!/bin/sh
# Check the files XTrackCAD's -Texport: test writes (LayoutExportTest(), fileio.c).
#
# Usage: tools/check-export-test.sh <directory holding export.pdf/.svg/.dxf/.png>
#
# The layout is app/bin/unittest/testfiles/export-test.xtc: a straight track, a
# line, a text of size 288, which prints at 18 pt at the default print scale
# of 1:16, and a 270 degree arc starting at 200 degrees. Needs pdfinfo and
# pdftohtml (poppler-utils).

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

# PDF: one page at least, holding the text at its point size. pdftohtml reports
# the size itself, whatever the font's proportions, unlike pdftotext's box
# height. Pango's default 96 dpi instead of 72 would give 24 here (Development
# Issues #62).
[ -s "$PDF" ] || fail "no PDF"
PAGES=$(pdfinfo "$PDF" | awk '/^Pages:/ {print $2}')
[ "${PAGES:-0}" -ge 1 ] || fail "PDF has no pages"
XML=$(pdftohtml -xml -stdout -i -zoom 1 "$PDF")
FONT=$(printf '%s\n' "$XML" | sed -n 's/.*font="\([0-9]*\)">CHECKTEXT<.*/\1/p' | head -n 1)
if [ -z "$FONT" ]; then
	fail "CHECKTEXT isn't in the PDF"
else
	SIZE=$(printf '%s\n' "$XML" | sed -n "s/.*<fontspec id=\"$FONT\" size=\"\([0-9]*\)\".*/\1/p")
	echo "PDF text is ${SIZE:-unknown} pt"
	[ "$SIZE" = 18 ] || fail "printed text should be 18 pt, not ${SIZE:-unknown}"
fi

# SVG: the text and something for each drawn object
if [ -f "$SVG" ]; then
	grep -q CHECKTEXT "$SVG" || fail "CHECKTEXT isn't in the SVG"
	# The DOCTYPE is a declaration, not an element: Mini-XML 4 wrote it
	# escaped and closed with "/>" until it got its own call (#99)
	grep -qx '<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">' "$SVG" ||
		fail "the SVG's DOCTYPE isn't right: $(grep -m1 DOCTYPE "$SVG")"
	[ "$(grep -c '<line\|<path\|<polyline' "$SVG")" -ge 3 ] || fail "SVG has too few lines"
	# The arc is the only one in the layout. It sweeps more than 180 degrees,
	# so its large-arc flag has to be 1. Comparing the sweep minus the start
	# angle instead gave 0, and viewers drew the short way round between the
	# same end points (Hg r8031).
	ARCS=$(sed -n 's/.*<path d="M [-0-9]* [-0-9]* A [0-9]* [0-9]* [0-9]* \([01]\) [01] .*/\1/p' "$SVG")
	if [ -z "$ARCS" ]; then
		fail "the arc isn't in the SVG"
	elif [ "$ARCS" != 1 ]; then
		fail "the 270 degree arc has large-arc flag $ARCS, not 1"
	fi
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

# PNG with the snap grid shown (12 in, 6 divisions: a line every foot, a dot
# every 2 in): the grid adds a few lines and dots, so few pixels may differ
# from export.png. Each dot used to fill a rectangle from the dot to the far
# corner of the bitmap, which turned most of it green (Development Issues #82).
GRIDPNG="$DIR/export-grid.png"
if [ ! -s "$GRIDPNG" ]; then
	fail "no PNG with the snap grid"
elif ! command -v python3 >/dev/null 2>&1; then
	echo "no python3, not comparing export-grid.png"
else
	PCT=$(python3 -I "$(dirname "$0")/png-diff-fraction.py" "$PNG" "$GRIDPNG")
	echo "the snap grid changes ${PCT:-unknown}% of the PNG"
	# awk compares the decimal number; an empty PCT fails too
	awk -v p="$PCT" 'BEGIN { exit !(p != "" && p + 0 < 10) }' ||
		fail "the snap grid changes ${PCT:-unknown}% of the PNG, not under 10%"
fi

[ $RC -eq 0 ] && echo "export files OK"
exit $RC
