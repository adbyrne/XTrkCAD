/**
 * \file utf8conv.c.
 *
 * UTF-8 conversion functions
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2020 Martin Fischer
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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <Windows.h>

#include <wlib.h>

/**
 * Convert a string from one code page to another, through UTF-16. The
 * output has to fit with its terminator; otherwise, or if the input isn't
 * valid in its code page, the conversion fails and the output is an empty
 * string. Converting to a code page other than UTF-8 fails too if a
 * character doesn't exist there: no best-fit mapping (L-stroke to L) and no
 * default character ('?').
 *
 * \param from code page of the input
 * \param to code page of the output
 * \param inString the input
 * \param outString the output buffer
 * \param outStringLength size of the output buffer in bytes
 * \returns true if the whole string was converted
 */

static bool
ConvertCodePage(UINT from, UINT to, const char *inString, char *outString,
                unsigned outStringLength)
{
	DWORD toFlags = (to == CP_UTF8) ? 0 : WC_NO_BEST_FIT_CHARS;
	BOOL usedDefault = FALSE;
	// UTF-8 doesn't take a default character
	LPBOOL pUsedDefault = (to == CP_UTF8) ? NULL : &usedDefault;
	WCHAR *wide;
	int wideLength;
	int length;

	if (outStringLength == 0) {
		return false;
	}
	outString[0] = '\0';

	// Number of UTF-16 units, terminator included
	wideLength = MultiByteToWideChar(from, MB_ERR_INVALID_CHARS, inString, -1,
	                                 NULL, 0);
	if (wideLength <= 0) {
		return false;
	}
	wide = malloc(wideLength * sizeof(WCHAR));
	if (wide == NULL) {
		return false;
	}
	MultiByteToWideChar(from, MB_ERR_INVALID_CHARS, inString, -1, wide,
	                    wideLength);

	// Number of output bytes, terminator included
	length = WideCharToMultiByte(to, toFlags, wide, -1, NULL, 0, NULL,
	                             pUsedDefault);
	if (length <= 0 || (unsigned)length > outStringLength || usedDefault) {
		free(wide);
		return false;
	}
	WideCharToMultiByte(to, toFlags, wide, -1, outString, outStringLength,
	                    NULL, NULL);
	free(wide);
	return true;
}

/**
 * Convert a string from the system code page to UTF-8.
 *
 * \param inString the input
 * \param outString the output buffer; an empty string if the conversion fails
 * \param outStringLength size of the output buffer in bytes, terminator
 *        included: three times the input's length is always enough
 * \returns true if the whole string was converted
 */

bool
wSystemToUTF8(const char *inString, char *outString, unsigned outStringLength)
{
	return ConvertCodePage(GetACP(), CP_UTF8, inString, outString,
	                       outStringLength);
}

/**
 * Convert a string from UTF-8 to the system code page. Fails if the input
 * isn't UTF-8 or has a character the code page doesn't.
 *
 * \param inString the input
 * \param outString the output buffer; an empty string if the conversion fails
 * \param outStringLength size of the output buffer in bytes, terminator
 *        included
 * \returns true if the whole string was converted
 */

bool
wUTF8ToSystem(const char *inString, char *outString, unsigned outStringLength)
{
	return ConvertCodePage(CP_UTF8, GetACP(), inString, outString,
	                       outStringLength);
}

/**
 * Is passed string in correct UTF-8 format?
 * Taken from https://stackoverflow.com/questions/1031645/how-to-detect-utf-8-in-plain-c
 *
 * \param  string The string to check.
 *
 * \returns True if UTF 8, false if not.
 */

bool wIsUTF8(const char * string)
{
	if (!string) {
		return 0;
	}

	const unsigned char * bytes = (const unsigned char *)string;
	while (*bytes) {
		// ASCII, control characters and DEL included
		if (bytes[0] <= 0x7F) {
			bytes += 1;
			continue;
		}

		if ((// non-overlong 2-byte
		            (0xC2 <= bytes[0] && bytes[0] <= 0xDF) &&
		            (0x80 <= bytes[1] && bytes[1] <= 0xBF)
		    )
		   ) {
			bytes += 2;
			continue;
		}

		if ((// excluding overlongs
		            bytes[0] == 0xE0 &&
		            (0xA0 <= bytes[1] && bytes[1] <= 0xBF) &&
		            (0x80 <= bytes[2] && bytes[2] <= 0xBF)
		    ) ||
		    (// straight 3-byte
		            ((0xE1 <= bytes[0] && bytes[0] <= 0xEC) ||
		             bytes[0] == 0xEE ||
		             bytes[0] == 0xEF) &&
		            (0x80 <= bytes[1] && bytes[1] <= 0xBF) &&
		            (0x80 <= bytes[2] && bytes[2] <= 0xBF)
		    ) ||
		    (// excluding surrogates
		            bytes[0] == 0xED &&
		            (0x80 <= bytes[1] && bytes[1] <= 0x9F) &&
		            (0x80 <= bytes[2] && bytes[2] <= 0xBF)
		    )
		   ) {
			bytes += 3;
			continue;
		}

		if ((// planes 1-3
		            bytes[0] == 0xF0 &&
		            (0x90 <= bytes[1] && bytes[1] <= 0xBF) &&
		            (0x80 <= bytes[2] && bytes[2] <= 0xBF) &&
		            (0x80 <= bytes[3] && bytes[3] <= 0xBF)
		    ) ||
		    (// planes 4-15
		            (0xF1 <= bytes[0] && bytes[0] <= 0xF3) &&
		            (0x80 <= bytes[1] && bytes[1] <= 0xBF) &&
		            (0x80 <= bytes[2] && bytes[2] <= 0xBF) &&
		            (0x80 <= bytes[3] && bytes[3] <= 0xBF)
		    ) ||
		    (// plane 16
		            bytes[0] == 0xF4 &&
		            (0x80 <= bytes[1] && bytes[1] <= 0x8F) &&
		            (0x80 <= bytes[2] && bytes[2] <= 0xBF) &&
		            (0x80 <= bytes[3] && bytes[3] <= 0xBF)
		    )
		   ) {
			bytes += 4;
			continue;
		}

		return false;
	}

	return true;
}
