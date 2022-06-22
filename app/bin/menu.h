/** \file menu.h
 * Application wide declarations and defines
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

#ifndef MENU_H
#define MENU_H

#include "common.h"

extern wMenu_p demoM;
extern wMenu_p popup1M, popup2M;
extern wButton_p undoB;
extern wButton_p redoB;
extern wButton_p zoomUpB;
extern wButton_p zoomDownB;
extern wButton_p zoomExtentsB;
extern wButton_p mapShowB;
extern wMenuToggle_p mapShowMI;
extern wMenuList_p winList_mi;
extern wMenuList_p fileList_ml;
extern wMenuToggle_p snapGridEnableMI;
extern wMenuToggle_p snapGridShowMI;

extern long stickySet;

extern wMenu_p MenuRegister( const char * label );
typedef void (*rotateDialogCallBack_t) ( void * );
typedef void (*indexDialogCallBack_t) (void * );
typedef void (*moveDialogCallBack_t) (void *);
extern void AddMoveMenu(wMenu_p m, moveDialogCallBack_t func);
extern void AddIndexMenu(wMenu_p m, indexDialogCallBack_t func);
extern void AddRotateMenu(wMenu_p m, rotateDialogCallBack_t func);
extern int MagneticSnap( int state );
extern void SelectFont(void * unused);
extern void DoSticky(void * unused);

extern void EnableMenus( void );
extern void MessageListAppend( char *, const char * );
extern const char * GetBalloonHelpStr(const char * helpKey);
extern wButton_p AddToolbarButton(const char * helpStr, wIcon_p icon, long options,
				wButtonCallBack_p action, void * context);
extern void ButtonGroupBegin(const char * menuTitle, const char * helpKey,
		const char * stickyLabel);
extern void ButtonGroupEnd(void);
extern wIndex_t AddMenuButton(wMenu_p menu, procCommand_t command,
		const char * helpKey, const char * nameStr, wIcon_p icon, int reqLevel,
		long options, long acclKey, void * context);
extern void CreateMenus(void);

#endif
