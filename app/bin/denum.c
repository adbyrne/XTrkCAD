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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <time.h>

#include "custom.h"
#include <dynstring.h>
#include "fileio.h"
#include "layout.h"
#include "form.h"
#include "paths.h"
#include "track.h"

static int log_denum = -1;

static wControl_p enumW;

#define ENUMOP_SAVE		(1)
#define ENUMOP_PRINT	(5)
#define ENUMOP_CLOSE	(6)

#undef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))

static void DoEnumOp( void * data );
static long enableListPrices;
static long enableListIndexes;

static paramTextData_t enumTextData = { 0, 0 };
static paramData_t enumPLs[] = {
#define I_ENUMTEXT		(0)
#define enumT			(enumPLs[I_ENUMTEXT].control)
	{   PD_TEXT, NULL, "text", PDO_DLGRESIZE, &enumTextData },
	{   PD_BUTTON, DoEnumOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, I2VP(ENUMOP_SAVE) },
	{   PD_BUTTON, DoEnumOp, "print", 0, NULL, NULL, 0, I2VP(ENUMOP_PRINT) },
	{   PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
#define I_ENUMLISTPRICE	(4)
	{   PD_TOGGLE, &enableListPrices, "list-prices" },
#define I_ENUMLISTINDEXES  (5)
	{   PD_TOGGLE, &enableListIndexes, "list-indexes" }
};
static paramGroup_t enumPG = { "enum", PGO_FULLDIALOGFROMBUILDER, enumPLs, COUNT( enumPLs ) };

static struct wFilSel_t * enumFile_fs;


static int count_utf8_chars(const char *s)
{
	int i = 0, j = 0;
	while (s[i]) {
		if ((s[i] & 0xc0) != 0x80) { j++; }
		i++;
	}
	return j;
}

static int DoEnumSave(
        int files,
        char **fileName,
        void * data )
{
	CHECK( fileName != NULL );
	CHECK( files == 1 );

	SetCurrentPath( PARTLISTPATHKEY, fileName[0] );
	return wTextSave( enumT, fileName[ 0 ] );
}


static void DoEnumOp(
        void * data )
{
	switch( VP2L(data) ) {
	case ENUMOP_SAVE:
		wFilSelect( enumFile_fs, GetCurrentPath(PARTLISTPATHKEY) );
		break;
	case ENUMOP_PRINT:
		wTextPrint( enumT );
		break;
	case ENUMOP_CLOSE:
		wHide( enumW );
		FormUpdate( &enumPG );
		break;
	default:
		if ( log_denum < 0 ) { log_denum = LogFindIndex( "denum" ); }
		LOG( log_denum, 1, ( "unexpected VP2L(data) %d in DoEnumOp\n", VP2L(data) ) )
		break;
	}
}


static void EnumDlgUpdate(
        paramGroup_p pg,
        int inx,
        void * valueP )
{
	if ( inx != I_ENUMLISTPRICE && inx != I_ENUMLISTINDEXES) { return; }
	EnumerateTracks( NULL );
}


int enumerateMaxDescLen;
static FLOAT_T enumerateTotal;

void EnumerateList(
        long count,
        FLOAT_T price,
        const char * desc,
        const char * indexes )
{
	char * cp;
	long len;
	sprintf( message, "%*ld | %s\n", count_utf8_chars(_("Count")), count, desc );
	if (enableListPrices) {
		cp = message + strlen( message )-1;
		len = (long)enumerateMaxDescLen-(long)strlen(desc);
		if (len<0) { len = 0; }
		memset( cp, ' ', len );
		cp += len;
		if (price > 0.0) {
			sprintf( cp, " | %7.2f |%9.2f\n", price, price*count );
			enumerateTotal += price*count;
		} else {
			sprintf( cp, " | %-*s |\n", (int) max( 7, count_utf8_chars( _("Each"))), " " );
		}
	}
	if (enableListIndexes && indexes) {
		sprintf( &message[strlen(message)], "%s -> %s \n", N_("Indexes"), indexes);
	}
	wTextAppend( enumT, message );
}

void
AddDateString(DynString* output)
{
	const struct tm *tm;
	time_t currentTime;
	size_t length = 8;
	char* formatted = malloc(length);

	time(&currentTime);
	tm = localtime(&currentTime);

	while (!strftime(formatted, length, "%x\n\n", tm)) {
		char *tmp;
		length *= 2;
		tmp = realloc(formatted, length);
		if (!tmp) {
			free(formatted);
			return;
		}
		formatted = tmp;
	}

	DynStringCatCStr(output, formatted);

	free(formatted);
}

void
CreateTableFooter(void)
{
	char* line = malloc(STR_SIZE);
	char* cp;

	memset(line, '\0', STR_SIZE);
	memset(line, '-', strlen(_("Count")) + 1);
	strcpy(line + strlen(_("Count")) + 1, "+");
	cp = line + strlen(line);
	memset(cp, '-', enumerateMaxDescLen + 2);
	if (enableListPrices) {
		strcpy(cp + enumerateMaxDescLen + 2, "+-");
		memset(cp + enumerateMaxDescLen + 4, '-', max(7, strlen(_("Each"))));
		strcat(cp, "-+-");
		memset(line + strlen(line), '-', max(9, strlen(_("Extended"))));
		*(line + strlen(line)) = '\n';
	} else {
		*(cp + enumerateMaxDescLen + 2) = '\n';
		*(cp + enumerateMaxDescLen + 3) = '\0';
	}
	wTextAppend(enumT, line);

	free(line);
}

static void
CreateHeader(void)
{
	DynString headerLine;
	DynStringMalloc(&headerLine, 256);

	DynStringPrintf(&headerLine, _("%s Parts List\n"), sProdName);

	if (*GetLayoutTitle()) {
		DynStringCatCStrs(&headerLine, GetLayoutTitle(), "\n", NULL);
	}
	if (*GetLayoutSubtitle()) {
		DynStringCatCStrs(&headerLine, GetLayoutSubtitle(), "\n", NULL);
	}

	AddDateString(&headerLine);

	wTextAppend(enumT, DynStringToCStr(&headerLine));

	DynStringFree(&headerLine);
}

static void
CreateTableHeader(void)
{
	DynString line;
	enumerateTotal = 0.0;

	DynStringMalloc(&line, 80);

	if (count_utf8_chars(_("Description")) > enumerateMaxDescLen) {
		enumerateMaxDescLen = count_utf8_chars(_("Description"));
	}

	if (enableListPrices) {
		DynStringPrintf(&line, "%s | %-*s | %-*s | %-*s\n",
		                _("Count"),
		                enumerateMaxDescLen, _("Description"),
		                (int)max(7,	count_utf8_chars(_("Each"))), _("Each"),
		                (int)max(9,	count_utf8_chars(_("Extended"))), _("Extended"));
	} else {
		DynStringPrintf(&line, "%s | %-*s\n", _("Count"), enumerateMaxDescLen,
		                _("Description"));
	}

	wTextAppend(enumT, DynStringToCStr(&line));

	CreateTableFooter();

	DynStringFree(&line);
}

void EnumerateStart(void)
{

	if (enumW == NULL) {
		FormRegister( &enumPG );
		enumW = FormCreateDialog( &enumPG, MakeWindowTitle(_("Parts List")),
		                          NULL, NULL,
		                          NULL, FormCancel_Current,
		                          TRUE, F_RESIZE,
		                          EnumDlgUpdate);
		enumFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("Parts List"),
		                             sPartsListFilePattern, DoEnumSave, NULL );
	}

	wTextClear( enumT );

	CreateHeader();
	CreateTableHeader();

}
/**
 * End of parts list. Print the footer line and the totals if necessary.
 * \todo These formatting instructions could be re-written in an easier
 * to understand fashion using the possibilities of the printf formatting
 * and some string functions.
 */

void EnumerateEnd(void)
{
	size_t len;
	char * cp;
	ScaleLengthEnd();

	CreateTableFooter();

	/**  \todo Fix layout of summary line, why -3? */
	if (enableListPrices) {
		len = strlen( message ) - strlen( _("Total")) - max( 9,
		        strlen(_("Extended"))) - 3 ;
		memset ( message, ' ', len );
		cp = message+len;
		sprintf( cp, ("%s |%9.2f\n"), _("Total"), enumerateTotal );
		wTextAppend( enumT, message );
	}
	wTextSetPosition( enumT, 0 );

	FormLoadControls( &enumPG );
	wShow( enumW );
}
