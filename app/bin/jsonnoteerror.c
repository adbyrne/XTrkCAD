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
	if (firstNonBlank == len) {
		info->cause = JSONNOTEERR_EMPTY;
	} else if (pos < len && ClassifyChar(text + pos) != JSONNOTEERR_GENERIC) {
		info->cause = ClassifyChar(text + pos);
	} else {
		info->cause = endedEarly ? JSONNOTEERR_UNEXPECTED_END
		              : JSONNOTEERR_GENERIC;
		for (size_t i = 0; i < len; i++) {
			jsonNoteErrorCause_e c = ClassifyChar(text + i);
			if (c != JSONNOTEERR_GENERIC) {
				info->cause = c;
				pos = i;
				break;
			}
		}
	}

	info->line = 1;
	info->column = 1;
	for (size_t i = 0; i < pos; i++) {
		if (text[i] == '\n') {
			info->line++;
			info->column = 1;
		} else if (!IS_UTF8_CONT(text[i])) {
			info->column++;
		}
	}

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
