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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <malloc.h>
#include <stdbool.h>
#include <string.h>

#include <Windows.h>

#include <wlib.h>

/**
 * Convert system codepage to UTF 8
 *
 * \param 		   inString		   The input string.
 * \param [in,out] outString	   The output string buffer.
 * \param 		   outStringLength Length of the output buffer
 *
 * \returns FALSE if it fails.
 */

bool
wSystemToUTF8(const char *inString, char *outString, unsigned outStringLength)
{
    unsigned int cnt = 2 * (strlen(inString) + 1);
    char *tempBuffer = malloc(cnt);

    // convert to wide character (UTF16)
    MultiByteToWideChar(CP_ACP,
                        0,
                        inString,
                        -1,
                        (LPWSTR)tempBuffer,
                        cnt);

    // convert from wide char to UTF-8
    WideCharToMultiByte(CP_UTF8,
                        0,
                        (LPCWCH)tempBuffer,
                        -1,
                        (LPSTR)outString,
                        outStringLength,
                        NULL,
                        NULL);

    free(tempBuffer);
    return true;
}

/**
 * Convert from UTF-8 to system codepage
 *
 * \param 		   inString		   The input string.
 * \param [in,out] outString	   the output string.
 * \param 		   outStringLength Length of the output buffer.
 *
 * \returns True if it succeeds, false if it fails.
 */

bool
wUTF8ToSystem(const char *inString, char *outString, unsigned outStringLength)
{
    unsigned int cnt = 2 * (strlen(inString) + 1);
    char *tempBuffer = malloc(cnt);

    // convert to wide character (UTF16)
    MultiByteToWideChar(CP_UTF8,
                        0,
                        inString,
                        -1,
                        (LPWSTR)tempBuffer,
                        cnt);


    cnt = WideCharToMultiByte(CP_ACP,
                              0,
                              (LPCWCH)tempBuffer,
                              -1,
                              (LPSTR)outString,
                              0L,
                              NULL,
                              NULL);

    if (outStringLength <= cnt) {
        return (false);
    }

    // convert from wide char to system codepage
    WideCharToMultiByte(CP_ACP,
                        0,
                        (LPCWCH)tempBuffer,
                        -1,
                        (LPSTR)outString,
                        outStringLength,
                        NULL,
                        NULL);

    free(tempBuffer);
    return true;
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
        if ((// ASCII
                    // use bytes[0] <= 0x7F to allow ASCII control characters
                    bytes[0] == 0x09 ||
                    bytes[0] == 0x0A ||
                    bytes[0] == 0x0D ||
                    (0x20 <= bytes[0] && bytes[0] <= 0x7E)
                )
           ) {
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