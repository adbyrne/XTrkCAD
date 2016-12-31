/** \file dxfoutput.c
 * Exporting DXF files
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef WINDOWS
#include <io.h>
#include <windows.h>
#if _MSC_VER >=1400
#define strdup _strdup
#endif
#endif

#include <locale.h>
#include <assert.h>
#include "track.h"
#include "i18n.h"

static FILE * dxfF;
static struct wFilSel_t * exportDXFFile_fs;

static void DxfLine(
		drawCmd_p d,
		coOrd p0,
		coOrd p1,
		wDrawWidth width,
		wDrawColor color )
{
	fprintf(dxfF, "  0\nLINE\n" );
	fprintf(dxfF, "  8\n%s%d\n", sProdNameUpper, curTrackLayer+1 );
	fprintf(dxfF, "  10\n%0.6f\n  20\n%0.6f\n  11\n%0.6f\n  21\n%0.6f\n",
		p0.x, p0.y, p1.x, p1.y );
	fprintf(dxfF, "  6\n%s\n", (d->options&DC_DASH)?"DASHED":"CONTINUOUS" );
}

static void DxfArc(
		drawCmd_p d,
		coOrd p,
		DIST_T r,
		ANGLE_T angle0,
		ANGLE_T angle1,
		BOOL_T drawCenter,
		wDrawWidth width,
		wDrawColor color )
{
	angle0 = NormalizeAngle(90.0-(angle0+angle1));
	if (angle1 >= 360.0) {
		fprintf(dxfF, "  0\nCIRCLE\n" );
		fprintf(dxfF, "  10\n%0.6f\n  20\n%0.6f\n  40\n%0.6f\n",
				p.x, p.y, r );
	} else {
		fprintf(dxfF, "  0\nARC\n" );
		fprintf(dxfF, "  10\n%0.6f\n  20\n%0.6f\n  40\n%0.6f\n  50\n%0.6f\n  51\n%0.6f\n",
				p.x, p.y, r, angle0, angle0+angle1 );
	}
	fprintf(dxfF, "  8\n%s%d\n", sProdNameUpper, curTrackLayer+1 );
	fprintf(dxfF, "  6\n%s\n", (d->options&DC_DASH)?"DASHED":"CONTINUOUS" );
}

static void DxfString(
		drawCmd_p d,
		coOrd p,
		ANGLE_T a,
		char * s,
		wFont_p fp,
		FONTSIZE_T fontSize,
		wDrawColor color )
{
	fprintf(dxfF, "  0\nTEXT\n" );
	fprintf(dxfF, "  1\n%s\n", s );
	fprintf(dxfF, "  8\n%s%d\n", sProdNameUpper, curTrackLayer+1 );
	fprintf(dxfF, "  10\n%0.6f\n  20\n%0.6f\n", p.x, p.y );
	fprintf(dxfF, "  40\n%0.6f\n", fontSize/72.0 );
}

static void DxfBitMap(
		drawCmd_p d,
		coOrd p,
		wDrawBitMap_p bm,
		wDrawColor color )
{
}

static void DxfFillPoly(
		drawCmd_p d,
		int cnt,
		coOrd * pts,
		wDrawColor color )
{
	int inx;
	for (inx=1; inx<cnt; inx++) {
		DxfLine( d, pts[inx-1], pts[inx], 0, color );
	}
	DxfLine( d, pts[cnt-1], pts[0], 0, color );
}

static void DxfFillCircle( drawCmd_p d, coOrd center, DIST_T radius, wDrawColor color )
{
	DxfArc( d, center, radius, 0.0, 360, FALSE, 0, color );
}


static drawFuncs_t dxfDrawFuncs = {
		0,
		DxfLine,
		DxfArc,
		DxfString,
		DxfBitMap,
		DxfFillPoly,
		DxfFillCircle };

static drawCmd_t dxfD = {
		NULL, &dxfDrawFuncs, 0, 1.0, 0.0, {0.0,0.0}, {0.0,0.0}, Pix2CoOrd, CoOrd2Pix, 100.0 };

static int DoExportDXFTracks(
		int cnt,
		char ** fileName,
		void * data )
{
	time_t clock;
	char *oldLocale;

	assert( fileName != NULL );
	assert( cnt == 1 );

	SetCurrentPath( DXFPATHKEY, fileName[ 0 ] );
	dxfF = fopen( fileName[0], "w" );
	if (dxfF==NULL) {
		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, "DXF", fileName[0], strerror(errno) );
		return FALSE;
	}

	oldLocale = SaveLocale( "C" );
	wSetCursor( wCursorWait );
	time(&clock);
	fprintf(dxfF,"\
  0\nSECTION\n\
  2\nHEADER\n\
  9\n$ACADVER\n  1\nAC1009\n\
  9\n$EXTMIN\n  10\n%0.6f\n  20\n%0.6f\n\
  9\n$EXTMAX\n  10\n%0.6f\n  20\n%0.6f\n\
  9\n$TEXTSTYLE\n  7\nSTANDARD\n\
  0\nENDSEC\n\
  0\nSECTION\n\
  2\nTABLES\n\
  0\nTABLE\n\
  2\nLTYPE\n\
  0\nLTYPE\n  2\nCONTINUOUS\n  70\n0\n\
  3\nSolid line\n\
  72\n65\n  73\n0\n  40\n0\n\
  0\nLTYPE\n  2\nDASHED\n  70\n0\n\
  3\n__ __ __ __ __ __ __ __ __ __ __ __ __ __ __\n\
  72\n65\n  73\n2\n  40\n0.15\n  49\n0.1\n  49\n-0.05\n\
  0\nLTYPE\n  2\nDOT\n  70\n0\n\
  3\n...............................................\n\
  72\n65\n  73\n2\n  40\n0.1\n  49\n0\n  49\n-0.05\n\
  0\nENDTAB\n\
  0\nTABLE\n\
  2\nLAYER\n\
  70\n0\n\
  0\nLAYER\n  2\n%s1\n  6\nCONTINUOUS\n  62\n7\n  70\n0\n\
  0\nLAYER\n  2\n%s2\n  6\nCONTINUOUS\n  62\n7\n  70\n0\n\
  0\nLAYER\n  2\n%s3\n  6\nCONTINUOUS\n  62\n7\n  70\n0\n\
  0\nLAYER\n  2\n%s4\n  6\nCONTINUOUS\n  62\n7\n  70\n0\n\
  0\nLAYER\n  2\n%s5\n  6\nCONTINUOUS\n  62\n7\n  70\n0\n\
  0\nLAYER\n  2\n%s6\n  6\nCONTINUOUS\n  62\n7\n  70\n0\n\
  0\nLAYER\n  2\n%s7\n  6\nCONTINUOUS\n  62\n7\n  70\n0\n\
  0\nLAYER\n  2\n%s8\n  6\nCONTINUOUS\n  62\n7\n  70\n0\n\
  0\nLAYER\n  2\n%s9\n  6\nCONTINUOUS\n  62\n7\n  70\n0\n\
  0\nLAYER\n  2\n%s10\n  6\nCONTINUOUS\n  62\n7\n  70\n0\n\
  0\nENDTAB\n\
  0\nENDSEC\n\
  0\nSECTION\n\
  2\nENTITIES\n\
",
		0.0, 0.0, mapD.size.x, mapD.size.y,
		sProdNameUpper, sProdNameUpper, sProdNameUpper, sProdNameUpper, sProdNameUpper,
		sProdNameUpper, sProdNameUpper, sProdNameUpper, sProdNameUpper, sProdNameUpper );
	DrawSelectedTracks( &dxfD );
	fprintf(dxfF,"  0\nENDSEC\n");
	fprintf(dxfF,"  0\nEOF\n");
	fclose(dxfF);
	RestoreLocale( oldLocale );
	Reset();
	wSetCursor( wCursorNormal );
	return TRUE;
}


void DoExportDXF( void )
{
	if (selectedTrackCount <= 0) {
		ErrorMessage( MSG_NO_SELECTED_TRK );
		return;
	}
	if (exportDXFFile_fs == NULL)
		exportDXFFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("Export to DXF"),
				sDXFFilePattern, DoExportDXFTracks, NULL );

	wFilSelect( exportDXFFile_fs, curDirName );
}
