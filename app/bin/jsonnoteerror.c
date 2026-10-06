/** \file jsonnoteerror.c
 * Locate and classify a JSON Note parse failure -- see jsonnoteerror.h.
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2026 XTrkCAD contributors
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

#include <string.h>

#include "cJSON.h"
#include "include/jsonnoteerror.h"

/** TRUE for a UTF-8 continuation byte (10xxxxxx). */
#define IS_UTF8_CONT(b) (((unsigned char)(b) & 0xC0) == 0x80)

/**
 * Classify the character starting at \p p, if it's one of the invisible or
 * look-alike characters that commonly break hand-typed or pasted JSON.
 *
 * \param p IN start of a character (NUL-terminated text)
 * \return the cause, or JSONNOTEERR_GENERIC if \p p is nothing special
 */
static jsonNoteErrorCause_e
ClassifyChar(const char *p)
{
	const unsigned char *u = (const unsigned char *)p;

	/* U+2018..U+201F: single/double curly, low-9 and reversed quotes */
	if (u[0] == 0xE2 && u[1] == 0x80 && u[2] >= 0x98 && u[2] <= 0x9F) {
		return JSONNOTEERR_TYPOGRAPHIC_QUOTE;
	}
	/* U+00A0 no-break space */
	if (u[0] == 0xC2 && u[1] == 0xA0) {
		return JSONNOTEERR_NONBREAKING_SPACE;
	}
	return JSONNOTEERR_GENERIC;
}

/**
 * Check whether \p text leaves an object, array, or string open at its end
 * -- i.e. whether it genuinely stops before the JSON is complete.
 *
 * \param text IN the text
 * \param len IN its length
 * \return TRUE if something is still open at the end
 */
static int
EndsInsideSomething(const char *text, size_t len)
{
	int depth = 0;
	int inString = 0;

	for (size_t i = 0; i < len; i++) {
		char c = text[i];
		if (inString) {
			if (c == '\\' && i + 1 < len) {
				i++;
			} else if (c == '"') {
				inString = 0;
			}
		} else if (c == '"') {
			inString = 1;
		} else if (c == '{' || c == '[') {
			depth++;
		} else if (c == '}' || c == ']') {
			depth--;
		}
	}
	return inString || depth > 0;
}

/** What the parser expects next inside the innermost open container. */
typedef enum {
	EXPECT_VALUE,		/**< a value (top level, after ':' or '[' or ',' in an array) */
	EXPECT_KEY,		/**< an object key (after '{' or ',' in an object) */
	EXPECT_COLON,		/**< ':' after an object key */
	EXPECT_COMMA_OR_CLOSE	/**< ',' or the closing bracket, after a complete value */
} jsonExpect_e;

#define JSON_SCAN_MAX_DEPTH 64

/** Result of scanning up to the error position. */
typedef struct {
	int valid;		/**< FALSE if the scan couldn't follow the text */
	jsonExpect_e expect;	/**< expectation at the error position */
	int afterComma;		/**< the last token before the position was ',' */
	size_t lastTokenEnd;	/**< offset just past the last complete token */
	size_t lastComma;	/**< offset of that ',' when afterComma */
	int depth;		/**< nesting depth at the error position */
} jsonScan_t;

/**
 * Follow the JSON structure from the start of \p text up to \p pos, closely
 * enough to say what the parser expected at \p pos. This only has to track
 * nesting and the key/colon/value/comma rhythm, not validate anything.
 *
 * \param text IN the text
 * \param pos IN offset to scan up to
 * \param scan OUT the result
 */
static void
ScanTo(const char *text, size_t pos, jsonScan_t *scan)
{
	char stack[JSON_SCAN_MAX_DEPTH];	/* '{' or '[' */
	jsonExpect_e expect[JSON_SCAN_MAX_DEPTH + 1];
	int depth = 0;
	size_t i = 0;

	memset(scan, 0, sizeof *scan);
	expect[0] = EXPECT_VALUE;

	while (i < pos) {
		char c = text[i];
		if ((unsigned char)c <= ' ') {
			i++;
			continue;
		}
		scan->afterComma = 0;
		if (c == '"') {
			i++;
			while (i < pos && text[i] != '"') {
				i += (text[i] == '\\' && i + 1 < pos) ? 2 : 1;
			}
			if (i >= pos) {
				return;		/* position is inside a string */
			}
			i++;
			expect[depth] = (expect[depth] == EXPECT_KEY) ? EXPECT_COLON
			                : EXPECT_COMMA_OR_CLOSE;
		} else if (c == '{' || c == '[') {
			if (depth >= JSON_SCAN_MAX_DEPTH) {
				return;
			}
			stack[depth++] = c;
			expect[depth] = (c == '{') ? EXPECT_KEY : EXPECT_VALUE;
			i++;
		} else if (c == '}' || c == ']') {
			if (depth == 0) {
				return;
			}
			depth--;
			expect[depth] = EXPECT_COMMA_OR_CLOSE;
			i++;
		} else if (c == ':') {
			expect[depth] = EXPECT_VALUE;
			i++;
		} else if (c == ',') {
			expect[depth] = (depth > 0 && stack[depth - 1] == '{') ? EXPECT_KEY
			                : EXPECT_VALUE;
			scan->afterComma = 1;
			scan->lastComma = i;
			i++;
		} else {
			/* number or true/false/null: consume the whole word */
			size_t wordStart = i;
			while (i < pos && ((text[i] >= '0' && text[i] <= '9')
			                   || (text[i] >= 'a' && text[i] <= 'z')
			                   || (text[i] >= 'A' && text[i] <= 'Z')
			                   || text[i] == '.' || text[i] == '+' || text[i] == '-')) {
				i++;
			}
			if (i == wordStart) {
				return;		/* not a character this scan understands */
			}
			expect[depth] = EXPECT_COMMA_OR_CLOSE;
		}
		if (!scan->afterComma) {
			scan->lastTokenEnd = i;
		}
	}
	scan->valid = 1;
	scan->depth = depth;
	scan->expect = expect[depth];
}

/** TRUE for a character that can be part of a bare word (key or literal). */
static int
IsWordChar(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
	       || (c >= '0' && c <= '9') || c == '_';
}

/** TRUE if the bare word at \p p is a literal from another language that
 * people write in place of JSON's true/false/null. */
static int
IsWrongLiteral(const char *p)
{
	static const char *const words[] = {
		"True", "TRUE", "False", "FALSE", "None", "NULL", "Null", "nil",
		"undefined", "NaN", "Infinity",
	};
	size_t n = 0;
	while (IsWordChar(p[n])) {
		n++;
	}
	for (size_t i = 0; i < sizeof words / sizeof words[0]; i++) {
		if (strlen(words[i]) == n && strncmp(p, words[i], n) == 0) {
			return 1;
		}
	}
	return 0;
}

/** TRUE if \p c can start a JSON value. */
static int
StartsValue(char c)
{
	return c == '"' || c == '{' || c == '[' || c == '-'
	       || (c >= '0' && c <= '9') || c == 't' || c == 'f' || c == 'n';
}

/**
 * Convert a byte offset to a 1-based line and character column.
 *
 * \param text IN the text
 * \param pos IN byte offset
 * \param line OUT line
 * \param column OUT column, counting characters rather than bytes
 */
static void
LineColumn(const char *text, size_t pos, int *line, int *column)
{
	*line = 1;
	*column = 1;
	for (size_t i = 0; i < pos; i++) {
		if (text[i] == '\n') {
			(*line)++;
			*column = 1;
		} else if (!IS_UTF8_CONT(text[i])) {
			(*column)++;
		}
	}
}

void
JsonNoteLocateError(const char *text, const char *errPtr,
                    jsonNoteErrorInfo_t *info)
{
	size_t len = strlen(text);
	size_t pos = 0;
	int endedEarly = 0;

	if (errPtr != NULL && errPtr >= text && errPtr <= text + len) {
		pos = (size_t)(errPtr - text);
	}

	/* cJSON's error position is on the offending character or just past
	 * its first byte, so back up to the start of a multi-byte character. */
	while (pos > 0 && pos < len && IS_UTF8_CONT(text[pos])) {
		pos--;
	}
	/* ...and inside a bare word (an unquoted key) it can stop one character
	 * in, so back up to the start of the word. */
	while (pos > 0 && pos < len && IsWordChar(text[pos])
	       && IsWordChar(text[pos - 1])) {
		pos--;
	}

	/* cJSON also reports end-of-text for both an unterminated value and,
	 * e.g., a trailing comma before the final '}'. Only the first really
	 * ended early; otherwise point at the last real character. */
	if (pos >= len) {
		endedEarly = EndsInsideSomething(text, len);
		if (!endedEarly) {
			size_t last = len;
			while (last > 0 && (unsigned char)text[last - 1] <= ' ') {
				last--;
			}
			if (last > 0) {
				pos = last - 1;
				while (pos > 0 && IS_UTF8_CONT(text[pos])) {
					pos--;
				}
			}
		}
	}

	/* Cause: empty text first, then the character at the error position,
	 * then anywhere in the text -- a curly quote that cJSON swallowed as
	 * part of a string makes it stop somewhere else entirely, so in that
	 * case report the culprit's own position instead. */
	size_t firstNonBlank = 0;
	while (firstNonBlank < len && (unsigned char)text[firstNonBlank] <= ' ') {
		firstNonBlank++;
	}
	size_t hint = pos;
	jsonScan_t scan;
	if (firstNonBlank < len && pos < len) {
		ScanTo(text, pos, &scan);
	} else {
		scan.valid = 0;
	}

	if (firstNonBlank == len) {
		info->cause = JSONNOTEERR_EMPTY;
	} else if (pos < len && ClassifyChar(text + pos) != JSONNOTEERR_GENERIC) {
		info->cause = ClassifyChar(text + pos);
	} else if (pos < len && text[pos] == '/' && (text[pos + 1] == '/'
	                || text[pos + 1] == '*')) {
		info->cause = JSONNOTEERR_COMMENT;
	} else if (pos < len && text[pos] == '\\') {
		info->cause = JSONNOTEERR_BAD_BACKSLASH;
	} else if (pos < len && (text[pos] == '\'' || (pos > 0
	                         && text[pos - 1] == '\''))) {
		/* cJSON stops on, or one past, a single quote */
		info->cause = JSONNOTEERR_SINGLE_QUOTE;
		if (text[pos] != '\'') {
			pos--;
		}
		hint = pos;
	} else if (scan.valid && scan.depth == 0
	           && scan.expect == EXPECT_COMMA_OR_CLOSE) {
		info->cause = JSONNOTEERR_TRAILING_CONTENT;
	} else if (scan.valid && (text[pos] == '}' || text[pos] == ']')
	           && scan.afterComma) {
		info->cause = JSONNOTEERR_TRAILING_COMMA;
		hint = scan.lastComma;
	} else if (scan.valid && scan.expect == EXPECT_COMMA_OR_CLOSE
	           && StartsValue(text[pos])) {
		info->cause = JSONNOTEERR_MISSING_COMMA;
		hint = scan.lastTokenEnd;
	} else if (scan.valid && scan.expect == EXPECT_COLON && text[pos] != ':') {
		info->cause = JSONNOTEERR_MISSING_COLON;
		hint = scan.lastTokenEnd;
	} else if (scan.valid && scan.expect == EXPECT_KEY && IsWordChar(text[pos])) {
		info->cause = JSONNOTEERR_UNQUOTED_KEY;
	} else if (scan.valid && scan.expect == EXPECT_VALUE
	           && IsWrongLiteral(text + pos)) {
		info->cause = JSONNOTEERR_WRONG_LITERAL;
	} else {
		info->cause = endedEarly ? JSONNOTEERR_UNEXPECTED_END
		              : JSONNOTEERR_GENERIC;
		for (size_t i = 0; i < len; i++) {
			jsonNoteErrorCause_e c = ClassifyChar(text + i);
			if (c != JSONNOTEERR_GENERIC) {
				info->cause = c;
				pos = i;
				hint = i;
				break;
			}
		}
	}

	LineColumn(text, pos, &info->line, &info->column);
	LineColumn(text, hint, &info->hintLine, &info->hintColumn);

	/* Snippet: from the error position to the end of its line, cut so it
	 * never ends partway through a multi-byte character. */
	size_t n = strcspn(text + pos, "\r\n");
	if (n > JSONNOTEERROR_SNIPPET_SIZE - 1) {
		n = JSONNOTEERROR_SNIPPET_SIZE - 1;
		while (n > 0 && IS_UTF8_CONT(text[pos + n])) {
			n--;
		}
	}
	memcpy(info->snippet, text + pos, n);
	info->snippet[n] = '\0';
}

/**
 * Recursive worker for JsonNoteFindDuplicateKey().
 *
 * \param node IN object or array to check, including its children
 * \return the first duplicated key, or NULL
 */
static const char *
FindDuplicateIn(const cJSON *node)
{
	for (const cJSON *a = node ? node->child : NULL; a != NULL; a = a->next) {
		if (cJSON_IsObject(node) && a->string != NULL) {
			for (const cJSON *b = a->next; b != NULL; b = b->next) {
				if (b->string != NULL && strcmp(a->string, b->string) == 0) {
					return a->string;
				}
			}
		}
		if (cJSON_IsObject(a) || cJSON_IsArray(a)) {
			const char *dup = FindDuplicateIn(a);
			if (dup != NULL) {
				return dup;
			}
		}
	}
	return NULL;
}

const char *
JsonNoteFindDuplicateKey(const void *root)
{
	return FindDuplicateIn((const cJSON *)root);
}
