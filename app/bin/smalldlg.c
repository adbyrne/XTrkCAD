/** \file smalldlg.c
 * Several simple and smaller dialogs.
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis
 *                2007 Martin Fischer
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

#include "common.h"
#include "custom.h"
#include "fileio.h"
#include <form.h>
#include "icons.h"
#include "misc.h"
#include "paths.h"
#include "smalldlg.h"

EXPORT wControl_p aboutW;
static wControl_p tipW;					/**< window handle for tip dialog */

static long showTipAtStart = 1;		/**< flag for visibility */

static dynArr_t tips_da;			/**< dynamic array for all tips */
#define tips(N) DYNARR_N( char *, tips_da, N )

static paramTextData_t tipTextData = { 1, 1 };

static paramGroup_t tipPG;
static paramData_t tipPLs[] = {
#define I_TIPTEXT		(1)
#define tipT			(tipPLs[I_TIPTEXT].control)
	{   PD_MESSAGE, NULL, "mess1", .group=&tipPG },
	{   PD_TEXT, NULL, "text", PDO_DLGRESIZE, &tipTextData, .group=&tipPG },
	{   PD_BUTTON, ShowTip, "prev", 0, NULL, NULL, 0L, I2VP(SHOWTIP_FORCESHOW | SHOWTIP_PREVTIP), .group=&tipPG },
#define I_TIPPREV (2)
	{   PD_BUTTON, ShowTip, "next", 0, NULL, NULL, 0L, I2VP(SHOWTIP_FORCESHOW | SHOWTIP_NEXTTIP), .group=&tipPG },
#define I_TIPNEXT (3)
	{   PD_TOGGLE, &showTipAtStart, "showatstart", .group=&tipPG }
};

static paramGroup_t tipPG = { "tip", PGO_FULLDIALOGFROMBUILDER, tipPLs, COUNT( tipPLs ) };

#define PREVIOUSBUTTON ((tipPLs[I_TIPPREV].control))
#define NEXTBUTTON (tipPLs[I_TIPNEXT].control)

#define MAX_TIP_LENGTH 4096

/**
 * Create and initialize the tip of the day window. The dialog box is created and the list of tips is loaded
 * into memory.
 */

static void CreateTipW( void )
{
	FILE * tipF;
	char buff[MAX_TIP_LENGTH];
	char *filename;
	char * cp;

	tipW = FormCreateDialog(&tipPG, MakeWindowTitle(_("Tip of the Day")),
	                        NULL, FormButtonOk,
	                        NULL, NULL,
	                        FALSE,
	                        F_CENTER, NULL );

	/* open the tip file */
	MakeFullpath(&filename, libDir, sTipF, NULL);
	tipF = fopen( filename, "r" );

	/* if tip file could not be opened, the only tip is an error message for the situation */
	if (tipF == NULL) {
		DYNARR_APPEND( char *, tips_da, 1 );
		tips(0) = N_("No tips are available");

		wControlActive( PREVIOUSBUTTON, FALSE );
		wControlActive( NEXTBUTTON, FALSE );
	} else {
		/* read all the tips from the file */
		while (fgets( buff, sizeof buff, tipF )) {

			/* lines starting with hash sign are ignored (comments) */
			if (buff[0] == '#') {
				continue;
			}

			/* remove CRs and LFs at end of line */
			cp = buff+strlen(buff)-1;
			if (*cp=='\n') { cp--; }
			if (*cp=='\r') { cp--; }

			/* get next line if the line was empty */
			if (cp < buff) {
				continue;
			}

			cp[1] = 0;

			/* if line ended with a continuation sign, get the rest */
			while (*cp=='\\') {
				/* put LF at end */
				*cp++ = '\n';

				/* read a line */
				if (!fgets( cp, (int)((sizeof buff) - (cp-buff)), tipF )) {
					break;
				}

				/* lines starting with hash sign are ignored (comments) */
				if (*cp=='#') {
					continue;
				}

				/* remove CRs and LFs at end of line */
				cp += strlen(cp)-1;
				if (*cp=='\n') { cp--; }
				if (*cp=='\r') { cp--; }
				cp[1] = 0;
			}

			/* allocate memory for the tip and store pointer in dynamic array */
			DYNARR_APPEND( char *, tips_da, 10 );
			tips(tips_da.cnt-1) = strdup( buff );
			if(tips(tips_da.cnt-1) == NULL ) {
				tips(tips_da.cnt-1) = "";
			}
		}
	}

	fclose(tipF);
	free(filename);
}

/**
 * Show tip of the day. As far as necessary, the dialog is created. The index of
 * the last tip shown is retrieved from the preferences and the next tip is
 * selected. At the end, the index of the shown tip is saved into the preferences.
 *
 * \param IN flags see definitions in smalldlg.h for possible values
 *
 */

void ShowTip( void * flagsVP )
{
	long flags = VP2L(flagsVP);
	long tipNum;

	wPrefGetInteger("tip", "showatstart", &showTipAtStart, 1);

	if (showTipAtStart || (flags & SHOWTIP_FORCESHOW)) {
		if (tipW == NULL) {
			CreateTipW();
		}
		FormLoadControls( &tipPG );
		wTextClear( tipT );
		/*  initial value is -1 which gets incremented 0 below */
		wPrefGetInteger( "tip", "number", &tipNum, -1 );

		if( flags & SHOWTIP_PREVTIP ) {
			if(tipNum == 0 ) {
				tipNum = tips_da.cnt - 1;
			} else {
				tipNum--;
			}
		} else {
			if (tipNum >= tips_da.cnt - 1) {
				tipNum = 0;
			} else {
				tipNum++;
			}
		}

		wTextAppend( tipT, _(tips(tipNum)) );

		wPrefSetInteger( "tip", "number", tipNum );
		wShow( tipW );
	}
}

/*--------------------------------------------------------------------*/

#define ABOUT_TEXT DESCRIPTION \
		"\n\nXTrackCAD is Copyright 2003 by Sillub Technology and 2017" \
		" by Bob Blackwell, Martin Fischer, Adam Richards and Russell Shilling.\n" \
		"\nIcons by: Tango Desktop Project (http://tango.freedesktop.org)\n" \
		"\nSome icons by Yusuke Kamiyamane." \
		"Licensed under a Creative Commons Attribution 3.0 License.\n" \
		"\nContributions by: Robert Heller, Mikko Nissinen," \
		"Timothy M. Shead,  Daniel Luis Spagnol" \
		"\nParameter Files by: Ralph Boyd, Dwayne Ward\n" \
		"\nThe following software is distributed with XTrackCAD\n\n" \
		"Cornu Algorithm and Implementation by: Raph Levien" \
		"\n\nuthash, utlist Copyright notice:" \
		"\nCopyright (c) 2005-2015, Troy D. Hanson http://troydhanson.github.com/uthash/" \
		"\nAll rights reserved." \
		"\n\ncJSON: Copyright (c) 2009-2017 Dave Gamble and cJSON contributors" \
		"\n\nlibzip:  Copyright(C) 1999 - 2019 Dieter Baron and Thomas Klausner\n" \
		"The authors can be contacted at libzip@nih.at" \
		"\n\nMiniXML: Copyright (c) 2003-2019 by Michael R Sweet.\n" \
		"The Mini - XML library is licensed under the Apache License Version 2.0 with an\n" \
		"exception to allow linking against GPL2 / LGPL2 - only software."

static paramTextData_t aboutTextData = { 50, 10 };

#define DESCRIPTION N_("XTrackCAD is a CAD (computer-aided design) program for designing model railroad layouts.")
static paramData_t aboutPLs[] = {
#define I_ABOUTDRAW				(0)
	{   PD_BITMAP, NULL, "about", PDO_NOPSHUPD | PDO_SAMEROW, NULL, NULL, 0 },
#define I_ABOUTVERSION			(1)
	{   PD_MESSAGE, NULL, "mess1", PDO_DLGNEWCOLUMN, NULL, NULL, BM_LARGE },
#define I_COPYRIGHT				 (2)
#define COPYRIGHT_T			(aboutPLs[I_COPYRIGHT].control)
	{   PD_TEXT, NULL, "text", PDO_DLGNEWCOLUMN, &aboutTextData, NULL, BO_READONLY | BT_TOP | BT_CHARUNITS}
};
static paramGroup_t aboutPG = { "about", 0l, aboutPLs, COUNT( aboutPLs ) };

/**
 *	Create and show the About window.
 */

void CreateAboutW(void *ptr)
{
	if (!aboutW) {
		aboutPLs[I_ABOUTDRAW].winData = CreateSymbolFromResource("xtc.png");
		FormRegister(&aboutPG);
		aboutW = FormCreateDialog(&aboutPG, MakeWindowTitle(_("About")),
		                          NULL, NULL,
		                          "Close", FormCancel_Current,
		                          FALSE, F_TOP | F_CENTER, NULL);
		wMessageSetValue(aboutPLs[I_ABOUTVERSION].control, sAboutProd);
		wTextAppend(COPYRIGHT_T, ABOUT_TEXT);
	}

	wShow(aboutW);

}

/*--------------------------------------------------------------------*/

/**
 * Initialize the functions for small dialogs.
 */

void InitSmallDlg( void )
{
}
