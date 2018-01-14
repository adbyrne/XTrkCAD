/** \file ctext.c
 *  Text command
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

#include "cundo.h"
#include "fileio.h"
#include "i18n.h"
#include "messages.h"
#include "param.h"
#include "track.h"
#include "wlib.h"
#include "draw.h"
#include "misc.h"

track_p NewText( wIndex_t index, coOrd p, ANGLE_T angle, char * text, CSIZE_T textSize, wDrawColor color );

void LoadFontSizeList( wList_p, long );
void UpdateFontSizeList( long *, wList_p, wIndex_t );
long GetFontSize(long);
long GetFontSizeIndex(long size);

static wMenu_p textPopupM;

/*****************************************************************************
 * TEXT COMMAND
 */

static struct {
		STATE_T state;
		CSIZE_T len;
		coOrd cursPos0, cursPos1;
		POS_T cursHeight;
		POS_T textLen;
		POS_T lastLineLen;
		POS_T lastLineOffset;
		coOrd pos;
		ANGLE_T angle;
		long size;
		wIndex_t fontSizeInx;
		char text[STR_LONG_SIZE];
        wDrawColor color;
		} Dt;

static paramData_t textPLs[] = {
#define textPD (textPLs[0])
		{ PD_DROPLIST, &Dt.fontSizeInx, "fontsize", 0, NULL, N_("Font Size"), BL_EDITABLE },
#define colorPD (textPLs[1])
        { PD_COLORLIST, &Dt.color, "color", PDO_NORECORD, NULL, N_("Color") }
        };
static paramGroup_t textPG = { "text", 0, textPLs, sizeof textPLs/sizeof textPLs[0] };

enum TEXT_POSITION
{
	POSITION_TEXT = 0,
	SHOW_TEXT
};

static void TextDlgUpdate(
		paramGroup_p pg,
		int inx,
		void * context )
{
	coOrd size, lastline;

	switch (inx) {
	case 0:
	case 1:
		if ( Dt.state == SHOW_TEXT) {
			DrawMultiString( &tempD, Dt.pos, Dt.text, NULL, (FONTSIZE_T)Dt.size, Dt.color, 0, NULL, NULL );
			DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, Dt.color );
		}
		UpdateFontSizeList( &Dt.size, (wList_p)textPLs[0].control, Dt.fontSizeInx );
		/*wWinSetBusy( mainW, TRUE );*/
		if ( Dt.state == SHOW_TEXT) {
			DrawMultiLineTextSize( &mainD, Dt.text, NULL, Dt.size, TRUE, &size, &lastline);
			Dt.textLen = size.x;
			Dt.lastLineLen = lastline.x;
			Dt.lastLineOffset = lastline.y;
		}
		DrawTextSize( &mainD, "Aquilp", NULL, Dt.size, TRUE, &size );
		Dt.cursHeight = size.y;
		/*wWinSetBusy( mainW, FALSE );*/
		if ( Dt.state == SHOW_TEXT) {
			Dt.cursPos0.x = Dt.cursPos1.x = Dt.pos.x+Dt.lastLineLen;
			Dt.cursPos1.y = Dt.pos.y+Dt.cursHeight+Dt.lastLineOffset;
			DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, Dt.color );
			DrawMultiString( &tempD, Dt.pos, Dt.text, NULL, (FONTSIZE_T)Dt.size, Dt.color, 0, NULL, NULL );
		}
        MainRedraw();
        MapRedraw();
        break;
	}
}


static STATUS_T CmdText( wAction_t action, coOrd pos )
{
	track_p t;
	unsigned char c;
	wControl_p controls[3];
	char * labels[2];
	coOrd size, lastline;

	switch (action & 0xFF) {
	case C_START:
        Dt.state = POSITION_TEXT;
		Dt.cursPos0 = Dt.cursPos1 = zero;
		Dt.len = 0;
		Dt.textLen = 0;
		Dt.text[0] = '\0';
		Dt.lastLineLen = 0;
		Dt.lastLineOffset = 0;

		if (textPD.control == NULL)
		{
			ParamCreateControls(&textPG, TextDlgUpdate);
			LoadFontSizeList((wList_p)textPD.control, Dt.size);
			ParamRegister(&textPG);
			Dt.size = GetFontSize(Dt.fontSizeInx);
		}
		Dt.size = (long)wSelectedFontSize();
		Dt.fontSizeInx = GetFontSizeIndex(Dt.size);
		ParamLoadControls(&textPG);
		ParamGroupRecord( &textPG );

		if (!inPlayback)
			wWinSetBusy(mainW, TRUE);
		DrawTextSize(&mainD, "Aquilp", NULL, Dt.size, TRUE, &size);
		Dt.cursHeight = size.y;
		if (!inPlayback)
			wWinSetBusy(mainW, FALSE);

		controls[0] = textPD.control;
		controls[1] = colorPD.control;
        controls[2] = 0;
		labels[0] = N_("Font Size");
        labels[1] = N_("Color");
		InfoSubstituteControls( controls, labels );
		return C_CONTINUE;
		break;
	case C_DOWN:
		if (Dt.state != 0) {
		}
		Dt.pos = pos;
		Dt.cursPos0.y = Dt.cursPos1.y = pos.y + Dt.lastLineOffset;
		Dt.cursPos0.x = Dt.cursPos1.x = pos.x + Dt.lastLineLen;
		DrawTextSize(&mainD, "Aquilp", NULL, Dt.size, TRUE, &size);  //In case fontsize change
		Dt.cursHeight = size.y;
		Dt.cursPos1.y += Dt.cursHeight;
		DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, Dt.color );
		DrawMultiString(&tempD, Dt.pos, Dt.text, NULL, (FONTSIZE_T)Dt.size, Dt.color, 0.0, NULL, NULL );
        Dt.state = SHOW_TEXT;
        MainRedraw();
        MapRedraw();
		return C_CONTINUE;
	case C_MOVE:
		Dt.pos = pos;
		Dt.cursPos0.y = Dt.cursPos1.y = pos.y + Dt.lastLineOffset;
		Dt.cursPos0.x = Dt.cursPos1.x = pos.x + Dt.lastLineLen;
		Dt.cursPos1.y += Dt.cursHeight;
		DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, wDrawColorBlack );
		DrawMultiString(&tempD, Dt.pos, Dt.text, NULL, (FONTSIZE_T)Dt.size, Dt.color, 0.0, NULL, NULL );
        MainRedraw();
        MapRedraw();
        return C_CONTINUE;
	case C_UP:
		return C_CONTINUE;
	case C_TEXT:
		if (Dt.state == POSITION_TEXT) {
			NoticeMessage( MSG_SEL_POS_FIRST, _("Ok"), NULL );
			return C_CONTINUE;
		}
		DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, Dt.color );
		DrawMultiString(&tempD, Dt.pos, Dt.text, NULL, (FONTSIZE_T)Dt.size, Dt.color, 0.0, NULL, NULL );
		c = (unsigned char)(action >> 8);
		switch (c) {
		case '\b':
		case 0xFF:
			if (Dt.len > 0) {
				Dt.len--;
				Dt.text[Dt.len] = '\000';
			} else {
				wBeep();
			}
			break;
		case '\n':    // Line Feed
			if (Dt.len < sizeof Dt.text - 1 ) {
				Dt.text[Dt.len++] = (char)c;
				Dt.text[Dt.len] = '\000';
			}
			break;
		case '\015':
			UndoStart( _("Create Text"), "newText - CR" );
			t = NewText( 0, Dt.pos, Dt.angle, Dt.text, (CSIZE_T)Dt.size, Dt.color );
			UndoEnd();
			DrawNewTrack(t);
			Dt.state = POSITION_TEXT;
			InfoSubstituteControls( NULL, NULL );
			return C_TERMINATE;
		default:
			if (Dt.len < sizeof Dt.text - 1 ) {
				Dt.text[Dt.len++] = (char)c;
				Dt.text[Dt.len] = '\000';
			}
		}
        DrawMultiLineTextSize( &mainD, Dt.text, NULL, Dt.size, TRUE, &size, &lastline);
		Dt.textLen = size.x;
		Dt.lastLineLen = lastline.x;
		Dt.lastLineOffset = lastline.y;
		Dt.cursPos0.x = Dt.cursPos1.x = Dt.pos.x + Dt.lastLineLen;
		Dt.cursPos0.y = Dt.cursPos1.y = Dt.pos.y + Dt.lastLineOffset;
		DrawTextSize(&mainD, "Aquilp", NULL, Dt.size, TRUE, &size);  //In case fontsize change
		Dt.cursHeight = size.y;
		Dt.cursPos1.y +=Dt.cursHeight;
		DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, Dt.color );
		DrawMultiString(&tempD, Dt.pos, Dt.text, NULL, (FONTSIZE_T)Dt.size, Dt.color, 0.0, NULL, NULL );
		return C_CONTINUE;
	case C_REDRAW:
		if (Dt.state == 1) {
			DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, Dt.color );
			DrawMultiString(&tempD, Dt.pos, Dt.text, NULL, (FONTSIZE_T)Dt.size, Dt.color, 0.0, NULL, NULL );
		}
		return C_CONTINUE;
	case C_CANCEL:
		if (Dt.state != POSITION_TEXT) {
			Dt.state = POSITION_TEXT;
		}
		InfoSubstituteControls( NULL, NULL );
		MainRedraw();
		MapRedraw();
		return C_TERMINATE;
	case C_OK:
		if (Dt.state != POSITION_TEXT) {
			DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, Dt.color );
			Dt.state = POSITION_TEXT;
			if (Dt.len) {
				UndoStart( _("Create Text"), "newText - OK" );
				t = NewText( 0, Dt.pos, Dt.angle, Dt.text, (CSIZE_T)Dt.size, Dt.color );
				UndoEnd();
				DrawNewTrack(t);
			}
		}
		InfoSubstituteControls( NULL, NULL );
        MainRedraw();
        MapRedraw();
		return C_TERMINATE;

	case C_FINISH:
		if (Dt.state != POSITION_TEXT && Dt.len > 0)
			 CmdText( C_OK, pos );
		else
			CmdText( C_CANCEL, pos );
		return C_TERMINATE;

	case C_CMDMENU:
		wMenuPopupShow( textPopupM );
		return C_CONTINUE;
	}
	return C_CONTINUE;
}


#include "bitmaps/text.xpm"

void InitCmdText( wMenu_p menu )
{
	AddMenuButton( menu, CmdText, "cmdText", _("Text"), wIconCreatePixMap(text_xpm), LEVEL0_50, IC_STICKY|IC_CMDMENU|IC_POPUP2, ACCL_TEXT, NULL );
	textPopupM = MenuRegister( "Text Font" );
	wMenuPushCreate( textPopupM, "", _("Fonts..."), 0, (wMenuCallBack_p)SelectFont, NULL );
	Dt.size = (CSIZE_T)wSelectedFontSize();
    Dt.color = wDrawColorBlack;
}

void InitTrkText( void )
{
}
