#include <stdlib.h>
#include <stdio.h>
#include "wlib.h"

#define TRUE (1)

wWin_p mainW;
wDraw_p mainD;
wWin_p popupW;
wDraw_p popupD;
/*wFont_p font;*/

void redrawColor( wDraw_p d, void * context, wPos_t w, wPos_t h )
{
    wDrawColor c;
    wPos_t x0, y0, x1, y1, incr;
    
    wDrawGetSize( mainD, &w, &h );
    incr = w / 16;
    x0 = y0 = 0;
    x1 = w; y1 = h;
    for ( c=0; c<=7; c++ ) {
	/*printf(" color %d - %0.3f,%0.3f - %0.3f,%0.3f\n",
		c, x0, y0, x1, y1 );*/
	wDrawLine( d, x0, y0, x1, y0, incr, wDrawLineSolid, c, wDrawOptTemp );
	wDrawLine( d, x1, y0, x1, y1, incr, wDrawLineSolid, c, wDrawOptTemp );
	wDrawLine( d, x1, y1, x0, y1, incr, wDrawLineSolid, c, 0 );
	wDrawLine( d, x0, y1, x0, y0, incr, wDrawLineSolid, c, 0 );
	x0 += incr;
	y0 += incr;
	x1 -= incr;
	y1 -= incr;
    }
}

void redrawGray( wDraw_p d, void * context, wPos_t w, wPos_t h )
{
    wDrawColor c;
    int i;
    wPos_t x0, y0, x1, y1, incr;
    
    wDrawGetSize( popupD, &w, &h );
    incr = w/32;
    x0 = y0 = 0;
    x1 = w; y1 = h;
    for ( i=0; i<=100; i+=6 ) {
	c = wDrawColorGray( i );
	/*printf(" color %d - %0.3f,%0.3f - %0.3f,%0.3f\n",
		c, x0, y0, x1, y1 );*/
	wDrawLine( d, x0, y0, x1, y0, incr, wDrawLineSolid, c, 0 );
	wDrawLine( d, x1, y0, x1, y1, incr, wDrawLineSolid, c, 0 );
	wDrawLine( d, x1, y1, x0, y1, incr, wDrawLineSolid, c, 0 );
	wDrawLine( d, x0, y1, x0, y0, incr, wDrawLineSolid, c, 0 );
	x0 += incr;
	y0 += incr;
	x1 -= incr;
	y1 -= incr;
    }
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
	wDrawSetSize( *(wDraw_p*)data, w, h );
	break;
    default:
	break;
    }
}


wWin_p wMain( int argc, char * argv[] )
{
    mainW = wWinMainCreate( "colortst", 300, 300, NULL, "Main", "Main", 0, winProc, &mainD );
    mainD = wDrawCreate( mainW, 0, 0, NULL, 0, 300, 300, NULL, redrawColor, NULL );
    popupW = wWinPopupCreate( mainW, 300, 300, NULL, "Popup", "Popup", 0, winProc, &popupD );
    popupD = wDrawCreate( popupW, 0, 0, NULL, 0, 300, 300, NULL, redrawGray, NULL );
    wWinShow( popupW, TRUE );
    /*font = wStandardFont( F_TIMES, 0, 0 );*/
    return mainW;
}
