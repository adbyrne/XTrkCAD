/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/gtkfont.c,v 1.2 2007-02-23 16:51:11 m_fischer Exp $
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
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "X11/Xlib.h"

#include "wlib.h"
#include "gtkint.h"

extern Display * gdk_display;

#ifndef TRUE
#define TRUE (1)
#define FALSE (0)
#endif

static wDraw_p font_d;

static char sampleText[] = "AbCdE0129!@$&()[]{}";

static wWin_p fontSelW;
static wButton_p fontOkB;
static wButton_p fontCancelB;
static wButton_p fontWeightB;
static wButton_p fontSlantB;
static wInteger_p fontSizeB;
static wList_p fontListB;
static int fontSelectMode = 0;

static long fontSize = 18;

int wLoadFont( wDraw_p, const char *, double, int );

/*****************************************************************************
 * FONT HANDLERS
 */

#define FW_MEDIUM	(0)
#define FW_BOLD		(1)
#define FS_REGULAR	(0)
#define FS_ITALIC	(1)


struct wFont_t {
		wIndex_t fi;
		wBool_t bold;
		wBool_t italic;
		};
static wIndex_t standardFonts[3];

typedef struct {
		const char * faceName;
		const char * fullName[2][2];
		struct wFont_t font[2][2];
		} fontInfo_t;
static dynArr_t fontInfo_da;
#define fontInfo(N) DYNARR_N(fontInfo_t,fontInfo_da,N)

static long fontClock = 1;

static long curFontInx;
static long curFontWeight;
static long curFontSlant;
static long newFontInx;
static long newFontWeight;
static long newFontSlant;
static long oldFontWeight = -1;
static long oldFontSlant = -1;


static void doFontOk( void )
{
	if (fontInfo(newFontInx).fullName[newFontWeight][newFontSlant] == NULL) {
		wNotice( "No font selected", "Continue", NULL );
		return;
	}
	if ( curFontWeight != newFontWeight ||
		 curFontSlant != newFontSlant ||
		 curFontInx != newFontInx ) {
		curFontInx = newFontInx;
		curFontWeight = newFontWeight;
		curFontSlant = newFontSlant;
	}
	fontClock++;
	wWinShow( fontSelW, FALSE );
	wPrefSetString( "font", "name", fontInfo(curFontInx).fullName[curFontWeight][curFontSlant] );
	wPrefSetInteger( "font", "size", fontSize );
}

static void doFontCancel( void )
{
	wWinShow( fontSelW, FALSE );
	wButtonSetBusy( fontWeightB, curFontWeight==0 );
	wButtonSetBusy( fontSlantB, curFontSlant==0 );
}


void gtkDrawString( wDraw_p, wPos_t, wPos_t, const char *, wFontSize_t, wDrawColor, wDrawOpts );
double fontSize1 = 50.0;
wDrawColor black;

static void fontRedraw( wDraw_p b, void * context, wPos_t w, wPos_t y )
{
	wDrawClear( b );
	if (fontInfo(newFontInx).fullName[newFontWeight][newFontSlant] == NULL )
		return;
	if ( ! wLoadFont( b, fontInfo(newFontInx).fullName[newFontWeight][newFontSlant],
				(double)fontSize, TRUE ) )
		return;
	gtkDrawString( b, 0, 8, sampleText, (double)fontSize, black, 0 );
}

#ifdef LATER
static void fontProc( wWin_p win, winProcEvent e, void * context )
{
	switch( e ) {
	case wRedraw_e:
		fontRedraw( font_d, NULL, 0, 0 );
		break;
	default:
	}
}
#endif


static void selectAttr( void )
{
	int i;
	const char * oldFontName;
	const char * newFontName;
	if (fontInfo_da.cnt==0) {
		wNotice( "No fonts", "Continue", NULL );
		wDrawClear( font_d );
		return;
	}
	wControlShow( (wControl_p)fontListB, FALSE );
	for (i=0;i<fontInfo_da.cnt;i++) {
		newFontName = fontInfo(i).fullName[newFontWeight][newFontSlant];
		if (oldFontWeight != -1 && oldFontSlant != -1 )
			oldFontName = fontInfo(i).fullName[oldFontWeight][oldFontSlant];
		else
			oldFontName = NULL;
		if ( (newFontName == NULL ) != (oldFontName == NULL) ) {
			wListSetActive( fontListB, i, newFontName != NULL );
		}
	}
	wControlShow( (wControl_p)fontListB, TRUE );
	if (fontInfo(newFontInx).fullName[newFontWeight][newFontSlant]) {
		wControlActive( (wControl_p)fontOkB, TRUE );
		wListSetIndex( fontListB, newFontInx );
	} else {
		wControlActive( (wControl_p)fontOkB, FALSE );
		wListSetIndex( fontListB, -1 );
	}
	if ( fontSelectMode == 0 ) {
		wControlActive( (wControl_p)fontWeightB, 
			fontInfo(newFontInx).fullName[1-newFontWeight][newFontSlant] != NULL);
		wControlActive( (wControl_p)fontSlantB, 
			fontInfo(newFontInx).fullName[newFontWeight][1-newFontSlant] != NULL);
	}
	oldFontWeight = newFontWeight;
	oldFontSlant = newFontSlant;
	fontRedraw( font_d, NULL, 0, 0 );
}


static void selectFace( wIndex_t index, const char * name, wIndex_t junk3, void * listData, void * itemData )
{
	newFontInx = (int)itemData;
	fontRedraw( font_d, NULL, 0, 0 );
	wControlActive( (wControl_p)fontOkB, TRUE );
	if ( fontSelectMode == 0 ) {
		wControlActive( (wControl_p)fontWeightB, 
			fontInfo(newFontInx).fullName[1-newFontWeight][newFontSlant] != NULL);
		wControlActive( (wControl_p)fontSlantB, 
			fontInfo(newFontInx).fullName[newFontWeight][1-newFontSlant] != NULL);
	}
}


static wBool_t addFont( const char * faceName, long fw, long fs, const char * fullName )
{
	int i, rc;

	for (i=0;i<fontInfo_da.cnt;i++)
		if ((rc=strcasecmp( fontInfo(i).faceName, faceName )) == 0) {
			if (fontInfo(i).fullName[fw][fs]) {
				if (wDebugFont >= 1)
					fprintf(stderr,"dup font %s %s\n",
						fontInfo(i).fullName[fw][fs], fullName );
				return FALSE;
			}
			goto found;
		}
	DYNARR_ADD( fontInfo_t, fontInfo_da, 10 );
	i = fontInfo_da.cnt-1;
	fontInfo(i).faceName = strdup( faceName );
	memset( &fontInfo(i).fullName, 0, sizeof ((fontInfo_t*)NULL)->fullName );
found:
	fontInfo(i).fullName[fw][fs] = strdup( fullName );
	return TRUE;
}

static int cmpFontName( const void * a, const void * b )
{
	return strcasecmp( ((fontInfo_t*)a)->faceName, ((fontInfo_t*)b)->faceName );
}


void findFont( const char * fontName )
{
	int f, w, s;
	if ( fontName == NULL )
		return;
	for ( f=0; f<fontInfo_da.cnt; f++ )
		for ( s=0; s<2; s++ )
			for ( w=0; w<2; w++ )
				if ( fontInfo(f).fullName[w][s] &&
					 strcmp( fontInfo(f).fullName[w][s], fontName ) == 0 ) {
					curFontInx = f;
					curFontWeight = w;
					curFontSlant = s;
					return;
				}
}


static wBool_t fontInitted = FALSE;

static wBool_t fontInit( wBool_t getPref )
{
	char ** fonts, *fn, *fw, *fs;
	char fontName[80], fullName[80];
	int count, i;
	long ifw, ifs;
	FILE * f=NULL;
	long stdFontInx;
	const char * stdSerifName;
	const char * stdSanserifName;

	stdSerifName = wPrefGetString( "gtkfont", "serif" );
	stdSanserifName = wPrefGetString( "gtkfont", "sanserif" );
	fontInitted = TRUE;
	black = wDrawFindColor( 0 );
	if (wDebugFont >= 2)
		f = fopen( "fonts.lst", "w" );
	fonts = XListFonts( gdk_display, "-*-*-*-*-*-*-0-0-*-*-*-0-iso8859-*", 1000, &count );
	for (i=0;i<count;i++) {
		if (wDebugFont >= 2)
			fprintf( f, "%s\n", fonts[i]+1 );
		strncpy( fontName, fonts[i]+1, sizeof fontName );
		strtok( fontName, "-" );
		fn = strtok( NULL, "-" );
		fw = strtok( NULL, "-" );
		fs = strtok( NULL, "-" );
		if ((!fn) || (!fw) || (!fs))
			continue;
		if (strcasecmp( fw, "medium" ) == 0 )
			ifw = FW_MEDIUM;
		else if (strcasecmp( fw, "bold" ) == 0 )
			ifw = FW_BOLD;
		else if (strcasecmp( fw, "demibold" ) == 0 )
			ifw = FW_BOLD;
		else if (strcasecmp( fw, "light" ) == 0 )
			ifw = FW_MEDIUM;
		else {
			if (wDebugFont >= 1)
				fprintf( stderr, "Unknown font weight: %s\n", fw );
			continue;
		}
		if (strcasecmp( fs, "r" ) == 0 )
			ifs = FS_REGULAR;
		else if (strcasecmp( fs, "o" ) == 0 )
			ifs = FS_ITALIC;
		else if (strcasecmp( fs, "i" ) == 0 )
			ifs = FS_ITALIC;
		else {
			if (wDebugFont >= 1)
				fprintf( stderr, "Unknown font slant: %s\n", fs );
			continue;
		}
		sprintf( fullName, "%s-%s-%s", fn, fw, fs );
		addFont( fn, ifw, ifs, fullName );
	}
	if (wDebugFont >= 2)
		fclose(f);
	XFreeFontNames( fonts );
	qsort( fontInfo_da.ptr, fontInfo_da.cnt, sizeof *(fontInfo_t*)NULL, cmpFontName );/**/

	standardFonts[F_TIMES] = -1;
	standardFonts[F_HELV] = -1;
	stdFontInx = -1;
	for ( i=0;i<fontInfo_da.cnt;i++ ) {
		if (strcmp(fontInfo(i).faceName, stdSerifName?stdSerifName:"times")==0) {
			curFontInx = newFontInx = i;
			standardFonts[F_TIMES] = i;
		} else if ( strcmp(fontInfo(i).faceName, stdSanserifName?stdSanserifName:"helvetica") == 0 )
			standardFonts[F_HELV] = i;
		if ( stdFontInx < 0 &&
			 fontInfo(i).fullName[0][0] != NULL )
			stdFontInx = i;
	}
	if ( standardFonts[F_TIMES] < 0 ) {
		wNotice( "Can't find standard Serif font.\nPlease choose a font", "Continue", NULL );
		wSelectStandardFont( F_TIMES );
	}
	if ( standardFonts[F_HELV] < 0 ) {
		wNotice( "Can't find standard San-Serif font.\nPlease choose a font", "Continue", NULL );
		wSelectStandardFont( F_HELV );
	}
	findFont( wPrefGetString( "font", "name" ) );
	wPrefGetInteger( "font", "size", &fontSize, fontSize );
	return TRUE;
}

void wSelectStandardFont( int fontNum )
{
	long oldFontInx = curFontInx;
	long oldFontWeight = curFontWeight;
	long oldFontSlant = curFontSlant;
	int inx;

	curFontInx = 0;
	for ( inx=0; inx<fontInfo_da.cnt; inx++ ) {
		if ( fontInfo(inx).fullName[0][0] != NULL ) {
			curFontInx = inx;
			break;
		}
	}
	curFontWeight = 0;
	curFontSlant = 0;
	fontSelectMode = 1;
	wSelectFont(fontNum==F_TIMES?"Standard Serif Font":"Standard San-Serif Font");
	standardFonts[fontNum] = curFontInx;
	wPrefSetString( "gtkfont", fontNum==F_TIMES?"serif":"sanserif", fontInfo(curFontInx).faceName );
	curFontInx = oldFontInx;
	curFontWeight = oldFontWeight;
	curFontSlant = oldFontSlant;
	fontSelectMode = 0;
}

void fontToggleWeightButton( void* junk )
{
	newFontWeight = !newFontWeight;
	wButtonSetBusy( fontWeightB, newFontWeight != 0 );
	selectAttr();
}


void fontToggleSlantButton( void* junk )
{
	newFontSlant = !newFontSlant;
	wButtonSetBusy( fontSlantB, newFontSlant != 0 );
	selectAttr();
}

#include "bold.bmp"
#include "italic.bmp"


void wSelectFont(
	const char * title )
{
	int i;
	wPos_t x, y;
	wIcon_p fontWeightBM, fontSlantBM;
	if (!fontInitted)
		fontInit( FALSE );
	if (fontSelW == NULL) {
		fontSelW = wWinPopupCreate( NULL, 2, 2, "fontSelW", "Font Select", "xvfontsel", F_AUTOSIZE|F_RECALLPOS|F_BLOCK, NULL, NULL );

		fontWeightBM = wIconCreateBitMap( bold_width, bold_height, bold_bits, wDrawColorBlack );
		fontWeightB = wButtonCreate( fontSelW,	2, 2, "fontSelWeight", (const char*)fontWeightBM, BO_ICON, 0, fontToggleWeightButton, NULL );
		fontSlantBM = wIconCreateBitMap( italic_width, italic_height, italic_bits, wDrawColorBlack );
		fontSlantB = wButtonCreate( fontSelW,	-4, 2, "fontSelSlant", (const char*)fontSlantBM, BO_ICON, 0, fontToggleSlantButton, NULL );
		fontSizeB = wIntegerCreate( fontSelW,	-4, 2, "fontSelSize", NULL, 0,
								80, 1, 100, &fontSize, (wIntegerCallBack_p)selectAttr, NULL );

		fontListB = wDropListCreate( fontSelW, 2, -4, "fontSelList", NULL, 0,
								10, 185, NULL, selectFace, NULL );
		x = 2 + wControlGetWidth( (wControl_p)fontListB ) + 10;

		fontOkB = wButtonCreate( fontSelW, x, 2, "fontSelOk", "Ok", 2, 0, (wButtonCallBack_p)doFontOk, NULL );
		fontCancelB = wButtonCreate( fontSelW, x, -4, "fontSelCancel", "Cancel", 0, 0, (wButtonCallBack_p)doFontCancel, NULL );
		x += wControlGetWidth( (wControl_p)fontOkB );
		y = wControlGetPosY( (wControl_p)fontListB ) + wControlGetHeight( (wControl_p)fontListB ) + 4;
		font_d = wDrawCreate( fontSelW, 2, y, "fontSelSample", 0, x, 50, NULL, fontRedraw, NULL );

		for (i=0;i<fontInfo_da.cnt;i++) {
			wListAddValue( fontListB, fontInfo(i).faceName, NULL, (void*)i );
			if (fontInfo(i).fullName[0][0] == NULL)
				wListSetActive( fontListB, i, FALSE );
		}
	}
	wControlActive( (wControl_p)fontCancelB, fontSelectMode==0 );
	wControlActive( (wControl_p)fontSizeB, fontSelectMode==0 );
	wControlActive( (wControl_p)fontWeightB, fontSelectMode==0 );
	wControlActive( (wControl_p)fontSlantB, fontSelectMode==0 );
	if (fontInfo_da.cnt == 0) {
		wNotice( "No fonts", "Continue", NULL);
		return;
	}
	newFontInx = curFontInx;
	newFontWeight = curFontWeight;
	newFontSlant = curFontSlant;
	wButtonSetBusy( fontWeightB, newFontWeight!=0 );
	wButtonSetBusy( fontSlantB, newFontSlant!=0 );
	selectAttr();
	wWinSetTitle( fontSelW, title );
	wWinShow( fontSelW, TRUE );
}


static const char * wCurFont( void )
{
	if (!fontInitted)
		fontInit( FALSE );
	if (fontInfo_da.cnt == 0 ||
		fontInfo(curFontInx).fullName[curFontWeight][curFontSlant] == NULL)
		return "times-medium-r";
	return fontInfo(curFontInx).fullName[curFontWeight][curFontSlant];
}


wFontSize_t wSelectedFontSize( void )
{
	return (wFontSize_t) fontSize;
}


const char * gtkFontTranslate(
		wFont_p fp )
{
	if (fp == NULL)
		return wCurFont();
	return fontInfo(fp->fi).fullName[fp->bold][fp->italic];
}


wFont_p wStandardFont( int face, wBool_t bold, wBool_t italic )
{
	wFont_p f;
	if (!fontInitted)
		fontInit( FALSE );
	if (fontInfo(standardFonts[face]).fullName[bold][italic]) {
		f = &(fontInfo(standardFonts[face]).font[bold][italic]);
		f->fi = standardFonts[face];
		f->bold = bold;
		f->italic = italic;
	} else if (fontInfo(standardFonts[face]).fullName[FALSE][FALSE]) {
		f = &(fontInfo(standardFonts[face]).font[FALSE][FALSE]);
		f->fi = standardFonts[face];
		f->bold = FALSE;
		f->italic = FALSE;
	} else {
		f = &(fontInfo(0).font[FALSE][FALSE]);
		f->fi = 0;
		f->bold = FALSE;
		f->italic = FALSE;
	}
	return f;
}

int wFontClock( void )
{
	return fontClock;
}

#ifdef TEST
void doSel( wButton_p b )
{
	wSelectFont("Test");
}

void Init( INT_T argc, char * argv[] )
{
}
#endif
