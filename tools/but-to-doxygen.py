#!/usr/bin/env python3
"""Convert halibut .but user-guide sources to Doxygen \\page syntax.

Prototype/investigation tool for the Halibut -> Doxygen user-guide migration
(see .claude/halibut-doxygen-investigation-plan.md, Phase 3+). Mechanically
converts the directive set validated in Phase 3's hand conversion
(\\H/\\S/\\S2/\\S3/\\A/\\C headings -> \\page, \\e/\\f/\\c/\\q inline formatting,
\\b/\\dd bullets, \\G images, \\K/\\k cross-references -> \\ref, \\W external
links) across every app/doc/*.but file (and intro.but.in, the one templated
source -- see load_macros()'s version parameter) in one pass, and -- just as
importantly -- LOGS every site it can't convert with full confidence
(\\dd/\\dt/\\lcont definition-list structure, \\i/\\I/\\ii index-registration
directives with no Doxygen equivalent, unrecognized directives) instead of
guessing silently. Not a full formal parser -- the format is regular enough
for line-oriented regex handling, verified against real content during
Phase 3.

Usage: tools/but-to-doxygen.py <but-source-dir> <output-dir> [<version>]

Output: one <name>.dox file per input .but file (all its \\H/\\S/\\S2/\\S3/\\A/\\C
sections as separate \\page blocks, nested ones linked via \\subpage from
their parent), an images/ dir with every referenced .png copied in,
conversion-log.md listing every site that needs human review, and
index-generated.dox -- an alphabetized index page rebuilt from every
\\i/\\I/\\ii occurrence (each becomes a Doxygen \\anchor at that exact spot;
this page collects them by normalized text and \\ref-links back to every
occurrence), reproducing halibut's automatic back-of-book index instead of
just discarding the indexing semantic.
"""

import os
import re
import sys
import shutil
from dataclasses import dataclass, field

DEFINE_RE = re.compile(r'\\define\{(\w+)\}\s*(.*)')
HEADING_RE = re.compile(r'^\\(H|A|C|S|S2|S3)\{([^}]+)\}\s*(.*)$')
COMMENT_RE = re.compile(r'^\\#')

# C (chapter) and A (appendix) are containers whose OWN page has no real body
# text of its own in the source -- halibut auto-generates their visible
# "2.1 Add Menu, 2.1.1 Circle Track..." TOC listing from the document
# structure. H (heading) sections nest *beneath* them, not alongside --
# confirmed real via a full-corpus diff against the halibut build (2026-08-08):
# treating C/H as the same level (as an earlier version of this table did)
# broke parent/child tracking, so chapter/appendix pages like commandMenus.html
# lost their \subpage list and rendered with no body content at all.
LEVEL_ORDER = {'C': 0, 'A': 0, 'H': 1, 'S': 2, 'S2': 3, 'S3': 4}

# halibut's real book order -- must track app/doc/CMakeLists.txt's
# target_sources list (confirmed real: support.html, intro's last page in
# the real halibut build, correctly links next="commandMenus.html", addm's
# first page). Without this, Doxygen has no way to know these files' root
# \page (top-level, no \subpage parent) form one linear book -- it falls
# back to alphabetizing all unparented roots by TITLE in the sidebar tree
# ("Change Menu" before "Command Menus" before "Draw Menu" ... with
# "Introduction To XTrackCAD" buried in the middle), not even by filename,
# confirmed via a real build's navtreedata.js (2026-08-08 user feedback).
# osxconf.but/linconf.but are CMake's if(APPLE)/else() choice for ONE
# platform build; both included here since a single combined conversion
# covers both. chmconf.but is referenced NOWHERE in CMakeLists.txt (dead,
# orphaned -- see the Windows CHM removal noted in CLAUDE.md/bug-tracker.md)
# but still included at the very end so it doesn't become a stray
# unparented alphabetical sidebar entry either.
CANONICAL_FILE_ORDER = [
	'intro.but.in', 'addm.but', 'changem.but', 'drawm.but', 'editm.but',
	'filem.but', 'helpm.but', 'hotbar.but', 'macrom.but', 'managem.but',
	'optionm.but', 'statusbar.but', 'view_winm.but', 'navigation.but',
	'appendix.but', 'upgrade.but', 'warranty.but', 'osxconf.but',
	'linconf.but', 'chmconf.but',
]

# messages.but sits here in the real book order (see app/doc/CMakeLists.txt's
# target_sources, between appendix.but and upgrade.but) but is deliberately
# NOT in CANONICAL_FILE_ORDER above: it's not a static file this script
# parses at all -- it's build-tree output from app/help/genmessages.c's own
# -doxygen mode, already valid Doxygen syntax (see
# app/doc-doxygen-poc/CMakeLists.txt), landing directly in out_dir. This
# script only needs to know its fixed root-page anchor id to splice it into
# the \mainpage subpage list at the right position -- see main() below.
MESSAGES_ROOT_ANCHOR = 'messageList'


@dataclass
class Page:
	anchor: str
	title: str
	level: int
	parent: str = None
	children: list = field(default_factory=list)
	body_lines: list = field(default_factory=list)


@dataclass
class LogEntry:
	source_file: str
	line_no: int
	anchor: str
	category: str
	detail: str


LOG = []

# Every \i/\I/\ii occurrence, for the generated index page: (display_text,
# anchor_id, containing_page_anchor).
INDEX_ENTRIES = []
_index_seq = [0]


def log(source_file, line_no, anchor, category, detail):
	LOG.append(LogEntry(source_file, line_no, anchor, category, detail))


def make_index_anchor(display_text, page_anchor):
	"""Unique Doxygen \\anchor id for one index occurrence. Occurrences must
	be individually unique (Doxygen anchors are global), but the generated
	index page groups them back together by normalized display_text -- the
	same merge-multiple-occurrences-under-one-entry behavior a real book
	index has."""
	_index_seq[0] += 1
	slug = re.sub(r'[^A-Za-z0-9]+', '_', display_text).strip('_') or 'entry'
	anchor_id = f'idx_{slug}_{_index_seq[0]}'
	INDEX_ENTRIES.append((display_text, anchor_id, page_anchor))
	return anchor_id


def load_macros(doc_dir, version=None):
	macros = {}
	intro_in = os.path.join(doc_dir, 'intro.but.in')
	if os.path.exists(intro_in):
		with open(intro_in, encoding='utf-8') as f:
			for line in f:
				m = DEFINE_RE.match(line.strip())
				if m:
					macros[m.group(1)] = m.group(2).strip()
	if version:
		# intro.but.in's \define{XTCWinPack}/\XTCRPMPack/\XTCStgzPack expand
		# to filenames containing the literal CMake template token
		# "@XTRKCAD_VERSION@" (substituted by configure_file() when the real
		# halibut build turns intro.but.in into intro.but -- see
		# app/doc/CMakeLists.txt). Now that intro.but.in itself is converted
		# by this script (see main()'s .but.in handling below), resolve the
		# token here the same way, from a caller-supplied real version
		# string, so pages that use these macros (e.g. \S{MSWinInstall})
		# don't leak the literal template token into the committed .dox.
		for name in macros:
			macros[name] = macros[name].replace('@XTRKCAD_VERSION@', version)
	return macros


def strip_braced(text, cmd):
	"""Replace \\cmd{...} with a callback-computed replacement, handling one
	level of nested braces (halibut content doesn't nest deeper than that
	for these inline commands, confirmed against the real corpus)."""
	out = []
	i = 0
	pat = '\\' + cmd + '{'
	while True:
		j = text.find(pat, i)
		if j == -1:
			out.append(text[i:])
			break
		out.append(text[i:j])
		depth = 1
		k = j + len(pat)
		start = k
		while k < len(text) and depth > 0:
			if text[k] == '{':
				depth += 1
			elif text[k] == '}':
				depth -= 1
			k += 1
		inner = text[start:k - 1]
		out.append(('MATCH', inner))
		i = k
	return out


def sub_braced(text, cmd, transform):
	"""re.sub-style wrapper around strip_braced: apply transform(inner) to
	every \\cmd{...} match and reassemble."""
	return ''.join(transform(part[1]) if isinstance(part, tuple) else part
	               for part in strip_braced(text, cmd))


def escape_quote_in_tag(s):
	"""Escape literal '"' before wrapping content in a real <i>/<b> HTML
	tag (used by \\e{}/\\f{}/\\i{}/\\ii{}'s non-title-line output below).
	Confirmed via an isolated Doxygen build (2026-08-09, real corpus
	occurrence: navigation.but's \\e{12ft 4 1/2in, 12' 4.5", ...}) that a
	literal '"' between an opening and closing tag makes Doxygen's HTML
	parser think it's inside a quoted attribute value -- it then keeps
	scanning PAST the real closing tag for the next '"' it can find
	anywhere later in the page, silently swallowing everything in between
	into one giant emphasis span (real, confirmed cross-content
	corruption, not just a warning, though it does also warn: "end of
	comment block while expecting command </i>"). A named HTML entity
	(matching \\uXXXX's degree-sign fix and CANONICAL_FILE_ORDER's brace
	fix -- named/raw, never numeric &#NNN; entities, see their comments)
	renders correctly and was confirmed not to reproduce the corruption.
	Not needed/applied for '"' outside these tags, or for \\q{}'s own
	generated quotes (added later in the pipeline than this) -- both
	confirmed safe as real '"' characters via the same isolated build."""
	return s.replace('"', '&quot;')


def convert_w(text):
	"""\\W{url}{text} external link -> Doxygen/Markdown "[text](url)". Both
	url and text are brace-depth-aware (not the plain "[^}]*" regex this
	used before) -- confirmed necessary: text commonly nests another
	macro's own braces, e.g. "\\W{url}{\\e{XTrackCAD} Fork Website}" (5 real
	occurrences: intro.but.in x4, upgrade.but x1). The old regex stopped at
	\\e{}'s own closing brace, truncating the link text and leaving a
	dangling, unpaired "_" after the malformed markdown link -- rendered as
	literal underscores in the browser (real user report, 2026-08-09)."""
	def scan_braced(start):
		"""From just after an opening '{' at start-1, return (end, balanced)
		where end is the index just past the matching '}' (or len(text) if
		never balanced)."""
		depth = 1
		k = start
		while k < len(text) and depth > 0:
			if text[k] == '{':
				depth += 1
			elif text[k] == '}':
				depth -= 1
			k += 1
		return k, depth == 0

	out = []
	i = 0
	pat = '\\W{'
	while True:
		j = text.find(pat, i)
		if j == -1:
			out.append(text[i:])
			break
		out.append(text[i:j])

		url_start = j + len(pat)
		url_end, url_balanced = scan_braced(url_start)
		has_second_group = url_balanced and url_end < len(text) and text[url_end] == '{'
		if not has_second_group:
			# Not a real \W{...}{...} (unbalanced url arg, or no second
			# group) -- leave as-is rather than guessing.
			out.append(text[j:url_end])
			i = url_end
			continue

		url = text[url_start:url_end - 1]
		text_start = url_end + 1
		text_end, _ = scan_braced(text_start)
		link_text = text[text_start:text_end - 1]
		out.append(f'[{link_text}]({url})')
		i = text_end
	return ''.join(out)


def convert_inline(text, macros, source_file, line_no, anchor, defer_anchors=None):
	"""Apply all inline-directive substitutions to one line/paragraph of body
	text. Returns the converted text; side-effects LOG for lossy/uncertain
	conversions.

	defer_anchors: if given a list, \\i/\\I/\\ii's \\anchor commands are
	pushed onto it instead of inlined into the returned text -- required
	when converting a \\page/\\subpage TITLE line, since Doxygen doesn't
	process \\anchor correctly there (confirmed: produces "Invalid anchor
	id" warnings and leaks the literal command text into <title>). The
	caller is responsible for emitting the deferred anchors as the page's
	first body line instead."""

	# Macro substitution (\XTCCopyRight etc.)
	for name, expansion in macros.items():
		text = text.replace('\\' + name, expansion)

	# Escape literal backtick characters used as old-style typographic open
	# quotes in prose (e.g. GPL boilerplate: "type `show w'." in
	# warranty.but) -- an unpaired "`" is a Markdown inline-code delimiter
	# and left the parser searching for a close for the rest of the file
	# (confirmed via a real build). Must run before \c{...} below generates
	# its own real backticks (confirmed no \c{...} content in the corpus
	# contains a literal backtick itself, so this can't clobber those).
	text = text.replace('`', '\\`')

	# \c{...} code/monospace -- must run before the "*" escape below: inside
	# a backtick code span markdown never interprets "*" as emphasis, so
	# escaping it there would leak a literal backslash into visible code
	# text (real case: \c{*.xtp} must render as `*.xtp`, not `\*.xtp`).
	# Brace-depth-aware (via sub_braced/strip_braced), not a plain "stop at
	# first }" regex: intro.but.in has \c{\{HOME\}/.xtrkcad/} -- halibut's
	# own literal-brace escape used *inside* a \c{} span. A naive [^}]*
	# regex stops at the '}' half of the inner "\}" escape, truncating the
	# match and leaking "}/.xtrkcad/}." as stray trailing text (confirmed
	# via a real build before this fix).
	text = sub_braced(text, 'c', lambda inner: f'`{inner}`')

	# Escape literal "*" in the source prose (e.g. "An asterisk (*) after the
	# filename..." in navigation.but, or "2 * Coupler Length" in managem.but)
	# before any of our own bold/italic markers are generated below. An
	# unescaped "*" is a Markdown emphasis delimiter -- confirmed via a real
	# build: one unpaired literal "*" left Doxygen's markdown parser looking
	# for a closing "*"/"_" for the rest of the file, corrupting unrelated
	# later pages ("end of comment block while expecting command </em>").
	# Must run before \f{}/\e{} below add their own "**"/"_..._" markers, or
	# those would be escaped too.
	text = text.replace('*', '\\*')

	# Escape literal "<"/">" in prose (GPL boilerplate placeholders in
	# warranty.but: "<year>", "<name of author>", "<signature of Ty Coon>")
	# -- Doxygen's HTML/XML validator otherwise treats them as real,
	# unsupported tags. None of our own generated output (\ref, \page,
	# ![](...), [text](url), or the <ul>/<li> wrapper added later in
	# parse_but_file) uses literal "<"/">", so this is safe to do
	# unconditionally on the raw source text.
	text = text.replace('<', '&lt;').replace('>', '&gt;')

	# halibut's own literal-brace escape ("\{"/"\}", e.g. "\{0,0\}" for a
	# coordinate notation). Two prior findings, both real, both confirmed
	# via isolated builds:
	#
	# 1. (2026-08-08) Leaving it as "\{"/"\}" -- Doxygen's OWN escape
	#    syntax for a literal brace, so not obviously wrong -- destabilizes
	#    Doxygen's markdown parser when it lands inside list content: real,
	#    silent cross-page content corruption (cmdRaiseElev.html rendered
	#    trackDescribeBezier's text instead of its own), not just a
	#    warning. HTML numeric entities (&#123;/&#125;) were adopted to
	#    sidestep this, since they have no special meaning to Doxygen's
	#    comment/command parser at all.
	#
	# 2. (2026-08-09) Those entities themselves never render: real user
	#    reports across ~10 pages (Directories Overview's "{HOME}", Common
	#    Draw Object Fields'/Cornu/Bezier Track Object Fields' "{0,0}",
	#    Protractor's "{Left-Drag", etc.) all showed the literal text
	#    "&#123;"/"&#125;" in the browser instead of "{"/"}". Confirmed via
	#    an isolated Doxygen build: Doxygen HTML-escapes the "&" in any
	#    "&#NNN;" numeric reference (producing "&amp;#NNN;") rather than
	#    decoding it, in every context tested (plain prose, list items,
	#    and inside \c{} code spans, where a numeric reference could never
	#    have rendered anyway -- CommonMark spec never decodes entities
	#    inside code spans, they're always literal).
	#
	# Fix, verified via the same isolated-build method as finding 1 (two
	# \page blocks, a list item with the brace on the first page, plain
	# prose on the second, checked for both correct rendering AND that the
	# second page's content doesn't bleed): plain, real "{"/"}" characters
	# (no backslash, not an entity) render correctly in every context
	# AND do not reproduce finding 1's corruption -- it's specifically
	# Doxygen's own "\{"/"\}" escape sequence that destabilizes the list
	# parser, not an unescaped brace character.
	text = text.replace('\\{', '{').replace('\\}', '}')

	# Bare \i immediately before another command with no braces of its own
	# (e.g. "\i\e{Circle Radius}" -- confirmed real, 3 occurrences total,
	# all this exact pattern) means "also index-register the following
	# command's text" in halibut. Not worth the complexity of threading
	# that through generically for 3 sites -- strip the bare \i and log it,
	# rather than silently leaving "\i" as literal text next to the
	# following command's output (confirmed via a real build: produces a
	# corrupt "\i_Circle Radius_"-style string that Doxygen then parses as
	# one broken command). Must run before \e{}/\f{}/etc are substituted --
	# otherwise the following command is already gone by the time this
	# pattern would match, and the bare \i survives uncaught.
	def bare_i_repl(m):
		log(source_file, line_no, anchor, 'index-term',
		    f'bare \\i immediately before {m.group(1)!r} (no braces of its own) -- index-registration on the following command dropped, needs manual review')
		return m.group(1)  # drop just the bare \i, keep the following command intact
	text = re.sub(r'\\i(\\[a-zA-Z]+)', bare_i_repl, text)

	# \W{url}{text} external link -- do before other brace commands since it
	# has two brace groups. See convert_w()'s own docstring: brace-depth-
	# aware, not a plain "[^}]*" regex (needed for nested macros like
	# \e{} inside the text argument).
	text = convert_w(text)

	# \G{path} image embed -- flatten to basename, images/ dir holds them all.
	def g_repl(m):
		path = m.group(1)
		base = os.path.basename(path)
		return f'![]({base})'
	text = re.sub(r'\\G\{([^}]*)\}', g_repl, text)

	# \K{id} / \k{id} cross-reference -> \ref id (Doxygen fills in the link
	# text from the target page's own title, matching halibut's own
	# auto-text behavior for \K without an explicit override).
	text = re.sub(r'\\[Kk]\{([^}]*)\}', lambda m: f'\\ref {m.group(1)}', text)

	# \f{...} bold (XTrkCad's own added directive). Real <b> HTML, not
	# markdown "**...**": confirmed via a minimal, isolated Doxygen build
	# that "**word**" silently loses its OPENING "**" (closing one stays as
	# literal text) whenever it's the first thing in a paragraph -- a real
	# Doxygen markdown-parser quirk, not a converter bug (mid-sentence bold
	# is unaffected). \f{} very often lands at a paragraph's start (used
	# both for inline emphasis and as informal sub-headings throughout the
	# corpus), so this hit constantly. halibut itself renders \f{} as
	# plain <strong> (confirmed against a real halibut build, e.g.
	# whyXTrackCAD.html) -- <b> is the exact same visual weight, so this is
	# equivalence with halibut's own output, not an upgrade beyond it.
	# Brace-depth-aware (via sub_braced), not a plain "[^}]*" regex --
	# confirmed real: intro.but.in has \f{Parameter Files (\K{cmdPrmfile})},
	# a nested command inside \f{}'s own argument. Currently harmless only
	# because \K/\k happens to run before \f{} in this pipeline (so its
	# braces are already gone by the time \f{}'s own match runs) -- fixed
	# properly rather than left order-dependent, same reasoning as \c{}'s
	# existing brace-depth handling and \e{}'s below (which IS live-broken
	# by this exact class of bug, see \e{}'s own comment).
	text = sub_braced(text, 'f', lambda inner: f'<b>{escape_quote_in_tag(inner)}</b>')

	# \q{...} quoted UI-option text. Brace-depth-aware for the same reason
	# as \f{} above, even though no real \q{\cmd{...}} nesting exists in
	# the corpus today.
	text = sub_braced(text, 'q', lambda inner: f'"{inner}"')

	# \e{...} emphasis -> italics. Real <i> HTML, not markdown "_..._" --
	# confirmed via isolated Doxygen builds (2026-08-09, real user reports
	# across ~7 pages) that Doxygen's markdown emphasis parser drops
	# underscore emphasis entirely in two real, common shapes in this
	# corpus: intraword (\e{Left-Click}ing -> "_Left-Click_ing", the
	# closing "_" immediately followed by more word characters, e.g.
	# appendix.but/addm.but/drawm.but's "ing" suffix pattern) and
	# adjacent to a literal "[" (\e{Ctrl+Shift+[} -> "_Ctrl+Shift+[_",
	# navigation.but's keyboard-shortcut list -- "[" appears to trigger
	# Doxygen's link-reference scanning and corrupts the emphasis match).
	# Tried "*...*" first since CommonMark's spec allows intraword
	# emphasis for asterisks but not underscores -- confirmed via the same
	# isolated-build method that Doxygen's implementation doesn't honor
	# that distinction either and drops asterisk emphasis in both shapes
	# too. Real <i> tags (matching \f{}'s existing <b> fix immediately
	# below, same underlying reasoning) sidestep Markdown's emphasis
	# parsing entirely and were confirmed correct in both broken shapes
	# plus the normal case.
	#
	# Brace-depth-aware (via sub_braced), not a plain "[^}]*" regex --
	# confirmed real AND live-broken: view_winm.but/changem.but both have
	# \e{\i{Zoom/Pan Shortcut Keys}}, a nested \i{} inside \e{}'s own
	# argument. \i{} runs AFTER \e{} in this pipeline, so \e{}'s old plain
	# regex stopped at \i{}'s still-unprocessed closing brace, truncating
	# the match and leaving a stray "}" that \i{}'s OWN later regex then
	# absorbed as trailing text -- confirmed real via the generated index
	# page showing a corrupted entry ("Zoom/Pan Shortcut Keys</i>") and a
	# real Doxygen warning ("found </i> tag without matching <i>"), found
	# investigating the underscore-emphasis bug above, 2026-08-09.
	text = sub_braced(text, 'e', lambda inner: f'<i>{escape_quote_in_tag(inner)}</i>')

	# \i{...} visible italic + index registration. Keep the visible text AND
	# preserve the indexing semantic via a Doxygen \anchor right after it --
	# index-generated.dox (built at the end of the run) collects every one
	# of these into a real alphabetized index page, same as halibut's own
	# automatic index, instead of just discarding the registration.
	# Brace-depth-aware (via sub_braced) for all three of \i{}/\ii{}/\I{}
	# below, not a plain "[^}]*" regex -- confirmed real: intro.but.in has
	# \i{Removing \e{XTrackCAD}}, a nested \e{} inside \i{}'s own argument
	# (currently harmless only because \e{} runs before \i{} in this
	# pipeline, same order-dependency reasoning as \f{}'s comment above).
	def i_repl(inner):
		idx_id = make_index_anchor(inner, anchor)
		log(source_file, line_no, anchor, 'index-term',
		    f'\\i{{{inner}}} -> italic + \\anchor {idx_id}, collected into index-generated.dox')
		if defer_anchors is not None:
			# Plain text only, no "_..._" emphasis markers -- confirmed via
			# a real build: markdown emphasis embedded directly in a \page
			# TITLE line (not just \anchor) corrupts emphasis parsing for
			# the rest of that page's body (real case: \S{enterValue}
			# \i{Entering Values} produced "\page enterValue _Entering
			# Values_", which threw off later, unrelated _..._ emphasis on
			# the same page with an "expecting command </em>" warning).
			defer_anchors.append(idx_id)
			return inner
		# Trailing space is required, not cosmetic: halibut's own source
		# often butts \i{...}/\I{...} directly against the next word with
		# no space (e.g. "\I{Run Trains}During this command..."). Without
		# it, "\anchor idx_..." swallows the following word as part of the
		# anchor name, and every \ref to that anchor then dangles
		# (confirmed via a real build: idx_Run_Trains_219 unresolved).
		# Real <i> HTML, not markdown "_..._" -- same fix as \e{} above,
		# same reasoning (confirmed via the same isolated-build method:
		# Doxygen drops underscore -- and asterisk -- emphasis in the
		# intraword and "["-adjacent shapes real \i{} occurrences hit too).
		return f'<i>{escape_quote_in_tag(inner)}</i> \\anchor {idx_id} '
	text = sub_braced(text, 'i', i_repl)

	# \ii{...} rare double-emphasis/index variant -- treat as bold, same
	# anchor-and-collect treatment.
	def ii_repl(inner):
		idx_id = make_index_anchor(inner, anchor)
		log(source_file, line_no, anchor, 'index-term',
		    f'\\ii{{{inner}}} -> bold + \\anchor {idx_id}, collected into index-generated.dox (rare directive, only 3 uses total)')
		if defer_anchors is not None:
			# Plain text only in a \page TITLE line -- same reasoning as
			# i_repl above.
			defer_anchors.append(idx_id)
			return inner
		# <b> HTML, not markdown "**...**" -- same paragraph-start bug as
		# \f{} above, see that comment.
		return f'<b>{escape_quote_in_tag(inner)}</b> \\anchor {idx_id} '
	text = sub_braced(text, 'ii', ii_repl)

	# \I{...} silent index-only registration -- no visible text in the
	# original either, so just the \anchor (no italic/bold wrapper),
	# collected the same way. Frequently appears stacked on a heading line
	# (e.g. "\S{cmdUndo} Undo and Redo \I{Undo} \I{Redo}") registering
	# multiple index aliases for one page -- confirmed via the real corpus,
	# this is the single biggest reason defer_anchors exists.
	def cap_i_repl(inner):
		idx_id = make_index_anchor(inner, anchor)
		log(source_file, line_no, anchor, 'index-term-silent',
		    f'\\I{{{inner}}} -> \\anchor {idx_id} only (matches original: no visible output), collected into index-generated.dox')
		if defer_anchors is not None:
			defer_anchors.append(idx_id)
			return ''
		return f'\\anchor {idx_id} '
	text = sub_braced(text, 'I', cap_i_repl)

	# \u000 / \u00B0 etc: \u000 is XTrkCad's blank-line spacer (no visible
	# text) -- drop. Other \uXXXX are literal unicode codepoints -- decode.
	def u_repl(m):
		code = m.group(1)
		try:
			codepoint = int(code, 16)
		except ValueError:
			log(source_file, line_no, anchor, 'unrecognized',
			    f'\\u{code} -- unrecognized unicode escape, left as-is')
			return m.group(0)
		# XTrkCad's own blank-line spacer shorthand -- seen as both \u000
		# and \u00 in the real corpus (the latter likely a source typo that
		# happens to still "work" the same way in halibut). Any codepoint
		# below 0x20 is a C0 control character, never an intentional
		# visible character in end-user docs -- treat all of them as the
		# same no-op rather than special-casing exact digit strings, which
		# missed \u00 and would've emitted a literal NUL byte.
		if codepoint < 0x20:
			return ''
		return chr(codepoint)
	text = re.sub(r'\\u([0-9A-Fa-f]+)', u_repl, text)

	if defer_anchors is not None:
		# A \page/\subpage TITLE line isn't Markdown-processed the way
		# body text is -- confirmed via a real build: a title containing
		# raw "_..._"/"**...**" emphasis (from \e{}/\i{}/\ii{} used
		# directly in a heading, e.g. "\A{upgrades} ... \e{XTrackCAD}
		# Version") corrupts emphasis parsing for the rest of that page's
		# BODY, not just the title itself ("expecting command </em>").
		# i_repl/ii_repl above already return plain text in this mode;
		# this catches \e{} (and anything else) the same way, plus
		# unwraps escapes that only mattered for Markdown body text (a
		# title is never Markdown-parsed, so \*/`` \` `` don't need
		# protecting there).
		text = re.sub(r'(?<!\\)\*\*?', '', text)
		text = re.sub(r'(?<!\\)_', '', text)
		text = text.replace('\\*', '*').replace('\\`', '`')

	return text


def parse_but_file(path, macros, stack):
	"""Parse one .but file into a flat list of Page objects (document order),
	with parent/child relationships recorded for \\subpage linking.

	stack: the (level, anchor) heading-ancestry list, SHARED and mutated
	across every file in CANONICAL_FILE_ORDER (see main()), not local to
	this file. Confirmed real and necessary: 11 of the corpus's 20 files
	(changem.but, drawm.but, editm.but, filem.but, helpm.but, hotbar.but,
	macrom.but, managem.but, optionm.but, statusbar.but, view_winm.but)
	open with a bare \\H{} heading, not their own \\C{}/\\A{} chapter --
	in halibut's real book, they CONTINUE addm.but's still-open
	\\C{commandMenus} chapter (confirmed via the real .but source: none of
	them starts a new \\C/\\A). A fresh per-file stack (the old behavior)
	always saw these as parentless, promoting all 11 to independent
	top-level sidebar entries instead of children of "Command Menus" --
	confirmed real via a user report, 2026-08-09 ("Command Menus" showing
	only its one "Add Menu" child). A real \\C{}/\\A{} heading is
	self-correcting even with a shared stack: LEVEL_ORDER's C/A level (0)
	pops the entire stack before pushing itself (see the "while stack and
	stack[-1][0] >= level" pop loop below), so navigation.but's/
	appendix.but's/etc.'s own chapter starts are unaffected."""
	source_file = os.path.basename(path)
	with open(path, encoding='utf-8') as f:
		lines = f.readlines()

	pages = []
	current = None
	# Halibut's \dt/\dd definition-list pair. Tried real HTML <dl>/<dt>/<dd>
	# first (Doxygen's HTML validator rejects it: \dd with no preceding \dt
	# is the *common* case in this corpus, not the exception, and it errors
	# on every one -- "expected <dt> tag but found <dd> instead!"), then
	# hand-tracked <ul>/<li> (worked in isolation, but its manual tag-balance
	# bookkeeping doesn't survive interacting with adjacent Markdown
	# constructs like the \subpage bullet list write_dox() appends after a
	# page's body -- cascading "lonely <li>" warnings, confirmed via a real
	# build). Landed on plain Markdown bullets: they compile to the exact
	# same <ul>/<li> HTML, but Markdown's own list parser (unlike Doxygen's
	# manual HTML tag tracker) already handles word-wrapped continuation
	# lines and mixes safely with everything else on the page.
	#
	# dt_open tracks halibut's real \dl semantics on top of that flat
	# scheme: a \dt's following \dd(s), up until the next \dt or the list's
	# end, are its children -- indent them 2 spaces to open a genuine
	# nested Markdown sub-list under the \dt bullet (matching halibut's own
	# real <dl>/<dt>/<dd> rendering, which browsers indent by default; our
	# flat bullets lost that visual hierarchy entirely). A \dd with no
	# open \dt (confirmed the common case, see above) stays flat, exactly
	# as before. Confirmed via a real, isolated Doxygen build: this nests
	# correctly, including a word-wrapped continuation line for the nested
	# \dd left *unindented* (plain CommonMark lazy continuation still
	# attaches it to the right list item -- no indentation needed there,
	# consistent with how continuation lines already work everywhere else
	# in this parser).
	in_list = False
	dt_open = False
	# lcont_open: true while inside a multi-line \lcont{...} block (opening
	# brace alone on its own line, closed later by a bare "}" on its own
	# line -- 71 of 112 \lcont{} occurrences in the real corpus are this
	# shape). halibut's \lcont means "this content, often a nested \b
	# bullet list, continues inside the CURRENT still-open \dd/\dt item" --
	# semantically the opposite of a list-closing event. \b bullets emitted
	# while lcont_open indent one level deeper than the current \dt/\dd
	# context (matching halibut's real <dd><ul>...</ul></dd> structure,
	# confirmed via a real build); the closing "}" is consumed silently
	# (see the dedicated check below) instead of leaking as literal text.
	lcont_open = False

	def close_list():
		nonlocal in_list, dt_open, lcont_open
		in_list = False
		dt_open = False
		lcont_open = False

	blank_spacer_re = re.compile(r'^\\u[0-9A-Fa-f]+$')

	for line_no, raw in enumerate(lines, 1):
		line = raw.rstrip('\n')
		stripped = line.strip()

		if COMMENT_RE.match(stripped):
			continue

		m = HEADING_RE.match(stripped)
		if m:
			close_list()
			kind, anchor, rest = m.group(1), m.group(2), m.group(3)
			level = LEVEL_ORDER[kind]
			deferred = []
			title_text = convert_inline(rest, macros, source_file, line_no, anchor,
			                            defer_anchors=deferred)
			parent = None
			while stack and stack[-1][0] >= level:
				stack.pop()
			if stack:
				parent = stack[-1][1]
			stack.append((level, anchor))
			current = Page(anchor=anchor, title=title_text, level=level, parent=parent)
			for idx_id in deferred:
				# \anchor doesn't work inline in a \page title line
				# (confirmed empirically -- Doxygen warns and leaks the
				# literal command text into <title>); emit it as the
				# page's own first body line instead, where it works fine.
				current.body_lines.append(f'\\anchor {idx_id}')
			pages.append(current)
			continue

		# Blank-spacer lines (\rule, or a standalone \u000/\u00 no-op escape
		# used the same way) are absorbed while inside a \dt/\dd run rather
		# than breaking it -- they're just paragraph spacing between a \dt
		# and its \dd, or between two \dd paragraphs of the same entry, in
		# the real corpus. Markdown's own "lazy continuation" already folds
		# the next non-blank line back into the same list item as long as
		# we don't insert an actual blank body_lines entry here.
		if stripped == '\\rule' or blank_spacer_re.match(stripped):
			if current is not None and not in_list:
				current.body_lines.append('')
			continue

		if stripped.startswith('\\cfg{'):
			close_list()
			log(source_file, line_no, current.anchor if current else '?',
			    'cfg-dropped', f'{stripped[:80]} -- dropped (per-page metadata, not needed: macOS release build uses browserhelp.c not Apple Help, confirmed Phase 2)')
			continue

		if stripped.startswith('\\lcont{'):
			# Deliberately NOT close_list(): \lcont always means "this
			# content continues inside the CURRENT still-open \dd/\dt
			# item", never a list-closing event -- closing the list here
			# was itself a real bug (confirmed via a real build: it
			# silently broke nesting for every \dt/\dd sequence that had a
			# \lcont anywhere in the middle, e.g. every \lcont{\u000}
			# blank-continuation marker, not just the multi-line blocks).
			inner = stripped[len('\\lcont{'):]
			if inner.endswith('}'):
				# Self-closed on one line (e.g. "\lcont{\u000}", 40 of 112
				# real occurrences) -- unchanged from before, just without
				# the close_list() call.
				inner = inner[:-1]
				if current is not None and inner.strip():
					current.body_lines.append(
					    convert_inline(inner, macros, source_file, line_no, current.anchor))
			else:
				# Multi-line block: content (typically nested \b bullets,
				# see the \b handler below) follows on subsequent lines,
				# closed later by a bare "}" on its own line (handled
				# next). 71 of 112 real occurrences are this shape.
				log(source_file, line_no, current.anchor if current else '?',
				    'lcont', 'multi-line list-continuation block -- nested content indented one level deeper, see \\b handler')
				lcont_open = True
			continue

		if stripped == '}' and lcont_open:
			# The multi-line \lcont{...} block's closing brace -- consume
			# silently instead of leaking as a literal "}" line (confirmed
			# real bug: e.g. whyXTrackCAD.html rendered a stray "<p>}</p>"
			# between the "Flexible and Powerful Printing" sublist and the
			# next \dd, because nothing previously recognized this line at
			# all).
			lcont_open = False
			continue

		if stripped.startswith('\\dd'):
			body = stripped[3:].strip()
			if current is not None:
				in_list = True
				marker = '  - ' if dt_open else '- '
				current.body_lines.append(marker + convert_inline(body, macros, source_file, line_no, current.anchor))
			continue

		if stripped.startswith('\\dt'):
			body = stripped[3:].strip()
			if current is not None:
				in_list = True
				dt_open = True
				# <b> HTML, not markdown "**...**": a plain-text \dt term is
				# fine either way (list items aren't affected by \f{}'s
				# paragraph-start bug, confirmed via a real build), but a
				# \dt whose own content contains other markup (\f{}/\i{},
				# now already <em>/<b>/<a> HTML by the time it gets here)
				# produces "**<b>...</b>**" -- markdown bold wrapping HTML
				# -- which Doxygen's parser also fails to close correctly
				# (confirmed real: bugs.html, mouseBcmd.html, cmdPrint.html,
				# mainW.html). <b> avoids this combination entirely.
				current.body_lines.append('- <b>' + convert_inline(body, macros, source_file, line_no, current.anchor) + '</b>')
			continue

		if stripped.startswith('\\b'):
			body = stripped[2:].strip()
			if current is not None:
				if lcont_open:
					# Nested inside an open \lcont block -- one level
					# deeper than whatever \dt/\dd context is already
					# active (matching halibut's real <dd><ul>...</ul></dd>
					# structure, confirmed via a real build), and does NOT
					# close that outer context (a \dd/\dt after this \lcont
					# closes must still see the same dt_open state).
					marker = '    - ' if dt_open else '  - '
				else:
					# Standalone \b (the common case, e.g. a page's own
					# top-level feature list) still resets any prior
					# \dt/\dd list state, same as always.
					close_list()
					marker = '- '
				current.body_lines.append(marker + convert_inline(body, macros, source_file, line_no, current.anchor))
			continue

		if stripped.startswith('\\n'):
			if in_list:
				# \n mid-list is just a fresh paragraph within the same
				# item in this corpus (word-wrapped source), not a new
				# item. Tried indenting it 2 spaces to make Markdown treat
				# it as a proper new paragraph within the list item --
				# rejected: confirmed via a real build that this triggers
				# "End of list marker found without any preceding list
				# items" AND actually corrupts a *different, later* page's
				# rendered content (cmdRaiseElev.html picked up
				# trackDescribeBezier's text) -- not just a warning, real
				# content corruption Doxygen's own build log never flags.
				# Plain lazy continuation (no indent, folds into the same
				# paragraph rather than starting a new one) sacrifices a
				# paragraph break but doesn't destabilize the parser.
				body = stripped[2:].strip()
				if current is not None:
					current.body_lines.append(convert_inline(body, macros, source_file, line_no, current.anchor))
				continue
			close_list()
			body = stripped[2:].strip()
			if current is not None:
				current.body_lines.append(convert_inline(body, macros, source_file, line_no, current.anchor))
			continue

		# Halibut's unbraced line-start \c (code paragraph, distinct from
		# the inline \c{...} code span) -- accumulate consecutive lines
		# into one Doxygen \verbatim block rather than leaving the literal
		# "\c " text in the output (confirmed real: 42 lines across
		# optionm.but/navigation.but/warranty.but).
		if re.match(r'^\\c(\s|$)', stripped):
			close_list()
			body = stripped[2:].strip()
			if current is not None:
				if current.body_lines and current.body_lines[-1] == '\\endverbatim':
					current.body_lines.pop()
					current.body_lines.append(body)
					current.body_lines.append('\\endverbatim')
				else:
					log(source_file, line_no, current.anchor if current else '?',
					    'code-paragraph', f'\\c code-paragraph line(s) -> \\verbatim block: {body[:60]}')
					current.body_lines.append('\\verbatim')
					current.body_lines.append(body)
					current.body_lines.append('\\endverbatim')
			continue

		# Halibut's columnar bold/italic marker line -- a bare \e line of
		# spaces and b/i letters positioned under the \c code line above to
		# mark which columns are bold/italic. No Markdown/Doxygen
		# equivalent for column-positioned emphasis; drop and log rather
		# than leaving the literal marker text in the output (confirmed
		# real: 8 occurrences, always immediately after a \c line).
		if stripped.startswith('\\e') and not stripped.startswith('\\e{'):
			log(source_file, line_no, current.anchor if current else '?',
			    'positional-emphasis-dropped',
			    f'bare \\e position-marker line (halibut columnar bold/italic marker for the preceding \\c line) -- no Markdown equivalent, dropped: {stripped[:60]}')
			continue

		# Catch-all: any other line starting with an unrecognized top-level
		# directive we haven't handled explicitly.
		unrec = re.match(r'^\\([A-Za-z]+)\b', stripped)
		known_inline_starts = ('f', 'e', 'c', 'q', 'i', 'ii', 'I', 'K', 'k', 'W', 'G', 'u')
		is_plain_continuation = not unrec or unrec.group(1) in known_inline_starts

		if (in_list or lcont_open) and is_plain_continuation:
			# Word-wrapped continuation of the paragraph a \dd/\dt started
			# (the common case: real prose in this corpus wraps across
			# several physical lines with no per-line directive). No
			# indentation -- plain CommonMark lazy continuation (a
			# non-blank, non-block-starting line immediately following a
			# list item folds into it with no indent needed) -- matches
			# the \n-mid-list fix above; indenting was found to
			# destabilize Doxygen's markdown parser on some pages, not
			# just draw a warning. Tried indenting this to match the
			# nested \b depth while lcont_open specifically (to fix a
			# trailing plain-prose paragraph after a nested \b sublist,
			# e.g. mainW.html's "Note:" case) -- rejected: confirmed via a
			# real, full-corpus build that this indents *every*
			# plain-continuation line while lcont_open, including the far
			# more common word-wrapped continuation of a \b bullet's own
			# text, which broke worse elsewhere (2 warnings -> 6, more
			# pages flagged). The "Note:"-after-sublist case is a real,
			# remaining imperfection (navigation.dox, 2 warnings) --
			# logged, not fixed; distinguishing the two cases needs more
			# than "are we inside an open \lcont".
			#
			# "or lcont_open" (2026-08-09): an \lcont{...} block can also
			# hold a plain paragraph directly, not just nested \b bullets
			# (e.g. intro.but.in's Working Directory page, the check-point
			# frequency paragraph). Without this, such a line didn't match
			# "in_list" (\lcont{ alone doesn't set in_list, only
			# lcont_open -- it's not itself a list item) and fell through
			# to the catch-all below, which calls close_list() and so
			# clears lcont_open before the block's own closing "}" line is
			# reached -- the "}" then had nothing left to recognize it and
			# leaked into the page as literal text (confirmed real: a
			# stray "<p>}</p>" on the rendered page, reported by a user
			# 2026-08-09, same failure shape as the cmdRaiseElev.html bug
			# this handler's sibling check already fixed once).
			if current is not None:
				current.body_lines.append(convert_inline(line, macros, source_file, line_no, current.anchor))
			continue

		# A genuinely new/unrecognized top-level directive ends the list.
		close_list()

		if unrec and unrec.group(1) not in known_inline_starts:
			log(source_file, line_no, current.anchor if current else '?',
			    'unrecognized-directive', f'\\{unrec.group(1)} at line start -- left as-is, needs manual review: {stripped[:70]}')

		if current is not None:
			current.body_lines.append(convert_inline(line, macros, source_file, line_no, current.anchor))

	close_list()
	return pages


def write_dox(pages, source_file, out_dir, keep_template=False,
              mainpage_anchor=None, extra_subpages=None):
	# Parent/child linking (p.parent -> the parent Page's .children list) is
	# done globally in main(), across ALL files' pages together, before
	# write_dox() is ever called -- not per-file here. Necessary since a
	# page's parent is often in a DIFFERENT file (see parse_but_file()'s
	# shared-stack comment: 11 of 20 files have their top heading parented
	# into addm.but's "Command Menus" chapter). by_anchor here is only for
	# the mainpage_anchor lookup below, which is always intro.but.in's own
	# page, always in THIS file's own pages when keep_template/mainpage_anchor
	# apply.
	by_anchor = {p.anchor: p for p in pages}

	# mainpage_anchor/extra_subpages (only ever set for intro.but.in's
	# "index" page, from main()'s cross-file pass): promotes that one page
	# to \mainpage (Doxygen's real "Main Page" tab target, and -- since
	# \mainpage's own implicit reference name is literally "index", which
	# is already this page's anchor -- a natural fit, not a renaming) and
	# appends every *other* file's own root page(s) as additional
	# \subpage entries, in halibut's real book order, after this page's
	# own natural children. This is what gives every other file's
	# top-level page a parent, fixing the alphabetical-sidebar bug -- see
	# CANONICAL_FILE_ORDER's comment.
	if mainpage_anchor and mainpage_anchor in by_anchor and extra_subpages:
		by_anchor[mainpage_anchor].children.extend(extra_subpages)

	# strip *both* extensions off a "<name>.but.in" source (os.path.splitext
	# only strips one, which would otherwise produce "intro.but.dox").
	stem = re.sub(r'\.but(\.in)?$', '', source_file)
	# keep_template (only ever true for intro.but.in, run without a
	# --version argument -- the intended default for the real one-time
	# bootstrap conversion, see main()): the output still contains the
	# literal "@XTRKCAD_VERSION@" token unresolved, so name it .dox.in, not
	# .dox, the same live-template signal intro.but.in itself already used.
	# This converter is meant to run ONCE and have its output committed as
	# the real, permanent source (see the plan file's "Important direction
	# correction" note) -- baking a real version string into that commit
	# would go stale on every future release. Keeping the template marker
	# means an ordinary, permanent configure_file() call in CMakeLists.txt
	# (see app/doc-doxygen-poc/CMakeLists.txt) resolves the version on
	# every build forever -- no Python involved at build time at all --
	# exactly like intro.but.in -> intro.but already does today for the
	# halibut build (app/doc/CMakeLists.txt).
	out_path = os.path.join(out_dir, stem + ('.dox.in' if keep_template else '.dox'))
	with open(out_path, 'w', encoding='utf-8') as f:
		f.write(f'/* Auto-converted from {source_file} by tools/but-to-doxygen.py.\n')
		f.write(' * See .claude/halibut-doxygen-investigation-plan.md and\n')
		f.write(' * conversion-log.md for anything flagged during conversion. */\n\n')
		for p in pages:
			f.write('/**\n')
			if p.anchor == mainpage_anchor:
				f.write(f'\\mainpage {p.title}\n\n')
			else:
				f.write(f'\\page {p.anchor} {p.title}\n\n')
			body = '\n'.join(p.body_lines).strip()
			# Collapse any run of 2+ consecutive blank lines down to one.
			# Semantically identical in Markdown (a single blank line
			# already fully separates paragraphs/list items; extra blank
			# lines never added visible spacing), but confirmed via a
			# real, bisected Doxygen build to matter for list-nesting
			# specifically: whenever two real blank body_lines entries
			# land back to back right before a \dt that follows nested
			# \dd/\lcont content -- several different combinations of
			# blank source lines and absorbed \u000/\rule spacers produce
			# this, not just one specific site -- Doxygen's list parser
			# treats the list as ended before reaching the nested
			# continuation ("Invalid list item found" on the *next* \dt).
			body = re.sub(r'\n{3,}', '\n\n', body)
			f.write(body + '\n')
			if p.children:
				f.write('\n')
				for child in p.children:
					f.write(f'- \\subpage {child}\n')
			f.write('\n*/\n\n')
	return out_path


def write_index_page(out_dir):
	"""Rebuild halibut's automatic back-of-book index as a real generated
	Doxygen page, from every \\i/\\I/\\ii occurrence collected during
	conversion. Groups occurrences by normalized display text (case- and
	whitespace-insensitive) so e.g. three separate \\i{Layout File} sites
	across different pages merge into one alphabetical entry with three
	\\ref links, same as a real index -- not one entry per occurrence."""
	groups = {}
	for text, idx_id, page_anchor in INDEX_ENTRIES:
		key = re.sub(r'\s+', ' ', text.strip()).lower()
		groups.setdefault(key, {'text': text, 'refs': []})
		groups[key]['refs'].append((idx_id, page_anchor))

	out_path = os.path.join(out_dir, 'index-generated.dox')
	with open(out_path, 'w', encoding='utf-8') as f:
		f.write('/* Auto-generated index page, rebuilt from every \\i/\\I/\\ii\n')
		f.write(' * occurrence across all converted pages -- see write_index_page()\n')
		f.write(' * in tools/but-to-doxygen.py. Reproduces halibut\'s automatic\n')
		f.write(' * back-of-book index instead of discarding the indexing semantic. */\n\n')
		f.write('/**\n\\page index-generated Index\n\n')
		for key in sorted(groups):
			entry = groups[key]
			refs = ', '.join(f'\\ref {idx_id} "{page_anchor}"' for idx_id, page_anchor in entry['refs'])
			f.write(f'- **{entry["text"]}** -- {refs}\n')
		f.write('\n*/\n')
	return out_path, len(groups), len(INDEX_ENTRIES)


def main():
	if len(sys.argv) not in (3, 4):
		print(f'Usage: {sys.argv[0]} <but-source-dir> <output-dir> [<version>]', file=sys.stderr)
		sys.exit(1)

	doc_dir, out_dir = sys.argv[1], sys.argv[2]
	# Optional, and NOT the intended default: resolves the literal
	# "@XTRKCAD_VERSION@" CMake template token that intro.but.in's
	# \XTCWinPack/\XTCRPMPack/\XTCStgzPack macros expand to (see
	# load_macros()) into a real version string right now, in this run's
	# output. This script is meant to run ONCE, with its output committed
	# as the real permanent source (see the plan file) -- a version baked
	# in at that one conversion moment would go stale on every later
	# release. Leave this unset for that real run: intro.but.in's page then
	# writes out as intro.dox.in (template preserved, see write_dox()).
	# This converter never touches CMake/Doxygen itself -- it's a one-time,
	# external text-conversion step (see app/doc-doxygen-poc/CMakeLists.txt
	# and release.yml's "Convert Halibut sources to Doxygen" step) -- so an
	# ordinary, permanent configure_file() call in CMakeLists.txt is what
	# actually resolves the version, on every build forever, with no
	# Python involved at build time at all. Only pass a version here for a
	# one-off fully-resolved dry run/demo.
	version = sys.argv[3] if len(sys.argv) == 4 else None
	images_dir = os.path.join(out_dir, 'images')
	os.makedirs(out_dir, exist_ok=True)
	os.makedirs(images_dir, exist_ok=True)

	macros = load_macros(doc_dir, version=version)

	found_files = set(f for f in os.listdir(doc_dir) if f.endswith('.but') or f.endswith('.but.in'))
	but_files = [f for f in CANONICAL_FILE_ORDER if f in found_files]
	unlisted = sorted(found_files - set(but_files))
	if unlisted:
		# A new .but file was added to app/doc/ without updating
		# CANONICAL_FILE_ORDER -- still convert it (append at the end,
		# alphabetically among itself) rather than silently dropping it
		# from the corpus, but flag it since its sidebar position will be
		# wrong until CANONICAL_FILE_ORDER is updated to match
		# app/doc/CMakeLists.txt's real target_sources order.
		log('(main)', 0, '?', 'unlisted-file',
		    f'found but not in CANONICAL_FILE_ORDER, appended at the end: {", ".join(unlisted)} -- update CANONICAL_FILE_ORDER to match app/doc/CMakeLists.txt')
		but_files += unlisted

	all_anchors = {}
	copied_images = set()

	# Pass 1: parse every file first (need every file's own root pages
	# before any file is written, to build intro's cross-file \mainpage
	# subpage list below). heading_stack is SHARED and mutated across every
	# file, in CANONICAL_FILE_ORDER (document order) -- see
	# parse_but_file()'s own docstring for why: 11 files continue a chapter
	# opened in an earlier file rather than starting their own.
	pages_by_file = {}
	heading_stack = []
	for fname in but_files:
		pages_by_file[fname] = parse_but_file(os.path.join(doc_dir, fname), macros, heading_stack)

	# Parent/child linking, globally across every file's pages together --
	# NOT per-file (see write_dox()'s own comment): a page's parent is
	# often in a different file now that heading_stack above threads
	# chapter continuation across file boundaries.
	all_pages_flat = [p for fname in but_files for p in pages_by_file[fname]]
	by_anchor_global = {p.anchor: p for p in all_pages_flat}
	for p in all_pages_flat:
		if p.parent and p.parent in by_anchor_global:
			by_anchor_global[p.parent].children.append(p.anchor)

	# Every file's own true root-level page(s) (p.parent is None -- thanks
	# to heading_stack above, this now correctly means "opens a new \C{}/
	# \A{} chapter", not just "first heading in this file"; the 11 files
	# that continue a prior file's chapter no longer qualify), in
	# CANONICAL_FILE_ORDER, each file's own roots in document order --
	# becomes intro.but.in's "index" page's \subpage list, on top of its
	# own existing children, so the whole corpus is one ordered tree
	# instead of a pile of alphabetically-sorted unparented roots (see
	# CANONICAL_FILE_ORDER's comment). intro.but.in's own root ("index"
	# itself) is excluded since it's about to become the parent.
	extra_subpages = []
	for fname in but_files:
		if fname == 'intro.but.in':
			continue
		extra_subpages += [p.anchor for p in pages_by_file[fname] if p.parent is None]
		# messages.but's real book position -- see MESSAGES_ROOT_ANCHOR's
		# comment. Spliced in unconditionally (not gated on the generated
		# messages.dox actually existing on disk): app/doc-doxygen-poc/
		# CMakeLists.txt's help-doxygen-full target always regenerates it
		# before building, so by the time Doxygen actually runs the
		# \subpage reference here resolves; if that generation step were
		# ever skipped, Doxygen's own "unable to resolve reference" warning
		# is the right signal, not a silent gap here.
		if fname == 'appendix.but':
			extra_subpages.append(MESSAGES_ROOT_ANCHOR)

	# Pass 2: write every file, now that pass 1 has everything needed.
	for fname in but_files:
		pages = pages_by_file[fname]
		path = os.path.join(doc_dir, fname)
		keep_template = fname.endswith('.but.in') and version is None
		is_intro = fname == 'intro.but.in'
		write_dox(pages, fname, out_dir, keep_template=keep_template,
		          mainpage_anchor='index' if is_intro else None,
		          extra_subpages=extra_subpages if is_intro else None)
		for p in pages:
			if p.anchor in all_anchors:
				log(fname, 0, p.anchor, 'duplicate-anchor',
				    f'anchor also defined in {all_anchors[p.anchor]} -- will collide')
			all_anchors[p.anchor] = fname

		# Copy referenced images.
		with open(path, encoding='utf-8') as f:
			content = f.read()
		for m in re.finditer(r'\\G\{([^}]*)\}', content):
			src = os.path.join(doc_dir, m.group(1))
			base = os.path.basename(m.group(1))
			if base not in copied_images and os.path.exists(src):
				shutil.copy(src, os.path.join(images_dir, base))
				copied_images.add(base)
			elif not os.path.exists(src):
				log(fname, 0, '?', 'missing-image', f'{m.group(1)} referenced but not found on disk')

	# Write the conversion log.
	log_path = os.path.join(out_dir, 'conversion-log.md')
	by_category = {}
	for entry in LOG:
		by_category.setdefault(entry.category, []).append(entry)

	with open(log_path, 'w', encoding='utf-8') as f:
		f.write('# but-to-doxygen.py conversion log\n\n')
		f.write(f'{len(but_files)} source files, {len(all_anchors)} pages generated, '
		        f'{len(LOG)} items flagged for review.\n\n')
		f.write('| Category | Count |\n|---|---|\n')
		for cat, entries in sorted(by_category.items(), key=lambda kv: -len(kv[1])):
			f.write(f'| {cat} | {len(entries)} |\n')
		f.write('\n')
		for cat, entries in sorted(by_category.items()):
			f.write(f'\n## {cat} ({len(entries)})\n\n')
			for e in entries:
				f.write(f'- `{e.source_file}:{e.line_no}` (\\anchor `{e.anchor}`) -- {e.detail}\n')

	index_path, index_entry_count, index_occurrence_count = write_index_page(out_dir)

	print(f'{len(but_files)} files converted, {len(all_anchors)} pages, '
	      f'{len(copied_images)} images copied, {len(LOG)} items logged.')
	print(f'Index: {index_entry_count} entries from {index_occurrence_count} occurrences -> {index_path}')
	print(f'Log: {log_path}')


if __name__ == '__main__':
	main()
