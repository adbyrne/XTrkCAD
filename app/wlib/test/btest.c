#include <stdio.h>
#include <stdlib.h>
/*#include "common.h"*/
/*#include "utility.h"*/
#include "wlib.h"
#include "dtest.bmp"
#include "bits.bmp"

#define TRUE (1)
#define FALSE (0)

    wWin_p mainW;
    wMenu_p m00, m01, m02;
    wWin_p w1, w2, w3, w4, w5, w6, w7, w8;
    wDraw_p d[9];
    wMenu_p m3, m4, m7, m8;
    wText_p txt;
wBool_t doresize = FALSE;

static void doExit( void * data )
{
    wExit(0);
}

/*static void doFont( void * data )
{
    char * f;
    wSelectFont();
    f = wCurFont();
    wMessage( w5, f, FALSE );
}*/


static void doRestore( void * data )
{
    wDraw_p d = (wDraw_p) data;
    wDrawSetSize( d, 200, 200 );
}


static void mainEvent( wWin_p w, winProcEvent e, void * data )
{
    wPos_t width, height;
    switch( e ) {
    case wResize_e:
	wWinGetSize( w, &width, &height );
	wTextSetSize( txt, width, height );
	break;
    default:
	;
    }
}

static void winEvent( wWin_p w, winProcEvent e, void * data )
{
    wPos_t width, height;
    wIndex_t inx;
    char msg[80];
    switch( e ) {
    case wResize_e:
	inx = (wIndex_t)data;
	wWinGetSize( w, &width, &height );
	sprintf( msg, "%s: %d x %d\n", wWinGetTitle(w), width, height );
	wTextAppend( txt, msg );
	if ( doresize)
	    wDrawSetSize( d[inx], width-4, height-4 );
	break;
    default:
	;
    }
}

wDrawBitMap_p bm_w8;
wDrawBitMap_p bm_w4;

void doMouse( wAction_t action, double x, double y )
{
    char buff[80];
    wPos_t ix, iy;
    ix = x * wDrawGetDPI( d[1] );
    iy = y * wDrawGetDPI( d[1] );
    sprintf( buff, "%d: [%0.3f %0.3f] ( %d %d )\n", (int)action, x, y, ix, iy );
    wTextAppend( txt, buff );
#ifdef LATER
    switch ( action ) {
    case wActionDown:
	
	wDrawBitMap( d[4], bm_w4, x, y, wDrawColorRed, wDrawOptTemp );
 	oldX = x;
	oldY = y;
 	break;
    case wActionMove:
	wDrawBitMap( d[4], bm_w4, oldX, oldY, wDrawColorRed, wDrawOptTemp );
	wDrawBitMap( d[4], bm_w4, x, y, wDrawColorRed, wDrawOptTemp );
 	oldX = x;
	oldY = y;
	break;
    case wActionUp: 
    wDrawBitMap( d[4], bm_w4, oldX, oldY, wDrawColorRed, wDrawOptTemp );
    wDrawBitMap( d[4], bm_w4, x, y, wDrawColorRed, 0 );
	break;
    }
#endif
}

void doBitMap( wAction_t action, double x, double y )
{
    static double oldX, oldY;
    switch ( action ) {
    case wActionDown:
	
	wDrawBitMap( d[4], bm_w4, x, y, wDrawColorRed, wDrawOptTemp );
 	oldX = x;
	oldY = y;
 	break;
    case wActionMove:
	wDrawBitMap( d[4], bm_w4, oldX, oldY, wDrawColorRed, wDrawOptTemp );
	wDrawBitMap( d[4], bm_w4, x, y, wDrawColorRed, wDrawOptTemp );
 	oldX = x;
	oldY = y;
	break;
    case wActionUp: 
    wDrawBitMap( d[4], bm_w4, oldX, oldY, wDrawColorRed, wDrawOptTemp );
    wDrawBitMap( d[4], bm_w4, x, y, wDrawColorRed, 0 );
	break;
    }
}

static wLines_t lines[] = {
	{ 0,   2,   2, 198,   2 },
	{ 0, 198,   2, 198, 198 },
	{ 0, 198, 198,   2, 198 },
	{ 0,   2, 198,   2,   2 },
 	{ 0,   2,   2, 198, 198 },
 	{ 0,   2, 198, 198,   2 } };

wWin_p wMain( int argc, char * argv[] )
{

    mainW = wWinMainCreate( "dtest", 0, 0, "Help", "Main", F_MENUBAR|F_AUTOSIZE, mainEvent, NULL );
    wWinSetIcon( mainW, dtest_width, dtest_height, dtest_bits );
    m00 = wMenuBarAdd( mainW, "menu0-0", "&Restore" );
    m01 = wMenuBarAdd( mainW, "menu0-1", "&Message" );
    m02 = wMenuBarAdd( mainW, "menu0-2", "M&isc" );
    txt = wTextCreate( mainW, 0, 0, NULL, NULL, BT_CHARUNITS, 40, 15 );

    w1 = wWinPopupCreate( mainW, 2, 2, "Help1", "1", F_AUTOSIZE, winEvent, (void*)1 );
    d[1] = wDrawCreate( w1, 2, 2, "Draw-1", 0, 200, 200, w1, NULL, doMouse );
    wLineCreate( w1, "", 6, lines ); 

    w2 = wWinPopupCreate( mainW, 2, 2, "Help2", "2 Resize", F_AUTOSIZE|F_RESIZE, winEvent, (void*)2 );
    d[2] = wDrawCreate( w2, 2, 2, "Draw-1", 0, 200, 200, w2, NULL, doMouse );
    wLineCreate( w2, "", 6, lines );

    w3 = wWinPopupCreate( mainW, 2, 2, "Help3", "3 MenuBar", F_AUTOSIZE|F_MENUBAR, winEvent, (void*)3 );
    d[3] = wDrawCreate( w3, 2, 2, "Draw-3", 0, 200, 200, w3, NULL, doMouse );
    m3 = wMenuBarAdd( w3, "HelpMB3", "Menu3" );
    wLineCreate( w3, "", 6, lines );

    w4 = wWinPopupCreate( mainW, 2, 2, "Help4", "4 Resize|MenuBar", F_AUTOSIZE|F_MENUBAR|F_RESIZE, winEvent, (void*)4 );
    d[4] = wDrawCreate( w4, 2, 2, "Draw-4", 0, 200, 200, w4, NULL, doBitMap );
    bm_w4 = wDrawBitMapCreate( d[4], bits_width, bits_height, 0, 0, bits_bits );
    m4 = wMenuBarAdd( w4, "HelpMB4", "Menu4" );
    wLineCreate( w4, "", 6, lines );

#ifdef LATER
    w5 = wWinPopupCreate( mainW, 3, 3, "Help5", "5 Footer", F_AUTOSIZE|F_FOOTER, winEvent, (void*)5 );
    d[5] = wDrawCreate( w5, 2, 2, "Draw-5", 0, 200, 200, w5, NULL, NULL );
    wLineCreate( w5, "", 6, lines );

    w6 = wWinPopupCreate( mainW, 3, 3, "Help6", "6 Footer|Resize", F_AUTOSIZE|F_FOOTER|F_RESIZE, winEvent, (void*)6 );
    d[6] = wDrawCreate( w6, 2, 2, "Draw-6", 0, 200, 200, w6, NULL, NULL );
    wLineCreate( w6, "", 6, lines );

    w7 = wWinPopupCreate( mainW, 3, 3, "Help7", "7 Footer|MenuBar", F_AUTOSIZE|F_FOOTER|F_MENUBAR, winEvent, (void*)7 );
    d[7] = wDrawCreate( w7, 2, 2, "Draw-7", 0, 200, 200, w7, NULL, NULL );
    m7 = wMenuBarAdd( w7, "HelpMB7", "Menu7" );
    wLineCreate( w7, "", 6, lines );

    w8 = wWinPopupCreate( mainW, 3, 3, "Help8", "8 Footer|Resize|MenuBar", F_AUTOSIZE|F_FOOTER|F_MENUBAR|F_RESIZE, winEvent, (void*)8 );
    d[8] = wDrawCreate( w8, 2, 2, "Draw-8", 0, 200, 200, w8, NULL, doMouse );
    bm_w8 = wDrawBitMapCreate( d[8], bits_width, bits_height, 0, 0, bits_bits );
    m8 = wMenuBarAdd( w8, "HelpMB8", "Menu8" );
    wLineCreate( w8, "", 6, lines );
#endif

    wMenuPushCreate( m00, "menu0-1", "&1", doRestore, (void*)d[1] );
    wMenuPushCreate( m00, "menu0-2", "&2", doRestore, (void*)d[2] );
    wMenuPushCreate( m00, "menu0-3", "&3", doRestore, (void*)d[3] );
    wMenuPushCreate( m00, "menu0-4", "&4", doRestore, (void*)d[4] );
    wMenuPushCreate( m00, "menu0-5", "&5", doRestore, (void*)d[5] );
    wMenuPushCreate( m00, "menu0-6", "&6", doRestore, (void*)d[6] );
    wMenuPushCreate( m00, "menu0-7", "&7", doRestore, (void*)d[7] );
    wMenuPushCreate( m00, "menu0-8", "&8", doRestore, (void*)d[8] );


    wMenuPushCreate( m02, "menu2-1", "&Exit", doExit, NULL );
/*    wMenuPushCreate( m02, "menu2-2", "&Font", doFont, NULL );*/

    wWinShow( w1, TRUE );
    wWinShow( w2, TRUE );
    wWinShow( w3, TRUE );
    wWinShow( w4, TRUE );
#ifdef LATER
    wWinShow( w5, TRUE );
    wWinShow( w6, TRUE );
    wWinShow( w7, TRUE );
    wWinShow( w8, TRUE );
#endif

    return mainW;
}
