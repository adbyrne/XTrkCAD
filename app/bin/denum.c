/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/denum.c,v 1.3 2009-07-09 18:29:42 m_fischer Exp $
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

#include <time.h>
#include "track.h"
#include "i18n.h"

/****************************************************************************
 *
 * ENUMERATE
 *
 */


static wWin_p enumW;

#define ENUMOP_SAVE		(1)
#define ENUMOP_PRINT	(5)
#define ENUMOP_CLOSE	(6)

static void DoEnumOp( void * );
static long enableListPrices;

static paramTextData_t enumTextData = { 80, 24 };
static char * priceLabels[] = { N_("Prices"), NULL };
static paramData_t enumPLs[] = {
#define I_ENUMTEXT		(0)
#define enumT			((wText_p)enumPLs[I_ENUMTEXT].control)
	{   PD_TEXT, NULL, "text", PDO_DLGRESIZE, &enumTextData, NULL, BT_CHARUNITS|BT_FIXEDFONT },
	{   PD_BUTTON, DoEnumOp, "save", PDO_DLGCMDBUTTON, NULL, N_("Save As ..."), 0, (void*)ENUMOP_SAVE },
	{   PD_BUTTON, DoEnumOp, "print", 0, NULL, N_("Print"), 0, (void*)ENUMOP_PRINT },
	{   PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, N_("Print Setup"), 0, NULL },
#define I_ENUMLISTPRICE	(4)
	{   PD_TOGGLE, &enableListPrices, "list-prices", PDO_DLGRESETMARGIN, priceLabels, NULL, BC_HORZ|BC_NOBORDER } };
static paramGroup_t enumPG = { "enum", 0, enumPLs, sizeof enumPLs/sizeof enumPLs[0] };

static struct wFilSel_t * enumFile_fs;


static int DoEnumSave(
		const char * pathName,
		const char * fileName,
		void * data )
{
	if (pathName == NULL)
		return TRUE;
	memcpy( curDirName, pathName, fileName-pathName );
	curDirName[fileName-pathName-1] = '\0';
	return wTextSave( enumT, pathName );
}


static void DoEnumOp(
		void * data )
{
	switch( (int)(long)data ) {
	case ENUMOP_SAVE:
		wFilSelect( enumFile_fs, curDirName );
		break;
	case ENUMOP_PRINT:
		wTextPrint( enumT );
		break;
	case ENUMOP_CLOSE:
		wHide( enumW );
		ParamUpdate( &enumPG );
	}
}


static void EnumDlgUpdate(
		paramGroup_p pg,
		int inx,
		void * valueP )
{
	if ( inx != I_ENUMLISTPRICE ) return;
	EnumerateTracks();
}


int enumerateMaxDescLen;
static FLOAT_T enumerateTotal;

void EnumerateList(
		long count,
		FLOAT_T price,
		char * desc )
{
	char * cp;
	int len;
	sprintf( message, "%5ld | %s\n", count, desc );
	if (enableListPrices) {
		cp = message + strlen( message )-1;
		len = enumerateMaxDescLen-strlen(desc);
		if (len<0) len = 0;
		memset( cp, ' ', len );
		cp += len;
		if (price > 0.0) {
			sprintf( cp, " |%7.2f |%9.2f\n", price, price*count );
			enumerateTotal += price*count;
		} else {
			sprintf( cp, " |        |\n" );
		}
	}
	wTextAppend( enumT, message );
}

void EnumerateStart(void)
{
	time_t clock;
	int len;
	char * cp;
	if (enumW == NULL) {
		ParamRegister( &enumPG );
		enumW = ParamCreateDialog( &enumPG, MakeWindowTitle(_("Parts List")), NULL, NULL, wHide, TRUE, NULL, F_RESIZE, EnumDlgUpdate );
		enumFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("Parts List"), sPartsListFilePattern, DoEnumSave, NULL );
	}

	wTextClear( enumT );

	sprintf( message, "%s Parts List\n\n", sProdName);
	wTextAppend( enumT, message );

	message[0] = '\0';
	cp = message;
	if ( Title1[0] ) {
		strcpy( cp, Title1 );
		cp += strlen(cp);
		*cp++ = '\n';
	}
	if ( Title2[0] ) {
		strcpy( cp, Title2 );
		cp += strlen(cp);
		*cp++ = '\n';
	}
	if ( cp > message ) {
		*cp++ = '\n';
		*cp++ = '\0';
		wTextAppend( enumT, message );
	}

	time(&clock);
/*    tm = localtime(&clock);
	strftime( dat, sizeof dat, "%a %y %b %d", tm );*/
	sprintf( message, "%s\n", ctime(&clock) );
	wTextAppend( enumT, message );

	enumerateTotal = 0.0;

	if (enableListPrices) {
		sprintf( message, "Count | Description" );
		cp = message + strlen( message );
		len = enumerateMaxDescLen-strlen( "Description");
		memset( cp, ' ', len );
		cp += len;
		sprintf( cp, " |   Each | Extended\n" );
	} else {
		sprintf( message, "Count | Description\n" );
	}
	wTextAppend( enumT, message );

	sprintf( message, "------+");
	cp = message+strlen(message);
	memset( cp, '-', enumerateMaxDescLen+2 );
	if (enableListPrices)
		strcpy( cp+enumerateMaxDescLen+2, "+--------+----------\n");
	else {
		*(cp+enumerateMaxDescLen+2) = '\n';
		*(cp+enumerateMaxDescLen+3) = '\0';
	}
	wTextAppend( enumT, message );

}

void EnumerateEnd(void)
{
	int len;
	char * cp;
	ScaleLengthEnd();
	sprintf( message, "------+" );
	cp = message+strlen(message);
	memset( cp, '-', enumerateMaxDescLen+2 );
	if (enableListPrices)
		strcpy( cp+enumerateMaxDescLen+2, "+--------+----------\n");
	else {
		*(cp+enumerateMaxDescLen+2) = '\n';
		*(cp+enumerateMaxDescLen+3) = '\0';
	}
	wTextAppend( enumT, message );

	if (enableListPrices) {
		len = enumerateMaxDescLen-strlen( "Total");
		memset ( message, ' ', len+1 );
		cp = message+len+1;
		sprintf( cp, "                Total |%9.2f\n", enumerateTotal );
		wTextAppend( enumT, message );
	}
	wTextSetPosition( enumT, 0 );

	ParamLoadControls( &enumPG );
	wShow( enumW );
}
