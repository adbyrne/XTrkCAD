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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "cundo.h"
#include "fileio.h"
#include "icons.h"
#include "form.h"
#include "track.h"
#include "draw.h"
#include "misc.h"
#include "common-ui.h"

static int log_ctext = -1;

static wMenu_p textPopupM;

/*****************************************************************************
 * TEXT COMMAND
 */

static struct {
	STATE_T state;
	CSIZE_T len;
	coOrd cursPos0, cursPos1;
	POS_T cursHeight;
	POS_T lastLineLen;
	POS_T lastLineOffset;
	coOrd pos;
	long size;
	long fontSizeInx;
	char text[STR_LONG_SIZE];
	wDrawColor color;
	long boxed;
	long filled;
	wDrawColor bg_color;
} Dt;

static char * boxLabels[] = { "", NULL };
static paramData_t textPLs[] = {
#define textPD (textPLs[0])
	{ PD_COMBOLIST, &Dt.fontSizeInx, "fontsize", 0, NULL, NULL, BL_EDITABLE },
#define colorPD (textPLs[1])
	{ PD_COLORLIST, &Dt.color, "color", PDO_NORECORD, NULL, NULL },
#define boxPD (textPLs[2])
	{ PD_TOGGLE, &Dt.boxed, "boxed", 0, boxLabels, N_("Boxed"), 0 },
#define fillPD (textPLs[3])
	{ PD_TOGGLE, &Dt.filled, "filled", 0, boxLabels, N_("Filled") },
#define backPD (textPLs[4])
	{ PD_COLORLIST, &Dt.bg_color, "bg_color", PDO_NORECORD, NULL, N_("Bg Color") }
};
static paramGroup_t textPG = { "text", 0, textPLs, COUNT( textPLs ) };

enum TEXT_POSITION {
	POSITION_TEXT = 0,
	SHOW_TEXT
};

#define TEXT_BUFFER_SIZE STR_LONG_SIZE
#define TEXT_SAFETY_MARGIN 2

#define BACKSPACE_KEY 0xFF
#define DELETE_CHAR '\b'
#define NEWLINE_CHAR '\n'
#define RETURN_CHAR '\015'
#define DEFAULT_TEXT_ANGLE 0.0
#define FONT_SAMPLE_TEXT "Aquilp"

static STATUS_T CmdText( wAction_t action, coOrd pos )
{
	track_p t;
	unsigned char c;
	coOrd size, lastline;
	POS_T descent, ascent;

	switch (action & 0xFF) {
	case C_START:
		Dt.state = POSITION_TEXT;
		Dt.cursPos0 = Dt.cursPos1 = zero;
		Dt.len = 0;
		Dt.text[0] = '\0';
		Dt.lastLineLen = 0;
		Dt.lastLineOffset = 0;

		if (textPD.control == NULL) {
			FormRegister(&textPG);
			FormCreateControls(&textPG);
			LoadFontSizeList(textPD.control, Dt.size);

			Dt.size = GetFontSize((long int)Dt.fontSizeInx);
		}
		Dt.size = (long)wSelectedFontSize();
		Dt.fontSizeInx = GetFontSizeIndex(Dt.size);
		FormLoadControls(&textPG);
		FormGroupRecord( &textPG );

		DrawTextSize(&mainD, FONT_SAMPLE_TEXT, NULL, Dt.size, TRUE, &size);
		Dt.cursHeight = size.y;
		InfoSetControls(mainW, textPG.nameStr);
		return C_CONTINUE;
	case C_DOWN:
		Dt.size = GetFontSize((long int)Dt.fontSizeInx);
		Dt.pos = pos;
		Dt.cursPos0.y = Dt.cursPos1.y = pos.y + Dt.lastLineOffset;
		Dt.cursPos0.x = Dt.cursPos1.x = pos.x + Dt.lastLineLen;

		DrawTextSize(&mainD, FONT_SAMPLE_TEXT, NULL, Dt.size, TRUE,
		             &size);  //In case fontsize change
		Dt.cursHeight = size.y;
		Dt.cursPos1.y += Dt.cursHeight;
		Dt.state = SHOW_TEXT;
		return C_CONTINUE;
	case C_MOVE:
		Dt.pos = pos;
		Dt.cursPos0.y = Dt.cursPos1.y = pos.y + Dt.lastLineOffset;
		Dt.cursPos0.x = Dt.cursPos1.x = pos.x + Dt.lastLineLen;
		Dt.cursPos1.y += Dt.cursHeight;
		return C_CONTINUE;
	case C_UP:
		return C_CONTINUE;
	case C_TEXT:
		if (Dt.state == POSITION_TEXT) {
			NoticeMessage( MSG_SEL_POS_FIRST, _("Ok"), NULL );
			return C_CONTINUE;
		}

		c = (unsigned char)(action >> 8);
		switch (c) {
		case DELETE_CHAR:
		case BACKSPACE_KEY:
			if (Dt.len > 0) {
				Dt.len--;
				Dt.text[Dt.len] = '\0';
			} else {
				wBeep();
			}
			break;
		case NEWLINE_CHAR:    // Line Feed
			if (Dt.len < TEXT_BUFFER_SIZE - TEXT_SAFETY_MARGIN) {
				Dt.text[Dt.len++] = (char)c;
				Dt.text[Dt.len] = '\0';
			} else {
				InfoMessage(_("Text too long - cannot add newline"));
				wBeep();
			}
			break;
		case RETURN_CHAR:
			UndoStart( _("Create Text"), "newText - CR" );
			t = NewText( 0, Dt.pos, DEFAULT_TEXT_ANGLE, Dt.text, (CSIZE_T)Dt.size, Dt.color,
			             Dt.boxed, Dt.filled, Dt.bg_color );
			if(t!= NULL) {
				DrawNewTrack(t);
			} else {
				InfoMessage(_("Failed to create text - please try again"));
			}
			UndoEnd();
			Dt.state = POSITION_TEXT;
			InfoDefaultControls();
			return C_TERMINATE;
		default:
			if (Dt.len < TEXT_BUFFER_SIZE - TEXT_SAFETY_MARGIN ) {
				Dt.text[Dt.len++] = (char)c;
				Dt.text[Dt.len] = '\0';
			} else {
				InfoMessage("Maximum length for text reached. - ignored");
				wBeep();
			}
		}
		DrawMultiLineTextSize( &mainD, Dt.text, NULL, Dt.size, TRUE, &size, &lastline);
		Dt.lastLineLen = lastline.x;
		Dt.lastLineOffset = lastline.y;
		Dt.cursPos0.x = Dt.cursPos1.x = Dt.pos.x + Dt.lastLineLen;
		Dt.cursPos0.y = Dt.cursPos1.y = Dt.pos.y + Dt.lastLineOffset;

		DrawTextSize2(&mainD, FONT_SAMPLE_TEXT, NULL, Dt.size, TRUE, &size, &descent,
		              &ascent);  //In case fontsize change
		Dt.cursHeight = size.y;
		Dt.cursPos0.y -= descent;
		Dt.cursPos1.y += Dt.cursHeight;
		return C_CONTINUE;
	case C_REDRAW:
		DrawLine( &tempD, Dt.cursPos0, Dt.cursPos1, 0, Dt.color );
		DrawMultiString(&tempD, Dt.pos, Dt.text, NULL, (FONTSIZE_T)Dt.size, Dt.color,
		                Dt.boxed, Dt.filled, Dt.bg_color, DEFAULT_TEXT_ANGLE, NULL, NULL );
		return C_CONTINUE;
	case C_CANCEL:
		Dt.state = POSITION_TEXT;
		InfoDefaultControls();
		return C_TERMINATE;
	case C_OK:
		if (Dt.state != POSITION_TEXT) {
			Dt.state = POSITION_TEXT;
			if (Dt.len) {
				UndoStart( _("Create Text"), "newText - OK" );
				t = NewText( 0, Dt.pos, DEFAULT_TEXT_ANGLE, Dt.text, (CSIZE_T)Dt.size, Dt.color,
				             Dt.boxed, Dt.filled, Dt.bg_color );
				if(t!=NULL) {
					DrawNewTrack(t);
				}
				UndoEnd();
			}
		}
		InfoDefaultControls();
		return C_TERMINATE;

	case C_FINISH:
		if (Dt.state != POSITION_TEXT && Dt.len > 0) {
			CmdText( C_OK, pos );
		} else {
			CmdText( C_CANCEL, pos );
		}
		return C_TERMINATE;

	case C_CMDMENU:
		menuPos = pos;
		wMenuPopupShow( textPopupM );
		return C_CONTINUE;
	default:
		if ( log_ctext < 0 ) { log_ctext = LogFindIndex( "ctext" ); }
		LOG( log_ctext, 1, ( "unexpected action & 0xFF %d in CmdText\n",
		                     action & 0xFF ) )
		break;
	}
	return C_CONTINUE;
}

void InitCmdText( wMenu_p menu )
{
	AddMenuButton( menu, CmdText, "cmdText", _("Text"),
	               CreateToolbarIconFromResource("text.png"), LEVEL0_50,
	               IC_STICKY|IC_CMDMENU|IC_POPUP2, ACCL_TEXT, NULL );
	textPopupM = MenuRegister( "Text Font" );
	wMenuPushCreate( textPopupM, "", _("Fonts..."), 0, SelectFont, NULL );
	Dt.size = (CSIZE_T)wSelectedFontSize();
	Dt.color = wDrawColorBlack;
}

void InitTrkText( void )
{
}
