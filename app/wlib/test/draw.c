
#include <stdio.h>
#include "wlib.h"


#define TRUE	(1)
#define FALSE	(0)

wFont_p font;
wDraw_p draw1;
wDrawColor black;
static wPos_t origX, origY, oldX, oldY;

void box( double x0, double y0, double x1, double y1, wDrawOpts opts )
{
    wDrawLine( draw1, x0, y0, x1, y0, 0, wDrawLineSolid,
	   black, opts );
    wDrawLine( draw1, x1, y0, x1, y1, 0, wDrawLineSolid,
	   black, opts );
    wDrawLine( draw1, x1, y1, x0, y1, 0, wDrawLineSolid,
	   black, opts );
    wDrawLine( draw1, x0, y1, x0, y0, 0, wDrawLineSolid,
	   black, opts );
}

void doDraw( wAction_t action, wPos_t x, wPos_t y )
{
    char str[2];
    switch (action & 0xFF) {
    case wActionLDown:
	origX = oldX = x;
	origY = oldY = y;
	box( origX, origY, oldX, oldY, wDrawOptTemp );
	break;
    case wActionLDrag:
	box( origX, origY, oldX, oldY, wDrawOptTemp );
	oldX = x; oldY = y;
	box( origX, origY, oldX, oldY, wDrawOptTemp );
	break;
    case wActionLUp:
	break;
    case wActionText:
	str[0] = (char)((action>>8)&0xFF);
	str[1] = 0;
	wDrawString( draw1, oldX, oldY, 0, str, font, 24, black, 0 );
	break;
    }
}

void doRedraw( wDraw_p bd, void * data, wPos_t x, wPos_t y )
{
	box( origX, origY, oldX, oldY, 0 );
}

void winProc(
	wWin_p win,
	winProcEvent ev,
	void * data )
{
    wPos_t w, h;
    switch( ev ) {
    case wResize_e:
	wWinGetSize( win, &w, &h );
	wDrawSetSize( draw1, w, h );
	break;
    default:
	break;
    }
}


wWin_p wMain( int argc, char * argv[] )
{

    wWin_p mainW;

    mainW = wWinMainCreate( "Fred", 2, 2, "Help", "Main", "Main", F_AUTOSIZE|F_MENUBAR, winProc, NULL );
    font = wStandardFont( F_TIMES, FALSE, FALSE );
    black = wDrawFindColor(0);
    draw1 = wDrawCreate( mainW, 2, 2, NULL, 0, 300, 200, NULL, doRedraw, doDraw );
    return mainW;
}
