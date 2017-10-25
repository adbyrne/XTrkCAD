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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#ifndef WINDOWS
#include <unistd.h>
#include <dirent.h>
#endif
#ifdef WINDOWS
#include <io.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif

#include "common.h"
#include "custom.h"
#include "draw.h"
#include "fileio.h"
#include "i18n.h"
#include "misc.h"
#include "paths.h"
#include "param.h"
#include "smalldlg.h"
#include "wlib.h"

extern char *sTipF;
wWin_p aboutW;
static wWin_p tipW;					/**< window handle for tip dialog */

static long showTipAtStart = 1;		/**< flag for visibility */

static dynArr_t tips_da;			/**< dynamic array for all tips */
#define tips(N) DYNARR_N( char *, tips_da, N )

static char * tipLabels[] = { N_("Show tips at start"), NULL };
static paramTextData_t tipTextData = { 40, 10 };

static paramData_t tipPLs[] = {
#define I_TIPTEXT		(1)
#define tipT			((wText_p)tipPLs[I_TIPTEXT].control)
	{   PD_MESSAGE, N_("Did you know..."), NULL, 0, NULL, NULL, BM_LARGE },
	{   PD_TEXT, NULL, "text", 0, &tipTextData, NULL, BO_READONLY|BT_CHARUNITS },
	{   PD_BUTTON, (void*)ShowTip, "prev", PDO_DLGRESETMARGIN, NULL, N_("Previous Tip"), 0L, (void *)(SHOWTIP_FORCESHOW | SHOWTIP_PREVTIP) },	
	{   PD_BUTTON, (void*)ShowTip, "next", PDO_DLGHORZ, NULL, N_("Next Tip"), 0L, (void *)(SHOWTIP_FORCESHOW | SHOWTIP_NEXTTIP) },
	{   PD_TOGGLE, &showTipAtStart, "showatstart", PDO_DLGCMDBUTTON, tipLabels, NULL, BC_NOBORDER }};

static paramGroup_t tipPG = { "tip", 0, tipPLs, sizeof tipPLs/sizeof tipPLs[0] };

/**
 * Create and initialize the tip of the day window. The dialog box is created and the list of tips is loaded
 * into memory. 
 */

static void CreateTipW( void )
{
	FILE * tipF;
	char buff[4096];
	char *filename;
	char * cp;

	tipW = ParamCreateDialog( &tipPG, MakeWindowTitle(_("Tip of the Day")), _("Ok"), (paramActionOkProc)wHide, NULL, FALSE, NULL, F_CENTER, NULL );

	/* open the tip file */
	MakeFullpath(&filename, libDir, sTipF, NULL);
	tipF = fopen( filename, "r" );
	
	/* if tip file could not be opened, the only tip is an error message for the situation */
	if (tipF == NULL) {
		DYNARR_APPEND( char *, tips_da, 1 );
		tips(0) = N_("No tips are available");
/*	TODO: enable buttons only if tips are available
		wControlActive( prev, FALSE );
		wControlActive( next, FALSE ); */
	} else {
		/* read all the tips from the file */
		while (fgets( buff, sizeof buff, tipF )) {

			/* lines starting with hash sign are ignored (comments) */
			if (buff[0] == '#')
				continue;
				
			/* remove CRs and LFs at end of line */				
			cp = buff+strlen(buff)-1;
			if (*cp=='\n') cp--;
			if (*cp=='\r') cp--;
			
			/* get next line if the line was empty */
			if (cp < buff)
				continue;

			cp[1] = 0;

			/* if line ended with a continuation sign, get the rest */
			while (*cp=='\\') {
				/* put LF at end */
				*cp++ = '\n';
				
				/* read a line */	
				if (!fgets( cp, (sizeof buff) - (cp-buff), tipF )) {
					return;
				}
	
				/* lines starting with hash sign are ignored (comments) */
				if (*cp=='#')
					continue;

				/* remove CRs and LFs at end of line */				
				cp += strlen(cp)-1;
				if (*cp=='\n') cp--;
				if (*cp=='\r') cp--;
				cp[1] = 0;
			}
			
			/* allocate memory for the tip and store pointer in dynamic array */
			DYNARR_APPEND( char *, tips_da, 10 );
			tips(tips_da.cnt-1) = strdup( buff );
		}
	}
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

void ShowTip( long flags )
{
	long tipNum;
	
	if (showTipAtStart || (flags & SHOWTIP_FORCESHOW))  
	{
		if (tipW == NULL) {
			CreateTipW();
		}
		ParamLoadControls( &tipPG );
		wTextClear( tipT );
		wPrefGetInteger( "misc", "tip-number", &tipNum, 0 );
		
		if( flags & SHOWTIP_PREVTIP ) {
			if(tipNum == 0 )
				tipNum = tips_da.cnt - 1;
			else
				tipNum--;	
		} else {
			if (tipNum >= tips_da.cnt - 1)
				tipNum = 0;
			else
				tipNum++;	
		}		

		wTextAppend( tipT, _(tips(tipNum)) );

		wPrefSetInteger( "misc", "tip-number", tipNum );
		wShow( tipW );
	}	
}

/*--------------------------------------------------------------------*/

#include "bitmaps/xtc.xpm"

static paramTextData_t aboutTextData = { 70, 10 };

#define DESCRIPTION N_("XTrackCAD is a CAD (computer-aided design) program for designing model railroad layouts.")
static paramData_t aboutPLs[] = {
#define I_ABOUTDRAW				(0)
	{   PD_BITMAP, NULL, "about", PDO_NOPSHUPD, NULL, NULL, 0 },
#define I_ABOUTVERSION			(1)
	{   PD_MESSAGE, NULL, NULL, PDO_DLGNEWCOLUMN, NULL, NULL, BM_LARGE },
#define I_COPYRIGHT				 (2)
#define COPYRIGHT_T			((wText_p)aboutPLs[I_COPYRIGHT].control)
	{   PD_TEXT, NULL, NULL, PDO_DLGRESIZE, &aboutTextData, NULL, BT_CHARUNITS }
};
static paramGroup_t aboutPG = { "about", 0, aboutPLs, sizeof aboutPLs/sizeof aboutPLs[0] };

/** 
 *	Create and show the About window.
 */

void CreateAboutW( void *ptr )
{
	char *copyright = sAboutProd;
	
	if( !aboutW ) {
		aboutPLs[I_ABOUTDRAW].winData = wIconCreatePixMap( xtc_xpm );
		ParamRegister( &aboutPG );
		aboutW = ParamCreateDialog( &aboutPG, MakeWindowTitle(_("About")), _("Ok"), (paramActionOkProc)wHide, NULL, FALSE, NULL, F_TOP|F_CENTER, NULL );
		ParamLoadMessage( &aboutPG, I_ABOUTVERSION, sAboutProd );
		wTextAppend( COPYRIGHT_T, DESCRIPTION );
		wTextAppend( COPYRIGHT_T, "\n\nXTrackCAD is Copyright 2003 by Sillub Technology and 2007 by Martin Fischer and Bob Blackwell." );
		wTextAppend( COPYRIGHT_T, "\n\nIcons by: Tango Desktop Project (http://tango.freedesktop.org)");
		wTextAppend( COPYRIGHT_T, "\n\nContributions by: Robert Heller, Mikko Nissinen, Timothy M. Shead, Daniel Luis Spagnol" );
		wTextAppend( COPYRIGHT_T, "\n\nParameter Files by: Ralph Boyd, Dwayne Ward" );
		wTextAppend( COPYRIGHT_T, "\n\nuthash Copyright notice:" );
		wTextAppend( COPYRIGHT_T, "\nCopyright (c) 2005-2015, Troy D. Hanson  http://troydhanson.github.com/uthash/");
		wTextAppend( COPYRIGHT_T, "\nAll rights reserved.");
	} 
		
	wShow( aboutW );
}	

/*--------------------------------------------------------------------*/

/**
 * Initialize the functions for small dialogs.
 */
 
void InitSmallDlg( void )
{
	ParamRegister( &tipPG );     
}	
