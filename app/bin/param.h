/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/param.h,v 1.6 2009-09-21 18:24:33 m_fischer Exp $
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

#ifndef PARAM_H
#define PARAM_H

typedef struct turnoutInfo_t * turnoutInfo_p;

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

#define PDO_DIM					(1L<<0)
#define PDO_ANGLE				(1L<<1)
#define PDO_NORECORD			(1L<<2)
#define PDO_NOPSHACT			(1L<<3)
#define PDO_NOPSHUPD			(1L<<4)
#define PDO_NOPREF				(1L<<5)
#define PDO_NOUPDACT			(1L<<6)
#define PDO_MISC				(1L<<7)
#define PDO_DRAW				(1L<<8)
#define PDO_FILE				(1L<<9)

#define PDO_SMALLDIM			(1L<<12)

#define PDO_DLGSTARTBTNS		(1L<<13)
#define PDO_DLGWIDE				(1L<<14)
#define PDO_DLGNARROW			(1L<<15)
#define PDO_DLGBOXEND			(1L<<16)	 /**< draw recessed frame around the controls */
#define PDO_DLGRESETMARGIN		(1L<<17)	 /**< position control on the left ?*/
#define PDO_DLGIGNORELABELWIDTH (1L<<18)
#define PDO_DLGHORZ				(1L<<20)  /**< arrange on same line as previous element */
#define PDO_DLGNEWCOLUMN		(1L<<21)
#define PDO_DLGNOLABELALIGN		(1L<<22)
#define PDO_LISTINDEX			(1L<<23)
#define PDO_DLGSETY				(1L<<24)
#define PDO_DLGIGNOREX			(1L<<25)
#define PDO_DLGUNDERCMDBUTT		(1L<<26)
#define PDO_DLGCMDBUTTON		(1L<<27)	/**< arrange button on the right with the default buttons */
#define PDO_DLGIGNORE			(1L<<28)

#define PDO_DLGRESIZEW			(1L<<29)
#define PDO_DLGRESIZEH			(1L<<30)
#define PDO_DLGRESIZE			(PDO_DLGRESIZEW|PDO_DLGRESIZEH)

#define PDO_NOACT		(PDO_NOPSHACT|PDO_NOUPDACT)
#define PDO_NOUPD		(PDO_NORSTUPD|PDO_NOPSHUPD|PDO_NOUPDUPD)

typedef struct paramGroup_t *paramGroup_p;

#define PDO_NORANGECHECK_LOW			(1<<0)
#define PDO_NORANGECHECK_HIGH	(1<<1)
typedef struct {
		long low;
		long high;
		wPos_t width;
		int rangechecks;
		} paramIntegerRange_t;
typedef struct {
		FLOAT_T low;
		FLOAT_T high;
		wPos_t width;
		int rangechecks;
		} paramFloatRange_t;
typedef struct {
		wPos_t width;
		wPos_t height;
		wDrawRedrawCallBack_p redraw;
		playbackProc action;
		drawCmd_p d;
		} paramDrawData_t;
typedef struct {
		wIndex_t number;
		wPos_t width;
		int colCnt;
		wPos_t * colWidths;
		const char * * colTitles;
		wPos_t height;
		} paramListData_t;
typedef struct {
		wPos_t width;
		wPos_t height; 
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
		char * nameStr;
		long option;
		void * winData;
		char * winLabel;
		long winOption;
		void * context;
		wControl_p control;
		paramGroup_p group;
		paramOldData_t oldD, demoD;
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

typedef void (*paramLayoutProc)( paramData_t *, int, wPos_t, wPos_t *, wPos_t * );
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
		wPos_t origW;
		wPos_t origH;
		wBox_p * boxs;
		} paramGroup_t;

wIndex_t ColorTabLookup( wDrawColor );

extern char * PREFSECT;
// extern char decodeErrorStr[STR_SHORT_SIZE];
FLOAT_T DecodeFloat( wString_p, BOOL_T * );
FLOAT_T DecodeDistance( wString_p, BOOL_T * );
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
void ParamStartRecord( void );
void ParamRestoreAll( void );
void ParamSaveAll( void );

void ParamMenuPush( void * );
int paramHiliteFast;
void ParamHilite( wWin_p, wControl_p, BOOL_T );

void ParamInit( void );

extern int paramLevel;
extern int paramLen;
extern unsigned long paramKey;
extern BOOL_T paramTogglePlaybackHilite;

#define ParamMenuPushCreate( PD, M, HS, NS, AK, FUNC ) \
		wMenuPushCreate( M, HS, NS, AK, paramMenuPush, &PD ); \
		(PD).valueP = FUNC; \
		if ( HS ) GetBalloonHelpStr(HS);

#define PD_F_ALT_CANCELLABEL	(1L<<30)
wWin_p ParamCreateDialog( paramGroup_p, char *, char *, paramActionOkProc, paramActionCancelProc, BOOL_T, paramLayoutProc, long, paramChangeProc );
void ParamCreateControls( paramGroup_p, paramChangeProc );
void ParamLayoutDialog( paramGroup_p );

void ParamDialogOkActive( paramGroup_p, int );

#define ParamControlShow( PG, INX, SHOW ) \
		wControlShow( ((PG)->paramPtr)[INX].control, SHOW )
#endif
