#include <stdio.h>
#include "wlib.h"


#define TRUE	(1)
#define FALSE	(0)

wMessage_p msgP;
int ticks;
char tickM[40] = "INIT";
int alarmCont = 0;

void tick( void )
{
   sprintf( tickM, "%d", ticks++ );
   wMessageSetValue( msgP, tickM );
   if (alarmCont)
       wAlarm( 1000, tick );
}


void doCmd( void * cmd )
{
    int i;
    switch ((int)cmd) {
    case 1:
	alarmCont = 1;
	wAlarm( 1000, tick );
	break;
    case 2:
	for (i=0; i<10; i++ ) {
	    sprintf( tickM, "%d", ticks++ );
	    wMessageSetValue( msgP, tickM );
	    wPause( 1000 );
	}
	break;
    case 3:
	alarmCont = 0;
	break;
    case 4:
	wExit( 0 );
    }
}

wWin_p wMain( int argc, char * argv[] )
{

    wWin_p mainW;
    wMenu_p m;

    mainW = wWinMainCreate( "Fred", 60, 40, "Help", "Main", "", F_MENUBAR, NULL, NULL );
    m = wMenuBarAdd( mainW, NULL, "Cmd" );
    wMenuPushCreate( m, NULL, "Alarm", 0, doCmd, (void*)1 );
    wMenuPushCreate( m, NULL, "Pause", 0, doCmd, (void*)2 );
    wMenuPushCreate( m, NULL, "Stop", 0, doCmd, (void*)3 );
    wMenuPushCreate( m, NULL, "Exit", 0, doCmd, (void*)4 );
    msgP = wMessageCreate( mainW, 2, 2, NULL, 40, tickM );
    return mainW;
}
