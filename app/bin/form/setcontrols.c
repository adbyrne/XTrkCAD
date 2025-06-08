/**
 * \file   setcontrols.c
 * \brief
 *
 * \author Martin Fischer
 */

/*  XTrackCad - Model Railroad CAD
 *  Copyright (C) 2005, 2025 Dave Bullis
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

#include <form.h>
#include "formprivate.h"


EXPORT void FormControlActive(
        paramGroup_p pg,
        int inx,
        BOOL_T active)
{
	paramData_p p = &pg->paramPtr[inx];
	if (p->control) {
		wControlActive(p->control, active);
	}
}



EXPORT void FormMenuPush(void* dp)
{
	paramData_p p = (paramData_p)dp;
	const char* groupNameStr = p->group ? p->group->nameStr : "misc";
	if ((p->option & PDO_NORECORD) == 0 && groupNameStr && p->nameStr) {
		FormMacroRecord("PARAMETER %s %s\n", groupNameStr, p->nameStr);
	}
	if ((p->option & PDO_NOPSHACT) == 0 && p->valueP) {
		((wMenuCallBack_p)(p->valueP))(p->context);
	}
}
