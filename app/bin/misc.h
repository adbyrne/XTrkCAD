/** \file misc.h
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef MISC_H
#define MISC_H

#define EXPORT

#include "acclkeys.h"

typedef void (*addButtonCallBack_t)(void*);

#include "custom.h"

#ifdef WINDOWS
#define FILE_SEP_CHAR "\\"
/* suppress warning from *.bmp about conversion of int to char */
#pragma warning( disable : 4305)
#else
#define FILE_SEP_CHAR "/"
#endif

#define COUNT(A) (sizeof(A)/sizeof(A[0]))

#define STR_SIZE		(256)
#define STR_SHORT_SIZE	(80)
#define STR_LONG_SIZE	(1024)

#define CAST_AWAY_CONST (char*)

#define TITLEMAXLEN (40)

/*
 * Globals
 */

extern long adjTimer;

typedef int SCALEINX_T;
typedef int GAUGEINX_T;
typedef int SCALEDESCINX_T;

extern int log_error;

extern long toolbarSet;
extern ANGLE_T turntableAngle;
extern long maxCouplingSpeed;
extern long hideSelectionWindow;
extern long labelWhen;
extern long labelScale;
extern long labelEnable;
extern long colorLayers;
extern long carHotbarModeInx;
extern DIST_T minLength;
extern DIST_T connectDistance;
extern ANGLE_T connectAngle;
extern long twoRailScale;
extern long mapScale;
extern char Title1[40];
extern char Title2[40];
extern long checkPtInterval;
extern long liveMap;
extern long preSelect;
extern long hideTrainsInTunnels;
extern long listLabels;
extern long layoutLabels;
extern long descriptionFontSize;
extern long units;
extern long onStartup;
extern long angleSystem;
extern SCALEINX_T curScaleInx;
extern GAUGEINX_T curGaugeInx;
extern SCALEDESCINX_T curScaleDescInx;
extern DIST_T trackGauge;
extern DIST_T curScaleRatio;
extern char * curScaleName;
extern int enumerateMaxDescLen;
extern long enableBalloonHelp;
extern long hotBarLabels;
extern long rightClickMode;
extern void * commandContext;
extern coOrd cmdMenuPos;
#define MODE_DESIGN		(0)
#define MODE_TRAIN		(1)
extern long programMode;
#define DISTFMT_DECS			0x00FF
#define DISTFMT_FMT				0x0300
#define DISTFMT_FMT_NONE		0x0000
#define DISTFMT_FMT_SHRT		0x0100
#define DISTFMT_FMT_LONG		0x0200
#define DISTFMT_FMT_MM			0x0100
#define DISTFMT_FMT_CM			0x0200
#define DISTFMT_FMT_M			0x0300
#define DISTFMT_FRACT			0x0400
#define DISTFMT_FRACT_NUM		0x0000
#define DISTFMT_FRACT_FRC		0x0400

#define UNITS_ENGLISH	(0)
#define UNITS_METRIC	(1)
#define GetDim(X) ((units==UNITS_METRIC)?(X)/2.54:(X))
#define PutDim(X) ((units==UNITS_METRIC)?(X)*2.54:(X))
#define ANGLE_POLAR		(0)
#define ANGLE_CART		(1)
#define GetAngle(X)		((angleSystem==ANGLE_POLAR)?(X):NormalizeAngle(90.0-(X)))
#define PutAngle(X)		((angleSystem==ANGLE_POLAR)?(X):NormalizeAngle(90.0-(X)))
#define LABELENABLE_TRKDESC		(1<<0)
#define LABELENABLE_LENGTHS		(1<<1)
#define LABELENABLE_ENDPT_ELEV	(1<<2)
#define LABELENABLE_TRACK_ELEV	(1<<3)
#define LABELENABLE_CARS		(1<<4)

/*
 * Command Action
 */
#define C_DOWN			wActionLDown
#define C_MOVE			wActionLDrag
#define C_UP			wActionLUp
#define C_RDOWN			wActionRDown
#define C_RMOVE			wActionRDrag
#define C_RUP			wActionRUp
#define C_TEXT			wActionText
#define C_WUP			wActionWheelUp
#define C_WDOWN			wActionWheelDown
#define C_INIT			(wActionLast+1)
#define C_START			(wActionLast+2)
#define C_REDRAW		(wActionLast+3)
#define C_CANCEL		(wActionLast+4)
#define C_OK			(wActionLast+5)
#define C_CONFIRM		(wActionLast+6)
#define C_LCLICK		(wActionLast+7)
#define C_RCLICK		(wActionLast+8)
#define C_CMDMENU		(wActionLast+9)
#define C_FINISH		(wActionLast+10)

#define C_CONTINUE		(100)
#define C_TERMINATE		(101)
#define C_INFO			(102)
#define C_ERROR			(103)

/*
 * Commands
 */
#define LEVEL0			(0)
#define LEVEL0_50		(1)
#define LEVEL1			(2)
#define LEVEL2			(3)

typedef STATUS_T (*procCommand_t) (wAction_t, coOrd);

/*
 * Windows and buttons
 */
extern wPos_t DlgSepLeft;
extern wPos_t DlgSepMid;
extern wPos_t DlgSepRight;
extern wPos_t DlgSepTop;
extern wPos_t DlgSepBottom;
extern wPos_t DlgSepNarrow;
extern wPos_t DlgSepWide;
extern wPos_t DlgSepFrmLeft;
extern wPos_t DlgSepFrmRight;
extern wPos_t DlgSepFrmTop;
extern wPos_t DlgSepFrmBottom;

extern wWin_p mainW;
extern wPos_t toolbarHeight;
extern wIndex_t changed;
extern char FAR message[STR_LONG_SIZE];
extern REGION_T curRegion;
extern long paramVersion;
extern coOrd zero;
extern wBool_t extraButtons;
extern wButton_p undoB;
extern wButton_p redoB;
extern wButton_p zoomUpB;			/** ZoomUp button on toolbar */
extern wButton_p zoomDownB;		/** ZoomDown button on toolbar */
// extern wButton_p easementB;
extern wIndex_t checkPtMark;
extern wMenu_p demoM;
extern wMenu_p popup1M, popup2M;

#define wControlBelow( B )		(wControlGetPosY((wControl_p)(B))+wControlGetHeight((wControl_p)(B)))
#define wControlBeside( B )		(wControlGetPosX((wControl_p)(B))+wControlGetWidth((wControl_p)(B)))

typedef void (*rotateDialogCallBack_t) ( void * );
extern void AddRotateMenu( wMenu_p, rotateDialogCallBack_t );
extern void StartRotateDialog( rotateDialogCallBack_t );
/*
 * Safe Memory etc
 */
void * MyMalloc( long );
void * MyRealloc( void *, long );
void MyFree( void * );
void * memdup( void *, size_t );
char * MyStrdup( const char * );
void AbortProg( char *, ... );
#define ASSERT( X ) if ( !(X) ) AbortProg( "%s: %s:%d", #X, __FILE__, __LINE__ )
char * Strcpytrimed( char *, char *, BOOL_T );
char * BuildTrimedTitle( char *, char *, char *, char *, char * );
void ErrorMessage( char *, ... );
void InfoMessage( char *, ... );
int NoticeMessage( char *, char*, char *, ... );
int NoticeMessage2( int, char *, char*, char *, ... );
void DoQuit( void );

void wShow( wWin_p );
void wHide( wWin_p );
void CloseDemoWindows( void );
void DefaultProc( wWin_p, winProcEvent, void * );
void SelectFont();

void CheckRoomSize( BOOL_T );
const char * GetBalloonHelpStr( char* );
void EnableCommands( void );
void Reset( void );
void ResetIfNotSticky( void );
wBool_t DoCurCommand( wAction_t, coOrd );
void ConfirmReset( BOOL_T );
void LayoutToolBar( void );
#define IC_STICKY		(1<<0)
#define IC_CANCEL		(1<<1)
#define IC_MENU			(1<<2)
#define IC_NORESTART	(1<<3)
#define IC_SELECTED		(1<<4)
#define IC_POPUP		(1<<5)
#define IC_LCLICK		(1<<6)
#define IC_RCLICK		(1<<7)
#define IC_CMDMENU		(1<<8)
#define IC_POPUP2		(1<<9)
#define IC_ABUT			(1<<10)
#define IC_ACCLKEY		(1<<11)
#define IC_MODETRAIN_TOO		(1<<12)
#define IC_MODETRAIN_ONLY		(1<<13)
#define IC_WANT_MOVE	(1<<14)
#define IC_PLAYBACK_PUSH		(1<<15)
wIndex_t InitCommand( wMenu_p, procCommand_t, char *, char *,  int, long, long );
void AddToolbarControl( wControl_p, long );
BOOL_T CommandEnabled( wIndex_t );
wButton_p AddToolbarButton( char*, wIcon_p, long, wButtonCallBack_p, void * context );
wIndex_t AddCommandButton( procCommand_t, char*, char*, wIcon_p, int, long, long, void* );
wIndex_t AddMenuButton( wMenu_p, procCommand_t, char*, char*, wIcon_p, int, long, long, void* );
void PlaybackButtonMouse( wIndex_t );
void ButtonGroupBegin( char *, char *, char * );
void ButtonGroupEnd( void );

void SaveState( void );

void PlaybackCommand( char *, wIndex_t );
wMenu_p MenuRegister( char * label );
void DoCommandB( void * );

extern void EnumerateTracks( void );
void InitDebug( char *, long * );

#define CHANGE_SCALE	(1<<0)
#define CHANGE_PARAMS	(1<<1)
#define CHANGE_MAIN		(1<<2)
#define CHANGE_MAP		(1<<4)
#define CHANGE_GRID		(1<<5)
#define CHANGE_UNITS	(1<<7)
#define CHANGE_TOOLBAR	(1<<8)
#define CHANGE_CMDOPT	(1<<9)
#define CHANGE_LIMITS	(1<<10)
#define CHANGE_ALL		(CHANGE_SCALE|CHANGE_PARAMS|CHANGE_MAIN|CHANGE_MAP|CHANGE_UNITS|CHANGE_TOOLBAR|CHANGE_CMDOPT)
typedef void (*changeNotificationCallBack_t)( long );
void RegisterChangeNotification( changeNotificationCallBack_t );
void DoChangeNotification( long );

#include "param.h"
#include "misc2.h"
#include "fileio.h"

/* foreign externs */
extern drawCmd_t mapD;
extern STATUS_T CmdEnumerate( wAction_t, coOrd );

wIndex_t modifyCmdInx;
wIndex_t joinCmdInx;
wIndex_t tunnelCmdInx;

/* ctodesgn.c */
void InitNewTurn( wMenu_p m );

/* cnote.c */
void ClearNote( void );

/* cruler.c */
void RulerRedraw( BOOL_T );
STATUS_T ModifyRuler( wAction_t, coOrd );

/* dialogs */
void OutputBitMap( void );

wDrawColor snapGridColor;

addButtonCallBack_t ColorInit( void );
addButtonCallBack_t PrefInit( void );
addButtonCallBack_t LayoutInit( void );
addButtonCallBack_t DisplayInit( void );
addButtonCallBack_t CmdoptInit( void );
addButtonCallBack_t OutputBitMapInit( void );
addButtonCallBack_t CustomMgrInit( void );
addButtonCallBack_t PriceListInit( void );
addButtonCallBack_t ParamFilesInit( void );

wIndex_t InitGrid( wMenu_p menu );

void SnapPos( coOrd * );
void DrawSnapGrid( drawCmd_p, coOrd, BOOL_T );
BOOL_T GridIsVisible( void );
void InitSnapGridButtons( void );
void SnapGridEnable( void );
void SnapGridShow( void );
void MapWindowShow( int state );
wMenuToggle_p snapGridEnableMI;
wMenuToggle_p snapGridShowMI;
wMenuToggle_p mapShowMI;

void ScaleLengthEnd( void );
void EnumerateList( long, FLOAT_T, char * );
void EnumerateStart(void);
void EnumerateEnd(void);

/* cnote.c */
void DoNote( void );
BOOL_T WriteMainNote( FILE * );

/* dbench.c */
long GetBenchData( long, long );
wIndex_t GetBenchListIndex( long );
long SetBenchData( char *, wDrawWidth, wDrawColor );
void DrawBench( drawCmd_p, coOrd, coOrd, wDrawColor, wDrawColor, long, long );
void BenchUpdateOrientationList( long, wList_p );
void BenchUpdateChoiceList( wIndex_t, wList_p, wList_p );
addButtonCallBack_t InitBenchDialog( void );
void BenchLoadLists( wList_p, wList_p );
void BenchGetDesc( long, char * );
void CountBench( long, DIST_T );
void TotalBench( void );
long BenchInputOption( long );
long BenchOutputOption( long );
DIST_T BenchGetWidth( long );

/* dcustmgm.c */
FILE * customMgmF;
#define CUSTMGM_DO_COPYTO		(1)
#define CUSTMGM_CAN_EDIT		(2)
#define CUSTMGM_DO_EDIT			(3)
#define CUSTMGM_CAN_DELETE		(4)
#define CUSTMGM_DO_DELETE		(5)
#define CUSTMGM_GET_TITLE		(6)

typedef int (*custMgmCallBack_p)( int, void * );
void CustMgmLoad( wIcon_p, custMgmCallBack_p, void * );
void CompoundCustMgmLoad();
void CarCustMgmLoad();
BOOL_T CompoundCustomSave(FILE*);
BOOL_T CarCustomSave(FILE*);

/* doption.c */
long GetDistanceFormat( void );

/* ctrain.c */
BOOL_T WriteCars( FILE * );
void ClearCars( void );
void CarDlgAddProto( void );
void CarDlgAddDesc( void );
void AttachTrains( void );
#endif

/* cblock.c */
void InitCmdBlock( wMenu_p menu );

/* cswitchmotor.c */
void InitCmdSwitchMotor( wMenu_p menu );
