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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef MISC_H
#define MISC_H

#include "common.h"


/*
 * Globals
 */

extern int iconSize;
extern wWinPix_t displayWidth;
extern wWinPix_t displayHeight;
extern wControl_p mainW;
extern char message[STR_HUGE_SIZE];
extern long paramVersion;
extern coOrd zero;
extern wBool_t extraButtons;
extern long onStartup;
#define UNITS_ENGLISH	(0)
#define UNITS_METRIC	(1)
extern long units;
extern long labelScale;
#define LABELENABLE_TRKDESC	(1<<0)
#define LABELENABLE_LENGTHS	(1<<1)
#define LABELENABLE_ENDPT_ELEV	(1<<2)
#define LABELENABLE_TRACK_ELEV	(1<<3)
#define LABELENABLE_CARS	(1<<4)
extern long labelEnable;
extern long labelWhen;

extern long dontHideCursor;	// not used
#ifdef HIDESELECTIONWINDOW
extern long hideSelectionWindow;	// not used
#endif

#define GetDim(X) ((units==UNITS_METRIC)?(X)/2.54:(X))
#define PutDim(X) ((units==UNITS_METRIC)?(X)*2.54:(X))

#define wControlBelow( B )		(wControlGetPosY((wControl_p)(B))+wControlGetHeight((wControl_p)(B)))
#define wControlBeside( B )		(wControlGetPosX((wControl_p)(B))+wControlGetWidth((wControl_p)(B)))
#include "mymalloc.h"

extern char * ConvertFromEscapedText(const char * text);
extern char * ConvertToEscapedText(const char * text);

extern const char * AbortMessage( const char *, ... );
extern void AbortProg( const char *, const char *, int, const char * );
#ifdef LOG_CHECK_COVERAGE
#define CHECK( X ) lprintf( "CHECK %s:%i\n", __FILE__, __LINE__ ); if ( !(X) ) AbortProg( #X, __FILE__, __LINE__, NULL )
#define CHECKMSG( X, MSG ) lprintf( "CHECK %s:%i\n", __FILE__, __LINE__ ); if ( !(X) ) AbortProg( #X, __FILE__, __LINE__, AbortMessage MSG )
#else
#define CHECK( X ) if ( !(X) ) AbortProg( #X, __FILE__, __LINE__, NULL )
#define CHECKMSG( X, MSG ) if ( !(X) ) AbortProg( #X, __FILE__, __LINE__, AbortMessage MSG )
#endif

extern char * Strcpytrimed( char *, const char *, BOOL_T );
extern wBool_t CheckHelpTopicExists(const char * topic);

extern void InfoMessage( const char *, ... );
extern void ErrorMessage( const char *, ... );
extern int NoticeMessage( const char *, const char*, const char *, ... );
extern int NoticeMessage2( int, const char *, const char*, const char *, ... );

extern bool Confirm( char *, doSaveCallBack_p );
extern void DoQuit( void * unused );
extern void DoClear( void * unused );
extern void MapWindowToggleShow( void * unused );
extern void MapWindowShow( int state );
extern void DoShowWindow(int index, const char * name, void * data);

extern void wShow( wWin_p );
extern void wHide( wWin_p );
extern void CloseDemoWindows( void );
extern void DefaultProc( wWin_p, winProcEvent, void * );
typedef void (*changeNotificationCallBack_t)( long );
#define CHANGE_SCALE	(1<<0)
#define CHANGE_PARAMS	(1<<1)
#define CHANGE_MAIN		(1<<2)
#define CHANGE_MAP		(1<<4)
#define CHANGE_GRID		(1<<5)
#define CHANGE_BACKGROUND (1<<6)
#define CHANGE_UNITS	(1<<7)
#define CHANGE_TOOLBAR	(1<<8)
#define CHANGE_CMDOPT	(1<<9)
#define CHANGE_LIMITS	(1<<10)
#define CHANGE_ICONSIZE	(1<<11)
#define CHANGE_LAYER    (1<<3)
#define CHANGE_ALL		(CHANGE_SCALE|CHANGE_PARAMS|CHANGE_MAIN|CHANGE_LAYER|CHANGE_MAP|CHANGE_UNITS|CHANGE_TOOLBAR|CHANGE_CMDOPT|CHANGE_BACKGROUND)
extern void RegisterChangeNotification( changeNotificationCallBack_t );
extern void DoChangeNotification( long );


/* foreign externs */

/* Initializers */
addButtonCallBack_t ColorInit( void );
addButtonCallBack_t SettingsInit( void );
addButtonCallBack_t PrefInit( void );
addButtonCallBack_t LayoutInit( void );
addButtonCallBack_t DisplayInit( void );
addButtonCallBack_t CmdoptInit( void );
addButtonCallBack_t OutputBitMapInit( void );
addButtonCallBack_t CustomMgrInit( void );
addButtonCallBack_t PriceListInit( void );
addButtonCallBack_t ParamFilesInit( void );
addButtonCallBack_t ControlMgrInit ( void );

/* cdraw.h */
track_p NewText( wIndex_t index, coOrd p, ANGLE_T angle, char * text,
                 CSIZE_T textSize, wDrawColor color, BOOL_T boxed );
void LoadFontSizeList( wList_p, long );
void UpdateFontSizeList( long *, wList_p, wIndex_t );
long GetFontSize(wIndex_t);
long GetFontSizeIndex(long size);

/* cnote.c */
void ClearNote( void );
void DoNote( void  * unused );
BOOL_T WriteMainNote( FILE * );
BOOL_T ReadMainNote(char * line);

/* cprintc.c */
coOrd GetPrintOrig();
ANGLE_T GetPrintAngle();


/* cruler.c */
void RulerRedraw( BOOL_T );
STATUS_T ModifyRuler( wAction_t, coOrd );
STATUS_T ModifyProtractor( wAction_t, coOrd );

/* csnap.c */
wIndex_t InitGrid( wMenu_p menu );
BOOL_T SnapPos( coOrd * );
BOOL_T SnapPosAngle( coOrd *, ANGLE_T * );
void DrawSnapGrid( drawCmd_p, coOrd, BOOL_T );
BOOL_T GridIsVisible( void );
void InitSnapGridButtons( void );
void SnapGridEnable( void * unused );
void SnapGridShow( void * unused );

/* ctodesgn.c */
void InitNewTurn( wMenu_p m );

/* cturntbl.c */
extern ANGLE_T turntableAngle;

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
extern FILE * customMgmF;
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

/* dcontmgm.c */
#define CONTMGM_CAN_EDIT                (1)
#define CONTMGM_DO_EDIT                 (2)
#define CONTMGM_CAN_DELETE              (3)
#define CONTMGM_DO_DELETE               (4)
#define CONTMGM_GET_TITLE               (5)
#define CONTMGM_DO_HILIGHT              (6)
#define CONTMGM_UN_HILIGHT              (7)

typedef int (*contMgmCallBack_p) (int, void *);
void ContMgmLoad (wIcon_p,contMgmCallBack_p,void *);

/* dease.c */
extern DIST_T easementVal;
extern DIST_T easeR;
extern DIST_T easeL;

/* denum.c */
extern int enumerateMaxDescLen;
void EnumerateList( long, FLOAT_T, char *, char * );
void EnumerateStart(void);
void EnumerateEnd(void);

/* doption.c */
extern long enableBalloonHelp;
extern long enableAudio;
long GetDistanceFormat( void );

/* cblock.c */
void InitCmdBlock( wMenu_p menu );
void BlockMgmLoad( void );
/* cswitchmotor.c */
void InitCmdSwitchMotor( wMenu_p menu );
void SwitchmotorMgmLoad( void );
/* csignal.c */
void InitCmdSignal ( wMenu_p menu );
void SignalMgmLoad ( void );
/* ccontrol.c */
void ControlMgmLoad ( void );
void InitCmdControl ( wMenu_p menu );
/* csensor.c */
void SensorMgmLoad ( void );
void InitCmdSensor ( wMenu_p menu );

/* cmodify.c */
extern wIndex_t modifyCmdInx;
STATUS_T CmdModify(wAction_t action,coOrd pos );

/* ctrain.c */
#define MODE_DESIGN		(0)
#define MODE_TRAIN		(1)
extern long programMode;
extern long maxCouplingSpeed;
extern long hideTrainsInTunnels;

/* fileio.c */
extern long checkPtInterval;
extern long autosaveChkPoints;
extern wIndex_t checkPtMark;

/* layout.c */
extern wIndex_t changed;
void SetFileChanged(void);

/* macro.c */
extern long adjTimer;
int RegressionTestAll();

/* lprintf.c */
typedef struct {
	char * name;
	int level;
} logTable_t;
extern dynArr_t logTable_da;
#define logTable(N) DYNARR_N( logTable_t, logTable_da, N )
extern time_t logClock;
void LogOpen( char * );
void LogClose( void );
void LogSet( char *, int );
int LogFindIndex( const char * );
void LogPrintf( const char *, ... );
#define LOG( DBINX, DBLVL, DBMSG ) \
		if ( (DBINX) > 0 && logTable( (DBINX) ).level >= (DBLVL) ) { \
				LogPrintf DBMSG ; \
		}
#define LOG1( DBINX, DBMSG ) LOG( DBINX, 1, DBMSG )
#define LOGNAME( DBNAME, DBMSG ) LOG( LogFindIndex( DBNAME ), DBMSG )

#define NUM_FILELIST (5)
#define lprintf LogPrintf

/* track.c */
extern void EnumerateTracks( void * unused );

#endif
