#!/usr/bin/env bash
# Turn a plain-text static-analysis report (cppcheck/clang-tidy/compiler
# warnings/codespell) into a browsable static HTML page: findings grouped by
# category with counts, each finding linking back to the exact file:line on
# GitHub at the commit that produced the report.
#
# Usage: format-findings-report.sh <input.txt> <output.html> <title> \
#            <format: bracketed|codespell> <github-blob-base-url>
#
# bracketed  -- cppcheck/clang-tidy/gcc-warning style lines ending in a
#               bracketed category, e.g. "file.c:12:3: warning: msg [flag]"
# codespell  -- "file.c:12: word ==> suggestion" (no bracketed category;
#               grouped by the misspelled word instead)

set -euo pipefail

IN="$1"
OUT="$2"
TITLE="$3"
FORMAT="$4"
BLOB_BASE="$5"

html_escape() {
	sed -e 's/&/\&amp;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g'
}

# file:line -> a GitHub permalink, or empty if the line doesn't parse
link_for() {
	local file="$1" line="$2"
	if [ -n "$file" ] && [ -n "$line" ]; then
		printf '%s%s#L%s' "$BLOB_BASE" "$file" "$line"
	fi
}

WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT

# Normalize every finding line into: category<TAB>file<TAB>line<TAB>message
# (message already HTML-escaped). Non-matching lines (blank lines, tool
# banners) are dropped -- this is a findings report, not a full log capture.
if [ "$FORMAT" = "codespell" ]; then
	grep -E '^[^:]+:[0-9]+: .+ ==> .+$' "$IN" | while IFS= read -r line; do
		file=$(echo "$line" | sed -E 's/^([^:]+):[0-9]+:.*/\1/')
		lineno=$(echo "$line" | sed -E 's/^[^:]+:([0-9]+):.*/\1/')
		word=$(echo "$line" | sed -E 's/^[^:]+:[0-9]+: ([^ ]+).*/\1/')
		msg=$(echo "$line" | sed -E 's/^[^:]+:[0-9]+: //' | html_escape)
		printf '%s\t%s\t%s\t%s\n' "$word" "$file" "$lineno" "$msg"
	done > "$WORKDIR/findings.tsv" || true
else
	grep -E '^[^:]+:[0-9]+:.*\[[A-Za-z0-9._-]+\]$' "$IN" | while IFS= read -r line; do
		file=$(echo "$line" | sed -E 's/^([^:]+):[0-9]+:.*/\1/')
		lineno=$(echo "$line" | sed -E 's/^[^:]+:([0-9]+):.*/\1/')
		cat=$(echo "$line" | sed -E 's/.*\[([A-Za-z0-9._-]+)\]$/\1/')
		msg=$(echo "$line" | sed -E 's/^[^:]+:[0-9]+:([0-9]+:)? *//' | html_escape)
		printf '%s\t%s\t%s\t%s\n' "$cat" "$file" "$lineno" "$msg"
	done > "$WORKDIR/findings.tsv" || true
fi

# Drop line-0 entries: cppcheck emits informational "analysis limited" notes
# at 0:0 (not a real finding, and a #L0 GitHub link is meaningless).
grep -v $'\t[^\t]*\t0\t' "$WORKDIR/findings.tsv" > "$WORKDIR/findings-clean.tsv" 2>/dev/null || true
mv "$WORKDIR/findings-clean.tsv" "$WORKDIR/findings.tsv" 2>/dev/null || true

TOTAL=$(wc -l < "$WORKDIR/findings.tsv" | tr -d ' ')
UNPARSED=$(($(wc -l < "$IN") - TOTAL))

{
	cat <<HTMLHEAD
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>${TITLE}</title>
<style>
  body { font: 14px/1.5 -apple-system, sans-serif; max-width: 1000px; margin: 2rem auto; padding: 0 1rem; color: #1a1a1a; }
  h1 { font-size: 1.4rem; }
  h2 { font-size: 1.1rem; margin-top: 2rem; border-bottom: 1px solid #ddd; padding-bottom: .25rem; }
  table.summary { border-collapse: collapse; margin: 1rem 0; }
  table.summary td, table.summary th { padding: .25rem .75rem; text-align: left; border-bottom: 1px solid #eee; }
  table.summary th { border-bottom: 2px solid #ccc; }
  ul.findings { list-style: none; padding: 0; }
  ul.findings li { padding: .35rem 0; border-bottom: 1px solid #f0f0f0; font-family: ui-monospace, monospace; font-size: 13px; }
  a { color: #0969da; text-decoration: none; }
  a:hover { text-decoration: underline; }
  .count { color: #666; font-weight: normal; }
  @media (prefers-color-scheme: dark) {
    body { background: #0d1117; color: #e6edf3; }
    table.summary td, table.summary th { border-color: #30363d; }
    ul.findings li { border-color: #21262d; }
    a { color: #58a6ff; }
    .count { color: #8b949e; }
  }
</style>
</head>
<body>
<h1>${TITLE}</h1>
<p>${TOTAL} findings$([ "$UNPARSED" -gt 0 ] && echo " ($UNPARSED additional lines in the raw log did not parse as findings)").</p>
HTMLHEAD

	if [ "$TOTAL" -gt 0 ]; then
		echo '<h2>By category</h2>'
		echo '<table class="summary"><tr><th>Category</th><th>Count</th></tr>'
		cut -f1 "$WORKDIR/findings.tsv" | sort | uniq -c | sort -rn | while read -r count cat; do
			cat_esc=$(printf '%s' "$cat" | html_escape)
			printf '<tr><td>%s</td><td>%s</td></tr>\n' "$cat_esc" "$count"
		done
		echo '</table>'

		echo '<h2>Findings</h2>'
		cut -f1 "$WORKDIR/findings.tsv" | sort -u | while read -r cat; do
			cat_esc=$(printf '%s' "$cat" | html_escape)
			cat_count=$(awk -F'\t' -v c="$cat" '$1==c' "$WORKDIR/findings.tsv" | wc -l)
			printf '<h3>%s <span class="count">(%s)</span></h3>\n' "$cat_esc" "$cat_count"
			echo '<ul class="findings">'
			awk -F'\t' -v c="$cat" '$1==c' "$WORKDIR/findings.tsv" | while IFS=$'\t' read -r _ file lineno msg; do
				href=$(link_for "$file" "$lineno")
				if [ -n "$href" ]; then
					printf '<li><a href="%s">%s:%s</a> — %s</li>\n' "$href" "$file" "$lineno" "$msg"
				else
					printf '<li>%s</li>\n' "$msg"
				fi
			done
			echo '</ul>'
		done
	fi

	echo '</body></html>'
} > "$OUT"

echo "Wrote $OUT ($TOTAL findings parsed from $IN)"
