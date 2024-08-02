/** \file param.h
 * Definitions for parameter dialog handling
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

#ifndef PARAM_H
#define PARAM_H

#include "common.h"
#include "draw.h" //- playbackAction

typedef struct turnoutInfo_t * turnoutInfo_p;

extern wWinPix_t DlgSepTop;
extern wWinPix_t DlgSepBottom;
extern wWinPix_t DlgSepLeft;
extern wWinPix_t DlgSepRight;

typedef enum {
	PD_LONG,
	PD_FLOAT,
	PD_RADIO,
	PD_TOGGLE,
	PD_STRING,
	PD_LIST,
	PD_DROPLIST,
	PD_COMBOLIST,
	PD_BUTTON,
	PD_COLORLIST,
	PD_MESSAGE,					/* static text */
	PD_DRAW,
	PD_TEXT,
	PD_MENU,
	PD_MENUITEM,
	PD_BITMAP
} parameterType;

// PD_FLOAT modifiers
#define PDO_DIM					(1L<<0)		/**< value is a dimension */
#define PDO_ANGLE				(1L<<1)		/**< value is an angle in degrees */
#define PDO_SMALLDIM			(1L<<2)
// PD_STRING modifiers
#define PDO_NOTBLANK			(1L<<3)

#define PDO_NORECORD			(1L<<6)
#define PDO_NOPSHACT			(1L<<7)
#define PDO_NOPSHUPD			(1L<<8)		/**< do not update the input variable */
#define PDO_NOUPDACT			(1L<<9)
#define PDO_NOACT		(PDO_NOPSHACT|PDO_NOUPDACT)
#define PDO_NOUPD		(PDO_NORSTUPD|PDO_NOPSHUPD)

// Override paramGroup pref group
#define PDO_NOPREF			(1L<<10)		/**< save entered value as default */
#define PDO_MISC			(1L<<11)
#define PDO_DRAW			(1L<<12)
#define PDO_FILE			(1L<<13)
//#define PDO_ENTER               	(1L<<14)
#define PDO_ENTER               	0

//#define PDO_STRINGLIMITLENGTH		(1L<<11)	/**< context has maximum length for string */
#define PDO_STRINGLIMITLENGTH		0	/**< context has maximum length for string */

// Ignore param
#define PDO_DLGIGNORE			(1L<<15)

// Layout options
#define PDO_DLGSTARTBTNS		(1L<<16)
#define PDO_DLGWIDE			(1L<<17)
#define PDO_DLGNARROW			(1L<<18)
#define PDO_DLGBOXEND			(1L<<19)	 /**< draw recessed frame around the controls */
#define PDO_DLGRESETMARGIN		(1L<<20)	 /**< position control on the left ?*/
#define PDO_DLGIGNORELABELWIDTH 	(1L<<21)
#define PDO_DLGHORZ			(1L<<22)  /**< arrange on same line as previous element */
#define PDO_DLGNEWCOLUMN		(1L<<23)
#define PDO_DLGNOLABELALIGN		(1L<<24)
#define PDO_LISTINDEX			(1L<<25)
#define PDO_DLGSETY			(1L<<26)
#define PDO_DLGIGNOREX			(1L<<27)
#define PDO_DLGUNDERCMDBUTT		(1L<<28)
#define PDO_DLGCMDBUTTON		(1L<<29)	/**< arrange button on the right with the default buttons */
#define PDO_DLGRESIZEW			(1L<<30)
#define PDO_DLGRESIZEH			(1L<<31)
#define PDO_DLGRESIZE			(PDO_DLGRESIZEW|PDO_DLGRESIZEH)




typedef struct paramGroup_t *paramGroup_p;

#define PDO_NORANGECHECK_LOW			(1<<0)
#define PDO_NORANGECHECK_HIGH	(1<<1)
typedef struct {
	long low;
	long high;
	wWinPix_t width;
	int rangechecks;
} paramIntegerRange_t;
typedef struct {
	FLOAT_T low;
	FLOAT_T high;
	wWinPix_t width;
	int rangechecks;
} paramFloatRange_t;
typedef struct {
	wWinPix_t width;
	wWinPix_t height;
	wDrawRedrawCallBack_p redraw;
	playbackProc action;
	drawCmd_p d;
} paramDrawData_t;
typedef struct {
	wIndex_t number;
	wWinPix_t width;
	int colCnt;
	wWinPix_t * colWidths;
	const char * * colTitles;
	wWinPix_t height;
} paramListData_t;
typedef struct {
	wWinPix_t width;
	wWinPix_t height;
} paramTextData_t;

typedef union {
	long l;
	FLOAT_T f;
	char * s;
	turnoutInfo_p p;
	wDrawColor dc;
} paramOldData_t;
typedef struct {
	parameterType type;
	void * valueP;
	const char * nameStr;
	long option;
	const void * winData;
	const char * winLabel;
	long winOption;
	void * context;
	unsigned int max_string;
	wControl_p control;
	paramGroup_p group;
	paramOldData_t oldD, demoD;
	wBool_t enter_pressed;
	wBool_t bInvalid;
	unsigned posColumn, posRow; 	
	wBool_t bShown;
} paramData_t, *paramData_p;


typedef void (*paramGroupProc_t) ( long, long );
#define PGACT_OK		(1)
#define PGACT_PARAM		(2)
#define PGACT_UPDATE	(3)
#define PGACT_RESTORE	(4)

#define PGO_RECORD				(1<<1)
#define PGO_NODEFAULTPROC		(1<<2)
#define PGO_PREFGROUP			(1<<8)
#define PGO_PREFMISCGROUP		(1<<8)
#define PGO_PREFDRAWGROUP		(1<<9)
#define PGO_PREFMISC			(1<<10)

typedef void (*paramLayoutProc)( paramData_t *, int, wWinPix_t, wWinPix_t *,
                                 wWinPix_t * );
typedef void (*paramActionOkProc)( void * );
typedef void (*paramActionCancelProc)( wWin_p );
typedef void (*paramChangeProc)( paramGroup_p, int, void * );

typedef struct paramGroup_t {
	char * nameStr;
	long options;
	paramData_p paramPtr;
	int paramCnt;
	paramActionOkProc okProc;
	paramActionCancelProc cancelProc;
	paramLayoutProc layoutProc;
	long winOption;
	paramChangeProc changeProc;
	long action;
	paramGroupProc_t proc;
	wWin_p win;
	wButton_p okB;
	wButton_p cancelB;
	wButton_p helpB;
	wWinPix_t origW;
	wWinPix_t origH;
	wBox_p * boxs;
} paramGroup_t;

wIndex_t ColorTabLookup( wDrawColor );

extern char * PREFSECT;
// extern char decodeErrorStr[STR_SHORT_SIZE];


#define ANGLE_POLAR		(0)
#define ANGLE_CART		(1)
extern long angleSystem;
#define PutAngle(X)		((angleSystem==ANGLE_POLAR)?(X):NormalizeAngle(90.0-(X)))

#define DISTFMT_DECS			0x00FF
#define DISTFMT_FMT			0x0300
#define DISTFMT_FMT_NONE		0x0000
#define DISTFMT_FMT_SHRT		0x0100
#define DISTFMT_FMT_LONG		0x0200
#define DISTFMT_FMT_MM			0x0100
#define DISTFMT_FMT_CM			0x0200
#define DISTFMT_FMT_M			0x0300
#define DISTFMT_FRACT			0x0400
#define DISTFMT_FRACT_NUM		0x0000
#define DISTFMT_FRACT_FRC		0x0400

FLOAT_T DecodeFloat( wEntry_p entry, BOOL_T * valid);
FLOAT_T DecodeDistance( wEntry_p entry, BOOL_T * valid);
char * FormatLong( long );
char * FormatFloat( FLOAT_T );
char * FormatDistance( FLOAT_T );
char * FormatSmallDistance( FLOAT_T );
char * FormatDistanceEx( FLOAT_T, long );


void ParamLoadControls( paramGroup_p );
void ParamLoadControl( paramGroup_p, int );
void ParamControlActive( paramGroup_p, int, BOOL_T );
void ParamLoadMessage( paramGroup_p, int, char * );
void ParamLoadData( paramGroup_p );
long ParamUpdate( paramGroup_p );
void ParamRegister( paramGroup_p );
void ParamGroupRecord( paramGroup_p );
void ParamUpdatePrefs( void );
void ParamStartRecord( FILE *recordF );
void ParamRestoreAll( void );
void ParamSaveAll( void );
void ParamSetInReadTracks(bool state);
void ParamSetInPlayback(bool state, long delay);
void ParamTurnOffDelays(bool disable);



void ParamMenuPush( void * );
void ParamHilite( wWin_p, wControl_p, BOOL_T );
wBool_t ParamCheckInputs( paramGroup_p pg, wControl_p b );

void ParamInit( void );

extern int paramLevel;
extern int paramLen;
extern unsigned long paramKey;
extern BOOL_T paramTogglePlaybackHilite;

long GetChanges(paramGroup_p pg);


#define ParamMenuPushCreate( PD, M, HS, NS, AK, FUNC ) \
		wMenuPushCreate( M, HS, NS, AK, paramMenuPush, &PD ); \
		(PD).valueP = FUNC; \
		if ( HS ) GetBalloonHelpStr(HS);

#define PD_F_ALT_CANCELLABEL	(1L<<30)		/**<use Close or Cancel for the discard button */

#define PARAMCANCEL_NEWUNDO

extern void *ParamCancel_Null;		// Cancel button not needed: map, demo
#ifdef PARAMCANCEL_NEWUNDO
extern void *ParamCancel_Undo;		// No Cancel button, use Undo to revert
#else
void ParamCancel_Undo( wWin_p );	// 
#endif
void ParamCancel_Current( wWin_p );	// Cancel leaves values in current state
void ParamCancel_Reset( wWin_p );	// As above and exits command regardless of Sticky
void ParamCancel_Restore( wWin_p );	// Cancel restores values to previous state

wWin_p ParamCreateDialog( paramGroup_p, char *, char *, paramActionOkProc,
                          paramActionCancelProc, BOOL_T, paramLayoutProc, long, paramChangeProc );
void ParamCreateControls( paramGroup_p, paramChangeProc );
void ParamLayoutDialog( paramGroup_p );

void ParamDialogOkActive( paramGroup_p, int );

void ParamResetInvalid( wWin_p win );

void ParamControlShow( paramGroup_t *, wIndex_t, wBool_t );
#endif
