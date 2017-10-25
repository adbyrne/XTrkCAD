/** \file cprint.c
 * Printing functions. 
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

#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "custom.h"
#include "fileio.h"
#include "i18n.h"
#include "layout.h"
#include "messages.h"
#include "param.h"
#include "track.h"
#include "utility.h"

#define PRINT_GAUDY		(0)
#define PRINT_PLAIN		(1)
#define PRINT_BARE		(2)
#define PORTRAIT		(0)
#define LANDSCAPE		(1)

#define PRINTOPTION_SNAP		(1<<0)

typedef struct {
		int x0, x1, y0, y1;
		char * bm;
		int memsize;
		coOrd orig;
		coOrd size;
		ANGLE_T angle;
		} bitmap_t;
static bitmap_t bm, bm0;
#define BITMAP( BM, X, Y ) \
		(BM).bm[ (X)-(BM).x0 + ((Y)-(BM).y0) * ((BM).x1-(BM).x0) ]

struct {
		coOrd size;
		coOrd orig;
		ANGLE_T angle;
		} currPrintGrid, newPrintGrid;


/*
 * GUI VARS
 */


static long printGaudy = 1;
static long printRegistrationMarks = 1;
static long printPhysSize = FALSE;
static long printFormat = PORTRAIT;
static long printOrder = 0;
static long printGrid = 0;
static long printRuler = 0;
static long printRoadbed = 0;
static DIST_T printRoadbedWidth = 0.0;
static BOOL_T printRotate = FALSE;
static BOOL_T rotateCW = FALSE;
static long printCenterLine = 0;

static double printScale = 16;
static long iPrintScale = 16;
static coOrd maxPageSize;
static coOrd realPageSize;

static wWin_p printWin;

static wMenu_p printGridPopupM;

static wIndex_t pageCount = 0;

static int log_print = 0;

static void PrintSnapShot( void );
static void DoResetGrid( void );
static void DoPrintSetup( void );
static void PrintClear( void );
static void PrintMaxPageSize( void );

static char * printFormatLabels[] = { N_("Portrait"), N_("Landscape"), NULL };
static char * printOrderLabels[] = { N_("Normal"), N_("Reverse"), NULL };
static char * printGaudyLabels[] = { N_("Engineering Data"), NULL };
static char * printRegistrationMarksLabels[] = { N_("Print Registration Marks"), NULL };
static char * printPhysSizeLabels[] = { N_("Ignore Page Margins"), NULL };
static char * printGridLabels[] = { N_("Print Snap Grid"), NULL };
static char * printRulerLabels[] = { N_("Print Rulers"), NULL };
static char * printRoadbedLabels[] = { N_("Print Roadbed Outline"), NULL };
static char * printCenterLineLabels[] = { N_("Print Centerline below Scale 1:1"), NULL };
static paramIntegerRange_t rminScale_999 = { 1, 999, 0, PDO_NORANGECHECK_HIGH };
static paramFloatRange_t r0_ = { 0, 0, 0, PDO_NORANGECHECK_HIGH };
static paramFloatRange_t r1_ = { 1, 0, 0, PDO_NORANGECHECK_HIGH };
static paramFloatRange_t r_10_99999 = { -10, 99999, 0, PDO_NORANGECHECK_HIGH };
static paramFloatRange_t r0_360 = { 0, 360 };

static paramData_t printPLs[] = {
/*0*/ { PD_LONG, &iPrintScale, "scale", 0, &rminScale_999, N_("Print Scale"), 0, (void*)1 },
/*1*/ { PD_FLOAT, &newPrintGrid.size.x, "pagew", PDO_DIM|PDO_SMALLDIM|PDO_NORECORD|PDO_NOPREF, &r1_, N_("Page Width"), 0, (void*)2 },
/*2*/ { PD_BUTTON, (void*)PrintMaxPageSize, "max", PDO_DLGHORZ, NULL, N_("Max") },
/*3*/ { PD_FLOAT, &newPrintGrid.size.y, "pageh", PDO_DIM|PDO_SMALLDIM|PDO_NORECORD|PDO_NOPREF, &r1_, N_("Height"), 0, (void*)2 },
/*4*/ { PD_BUTTON, (void*)PrintSnapShot, "snapshot", PDO_DLGHORZ, NULL, N_("Snap Shot") },
/*5*/ { PD_RADIO, &printFormat, "format", 0, printFormatLabels, N_("Page Format"), BC_HORZ|BC_NOBORDER, (void*)1 },
/*6*/ { PD_RADIO, &printOrder, "order", PDO_DLGBOXEND, printOrderLabels, N_("Print Order"), BC_HORZ|BC_NOBORDER },

/*7*/ { PD_TOGGLE, &printGaudy, "style", PDO_DLGNOLABELALIGN, printGaudyLabels, NULL, BC_HORZ|BC_NOBORDER, (void*)1 },
/*8*/ { PD_TOGGLE, &printPhysSize, "physsize", PDO_DLGNOLABELALIGN, printPhysSizeLabels, NULL, BC_HORZ|BC_NOBORDER, (void*)1 },
#define I_REGMARKS		(9)
/*9*/ { PD_TOGGLE, &printRegistrationMarks, "registrationMarks", PDO_DLGNOLABELALIGN, printRegistrationMarksLabels, NULL, BC_HORZ|BC_NOBORDER },
#define I_GRID			(10)
/*10*/ { PD_TOGGLE, &printGrid, "grid", PDO_DLGNOLABELALIGN, printGridLabels, NULL, BC_HORZ|BC_NOBORDER },
#define I_RULER			(11)
/*11*/ { PD_TOGGLE, &printRuler, "ruler", PDO_DLGNOLABELALIGN, printRulerLabels, NULL, BC_HORZ|BC_NOBORDER },
#define I_CENTERLINE    (12)
/*12*/ { PD_TOGGLE, &printCenterLine, "centerLine", PDO_DLGNOLABELALIGN, printCenterLineLabels, NULL, BC_HORZ|BC_NOBORDER },
#define I_ROADBED		(13)
/*13*/{ PD_TOGGLE, &printRoadbed, "roadbed", PDO_DLGNOLABELALIGN, printRoadbedLabels, NULL, BC_HORZ|BC_NOBORDER },
#define I_ROADBEDWIDTH	(14)
/*14*/{ PD_FLOAT, &printRoadbedWidth, "roadbedWidth", PDO_DIM|PDO_DLGBOXEND, &r0_, N_("Width") },
/*15*/{ PD_FLOAT, &newPrintGrid.orig.x, "origx", PDO_DIM|PDO_DLGRESETMARGIN, &r_10_99999, N_("Origin: X"), 0, (void*)2 },
/*16*/ { PD_FLOAT, &newPrintGrid.orig.y, "origy", PDO_DIM, &r_10_99999, N_("Y"), 0, (void*)2 },
/*17*/ { PD_BUTTON, (void*)DoResetGrid, "reset", PDO_DLGHORZ, NULL, N_("Reset") },
/*18*/ { PD_FLOAT, &newPrintGrid.angle, "origa", PDO_ANGLE|PDO_DLGBOXEND, &r0_360, N_("Angle"), 0, (void*)2 },
/*19*/ { PD_BUTTON, (void*)DoPrintSetup, "setup", PDO_DLGCMDBUTTON, NULL, N_("Setup") },
/*20*/ { PD_BUTTON, (void*)PrintClear, "clear", 0, NULL, N_("Clear") },
#define I_PAGECNT		(21)
/*21*/ { PD_MESSAGE, N_("0 pages"), NULL, 0, (void*)80 },
/*22*/ { PD_MESSAGE, N_("selected"), NULL, 0, (void*)80 }
};

static paramGroup_t printPG = { "print", PGO_PREFMISCGROUP, printPLs, sizeof printPLs/sizeof printPLs[0] };


/*****************************************************************************
 *
 * TEMP DRAW
 *
 */


static void ChangeDim( void )
{
	int x, y, x0, x1, y0, y1;
	coOrd p0;
	int size;
	bitmap_t tmpBm;
	BOOL_T selected;

	MapGrid( zero, mapD.size, 0.0, currPrintGrid.orig, currPrintGrid.angle, currPrintGrid.size.x, currPrintGrid.size.y,
		&x0, &x1, &y0, &y1 );

	if ( x0==bm.x0 && x1==bm.x1 && y0==bm.y0 && y1==bm.y1 )
		return;
	size = (x1-x0) * (y1-y0);
	if (size > bm0.memsize) {
		bm0.bm = MyRealloc( bm0.bm, size );
		bm0.memsize = size;
	}
	bm0.x0 = x0; bm0.x1 = x1; bm0.y0 = y0; bm0.y1 = y1;
	memset( bm0.bm, 0, bm0.memsize );
	pageCount = 0;
	if (bm.bm) {
		for ( x=bm.x0; x<bm.x1; x++ ) {
			for ( y=bm.y0; y<bm.y1; y++ ) {
				selected = BITMAP( bm, x, y );
				if (selected) {
					p0.x = bm.orig.x + x * bm.size.x + bm.size.x/2.0;
					p0.y = bm.orig.y + y * bm.size.y + bm.size.y/2.0;
					Rotate( &p0, bm.orig, bm.angle );
					p0.x -= currPrintGrid.orig.x;
					p0.y -= currPrintGrid.orig.y;
					Rotate( &p0, zero, -currPrintGrid.angle );
					x0 = (int)floor(p0.x/currPrintGrid.size.x);
					y0 = (int)floor(p0.y/currPrintGrid.size.y);
					if ( x0>=bm0.x0 && x0<bm0.x1 && y0>=bm0.y0 && y0<bm0.y1 ) {
						if ( BITMAP( bm0, x0, y0 ) == FALSE ) {
							pageCount++;
							BITMAP( bm0, x0, y0 ) = TRUE;
						}
					}
				}
			}
		}
	}
	tmpBm = bm0;
	bm0 = bm;
	bm = tmpBm;
	bm.orig = currPrintGrid.orig;
	bm.size = currPrintGrid.size;
	bm.angle = currPrintGrid.angle;
	sprintf( message, _("%d pages"), pageCount );
	ParamLoadMessage( &printPG, I_PAGECNT, message );
	ParamDialogOkActive( &printPG, pageCount!=0 );
}


static void MarkPage(
		wIndex_t x,
		wIndex_t y )
/*
 * Hilite a area
 */
{
	coOrd p[4];
		
LOG1( log_print, ( "MarkPage( %d, %d )\n", x, y) )
	if ( x<bm.x0 || x>=bm.x1 || y<bm.y0 || y>=bm.y1) {
		ErrorMessage( MSG_OUT_OF_BOUNDS );
		return;
	}
	p[0].x = p[3].x = currPrintGrid.orig.x + x * currPrintGrid.size.x;
	p[0].y = p[1].y = currPrintGrid.orig.y + y * currPrintGrid.size.y;
	p[2].x = p[1].x = p[0].x + currPrintGrid.size.x;
	p[2].y = p[3].y = p[0].y + currPrintGrid.size.y;
	Rotate( &p[0], currPrintGrid.orig, currPrintGrid.angle );
	Rotate( &p[1], currPrintGrid.orig, currPrintGrid.angle );
	Rotate( &p[2], currPrintGrid.orig, currPrintGrid.angle );
	Rotate( &p[3], currPrintGrid.orig, currPrintGrid.angle );
LOG( log_print, 2, ( "MP(%d,%d) [%0.3f %0.3f] x [%0.3f %0.3f]\n", x, y, p[0].x, p[0].y, p[2].x, p[2].y ) )
	DrawHilightPolygon( &mainD, p, 4 );
}


static void SelectPage( coOrd pos )
{
	int x, y;
	BOOL_T selected;
	/*PrintUpdate();*/
	pos.x -= currPrintGrid.orig.x;
	pos.y -= currPrintGrid.orig.y;
	Rotate( &pos, zero, -currPrintGrid.angle );
	x = (int)floor(pos.x/currPrintGrid.size.x);
	y = (int)floor(pos.y/currPrintGrid.size.y);
	if ( x<bm.x0 || x>=bm.x1 || y<bm.y0 || y>=bm.y1)
		return;
	selected = BITMAP( bm, x, y );
	pageCount += (selected?-1:1);
	BITMAP( bm, x, y ) = !selected;
	MarkPage( x, y );
	sprintf( message, _("%d pages"), pageCount );
	ParamLoadMessage( &printPG, I_PAGECNT, message );
	ParamDialogOkActive( &printPG, pageCount!=0 );
}


static void DrawPrintGrid( void )
/*
 * Draw a grid using currPrintGrid.orig, currPrintGrid.angle, currPrintGrid.size.
 * Drawing it twice erases the grid.
 * Also hilite any marked pages.
 */
{
	wIndex_t x, y;

	DrawGrid( &tempD, &mapD.size, currPrintGrid.size.x, currPrintGrid.size.y, 0, 0, currPrintGrid.orig, currPrintGrid.angle, wDrawColorBlack, TRUE );

	for (y=bm.y0; y<bm.y1; y++)
		for (x=bm.x0; x<bm.x1; x++)
			if (BITMAP(bm,x,y)) {
				MarkPage( x, y );
			}
}

/*****************************************************************************
 *
 * PRINTING FUNCTIONS
 *
 */


static drawCmd_t print_d = {
		NULL,
		&printDrawFuncs,
		DC_PRINT,
		16.0,
		0.0,
		{0.0, 0.0}, {1.0, 1.0},
		Pix2CoOrd, CoOrd2Pix };

static drawCmd_t page_d = {
		NULL,
		&printDrawFuncs,
		DC_PRINT,
		1.0,
		0.0,
		{0.0, 0.0}, {1.0, 1.0},
		Pix2CoOrd, CoOrd2Pix };


/**
 * Print the basic layout for a trackplan. This includes the frame and some
 * information like room size, print scale etc..
 *
 * \param roomSize IN size of the layout  
 */

static void PrintGaudyBox(
		coOrd roomSize )
{
	coOrd p00, p01, p10, p11;
	struct tm *tm;
	time_t clock;
	char dat[STR_SIZE];
	wFont_p fp;
	DIST_T pageW, pageH;
	DIST_T smiggin;
	coOrd textsize;

	/*GetTitle();*/
	time(&clock);
	tm = localtime(&clock);
	strftime( dat, STR_SIZE, "%x", tm );

	smiggin = wDrawGetDPI( print_d.d );
	if (smiggin>4.0)
		smiggin = 4.0/smiggin;
	pageW = currPrintGrid.size.x/print_d.scale;
	pageH = currPrintGrid.size.y/print_d.scale;
	/* Draw some lines */
	p00.x = p01.x = 0.0;
	p00.y = p10.y = 0.0;
	p10.x = p11.x = pageW-smiggin;
	p01.y = p11.y = pageH+1.0-smiggin;

	DrawLine( &page_d, p00, p10, 0, wDrawColorBlack );
	DrawLine( &page_d, p10, p11, 0, wDrawColorBlack );
	DrawLine( &page_d, p11, p01, 0, wDrawColorBlack );
	DrawLine( &page_d, p01, p00, 0, wDrawColorBlack );

	p00.y = p10.y = 1.0;
	DrawLine( &page_d, p00, p10, 0, wDrawColorBlack );
	p00.y = p10.y = 0.5;
	DrawLine( &page_d, p00, p10, 0, wDrawColorBlack );
	p00.y = 0.5;
	p01.y = 1.0;
	p00.x = 0.05; p00.y = 0.5+0.05;
	fp = wStandardFont( F_TIMES, TRUE, TRUE );
	DrawString( &page_d, p00, 0.0, sProdName, fp, 30.0, wDrawColorBlack );

	p00.y = 0.5; p01.y = 1.0;
	p00.x = p01.x = (157.0/72.0)+0.1;
	DrawLine( &page_d, p00, p01, 0, wDrawColorBlack );
	p00.x = p01.x = pageW-((157.0/72.0)+0.1);
	DrawLine( &page_d, p00, p01, 0, wDrawColorBlack );

	fp = wStandardFont( F_TIMES, FALSE, FALSE );
	p00.x = pageW-((157.0/72.0)+0.05); p00.y = 0.5+0.25+0.05;
	DrawString( &page_d, p00, 0.0, dat, fp, 16.0, wDrawColorBlack );
	p00.y = 0.5+0.05;

	DrawTextSize( &mainD, GetLayoutTitle(), fp, 16.0, FALSE, &textsize );
	p00.x = (pageW/2.0)-(textsize.x/2.0);
	p00.y = 0.75+0.05;
	DrawString( &page_d, p00, 0.0, GetLayoutTitle(), fp, 16.0, wDrawColorBlack );
	DrawTextSize( &mainD, GetLayoutSubtitle(), fp, 16.0, FALSE, &textsize );
	p00.x = (pageW/2.0)-(textsize.x/2.0);
	p00.y = 0.50+0.05;
	DrawString( &page_d, p00, 0.0, GetLayoutSubtitle(), fp, 16.0, wDrawColorBlack );

	sprintf( dat, _("PrintScale 1:%ld   Room %s x %s   Model Scale %s   File %s"),
		(long)printScale, 
		FormatDistance( roomSize.x ),
		FormatDistance( roomSize.y ),
		curScaleName, GetLayoutFilename() );
	p00.x = 0.05; p00.y = 0.25+0.05;
	DrawString( &page_d, p00, 0.0, dat, fp, 16.0, wDrawColorBlack );
}


static void PrintPlainBox(
		wPos_t x,
		wPos_t y,
		coOrd *corners )
{
	coOrd p00, p01, p10, p11;
	char tmp[30];
	wFont_p fp;
	DIST_T pageW, pageH;
	DIST_T smiggin;

	smiggin = wDrawGetDPI( print_d.d );
	if (smiggin>4.0)
		smiggin = 4.0/smiggin;

	pageW = currPrintGrid.size.x/print_d.scale;
	pageH = currPrintGrid.size.y/print_d.scale;

	p00.x = p01.x = 0.0;
	p00.y = p10.y = 0.0;
	p10.x = p11.x = pageW-smiggin;
	p01.y = p11.y = pageH-smiggin;
	DrawLine( &page_d, p00, p10, 0, wDrawColorBlack );
	DrawLine( &page_d, p10, p11, 0, wDrawColorBlack );
	DrawLine( &page_d, p11, p01, 0, wDrawColorBlack );
	DrawLine( &page_d, p01, p00, 0, wDrawColorBlack );

	fp = wStandardFont( F_HELV, FALSE, FALSE );
	sprintf( tmp, "[%d,%d]", x, y );
	p00.x = pageW/2.0 - 20.0/72.0;
	p00.y = pageH - 10.0/72.0;
	DrawString( &page_d, p00, 0.0, tmp, fp, 4.0, wDrawColorBlack );

	sprintf( tmp, "[%0.2f,%0.2f]", corners[0].x, corners[0].y );
	p00.x = 4.0/72.0;
	p00.y = 4.0/72.0;
	DrawString( &page_d, p00, 0.0, tmp, fp, 4.0, wDrawColorBlack );

	sprintf( tmp, "[%0.2f,%0.2f]", corners[1].x, corners[1].y );
	p00.x = pageW - 40.0/72.0;
	p00.y = 4.0/72.0;
	DrawString( &page_d, p00, 0.0, tmp, fp, 4.0, wDrawColorBlack );

	sprintf( tmp, "[%0.2f,%0.2f]", corners[2].x, corners[2].y );
	p00.x = pageW - 40.0/72.0;
	p00.y = pageH - 10.0/72.0;
	DrawString( &page_d, p00, 0.0, tmp, fp, 4.0, wDrawColorBlack );

	sprintf( tmp, "[%0.2f,%0.2f]", corners[3].x, corners[3].y );
	p00.x = 4.0/72.0;
	p00.y = pageH - 10.0/72.0;
	DrawString( &page_d, p00, 0.0, tmp, fp, 4.0, wDrawColorBlack );

}

/*****************************************************************************
 *
 * BUTTON HANDLERS
 *
 */


static void PrintEnableControls( void )
{
	if (printScale <= 1) {
		ParamLoadControl( &printPG, I_REGMARKS );
		ParamControlActive( &printPG, I_REGMARKS, TRUE );
	} else {
		ParamLoadControl( &printPG, I_REGMARKS );
		printRegistrationMarks = 0;
		ParamControlActive( &printPG, I_REGMARKS, FALSE );
	}
	if (printScale <= (twoRailScale*2+1)/2.0) {
		ParamLoadControl( &printPG, I_ROADBED );
		ParamControlActive( &printPG, I_ROADBED, TRUE );
		ParamControlActive( &printPG, I_ROADBEDWIDTH, TRUE );
		ParamControlActive( &printPG, I_CENTERLINE, TRUE);
	} else {
		printRoadbed = 0;
		ParamLoadControl( &printPG, I_ROADBED );
		ParamControlActive( &printPG, I_ROADBED, FALSE );
		ParamControlActive( &printPG, I_ROADBEDWIDTH, FALSE );
		ParamControlActive( &printPG, I_CENTERLINE, FALSE );
	}
}


static void PrintUpdate( int inx0 )
/*
 * Called when print page size (x or y) is changed.
 * Checks for valid values
 */
{
	int inx;

	DrawPrintGrid();
	ParamLoadData( &printPG );

	if (newPrintGrid.size.x > maxPageSize.x+0.01 ||
		newPrintGrid.size.y > maxPageSize.y+0.01) {
		NoticeMessage( MSG_PRINT_MAX_SIZE, _("Ok"), NULL,
				FormatSmallDistance(maxPageSize.x), FormatSmallDistance(maxPageSize.y) );
	}
	if (newPrintGrid.size.x > maxPageSize.x) {
		newPrintGrid.size.x = maxPageSize.x;
		ParamLoadControl( &printPG, 1 );
	}
	if (newPrintGrid.size.y > maxPageSize.y) {
		newPrintGrid.size.y = maxPageSize.y;
		ParamLoadControl( &printPG, 3 );
	}
	currPrintGrid = newPrintGrid;
	for ( inx = 0; inx < sizeof printPLs/sizeof printPLs[0]; inx++ ) {
		if ( inx != inx0 && printPLs[inx].context == (void*)2 )
			ParamLoadControl( &printPG, inx );
	}
	ChangeDim();
	DrawPrintGrid();
}


static void SetPageSize( BOOL_T doScale )
{
	WDOUBLE_T temp, x, y;
	if (printPhysSize)
		wPrintGetPhysSize( &x, &y );
	else
		wPrintGetPageSize( &x, &y );
	maxPageSize.x = x;
	maxPageSize.y = y;
	realPageSize = maxPageSize;
	if ( (printFormat == PORTRAIT) == (maxPageSize.x > maxPageSize.y) ) {
		temp = maxPageSize.x;
		maxPageSize.x = maxPageSize.y;
		maxPageSize.y = temp;
		printRotate = TRUE;
	} else {
		printRotate = FALSE;
	}
	if (doScale) {
		if (printGaudy)
			maxPageSize.y -= 1.0;
		maxPageSize.x *= printScale;
		maxPageSize.y *= printScale;
	}
}


static void PrintMaxPageSize( void )
/*
 * Called when print:maxPageSize button is clicked.
 * Set print page size to maximum
 * (depending on paper size, scale and orientation)
 */
{
	DrawPrintGrid();
	SetPageSize( TRUE );
	currPrintGrid.size = maxPageSize;
	newPrintGrid = currPrintGrid;
	ParamLoadControls( &printPG );
	ChangeDim();
	DrawPrintGrid();
	wShow( printWin);
}


static void DoPrintScale( void )
/*
 * Called whenever print scale or orientation changes.
 */
{
	printScale = iPrintScale;
	PrintMaxPageSize();
	PrintEnableControls();
}


static void DoPrintSetup( void )
{
	wPrintSetup( (wPrintSetupCallBack_p)DoPrintScale );
}


static void PrintClear( void )
/*
 * Called when print:clear button is clicked.
 * Flip the status of all printable pages
 * (Thus making them non-print)
 */
{
	wIndex_t x, y;
	for (y=bm.y0; y<bm.y1; y++)
		for (x=bm.x0; x<bm.x1; x++)
			if (BITMAP(bm,x,y)) {
				BITMAP(bm,x,y) = 0;
				MarkPage( x, y );
			}
	pageCount = 0;
	ParamLoadMessage( &printPG, I_PAGECNT, _("0 pages") );
	ParamDialogOkActive( &printPG, FALSE );
}


static void PrintSnapShot( void )
/*
 * Called when print:SnapShot button is clicked.
 * Set scale and orientation so the whole layout is printed on one page.
 */
{
	coOrd size;
	ANGLE_T scaleX, scaleY;
	long scaleH, scaleV;
	int i;
	coOrd pageSize;
	POS_T t;

	PrintClear();
	DrawPrintGrid();
	SetPageSize( FALSE );
	pageSize = realPageSize;
	if (pageSize.x > pageSize.y) {
		t = pageSize.x;
		pageSize.x = pageSize.y;
		pageSize.y = t;
	}
	size = mapD.size;

	scaleH = 1;
	for (i=0;i<3;i++) {
		size = mapD.size;
		size.x += 0.75*scaleH;
		size.y += 0.75*scaleH;
		if (printGaudy)
			size.y += 1.0*scaleH;
		scaleX = size.x/pageSize.x;
		scaleY = size.y/pageSize.y;
		scaleH = (long)ceil(max( scaleX, scaleY ));
	}

	scaleV = 1;
	for (i=0;i<3;i++) {
		size = mapD.size;
		size.x += 0.75*scaleV;
		size.y += 0.75*scaleV;
		if (printGaudy)
			size.y += 1.0*scaleV;
		scaleX = size.x/pageSize.y;
		scaleY = size.y/pageSize.x;
		scaleV = (long)ceil(max( scaleX, scaleY ));
	}

	if ( scaleH <= scaleV ) {
		printScale = scaleH;
		printFormat = PORTRAIT;
	} else {
		printScale = scaleV;
		printFormat = LANDSCAPE;
	}

	SetPageSize( TRUE );
/*
	if (printFormat == LANDSCAPE) {
		currPrintGrid.orig.x = -0.5*printScale;
		currPrintGrid.orig.y = maxPageSize.x-0.5*printScale;
		currPrintGrid.angle = 90.0;
	} else {*/
		currPrintGrid.orig.x = -0.5*printScale;
		currPrintGrid.orig.y = -0.5*printScale;
		currPrintGrid.angle = 0.0;
/*    }*/
	currPrintGrid.size = maxPageSize;
	newPrintGrid = currPrintGrid;
	iPrintScale = (long)printScale;
	ParamLoadControls( &printPG );
	ParamGroupRecord( &printPG );
	ChangeDim();
	pageCount = 1;
	BITMAP(bm,0,0) = TRUE;
	DrawPrintGrid();
	ParamLoadMessage( &printPG, I_PAGECNT, _("1 page") );
	ParamDialogOkActive( &printPG, TRUE );
	PrintEnableControls();
	wShow( printWin );
}


static void DrawRegistrationMarks( drawCmd_p d )
{
	long x, y, delta, divisor;
	coOrd p0, p1, qq, q0, q1;
	POS_T len;
	char msg[10];
	wFont_p fp;
	wFontSize_t fs;
	fp = wStandardFont( F_TIMES, FALSE, FALSE );
	if ( units==UNITS_METRIC ) {
		delta = 10;
		divisor = 100;
	} else {
		delta = 3;
		divisor = 12;
	}
	for ( x=delta; (POS_T)x<PutDim(mapD.size.x); x+=delta ) {
		qq.x = p0.x = p1.x = (POS_T)GetDim(x);
		p0.y = 0.0;
		p1.y = mapD.size.y;
		if (!ClipLine( &p0, &p1, d->orig, d->angle, d->size ))
			continue;
		for ( y=(long)(ceil(PutDim(p0.y)/delta)*delta); (POS_T)y<PutDim(p1.y); y+=delta ) {
			qq.y = (POS_T)GetDim(y);
			q0.x = q1.x = qq.x;
			if ( x%divisor == 0 && y%divisor == 0 ) {
				len = 0.25;
				fs = 12.0;
			} else {
				len = 0.125;
				fs = 8.0;
			}
			q0.y = qq.y-len;
			q1.y = qq.y+len;
			DrawLine( d, q0, q1, 0, wDrawColorBlack );
			q0.y = q1.y = qq.y;
			q0.x = qq.x-len;
			q1.x = qq.x+len;
			DrawLine( d, q0, q1, 0, wDrawColorBlack );
			q0.x = qq.x + len/4;;
			q0.y = qq.y + len/4;;
			if (units == UNITS_METRIC)
				sprintf( msg, "%0.1fm", (DOUBLE_T)x/100.0 );
			else
				sprintf( msg, "%ld\' %ld\"", x/12, x%12 );
			DrawString( d, q0, 0.0, msg, fp, fs, wDrawColorBlack );
			q0.y = qq.y - len*3/4;
			if (units == UNITS_METRIC)
				sprintf( msg, "%0.1fm", (DOUBLE_T)y/100.0 );
			else
				sprintf( msg, "%ld\' %ld\"", y/12, y%12 );
			DrawString( d, q0, 0.0, msg, fp, fs, wDrawColorBlack );
		}
	}
}


static BOOL_T PrintPage(
		int x,
		int y )
{
	coOrd orig, p[4], minP, maxP;
	int i;
	coOrd clipOrig, clipSize;
	wFont_p fp;
	coOrd roomSize;

			if (BITMAP(bm,x,y)) {
				orig.x = currPrintGrid.orig.x + x*currPrintGrid.size.x;
				orig.y = currPrintGrid.orig.y + y*currPrintGrid.size.y;
				Rotate( &orig, currPrintGrid.orig, currPrintGrid.angle );
				p[0] = p[1] = p[2] = p[3] = orig;
				p[1].x = p[2].x = orig.x + currPrintGrid.size.x;
				p[2].y = p[3].y = orig.y + currPrintGrid.size.y + 
						( printGaudy ? printScale : 0.0 );
				Rotate( &p[0], orig, currPrintGrid.angle );
				Rotate( &p[1], orig, currPrintGrid.angle );
				Rotate( &p[2], orig, currPrintGrid.angle );
				Rotate( &p[3], orig, currPrintGrid.angle );
				minP = maxP = p[0];
				for (i=1; i<4; i++) {
					if (maxP.x < p[i].x) maxP.x = p[i].x;
					if (maxP.y < p[i].y) maxP.y = p[i].y;
					if (minP.x > p[i].x) minP.x = p[i].x;
					if (minP.y > p[i].y) minP.y = p[i].y;
				} 
				maxP.x -= minP.x;
				maxP.y -= minP.y;
				print_d.d = page_d.d = wPrintPageStart();
				if (page_d.d == NULL)
					return FALSE;
				print_d.dpi = page_d.dpi = wDrawGetDPI( print_d.d );
				print_d.angle = currPrintGrid.angle;
				print_d.orig = orig;
				print_d.size = /*maxP*/ currPrintGrid.size;
				page_d.orig = zero;
				page_d.angle = 0.0;
				if ( printGaudy ) {
					Translate( &print_d.orig, orig, currPrintGrid.angle+180.0, printScale );
					print_d.size.y += printScale;
				}
				if (printRotate) {
					rotateCW = (printFormat != PORTRAIT);
					if (rotateCW) {
						page_d.orig.x = realPageSize.y;
						page_d.orig.y = 0.0;
						page_d.angle = -90.0;
						print_d.angle += -90.0;
						Translate( &print_d.orig, print_d.orig, currPrintGrid.angle+90, maxPageSize.x );
					} else {
						page_d.orig.x = 0.0;
						page_d.orig.y = realPageSize.x;
						page_d.angle = 90.0;
						print_d.angle += 90.0;
						Translate( &print_d.orig, print_d.orig, currPrintGrid.angle,
								maxPageSize.y+(printGaudy?printScale:0) );
					}
					page_d.size.x = print_d.size.y/printScale;
					page_d.size.y = print_d.size.x/printScale;
					print_d.size.x = currPrintGrid.size.y;
					print_d.size.y = currPrintGrid.size.x;
				} else {
					page_d.size.x = print_d.size.x/printScale;
					page_d.size.y = print_d.size.y/printScale;
				}
				wSetCursor( wCursorWait );
				print_d.scale = printScale;
				if (print_d.d == NULL)
					 AbortProg( "wPrintPageStart" );
				clipOrig.x = clipOrig.y = 0;
				clipSize.x = maxPageSize.x/printScale;
				clipSize.y = maxPageSize.y/printScale;
				GetRoomSize( &roomSize );
				if (printGaudy) {
					PrintGaudyBox( roomSize );
					if ((!printRotate) || rotateCW) {
						clipOrig.y = 1.0;
					}
					if (printRotate && rotateCW) {
						print_d.size.x += printScale;
					}
				} else if (printRegistrationMarks)
					PrintPlainBox( x, y, p );
				if (printRotate) {
					wPrintClip( (wPos_t)(clipOrig.y*print_d.dpi), (wPos_t)(clipOrig.x*print_d.dpi),
							(wPos_t)(clipSize.y*print_d.dpi), (wPos_t)(clipSize.x*print_d.dpi) );
				} else {
					wPrintClip( (wPos_t)(clipOrig.x*print_d.dpi), (wPos_t)(clipOrig.y*print_d.dpi),
							(wPos_t)(clipSize.x*print_d.dpi), (wPos_t)(clipSize.y*print_d.dpi) );
				}
				p[0].x = p[3].x = 0.0;
				p[1].x = p[2].x = roomSize.x;
				p[0].y = p[1].y = 0.0;
				p[2].y = p[3].y = roomSize.y;
				fp = wStandardFont( F_TIMES, FALSE, FALSE );
				DrawRuler( &print_d, p[0], p[1], 0.0, TRUE, FALSE, wDrawColorBlack );
				DrawRuler( &print_d, p[0], p[3], 0.0, TRUE, TRUE, wDrawColorBlack );
				DrawRuler( &print_d, p[1], p[2], 0.0, FALSE, FALSE, wDrawColorBlack );
				DrawRuler( &print_d, p[3], p[2], 0.0, FALSE, TRUE, wDrawColorBlack );
				if ( printRuler && currPrintGrid.angle == 0 ) {
					if ( !printRotate ) {
						p[2] = p[3] = print_d.orig;
						p[3].x += print_d.size.x;
						p[3].y += print_d.size.y;
					} else if ( rotateCW ) {
						p[2].x = print_d.orig.x - print_d.size.y;
						p[2].y = print_d.orig.y; 
						p[3].x = print_d.orig.x;
						p[3].y = print_d.orig.y + print_d.size.x;
					} else {
						p[2].x = print_d.orig.x;
						p[2].y = print_d.orig.y - print_d.size.x;
						p[3].x = print_d.orig.x + print_d.size.y;
						p[3].y = print_d.orig.y;
					}
					if ( p[2].x > 0 )
						minP.x = p[2].x + 0.4 * print_d.scale;
					else
						minP.x = 0.0;
					if ( p[3].x < roomSize.x )
						maxP.x = p[3].x - 0.2 * print_d.scale;
					else
						maxP.x = roomSize.x;
					if ( p[2].y > 0 )
						minP.y = p[2].y + 0.4 * print_d.scale;
					else
						minP.y = 0.0;
					if ( p[3].y < roomSize.y )
						maxP.y = p[3].y - 0.2 * print_d.scale;
					else
						maxP.y = roomSize.y;
					p[0].y = 0.0;
					p[1].y = maxP.y - minP.y;
					if ( p[2].x > 0 ) {
						p[0].x = p[1].x = p[2].x + 0.4 * print_d.scale;
						DrawRuler( &print_d, p[0], p[1], minP.y, TRUE, TRUE, wDrawColorBlack );
					}
					if ( p[3].x < roomSize.x ) {
						p[0].x = p[1].x = p[3].x - 0.2 * print_d.scale;
						DrawRuler( &print_d, p[0], p[1], minP.y, FALSE, FALSE, wDrawColorBlack );
					}
					p[0].x = 0;
					p[1].x = maxP.x - minP.x;
					if ( p[2].y > 0 ) {
						p[0].y = p[1].y = p[2].y + 0.4 * print_d.scale;
						DrawRuler( &print_d, p[0], p[1], minP.x, TRUE, FALSE, wDrawColorBlack );
					}
					if ( p[3].y < roomSize.y ) {
						p[0].y = p[1].y = p[3].y - 0.2 * print_d.scale;
						DrawRuler( &print_d, p[0], p[1], minP.x, FALSE, TRUE, wDrawColorBlack );
					}
				}
				if (printGrid)
					DrawSnapGrid( &print_d, mapD.size, FALSE );
				roadbedWidth = printRoadbed?printRoadbedWidth:0.0;
				printCenterLines = printCenterLine;
				DrawTracks( &print_d, print_d.scale, minP, maxP );
				if (printRegistrationMarks && printScale == 1)
					DrawRegistrationMarks( &print_d );
				if ( !wPrintPageEnd( print_d.d ) )
					return FALSE;
				/*BITMAP(bm,x,y) = 0;*/
				MarkPage( x, y );
	}
	return TRUE;
}


static void DoPrintPrint( void * junk )
/*
 * Called when print:print button is clicked.
 * Print all the printable pages and mark them
 * non-print.
 */
{
	wIndex_t x, y;
	int copy, copies;
	long noDecoration;

	if (pageCount == 0) {
		NoticeMessage( MSG_PRINT_NO_PAGES, _("Ok"), NULL );
		return;
	}

	wPrefGetInteger( "print", "nodecoration", &noDecoration, 0 );

	print_d.CoOrd2Pix = page_d.CoOrd2Pix = mainD.CoOrd2Pix;
	wSetCursor( wCursorWait );
	if (!wPrintDocStart(GetLayoutTitle(), pageCount, &copies )) {
		wSetCursor( wCursorNormal );
		return;
	}
	if (copies <= 0)
		copies = 1;
	for ( copy=1; copy<=copies; copy++) {
		if ( printOrder == 0 ) {
			for (x=bm.x0; x<bm.x1; x++)
				for (y=bm.y1-1; y>=bm.y0; y--)
					if (!PrintPage( x, y )) goto quitPrinting;
		} else {
			for (y=bm.y0; y<bm.y1; y++)
				for (x=bm.x0; x<bm.x1; x++)
					if (!PrintPage( x, y )) goto quitPrinting;
		}
		for (y=bm.y0; y<bm.y1; y++)
			for (x=bm.x0; x<bm.x1; x++)
				if (BITMAP(bm,x,y)) {
					if (copy < copies)
						MarkPage( x, y );
					else
						BITMAP(bm,x,y) = 0;
				}
	}

quitPrinting:
	wPrintDocEnd();
	wSetCursor( wCursorNormal );
	Reset();		/* undraws grid, resets pagecount, etc */
}


static void DoResetGrid( void )
{
	DrawPrintGrid();
	currPrintGrid.orig = zero;
	currPrintGrid.angle = 0.0;
	ChangeDim();
	newPrintGrid = currPrintGrid;
	ParamLoadControls( &printPG );
	DrawPrintGrid();
}


static void PrintGridRotate( void * pangle )
{
	ANGLE_T angle = (ANGLE_T)(long)pangle;
	DrawPrintGrid();
	currPrintGrid.orig = cmdMenuPos;
	currPrintGrid.angle += angle;
	newPrintGrid = currPrintGrid;
	ParamLoadControls( &printPG );
	ChangeDim();
	DrawPrintGrid();
}

/*****************************************************************************
 *
 * PAGE PRINT COMMAND
 *
 */

static void PrintChange( long changes )
{
	if ( (changes&(CHANGE_MAP|CHANGE_UNITS|CHANGE_GRID))==0 || printWin==NULL || !wWinIsVisible(printWin) )
		return;
	newPrintGrid = currPrintGrid;
	if (!GridIsVisible())
		printGrid = 0;
	ParamLoadControls( &printPG );
	ParamControlActive( &printPG, I_GRID, GridIsVisible() );
	PrintEnableControls();
}


static void PrintDlgUpdate(
		paramGroup_p pg,
		int inx,
		void * valueP )
{
	if ( inx < 0 ) return;
	if ( pg->paramPtr[inx].context == (void*)1 )
		DoPrintScale();
	else if ( pg->paramPtr[inx].context == (void*)2 )
		PrintUpdate( inx );
	ParamControlActive( &printPG, I_RULER, currPrintGrid.angle == 0 );
}

static STATUS_T CmdPrint(
		wAction_t action,
		coOrd pos )
/*
 * Print command:
 *
 * 3 Sub-states:
 *		Select - grid coordinates are computed and the selected page is marked.
 *		Move - grid base (currPrintGrid.orig) is moved.
 *		Rotate - grid base and angle is rotated about selected point.
 */
{
	STATUS_T rc = C_CONTINUE;
	static BOOL_T downShift;

	switch (action) {

	case C_START:
		if (!wPrintInit())
			return C_TERMINATE;
		printScale = iPrintScale;
		if (printWin == NULL) {
			rminScale_999.low = 1;
			if (printScale < rminScale_999.low)
				printScale = rminScale_999.low;
			print_d.scale = printScale;
			printWin = ParamCreateDialog( &printPG, MakeWindowTitle(_("Print")), _("Print"), DoPrintPrint, (paramActionCancelProc)Reset, TRUE, NULL, 0, PrintDlgUpdate );
		}
		wShow( printWin );
		SetPageSize( TRUE );
		if (currPrintGrid.size.x == 0.0) {
			currPrintGrid.size.x = maxPageSize.x;
			currPrintGrid.size.y = maxPageSize.y;
		}
		if (currPrintGrid.size.x >= maxPageSize.x)
			currPrintGrid.size.x = maxPageSize.x;
		if (currPrintGrid.size.y >= maxPageSize.y)
			currPrintGrid.size.y = maxPageSize.y;
		newPrintGrid = currPrintGrid;
		ParamLoadControls( &printPG );
		DrawPrintGrid();
		pageCount = 0;
LOG( log_print, 2, ( "Page size = %0.3f %0.3f\n", currPrintGrid.size.x, currPrintGrid.size.y ) )
		PrintChange( CHANGE_MAP|CHANGE_UNITS );
		ParamGroupRecord( &printPG );
		ParamLoadMessage( &printPG, I_PAGECNT, "0 pages" );
		ParamDialogOkActive( &printPG, FALSE );
		ChangeDim();
		InfoMessage( _("Select pages to print, or drag to move print grid") );
		downShift = FALSE;
		ParamControlActive( &printPG, I_RULER, currPrintGrid.angle == 0 );
		return C_CONTINUE;

	case C_DOWN:
		downShift = FALSE;
		if (MyGetKeyState()&WKEY_SHIFT) {
			newPrintGrid = currPrintGrid;
			rc = GridAction( C_DOWN, pos, &newPrintGrid.orig, &newPrintGrid.angle );
			downShift = TRUE;
		}
		return C_CONTINUE;

	case C_MOVE:
		if (downShift) {
			rc = GridAction( action, pos, &newPrintGrid.orig, &newPrintGrid.angle );
			ParamLoadControls( &printPG );
		}
		return C_CONTINUE;

	case C_UP:
		if (downShift) {
			rc = GridAction( action, pos, &newPrintGrid.orig, &newPrintGrid.angle );
			ParamLoadControls( &printPG );
			DrawPrintGrid();
			currPrintGrid = newPrintGrid;
			ChangeDim();
			DrawPrintGrid();
			downShift = FALSE;
		}
		return C_CONTINUE;

	case C_LCLICK:
		SelectPage( pos );
		return C_CONTINUE;

	case C_RDOWN:
		downShift = FALSE;
		if (MyGetKeyState()&WKEY_SHIFT) {
			newPrintGrid = currPrintGrid;
			rc = GridAction( action, pos, &newPrintGrid.orig, &newPrintGrid.angle );
			downShift = TRUE;
		}
		return rc;

	case C_RMOVE:
		if (downShift) {
			rc = GridAction( action, pos, &newPrintGrid.orig, &newPrintGrid.angle );
			ParamLoadControls( &printPG );
		}
		return rc;

	case C_RUP:
		if (downShift) {
			rc = GridAction( action, pos, &newPrintGrid.orig, &newPrintGrid.angle );
			ParamLoadControls( &printPG );
			DrawPrintGrid();
			currPrintGrid = newPrintGrid;
			ChangeDim();
			DrawPrintGrid();
			downShift = FALSE;
			ParamControlActive( &printPG, I_RULER, currPrintGrid.angle == 0 );
		}
		return rc;

	case C_REDRAW:
		DrawPrintGrid();
		return C_TERMINATE;

	case C_CANCEL:
		if (printWin == NULL)
			return C_TERMINATE;
		PrintClear();
		DrawPrintGrid();
		wHide( printWin );
		return C_TERMINATE;

	case C_OK:
		DoPrintPrint( NULL );
		return C_TERMINATE;

	case C_CMDMENU:
		wMenuPopupShow( printGridPopupM );
		return C_CONTINUE;

	default:
		return C_CONTINUE;
	}
}

EXPORT wIndex_t InitCmdPrint( wMenu_p menu )
{
	ParamRegister( &printPG );
	currPrintGrid = newPrintGrid;
	log_print = LogFindIndex( "print" );
	RegisterChangeNotification( PrintChange );
	printGridPopupM = MenuRegister( "Print Grid Rotate" );
	AddRotateMenu( printGridPopupM, PrintGridRotate );
	return InitCommand( menu, CmdPrint, N_("Print..."), NULL, LEVEL0, IC_LCLICK|IC_POPUP2|IC_CMDMENU, ACCL_PRINT );
}

/*****************************************************************************
 *
 * TEST
 *
 */
#ifdef TEST

wDrawable_t printD, mainD;

void wDrawHilight( void * d, coOrd orig, coOrd size )
{
	lprintf( "wDrawHilight (%0.3f %0.3f) (%0.3f %0.3f)\n", orig.x, orig.y, size.x, size.y );
}
void PrintPage( void * d, wIndex_t mode , wIndex_t x, wIndex_t y )
{
	lprintf( "printPage %dx%d at (%0.3f %0.3f)\n", x, y, orig.x, orig.y );
}
void PrintStart( wDrawable_t *d, wIndex_t mode )
{
}
void PrintEnd( wDrawable *d )
{
}
void wPrintGetPageSize( int style, int format, int scale )
{
	printD.size.x = 11.5-(48.0/72.0);
	printD.size.y = 8.0-(48.0/72.0);
}

void DumpMap( char * f, ANGLE_T a, ANGLE_T b )
{
	wIndex_t x, y;
	lprintf( f, a, b );
	for (y=bm.y1-1; y>=bm.y1; y--) {
		for (x=bm.x0; x<bm.x1; x++)
			if (BITMAP(bm,x,y)) {
				lprintf( "X");
			} else {
				lprintf( " ");
			}
	lprintf( "\n");
	}
}

#define C_PRINT (C_UP+1)
#define C_CANCEL (C_UP+2)
#define C_SCALE (C_UP+3)

struct {
	wAction_t cmd;
	coOrd pos;
} cmds[] = {
		{ C_START, 0, 0 },
		{ C_DOWN, 20.5, 12.4 },
		{ C_MOVE, 20.5, 12.5 },
		{ C_MOVE, 20.5, 12.3 },
		{ C_MOVE, 39.3, 69.4 },
		{ C_MOVE, 39.4, 4.5 },
		{ C_MOVE, 2.4, 4.5 },
		{ C_MOVE, 2.4, 50.3 },
		{ C_UP, 0, 0 },
		{ C_DOWN, 20.5, 12.4 },
		{ C_UP, 0, 0 },
		{ C_DOWN, 32.5, 4.4 },
		{ C_UP, 0, 0 },
		{ C_PRINT, 0, 0, },
		{ C_START, 0, 0, },
		{ C_DOWN, 45.3, 43.5 },
		{ C_CANCEL, 0, 0 }
	};

main( INT_T argc, char * argv[] )
{
	INT_T i;
	mapD.size.x = 4*12;
	mapD.size.y = 3*12;
	printD.scale = 1.0;
	for (i=0; i<(sizeof cmds)/(sizeof cmds[0]); i++) {
		switch (cmds[i].cmd) {
		case C_START:
			CmdPrint( cmds[i].cmd );
			DumpMap( "Start\n", 0, 0 );
			break;
		case C_DOWN:
			CmdPrint( cmds[i].cmd, cmds[i].pos );
			DumpMap( "Down (%0.3f %0.3f)\n", cmds[i].pos.x, cmds[i].pos.y );
			break;
		case C_MOVE:
			CmdPrint( cmds[i].cmd, cmds[i].pos );
			DumpMap( "Move (%0.3f %0.3f)\n", cmds[i].pos.x, cmds[i].pos.y );
			break;
		case C_UP:
			CmdPrint( cmds[i].cmd, cmds[i].pos );
			DumpMap( "Up\n", 0, 0 );
			break;
		case C_PRINT:
			DoPrintPrint( NULL );
			DumpMap( "Print\n", 0, 0 );
			break;
		case C_CANCEL:
			ClearPrint();
			DumpMap( "Cancel\n", 0, 0 );
			break;
		case C_SCALE:
			printD.scale = cmds[i].x;
			break;
		}
	}
}
#endif
