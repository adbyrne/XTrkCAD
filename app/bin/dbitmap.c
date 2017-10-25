/** \file dbitmap.c
 *  Print to Bitmap
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

#include <assert.h>

#include "custom.h"
#include "fileio.h"
#include "i18n.h"
#include "layout.h"
#include "messages.h"
#include "param.h"
#include "paths.h"
#include "track.h"


static long outputBitMapTogglesV = 3;
static double outputBitMapDensity = 10;

static struct wFilSel_t * bitmap_fs;
static long bitmap_w, bitmap_h;
static drawCmd_t bitmap_d = {
		NULL,
		&screenDrawFuncs,
		0,
		16.0,
		0.0,
		{0.0, 0.0}, {1.0,1.0},
		Pix2CoOrd, CoOrd2Pix };


static int SaveBitmapFile( 
		int files,
		char **fileName,
		void * data )
{
	coOrd p[4];
	FLOAT_T y0, y1;
	wFont_p fp, fp_bi;
	wFontSize_t fs;
	coOrd textsize, textsize1;

	assert( fileName != NULL );
	assert( files == 1 );

	SetCurrentPath( BITMAPPATHKEY, fileName[ 0 ] ); 

	bitmap_d.d = wBitMapCreate( (wPos_t)bitmap_w, (wPos_t)bitmap_h, 8 );
	if (bitmap_d.d == (wDraw_p)0) {
		NoticeMessage( MSG_WBITMAP_FAILED, _("Ok"), NULL );
		return FALSE;
	}
	y0 = y1 = 0.0;
	p[0].x = p[3].x = 0.0;
	p[1].x = p[2].x = mapD.size.x;
	p[0].y = p[1].y = 0.0;
	p[2].y = p[3].y = mapD.size.y;
	if ( (outputBitMapTogglesV&2) ) {
		DrawRuler( &bitmap_d, p[0], p[1], 0.0, TRUE, FALSE, wDrawColorBlack );
		DrawRuler( &bitmap_d, p[0], p[3], 0.0, TRUE, TRUE, wDrawColorBlack );
		DrawRuler( &bitmap_d, p[1], p[2], 0.0, FALSE, FALSE, wDrawColorBlack );
		DrawRuler( &bitmap_d, p[3], p[2], 0.0, FALSE, TRUE, wDrawColorBlack );
		y0 = 0.37;
		y1 = 0.2;
	}
	if ( (outputBitMapTogglesV&3) == 1) {
		DrawLine( &bitmap_d, p[0], p[1], 2, wDrawColorBlack );
		DrawLine( &bitmap_d, p[0], p[3], 2, wDrawColorBlack );
		DrawLine( &bitmap_d, p[1], p[2], 2, wDrawColorBlack );
		DrawLine( &bitmap_d, p[3], p[2], 2, wDrawColorBlack );
	}
	if (outputBitMapTogglesV&1) {
		fp = wStandardFont( F_TIMES, FALSE, FALSE );
		fs = 18;
		DrawTextSize( &mainD, GetLayoutTitle(), fp, fs, FALSE, &textsize );
		p[0].x = (bitmap_d.size.x - (textsize.x*bitmap_d.scale))/2.0 + bitmap_d.orig.x;
		p[0].y = mapD.size.y + (y1+0.30)*bitmap_d.scale;
		DrawString( &bitmap_d, p[0], 0.0, GetLayoutTitle(), fp, fs*bitmap_d.scale, wDrawColorBlack );
		DrawTextSize( &mainD, GetLayoutSubtitle(), fp, fs, FALSE, &textsize );
		p[0].x = (bitmap_d.size.x - (textsize.x*bitmap_d.scale))/2.0 + bitmap_d.orig.x;
		p[0].y = mapD.size.y + (y1+0.05)*bitmap_d.scale;
		DrawString( &bitmap_d, p[0], 0.0, GetLayoutSubtitle(), fp, fs*bitmap_d.scale, wDrawColorBlack );
		fp_bi = wStandardFont( F_TIMES, TRUE, TRUE );
		DrawTextSize( &mainD, _("Drawn with "), fp, fs, FALSE, &textsize );
		DrawTextSize( &mainD, sProdName, fp_bi, fs, FALSE, &textsize1 );
		p[0].x = (bitmap_d.size.x - ((textsize.x+textsize1.x)*bitmap_d.scale))/2.0 + bitmap_d.orig.x;
		p[0].y = -(y0+0.23)*bitmap_d.scale;
		DrawString( &bitmap_d, p[0], 0.0, _("Drawn with "), fp, fs*bitmap_d.scale, wDrawColorBlack );
		p[0].x += (textsize.x*bitmap_d.scale);
		DrawString( &bitmap_d, p[0], 0.0, sProdName, fp_bi, fs*bitmap_d.scale, wDrawColorBlack );
	}
	wDrawClip( bitmap_d.d,
		 (wPos_t)(-bitmap_d.orig.x/bitmap_d.scale*bitmap_d.dpi),
		 (wPos_t)(-bitmap_d.orig.y/bitmap_d.scale*bitmap_d.dpi),
		 (wPos_t)(mapD.size.x/bitmap_d.scale*bitmap_d.dpi),
		 (wPos_t)(mapD.size.y/bitmap_d.scale*bitmap_d.dpi) );
	wSetCursor( wCursorWait );
	InfoMessage( _("Drawing tracks to BitMap") );
	DrawSnapGrid( &bitmap_d, mapD.size, TRUE );
	if ( (outputBitMapTogglesV&4) )
		bitmap_d.options |= DC_CENTERLINE;
	else
		bitmap_d.options &= ~DC_CENTERLINE;
	DrawTracks( &bitmap_d, bitmap_d.scale, bitmap_d.orig, bitmap_d.size );
	InfoMessage( _("Writing BitMap to file") );
	if ( wBitMapWriteFile( bitmap_d.d, fileName[0] ) == FALSE ) {
		NoticeMessage( MSG_WBITMAP_FAILED, _("Ok"), NULL );
		return FALSE;
	}
	InfoMessage( "" );
	wSetCursor( wCursorNormal );
	wBitMapDelete( bitmap_d.d );
	return TRUE;
}



/*******************************************************************************
 *
 * Output BitMap Dialog
 *
 */

static wWin_p outputBitMapW;

static char *bitmapTogglesLabels[] = { N_("Print Titles"), N_("Print Borders"),
	N_("Print Centerline"), NULL };
static paramFloatRange_t r0o1_100 = { 0.1, 100.0, 60 };

static paramData_t outputBitMapPLs[] = {
#define I_TOGGLES		(0)
	{   PD_TOGGLE, &outputBitMapTogglesV, "toggles", 0, bitmapTogglesLabels },
#define I_DENSITY		(1)
	{   PD_FLOAT, &outputBitMapDensity, "density", PDO_DLGRESETMARGIN, &r0o1_100, N_("    dpi") },
#define I_MSG1			(2)
	{   PD_MESSAGE, N_("Bitmap : 99999 by 99999 pixels"), NULL, PDO_DLGRESETMARGIN|PDO_DLGUNDERCMDBUTT|PDO_DLGWIDE, (void*)180 },
#define I_MSG2			(3)
	{   PD_MESSAGE, N_("Approximate file size: 999.9Mb"), NULL, PDO_DLGUNDERCMDBUTT, (void*)180 } };

static paramGroup_t outputBitMapPG = { "outputbitmap", 0, outputBitMapPLs, sizeof outputBitMapPLs/sizeof outputBitMapPLs[0] };


static void OutputBitMapComputeSize( void )
{
	FLOAT_T Lborder=0.0, Rborder=0.0, Tborder=0.0, Bborder=0.0;
	FLOAT_T size;

	ParamLoadData( &outputBitMapPG );
	bitmap_d.dpi = mainD.dpi;
	bitmap_d.scale = mainD.dpi/outputBitMapDensity;

	if (outputBitMapTogglesV&2) {
		Lborder = 0.37;
		Rborder = 0.2;
		Tborder = 0.2;
		Bborder = 0.37;
	}
	if (outputBitMapTogglesV&1) {
		Tborder += 0.60;
		Bborder += 0.28;
	}
	bitmap_d.orig.x = 0.0-Lborder*bitmap_d.scale;
	bitmap_d.size.x = mapD.size.x + (Lborder+Rborder)*bitmap_d.scale;
	bitmap_d.orig.y = 0.0-Bborder*bitmap_d.scale;
	bitmap_d.size.y = mapD.size.y + (Bborder+Tborder)*bitmap_d.scale;
	bitmap_w = (long)(bitmap_d.size.x/bitmap_d.scale*bitmap_d.dpi)/*+1*/;
	bitmap_h = (long)(bitmap_d.size.y/bitmap_d.scale*bitmap_d.dpi)/*+1*/;
	sprintf( message, _("Bitmap : %ld by %ld pixels"), bitmap_w, bitmap_h );
	ParamLoadMessage( &outputBitMapPG, I_MSG1, message );
	size = bitmap_w * bitmap_h;
	if ( size < 1e4 )
		sprintf( message, _("Approximate file size : %0.0f"), size );
	else if ( size < 1e6 )
		sprintf( message, _("Approximate file size : %0.1fKb"), (size+50.0)/1e3 );
	else
		sprintf( message, _("Approximate file size : %0.1fMb"), (size+5e4)/1e6 );
	ParamLoadMessage( &outputBitMapPG, I_MSG2, message );
}


static void OutputBitMapOk( void * junk )
{
	FLOAT_T size;
	if (bitmap_w>32000 || bitmap_h>32000) {
		NoticeMessage( MSG_BITMAP_TOO_LARGE, _("Ok"), NULL );
		return;
	}
	size = bitmap_w * bitmap_h;
	if (size >= 1000000) {
		if (NoticeMessage(MSG_BITMAP_SIZE_WARNING, _("Yes"), _("Cancel") )==0)
			return;
	}
	wHide( outputBitMapW );
	if (bitmap_fs == NULL)
		bitmap_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("Save Bitmap"),
#ifdef WINDOWS
				_("Bitmap files|*.bmp"),
#else
				_("Bitmap files|*.xpm"),
#endif
				SaveBitmapFile, NULL );
	wFilSelect( bitmap_fs, GetCurrentPath( BITMAPPATHKEY ));
}



static void OutputBitMapChange( long changes )
{
	if ((changes&(CHANGE_UNITS|CHANGE_MAP))==0 || outputBitMapW==NULL)
		return;
	wControlSetLabel( outputBitMapPLs[I_DENSITY].control, units==UNITS_METRIC?"dpcm":"dpi" );
	ParamLoadControls( &outputBitMapPG );
	OutputBitMapComputeSize();
}


static void DoOutputBitMap( void * junk )
{
	if (outputBitMapW == NULL) {
		outputBitMapW = ParamCreateDialog( &outputBitMapPG, MakeWindowTitle(_("BitMap")), _("Ok"), OutputBitMapOk, wHide, TRUE, NULL, 0, (paramChangeProc)OutputBitMapComputeSize );
	}
	ParamLoadControls( &outputBitMapPG );
	ParamGroupRecord( &outputBitMapPG );
	OutputBitMapComputeSize();
	wShow( outputBitMapW );
}


EXPORT addButtonCallBack_t OutputBitMapInit( void )
{
	ParamRegister( &outputBitMapPG );
	RegisterChangeNotification(OutputBitMapChange);
	return &DoOutputBitMap;
}
