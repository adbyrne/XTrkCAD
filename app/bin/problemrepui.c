/** \file problemrepui.c
 * UI for the problem report function
*/

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2024 Martin Fischer
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

#include <wlib.h>

#include "custom.h"
#include "fileio.h"
#include "layout.h"
#include "param.h"
#include "include/problemrep.h"

static wWin_p problemrepW;

#define DESCRIPTION N_("This function puts together information that can " \
						"be helpful to analyze a problem. Private information " \
						"like userids is removed\n\n")

static paramTextData_t textData = { 60, 10 };

static paramData_t problemrepPLs[] = {
#define I_PROBLEMREPPROGRESS				 (0)
#define PROBLEMREP_T			((wText_p)problemrepPLs[I_PROBLEMREPPROGRESS].control)
	{   PD_TEXT, NULL, NULL, PDO_DLGRESIZE, &textData, NULL, BO_READONLY | BT_TOP | BT_CHARUNITS }
};
static paramGroup_t problemrepPG = { "problemdata", 0, problemrepPLs, COUNT(problemrepPLs) };

/**
 *	Create and show the problem report window
 */

void ProblemrepCreateW(void* ptr)
{
	if (!problemrepW) {
		ParamRegister(&problemrepPG);

		problemrepW = ParamCreateDialog(&problemrepPG,
		                                MakeWindowTitle(_("Data for Problem Report")),
		                                NULL, NULL,	ParamCancel_Current, TRUE, NULL,
		                                F_TOP | F_CENTER | PD_F_ALT_CANCELLABEL, NULL);
	} else {
		wTextClear(PROBLEMREP_T);
	}
	wTextAppend(PROBLEMREP_T, DESCRIPTION);
	wShow(problemrepW);
}

void ProblemrepUpdateW(char* fmt, ...)
{
	int length;
	char* buffer;
	va_list args;
	va_list argsc;
	va_start(args, fmt);
	va_copy(argsc, args);

	length = vsnprintf(NULL, 0, fmt, args);
	buffer = MyMalloc(length + sizeof(NULL));
	va_end(args);

	if (buffer) {
		memset(buffer, 0, length +sizeof(NULL));

		vsnprintf(buffer, length+sizeof(NULL), fmt, argsc);
		wTextAppend(PROBLEMREP_T, buffer);

	}
	va_end(argsc);
	MyFree(buffer);
}

BOOL_T
ProblemSaveLayout(void)
{
	int rc = TRUE;
	if (GetLayoutChanged()) {

		rc = wNoticeWithIcon( NT_WARNING,
		                _("Trackplan has to be saved first. " \
		                  "Do you want to do so now ? "),
		                N_("Save Now..."),
		                N_("Cancel"));

		if (rc) {
			DoSave(NULL);
		} else {
			ProblemrepUpdateW(_("Operation cancelled\n"));
		}
	}
	return(rc);
}

