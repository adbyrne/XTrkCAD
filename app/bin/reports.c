/** \file reports.c
 * Native "Reports" feature (SF #217), phase 1: a generic report-text
 * viewer (Save/Print/Print Setup, same shape as denum.c's "Parts List"
 * dialog -- see the phase-1 implementation plan for why this is built
 * generic from the start, meant for every future report phase to reuse)
 * plus the phase-1 report itself, unconnected track endpoints.
 *
 * The date-header logic (AddReportDateString()) is deliberately duplicated
 * from denum.c's AddDateString() rather than shared, to keep this
 * feature's first PR scoped to new files only -- see the phase-1
 * implementation plan's "Shared helper duplication" section for the
 * reasoning and the follow-up cleanup this leaves flagged (extract to
 * app/dynstring/).
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2026 XTrkCAD contributors
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
#include "include/reports.h"

static wControl_p reportsW;

#define REPORTSOP_SAVE  (1)
#define REPORTSOP_PRINT (2)

static void DoReportsOp(void *data);

static paramTextData_t reportsTextData = { 0, 0 };
static paramData_t reportsPLs[] = {
#define I_REPORTSTEXT (0)
#define reportsT (reportsPLs[I_REPORTSTEXT].control)
	{ PD_TEXT, NULL, "text", PDO_DLGRESIZE, &reportsTextData },
	{ PD_BUTTON, DoReportsOp, "save", PDO_DLGCMDBUTTON, NULL, NULL, 0, I2VP(REPORTSOP_SAVE) },
	{ PD_BUTTON, DoReportsOp, "print", 0, NULL, NULL, 0, I2VP(REPORTSOP_PRINT) },
	{ PD_BUTTON, wPrintSetup, "printsetup", 0, NULL, NULL, 0, NULL },
};
static paramGroup_t reportsPG = { "reports", PGO_FULLDIALOGFROMBUILDER, reportsPLs, COUNT( reportsPLs ) };

static struct wFilSel_t * reportsFile_fs;

static int DoReportsSave(
        int files,
        char **fileName,
        void * data )
{
	CHECK( fileName != NULL );
	CHECK( files == 1 );

	SetCurrentPath( REPORTPATHKEY, fileName[0] );
	return wTextSave( reportsT, fileName[ 0 ] );
}

static void DoReportsOp(
        void * data )
{
	switch( VP2L(data) ) {
	case REPORTSOP_SAVE:
		wFilSelect( reportsFile_fs, GetCurrentPath(REPORTPATHKEY) );
		break;
	case REPORTSOP_PRINT:
		wTextPrint( reportsT );
		break;
	default:
		break;
	}
}

/** Duplicated from denum.c's AddDateString() -- see file header. */
static void AddReportDateString(DynString* output)
{
	struct tm tm;
	time_t currentTime;
	size_t length = 8;
	char* formatted = malloc(length);

	time(&currentTime);
#ifdef WINDOWS
	localtime_s(&tm, &currentTime);
#else
	localtime_r(&currentTime, &tm);
#endif

	while (!strftime(formatted, length, "%x\n\n", &tm)) {
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

void ReportsShowText(const char *title, DynString *content)
{
	if (reportsW == NULL) {
		FormRegister( &reportsPG );
		reportsW = FormCreateDialog( &reportsPG, MakeWindowTitle(title),
		                             NULL, NULL,
		                             NULL, FormCancel_Current,
		                             TRUE, F_RESIZE,
		                             NULL );
		reportsFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, title,
		                                sReportsFilePattern, DoReportsSave, NULL );
	}
	/* NOTE: subsequent calls reuse the same window/title -- fine while
	 * phase 1 is the only report; retitling on reuse becomes relevant
	 * once phase 2 adds a second report type, deliberately not solved
	 * here (see the phase-1 implementation plan). */

	wTextClear( reportsT );
	wTextAppend( reportsT, DynStringToCStr(content) );
	wTextSetPosition( reportsT, 0 );

	FormLoadControls( &reportsPG );
	wShow( reportsW );
}

/** Header text shared by every report: product/layout title + date.
 * Report-specific content is appended by the caller. */
static void ReportsAddHeader(DynString *out, const char *reportTitle)
{
	DynStringPrintf(out, "%s %s\n", sProdName, reportTitle);

	if (*GetLayoutTitle()) {
		DynStringCatCStrs(out, GetLayoutTitle(), "\n", NULL);
	}
	if (*GetLayoutSubtitle()) {
		DynStringCatCStrs(out, GetLayoutSubtitle(), "\n", NULL);
	}

	AddReportDateString(out);
}

void ReportsUnconnectedEndpoints( void * unused )
{
	track_p trk;
	dynArr_t list_da;
	DynString content;

	DYNARR_INIT( reportsEndPt_t, list_da );

	TRK_ITERATE( trk ) {
		EPINX_T ep;
		for ( ep = 0; ep < GetTrkEndPtCnt(trk); ep++ ) {
			if ( GetTrkEndTrk(trk, ep) == NULL ) {
				reportsEndPt_t *entry;
				DYNARR_APPEND( reportsEndPt_t, list_da, 10 );
				entry = &DYNARR_LAST( reportsEndPt_t, list_da );
				entry->trackId = GetTrkIndex(trk);
				entry->pos = GetTrkEndPos(trk, ep);
				entry->angle = GetTrkEndAngle(trk, ep);
			}
		}
	}

	DynStringMalloc( &content, 256 );
	ReportsAddHeader( &content, _("Unconnected Endpoints Report") );

	if ( list_da.cnt == 0 ) {
		DynStringCatCStrs( &content, "\n", _("No unconnected endpoints found."), "\n",
		                   NULL );
	} else {
		/* snprintf into a local buffer, then append -- DynStringPrintf()
		 * formats INTO its target starting at offset 0 (replacing whatever
		 * was already there, same as plain sprintf into a fresh buffer),
		 * it does not append. Calling it directly on `content` here would
		 * silently wipe out the header ReportsAddHeader() just wrote --
		 * exactly the bug this comment is here to stop someone
		 * reintroducing. Width 6 for the Track column matches
		 * ReportsFormatUnconnectedList()'s "%6d" exactly, so header and
		 * data rows stay aligned. */
		char headerLine[64];
		snprintf( headerLine, sizeof headerLine, "\n%6s | %8s | %8s | %7s\n",
		          _("Track"), _("X"), _("Y"), _("Angle") );
		DynStringCatCStr( &content, headerLine );
		ReportsFormatUnconnectedList( &content, &DYNARR_N(reportsEndPt_t, list_da, 0),
		                              list_da.cnt );
	}

	DYNARR_FREE( reportsEndPt_t, list_da );

	ReportsShowText( _("Unconnected Endpoints Report"), &content );

	DynStringFree( &content );
}
