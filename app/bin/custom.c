#define RENAME_H
/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/custom.c,v 1.5 2008-01-28 06:23:02 mni77 Exp $
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis
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

#include <stdlib.h>
#include <stdio.h>
#ifndef WINDOWS
#include <unistd.h>
#include <dirent.h>
#endif
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifdef WINDOWS
#include <io.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif
#include <stdarg.h>
#include <errno.h>

#include "track.h"
#include "version.h"
#include "common.h"
#include "misc.h"
#include "fileio.h"
#include "cjoin.h"
#include "i18n.h"

#define Product "XTrackCAD"
#define product "xtrkcad"
#define PRODUCT "XTRKCAD"
#define Version VERSION
#define KEYCODE "x"
#define NOFLEX
#define ENABLEFLEX
#define ENABLEPARAMFILES
#define ENABLEPRICES
#define PARAMKEY		(0)


char * sProdName = Product;
char * sProdNameLower = product;
char * sProdNameUpper = PRODUCT;

char * sEnvExtra = PRODUCT "EXTRA";

char * sTurnoutDesignerW = NULL;

char * sAboutProd = NULL;

char * sCustomF = product ".cus";
char * sCheckPointF = product ".ckp";
char * sCheckPoint1F = product ".ck1";
char * sClipboardF = product ".clp";
char * sParamQF = product "." KEYCODE "tq";
char * sUndoF = product ".und";
char * sAuditF = product ".aud";

char * sSourceFilePattern = NULL;
char * sImportFilePattern = NULL;
char * sDXFFilePattern = NULL;
char * sRecordFilePattern = NULL;
char * sNoteFilePattern = NULL;
char * sLogFilePattern = NULL;
char * sPartsListFilePattern = NULL;

char * sVersion = Version;
int iParamVersion = PARAMVERSION;
int iMinParamVersion = MINPARAMVERSION;
long lParamKey = PARAMKEY;

#ifdef ENABLEFLEX
int bEnableFlex = 1;
#else
int bEnableFlex = 0;
#endif

#ifdef ENABLEPARAMFILES
int bParamFiles = 1;
#else
int bParamFiles = 0;
#endif

#ifdef ENABLEPRICES
int bEnablePrices = 1;
#else
int bEnablePrices = 0;
#endif

EXPORT char * MakeWindowTitle( char * name )
{
	static char title[STR_SHORT_SIZE];
	sprintf( title, "%s", name );
	return title;
}

static addButtonCallBack_t easementP;

void InitCmdEasement( void )
{
	easementP = EasementInit();
}
void DoEasementRedir( void )
{
	if (easementP)
		easementP(NULL);
}

#ifdef STRUCTDESIGNER
static addButtonCallBack_t structDesignerP;
void DoStructDesignerRedir( void )
{
	if (structDesignerP)
		structDesignerP(NULL);
}
#endif

void InitNewTurnRedir( wMenu_p m )
{
#ifdef ENABLEFLEX
	InitNewTurn(m);
#endif
}

#include "xtc64.bmp"
#define icon64_width	xtc64_width
#define icon64_height	xtc64_height
#define icon64_bits		xtc64_bits
#include "xtc16.bmp"
#define icon16_width	xtc16_width
#define icon16_height	xtc16_height
#define icon16_bits		xtc16_bits

#if ICON_WIDTH != icon64_width
error
#endif
#if ICON_HEIGHT != icon64_height
error
#endif

void RedrawAbout(
		wDraw_p d,
		void * context,
		wPos_t w,
		wPos_t h )
{
	static wDrawBitMap_p aboutBM = NULL;
	if (aboutBM == NULL)
		aboutBM = wDrawBitMapCreate( d, w, h, 0, 0, icon64_bits );
	wDrawClear( d );
	wDrawBitMap( d, aboutBM, 0, 0, wDrawColorBlack, 0 );
}

BOOL_T Initialize( void )
{
	wWinSetBigIcon( mainW, wIconCreateBitMap(icon64_width, icon64_height, icon64_bits, wDrawColorBlack) );
	wWinSetSmallIcon( mainW, wIconCreateBitMap(icon16_width, icon16_height, icon16_bits, wDrawColorBlack) );

	InitTrkCurve();
	InitTrkStraight();
	InitTrkEase();
	InitTrkTurnout();
	InitTrkTurntable();
	InitTrkStruct();
	InitTrkText();
	InitTrkDraw();
	InitTrkNote();

	InitCarDlg();

	memset( message, 0, sizeof message );
	
	return TRUE;
}


void InitCustom( void )
{
	char buf[STR_SHORT_SIZE];

	/* Initialize some localized strings */
	if (sTurnoutDesignerW == NULL)
	{
		sprintf(buf, _("%s Turnout Designer"), Product);
		sTurnoutDesignerW = strdup(buf);
	}
	if (sAboutProd == NULL)
	{
		sprintf(buf, _("%s Version %s"), Product, Version);
		sAboutProd = strdup(buf);
	}
	if (sSourceFilePattern == NULL)
	{
		sprintf(buf, _("%s Files|*.xtc"), Product);
		sSourceFilePattern = strdup(buf);
	}
	if (sImportFilePattern == NULL)
	{
		sprintf(buf, _("%s Import Files|*.%sti"), Product, KEYCODE);
		sImportFilePattern = strdup(buf);
	}
	if (sDXFFilePattern == NULL)
	{
		sprintf(buf, _("%s Import Files|*.%sti"), Product, KEYCODE);
		sDXFFilePattern = strdup(buf);
	}
	if (sRecordFilePattern == NULL)
	{
		sprintf(buf, _("%s Record Files|*.%str"), Product, KEYCODE);
		sRecordFilePattern = strdup(buf);
	}
	if (sNoteFilePattern == NULL)
	{
		sprintf(buf, _("%s Note Files|*.not"), Product);
		sNoteFilePattern = strdup(buf);
	}
	if (sLogFilePattern == NULL)
	{
		sprintf(buf, _("%s Log Files|*.log"), Product);
		sLogFilePattern = strdup(buf);
	}
	if (sPartsListFilePattern == NULL)
	{
		sprintf(buf, _("%s PartsList Files|*.log"), Product);
		sPartsListFilePattern = strdup(buf);
	}
}


void CleanupCustom( void )
{
	/* Free dynamically allocated strings */
	if (sTurnoutDesignerW)
	{
		free(sTurnoutDesignerW);
		sTurnoutDesignerW = NULL;
	}
	if (sAboutProd)
	{
		free(sAboutProd);
		sAboutProd = NULL;
	}
	if (sSourceFilePattern)
	{
		free(sSourceFilePattern);
		sSourceFilePattern = NULL;
	}
	if (sImportFilePattern)
	{
		free(sImportFilePattern);
		sImportFilePattern = NULL;
	}
	if (sDXFFilePattern)
	{
		free(sDXFFilePattern);
		sDXFFilePattern = NULL;
	}
	if (sRecordFilePattern)
	{
		free(sRecordFilePattern);
		sRecordFilePattern = NULL;
	}
	if (sNoteFilePattern)
	{
		free(sNoteFilePattern);
		sNoteFilePattern = NULL;
	}
	if (sLogFilePattern)
	{
		free(sLogFilePattern);
		sLogFilePattern = NULL;
	}
	if (sPartsListFilePattern)
	{
		free(sPartsListFilePattern);
		sPartsListFilePattern = NULL;
	}
}
