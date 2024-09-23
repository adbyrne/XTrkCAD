/** \file testapp.c
 * Small test application to demonstrate functionality of the XTrkCad windowing library wlib
 *
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/test/testapp.c,v 1.2 2007-09-14 16:17:24 m_fischer Exp $
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 
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

#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include "wlib.h"
#include <string.h>

#define N_ 
#include "tooltips.h"

#define APPNAME "testapp"
#define WINDOWTITLE "Test Application"

#define FILEPATTERN "All XTrackCAD Files (*.xtc,*.xtce)|*.xtc;*.xtce|" \
		               "XTrackCAD Trackplan (*.xtc)|*.xtc|" \
		               "XTrackCAD Extended Trackplan (*.xtce)|*.xtce|" \
		               "All Files (*)|*"

// #define TEST_ARGV
// #define TEST_SPLASH
#define TEST_PULLDOWNMENU
#define TEST_STATUSBAR
#define TEST_TOOLBAR
//#define TEST_DRAW

#define TRUE  (1)
#define FALSE (0)

long dontHideCursor = 0;


wControl_p mainW;
wControl_p menuList;
wControl_p statusMsg;

static char* map_x16[] = {
	"16 16 41 1",
	" 	c	None",
	"0	c	#4c4c4c",
	"1	c	#b040b0",
	"2	c	#18c0f0",
	"3	c	#f0a01d",
	"4	c	#787058",
	"5	c	#7ac8e0",
	"6	c	#d8a098",
	"7	c	#386870",
	"8	c	#d060b0",
	"9	c	#702860",
	"A	c	#808080",
	"B	c	#8890c0",
	"C	c	#d8a040",
	"D	c	#48b8d8",
	"E	c	#606060",
	"F	c	#e828b0",
	"G	c	#705830",
	"H	c	#c890c8",
	"I	c	#706050",
	"J	c	#777678",
	"K	c	#e548b5",
	"L	c	#e0b872",
	"M	c	#d89830",
	"N	c	#686767",
	"O	c	#707070",
	"P	c	#8c8c8c",
	"Q	c	#3cc8e8",
	"R	c	#6d4060",
	"S	c	#585858",
	"T	c	#eba02c",
	"U	c	#d87cc0",
	"V	c	#e6a84c",
	"W	c	#686878",
	"X	c	#e834b8",
	"Y	c	#78726e",
	"Z	c	#506870",
	"a	c	#a090c0",
	"b	c	#48c6e8",
	"c	c	#25c0f0",
	"d	c	#e060c0",
	"          J     ",
	"OOO     ANSNA   ",
	"O  JJ PE  I  EA ",
	"O    0   LG3L  J",
	"Y VT N   TN VV J",
	"4C  TGV3T N  3 J",
	"O    I6K  N  T Y",
	"O    RFdX N  LMY",
	"Y8XXKR  UFNQ25 J",
	"Oa   N   URb cBJ",
	"JD2b Z   b9H X1W",
	"O   b7c5c5NXK  J",
	"O    N Q  0    J",
	" AN  N  EA  N  J",
	"   ANSOA     PEN",
	"     J          " };

static char* ballgreen[] = {
"16 16 16 1",
" 	c None",
".	c #292929",
"+	c #292B29",
"@	c #29322B",
"#	c #365233",
"$	c #2D432E",
"%	c #57A572",
"&	c #B0D6C1",
"*	c #3F9159",
"=	c #7EC097",
"-	c #296932",
";	c #2D8840",
">	c #2C9231",
",	c #44C530",
"'	c #45BA32",
")	c #7BF737",
"    .+@#$@+.    ",
"   +$%&&&&%$+   ",
"  +#&&&&&&&&*+  ",
" +#=&===&&&==#+ ",
".@%%%%%%%%%%%*@.",
"+-;%%;*%**%%%*$+",
"@-**;*;;;;;;*;-@",
"@--;-;;;;;;;;;-@",
"@---->>>;->>---$",
"@->->>>>>;>>;--@",
"+->>>>,>>,>>>>-+",
".$>>>,,,,,,>>>@.",
" +->',,)),,,>-+ ",
"  +#,)))))),>+  ",
"   +#,)))),$+   ",
"    .+$##$+.    " };

static char* yellowstar[] = {
"16 16 48 1",
" 	c None",
".	c #AB9600",
"+	c #AE9900",
"@	c #D0B600",
"#	c #DBC000",
"$	c #FFDF02",
"%	c #FFDF00",
"&	c #A28D00",
"*	c #766700",
"=	c #A59000",
"-	c #B09A00",
";	c #BBA400",
">	c #C5AC00",
",	c #DFC300",
"'	c #FFE111",
")	c #FFE00A",
"!	c #D2B800",
"~	c #AC9700",
"{	c #9B8800",
"]	c #948100",
"^	c #9D8A00",
"/	c #F6D700",
"(	c #FFE004",
"_	c #FFE31D",
":	c #FFE10F",
"<	c #F2D400",
"[	c #E5C800",
"}	c #917F00",
"|	c #CEB500",
"1	c #FFE10E",
"2	c #FCDC00",
"3	c #F0D200",
"4	c #C7AE00",
"5	c #CCB300",
"6	c #FADB00",
"7	c #BEA600",
"8	c #EED100",
"9	c #ECCF00",
"0	c #E6CA00",
"a	c #B8A100",
"b	c #988500",
"c	c #DABE00",
"d	c #A38F00",
"e	c #B69F00",
"f	c #897800",
"g	c #BFA700",
"h	c #8D7B00",
"i	c #877600",
"                ",
"                ",
"                ",
"       .+       ",
"       @#       ",
"       $%&      ",
" *=-;>,')!~={]  ",
"  ^#/('_:$<[@}  ",
"    |%)1)234    ",
"     52%6<7     ",
"     ,8390a     ",
"    bccd;c|     ",
"    &ef  bg}    ",
"    h      i    ",
"                ",
"                " };


//
//------------------------- Pulldown Menu ------------------------------------
//

/**
 *	doFile: callback function for file submenu 
 */

void doFile( void * cmd )
{
	static int recent = 1;
	char buffer[20];

      switch ((uintptr_t)cmd) {
		case 1:
			sprintf(buffer, "Recent %d", recent++);
			wMenuListAdd(menuList, 0, buffer, "data for recent x");
			break;				/* push test */
		case 2:
//			printf("Menu toggle changed!\n");
			wStatusSetValue(statusMsg, "Menu toggle changed!");
			break;				/* toggle test */
		case 3: 
//			printf("Active radio button changed!\n");
			wStatusSetValue(statusMsg, "Active radio button changed!");
			break;				/* Radio test */
   		case 0:					/* 'Quit ' */
			wExit( 0 );			/* terminate application */
    }
}

void
doPause(void* unused)
{
	printf("Start pause\n");
	wPause(1000L);
	printf("End pause\n");
}

/**
 * RecentusedCallback.
 * 
 * \param attributes	pointer to item attributes 
 */

void
RecentUsedCallback(int unused, const char *label, void * attributes)
{
	printf("Recent used: %s - %s\n", label, (char *)attributes);
}

//
//------------------------- File selection -----------------------------------
//

static struct wFilSel_t* loadFile_fs = NULL;
static struct wFilSel_t* saveFile_fs = NULL;
static struct wFilSel_t* pictureFile_fs = NULL;
static struct wFilSel_t* uiFile_fs;

int LoadData(
	int cnt,
	char** fileName,
	void* attributes)
{
	for (int i = 0; i < cnt; i++) {
		printf("File no. %d is %s\n", i, fileName[i]);
	}
	return(0);
}

void
OpenForLoad(void* attributes)
{
	wFilSelect(loadFile_fs, ".");
}

void
OpenPictures(void* attributes)
{
	wFilSelect(pictureFile_fs, ".");
}

void
OpenForSave(void* attributes)
{
	wFilSelect(saveFile_fs, ".");
}

void
LoadFileSelector(wControl_p mainW, wControl_p parent)
{
	wMenuPushCreate(parent, "menuFile-load", "Open ...", WCTL+'o',
		OpenForLoad, NULL);

	if (loadFile_fs == NULL) 
		loadFile_fs = wFilSelCreate(mainW, FS_LOAD, FSO_MULTIPLEFILES, "Open Tracks",
			FILEPATTERN, LoadData, NULL);
}

void
PixmapFileSelector(wControl_p mainW, wControl_p parent)
{
	wMenuPushCreate(parent, "menuFile-load", "Open Pictures...", WCTL + 'p',
		OpenPictures, NULL);

	if(pictureFile_fs == NULL)
		pictureFile_fs = wFilSelCreate(mainW, FS_LOAD, FSO_PICTURES, "Open Pictures",
			NULL, LoadData, NULL);
}

void
SaveFileSelector(wControl_p mainW, wControl_p parent)
{
	wMenuPushCreate(parent, "menuFile-save", "Save ...", WCTL + 's',
		OpenForSave, NULL);

	if (saveFile_fs == NULL)
		saveFile_fs = wFilSelCreate(mainW, FS_SAVE, 0, "_Save Tracks",
			FILEPATTERN, LoadData, NULL);
}


//
//----------------------- Font Selection -------------------------------------
//

void
SelectFont(void* unused)
{
	wSelectFont("Choose font");
}

bool
DialogProc(wControl_p window, winProcEvent event, void *attributes, void *data2)
{	
	char* cause;
	switch (event) {
	case wAccept_e:
		cause = "OK";
		break;
	case wCancel_e:
		cause = "Cancel";
		break;
	default:
		cause = "Unknown";
		break;
	}

	printf("Dialog was closed with %s\n", cause);
	wControlShow(window, FALSE);
	return(TRUE);
}

void
BasicDialog(void* unused)
{
	wWinDialogCreate(NULL,
		"basicdialog-help",
		"Basic Dialog Test",
		"basicdialog",
		0L,
		DialogProc,
		NULL);
}

wControl_p text;

bool
NoteDialogProc(wControl_p window, winProcEvent event, void* attributes, void* data2)
{
	int textSize;
	char* textString;

	switch (event) {
	case wAccept_e:
		if (wTextGetModified(text)) {
			textSize = wTextGetSize(text);
			if (textSize) {
				textString = malloc(textSize);
				wTextGetText(text, textString, textSize);
				printf("Entered Text: %s\n", textString);
				free(textString);
			}
			else {
				printf("Nothing entered!\n");
			}
		}
		break;
	default:
		break;
	}
	wControlShow(window, false);

	return(true);
}

void
NoteDialog(void* unused)
{
 	wControl_p dialog = wWinDialogCreate(NULL,
		"note-text",
		"Notes",
		"note",
		BO_USEBUILDER,
		NoteDialogProc,
		NULL);

	text = wTextCreate(dialog, 0, 0, "note-text", "Enter notes here... ", 
		BO_USEBUILDER, 0, 0);
}

bool
SimpleDynamicProc(wControl_p window, winProcEvent event, void* attributes, void* data2)
{
	wControlShow(window, false);

	return(true);
}

bool
EntryCallBack(const char* string, wEntry_p entry)
{
	printf("Entered <%s>\n", string);

	if (!strcmp(string, "err")) {
		return(FALSE);
	}
	else {
		return(TRUE);
	}
}

wControl_p button; 

void
ButtonAction(void* attributes)
{
	static bool clicked = 0;
	if (!clicked)
		wButtonSetLabel(button, "Clicked!");
	else
		wButtonSetLabel(button, "Click me!");

	clicked = !clicked;
}

void
ComboBoxAction(unsigned inx, const char* string, unsigned inx2, void* attributes, void* data2)
{
	printf("Selected from combo box: %s\n", string);
}

char* toggleLabels1[] = { "Prices", NULL };
long toggle1;

char* toggleLabels2[] = { "Toggle 1", "Toggle 2", "Toggle 3", NULL };
long toggle2 = 3;

char* radioLabels[] = { "FM", "AM", NULL };
long radio1 = 1;
long radio2 = 0;

char* entryText = "default";

char* comboLines[] = { "Line 1", "Line 2", "Line 3" };
#define COMBOLINES (sizeof(comboLines) / sizeof(  char *))

void
SimpleDynamic(void* unused)
{
	wControl_p msg;
	wDrawColor color = 0xFF00FF;

 	wControl_p dialog = wWinDialogCreate(NULL,
		"simple-dyn",
		"Simple Generated",
		"basicdialog",
		0,
		SimpleDynamicProc,
		NULL);

	wColorSelectButtonCreate(dialog, 1, 4, "", "Select cool color..", 0, 1, &color, NULL, NULL);

	wToggleCreate(dialog, 1, 0, "singletoggle", "Single toggle", BC_NOBORDER | BC_HORIZONTAL, toggleLabels1, &toggle1, NULL, NULL);

	wToggleCreate(dialog, 1, 1, "multitoggle", "Multiple toggles", 0, toggleLabels2, &toggle2, NULL, NULL);

	wRadioCreate(dialog, 1, 2, "radio", "Radio vertical", 0, radioLabels, &radio1, NULL, NULL);

	wRadioCreate(dialog, 1, 3, "radio", "Radio horizontal", BC_NOBORDER | BC_HORIZONTAL, radioLabels, &radio2, NULL, NULL);

	wEntryCreate(dialog, 1, 6, "textentry", "Entry field", 0L, 10, entryText, 10, EntryCallBack, NULL);

	printf("Final entry: <%s>\n", entryText);

	button = wButtonCreate(dialog, 1, 7, "testbutton", "Button", 0L, 1, ButtonAction, NULL);

	wControl_p	combo = wComboBoxCreate(dialog, 1, 8, "combobox", NULL, 0L, 0, 1, NULL, ComboBoxAction, NULL);

	for (int i = 0; i < COMBOLINES; i++) { 
		wComboBoxAddValue(combo, comboLines[i], NULL);
	}

 	button = wButtonCreate(dialog, 1, 9, "bitmapbutton", (const char *)map_x16, BO_ICON, 1, NULL, NULL);

	msg = wMessageCreateEx(dialog, 0, 10, "", 1, "Small left label", BM_SMALL | BM_ALIGNLEFT);

	msg = wMessageCreate(dialog, 1, 10, "", 1, "Normal label");

	msg = wMessageCreateEx(dialog, 0, 11, "testmsg", 1, "Right aligned", BM_ALIGNRIGHT);

	msg = wMessageCreateEx(dialog, 1, 11, "testmsg", 1, "Large right aligned", BM_LARGE | BM_ALIGNRIGHT);
}

void Notice(void* unused)
{
	wNotice3("What do you want to do now?", "Continue", "Cancel", "Leave");
}

/*----------------------------------------------------------------------------
* Load and show a UI file from disk
*/ 

int LoadDesign(
	int cnt,
	char** fileName,
	void* menuList)
{
	wControl_p dialog;
	printf("UI File is %s\n", fileName[0]);
	wMenuListAdd((wControl_p)menuList, 0, fileName[0], NULL);

	dialog = wWinDialogCreate(mainW, "", "UI Viewer", fileName[0], DO_FILESYSTEM,
		SimpleDynamicProc, NULL);
	
	return(0);
}

void
RecentUsedDesigns(int unused, const char* label, void * attributes)
{
	wControl_p dialog;

	printf("Recently used design: %s - %s\n", label, (char *)attributes);
	dialog = wWinDialogCreate(mainW, "", "UI Viewer", label, DO_FILESYSTEM,
		SimpleDynamicProc, NULL);
}

void Viewer(void* menuDesigns)
{
	if (uiFile_fs == NULL) {


		uiFile_fs = wFilSelCreate(mainW, FS_LOAD, 0, "Open Designs",
			"UI Designs|*.ui", LoadDesign, menuDesigns);
	}

	wFilSelect(uiFile_fs, ".");
}

/*----------------------------------------------------------------------------
* Load and show a dialog from disk, fields can be populated for tests
*/

wControl_p DialogViewer(char * builder)
{
	wControl_p dialog;

	dialog = wWinDialogCreate(mainW, "", NULL, (char *)builder, 
		DO_FILESYSTEM | BO_USEBUILDER,
		SimpleDynamicProc, NULL);

	return(dialog);
}

char entry1[80] = "Name";
char entry2[80] = "Script";

bool
TestLength(const char* value, void* attributes)
{
	if (strlen(value) < 4)
		return(false);
	else
		return(true);
}

void TextFieldTest(void* builder)
{ 
	wControl_p widget1;
	wControl_p widget2;
  	wControl_p dialog = DialogViewer((char*)builder);

	widget1 = wEntryCreate(dialog, 0, 0, "name", NULL, 0L, 0, entry1, sizeof(entry1) - 1, TestLength, NULL);

	widget2 = wEntryCreate(dialog, 0, 0, "script", NULL, 0L, 0, entry2, sizeof(entry2) - 1, TestLength, NULL);
}

long choiceSelect;

void
ChoiceCallback(long value, void* unused)
{
	printf("Choice callback: value=%ld\n", value);
}

void
ButtonCallback(void* choice)
{
	printf("Button clicked %d\n", (int)(uintptr_t)choice);
}

void CheckboxTest(void* builderFile)
{
	wControl_p choice;
	wControl_p text;

	wControl_p dialog = DialogViewer((char*)builderFile);
	
	choice = wToggleCreate(dialog, 0, 0, "label", NULL, 0, NULL, &choiceSelect, ChoiceCallback, NULL);
	wToggleSetValue(choice, 1L);

	text = wTextCreate(dialog, 0, 0, "text", NULL, BO_READONLY, 0, 0);
	wTextClear(text);
	wTextAppend(text, "This is the default text");

 	wButtonCreate(dialog, 0, 0, "tip-prev", NULL, 0, 0, ButtonCallback,(void *)-1);
	wButtonCreate(dialog, 0, 0, "tip-next", NULL, 0, 0, ButtonCallback, (void *)1);
}

void AddElevationTest(void* unused)
{
	wControl_p dialog;
	dialog = wWinDialogCreate(NULL,
		NULL,
		"Change Elevations",
		"addElev",
		0L,
		DialogProc,
		NULL);

	wDialogButtonsConfigure(dialog, "Change", "Cancel", NULL);

	wEntryCreate(dialog, 0, 0, "value", NULL, 0L, 20, NULL, 0, NULL, NULL);
	wControlShow(dialog, TRUE);
}

void AspectEditTest(void* unused)
{
	wControl_p dialog;

	dialog = wWinDialogCreate(NULL,
		NULL,
		"Aspect Edit",
		"aspectEdit",
		0L,
		DialogProc,
		NULL);

	wDialogButtonsConfigure(dialog, "OK", "Cancel", "Help");

	wEntryCreate(dialog, 1, 0, "name", "Name", 0, 20, NULL, 0, NULL, NULL);
	wEntryCreate(dialog, 1, 1, "script", "Script", 0, 35, NULL, 0, NULL, NULL);
	wEntryCreate(dialog, 1, 2, "index", "Aspect Index", BO_READONLY, 20, NULL, 0, NULL, NULL);

	wControlShow(dialog, TRUE);
}


const char* entryList[] = {"Slowest", "Slow", "Normal", "Fast", "Faster", "Fastest", NULL};
void DemoTest(void* unused)
{
	wControl_p dialog;
	int entries = 0;
	wControl_p combo;

	dialog = wWinDialogCreate(NULL,
		NULL,
		"Demo", 
		"demo",
		BO_USEBUILDER,
		DialogProc,
		NULL);

	wDialogButtonsConfigure(dialog, "AutoPlay", "Finish", NULL);

	combo = wComboBoxCreate(dialog, 0, 0, "demo-speed", NULL, 0L, 0L, 0, NULL, NULL, NULL);

	while (entryList[entries]) {
		wComboBoxAddValue(combo, entryList[entries], 0);
		entries++;
	}
	wComboBoxSetIndex(combo, 3);

	wControlShow(dialog, TRUE);
}

wControl_p contextMenu;

void
DrawAction(wControl_p control, void* context, wAction_t action, wDrawPix_t x, wDrawPix_t y)
{
	printf("DrawAction: %d\n", action);

	if (action == wActionRUp) {
		wMenuPopupShow(contextMenu);
	}
}

void
ContextMenuAction(void* unused)
{

}

void
CreateDraw()
{
	wControl_p maindrawing;

	maindrawing = wDrawCreate(mainW, 0, 0, "maindraw", 0, 0, 0, NULL, NULL, DrawAction);

	contextMenu = wMenuPopupCreate(maindrawing,
		"Context Menu");

	wMenuPushCreate(contextMenu, NULL, "Test", 0L, ContextMenuAction, NULL);
	wMenuSeparatorCreate(contextMenu);
	wMenuToggleCreate(contextMenu, NULL, "Toggle", 0L, TRUE, ContextMenuAction, NULL);
}

wDrawColor snapColor = 0x00FF00;
wDrawColor markerColor = 0x009999;

void
ColorButtonTest(void* unused)
{
	wControl_p dialog;
	dialog = wWinDialogCreate(NULL,
		NULL,
		"Color",
		"rgbcolor",
		0L,
		DialogProc,
		NULL);

	wDialogButtonsConfigure(dialog, "OK", "Cancel", "Help");

	printf("%x\n",wDrawFindColor(0x00ff00));
	printf("%d\n",wDrawGetRGB(1000));
	wColorSelectButtonCreate(dialog, 1, 0, "snapgrid", "Snap Grid", 0, 10, &snapColor, NULL, NULL);
	wColorSelectButtonCreate(dialog, 1, 1, "marker", "Marker", 0, 10, &markerColor, NULL, NULL);
}

void
CreateMenuDialog(wControl_p mainW)
{
	wControl_p menu;
	/* create a second submenu */
	menu = wMenuBarAdd(mainW, 		/* parent window */
					NULL, 			/* help topic */
					"_Dialogs"		/* submenu title */
	);

	wMenuPushCreate(menu, "menuDialog-font", "Font Chooser...", WCTL + 'f',
		SelectFont, NULL);

	wMenuPushCreate(menu, "basicdialog", "Basic Dialog", WCTL + 'b',
		BasicDialog, NULL);

	wMenuPushCreate(menu, "notedialog", "Notes...", WCTL + 'n',
		NoteDialog, NULL);

	wMenuPushCreate(menu, "simpledyn", "Simply generated...", WCTL + 's',
		SimpleDynamic, NULL);

}

void TestMenu(wControl_p mainW)
{
	wControl_p menu1;
	wControl_p menu2;
	wControl_p menu3;
	wControl_p menu4;
	wControl_p menu;

	/* add a submenu */
	menu1 = wMenuBarAdd(mainW, 		/* parent window */
		NULL, 			/* help topic */
		"_File" 			/* submenu title */
	);

	LoadFileSelector(mainW, menu1);
	PixmapFileSelector(mainW, menu1);
	SaveFileSelector(mainW, menu1);

	wMenuSeparatorCreate(menu1);

	/* create a menuitem in submenu */
	wMenuPushCreate(menu1, 				/* parent menu */
		NULL, 				/* help topic */
		"Add to MRU", 				/* submenu title */
		WCTL + 'a', 					/* accelerator key */
		doFile, 				/* callback funtion */
		(void*)1 			/* pointer to user attributes */
	);

 	menu4 = wMenuMenuCreate(menu1, NULL, "Recently used");
	menuList = wMenuListCreate(menu4, NULL, 10, RecentUsedCallback);

	/* create a separator before 'Quit' */

	wMenuSeparatorCreate(menu1);

	/* create a menuitem in submenu */
	wMenuPushCreate(menu1, 				/* parent menu */
		NULL, 				/* help topic */
		"_Quit", 				/* submenu title */
		WALT + 'x',					/* accelerator key */
		doFile, 				/* callback funtion */
		(void*)0 			/* pointer to user attributes */
	);

	/* create a second submenu */
	menu2 = wMenuBarAdd(mainW, 		/* parent window */
		NULL, 			/* help topic */
		"_Checks" 			/* submenu title */
	);

	wMenuToggleCreate(menu2,
		NULL,
		"Active",
		WALT + 'a',
		1,
		doFile,
		(void*)2
	);

	wMenuToggleCreate(menu2,
		NULL,
		"Inactive",
		0,
		0,
		doFile,
		(void*)2
	);

	wControl_p mt = wMenuToggleCreate(menu2,
		NULL,
		"Disabled",
		0,
		0,
		doFile,
		(void*)2
	);

	wMenuToggleEnable(mt, FALSE);

	wMenuSeparatorCreate(menu2);

 	menu3 = wMenuMenuCreate(menu2, NULL, "Radio Buttons");

	wMenuRadioCreate(menu3,
		NULL,
		"Radio 1",
		WCTL + '1',
		doFile,
		(void*)3
	);

	wMenuRadioCreate(menu3,
		NULL,
		"Radio 2",
		WCTL + '2',
		doFile,
		(void*)3
	);

	wMenuRadioCreate(menu3,
		NULL,
		"Radio 3",
		WCTL + '3',
		doFile,
		(void*)3
	);

	wMenuPushCreate(menu2, 				/* parent menu */
		NULL, 				/* help topic */
		"Pause", 				/* submenu title */
		WALT + 'p',					/* accelerator key */
		doPause, 				/* callback funtion */
		(void*)0 			/* pointer to user attributes */
	);

	CreateMenuDialog(mainW);

	menu3 = wMenuBarAdd(mainW, 		/* parent window */
		NULL, 			/* help topic */
		"_Notice" 			/* submenu title */
	);

	wMenuPushCreate(menu3, 				/* parent menu */
		NULL, 				/* help topic */
		"3 Button Notice", 				/* submenu title */
		WCTL + '3', 					/* accelerator key */
		Notice, 				/* callback funtion */
		(void*)3 			/* pointer to user attributes */
	);

	wSetBalloonHelp(balloonHelp);

	menu = wMenuBarAdd(mainW, NULL, "_UI Designs");
	wControl_p menuRecent;
	wControl_p menuDesigns;
	menuRecent = wMenuMenuCreate(menu, NULL, "Recently used");
	menuDesigns = wMenuListCreate(menuRecent, NULL, 10, RecentUsedDesigns);

	wMenuPushCreate(menu,
		NULL,
		"UI _Viewer",
		0L,
		Viewer,
		menuDesigns);

	wMenuPushCreate(menu,
		NULL,
		"Text Entries",
		0L,
		TextFieldTest,
		"block.ui");

	wMenuPushCreate(menu,
		NULL,
		"Toggle, Multiline Text",
		0L,
		CheckboxTest,
		"tip.ui");

	wMenuPushCreate(menu,
		NULL,
		"Add Elevation",
		0L,
		AddElevationTest,
		NULL);

	wMenuPushCreate(menu,
		NULL,
		"Aspect Edit",
		0L,
		AspectEditTest,
		NULL);

	wMenuPushCreate(menu,
		NULL,
		"RGB Color",
		0L,
		ColorButtonTest,
		NULL);

	wMenuPushCreate(menu,
		NULL,
		"Demo",
		0L,
		DemoTest,
		NULL
	);

	menu = wMenuBarAdd(mainW, NULL, "Drawing");

	wMenuPushCreate(menu,
		NULL,
		"Create",
		0L,
		CreateDraw,
		NULL);
}

void
TestStatusbar(wControl_p mainWindow)
{
	wStatusCreate(mainWindow,
		"statusbarPosX",
		"X: 0");

	wStatusCreate(mainWindow,
		"statusbarPosY",
		"Y: 0");

	wStatusCreate(mainWindow,
		"statusbarScale",
		"Scale: 1/87");

	statusMsg = wStatusCreate(mainWindow,
		"statusbarMessage",
		"Statusbar was created successfully...");

}

void
TestToolbar(wControl_p mainWindow)
{
  	wControl_p button =	wButtonCreateForToolbar(mainWindow, 0, 0, "", (const char *)map_x16,
		0, 0, NULL, NULL);

	button = wButtonCreateForToolbar(mainWindow, 0, 0, "", (const char *)ballgreen,
		BO_GAP, 0, NULL, NULL);

	button = wButtonCreateForToolbar(mainWindow, 0, 0, "", (const char *)yellowstar,
		BO_ABUT, 0, NULL, NULL);
}

static
bool mainCallBack(wControl_p window, winProcEvent ev, void *data1, void *data2 )
{
	int result;
	switch (ev) {
	case wClose_e:
  		result = wNotice3("Close application without saving?", "_Yes", "_Save", "_ No");
		printf("Exit warning result=%d\n", result);
		if (result != 1)
			return TRUE;
		else
			return FALSE;
		break;
	default:
		return FALSE;
	}
}

wControl_p wMain( int argc, char * argv[] )
{

#ifdef TEST_ARGV
	printf("testapp: argc: %d\n", argc);

	for(int i = 0; i< argc; i++)
		printf("%s\n", argv[i]);
#endif

	wInitAppName(APPNAME);

#ifdef PATH

	printf("%s\n", wGetUserHomeDir());
	printf("%s\n", wGetAppLibDir());

#endif // PATH

#ifdef TEST_SPLASH
	/* add a splash window */
	wCreateSplash( WINDOWTITLE,			/* name of application to show */
						"1.0"						/* application version information */
					 );	

	wFlush();									/* make sure splash window is shown */
#endif

	/* create main window */	
    mainW = wWinMainCreate( APPNAME, 	/* application name  */
	 								800, 			/* position x */
									600, 			/* position y */
									"Help", 		/* help topic */
									WINDOWTITLE, /* window title */
									APPNAME, 	/* window name */	
									BO_USEBUILDER | F_RESIZE|F_MENUBAR, /* options */
									mainCallBack, 		/* window callback function */
									NULL 			/* pointer to user attributes */
									);

	// wWinShow( mainW, FALSE );

#ifdef TEST_PULLDOWNMENU
	TestMenu(mainW);
#endif

#ifdef TEST_STATUSBAR
	TestStatusbar(mainW);
#endif

#ifdef TEST_TOOLBAR
	TestToolbar(mainW);
#endif
	
#ifdef TEST_SPLASH	
	char buffer[ 80 ];

	for( int i = 2; i > 0; i-- ) {
		sprintf(buffer, "Countdown %d", i );
		wSetSplashInfo( buffer );
		wPause( 1000L );
	}
 
	wDestroySplash();							/* remove the splash window again */
#endif	

	return mainW;
}
