#include <stdio.h>
#include "wlib.h"


#define TRUE	(1)
#define FALSE	(0)

wWin_p mainW, popup1W, popup2W;
wList_p list1lB;
wList_p list1dB;
wList_p list1cB;
wList_p list2lB;
wList_p list2dB;
wList_p list2cB;

static long valList1 = 3;
static void doList1( wIndex_t inx, char * name, wIndex_t op, void * data, void * itemContext )
{
    wNotice( name, "Ok", NULL );
}

static long valList2 = 4;
static void doList2( wIndex_t inx, char * name, wIndex_t op, void * data, void * itemContext )
{
    wNotice( name, "Ok", NULL );
}


static long valList3 = 5;
static void doList3( wIndex_t inx, char * name, wIndex_t op, void * data, void * itemContext )
{
    wNotice( name, "Ok", NULL );
}


void populateList( wList_p l ) {
    wListAddValue( l, "Giraffe", NULL, NULL );
    wListAddValue( l, "Marmaset", NULL, NULL );
    wListAddValue( l, "Nasty", NULL, NULL );
    wListAddValue( l, "Beaver", NULL, NULL );
    wListAddValue( l, "Lion", NULL, NULL );
    wListAddValue( l, "Donkey", NULL, NULL );
    wListAddValue( l, "Elephant", NULL, NULL );
    wListAddValue( l, "Jack Rabbit", NULL, NULL );
    wListAddValue( l, "Horse", NULL, NULL );
    wListAddValue( l, "Aardvark", NULL, NULL );
    wListAddValue( l, "Igloo", NULL, NULL );
    wListAddValue( l, "Cow", NULL, NULL );
    wListAddValue( l, "Kangaroo", NULL, NULL );
    wListAddValue( l, "Fawn", NULL, NULL );
}

wWin_p wMain( int argc, char * argv[] )
{

    wMenu_p m1, m2;

    mainW = wWinMainCreate( "Fred", 40, 80, "Help", "Main", F_AUTOSIZE|F_MENUBAR, NULL, NULL );
    popup1W = wWinPopupCreate( mainW, 2, 2, "Help2", "double click", F_AUTOSIZE|F_MENUBAR, NULL, NULL );

    list1lB = wListCreate( popup1W, 2, 2, "List1", NULL, BL_DBLCLICK, 10, 200, &valList1, doList1, NULL );
    populateList( list1lB );
    list1dB = wDropListCreate( popup1W, 2, -4, "List2", NULL, BL_DBLCLICK, 10, 200, &valList2, doList2, NULL );
    populateList( list1dB );
    list1cB = wComboListCreate( popup1W, 2, -4, "List3", NULL, BL_DBLCLICK, 10, 200, &valList3, doList3, NULL );
    populateList( list1cB );
    m1 = wMenuBarAdd( popup1W, NULL, "Menu" );
    wMenuPushCreate( m1, NULL, "Clear List 1", (wMenuCallBack_p)wListClear, list1lB );
    wMenuPushCreate( m1, NULL, "Clear List 2", (wMenuCallBack_p)wListClear, list1dB );
    wMenuPushCreate( m1, NULL, "Clear List 3", (wMenuCallBack_p)wListClear, list1cB );
    wMenuPushCreate( m1, NULL, "Pop List 1", (wMenuCallBack_p)populateList, list1lB );
    wMenuPushCreate( m1, NULL, "Pop List 2", (wMenuCallBack_p)populateList, list1dB );
    wMenuPushCreate( m1, NULL, "Pop List 3", (wMenuCallBack_p)populateList, list1cB );


    popup2W = wWinPopupCreate( mainW, 2, 2, "Help2", "single click", F_AUTOSIZE|F_MENUBAR, NULL, NULL );
    list2lB = wListCreate( popup2W, 2, -4, "List1", NULL, BL_SORT, 10, 200, &valList1, doList1, NULL );
    populateList( list2lB );
    list2dB = wDropListCreate( popup2W, 2, -4, "List2", NULL, BL_SORT, 10, 200, &valList2, doList2, NULL );
    populateList( list2dB );
    list2cB = wComboListCreate( popup2W, 2, -4, "List3", NULL, BL_SORT, 10, 200, &valList3, doList3, NULL );
    populateList( list2cB );
    m2 = wMenuBarAdd( popup2W, NULL, "Menu" );
    wMenuPushCreate( m2, NULL, "Clear List 1", (wMenuCallBack_p)wListClear, list2lB );
    wMenuPushCreate( m2, NULL, "Clear List 2", (wMenuCallBack_p)wListClear, list2dB );
    wMenuPushCreate( m2, NULL, "Clear List 3", (wMenuCallBack_p)wListClear, list2cB );
    wMenuPushCreate( m2, NULL, "Pop List 1", (wMenuCallBack_p)populateList, list2lB );
    wMenuPushCreate( m2, NULL, "Pop List 2", (wMenuCallBack_p)populateList, list2dB );
    wMenuPushCreate( m2, NULL, "Pop List 3", (wMenuCallBack_p)populateList, list2cB );



    wWinShow( popup1W, TRUE );
    wWinShow( popup2W, TRUE );
    return mainW;
}
