#!/usr/bin/env python3
"""Compare the halibut-built user guide against the Doxygen-converted one,
page by page, to focus dev review on pages that actually changed instead of
asking devs to eyeball all 234+ pages.

For every HTML filename present in both builds (the topic-ID/filename
compatibility that wHelp()/browserhelp.c depends on -- see
.claude/halibut-doxygen-investigation-plan.md), extracts visible body text
from each, strips known boilerplate (halibut's "Previous | Contents | Index |
Next" nav line), and scores similarity with difflib.SequenceMatcher. Also
compares each page's referenced image filenames, since a missing image is a
real regression a text-only diff would miss. Pages are sorted worst-first in
the report so low-fidelity conversions get reviewed before high-fidelity
ones that are probably fine.

Also reports:
  - halibut-only pages: topics that exist in halibut's build but have no
    matching Doxygen page at all. Includes both the expected/out-of-scope
    messages.but-generated category (topic names start with "MSG_") and any
    other name, which is a real, unconverted content gap worth flagging
    (this is how intro.but.in's 21 headings -- a 20th source file not yet
    fed into the converter -- were first found to be entirely missing,
    2026-08-08; intro.but.in was converted later the same day, and a
    smaller, still-unexplained "other" list remains -- see the plan file's
    Next Session Plan items #3 and #5).
  - doxygen-only pages: topics with no halibut match, after excluding
    Doxygen's own generated navigation infrastructure (dir_*.html,
    doxygen_crawl.html, pages.html, index-generated.html, search.html) --
    what's left is either a real filename-mismatch bug in the converter
    (e.g. an anchor ID containing "." gets a different filename out of
    Doxygen's own ID sanitizer than the literal "topic.html" halibut
    produces) or a genuinely new page not present in halibut at all.

Usage: tools/compare-userguide-html.py <halibut-html-dir> <doxygen-html-dir> <output-report.md>
"""

import os
import re
import sys
import glob
from difflib import SequenceMatcher

DOXYGEN_INFRA_FILES = {
    'index.html', 'pages.html', 'doxygen_crawl.html', 'annotated.html',
    'search.html', 'classes.html', 'namespaces.html', 'files.html',
}

NAV_BOILERPLATE_RE = re.compile(
    r'(Previous|Next|Contents|Index)(\s*\|\s*(Previous|Next|Contents|Index))+',
    re.I,
)

TAG_RE = re.compile(r'<[^>]+>')
SCRIPT_STYLE_RE = re.compile(r'<(script|style)[^>]*>.*?</\1>', re.S | re.I)
IMAGE_SRC_RE = re.compile(r'src="([^"]+\.(?:png|jpg|jpeg|gif|svg))"', re.I)

ENTITIES = {
    '&nbsp;': ' ', '&amp;': '&', '&lt;': '<', '&gt;': '>',
    '&quot;': '"', '&#39;': "'", '&mdash;': '-', '&ndash;': '-',
}


def normalize(text):
    for entity, repl in ENTITIES.items():
        text = text.replace(entity, repl)
    text = re.sub(r'\s+', ' ', text).strip()
    return text


def strip_tags(html):
    html = SCRIPT_STYLE_RE.sub(' ', html)
    return TAG_RE.sub(' ', html)


def extract_halibut_text(path):
    with open(path, encoding='utf-8', errors='replace') as f:
        html = f.read()
    m = re.search(r'<body[^>]*>(.*)</body>', html, re.S | re.I)
    body = m.group(1) if m else html
    text = strip_tags(body)
    text = NAV_BOILERPLATE_RE.sub(' ', text)
    return normalize(text)


def extract_doxygen_text(path):
    with open(path, encoding='utf-8', errors='replace') as f:
        html = f.read()
    # Doxygen's real page content lives in <div class="contents">...</div>,
    # after the search-box/header/breadcrumb chrome -- grabbing the whole
    # <body> the way extract_halibut_text() does would pollute every score
    # with identical search-widget JS/text, masking real differences.
    # Bounded at Doxygen's own "<!-- contents -->" end marker, not just
    # "rest of file": GENERATE_TREEVIEW (enabled 2026-08-08) adds a new
    # per-page "page-nav-panel" div right after the real content, which an
    # earlier, unbounded ".*" swept in too, diluting scores across the
    # whole corpus (confirmed real: avg similarity dropped 0.907 -> 0.889
    # purely from this, no actual content regression).
    m = re.search(r'<div class="contents">(.*?)</div><!-- contents -->', html, re.S)
    body = m.group(1) if m else html
    text = strip_tags(body)
    return normalize(text)


SUBPAGE_ONLY_RE = re.compile(
    r'^\s*<div class="textblock">\s*(<p>.*?</p>\s*)?'
    r'<ul>\s*(<li><a class="el"[^>]*>[^<]*</a>\s*</li>\s*)+</ul>\s*</div>\s*$',
    re.S,
)


def is_subpage_index_only(path):
    """True if a Doxygen page's entire content is a \\subpage link list (an
    optional lead-in <p>, then nothing but <li><a class="el">...) -- the
    structural signature of a converted chapter/appendix container page.
    A much more reliable signal than guessing from the halibut side's
    leading section number, which nearly every page has regardless of
    whether it's a real TOC-index page (confirmed real: an earlier,
    text-based heuristic false-positived on 72 of 232 pages)."""
    with open(path, encoding='utf-8', errors='replace') as f:
        html = f.read()
    m = re.search(r'<div class="contents">(.*)</div><!-- contents -->', html, re.S)
    if not m:
        return False
    return bool(SUBPAGE_ONLY_RE.match(m.group(1).strip()))


def extract_images(path):
    with open(path, encoding='utf-8', errors='replace') as f:
        html = f.read()
    return {os.path.basename(m) for m in IMAGE_SRC_RE.findall(html)}


def is_doxygen_infra(filename):
    return filename in DOXYGEN_INFRA_FILES or filename.startswith('dir_')


def main():
    if len(sys.argv) != 4:
        print(f'Usage: {sys.argv[0]} <halibut-html-dir> <doxygen-html-dir> <output-report.md>',
              file=sys.stderr)
        sys.exit(1)

    halibut_dir, doxygen_dir, out_path = sys.argv[1], sys.argv[2], sys.argv[3]

    halibut_files = {os.path.basename(p) for p in glob.glob(os.path.join(halibut_dir, '*.html'))}
    doxygen_files = {os.path.basename(p) for p in glob.glob(os.path.join(doxygen_dir, '*.html'))}

    doxygen_infra = {f for f in doxygen_files if is_doxygen_infra(f)}
    doxygen_content = doxygen_files - doxygen_infra

    matched = sorted(halibut_files & doxygen_content)
    halibut_only = sorted(halibut_files - doxygen_content)
    doxygen_only = sorted(doxygen_content - halibut_files)

    halibut_only_msg = sorted(f for f in halibut_only if f.startswith('MSG_'))
    halibut_only_other = sorted(f for f in halibut_only if not f.startswith('MSG_'))

    results = []
    for name in matched:
        h_text = extract_halibut_text(os.path.join(halibut_dir, name))
        d_text = extract_doxygen_text(os.path.join(doxygen_dir, name))
        # Word-level, not character-level: character-level SequenceMatcher.ratio()
        # is pathological on long prose by default (autojunk=True treats any
        # character occurring in >1% of a 200+-char sequence, e.g. plain
        # spaces, as "popped junk" and badly undercounts real matching
        # blocks -- confirmed via a real page that was obviously a near-exact
        # match by eye but scored 0.001 character-level vs. 0.94 word-level).
        # Word tokens are also a more meaningful unit for a prose diff.
        ratio = SequenceMatcher(None, h_text.split(), d_text.split()).ratio()

        h_images = extract_images(os.path.join(halibut_dir, name))
        d_images = extract_images(os.path.join(doxygen_dir, name))
        missing_images = sorted(h_images - d_images)

        results.append({
            'name': name,
            'ratio': ratio,
            'h_len': len(h_text),
            'd_len': len(d_text),
            'missing_images': missing_images,
            # halibut auto-generates a full recursive TOC ("2.1 Add Menu
            # 2.1.1 Circle Track ...") as visible body text for a chapter/
            # appendix container page; Doxygen's \subpage instead puts
            # multi-level nesting in the sidebar navigation tree, not the
            # page body, so these pages score low by design, not because
            # content was lost -- flagged separately so they don't
            # masquerade as real conversion problems (confirmed real via a
            # full-corpus run, 2026-08-08).
            'is_chapter_index': is_subpage_index_only(os.path.join(doxygen_dir, name)),
        })

    results.sort(key=lambda r: (r['ratio'], -len(r['missing_images'])))

    LOW_THRESHOLD = 0.90
    needs_review = [r for r in results
                     if not r['is_chapter_index'] and (r['ratio'] < LOW_THRESHOLD or r['missing_images'])]
    chapter_index_low = [r for r in results if r['is_chapter_index'] and r['ratio'] < LOW_THRESHOLD]
    avg_ratio = sum(r['ratio'] for r in results) / len(results) if results else 0.0

    with open(out_path, 'w', encoding='utf-8') as f:
        f.write('# Halibut vs. Doxygen user-guide comparison\n\n')
        f.write(f'{len(matched)} pages matched by filename, {len(halibut_only)} halibut-only, '
                f'{len(doxygen_only)} Doxygen-only (after excluding Doxygen nav infrastructure).\n\n')
        f.write(f'Average text-similarity ratio across matched pages: **{avg_ratio:.3f}**. '
                f'{len(needs_review)} pages flagged below for review '
                f'(similarity < {LOW_THRESHOLD} and/or a missing image; '
                f'{len(chapter_index_low)} more score low too but are expected, see below).\n\n')

        f.write('## Focus review here first\n\n')
        f.write('Sorted worst-first. A low ratio usually means real content was dropped, '
                'reordered, or mangled -- not just a formatting difference (Markdown vs. '
                'halibut markup naturally produces some noise even on a perfect conversion, '
                'so this is a triage signal, not a pass/fail grade).\n\n')
        if needs_review:
            f.write('| Topic | Similarity | Missing images |\n|---|---|---|\n')
            for r in needs_review:
                imgs = ', '.join(r['missing_images']) if r['missing_images'] else '--'
                f.write(f"| `{r['name']}` | {r['ratio']:.3f} | {imgs} |\n")
        else:
            f.write('None -- every matched page scored above the threshold with no missing images.\n')
        f.write('\n')

        f.write('## Expected low scores: chapter/appendix index pages\n\n')
        f.write('halibut auto-generates a full recursive table of contents ("2.1 Add Menu, '
                '2.1.1 Circle Track, ...") as visible body text for a chapter/appendix '
                'container page. Doxygen\'s `\\subpage` instead puts multi-level nesting in '
                'the sidebar navigation tree, not the page body -- these score low by design, '
                'not because content was lost. Not worth reviewing individually unless '
                'something else about a specific one looks wrong:\n\n')
        if chapter_index_low:
            f.write(', '.join(f"`{r['name']}` ({r['ratio']:.3f})" for r in chapter_index_low))
        else:
            f.write('(none)')
        f.write('\n')

        f.write('## All matched pages\n\n')
        f.write('| Topic | Similarity | Halibut chars | Doxygen chars |\n|---|---|---|---|\n')
        for r in results:
            f.write(f"| `{r['name']}` | {r['ratio']:.3f} | {r['h_len']} | {r['d_len']} |\n")
        f.write('\n')

        f.write('## Halibut-only pages (no matching Doxygen page)\n\n')
        f.write(f'{len(halibut_only_msg)} are `MSG_*` pages generated by `genmessages.c` from '
                '`messages.in` -- out of scope for this converter run (a separate, cheap '
                'retargeting job, not hand-conversion; see the investigation plan). Listed for '
                'completeness, not something to review individually:\n\n')
        f.write(', '.join(f'`{n}`' for n in halibut_only_msg) if halibut_only_msg else '(none)')
        f.write('\n\n')
        f.write(f'{len(halibut_only_other)} are NOT from `messages.in` -- these are real content '
                'this converter run never touched and need investigating. NOT intro.but.in '
                '(that earlier hypothesis, from before intro.but.in was converted, was wrong: '
                'converting it on 2026-08-08 left this list unchanged). Likely halibut\'s own '
                'auto-generated navigation/index pages (contents/index/IndexPage/messageList) '
                'plus the anchor-ID-with-periods filename-mismatch pages (v4.0.3_revisions/ '
                'v4.0.x_revisions, which also show up under a different sanitized name in the '
                'Doxygen-only list below) -- not yet root-caused, see the plan file\'s Next '
                'Session Plan items #3 and #5:\n\n')
        f.write(', '.join(f'`{n}`' for n in halibut_only_other) if halibut_only_other else '(none)')
        f.write('\n\n')

        f.write('## Doxygen-only pages (no halibut match, excluding nav infrastructure)\n\n')
        f.write('Each of these is either a real filename-mismatch bug in the converter (e.g. an '
                'anchor ID containing "." gets a different filename from Doxygen\'s own ID '
                'sanitizer than the literal halibut filename) or a genuinely new page:\n\n')
        f.write(', '.join(f'`{n}`' for n in doxygen_only) if doxygen_only else '(none)')
        f.write('\n')

    print(f'{len(matched)} matched, {len(halibut_only)} halibut-only '
          f'({len(halibut_only_msg)} MSG_*, {len(halibut_only_other)} other), '
          f'{len(doxygen_only)} Doxygen-only. Average similarity: {avg_ratio:.3f}. '
          f'{len(needs_review)} flagged for review.')
    print(f'Report written to {out_path}')


if __name__ == '__main__':
    main()
