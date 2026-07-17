/** \file getargs.c
 * InputError() and GetArgs(): the format-string parser used throughout the
 * .xtc/.xtr/.xtp file readers to pull typed fields out of untrusted file
 * lines, and its error-reporting helper.
 *
 * Extracted out of fileio.c (2026-07-11, pure code motion, no logic change)
 * so these two functions -- which only depend on a handful of globals
 * (paramFile/paramFileName/paramLineNum/paramLine/message/mainD) and have
 * no dependency on the rest of fileio.c's ~100-symbol dependency graph
 * (track loading, GTK widget state, undo, archive/zip handling, etc.) --
 * can be built and fuzz-tested (see app/bin/fuzz/fuzz_getargs.c) in
 * isolation, without needing a running GTK application.
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis
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

#include "common.h"
#include "draw.h"
#include "fileio.h"
#include "misc.h"

#ifdef UTFCONVERT
#include "include/utf8convert.h"
#endif // UTFCONVERT

/**
 * Show an error message if problems occur during loading of a param or layout file.
 * The user has the choice to cancel the operation or to continue. If operation is
 * canceled the open file is closed.
 *
 * \param[in] msg error message
 * \param[in] showLine set to true if current line should be included in error message
 * \param[in] ... variable number additional error information
 * \return TRUE to continue, FALSE to abort operation
 *
 */

EXPORT int InputError(
        char * msg,
        BOOL_T showLine,
        ... )
{
	va_list ap;
	char * mp = message;
	int ret;

	mp += sprintf( message, "INPUT ERROR: %s:%d\n",
	               paramFileName, paramLineNum );
	va_start( ap, showLine );
	mp += vsprintf( mp, msg, ap );
	va_end( ap );
	if (showLine) {
		*mp++ = '\n';
		/* paramLine is read via fgets() from the file being loaded (up to
		 * STR_HUGE_SIZE bytes) -- an untrusted, potentially adversarial or
		 * corrupt-file-controlled length. message[] is a fixed-size buffer
		 * already partially filled above by the header/msg text, and a
		 * further strcat() of a fixed prompt follows this block -- bound
		 * the copy to what's actually left (reserving headroom for that
		 * trailing prompt) instead of trusting paramLine to fit. */
		size_t bufSize = sizeof(message);
		size_t used = (size_t)(mp - message);
		size_t reserve = 128; /* headroom for the trailing "continue?" prompt */
		size_t avail = (used + reserve < bufSize) ? bufSize - used - reserve : 0;
		strncpy( mp, paramLine, avail );
		mp[avail] = '\0';
	}
	strcat( mp, _("\nDo you want to continue?") );
	if (!(ret = wNoticeWithIcon( NT_ERROR, message, _("Continue"), _("Stop") ))) {
		if ( paramFile ) {
			fclose(paramFile);
			paramFile = NULL;
		}
		if ( paramFileName ) {
			free( paramFileName );
			paramFileName = NULL;
		}
	}
	return ret;
}


/* SyntaxError() stays in fileio.c (calls InputError() but doesn't need to
 * move for the fuzz-target extraction this file exists for). */

/**
 * Parse a line in XTrackCAD's file format
 *
 * \param line IN line to parse
 * \param format IN ???
 *
 * \return FALSE in case of parsing error, TRUE on success
 * In the error case, InputError had been called which may have closed the input file (paramFile)
 *
 * format chars are:
 * 0 - read a number and discard
 * X - no read, *pi = 0
 * Y - no read, *pf = 0L
 * Z - no read, *pl = 0.0
 * L - *pi = number
 * d - *pi = number
 * w - *pf = read a width
 * u - *pul = number
 * l - *pl = number
 * f - *pf = number
 * z - *pf = 0.0
 * p - *pp = ( number, number ) a coOrd
 * s - *ps = string, bounded by a mandatory inline width (e.g. "s9" for a
 *     10-byte buffer, mirroring scanf's "%9s") -- the width is the max
 *     characters copied, not counting the terminating NUL
 * q - *ps = quoted string
 * c - *qp = position of next non-space char or NULL
 */

EXPORT BOOL_T GetArgs(
        char * line,
        const char * format,
        ... )
{
	char * cp, * cq;
	long * pl;
	unsigned long *pul;
	int * pi;
	FLOAT_T *pf;
	coOrd p, *pp;
	char * ps;
	char ** qp;
	va_list ap;
	char * sError = NULL;

	cp = line;
	va_start( ap, format );
	for ( ; sError==NULL && *format; format++ ) {
		while (isspace((unsigned char)*cp)) { cp++; }
		if (!*cp && strchr( "XZYzc", *format ) == NULL ) {
			sError = "EOL unexpected";
			break;
		}
		switch (*format) {
		case '0':
			(void)strtol( cp, &cq, 10 );
			if (cp == cq) {
				sError = "%s: expected integer";
				break;
			}
			cp = cq;
			break;
		case 'X':
			pi = va_arg( ap, int * );
			*pi = 0;
			break;
		case 'Z':
			pl = va_arg( ap, long * );
			*pl = 0;
			break;
		case 'Y':
			pf = va_arg( ap, FLOAT_T * );
			*pf = 0;
			break;
		case 'L':
			pi = va_arg( ap, int * );
			*pi = (int)strtol( cp, &cq, 10 );
			if (cp == cq) {
				sError = "%s: expected integer";
				break;
			}
			cp = cq;
			break;
		case 'd':
			pi = va_arg( ap, int * );
			*pi = (int)strtol( cp, &cq, 10 );
			if (cp == cq) {
				sError = "%s: expected integer";
				break;
			}
			cp = cq;
			break;
		case 'w':
			pf = va_arg( ap, FLOAT_T * );
			*pf = (FLOAT_T)strtol( cp, &cq, 10 );
			if (cp == cq) {
				sError = "%s: expected integer";
				break;
			}
			if (*cq == '.') {
				*pf = strtod( cp, &cq );
			} else {
				*pf /= mainD.dpi;
			}
			cp = cq;
			break;
		case 'u':
			pul = va_arg( ap, unsigned long * );
			*pul = strtoul( cp, &cq, 10 );
			if (cp == cq) {
				sError = "%s: expected integer";
				break;
			}
			cp = cq;
			break;
		case 'l':
			pl = va_arg( ap, long * );
			*pl = strtol( cp, &cq, 10 );
			if (cp == cq) {
				sError = "%s: expected integer";
				break;
			}
			cp = cq;
			break;
		case 'f':
			pf = va_arg( ap, FLOAT_T * );
			*pf = strtod( cp, &cq );
			if (cp == cq) {
				sError = "%s: expected float";
				break;
			}
			cp = cq;
			break;
		case 'z':
			pf = va_arg( ap, FLOAT_T * );
			*pf = 0.0;
			break;
		case 'p':
			pp = va_arg( ap, coOrd * );
			p.x = strtod( cp, &cq );
			if (cp == cq) {
				sError = "%s: expected float";
				break;
			}
			cp = cq;
			p.y = strtod( cp, &cq );
			if (cp == cq) {
				sError = "%s: expected float";
				break;
			}
			cp = cq;
			*pp = p;
			break;
		case 's': {
			char * ps0;
			size_t maxLen = 0;
			ps = va_arg( ap, char * );
			/* Mandatory inline width, e.g. "s9" -- see format-chars comment
			 * above. A missing width is a caller bug (every real call site
			 * has a fixed-size destination buffer), not malformed input, so
			 * it's enforced with CHECK() rather than sError. */
			while (isdigit((unsigned char)format[1])) {
				maxLen = maxLen * 10 + (size_t)(format[1] - '0');
				format++;
			}
			CHECK( maxLen > 0 );
			while (isspace((unsigned char)*cp)) { cp++; }
			ps0 = ps;
			while (*cp && !isspace((unsigned char)*cp)) {
				if ((size_t)(ps - ps0) >= maxLen) {
					sError = "%s: value too long";
					break;
				}
				*ps++ = *cp++;
			}
			if (sError) {
				break;
			}
			*ps++ = '\0';
			break;
		}
		case 'q':
			qp = va_arg( ap, char * * );
			if (*cp != '\"')
				/* Stupid windows */
			{
				cq = strchr( cp, '\"' );
			} else {
				cq = cp;
			}
			if (cq!=NULL) {
				cp = cq;
				ps = &message[0];
				cp++;
				while (*cp) {
					CHECK( (size_t)(ps-message)<sizeof message );
					if (*cp == '\"') {
						if (*++cp == '\"') {
							*ps++ = '\"';
						} else {
							*ps = '\0';
							/* *++cp above may have already landed on the
							 * terminating NUL (closing quote was the last
							 * byte of input) -- only advance past it if
							 * there's more to consume, or this reads one
							 * byte past the buffer. */
							if (*cp) {
								cp++;
							}
							break;
						}
					} else {
						*ps++ = *cp;
					}
					cp++;
				}
				*ps = '\0';
			} else {
				message[0] = '\0';
			}
#ifdef UTFCONVERT
			ConvertUTF8ToSystem(message);
#endif
			*qp = (char*)ConvertFromEscapedText(message);
			break;
		case 'c':
			qp = va_arg( ap, char * * );
			while (isspace((unsigned char)*cp)) { cp++; }
			if (*cp) {
				*qp = cp;
			} else {
				*qp = NULL;
			}
			break;
		default:
			CHECKMSG( FALSE, ( "getArgs: bad format char: %c", *format ) );
		}
	}
	va_end( ap );
	if ( sError ) {
		InputError( sError, TRUE, cp );
		return FALSE;
	}
	return TRUE;
}
