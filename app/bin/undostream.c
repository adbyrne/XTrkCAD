/** \file undostream.c
 * Expandable ring-buffer stream for the undo/redo system.
 *
 * Contains the ring-buffer diagnostic log (Rprintf/Rdump) and the five
 * primitive stream operations (Read/Write/Trim/Clear/Truncate).
 * Extracted from cundo.c so these can be unit-tested without the full
 * application dependency chain.
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

#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "misc.h"        /* MyMalloc, MyFree, MyRealloc, lprintf, STR_SIZE */
#include "undostream.h"

/****************************************************************************
 *
 * RPRINTF — ring-buffer diagnostic log
 *
 */

#define RBUFF_SIZE (8192)
static char rbuff[RBUFF_SIZE+1];
static int roff;
static int rbuff_record = 0;

BOOL_T recordUndo = 1;

EXPORT void Rdump( FILE * outf )
{
	time_t clock;
	time(&clock);
	fprintf( outf, "Record Buffer %s:\n", ctime(&clock) );
	rbuff[RBUFF_SIZE] = '\0';
	fprintf( outf, "%s", rbuff+roff );
	rbuff[roff] = '\0';
	fprintf( outf, "%s", rbuff );
	memset( rbuff, 0, sizeof rbuff );
	fflush( outf );
	roff = 0;
}


void Rprintf(
        const char * format,
        ... )
{
	static char buff[STR_SIZE];
	char * cp;
	va_list ap;
	va_start( ap, format );
	vsprintf( buff, format, ap );
	va_end( ap );
	if (rbuff_record >= 1) {
		lprintf( buff );
	}
	for ( cp=buff; *cp; cp++ ) {
		rbuff[roff] = *cp;
		roff++;
		if (roff>=RBUFF_SIZE) {
			roff=0;
		}
	}
}

/****************************************************************************
 *
 * STREAM — expandable ring-buffer of fixed-size blocks
 *
 */

static int log_undo = 0;

BOOL_T ReadStream( stream_t * stream, void * ptr, int size )
{
	size_t binx, boff, brem;
	streamBlocks_p blk;
	if ( stream->curr+size > stream->end ) {
		UndoFail( "Overrun on stream", (uintptr_t)(stream->curr+size), __FILE__,
		          __LINE__ );
		return FALSE;
	}
	LOG( log_undo, 5, ( "ReadStream( , "SLOG_FMT", %d ) %ld %ld %ld\n",
	                    (uintptr_t)ptr, size, stream->startBInx, stream->curr, stream->end ) )
	binx = stream->curr/BSTREAM_SIZE;
	boff = stream->curr%BSTREAM_SIZE;
	stream->curr += size;
	binx -= stream->startBInx;
	brem = BSTREAM_SIZE - boff;
	while ( brem < size ) {
		UASSERT( binx>=0 && binx < stream->stream_da.cnt, binx );
		blk = DYNARR_N( streamBlocks_p, stream->stream_da, binx );
		memcpy( ptr, &(*blk)[boff], (size_t)brem );
		ptr = (char*)ptr + brem;
		size -= (int)brem;
		binx++;
		boff = 0;
		brem = BSTREAM_SIZE;
	}
	if (size) {
		UASSERT( binx>=0 && binx < stream->stream_da.cnt, binx );
		blk = DYNARR_N( streamBlocks_p, stream->stream_da, binx );
		memcpy( ptr, &(*blk)[boff], size );
	}
	return TRUE;
}

BOOL_T WriteStream( stream_p stream, void * ptr, int size )
{
	size_t binx, boff, brem;
	streamBlocks_p blk;
	LOG( log_undo, 5,
	     ( "WriteStream( , "SLOG_FMT", %d ) %ld "SLOG_FMT" "SLOG_FMT"\n", (uintptr_t)ptr,
	       size, stream->startBInx, stream->curr, stream->end ) )
	if (size == 0) {
		return TRUE;
	}
	binx = stream->end/BSTREAM_SIZE;
	boff = stream->end%BSTREAM_SIZE;
	stream->end += size;
	binx -= stream->startBInx;
	brem = BSTREAM_SIZE - boff;
	while ( size ) {
		if (boff==0) {
			UASSERT( binx == stream->stream_da.cnt, binx );
			DYNARR_APPEND( streamBlocks_p, stream->stream_da, 10 );
			blk = (streamBlocks_p)MyMalloc( sizeof *blk );
			DYNARR_N( streamBlocks_p, stream->stream_da, binx ) = blk;
		} else {
			UASSERT( binx == stream->stream_da.cnt-1, binx );
			blk = DYNARR_N( streamBlocks_p, stream->stream_da, binx );
		}
		if (size > brem) {
			memcpy( &(*blk)[boff], ptr, (size_t)brem );
			ptr = (char*)ptr + brem;
			size -= (int)brem;
			binx++;
			boff = 0;
			brem = BSTREAM_SIZE;
		} else {
			memcpy( &(*blk)[boff], ptr, size );
			break;
		}
	}
	return TRUE;
}

BOOL_T TrimStream( stream_p stream, uintptr_t off )
{
	size_t binx, cnt, inx;
	streamBlocks_p blk;
	LOG( log_undo, 3, ( "    TrimStream( , %ld )\n", off ) )
	binx = off/BSTREAM_SIZE;
	cnt = binx-stream->startBInx;
	if (recordUndo) {
		Rprintf("Trim("SLOG_FMT") %ld blocks (out of %d)\n", off, cnt,
		        stream->stream_da.cnt);
	}
	UASSERT( cnt >= 0 && cnt <= stream->stream_da.cnt, cnt );
	if (cnt == 0) {
		return TRUE;
	}
	for (inx=0; inx<cnt; inx++) {
		blk = DYNARR_N( streamBlocks_p, stream->stream_da, inx );
		MyFree( blk );
	}
	for (inx=cnt; inx<stream->stream_da.cnt; inx++ ) {
		DYNARR_N( streamBlocks_p, stream->stream_da,
		          inx-cnt ) = DYNARR_N( streamBlocks_p, stream->stream_da, inx );
	}
	stream->startBInx =(long)binx;
	stream->stream_da.cnt -= (wIndex_t)cnt;
	UASSERT( stream->stream_da.cnt >= 0, stream->stream_da.cnt );
	return TRUE;
}


void ClearStream( stream_p stream )
{
	long inx;
	streamBlocks_p blk;
	for (inx=0; inx<stream->stream_da.cnt; inx++) {
		blk = DYNARR_N( streamBlocks_p, stream->stream_da, inx );
		MyFree( blk );
	}
	DYNARR_FREE( streamBlocks_p, stream->stream_da );
	stream->startBInx = 0;
	stream->end = stream->curr = 0;
}


BOOL_T TruncateStream( stream_p stream, uintptr_t off )
{
	size_t binx, boff, cnt, inx;
	streamBlocks_p blk;
	LOG( log_undo, 3, ( "TruncateStream( , %ld )\n", off ) )
	binx = off/BSTREAM_SIZE;
	boff = off%BSTREAM_SIZE;
	if (boff!=0) {
		binx++;
	}
	binx -= stream->startBInx;
	cnt = stream->stream_da.cnt-binx;
	if (recordUndo) {
		Rprintf("Truncate("SLOG_FMT") %ld blocks (out of %d)\n", off, cnt,
		        stream->stream_da.cnt);
	}
	UASSERT( cnt >= 0 && cnt <= stream->stream_da.cnt, cnt );
	stream->end = off;
	if (cnt == 0) {
		return TRUE;
	}
	for (inx=binx; inx<stream->stream_da.cnt; inx++) {
		blk = DYNARR_N( streamBlocks_p, stream->stream_da, inx );
		MyFree( blk );
	}
	DYNARR_SET( streamBlocks_p, stream->stream_da, (wIndex_t)binx );
	UASSERT( stream->stream_da.cnt >= 0, stream->stream_da.cnt );
	return TRUE;
}
