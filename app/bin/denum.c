/** \file denum.c
 * Creating and showing the parts list.
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
	{   PD_BUTTON, (void*)DoEnumOp, "save", PDO_DLGCMDBUTTON, NULL, N_("Save As ..."), 0, (void*)ENUMOP_SAVE },
	{   PD_BUTTON, (void*)DoEnumOp, "print", 0, NULL, N_("Print"), 0, (void*)ENUMOP_PRINT },
	{   PD_BUTTON, (void*)wPrintSetup, "printsetup", 0, NULL, N_("Print Setup"), 0, NULL },
#define I_ENUMLISTPRICE	(4)
	{   PD_TOGGLE, &enableListPrices, "list-prices", PDO_DLGRESETMARGIN, priceLabels, NULL, BC_HORZ|BC_NOBORDER } };
static paramGroup_t enumPG = { "enum", 0, enumPLs, sizeof enumPLs/sizeof enumPLs[0] };

static struct wFilSel_t * enumFile_fs;


static int count_utf8_chars(char *s) {
	int i = 0, j = 0;
	while (s[i]) {
		if ((s[i] & 0xc0) != 0x80) j++;
		i++;
	}
	return j;
}

static int DoEnumSave(
		int files,
		char **fileName,
		void * data )
{
	assert( fileName != NULL );
	assert( files == 1 );
	
	SetCurrentPath( PARTLISTPATHKEY, fileName[0] );
	return wTextSave( enumT, fileName[ 0 ] );
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
	sprintf( message, "%*ld | %s\n", count_utf8_chars(_("Count")), count, desc );
	if (enableListPrices) {
		cp = message + strlen( message )-1;
		len = enumerateMaxDescLen-strlen(desc);
		if (len<0) len = 0;
		memset( cp, ' ', len );
		cp += len;
		if (price > 0.0) {
			sprintf( cp, " | %7.2f |%9.2f\n", price, price*count );
			enumerateTotal += price*count;
		} else {
			sprintf( cp, " | %-*s |\n", (int) max( 7, count_utf8_chars( _("Each"))), " " );
		}
	}
	wTextAppend( enumT, message );
}

void EnumerateStart(void)
{
	time_t clock;
	struct tm *tm;
	char * cp;

	if (enumW == NULL) {
		ParamRegister( &enumPG );
		enumW = ParamCreateDialog( &enumPG, MakeWindowTitle(_("Parts List")), NULL, NULL, wHide, TRUE, NULL, F_RESIZE, EnumDlgUpdate );
		enumFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("Parts List"), sPartsListFilePattern, DoEnumSave, NULL );
	}

	wTextClear( enumT );

	sprintf( message, _("%s Parts List\n\n"), sProdName);
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
    tm = localtime(&clock);
	strftime( message, STR_LONG_SIZE, "%x\n", tm );
	wTextAppend( enumT, message );

	enumerateTotal = 0.0;

	if( count_utf8_chars( _("Description")) > enumerateMaxDescLen )
		enumerateMaxDescLen = count_utf8_chars( _("Description" ));

	/* create the table header */
	sprintf( message, "%s | %-*s", _("Count"), enumerateMaxDescLen, _("Description"));

	if( enableListPrices )
		sprintf( message+strlen(message), " | %-*s | %-*s\n", (int) max( 7, count_utf8_chars( _("Each"))), _("Each"), (int) max( 9, count_utf8_chars(_("Extended"))), _("Extended"));
	else 
		strcat( message, "\n" );
	wTextAppend( enumT, message );

	/* underline the header */
	cp = message;
	while( *cp && *cp != '\n' )			
		if( *cp == '|' )
			*cp++ = '+';
		else
			*cp++ = '-';

	wTextAppend( enumT, message );
}
/**
 * End of parts list. Print the footer line and the totals if necessary.
 * \todo These formatting instructions could be re-written in an easier 
 * to understand fashion using the possibilities of the printf formatting
 * and some string functions.
 */

void EnumerateEnd(void)
{
	int len;
	char * cp;
	ScaleLengthEnd();
	
	memset( message, '\0', STR_LONG_SIZE );
	memset( message, '-', strlen(_("Count")) + 1 );
	strcpy( message + strlen(_("Count")) + 1, "+");
	cp = message+strlen(message);
	memset( cp, '-', enumerateMaxDescLen+2 );
	if (enableListPrices){
		strcpy( cp+enumerateMaxDescLen+2, "+-" );
		memset( cp+enumerateMaxDescLen+4, '-', max( 7, strlen( _("Each"))));
		strcat( cp, "-+-");
		memset( message+strlen( message ), '-', max( 9, strlen(_("Extended"))));
		*(message + strlen( message )) = '\n';
	} else {
		*(cp+enumerateMaxDescLen+2) = '\n';
		*(cp+enumerateMaxDescLen+3) = '\0';
	}
	wTextAppend( enumT, message );

	if (enableListPrices) {
		len = strlen( message ) - strlen( _("Total")) - max( 9, strlen(_("Extended"))) - 4 ;
		memset ( message, ' ', len );
		cp = message+len;
		sprintf( cp, ("%s |%9.2f\n"), _("Total"), enumerateTotal );
		wTextAppend( enumT, message );
	}
	wTextSetPosition( enumT, 0 );

	ParamLoadControls( &enumPG );
	wShow( enumW );
}
