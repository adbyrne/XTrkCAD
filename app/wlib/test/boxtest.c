#include <stdlib.h>
#include <stdio.h>
#include "wlib.h"

#define TRUE (1)

wWin_p mainW;

int tests = 255;

wWin_p wMain( int argc, char * argv[] )
{
    mainW = wWinMainCreate( "drawtest", 300, 300, NULL, "Main", "Main", F_RESIZE, NULL, NULL );
    if (tests & 0x01)
    wBoxCreate( mainW, 10, 10, NULL, wBoxThinB, 280, 280 );
    if (tests & 0x02)
    wBoxCreate( mainW, 20, 20, NULL, wBoxThinW, 260, 260 );
    if (tests & 0x04)
    wBoxCreate( mainW, 30, 30, NULL, wBoxAbove, 240, 240 );
    if (tests & 0x08)
    wBoxCreate( mainW, 40, 40, NULL, wBoxBelow, 220, 220 );
    if (tests & 0x10)
    wBoxCreate( mainW, 50, 50, NULL, wBoxThickB, 200, 200 );
    if (tests & 0x20)
    wBoxCreate( mainW, 60, 60, NULL, wBoxThickW, 180, 180 );
    if (tests & 0x40)
    wBoxCreate( mainW, 70, 70, NULL, wBoxRidge, 160, 160 );
    if (tests & 0x80)
    wBoxCreate( mainW, 80, 80, NULL, wBoxTrough, 140, 140 );
    return mainW;
}
