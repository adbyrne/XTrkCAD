#!/usr/bin/env python3
"""Convert halibut .but user-guide sources to Doxygen \\page syntax.

Prototype/investigation tool for the Halibut -> Doxygen user-guide migration
(see .claude/halibut-doxygen-investigation-plan.md, Phase 3+). Mechanically
converts the directive set validated in Phase 3's hand conversion
(\\H/\\S/\\S2/\\S3/\\A/\\C headings -> \\page, \\e/\\f/\\c/\\q inline formatting,
\\b/\\dd bullets, \\G images, \\K/\\k cross-references -> \\ref, \\W external
links) across every app/doc/*.but file in one pass, and -- just as
importantly -- LOGS every site it can't convert with full confidence
(\\dd/\\dt/\\lcont definition-list structure, \\i/\\I/\\ii index-registration
directives with no Doxygen equivalent, unrecognized directives) instead of
guessing silently. Not a full formal parser -- the format is regular enough
for line-oriented regex handling, verified against real content during
Phase 3.

Usage: tools/but-to-doxygen.py <but-source-dir> <output-dir>

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

LEVEL_ORDER = {'C': 0, 'H': 0, 'A': 0, 'S': 1, 'S2': 2, 'S3': 3}


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


def load_macros(doc_dir):
	macros = {}
	intro_in = os.path.join(doc_dir, 'intro.but.in')
	if os.path.exists(intro_in):
		with open(intro_in, encoding='utf-8') as f:
			for line in f:
				m = DEFINE_RE.match(line.strip())
				if m:
					macros[m.group(1)] = m.group(2).strip()
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
	text = re.sub(r'\\c\{([^}]*)\}', lambda m: f'`{m.group(1)}`', text)

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
	# has two brace groups.
	def w_repl(m):
		return f'[{m.group(2)}]({m.group(1)})'
	text = re.sub(r'\\W\{([^}]*)\}\{([^}]*)\}', w_repl, text)

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

	# \f{...} bold (XTrkCad's own added directive)
	text = re.sub(r'\\f\{([^}]*)\}', lambda m: f'**{m.group(1)}**', text)

	# \q{...} quoted UI-option text
	text = re.sub(r'\\q\{([^}]*)\}', lambda m: f'"{m.group(1)}"', text)

	# \e{...} emphasis -> italics. (Any literal "*" inside is already escaped
	# by the whole-text pass above, e.g. \e{*.xtc}.)
	def e_repl(m):
		return f'_{m.group(1)}_'
	text = re.sub(r'\\e\{([^}]*)\}', e_repl, text)

	# \i{...} visible italic + index registration. Keep the visible text AND
	# preserve the indexing semantic via a Doxygen \anchor right after it --
	# index-generated.dox (built at the end of the run) collects every one
	# of these into a real alphabetized index page, same as halibut's own
	# automatic index, instead of just discarding the registration.
	def i_repl(m):
		idx_id = make_index_anchor(m.group(1), anchor)
		log(source_file, line_no, anchor, 'index-term',
		    f'\\i{{{m.group(1)}}} -> italic + \\anchor {idx_id}, collected into index-generated.dox')
		if defer_anchors is not None:
			# Plain text only, no "_..._" emphasis markers -- confirmed via
			# a real build: markdown emphasis embedded directly in a \page
			# TITLE line (not just \anchor) corrupts emphasis parsing for
			# the rest of that page's body (real case: \S{enterValue}
			# \i{Entering Values} produced "\page enterValue _Entering
			# Values_", which threw off later, unrelated _..._ emphasis on
			# the same page with an "expecting command </em>" warning).
			defer_anchors.append(idx_id)
			return m.group(1)
		# Trailing space is required, not cosmetic: halibut's own source
		# often butts \i{...}/\I{...} directly against the next word with
		# no space (e.g. "\I{Run Trains}During this command..."). Without
		# it, "\anchor idx_..." swallows the following word as part of the
		# anchor name, and every \ref to that anchor then dangles
		# (confirmed via a real build: idx_Run_Trains_219 unresolved).
		return f'_{m.group(1)}_ \\anchor {idx_id} '
	text = re.sub(r'\\i\{([^}]*)\}', i_repl, text)

	# \ii{...} rare double-emphasis/index variant -- treat as bold, same
	# anchor-and-collect treatment.
	def ii_repl(m):
		idx_id = make_index_anchor(m.group(1), anchor)
		log(source_file, line_no, anchor, 'index-term',
		    f'\\ii{{{m.group(1)}}} -> bold + \\anchor {idx_id}, collected into index-generated.dox (rare directive, only 3 uses total)')
		if defer_anchors is not None:
			# Plain text only in a \page TITLE line -- same reasoning as
			# i_repl above.
			defer_anchors.append(idx_id)
			return m.group(1)
		return f'**{m.group(1)}** \\anchor {idx_id} '
	text = re.sub(r'\\ii\{([^}]*)\}', ii_repl, text)

	# \I{...} silent index-only registration -- no visible text in the
	# original either, so just the \anchor (no italic/bold wrapper),
	# collected the same way. Frequently appears stacked on a heading line
	# (e.g. "\S{cmdUndo} Undo and Redo \I{Undo} \I{Redo}") registering
	# multiple index aliases for one page -- confirmed via the real corpus,
	# this is the single biggest reason defer_anchors exists.
	def cap_i_repl(m):
		idx_id = make_index_anchor(m.group(1), anchor)
		log(source_file, line_no, anchor, 'index-term-silent',
		    f'\\I{{{m.group(1)}}} -> \\anchor {idx_id} only (matches original: no visible output), collected into index-generated.dox')
		if defer_anchors is not None:
			defer_anchors.append(idx_id)
			return ''
		return f'\\anchor {idx_id} '
	text = re.sub(r'\\I\{([^}]*)\}', cap_i_repl, text)

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


def parse_but_file(path, macros):
	"""Parse one .but file into a flat list of Page objects (document order),
	with parent/child relationships recorded for \\subpage linking."""
	source_file = os.path.basename(path)
	with open(path, encoding='utf-8') as f:
		lines = f.readlines()

	pages = []
	stack = []  # (level, anchor) -- current open ancestry
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
	in_list = False

	def close_list():
		nonlocal in_list
		in_list = False

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
			close_list()
			log(source_file, line_no, current.anchor if current else '?',
			    'lcont', 'list-continuation block -- flattened into surrounding text, needs manual review for correct nesting')
			# Strip the wrapper, keep inner content as best-effort.
			inner = stripped[len('\\lcont{'):]
			if inner.endswith('}'):
				inner = inner[:-1]
			if current is not None and inner.strip():
				current.body_lines.append(
				    convert_inline(inner, macros, source_file, line_no, current.anchor))
			continue

		if stripped.startswith('\\dd'):
			body = stripped[3:].strip()
			if current is not None:
				in_list = True
				current.body_lines.append('- ' + convert_inline(body, macros, source_file, line_no, current.anchor))
			continue

		if stripped.startswith('\\dt'):
			body = stripped[3:].strip()
			if current is not None:
				in_list = True
				current.body_lines.append('- **' + convert_inline(body, macros, source_file, line_no, current.anchor) + '**')
			continue

		if stripped.startswith('\\b'):
			close_list()
			body = stripped[2:].strip()
			if current is not None:
				current.body_lines.append('- ' + convert_inline(body, macros, source_file, line_no, current.anchor))
			continue

		if stripped.startswith('\\n'):
			if in_list:
				# \n mid-list is just a fresh paragraph within the same
				# item in this corpus (word-wrapped source), not a new item
				# -- indent it so Markdown treats it as part of the
				# preceding list item's own paragraph rather than ending
				# the list.
				body = stripped[2:].strip()
				if current is not None:
					current.body_lines.append('  ' + convert_inline(body, macros, source_file, line_no, current.anchor))
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

		if in_list and is_plain_continuation:
			# Word-wrapped continuation of the paragraph a \dd/\dt started
			# (the common case: real prose in this corpus wraps across
			# several physical lines with no per-line directive) -- indent
			# so Markdown folds it into the same list item instead of
			# ending the list.
			if current is not None:
				current.body_lines.append('  ' + convert_inline(line, macros, source_file, line_no, current.anchor))
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


def write_dox(pages, source_file, out_dir):
	by_anchor = {p.anchor: p for p in pages}
	for p in pages:
		if p.parent and p.parent in by_anchor:
			by_anchor[p.parent].children.append(p.anchor)

	out_path = os.path.join(out_dir, os.path.splitext(source_file)[0] + '.dox')
	with open(out_path, 'w', encoding='utf-8') as f:
		f.write(f'/* Auto-converted from {source_file} by tools/but-to-doxygen.py.\n')
		f.write(' * See .claude/halibut-doxygen-investigation-plan.md and\n')
		f.write(' * conversion-log.md for anything flagged during conversion. */\n\n')
		for p in pages:
			f.write('/**\n')
			f.write(f'\\page {p.anchor} {p.title}\n\n')
			body = '\n'.join(p.body_lines).strip()
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
	if len(sys.argv) != 3:
		print(f'Usage: {sys.argv[0]} <but-source-dir> <output-dir>', file=sys.stderr)
		sys.exit(1)

	doc_dir, out_dir = sys.argv[1], sys.argv[2]
	images_dir = os.path.join(out_dir, 'images')
	os.makedirs(out_dir, exist_ok=True)
	os.makedirs(images_dir, exist_ok=True)

	macros = load_macros(doc_dir)

	but_files = sorted(f for f in os.listdir(doc_dir) if f.endswith('.but'))
	all_anchors = {}
	copied_images = set()

	for fname in but_files:
		path = os.path.join(doc_dir, fname)
		pages = parse_but_file(path, macros)
		write_dox(pages, fname, out_dir)
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
