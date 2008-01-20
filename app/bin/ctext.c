/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/ctext.c,v 1.3 2008-01-20 23:29:15 mni77 Exp $
 *
 * TEXT
 *
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

#include "track.h"
#include "i18n.h"


track_p NewText( wIndex_t index, coOrd p, ANGLE_T angle, char * text, CSIZE_T textSize );

void LoadFontSizeList( wList_p, long );
void UpdateFontSizeList( long *, wList_p, wIndex_t );

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
		coOrd pos;
		ANGLE_T angle;
		long size;
		wIndex_t fontSizeInx;
		char text[STR_SIZE];
		} Dt;

static paramData_t textPLs[] = {
#define textPD (textPLs[0])
		{ PD_DROPLIST, &Dt.fontSizeInx, "fontsize", 0, NULL, N_("Font Size"), BL_EDITABLE } };
static paramGroup_t textPG = { "text", 0, textPLs, sizeof textPLs/sizeof textPLs[0] };


static void TextDlgUpdate(
		paramGroup_p pg,
		int inx,
		void * context )
{
	coOrd size;

	switch (inx) {
	case 0:
		if ( Dt.state == 1 ) {
			DrawString( &tempD, Dt.pos, 0.0, Dt.text, NULL, (FONTSIZE_T)Dt.size, wDrawColorBlack );
			DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, wDrawColorBlack );
		}
		UpdateFontSizeList( &Dt.size, (wList_p)textPLs[0].control, Dt.fontSizeInx );
		/*wWinSetBusy( mainW, TRUE );*/
		if ( Dt.state == 1 ) {
			DrawTextSize( &mainD, Dt.text, NULL, Dt.size, TRUE, &size );
			Dt.textLen = size.x;
		}
		DrawTextSize( &mainD, "X", NULL, Dt.size, TRUE, &size );
		Dt.cursHeight = size.y;
		/*wWinSetBusy( mainW, FALSE );*/
		if ( Dt.state == 1 ) {
			Dt.cursPos0.x = Dt.cursPos1.x = Dt.pos.x+Dt.textLen;
			Dt.cursPos1.y = Dt.pos.y+Dt.cursHeight;
			DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, wDrawColorBlack );
			DrawString( &tempD, Dt.pos, 0.0, Dt.text, NULL, (FONTSIZE_T)Dt.size, wDrawColorBlack );
		}
		break;
	}
}


static STATUS_T CmdText( wAction_t action, coOrd pos )
{
	track_p t;
	unsigned char c;
	wControl_p controls[2];
	char * labels[1];
	coOrd size;

	switch (action & 0xFF) {
	case C_START:
		Dt.state = 0;
		Dt.cursPos0 = Dt.cursPos1 = zero;
		Dt.len = 0;
		Dt.textLen = 0;
		Dt.text[0] = '\0';
		if ( !inPlayback )
			wWinSetBusy( mainW, TRUE );
		DrawTextSize( &mainD, "X", NULL, Dt.size, TRUE, &size );
		Dt.cursHeight = size.y;
		if ( !inPlayback )
			wWinSetBusy( mainW, FALSE );
		if ( textPD.control==NULL ) {
			ParamCreateControls( &textPG, TextDlgUpdate );
		}
		LoadFontSizeList( (wList_p)textPD.control, Dt.size );
		ParamGroupRecord( &textPG );
		controls[0] = textPD.control;
		controls[1] = NULL;
		labels[0] = N_("Font Size");
		InfoSubstituteControls( controls, labels );
		return C_CONTINUE;
		break;
	case C_DOWN:
		if (Dt.state != 0) {
			DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, wDrawColorBlack );
			DrawString( &tempD, Dt.pos, 0.0, Dt.text, NULL, (FONTSIZE_T)Dt.size, wDrawColorBlack );
		}
		Dt.pos = pos;
		Dt.cursPos0.y = Dt.cursPos1.y = pos.y;
		Dt.cursPos0.x = Dt.cursPos1.x = pos.x + Dt.textLen;
		Dt.cursPos1.y += Dt.cursHeight;
		DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, wDrawColorBlack );
		DrawString( &tempD, Dt.pos, 0.0, Dt.text, NULL, (FONTSIZE_T)Dt.size, wDrawColorBlack );
		Dt.state = 1;
		return C_CONTINUE;
	case C_MOVE:
		DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, wDrawColorBlack );
		DrawString( &tempD, Dt.pos, 0.0, Dt.text, NULL, (FONTSIZE_T)Dt.size, wDrawColorBlack );
		Dt.pos = pos;
		Dt.cursPos0.y = Dt.cursPos1.y = pos.y;
		Dt.cursPos0.x = Dt.cursPos1.x = pos.x + Dt.textLen;
		Dt.cursPos1.y += Dt.cursHeight;
		DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, wDrawColorBlack );
		DrawString( &tempD, Dt.pos, 0.0, Dt.text, NULL, (FONTSIZE_T)Dt.size, wDrawColorBlack );
		return C_CONTINUE;
	case C_UP:
		return C_CONTINUE;
	case C_TEXT:
		if (Dt.state == 0) {
			NoticeMessage( MSG_SEL_POS_FIRST, _("Ok"), NULL );
			return C_CONTINUE;
		}
		DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, wDrawColorBlack );
		DrawString( &tempD, Dt.pos, 0.0, Dt.text, NULL, (FONTSIZE_T)Dt.size, wDrawColorBlack );
		c = (unsigned char)(action >> 8);
/*lprintf("C=%x\n", c);*/
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
		case '\015':
			UndoStart( _("Create Text"), "newText - CR" );
			t = NewText( 0, Dt.pos, Dt.angle, Dt.text, (CSIZE_T)Dt.size );
			UndoEnd();
			DrawNewTrack(t); 
			Dt.state = 0;
			InfoSubstituteControls( NULL, NULL );
			return C_TERMINATE;
		default:
			if (Dt.len < sizeof Dt.text - 1 ) {
				Dt.text[Dt.len++] = (char)c;
				Dt.text[Dt.len] = '\000';
			}
		}
		DrawTextSize( &mainD, Dt.text, NULL, Dt.size, TRUE, &size );
		Dt.textLen = size.x;
		Dt.cursPos0.x = Dt.cursPos1.x = Dt.pos.x + Dt.textLen;
		DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, wDrawColorBlack );
		DrawString( &tempD, Dt.pos, 0.0, Dt.text, NULL, (FONTSIZE_T)Dt.size, wDrawColorBlack );
		return C_CONTINUE;
	case C_REDRAW:
		if (Dt.state == 1) {
			DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, wDrawColorBlack );
			DrawString( &tempD, Dt.pos, 0.0, Dt.text, NULL, (FONTSIZE_T)Dt.size, wDrawColorBlack );
		}
		return C_CONTINUE;
	case C_CANCEL:
		if (Dt.state != 0) {
			DrawString( &tempD, Dt.pos, 0.0, Dt.text, NULL, (FONTSIZE_T)Dt.size, wDrawColorBlack );
			DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, wDrawColorBlack );
			Dt.state = 0;
		}
		InfoSubstituteControls( NULL, NULL );
		return C_TERMINATE;
	case C_OK:
		if (Dt.state != 0) {
			DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, wDrawColorBlack );
			Dt.state = 0;
			if (Dt.len) {
				UndoStart( _("Create Text"), "newText - OK" );
				t = NewText( 0, Dt.pos, Dt.angle, Dt.text, (CSIZE_T)Dt.size );
				UndoEnd();
				DrawNewTrack(t);
			}
		}
		InfoSubstituteControls( NULL, NULL );
		return C_TERMINATE;

	case C_FINISH:
		if (Dt.state != 0 && Dt.len > 0)
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


#include "text.xpm"

void InitCmdText( wMenu_p menu )
{
	AddMenuButton( menu, CmdText, "cmdText", _("Text"), wIconCreatePixMap(text_xpm), LEVEL0_50, IC_STICKY|IC_CMDMENU|IC_POPUP2, ACCL_TEXT, NULL );
	textPopupM = MenuRegister( "Text Font" );
	wMenuPushCreate( textPopupM, "", _("Fonts..."), 0, (wMenuCallBack_p)SelectFont, NULL );
	Dt.size = (CSIZE_T)wSelectedFontSize();
	ParamRegister( &textPG );
}

void InitTrkText( void )
{
}
