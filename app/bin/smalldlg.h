/** \file smalldlg.h
 * Definitions and declarations for the small dialog box functions. 
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef SMALLDLG_H
#define SMALLDLG_H

#include "wlib.h"

#define SHOWTIP_NEXTTIP (0L)
#define SHOWTIP_PREVTIP (1L)
#define SHOWTIP_FORCESHOW (2L)

extern wWin_p aboutW;

void InitSmallDlg( void );
void ShowTip( long flags );
void CreateAboutW( void *ptr );

#endif
