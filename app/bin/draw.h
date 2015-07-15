/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/draw.h,v 1.4 2008-10-11 06:03:06 m_fischer Exp $
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

#ifndef DRAW_H
#define DRAW_H

#define MSG_BASE (1000)
#include "messages.h"

#define DC_TICKS		(1<<1)
#define DC_PRINT		(1<<2)
#define DC_NOCLIP		(1<<3)
#define DC_QUICK		(1<<4)
#define DC_DASH			(1<<5)
#define DC_SIMPLE		(1<<6)
#define DC_GROUP		(1<<7)
#define DC_CENTERLINE	(1<<8)
#define DC_SEGTRACK		(1<<9)
#define DC_TIES			(1<<10)

typedef struct drawCmd_t * drawCmd_p;

typedef struct {
		long options;
		void (*drawLine)( drawCmd_p, coOrd, coOrd, wDrawWidth, wDrawColor );
		void (*drawArc)( drawCmd_p, coOrd, DIST_T, ANGLE_T, ANGLE_T, BOOL_T, wDrawWidth, wDrawColor );
		void (*drawString)( drawCmd_p, coOrd, ANGLE_T, char *, wFont_p, FONTSIZE_T, wDrawColor );
		void (*drawBitMap)( drawCmd_p, coOrd, wDrawBitMap_p, wDrawColor );
		void (*drawFillPoly) (drawCmd_p, int, coOrd *, wDrawColor );
		void (*drawFillCircle) (drawCmd_p, coOrd, DIST_T,  wDrawColor );
		} drawFuncs_t;

typedef void (*drawConvertPix2CoOrd)( drawCmd_p, wPos_t, wPos_t, coOrd * );
typedef void (*drawConvertCoOrd2Pix)( drawCmd_p, coOrd, wPos_t *, wPos_t * );
typedef struct drawCmd_t {
		wDraw_p d;
		drawFuncs_t * funcs;
		long options;
		DIST_T scale;
		ANGLE_T angle;
		coOrd orig;
		coOrd size;
		drawConvertPix2CoOrd Pix2CoOrd;
		drawConvertCoOrd2Pix CoOrd2Pix;
		FLOAT_T dpi;
		} drawCmd_t;

#define SCALEX(D,X)		((X)/(D).dpi)
#define SCALEY(D,Y)		((Y)/(D).dpi)

#ifdef WINDOWS
#define LBORDER (33)
#define BBORDER (32)
#else
#define LBORDER (26)
#define BBORDER (27)
#endif
#define RBORDER (9)
#define TBORDER (8)

#ifdef LATER
#define Pix2CoOrd( D, pos, X, Y ) { \
	pos.x = ((long)(((POS_T)((X)-LBORDER)*pixelBins)/D.dpi))/pixelBins * D.scale + D.orig.x; \
	pos.y = ((long)(((POS_T)((Y)-BBORDER)*pixelBins)/D.dpi))/pixelBins * D.scale + D.orig.y; \
	}
#endif
void Pix2CoOrd( drawCmd_p, wPos_t, wPos_t, coOrd * );
void CoOrd2Pix( drawCmd_p, coOrd, wPos_t *, wPos_t * );

extern BOOL_T inError;
extern DIST_T pixelBins;
extern wWin_p mapW;
extern BOOL_T mapVisible;
extern drawCmd_t mainD;
extern coOrd mainCenter;
extern drawCmd_t mapD;
extern drawCmd_t tempD;
#define RoomSize (mapD.size)
extern coOrd oldMarker;
extern wPos_t closePixels;
#define dragDistance	(dragPixels*mainD.scale / mainD.dpi)
extern long dragPixels;
extern long dragTimeout;
extern long autoPan;
extern long minGridSpacing;
extern long drawCount;
extern BOOL_T drawEnable;
extern long currRedraw;

extern wDrawColor drawColorBlack;
extern wDrawColor drawColorWhite;
extern wDrawColor drawColorRed;
extern wDrawColor drawColorBlue;
extern wDrawColor drawColorGreen;
extern wDrawColor drawColorAqua;
extern wDrawColor drawColorPurple;
extern wDrawColor drawColorGold;
#define wDrawColorBlack drawColorBlack
#define wDrawColorWhite drawColorWhite
#define wDrawColorBlue  drawColorBlue

extern wDrawColor markerColor;
extern wDrawColor borderColor;
extern wDrawColor crossMajorColor;
extern wDrawColor crossMinorColor;
extern wDrawColor snapGridColor;
extern wDrawColor selectedColor;
extern wDrawColor profilePathColor;

BOOL_T IsClose( DIST_T );

drawFuncs_t screenDrawFuncs;
drawFuncs_t tempDrawFuncs;
drawFuncs_t tempSegDrawFuncs;
drawFuncs_t printDrawFuncs;

#define DrawLine( D, P0, P1, W, C ) (D)->funcs->drawLine( D, P0, P1, W, C )
#define DrawArc( D, P, R, A0, A1, F, W, C ) (D)->funcs->drawArc( D, P, R, A0, A1, F, W, C )
#define DrawString( D, P, A, S, FP, FS, C ) (D)->funcs->drawString( D, P, A, S, FP, FS, C )
#define DrawBitMap( D, P, B, C ) (D)->funcs->drawBitMap( D, P, B, C )
#define DrawFillPoly( D, N, P, C ) (D)->funcs->drawFillPoly( D, N, P, C );
#define DrawFillCircle( D, P, R, C ) (D)->funcs->drawFillCircle( D, P, R, C );

#define REORIGIN( Q, P, A, O ) { \
		(Q) = (P); \
		REORIGIN1( Q, A, O ) \
		}
#define REORIGIN1( Q, A, O ) { \
		if ( (A) != 0.0 ) \
			Rotate( &(Q), zero, (A) ); \
		(Q).x += (O).x; \
		(Q).y += (O).y; \
	}
#define OFF_D( ORIG, SIZE, LO, HI ) \
		( (HI).x < (ORIG).x || \
		  (LO).x > (ORIG).x+(SIZE).x || \
		  (HI).y < (ORIG).y || \
		  (LO).y > (ORIG).y+(SIZE).y )
#define OFF_MAIND( LO, HI ) \
		OFF_D( mainD.orig, mainD.size, LO, HI )

void DrawHilight( drawCmd_p, coOrd, coOrd );
void DrawHilightPolygon( drawCmd_p, coOrd *, int );
#define BOX_NONE		(0)
#define BOX_UNDERLINE	(1)
#define BOX_BOX			(2)
#define BOX_INVERT		(3)
#define BOX_ARROW		(4)
#define BOX_BACKGROUND	(5)
void DrawBoxedString( int, drawCmd_p, coOrd, char *, wFont_p, wFontSize_t, wDrawColor, ANGLE_T );
void DrawTextSize2( drawCmd_p, char *, wFont_p, wFontSize_t, BOOL_T, coOrd *, POS_T * );
void DrawTextSize( drawCmd_p, char *, wFont_p, wFontSize_t, BOOL_T, coOrd * );
BOOL_T SetRoomSize( coOrd );
void GetRoomSize( coOrd * );
void DoRedraw( void );
void SetMainSize( void );
void MainRedraw( void );
void DrawMarkers( void );
void DrawMapBoundingBox( BOOL_T );
void DrawTicks( drawCmd_p, coOrd );
void DrawRuler( drawCmd_p, coOrd, coOrd, DIST_T, int, int, wDrawColor );
void MainProc( wWin_p, winProcEvent, void * );
void InitInfoBar( void );
void DrawInit( int );
void DoZoomUp( void * );
void DoZoomDown( void * );
void DoZoom( DIST_T * );

void InitCmdZoom( wMenu_p, wMenu_p );

void InfoPos( coOrd );
void InfoCount( wIndex_t );
void SetMessage( char * );

void InfoSubstituteControls( wControl_p *, char * * );

void MapGrid( coOrd, coOrd, ANGLE_T, coOrd, ANGLE_T, POS_T, POS_T, int *, int *, int *, int * );
void DrawGrid( drawCmd_p, coOrd *, POS_T, POS_T, long, long, coOrd, ANGLE_T, wDrawColor, BOOL_T );
STATUS_T GridAction( wAction_t, coOrd, coOrd *, DIST_T * );

void ResetMouseState( void );
void FakeDownMouseState( void );
void GetMousePosition( int  *x, int *y );
void RecordMouse( char *, wAction_t, POS_T, POS_T );
extern long playbackDelay;
void MovePlaybackCursor( drawCmd_p, wPos_t, wPos_t );
typedef void (*playbackProc)( wAction_t, coOrd );
void PlaybackMouse( playbackProc, drawCmd_p, wAction_t, coOrd, wDrawColor );
#endif
