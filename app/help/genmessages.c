/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis
 *						2007 Martin Fischer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef WINDOWS
#if _MSC_VER >=1400
#define strdup _strdup
#endif
#endif

#define I18NHEADERFILE "i18n.h"

typedef struct helpMsg_t * helpMsg_p;
typedef struct helpMsg_t {
	char * key;
	char * title;
	char * help;
} helpMsg_t;

helpMsg_t helpMsgs[200];
int helpMsgCnt = 0;

struct transTbl {
	char *inChar;
	char *outChar[];
};

/* ATTENTION: make sure that the characters are in the same order as the equivalent escape sequences below */

/* translation table for escape sequences understood by C compiler */

struct transTbl toC = {
	"\n\\\"\0",
	{
		"\\n",
		"\\\\",
		"\\\"",
		"\\0"
	}
};

/* escape a literal backtick before \c{} below adds real ones -- see
 * ConvertToDoxygen()'s ordering comment */
struct transTbl toBacktickEscape = {
	"`",
	{
		"\\`"
	}
};

/* escape literal '*' and '<'/'>' in prose so Doxygen's Markdown/HTML parser
 * doesn't misread them as emphasis markers or tags -- run only AFTER \c{}
 * has already produced its own real backticks/content, see
 * ConvertToDoxygen() */
struct transTbl toAsteriskAngleEscape = {
	"*<>",
	{
		"\\*",
		"&lt;",
		"&gt;"
	}
};


char *
TranslateString(char *srcString, struct transTbl *trTbl)
{
	char *destString;
	char *cp;
	size_t bufLen = strlen(srcString) + 1;
	char *idx;

	/* calculate the expected result length */
	for (cp = srcString; *cp; cp++) {
		idx = strchr(trTbl->inChar, *cp);

		if (idx) {       			/* does character need translation ? */
			bufLen += strlen((trTbl->outChar)[idx - trTbl->inChar]) -
			          1;    /* yes, extend buffer accordingly */
		}
	}

	/* allocate memory for result */
	destString = malloc(bufLen);

	if (destString) {
		char *cp2;
		/* copy and translate characters as needed */
		cp2 = destString;

		for (cp = srcString; *cp; cp++) {
			idx = strchr(trTbl->inChar, *cp);

			if (idx != NULL) {        /* does character need translation ? */
				strcpy(cp2, (trTbl->outChar)[idx -
				                             trTbl->inChar ]);     /* yes, copy the escaped character sequence */
				cp2 += strlen((trTbl->outChar)[idx - trTbl->inChar ]);
			} else {
				*cp2++ = *cp;                       /* no, just copy the character */
			}
		}

		/* terminate string */
		*cp2 = '\0';
	} else {
		/* memory allocation failed */
		exit(1);
	}

	return (destString);
}


/* minimal growable string buffer, used only by ReplaceBraced/ReplaceW below */
typedef struct {
	char *buf;
	size_t len;
	size_t cap;
} DBuf;

static void
DBufInit(DBuf *d)
{
	d->cap = 256;
	d->buf = malloc(d->cap);
	d->buf[0] = '\0';
	d->len = 0;
}

static void
DBufAppendN(DBuf *d, const char *s, size_t n)
{
	if (d->len + n + 1 > d->cap) {
		while (d->len + n + 1 > d->cap) {
			d->cap *= 2;
		}

		d->buf = realloc(d->buf, d->cap);
	}

	memcpy(d->buf + d->len, s, n);
	d->len += n;
	d->buf[d->len] = '\0';
}

static void
DBufAppend(DBuf *d, const char *s)
{
	DBufAppendN(d, s, strlen(s));
}

/**
 * Given cp pointing at an opening '{', return the matching closing '}',
 * counting nested {...} pairs -- e.g. messages.in's
 * "\W{url}{\e{XTrackCAD} Website}" nests \e{}'s own braces inside \W{}'s
 * second argument. Returns NULL if unbalanced.
 */
static const char *
FindMatchingBrace(const char *cp)
{
	int depth = 1;
	cp++;

	while (*cp) {
		if (*cp == '{') {
			depth++;
		} else if (*cp == '}') {
			depth--;

			if (depth == 0) {
				return cp;
			}
		}

		cp++;
	}

	return NULL;
}

/**
 * Replace every "\<cmd>{inner}" in src with prefix+inner+suffix. Handles
 * one level of brace nesting inside inner (see FindMatchingBrace) --
 * confirmed necessary for \W{}'s second argument, and applied here too for
 * consistency/robustness even though no bare \c/\K/\k/\f/\q/\e{} in the
 * current messages.in corpus nests.
 *
 * escapeQuotes: when true, every literal '"' in inner is escaped to
 * "&quot;" before being appended -- required when prefix/suffix are real
 * HTML tags (<b>/<i>, used for \f{}/\e{}). Confirmed via an isolated
 * Doxygen build (2026-08-09, same finding as tools/but-to-doxygen.py's
 * escape_quote_in_tag()) that a literal '"' between an opening and closing
 * HTML tag makes Doxygen's parser think it's inside a quoted attribute
 * value and keep scanning PAST the real closing tag for the next '"' --
 * real cross-content corruption, not just a warning. Not needed for
 * \c/\K/\k (no real HTML tags involved) or \q{} (its own prefix/suffix
 * ARE literal quote characters, by design).
 *
 * Returns a newly malloc'd string; caller frees.
 */
char *
ReplaceBraced(const char *src, const char *cmd, const char *prefix,
              const char *suffix, int escapeQuotes)
{
	size_t cmdLen = strlen(cmd);
	const char *cp = src;
	DBuf d;
	DBufInit(&d);

	while (*cp) {
		if (cp[0] == '\\' && strncmp(cp + 1, cmd, cmdLen) == 0 &&
		    cp[1 + cmdLen] == '{') {
			const char *openBrace = cp + 1 + cmdLen;
			const char *closeBrace = FindMatchingBrace(openBrace);

			if (closeBrace) {
				const char *innerStart = openBrace + 1;
				DBufAppend(&d, prefix);

				if (escapeQuotes) {
					const char *ip;

					for (ip = innerStart; ip < closeBrace; ip++) {
						if (*ip == '"') {
							DBufAppend(&d, "&quot;");
						} else {
							DBufAppendN(&d, ip, 1);
						}
					}
				} else {
					DBufAppendN(&d, innerStart, closeBrace - innerStart);
				}

				DBufAppend(&d, suffix);
				cp = closeBrace + 1;
				continue;
			}
		}

		DBufAppendN(&d, cp, 1);
		cp++;
	}

	return d.buf;
}

/**
 * Replace every "\W{url}{text}" external-link directive with Doxygen/
 * Markdown's "[text](url)". text commonly nests another macro's own braces
 * (e.g. "\W{url}{\e{XTrackCAD} Website}", 7 occurrences in messages.in) --
 * see FindMatchingBrace.
 */
char *
ReplaceW(const char *src)
{
	const char *cp = src;
	DBuf d;
	DBufInit(&d);

	while (*cp) {
		if (cp[0] == '\\' && cp[1] == 'W' && cp[2] == '{') {
			const char *urlOpen = cp + 2;
			const char *urlClose = FindMatchingBrace(urlOpen);

			if (urlClose && urlClose[1] == '{') {
				const char *textOpen = urlClose + 1;
				const char *textClose = FindMatchingBrace(textOpen);

				if (textClose) {
					const char *urlStart = urlOpen + 1;
					const char *textStart = textOpen + 1;
					DBufAppendN(&d, "[", 1);
					DBufAppendN(&d, textStart, textClose - textStart);
					DBufAppend(&d, "](");
					DBufAppendN(&d, urlStart, urlClose - urlStart);
					DBufAppendN(&d, ")", 1);
					cp = textClose + 1;
					continue;
				}
			}
		}

		DBufAppendN(&d, cp, 1);
		cp++;
	}

	return d.buf;
}

/**
 * Append the UTF-8 encoding of one Unicode codepoint to d.
 */
static void
AppendUtf8Codepoint(DBuf *d, long codepoint)
{
	unsigned char bytes[4];
	int n = 0;

	if (codepoint <= 0x7F) {
		bytes[n++] = (unsigned char)codepoint;
	} else if (codepoint <= 0x7FF) {
		bytes[n++] = (unsigned char)(0xC0 | (codepoint >> 6));
		bytes[n++] = (unsigned char)(0x80 | (codepoint & 0x3F));
	} else if (codepoint <= 0xFFFF) {
		bytes[n++] = (unsigned char)(0xE0 | (codepoint >> 12));
		bytes[n++] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3F));
		bytes[n++] = (unsigned char)(0x80 | (codepoint & 0x3F));
	} else {
		bytes[n++] = (unsigned char)(0xF0 | (codepoint >> 18));
		bytes[n++] = (unsigned char)(0x80 | ((codepoint >> 12) & 0x3F));
		bytes[n++] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3F));
		bytes[n++] = (unsigned char)(0x80 | (codepoint & 0x3F));
	}

	DBufAppendN(d, (const char *)bytes, (size_t)n);
}

/**
 * halibut's \uXXXX unicode-codepoint escape, used directly as literal text
 * in messages.in's source (not raw bytes) for characters like the degree
 * sign (e.g. "0 degrees and 360 degrees"). Codepoints below 0x20 are XTrkCad's
 * own blank-line spacer shorthand (\u000, \u00) with no visible output;
 * anything else becomes the real UTF-8 character. NOT an HTML numeric
 * entity (&#176; etc.) -- confirmed via an isolated Doxygen build
 * (2026-08-09) that Doxygen HTML-escapes the leading "&" of any "&#NNN;"
 * reference rather than decoding it, in every context (plain prose, list
 * items, \c{} code spans), so it always shows up as literal "&#176;" text
 * in the browser instead of the character -- exactly the real bug reported
 * against the degree signs in Angle/Radius/Connection-parameter message
 * pages. A real UTF-8 character renders correctly in all the same
 * contexts and was verified not to trigger the unrelated Doxygen list-
 * parser corruption bug that HTML-entity-escaping braces originally
 * guarded against (see tools/but-to-doxygen.py's matching fix and
 * comment) -- that bug is specific to Doxygen's OWN "\{"/"\}" escape
 * syntax, not to real characters in general.
 * Mirrors tools/but-to-doxygen.py's u_repl().
 */
char *
ReplaceUnicodeEscapes(const char *src)
{
	const char *cp = src;
	DBuf d;
	DBufInit(&d);

	while (*cp) {
		if (cp[0] == '\\' && cp[1] == 'u' && isxdigit((unsigned char)cp[2])) {
			const char *hexStart = cp + 2;
			const char *hexEnd = hexStart;
			char hexBuf[16];
			size_t hexLen;
			long codepoint;

			while (isxdigit((unsigned char)*hexEnd)) {
				hexEnd++;
			}

			hexLen = (size_t)(hexEnd - hexStart);

			if (hexLen >= sizeof hexBuf) {
				hexLen = sizeof hexBuf - 1;
			}

			memcpy(hexBuf, hexStart, hexLen);
			hexBuf[hexLen] = '\0';
			codepoint = strtol(hexBuf, NULL, 16);

			if (codepoint >= 0x20) {
				AppendUtf8Codepoint(&d, codepoint);
			}

			cp = hexEnd;
			continue;
		}

		DBufAppendN(&d, cp, 1);
		cp++;
	}

	return d.buf;
}

/**
 * halibut's \b bullet-list-item marker, always at the start of its own
 * line in messages.in (no \lcont/nesting in this corpus, unlike the wider
 * .but corpus tools/but-to-doxygen.py has to handle) -- convert to a
 * Markdown "- " bullet.
 */
char *
ReplaceBulletLines(const char *src)
{
	const char *cp = src;
	DBuf d;
	DBufInit(&d);
	int atLineStart = 1;

	while (*cp) {
		if (atLineStart && cp[0] == '\\' && cp[1] == 'b' && cp[2] == ' ') {
			DBufAppend(&d, "- ");
			cp += 3;
			atLineStart = 0;
			continue;
		}

		atLineStart = (*cp == '\n');
		DBufAppendN(&d, cp, 1);
		cp++;
	}

	return d.buf;
}

/**
 * Convert the subset of halibut markup actually used in messages.in (\b,
 * \uXXXX, \c, \W, \K/\k, \f, \q, \e, plus escaping of literal backtick,
 * asterisk, and angle brackets) to Doxygen/Markdown syntax. Ordering
 * mirrors tools/but-to-doxygen.py's convert_inline(): backtick escape and
 * \c{} must run before the asterisk/angle-bracket escape, or \c{}'s own
 * generated content would get double-escaped.
 */
char *
ConvertToDoxygen(char *srcString)
{
	char *s0, *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8, *s9, *result;

	s0 = ReplaceUnicodeEscapes(srcString);
	s1 = ReplaceBulletLines(s0);
	free(s0);
	s2 = TranslateString(s1, &toBacktickEscape);
	free(s1);
	s3 = ReplaceBraced(s2, "c", "`", "`", 0);
	free(s2);
	s4 = TranslateString(s3, &toAsteriskAngleEscape);
	free(s3);
	s5 = ReplaceW(s4);
	free(s4);
	s6 = ReplaceBraced(s5, "K", "\\ref ", "", 0);
	free(s5);
	s7 = ReplaceBraced(s6, "k", "\\ref ", "", 0);
	free(s6);
	s8 = ReplaceBraced(s7, "f", "<b>", "</b>", 1);
	free(s7);
	s9 = ReplaceBraced(s8, "q", "\"", "\"", 0);
	free(s8);
	result = ReplaceBraced(s9, "e", "<i>", "</i>", 1);
	free(s9);

	return result;
}


int cmpHelpMsg(const void * a, const void * b)
{
	helpMsg_p aa = (helpMsg_p)a;
	helpMsg_p bb = (helpMsg_p)b;
	return strcmp(aa->title, bb->title);
}

void unescapeString(FILE * f, char * str)
{
	while (*str) {
		if (*str != '\\') {
			fputc(*str, f);
		}

		str++;
	}
}

/**
 * Generate the help file in Doxygen syntax -- see SF feature-requests #219 /
 * .claude/halibut-doxygen-investigation-plan.md. Emits one \page block per
 * message under a "messageList" root page (the fixed anchor id
 * tools/but-to-doxygen.py's CANONICAL_FILE_ORDER splices into the sidebar
 * between appendix.but and upgrade.but). Halibut's \S{key}/\H{} output is
 * gone -- this branch is retiring halibut, not running the two side by
 * side (the still-halibut-based app/doc/CMakeLists.txt production build is
 * a separate, not-yet-updated step).
 */

void dumpHelp(FILE *hlpsrcF)
{
	int inx;

	fputs("/* DO NOT EDIT! This file has been automatically created by genmessages.\n * Changes to this file will be overwritten. */\n\n",
	      hlpsrcF);
	/* sort in alphabetical order */
	qsort(helpMsgs, helpMsgCnt, sizeof helpMsgs[0], cmpHelpMsg);

	fprintf(hlpsrcF, "/**\n\\page messageList Message Explanations\n\n");

	for (inx=0; inx<helpMsgCnt; inx++) {
		fprintf(hlpsrcF, "- \\subpage %s\n", helpMsgs[inx].key);
	}

	fprintf(hlpsrcF, "*/\n\n");

	/* now save all the help messages */
	for (inx=0; inx<helpMsgCnt; inx++) {
		char *docStr;

		docStr = ConvertToDoxygen(helpMsgs[inx].title);
		fprintf(hlpsrcF, "/**\n\\page %s %s\n\n", helpMsgs[inx].key, docStr);
		free(docStr);

		docStr = ConvertToDoxygen(helpMsgs[inx].help);
		fprintf(hlpsrcF, "%s\n*/\n\n", docStr);
		free(docStr);
	}
}


int main(int argc, char * argv[])
{
	FILE * hdrF;
	FILE *inF;
	FILE *outF;
	char buff[ 4096 ];
	char * cp;
	int inFileIdx = 1;
	enum {m_init, m_title, m_alt, m_help } mode = m_init;
	char msgName[256];
	char msgAlt[256];
	char msgTitle[1024];
	char msgHelp[4096];
	char *tName, *tAlt, *tTitle;

	int i18n = 0;

	/* check argument count */
	if (argc < 3 || argc > 4) {
		fprintf(stderr, "Usage: %s [-i18n] INFILE OUTFILE\n\n", argv[0]);
		fprintf(stderr,
		        "       -i18n is used to generate a include file with gettext support.\n\n");
		exit(1);
	}

	/* check options */
	if (argc == 4) {
		if (!strcmp(argv[ 1 ], "-i18n")) {
			i18n = 1;
			inFileIdx = 2;	/* second argument is input file */
		}

		/* inFileIdx = 2;  skip over option argument */
	} else {
		inFileIdx = 1;	/* first argument is input file */
	}

	/* open the file for reading */
	inF = fopen(argv[ inFileIdx ], "r");

	if (!inF) {
		fprintf(stderr, "Could not open %s for reading!\n", argv[ inFileIdx ]);
		exit(1);
	}

	/* open the include file to generate */
	hdrF = fopen("messages.h", "w");

	if (!hdrF) {
		fprintf(stderr, "Could not open messages.h for writing!\n");
		exit(1);
	}

	fputs("/*\n * DO NOT EDIT! This file has been automatically created by genmessages.\n * Changes to this file will be overwritten.\n */\n",
	      hdrF);
	fputs("#ifndef HAVE_MESSAGES_H\n#define HAVE_MESSAGES_H\n", hdrF);
	/* open the help file to generate */
	outF = fopen(argv[ inFileIdx + 1 ], "w");

	if (!inF) {
		fprintf(stderr, "Could not open %s for writing!\n", argv[ inFileIdx ]);
		exit(1);
	}

	/* Include i18n header, if needed */
	if (i18n) {
		fprintf(hdrF, "#include \"" I18NHEADERFILE "\"\n\n");
	}

	while (fgets(buff, sizeof buff, inF)) {
		/* skip comment lines */
		if (buff[0] == '#') {
			continue;
		}

		/* remove trailing whitespaces */
		cp = buff+strlen(buff)-1;

		while (cp >= buff && isspace(*cp)) {
			*cp = '\0';
			cp--;
		}

		if (strncmp(buff, "MESSAGE ", 8) == 0) {
			/* skip any spaces */
			cp = strchr(buff+8, ' ');

			if (cp)
				while (*cp == ' ') {
					*cp++ = 0;
				}

			/* save the name of the message */
			strcpy(msgName, buff + 8);
			msgAlt[0] = 0;
			msgTitle[0] = 0;
			msgHelp[0] = 0;
			mode = m_title;
		} else if (strncmp(buff, "ALT", 3) == 0) {
			mode = m_alt;
			msgAlt[0] = 0;
		} else if (strncmp(buff, "HELP", 4) == 0) {
			mode = m_help;
		} else if (strncmp(buff, "END", 3) == 0) {
			/* the whole message has been read */
			/* create escape sequences */
			tName = TranslateString(msgName, &toC);
			tTitle = TranslateString(msgTitle, &toC);
			tAlt = TranslateString(msgAlt, &toC);

			if (msgHelp[0]==0) {
				/* no help text is included */
				if (i18n) {
					fprintf(hdrF, "#define %s N_(\"%s\")\n", tName, tTitle);
				} else {
					fprintf(hdrF, "#define %s \"%s\"\n", tName, tTitle);
				}
			} else if (msgAlt[0]) {
				/* a help text and an alternate description are included */
				if (i18n) {
					fprintf(hdrF, "#define %s N_(\"%s\\t%s\\t%s\")\n", tName, tName, tAlt, tTitle);
				} else {
					fprintf(hdrF, "#define %s \"%s\\t%s\\t%s\"\n", tName, tName, tAlt, tTitle);
				}
			} else {
				/* a help text but no alternate description are included */
				if (i18n) {
					fprintf(hdrF, "#define %s N_(\"%s\\t%s\")\n", tName, tName, tTitle);
				} else {
					fprintf(hdrF, "#define %s \"%s\\t%s\"\n", tName, tName, tTitle);
				}
			}

			/*free temp stzrings */
			free(tName);
			free(tTitle);
			free(tAlt);

			/* save the help text for later use */
			if (msgHelp[0]) {
				helpMsgs[helpMsgCnt].key = strdup(msgName);

				if (msgAlt[0]) {
					helpMsgs[helpMsgCnt].title = strdup(msgAlt);
				} else {
					helpMsgs[helpMsgCnt].title = strdup(msgTitle);
				}

				helpMsgs[helpMsgCnt].help = strdup(msgHelp);
				helpMsgCnt++;
			}

			mode = 0;
		} else {
			/* are we currently reading the message text? */
			if (mode == m_title) {
				/* yes, is the message text split over two lines ? */
				if (msgTitle[0]) {
					/* if yes, keep the first part as the short text */
					if (msgAlt[0] == 0) {
						strcpy(msgAlt, msgTitle);
						strcat(msgAlt, "...");
					}

					/* add a newline to the first part */
					strcat(msgTitle, "\n");
				}

				/* now save the buffer into the message title */
				strcat(msgTitle, buff);
			} else if (mode == m_alt) {
				/* an alternate text was explicitly specified, save */
				if (msgAlt[ 0 ]) {
					strcat(msgAlt, " ");
					strcat(msgAlt, buff);
				} else {
					strcpy(msgAlt, buff);
				}
			} else if (mode == m_help) {
				/* we are reading the help text, save in buffer */
				strcat(msgHelp, buff);
				strcat(msgHelp, "\n");
			}
		}
	}

	dumpHelp(outF);
	fputs("#endif // HAVE_MESSAGES_H\n", hdrF);
	fclose(hdrF);
	fclose(inF);
	fclose(outF);
	printf("%d messages\n", helpMsgCnt);
	return 0;
}
