/** \file cundo.c
 * Undo / redo functions.
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

/*
 * Implements Undo/Redo
 *
 * Each action/change (New/Modify/Delete) is recorded by UndoNew(), UndoModify(), UndoDelete() within an undo transaction (initiated by UndoStart().
 *
 * New tracks are added to the end of the tracklist (headed by to_first).
 *
 * Modify/Delete generate an undo record (WriteObject) in undoStream.
 * Each record contains:
 *  - op (ModifyOp or DeleteOp)
 *  - address for the existing track
 *  - copy of the track
 *  - endpts, extradata, extra-extradata
 *
 *  Undo pulls records from the undoStream (ReadObject) for the current transation
 *  to recreate modified and deleted tracks.
 *  New tracks are snipped from the tracklist
 *
 * Undone records can be copied to redoStream for susequent redo ops
 *
 * The undo transactions are stored in a circular buffer (undoStack).
 * When this buffer wraps around, the old transaction is recycled,
 * At this point, any DeleteOp records in the old transaction are processed
 * (DeleteInStream) and the deleted track is Free'd
 *
 * The streams are expandable ring buffers.
 * When the transaction buffer wraps, the unreferenced start of the undoStreams is trimmed.
 * THe redoStream is purged for every transaction.
 *
 *
 * Note on Delete
 *
 * UndoDelete does 2 things:
 * 1 Marks the track's transaction record with DeleteOp
 *   When the transaction record is recycled, the old track object will be Free'd.
 * 2 Sets the .delete flag in the track object
 *   For the most part (except dcar.c and cundo.c) IsTrackDeleted() is used in CHECKs
 *   There are a few cases where we have to deal with deleted track.
 *   In general, we do not need to look inside a deleted track and
 *   GET_EXTRA_DATA will complain if we try (FreeTrack is the exception)
 */

#include "cselect.h"
#include "custom.h"
#include "fileio.h"
#include "paths.h"
#include "track.h"
// We need to fiddle with the track list
#include "trackx.h"	// tempTrk, to_first, to_last
#include "trkendpt.h"
#include "draw.h"
#include "cundo.h"
#include "common-ui.h"
#include "ctrain.h"

#include <inttypes.h>

#include <stdint.h>

#define SLOG_FMT "0x%.12" PRIxPTR


/****************************************************************************
 *
 * RPRINTF
 *
 */


#define RBUFF_SIZE (8192)
static char rbuff[RBUFF_SIZE+1];
static int roff;
static int rbuff_record = 0;

EXPORT void Rdump( FILE * outf )
{
	fprintf( outf, "Record Buffer:\n" );
	rbuff[RBUFF_SIZE] = '\0';
	fprintf( outf, "%s", rbuff+roff );
	rbuff[roff] = '\0';
	fprintf( outf, "%s", rbuff );
	memset( rbuff, 0, sizeof rbuff );
	roff = 0;
}


static void Rprintf(
        char * format,
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

/*****************************************************************************
 *
 * UNDO
 *
 */

static int log_undo = 0;	/**< loglevel, can only be set at compile time */

#define UNDO_STACK_SIZE (10)

typedef struct {
	wIndex_t modCnt;
	wIndex_t newCnt;
	wIndex_t delCnt;
	wIndex_t trackCount;
	track_p newTrks;
	uintptr_t undoStart;
	uintptr_t undoEnd;
	uintptr_t redoStart;
	uintptr_t redoEnd;
	BOOL_T needRedo;
	track_p * oldTail;
	track_p * newTail;
	char * label;
} undoStack_t, *undoStack_p;

static undoStack_t undoStack[UNDO_STACK_SIZE];
static wIndex_t undoHead = -1;
static BOOL_T undoActive = FALSE;
static int doCount = 0;
static int undoCount = 0;

static char ModifyOp = 1;
static char DeleteOp = 2;

static BOOL_T recordUndo = 1;

#define UASSERT( ARG, VAL ) \
		if (!(ARG)) return UndoFail( #ARG, VAL, __FILE__, __LINE__ )
#define UASSERT2( ARG, VAL ) \
		if (!(ARG)) { UndoFail( #ARG, VAL, __FILE__, __LINE__ ); return; }

#define INC_UNDO_INX( INX ) {\
		if (++INX >= UNDO_STACK_SIZE) \
			INX = 0; \
		}
#define DEC_UNDO_INX( INX ) {\
		if (--INX < 0) \
			INX = UNDO_STACK_SIZE-1; \
		}

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
static stream_t undoStream;
static stream_t redoStream;

static BOOL_T needAttachTrains = FALSE;

void UndoResume( void )
{
	LOG( log_undo, 1, ( "UndoResume()\n" ) )
	undoActive = TRUE;
}

void UndoSuspend( void )
{
	LOG( log_undo, 1, ( "UndoSuspend()\n" ) )
	undoActive = FALSE;
}


static void DumpStream( FILE * outf, stream_p stream, char * name )
{
	long binx;
	long i, j;
	uintptr_t off;
	streamBlocks_p blk;
	int zeroCnt;
	static char zeros[16] = { 0 };
	fprintf( outf, "Dumping %s\n", name );
	off = stream->startBInx*BSTREAM_SIZE;
	zeroCnt = 0;
	for ( binx=0; binx<stream->stream_da.cnt; binx++ ) {
		blk = DYNARR_N( streamBlocks_p, stream->stream_da, binx );
		for ( i=0; i<BSTREAM_SIZE; i+= 16 ) {
			if ( memcmp( &((*blk)[i]), zeros, 16 ) == 0 ) {
				zeroCnt++;
			} else {
				if ( zeroCnt == 2 ) {
					fprintf( outf, "%6.6lx 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n",
					         (unsigned long)off-16 );
				}
				zeroCnt = 0;
			}
			if ( zeroCnt <= 1 ) {
				fprintf( outf, SLOG_FMT" ", off );
				for ( j=0; j<16; j++ ) {
					fprintf( outf, "%2.2x ", (unsigned char)((*blk)[i+j]) );
				}
				fprintf( outf, "\n" );
			} else if ( zeroCnt == 3 ) {
				fprintf( outf, SLOG_FMT" .. .. .. .. .. .. .. .. .. .. .. .. .. .. .. ..\n",
				         off );
			}
			off += 16;
		}
	}
	if ( zeroCnt > 2 ) {
		fprintf( outf, SLOG_FMT" 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n",
		         off-16 );
	}
}

static BOOL_T UndoFail( char * cause, uintptr_t val, char * fileName,
                        int lineNumber )
{
	int inx, cnt;
	undoStack_p us;
	FILE * outf;
	time_t clock;
	char *temp;
	NoticeMessage( MSG_UNDO_ASSERT, _("Ok"), NULL, fileName, lineNumber, val, val,
	               cause );
	MakeFullpath(&temp, workingDir, sUndoF, NULL);
	outf = fopen( temp, "a+" );
	free(temp);
	if ( outf == NULL ) {
		NoticeMessage(  MSG_OPEN_FAIL, _("Ok"), NULL, _("Undo Trace"), temp,
		                strerror(errno) );
		return FALSE;
	}
	time( &clock );

	fprintf(outf, "\nUndo Assert: %s @ %s:%d (%s)\n", cause, fileName, lineNumber,
	        ctime(&clock) );
	fprintf(outf, "Val = %lld(" SLOG_FMT ")\n", (long long)val, val );
	fprintf(outf, "to_first="SLOG_FMT", to_last="SLOG_FMT"\n", (uintptr_t)to_first,
	        (uintptr_t)to_last );
	fprintf(outf, "undoHead=%d, doCount=%d, undoCount=%d\n", undoHead, doCount,
	        undoCount );
	if (undoHead >= 0 && undoHead < UNDO_STACK_SIZE) {
		inx=undoHead;
	} else {
		inx = 0;
	}
	for (cnt=0; cnt<UNDO_STACK_SIZE; cnt++) {
		us = &undoStack[inx];
		fprintf( outf,
		         "US[%d]: M:%d N:%d D:%d TC:%d NT:"SLOG_FMT" OT:"SLOG_FMT" NT:"SLOG_FMT" US:"SLOG_FMT" UE:"SLOG_FMT" RS:"SLOG_FMT" RE:"SLOG_FMT" NR:%d\n",
		         inx, us->modCnt, us->newCnt, us->delCnt, us->trackCount,
		         (uintptr_t)us->newTrks, (uintptr_t)us->oldTail, (uintptr_t)us->newTail,
		         us->undoStart, us->undoEnd, us->redoStart, us->redoEnd, us->needRedo );
		INC_UNDO_INX(inx);
	}
	fprintf( outf, "Undo: SBI:%ld E:"SLOG_FMT" C:"SLOG_FMT" SC:%d SM:%d\n",
	         undoStream.startBInx, undoStream.end, undoStream.curr, undoStream.stream_da.cnt,
	         undoStream.stream_da.max );
	fprintf( outf, "Redo: SBI:%ld E:"SLOG_FMT" C:"SLOG_FMT" SC:%d SM:%d\n",
	         redoStream.startBInx, redoStream.end, redoStream.curr, redoStream.stream_da.cnt,
	         redoStream.stream_da.max );
	DumpStream( outf, &undoStream, "undoStream" );
	DumpStream( outf, &redoStream, "redoStream" );
	Rdump(outf);
	fclose( outf );
	UndoClear();
	UndoStart( "undoFail", "undoFail" );
	return FALSE;
}


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
	DYNARR_RESET( streamBlocks_p, stream->stream_da );
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
	if (cnt == 0) {
		return TRUE;
	}
	for (inx=binx; inx<stream->stream_da.cnt; inx++) {
		blk = DYNARR_N( streamBlocks_p, stream->stream_da, inx );
		MyFree( blk );
	}
	DYNARR_SET( streamBlocks_p, stream->stream_da, (wIndex_t)binx );
	stream->end = off;
	UASSERT( stream->stream_da.cnt >= 0, stream->stream_da.cnt );
	return TRUE;
}


BOOL_T WriteObject( stream_p stream, char op, track_p trk )
{
	void * buff = NULL;
	long len = 0;
	if (!WriteStream( stream, &op, sizeof op ) ||
	    !WriteStream( stream, &trk, sizeof trk ) ||
	    !WriteStream( stream, trk, sizeof *trk ) ||
	    !WriteStream( stream, trk->endPt, EndPtSize(trk->endCnt) ) ||
	    !WriteStream( stream, trk->extraData, trk->extraSize )) {
		return FALSE;
	}
	/* Add a copy of the any type specific data before it is tampered with, for example */
	if ( !IsTrackDeleted(trk) ) {
		StoreTrackData(trk,&buff,&len);
	} else {
		len = 0;
		buff = NULL;
	}
	if (!WriteStream( stream, &len, sizeof len )) {
		return FALSE;
	}
	if (len)
		if (!WriteStream( stream, buff, len )) {
			return FALSE;
		}
	return TRUE;
}


/**
 * Read an object from a stream
 *
 * \param stream
 * \param needRedo copy current object to redoStream
 *
 */
static BOOL_T ReadObject( stream_p stream, BOOL_T needRedo )
{
	track_p trk;
	track_t tempTrk;
	char op;
	if (!ReadStream( stream, &op, sizeof op )) {
		return FALSE;
	}
	if (!ReadStream( stream, &trk, sizeof trk )) {
		return FALSE;
	}
	LOG( log_undo, 4, ( "     @ " SLOG_FMT " %s\n", stream->curr-1,
	                    op==ModifyOp?"Mod":"Del" ) );
	if (needRedo) {
		if (!WriteObject( &redoStream, op, trk )) {
			return FALSE;
		}
	}
	if (!ReadStream( stream, &tempTrk, sizeof tempTrk )) {
		return FALSE;
	}
	if (op == ModifyOp) {
		UASSERT( (op==ModifyOp) && !IsTrackDeleted(&tempTrk), GetTrkIndex(&tempTrk) );
	}
	// op==DeleteOp doesnot imply that tmpTrk.delete == TRUE: SetDeleteOpInStream
	if (tempTrk.endCnt != trk->endCnt) {
		tempTrk.endPt = MyRealloc( trk->endPt, EndPtSize(tempTrk.endCnt) );
	} else {
		tempTrk.endPt = trk->endPt;
	}
	if (!ReadStream( stream, tempTrk.endPt, EndPtSize(tempTrk.endCnt) )) {
		return FALSE;
	}
	if (tempTrk.extraSize != trk->extraSize) {
		tempTrk.extraData = (extraDataBase_t*)MyRealloc( trk->extraData,
		                    tempTrk.extraSize );
	} else {
		tempTrk.extraData = trk->extraData;
	}
	if (!ReadStream( stream, tempTrk.extraData, tempTrk.extraSize )) {
		return FALSE;
	}
	long Addsize;
	void * tempBuff;
	/* Fix up pts to be as big as it was before -> because it may have changed since */
	if (!ReadStream (stream, &Addsize, sizeof Addsize)) {
		return FALSE;
	}
	if (Addsize) {
		tempBuff = MyMalloc(Addsize);
		if (!ReadStream( stream, tempBuff, Addsize )) {
			return FALSE;
		}
		if ( ! IsTrackDeleted(&tempTrk) ) {
			ReplayTrackData(&tempTrk, tempBuff, Addsize);
		}
		MyFree(tempBuff);
	}
	if ( ! IsTrackDeleted(&tempTrk) ) {
		RebuildTrackSegs(&tempTrk);        //If we had an array of Segs - recreate it
	}
	if (recordUndo) { Rprintf( "Restore T%D(%d) @ "SLOG_FMT"\n", trk->index, tempTrk.index, (uintptr_t)trk ); }
	tempTrk.index = trk->index;
	tempTrk.next = trk->next;
	if ( (tempTrk.bits&TB_CARATTACHED) != 0 ) {
		needAttachTrains = TRUE;
	}
	tempTrk.bits &= ~TB_TEMPBITS;
	*trk = tempTrk;
	if (!IsTrackDeleted(trk)) {
		ClrTrkElev( trk );
	}
	return TRUE;
}


static BOOL_T RedrawInStream( stream_p stream, uintptr_t start, uintptr_t end,
                              BOOL_T draw )
{
	char op;
	track_p trk;
	track_t tempTrk;
	stream->curr = start;
	while (stream->curr < end ) {
		if (!ReadStream( stream, &op, sizeof op ) ||
		    !ReadStream( stream, &trk, sizeof trk ) ||
		    !ReadStream( stream, &tempTrk, sizeof tempTrk ) ) {
			return FALSE;
		}
		stream->curr += tempTrk.extraSize + EndPtSize(tempTrk.endCnt);
		long Addsize;
		if (!ReadStream( stream, &Addsize, sizeof Addsize )) {
			return FALSE;
		}
		stream->curr += Addsize;
		if (!IsTrackDeleted(trk)) {
			if (draw) {
				DrawNewTrack( trk );
			} else {
				UndrawNewTrack( trk );
			}
		}
	}
	return TRUE;
}


/**
 * Delete unreferenced objects from stream
 *
 * \param stream
 * \param start
 * \param end
 *
 * The current transaction is being recycled:
 * unlink and free any deleted objects from the old transaction
 */
static BOOL_T DeleteInStream( stream_p stream, uintptr_t start, uintptr_t end )
{
	char op;
	track_p trk;
	track_p *ptrk;
	track_t tempTrk;
	int delCount = 0;
	LOG( log_undo, 3, ( "    DeleteInStream( , "SLOG_FMT", "SLOG_FMT" )\n", start,
	                    end ) )
	stream->curr = start;
	while (stream->curr < end ) {
		if (!ReadStream( stream, &op, sizeof op )) {
			return FALSE;
		}
		UASSERT( op == ModifyOp || op == DeleteOp, (long)op );
		LOG( log_undo, 4, ( "     @ " SLOG_FMT " %s\n", stream->curr-1,
		                    op==ModifyOp?"Mod":"Del" ) );
		if (!ReadStream( stream, &trk, sizeof trk ) ||
		    !ReadStream( stream, &tempTrk, sizeof tempTrk )) {
			return FALSE;
		}
		stream->curr += tempTrk.extraSize + EndPtSize(tempTrk.endCnt);
		long Addsize;
		if (!ReadStream( stream, &Addsize, sizeof Addsize )) {
			return FALSE;
		}
		stream->curr += Addsize;
		if (op == DeleteOp) {
			if (recordUndo) { Rprintf( "    Free T%D(%d) @ "SLOG_FMT"\n", trk->index, tempTrk.index, (uintptr_t)trk ); }
			LOG( log_undo, 3, ( "        Free T%d @ "SLOG_FMT"\n", GetTrkIndex(trk),
			                    (uintptr_t)trk ) );
			UASSERT( IsTrackDeleted(trk), GetTrkIndex(trk) );
			trk->index = -1;
			delCount++;
		}

	}
	if (delCount) {
		for (ptrk=&to_first; *ptrk; ) {
			if ((*ptrk)->index == -1) {
				// old track to be discarded: Unlink and Free it
				trk = *ptrk;
				UASSERT( IsTrackDeleted(trk), (uintptr_t)trk );
				*ptrk = trk->next;
				FreeTrack(trk);
			} else {
				ptrk = &(*ptrk)->next;
			}
		}
		to_last = ptrk;
	}
	return TRUE;
}


/**
 * Find undo record for 'trk' and change op from Modify to Delete
 *
 * \param stream
 * \param start
 * \param end
 * \param trk
 *
 * Note: does not set trk->delete flag
 */
static BOOL_T SetDeleteOpInStream( stream_p stream, uintptr_t start,
                                   uintptr_t end, track_p trk0 )
{
	char op;
	track_p trk;
	track_t tempTrk;
	size_t binx, boff;
	streamBlocks_p blk;

	LOG( log_undo, 3, ( "        SetDeleteOpInStream T%d @ "SLOG_FMT"\n",
	                    GetTrkIndex(trk0), (uintptr_t)trk0) );
	stream->curr = start;
	while (stream->curr < end) {
		binx = stream->curr/BSTREAM_SIZE;
		binx -= stream->startBInx;
		boff = stream->curr%BSTREAM_SIZE;
		if (!ReadStream( stream, &op, sizeof op )) {
			return FALSE;
		}
		UASSERT( op == ModifyOp || op == DeleteOp, (long)op );
		LOG( log_undo, 4, ( "         @ " SLOG_FMT " %s\n", stream->curr-1,
		                    op==ModifyOp?"Mod":"Del" ) );
		if (!ReadStream( stream, &trk, sizeof trk ) ) {
			return FALSE;
		}
		if (!ReadStream( stream, &tempTrk, sizeof tempTrk )) {
			return FALSE;
		}
		if (trk == trk0) {
			UASSERT( op == ModifyOp, (long)op );
			blk = DYNARR_N( streamBlocks_p, stream->stream_da, binx );
			memcpy( &(*blk)[boff], &DeleteOp, sizeof DeleteOp );
			// Should set .delete flag in stream
			LOG( log_undo, 3, ( "         -> Delete\n") );
			return TRUE;
		}
		stream->curr += tempTrk.extraSize + EndPtSize(tempTrk.endCnt);
		long Addsize;
		if (!ReadStream( stream, &Addsize, sizeof Addsize)) {
			return FALSE;
		}
		stream->curr += Addsize;
	}
	UASSERT( "Cannot find undo record to convert to DeleteOp", 0 );
	return FALSE;
}


static void SetButtons( BOOL_T undoSetting, BOOL_T redoSetting )
{
	static BOOL_T undoButtonEnabled = FALSE;
	static BOOL_T redoButtonEnabled = FALSE;
	int index;
	static char undoHelp[STR_SHORT_SIZE];
	static char redoHelp[STR_SHORT_SIZE];

	if (undoButtonEnabled != undoSetting) {
		wControlActive( (wControl_p)undoB, undoSetting );
		undoButtonEnabled = undoSetting;
	}
	if (redoButtonEnabled != redoSetting) {
		wControlActive( (wControl_p)redoB, redoSetting );
		redoButtonEnabled = redoSetting;
	}
	if (undoSetting) {
		sprintf( undoHelp, _("Undo: %s"), undoStack[undoHead].label );
		wControlSetBalloonText( (wControl_p)undoB, undoHelp );
	} else {
		wControlSetBalloonText( (wControl_p)undoB, _("Undo last command") );
	}
	if (redoSetting) {
		index = undoHead;
		INC_UNDO_INX(index);
		sprintf( redoHelp, _("Redo: %s"), undoStack[index].label );
		wControlSetBalloonText( (wControl_p)redoB, redoHelp );
	} else {
		wControlSetBalloonText( (wControl_p)redoB, _("Redo last undo") );
	}
}


static track_p * FindParent( track_p trk, int lineNum )
{
	track_p *ptrk;
	ptrk = &to_first;
	while ( 1 ) {
		if ( *ptrk == trk ) {
			return ptrk;
		}
		if (*ptrk == NULL) {
			break;
		}
		ptrk = &(*ptrk)->next;
	}
	UndoFail( "Cannot find trk on list", (uintptr_t)trk, "cundo.c", lineNum );
	return NULL;
}


static int undoIgnoreEmpty = 0;

/**
 * Start an Undo transcation
 *
 * \param label help text for balloon help
 * \param format logging info
 *
 *
 */
void UndoStart(
        char * label,
        char * format,
        ... )
{
	static char buff[STR_SIZE];
	va_list ap;
	track_p trk, next;
	undoStack_p us, us1;
	int inx;
	int usp;

	LOG( log_undo, 1, ( "UndoStart[%d] (%s) d:%d u:%d us:"SLOG_FMT"\n", undoHead,
	                    label, undoHead, doCount, undoCount, undoStream.end ) )
	if (recordUndo) {
		va_start( ap, format );
		vsprintf( buff, format, ap );
		va_end( ap );
		Rprintf( "Start(%s)[%d] d:%d u:%d us:"SLOG_FMT"\n", buff, undoHead, doCount,
		         undoCount, undoStream.end );
	}

	if ( undoHead >= 0 ) {
		us = &undoStack[undoHead];
		if ( us->modCnt == 0 && us->delCnt == 0 && us->newCnt == 0 ) {
			LOG( log_undo, 1, ( "    noop: %s - %s\n", us->label?us->label:"<>",
			                    label?label:"<>" ) );
			if ( undoIgnoreEmpty ) {
				us->label = label;
				return;
			}
		}
	}

	INC_UNDO_INX(undoHead);
	us = &undoStack[undoHead];

	SetFileChanged();
	if (doCount == UNDO_STACK_SIZE) {
		if (recordUndo) { Rprintf( "  Wrapped N:%d M:%d D:%d\n", us->newCnt, us->modCnt, us->delCnt ); }
		/* wrapped around stack */
		/* if track saved in undoStream is deleted then really deleted since
		   we can't get it back */
		if (!DeleteInStream( &undoStream, us->undoStart, us->undoEnd )) {
			return;
		}
		/* strip off unused head of stream */
		if (!TrimStream( &undoStream, us->undoEnd )) {
			return;
		}
	} else if (undoCount != 0) {
		if (recordUndo) { Rprintf( "  Undid N:%d M:%d D:%d\n", us->newCnt, us->modCnt, us->delCnt ); }
		/* reusing an undid entry */
		/* really delete all new tracks since this point */
		for( inx=0,usp = undoHead; inx<undoCount; inx++ ) {
			us1 = &undoStack[usp];
			if (recordUndo) { Rprintf("  U[%d] N:%d\n", usp, us1->newCnt ); }
			for (trk=us1->newTrks; trk; trk=next) {
				if (recordUndo) { Rprintf( "    Free T%d @ "SLOG_FMT"\n", trk->index, (uintptr_t)trk ); }
				// trk->delete may not be TRUE, see SetDeleteOpInStream
				LOG( log_undo, 4, ("    Free T%d @ "SLOG_FMT"\n", trk->index,
				                   (uintptr_t)trk ) );
				next = trk->next;
				FreeTrack( trk );
			}
			INC_UNDO_INX(usp);
		}
		/* strip off unused tail of stream */
		if (!TruncateStream( &undoStream, us->undoStart )) {
			return;
		}
	}
	us->label = label;
	us->modCnt = 0;
	us->newCnt = 0;
	us->delCnt = 0;
	us->undoStart = us->undoEnd = undoStream.end;
	ClearStream( &redoStream );
	for ( inx=0; inx<UNDO_STACK_SIZE; inx++ ) {
		undoStack[inx].needRedo = TRUE;
		undoStack[inx].oldTail = NULL;
		undoStack[inx].newTail = NULL;
	}
	us->newTrks = NULL;
	undoStack[undoHead].trackCount = trackCount;
	undoCount = 0;
	undoActive = TRUE;
	for (trk=to_first; trk; trk=trk->next ) {
		trk->modified = FALSE;
		trk->new = FALSE;
	}
	if (doCount < UNDO_STACK_SIZE) {
		doCount++;
	}
	SetButtons( TRUE, FALSE );
}


/**
 * Record Modify'd track for Undo
 * \param trk
 *
 * If track has not been previously recorded in these Undo transaction
 * or is not 'new' write the track to the undoStream which a ModifyOp flag
 */
BOOL_T UndoModify( track_p trk )
{
	undoStack_p us;

	if ( !undoActive ) { return TRUE; }
	if (trk == NULL) { return TRUE; }
	UASSERT(undoCount==0, undoCount);
	UASSERT(undoHead >= 0, undoHead);
	UASSERT(!IsTrackDeleted(trk), GetTrkIndex(trk));
	if (trk->modified || trk->new) {
		return TRUE;
	}
	LOG( log_undo, 2, ( "    UndoModify( T%d, E%d, X%ld @ "SLOG_FMT"\n", trk->index,
	                    trk->endCnt, trk->extraSize, (uintptr_t)trk ) )
	if ( (GetTrkBits(trk)&TB_CARATTACHED)!=0 ) {
		needAttachTrains = TRUE;
	}
	us = &undoStack[undoHead];
	if (recordUndo) {
		Rprintf( " MOD T%d @ "SLOG_FMT"\n", trk->index, (uintptr_t)trk );
	}
	if (!WriteObject( &undoStream, ModifyOp, trk )) {
		return FALSE;
	}
	us->undoEnd = undoStream.end;
	trk->modified = TRUE;
	us->modCnt++;
	return TRUE;
}


/**
 * Record that the track has been deleted
 *
 * \param trk
 *
 * If the track has been Modified, then update undoStream to change op to DeleteOp
 * If the track is not New, then write the record to the undoSteam with a DeleteOp
 * When this undo transaction is recycled, DeleteOp records will unlinked and freed.
 *
 * Otherwise, we're deleting a New track: remove it from track list and discard it
 */
BOOL_T UndoDelete( track_p trk )
{
	undoStack_p us;
	if ( !undoActive ) { return TRUE; }
	LOG( log_undo, 2, ( "    UndoDelete( T%d, E%d, X%ld @ "SLOG_FMT" )\n",
	                    trk->index, trk->endCnt, trk->extraSize, (uintptr_t)trk ) )
	if ( (GetTrkBits(trk)&TB_CARATTACHED)!=0 ) {
		needAttachTrains = TRUE;
	}
	us = &undoStack[undoHead];
	if (recordUndo) {
		Rprintf( " DEL T%d @ "SLOG_FMT"\n", trk->index, (uintptr_t)trk );
	}
	UASSERT( !IsTrackDeleted(trk), trk->index );
	if ( trk->modified ) {
		if (!SetDeleteOpInStream( &undoStream, us->undoStart, us->undoEnd, trk )) {
			return FALSE;
		}
	} else if ( !trk->new ) {
		LOG( log_undo, 3, ( "        Write DeleteOp object\n" ) );
		if (!WriteObject( &undoStream, DeleteOp, trk )) {
			return FALSE;
		}
		us->undoEnd = undoStream.end;
	} else {
		LOG( log_undo, 3, ( "        Remove New object\n" ) );
		track_p * ptrk;
		if (us->newTrks == trk) {
			us->newTrks = trk->next;
		}
		if (!(ptrk = FindParent( trk, __LINE__ ))) {
			return FALSE;
		}
		if (trk->next == NULL) {
			UASSERT( to_last == &(*ptrk)->next, (uintptr_t)&(*ptrk)->next );
			to_last = ptrk;
		}
		*ptrk = trk->next;
		FreeTrack( trk );
		us->newCnt--;
		return TRUE;
	}
	ClrTrkBits( trk, TB_SELECTED );
	trk->deleted = TRUE;
	us->delCnt++;
	return TRUE;
}

/**
 * Record a New track for Undo
 *
 * \param trk
 *
 * New tracks are added to the end of the Track list
 * Save the begining of New tracks in this Undo transaction in us->newTrks
 */
BOOL_T UndoNew( track_p trk )
{
	undoStack_p us;
	if (!undoActive) {
		return TRUE;
	}

	LOG( log_undo, 2, ( "    UndoNew( T%d @ "SLOG_FMT")\n", trk->index,
	                    (uintptr_t)trk ) )

	if (recordUndo) {
		Rprintf( " NEW T%d @"SLOG_FMT"\n", trk->index, (uintptr_t)trk );
	}
	UASSERT(undoCount==0, undoCount);
	UASSERT(undoHead >= 0, undoHead);
	us = &undoStack[undoHead];
	trk->new = TRUE;
	if (us->newTrks == NULL) {
		us->newTrks = trk;
	}
	us->newCnt++;

	return TRUE;
}


/**
 * End of a Undo transaction
 */
void UndoEnd( void )
{
	if (recordUndo) { Rprintf( "End[%d] d:%d\n", undoHead, doCount ); }
	/*undoActive = FALSE;*/
	if ( needAttachTrains ) {
		AttachTrains();
		needAttachTrains = FALSE;
	}
	UpdateAllElevations();
}


/**
 * Reset the Undo state
 */
void UndoClear( void )
{
	int inx;
	LOG( log_undo, 2, ( "    UndoClear()\n" ) )
	undoActive = FALSE;
	undoHead = -1;
	undoCount = 0;
	doCount = 0;
	ClearStream( &undoStream );
	ClearStream( &redoStream );
	for (inx=0; inx<UNDO_STACK_SIZE; inx++) {
		undoStack[inx].undoStart = undoStack[inx].undoEnd = 0;
	}
	SetButtons( FALSE, FALSE );
}


EXPORT wBool_t undoStatus = TRUE;

/**
 * Undo the last transaction
 *
 * Move any New tracks from the end of the Track list
 * Cut the Track list at us->newTrks
 * Read Modified/Deleted tracks from undoSteam
 * Cleanup: redraw, update elevs, cars, counts, ...
 */
void UndoUndo( void * unused )
{
	undoStatus = FALSE;
	undoStack_p us;
	track_p trk;
	wIndex_t oldCount;
	BOOL_T redrawAll;

	if (doCount <= 0) {
		ErrorMessage( MSG_NO_UNDO );
		return;
	}

	ConfirmReset( FALSE );
	wDrawDelayUpdate( mainD.d, TRUE );
	us = &undoStack[undoHead];
	LOG( log_undo, 1, ( "    UndoUndo[%d] d:%d u:%d N:%d M:%d D:%d %s\n", undoHead,
	                    doCount, undoCount, us->newCnt, us->modCnt, us->delCnt,
	                    us->needRedo?"Redo":"" ) )
	if (recordUndo) { Rprintf( "Undo[%d] d:%d u:%d N:%d M:%d D:%d\n", undoHead, doCount, undoCount, us->newCnt, us->modCnt, us->delCnt ); }

	//redrawAll = (us->newCnt+us->modCnt) > incrementalDrawLimit;
	redrawAll = TRUE;
	if (!redrawAll) {
		for (trk=us->newTrks; trk; trk=trk->next ) {
			UndrawNewTrack( trk );
		}
		RedrawInStream( &undoStream, us->undoStart, us->undoEnd, FALSE );
	}

	if (us->needRedo) {
		us->redoStart = us->redoEnd = redoStream.end;
	}
	if (!(us->oldTail=FindParent(us->newTrks,__LINE__))) {
		return;
	}
	us->newTail = to_last;
	to_last = us->oldTail;
	*to_last = NULL;
	needAttachTrains = FALSE;
	undoStream.curr = us->undoStart;
	while ( undoStream.curr < us->undoEnd ) {
		if (!ReadObject( &undoStream, us->needRedo )) {
			return;
		}
	}
	if (us->needRedo) {
		us->redoEnd = redoStream.end;
	}
	us->needRedo = FALSE;

	if ( needAttachTrains ) {
		AttachTrains();
		needAttachTrains = FALSE;
	}
	UpdateAllElevations();
	if (!redrawAll) {
		RedrawInStream( &undoStream, us->undoStart, us->undoEnd, TRUE );
	} else {
		DoRedraw();
	}

	oldCount = trackCount;
	trackCount = us->trackCount;
	us->trackCount = oldCount;
	InfoCount( trackCount );

	doCount--;
	undoCount++;
	DEC_UNDO_INX( undoHead );
	AuditTracks( "undoUndo" );
	SelectRecount();
	SetButtons( doCount>0, TRUE );
	wBalloonHelpUpdate();
	wDrawDelayUpdate( mainD.d, FALSE );
	undoStatus = TRUE;
	return;
}


/**
 * Undo and last Undo op
 *
 * Attach the New tracks to the end of the Track list
 * Read Modified/Deleted object from redoStream
 * Cleanup: redraw, update elevs, cars, counts, ...
 */
void UndoRedo( void * unused )
{
	undoStatus = FALSE;
	undoStack_p us;
	wIndex_t oldCount;
	BOOL_T redrawAll;
	track_p trk;

	if (undoCount <= 0) {
		ErrorMessage( MSG_NO_REDO );
		return;
	}

	ConfirmReset( FALSE );
	wDrawDelayUpdate( mainD.d, TRUE );
	INC_UNDO_INX( undoHead );
	us = &undoStack[undoHead];
	LOG( log_undo, 1, ( "    UndoRedo[%d] d:%d u:%d N:%d M:%d D:%d\n", undoHead,
	                    doCount, undoCount, us->newCnt, us->modCnt, us->delCnt ) )
	if (recordUndo) { Rprintf( "Redo[%d] d:%d u:%d N:%d M:%d D:%d\n", undoHead, doCount, undoCount, us->newCnt, us->modCnt, us->delCnt ); }

	//redrawAll = (us->newCnt+us->modCnt) > incrementalDrawLimit;
	redrawAll = TRUE;
	if (!redrawAll) {
		RedrawInStream( &redoStream, us->redoStart, us->redoEnd, FALSE );
	}

	UASSERT2( us->newTail != NULL, (uintptr_t)us->newTail );
	*to_last = us->newTrks;
	to_last = us->newTail;
	UASSERT2( (*to_last) == NULL, (uintptr_t)*to_last );
	RenumberTracks();

	needAttachTrains = FALSE;
	redoStream.curr = us->redoStart;
	while ( redoStream.curr < us->redoEnd ) {
		if (!ReadObject( &redoStream, FALSE )) {
			return;
		}
	}

	if ( needAttachTrains ) {
		AttachTrains();
		needAttachTrains = FALSE;
	}
	UpdateAllElevations();
	if (!redrawAll) {
		for (trk=us->newTrks; trk; trk=trk->next ) {
			DrawNewTrack( trk );
		}
		RedrawInStream( &redoStream, us->redoStart, us->redoEnd, TRUE );
	} else {
		DoRedraw();
	}

	oldCount = trackCount;
	trackCount = us->trackCount;
	us->trackCount = oldCount;
	InfoCount( trackCount );

	undoCount--;
	doCount++;

	AuditTracks( "undoRedo" );
	SelectRecount();
	SetButtons( TRUE, undoCount>0 );
	wBalloonHelpUpdate();
	wDrawDelayUpdate( mainD.d, FALSE );
	undoStatus = TRUE;
	return;
}


void InitCmdUndo( void )
{
	log_undo = LogFindIndex( "undo" );
}
