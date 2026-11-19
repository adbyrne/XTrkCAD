#!/bin/sh
# Check that the app workflows trigger on the same paths.
#
# ci-gtk3.yml, ci-gtk3-preview.yml, ci-windows-msvc.yml, codeql.yml and
# release.yml each list the app's paths under on: push and on: pull_request
# (release.yml: push only). The lists are kept by hand, so this fails when
# any of them differs from ci-gtk3.yml's push list, or a workflow has none.
#
# usage: tools/check-workflow-paths.sh   (from the repository root)

dir=.github/workflows
files="ci-gtk3 ci-gtk3-preview ci-windows-msvc codeql release"

# Each paths: block of a workflow's on: section, one block per line.
blocks() {
	tr -d '\r' < "$1" | awk '
		/^on:/ { on = 1; next }
		on && /^[^ #]/ { exit }
		on && /^    paths:/ { inblock = 1; cur = ""; next }
		inblock && /^      - / { sub(/^      - /, ""); cur = cur " " $0; next }
		inblock { print cur; inblock = 0 }
		END { if (inblock) print cur }
	'
}

expected=$(blocks "$dir/ci-gtk3.yml" | head -n 1)
if [ -z "$expected" ]; then
	echo "FAIL: $dir/ci-gtk3.yml has no paths: list under on:"
	exit 1
fi
status=0
for f in $files; do
	path="$dir/$f.yml"
	found=$(blocks "$path")
	if [ -z "$found" ]; then
		echo "FAIL: $path has no paths: list under on:"
		status=1
		continue
	fi
	echo "$found" | while IFS= read -r block; do
		if [ "$block" != "$expected" ]; then
			echo "FAIL: $path lists paths:$block"
			echo "      ci-gtk3.yml lists:$expected"
			exit 1
		fi
	done || status=1
done
[ $status -eq 0 ] && echo "OK: the app workflows list the same paths:$expected"
exit $status
