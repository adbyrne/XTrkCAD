#include <stdio.h>
#include "wlib.h"

#include "fred.bmp"

wMenu_p menu1a;
wMenu_p menu2;
wMenuList_p mlist1;
wButton_p butt2;
#define TRUE	(1)
#define FALSE (0)

static void doButt1( void * data )
{
    static wBool_t busy2 = FALSE;
    wControlActive( (wControl_p)butt2, busy2 );
    busy2 = ! busy2;
    wNotice( "doButt1", "Ok", NULL );

}

static void doButt2( void * data )
{
    wNotice( "doButt2", "Ok", NULL );
}

static long valInt1 = 4;
static void doInt1( long val, void * data )
{
    wNotice( "doInt1", "Ok", NULL );
}

static double valFlt1 = 4;
static void doFlt1( double val, void * data )
{
    wNotice( "doFlt1", "Ok", NULL );
}

static char valTxt1[80] = "abcdefghijkl";
static void doTxt1( char * val, void * data )
{
    wNotice( "doText1", "Ok", NULL );
}

static long valList1 = 3;
static void doList1( wIndex_t inx, char * name, wIndex_t op, void * data, void * itemContext )
{
    wNotice( name, "Ok", NULL );
    wMenuListAdd( mlist1, 0, name, NULL );
}

static long valList2 = 4;
static void doList2( wIndex_t inx, char * name, wIndex_t op, void * data, void * itemContext )
{
    wNotice( name, "Ok", NULL );
    wMenuListDelete( mlist1, name );
}

static void doAnimal( void * data )

{
    wNotice( (char*)data, "Ok", NULL );
}

static long valList3 = 5;
static void doList3( wIndex_t inx, char * name, wIndex_t op, void * data, void * itemContext )
{
    wNotice( name, "Ok", NULL );
    wMenuPushCreate( menu1a, NULL, name, doAnimal, name );
}

static void doMList1( int index, char * name, void * data )
{
}

void printNewPageSize( void )
{
}


static wLines_t lines1[] = {
	{ 0, 100, 4, 200, 4 },
	{ 0, 200, 4, 100, 200 },
	{ 0, 100, 200, 200, 200 } };

void populateList( wList_p l ) {
    wListAddValue( l, "Aardvark", NULL, NULL );
    wListAddValue( l, "Beaver", NULL, NULL );
    wListAddValue( l, "Elephant", NULL, NULL );
    wListAddValue( l, "Cow", NULL, NULL );
    wListAddValue( l, "Donkey", NULL, NULL );
    wListAddValue( l, "Jack Rabbit", NULL, NULL );
    wListAddValue( l, "Fawn", NULL, NULL );
    wListAddValue( l, "Nasty", NULL, NULL );
    wListAddValue( l, "Giraffe", NULL, NULL );
    wListAddValue( l, "Horse", NULL, NULL );
    wListAddValue( l, "Igloo", NULL, NULL );
    wListAddValue( l, "Lion", NULL, NULL );
    wListAddValue( l, "Kangaroo", NULL, NULL );
    wListAddValue( l, "Marmaset", NULL, NULL );
}

char * labels1[] = { "one", "two", "three", NULL };

wWin_p wMain( int argc, char * argv[] )
{

    wWin_p mainW, popupW;
    wList_p list1B;
    wList_p list2B;
    wList_p list3B;
    wButtonBitMap_p fred_bmp;
    wMenu_p menu1;
    wMenu_p menu2a;
    wDraw_p draw1;

    mainW = wWinMainCreate( "Fred", 2, 2, "Help", "Main", F_AUTOSIZE|F_MENUBAR, NULL, NULL );
    popupW = wWinPopupCreate( mainW, 200, 400, "Help2", "Popup", 0, NULL, NULL );

    menu1 = wMenuBarAdd( mainW, "menu1", "&Menu1" );
    wMenuPushCreate( menu1, "menu1-1", "&Dog", doAnimal, "Dog" );

    wButtonCreate( mainW, 0, 0, "Button1", "Butt1", BB_DEFAULT, 0, doButt1, NULL );
    fred_bmp = wButtonBitMapCreate( fred_width, fred_height, fred_bits );
    butt2 = wButtonCreate( mainW, 100, 0, "Button2", (char*)fred_bmp, BO_ICON|BB_CANCEL, 0, doButt2, NULL );
    wButtonCreate( mainW, -1, 0, "Button2", (char*)fred_bmp, BO_ICON, 0, doButt2, NULL );
    wButtonCreate( mainW, -1, 0, "Button2", (char*)fred_bmp, BO_ICON, 0, doButt2, NULL );
    wButtonCreate( mainW, -1, 0, "Button2", (char*)fred_bmp, BO_ICON, 0, doButt2, NULL );
    wButtonCreate( mainW, -1, 0, "Button2", (char*)fred_bmp, BO_ICON, 0, doButt2, NULL );
    wButtonCreate( mainW, -1, 0, "Button2", (char*)fred_bmp, BO_ICON, 0, doButt2, NULL );
    wButtonCreate( mainW, -1, 0, "Button2", (char*)fred_bmp, BO_ICON, 0, doButt2, NULL );
    wButtonCreate( mainW, -1, 0, "Button2", (char*)fred_bmp, BO_ICON, 0, doButt2, NULL );
    wButtonCreate( mainW, -1, 0, "Button2", (char*)fred_bmp, BO_ICON, 0, doButt2, NULL );
    wButtonCreate( mainW, -1, 0, "Button2", (char*)fred_bmp, BO_ICON, 0, doButt2, NULL );
    menu2 = wMenuCreate( mainW, 50, -1, "Menu2", "2" );
    wMenuPushCreate( menu2, "menu2-1", "&Cat", doAnimal, "Cat" );
    wMenuPushCreate( menu2, "menu2-2", "&Mouse", doAnimal, "Mouse" );
    menu2a = wMenuMenuCreate( menu2, "menu2-3", "M&ore" );
    wMenuPushCreate( menu2a, "menu2a-1", "&Wolf", doAnimal, "Wolf" );
    wMenuPushCreate( menu2a, "menu2a-2", "&Pony", doAnimal, "Pony" );

    menu1a = wMenuMenuCreate( menu1, "menu1-3", "M&ore" );
    wMenuPushCreate( menu1a, "menu1a-1", "&Wolf", doAnimal, "Wolf" );
    wMenuPushCreate( menu1a, "menu1a-2", "&Pony", doAnimal, "Pony" );
    wMenuSeparatorCreate( menu1a );
    mlist1 = wMenuListCreate( menu1a, "menu1-4", 10, doMList1 );
    wMenuPushCreate( menu1a, "menu1-5", "&Zebra", doAnimal, "Zebra" );

    wIntegerCreate( mainW, 50, -1, "Integer1", "Int1", 0, 50, -100, 100, &valInt1, doInt1, NULL );
    wFloatCreate( mainW, 50, -1, "Float1", "Flt1", 0, 50, -100, 100, &valFlt1, doFlt1, NULL );
    wStringCreate( mainW, 50, -1, "Text1", "Txt1", 0, 100, valTxt1, sizeof valTxt1, doTxt1, NULL );
    wMessageCreate( mainW, 50, -1, "Message1", 150, "This is a message" );
    wRadioCreate( mainW, 50, -4, NULL, NULL, 0, labels1, NULL, NULL, NULL );
    wToggleCreate( mainW, 50, -4, NULL, NULL, 0, labels1, NULL, NULL, NULL );
    wRadioCreate( mainW, 50, -4, NULL, NULL, BC_HORZ, labels1, NULL, NULL, NULL );
    wToggleCreate( mainW, 50, -4, NULL, NULL, BC_HORZ, labels1, NULL, NULL, NULL );
    wLineCreate( mainW, "Line1", sizeof lines1 / sizeof lines1[0], lines1 );

    draw1 = wDrawCreate( mainW, 250, 50, "Draw-1", 0, 150, 150, NULL, NULL, NULL );
    list1B = wListCreate( mainW, 250, -4, "List1", NULL, 0, 10, 200, &valList1, doList1, NULL );
    populateList( list1B );
    list2B = wDropListCreate( mainW, 250, -4, "List2", NULL, 0, 10, 200, &valList2, doList2, NULL );
    populateList( list2B );
    list3B = wComboListCreate( mainW, 250, -4, "List3", NULL, 0, 10, 200, &valList3, doList3, NULL );
    populateList( list3B );


    return mainW;
}
