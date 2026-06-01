/** \file undostream.h
 * Expandable ring-buffer stream used by the undo/redo system.
 *
 * Split out from cundo.c so the five stream primitives (Read/Write/Trim/
 * Clear/Truncate) can be unit-tested without the full application
 * dependency chain.
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

#ifndef HAVE_UNDOSTREAM_H
#define HAVE_UNDOSTREAM_H

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include "common.h"

/* printf format string for uintptr_t */
#define SLOG_FMT "0x%.12" PRIxPTR

/* -----------------------------------------------------------------------
 * Ring-buffer diagnostic log (written by Rprintf, dumped by Rdump)
 * ----------------------------------------------------------------------- */

/* non-zero enables writing undo ops into the ring buffer */
extern BOOL_T recordUndo;

void Rdump(FILE *outf);
void Rprintf(const char *format, ...);

/* -----------------------------------------------------------------------
 * Stream buffer: fixed-size BSTREAM_SIZE blocks in a dynamic array
 * ----------------------------------------------------------------------- */

#define BSTREAM_SIZE (4096)
typedef char streamBlocks_t[BSTREAM_SIZE];
typedef streamBlocks_t *streamBlocks_p;
typedef struct {
	dynArr_t stream_da;
	long startBInx;
	uintptr_t end;
	uintptr_t curr;
} stream_t;
typedef stream_t *stream_p;

/* -----------------------------------------------------------------------
 * Assertion handler.
 * Defined in cundo.c for production builds; test files provide a stub.
 * ----------------------------------------------------------------------- */
BOOL_T UndoFail(char *cause, uintptr_t val, char *fileName, int lineNumber);

#define UASSERT( ARG, VAL ) \
		if (!(ARG)) return UndoFail( #ARG, (uintptr_t)(VAL), __FILE__, __LINE__ )
#define UASSERT2( ARG, VAL ) \
		if (!(ARG)) { UndoFail( #ARG, (uintptr_t)(VAL), __FILE__, __LINE__ ); return; }

/* -----------------------------------------------------------------------
 * Stream primitives
 * ----------------------------------------------------------------------- */
BOOL_T ReadStream(stream_t *stream, void *ptr, int size);
BOOL_T WriteStream(stream_p stream, void *ptr, int size);
BOOL_T TrimStream(stream_p stream, uintptr_t off);
void ClearStream(stream_p stream);
BOOL_T TruncateStream(stream_p stream, uintptr_t off);

#endif /* HAVE_UNDOSTREAM_H */
