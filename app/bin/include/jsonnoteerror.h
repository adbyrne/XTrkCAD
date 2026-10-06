/** \file jsonnoteerror.h
 * Locate and classify a JSON parse failure for the JSON Note editor, so its
 * "Invalid JSON" message can say where parsing stopped and, for the common
 * invisible culprits, why.
 *
 * Deliberately standalone (no dependency on cJSON, common.h, or wlib): it
 * takes the failed text plus the error position cJSON_ParseWithOpts()
 * reported, so it links and CMocka-tests the same lightweight way as
 * notenames.c -- see unittest/jsonnoteerrortest.c.
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

#ifndef JSONNOTEERROR_H
#define JSONNOTEERROR_H

/** Size of jsonNoteErrorInfo_t's snippet buffer, terminator included. */
#define JSONNOTEERROR_SNIPPET_SIZE 24

/** Likely cause of a parse failure, judged from the text at the error
 * position. */
typedef enum {
	JSONNOTEERR_GENERIC,		/**< nothing more specific identified */
	JSONNOTEERR_EMPTY,		/**< text is empty or only whitespace */
	JSONNOTEERR_UNEXPECTED_END,	/**< text ends before the JSON is complete */
	JSONNOTEERR_TYPOGRAPHIC_QUOTE,	/**< curly/low quote instead of a plain " */
	JSONNOTEERR_NONBREAKING_SPACE	/**< U+00A0 instead of an ordinary space */
} jsonNoteErrorCause_e;

/** Where and why parsing stopped. */
typedef struct {
	int line;	/**< 1-based line of the error position */
	int column;	/**< 1-based column, counted in characters, not bytes */
	jsonNoteErrorCause_e cause;
	/** Text starting at the error position, up to the end of that line,
	 * cut on a UTF-8 character boundary. Empty at end of text. */
	char snippet[JSONNOTEERROR_SNIPPET_SIZE];
} jsonNoteErrorInfo_t;

/**
 * Describe a parse failure in \p text.
 *
 * \param text IN the full text that failed to parse (NUL-terminated)
 * \param errPtr IN where parsing stopped, as reported by
 *        cJSON_ParseWithOpts()'s return_parse_end; NULL or a pointer outside
 *        \p text is treated as the start of the text
 * \param info OUT the result
 */
void JsonNoteLocateError(const char *text, const char *errPtr,
                         jsonNoteErrorInfo_t *info);

#endif /* JSONNOTEERROR_H */
