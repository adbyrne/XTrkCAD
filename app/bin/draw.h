/** \file draw.h
 * Definitions and prototypes for drawing operations
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

#ifndef DRAW_H
#define DRAW_H

#include "common.h"

// drawCmd_t.options
//
// SIMPLE: draw simplified objects.
// 	No endpts, descriptions, wide lines and arcs, ties, centerlines, special color
//	Draw simple lines for: wide lines and arcs, dimlines, benchwork, tableedge, filled poly andcircle
#define DC_SIMPLE		(1<<0)
// SEGTRACK: draw tracks as segments (SEG_*TRK) instead of lines and arcs
#define DC_SEGTRACK		(1<<1)
// PRINT: we're printing
#define DC_PRINT		(1<<2)
#define DC_NOCLIP		(1<<3)
// CENTERLINE: draw centerlines (for bitmaps)
#define DC_CENTERLINE		(1<<4)
// TICKS: draw rulers on edges
#define DC_TICKS		(1<<5)
// TEMP: temp mode draws
#define DC_TEMP			(1<<6)
// OUTLINE: use outline font
#define DC_OUTLINE		(1<<7)
// Round pixel pos for performance
#define DC_ROUND		(1<<8)

// Line styles
#define DC_THICK        	(1<<9)
#define DC_DASH			(1<<12)
#define DC_DOT          	(1<<13)
#define DC_DASHDOT      	(1<<14)
#define DC_DASHDOTDOT   	(1<<15)
#define DC_CENTER			(1<<16)
#define DC_PHANTOM          (1<<17)

#define DC_NOTSOLIDLINE (DC_DASH|DC_DOT|DC_DASHDOT|DC_DASHDOTDOT|DC_CENTER|DC_PHANTOM)

#define INIT_MAIN_SCALE (8.0)
#define INIT_MAP_SCALE	(64.0)
#define MAX_MAIN_SCALE	(1024.0)
#define MIN_MAIN_SCALE	(1.0)
#define MIN_MAIN_MACRO  (0.10)

typedef enum { DRAW_OPEN, DRAW_CLOSED, DRAW_FILL, DRAW_TRANSPARENT } drawFill_e;

struct drawCmd_t;
typedef struct drawCmd_t * drawCmd_p;

typedef struct {
	void (*drawLine)(drawCmd_p, coOrd, coOrd, wDrawWidth, wDrawColor);
	void (*drawArc)(drawCmd_p, coOrd, DIST_T, ANGLE_T, ANGLE_T, BOOL_T, wDrawWidth,
	                wDrawColor);
	void (*drawString)(drawCmd_p, coOrd, ANGLE_T, char *, wFont_p, FONTSIZE_T,
	                   wDrawColor);
	void (*drawBitMap)(drawCmd_p, coOrd, wDrawBitMap_p, wDrawColor);
	void (*drawPoly)(drawCmd_p, int, coOrd *, int *, wDrawColor, wDrawWidth,
	                 drawFill_e);
	void (*drawFillCircle)(drawCmd_p, coOrd, DIST_T,  wDrawColor);
	void (*drawRectangle)(drawCmd_p, coOrd, coOrd, wDrawColor, drawFill_e);
} drawFuncs_t;

typedef void (*drawConvertPix2CoOrd)(drawCmd_p, wDrawPix_t, wDrawPix_t,
                                     coOrd *);
typedef void (*drawConvertCoOrd2Pix)(drawCmd_p, coOrd, wDrawPix_t *,
                                     wDrawPix_t *);
typedef struct drawCmd_t {
	wDraw_p d;
	drawFuncs_t * funcs;
	unsigned long options;
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

void Pix2CoOrd(drawCmd_p, wDrawPix_t, wDrawPix_t, coOrd *);
void CoOrd2Pix(drawCmd_p, coOrd, wDrawPix_t *, wDrawPix_t *);

extern BOOL_T inError;
extern DIST_T pixelBins;
extern wWin_p mapW;
extern BOOL_T mapVisible;
extern BOOL_T magneticSnap;
extern drawCmd_t mainD;
extern coOrd mainCenter;
extern drawCmd_t mapD;
extern drawCmd_t tempD;
#define RoomSize (mapD.size)
extern coOrd oldMarker;
#define dragDistance	(dragPixels*mainD.scale / mainD.dpi)
extern long dragPixels;
extern long dragTimeout;
extern long autoPan;
extern long minGridSpacing;
extern long drawCount;
extern BOOL_T drawEnable;
extern long currRedraw;
extern long constrainMain;
extern long liveMap;
extern long descriptionFontSize;

extern coOrd panCenter;
extern coOrd menuPos;

extern int log_pan;

extern wBool_t wDrawDoTempDraw;

extern wDrawColor drawColorBlack;
extern wDrawColor drawColorWhite;
extern wDrawColor drawColorRed;
extern wDrawColor drawColorBlue;
extern wDrawColor drawColorGreen;
extern wDrawColor drawColorAqua;
extern wDrawColor drawColorDkRed;
extern wDrawColor drawColorDkBlue;
extern wDrawColor drawColorDkGreen;
extern wDrawColor drawColorDkAqua;
extern wDrawColor drawColorPowderedBlue;
extern wDrawColor drawColorPurple;
extern wDrawColor drawColorMagenta;
extern wDrawColor drawColorGold;
extern wDrawColor drawColorGrey10;
extern wDrawColor drawColorGrey20;
extern wDrawColor drawColorGrey30;
extern wDrawColor drawColorGrey40;
extern wDrawColor drawColorGrey50;
extern wDrawColor drawColorGrey60;
extern wDrawColor drawColorGrey70;
extern wDrawColor drawColorGrey80;
extern wDrawColor drawColorGrey90;
// Special colors
extern wDrawColor drawColorPreviewSelected;
extern wDrawColor drawColorPreviewUnselected;

#define wDrawColorBlack drawColorBlack
#define wDrawColorWhite drawColorWhite
#define wDrawColorBlue  drawColorBlue
#define wDrawColorPowderedBlue drawColorPowderedBlue
#define wDrawColorAqua  drawColorAqua
#define wDrawColorRed	drawColorRed
#define wDrawColorGrey10 drawColorGrey10
#define wDrawColorGrey20 drawColorGrey20
#define wDrawColorGrey30 drawColorGrey30
#define wDrawColorGrey40 drawColorGrey40
#define wDrawColorGrey50 drawColorGrey50
#define wDrawColorGrey60 drawColorGrey60
#define wDrawColorGrey70 drawColorGrey70
#define wDrawColorGrey80 drawColorGrey80
#define wDrawColorGrey90 drawColorGrey90
#define wDrawColorPreviewSelected drawColorPreviewSelected
#define wDrawColorPreviewUnselected drawColorPreviewUnselected

extern wDrawColor markerColor;
extern wDrawColor borderColor;
extern wDrawColor crossMajorColor;
extern wDrawColor crossMinorColor;
extern wDrawColor snapGridColor;
extern wDrawColor selectedColor;
extern wDrawColor profilePathColor;

BOOL_T IsClose(DIST_T);

extern drawFuncs_t screenDrawFuncs;
extern drawFuncs_t tempDrawFuncs;
extern drawFuncs_t tempSegDrawFuncs;
extern drawFuncs_t printDrawFuncs;

#define DrawLine( D, P0, P1, W, C ) (D)->funcs->drawLine( D, P0, P1, W, C )
#define DrawArc( D, P, R, A0, A1, F, W, C ) (D)->funcs->drawArc( D, P, R, A0, A1, F, W, C )
#define DrawString( D, P, A, S, FP, FS, C ) (D)->funcs->drawString( D, P, A, S, FP, FS, C )
#define DrawBitMap( D, P, B, C ) (D)->funcs->drawBitMap( D, P, B, C )
#define DrawPoly( D, N, P, T, C, W, O ) (D)->funcs->drawPoly( D, N, P, T, C, W, O );
#define DrawFillCircle( D, P, R, C ) (D)->funcs->drawFillCircle( D, P, R, C );
#define DrawRectangle( D, P, S, C, O ) (D)->funcs->drawRectangle( D, P, S, C, O )

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

void DrawHilight(drawCmd_p, coOrd, coOrd, BOOL_T add);
void DrawHilightPolygon(drawCmd_p, coOrd *, int);
#define BOX_NONE		(0)         // do not draw a frame around text
#define BOX_UNDERLINE	(1)         // draw underline under text only
#define BOX_BOX			(2)         // draw a frame around text
#define BOX_INVERT		(3)         // invert colors, text is drawn gray
#define BOX_ARROW		(4)         // box has a connector
#define BOX_BACKGROUND	(5)         // draw box with backgound only, no frame    
#define BOX_ARROW_BACKGROUND (6)    // box has a connector and background
#define BOX_BOX_BACKGROUND (7)      // draw complete frame and background

void DrawBoxedString(int, drawCmd_p, coOrd, char *, wFont_p, wFontSize_t,
                     wDrawColor, ANGLE_T);
void DrawMultiLineTextSize(drawCmd_p dp, char * text, wFont_p fp,
                           wFontSize_t fs, BOOL_T relative, coOrd * size, coOrd * lastline);
void DrawTextSize2(drawCmd_p, char *, wFont_p, wFontSize_t, BOOL_T, coOrd *,
                   POS_T *, POS_T *);
void DrawTextSize(drawCmd_p, char *, wFont_p, wFontSize_t, BOOL_T, coOrd *);
void DrawMultiString(drawCmd_p d, coOrd pos, char * text, wFont_p fp,
                     wFontSize_t fs, wDrawColor color, ANGLE_T a, coOrd * lo, coOrd * hi,
                     BOOL_T boxed);
void TranslateBackground(drawCmd_p drawP, POS_T origX, POS_T origY,
                         wWinPix_t* posX,
                         wWinPix_t* posY, wWinPix_t* pWidth);
BOOL_T SetRoomSize(coOrd);
void DoRedraw(void);
void SetMainSize(void);
void MainRedraw(void);
void MainLayout(wBool_t, wBool_t);
void TempRedraw(void);
void DrawRuler(drawCmd_p, coOrd, coOrd, DIST_T, int, int, wDrawColor);
void MainProc(wWin_p, winProcEvent, void *, void *);
void InitInfoBar(void);
void InitColor(void);
void DrawInit(int);
void DoZoomUp(void * modeVP);
void DoZoomDown(void * modeVP);
void DoZoomExtents( void * modeVP);
void PanHere(void * modeVP);
void PanMenuEnter(void * modeVP);

void InitCmdZoom(wMenu_p, wMenu_p, wMenu_p, wMenu_p);

void InfoPos(coOrd);
void InfoCount(wIndex_t);
void SetMessage(char *);

extern wIndex_t panCmdInx;

void InfoSubstituteControls(wControl_p *, char * *);

void MapGrid(coOrd, coOrd, ANGLE_T, coOrd, ANGLE_T, POS_T, POS_T, int *, int *,
             int *, int *);
void DrawGrid(drawCmd_p, coOrd *, POS_T, POS_T, long, long, coOrd, ANGLE_T,
              wDrawColor, BOOL_T);
STATUS_T GridAction(wAction_t, coOrd, coOrd *, DIST_T *);

void ResetMouseState(void);
void FakeDownMouseState(void);
void GetMousePosition(wDrawPix_t  *x, wDrawPix_t *y);
void RecordMouse(char *, wAction_t, POS_T, POS_T);
extern long playbackDelay;
void MovePlaybackCursor(drawCmd_p, coOrd pos, wBool_t, wControl_p);
typedef void (*playbackProc)(wAction_t, coOrd);
void PlaybackMouse(playbackProc, drawCmd_p, wAction_t, coOrd, wDrawColor);
void RedrawPlaybackCursor();
#endif
