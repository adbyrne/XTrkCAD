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

extern long adjTimer;
extern int log_error;

extern long toolbarSet;
extern ANGLE_T turntableAngle;
extern long maxCouplingSpeed;
extern long hideSelectionWindow;
extern long labelWhen;
extern long labelScale;
extern long labelEnable;
extern long colorTrack;
extern long colorDraw;
extern long carHotbarModeInx;
extern DIST_T minLength;
extern DIST_T connectDistance;
extern ANGLE_T connectAngle;
extern long twoRailScale;
extern long mapScale;
extern long constrainMain;
extern long dontHideCursor;
extern long checkPtInterval;
extern long autosaveChkPoints;
extern long liveMap;
extern long preSelect;
extern long hideTrainsInTunnels;
extern long listLabels;
extern long layoutLabels;
extern long descriptionFontSize;
extern long units;
extern long onStartup;
extern long angleSystem;
extern DIST_T trackGauge;
extern DIST_T curScaleRatio;
extern char * curScaleName;
extern int enumerateMaxDescLen;
extern long enableBalloonHelp;
extern long showFlexTrack;
extern long hotBarLabels;
extern long rightClickMode;
extern long selectMode;
extern long selectZero;
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



extern wWin_p mainW;
extern wIndex_t changed;
extern char message[STR_HUGE_SIZE];
extern long paramVersion;
extern coOrd zero;
extern wBool_t extraButtons;
extern wButton_p backgroundB;		/** background visibility control */
extern wIndex_t checkPtMark;

#define wControlBelow( B )		(wControlGetPosY((wControl_p)(B))+wControlGetHeight((wControl_p)(B)))
#define wControlBeside( B )		(wControlGetPosX((wControl_p)(B))+wControlGetWidth((wControl_p)(B)))

/*
 * Safe Memory etc
 */
void * MyMalloc( size_t );
void * MyRealloc( void *, size_t );
void MyFree( void * );
void * memdup( void *, size_t );
char * MyStrdup( const char * );

char * ConvertFromEscapedText(const char * text);
char * ConvertToEscapedText(const char * text);

const char * AbortMessage( const char *, ... );
void AbortProg( const char *, const char *, int, const char * );
#ifdef LOG_CHECK_COVERAGE
#define CHECK( X ) lprintf( "CHECK %s:%i\n", __FILE__, __LINE__ ); if ( !(X) ) AbortProg( #X, __FILE__, __LINE__, NULL )
#define CHECKMSG( X, MSG ) lprintf( "CHECK %s:%i\n", __FILE__, __LINE__ ); if ( !(X) ) AbortProg( #X, __FILE__, __LINE__, AbortMessage MSG )
#else
#define CHECK( X ) if ( !(X) ) AbortProg( #X, __FILE__, __LINE__, NULL )
#define CHECKMSG( X, MSG ) if ( !(X) ) AbortProg( #X, __FILE__, __LINE__, AbortMessage MSG )
#endif

char * Strcpytrimed( char *, const char *, BOOL_T );
wBool_t CheckHelpTopicExists(const char * topic);

void InfoMessage( const char *, ... );
void ErrorMessage( const char *, ... );
int NoticeMessage( const char *, const char*, const char *, ... );
int NoticeMessage2( int, const char *, const char*, const char *, ... );

bool Confirm( char *, doSaveCallBack_p );
void DoQuit( void * unused );
void MapWindowShow( int state );

void wShow( wWin_p );
void wHide( wWin_p );
void CloseDemoWindows( void );
void DefaultProc( wWin_p, winProcEvent, void * );
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
void RegisterChangeNotification( changeNotificationCallBack_t );
void DoChangeNotification( long );


/* foreign externs */

extern wIndex_t modifyCmdInx;
extern wIndex_t joinCmdInx;
/* chotbar.c */
extern long showFlexTrack;
extern long hotBarLabels;

/* ctodesgn.c */
void InitNewTurn( wMenu_p m );

/* cturntbl.c */
extern ANGLE_T turntableAngle;

/* cnote.c */
void ClearNote( void );

/* cprintc.c */
coOrd GetPrintOrig();
ANGLE_T GetPrintAngle();


/* cruler.c */
void RulerRedraw( BOOL_T );
STATUS_T ModifyRuler( wAction_t, coOrd );
STATUS_T ModifyProtractor( wAction_t, coOrd );

/* dialogs */
void OutputBitMap( void );

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

wIndex_t InitGrid( wMenu_p menu );

BOOL_T SnapPos( coOrd * );
BOOL_T SnapPosAngle( coOrd *, ANGLE_T * );
void DrawSnapGrid( drawCmd_p, coOrd, BOOL_T );
BOOL_T GridIsVisible( void );
void InitSnapGridButtons( void );
void SnapGridEnable( void * unused );
void SnapGridShow( void * unused );

void ScaleLengthEnd( void );
void EnumerateList( long, FLOAT_T, char * , char * );
void EnumerateStart(void);
void EnumerateEnd(void);

/* cnote.c */
void DoNote( void  * unused );
BOOL_T WriteMainNote( FILE * );

BOOL_T ReadMainNote(char * line);

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

/* doption.c */
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
STATUS_T CmdModify(wAction_t action,coOrd pos );

/* layout.c */
void SetFileChanged(void);

/* macro.c */
int RegressionTestAll();


#define LABEL_MANUF		(1<<0)
#define LABEL_PARTNO	(1<<1)
#define LABEL_DESCR		(1<<2)
#define LABEL_COST		(1<<7)
#define LABEL_FLIPPED	(1<<8)
#define LABEL_TABBED	(1<<9)
#define LABEL_UNGROUPED (1<<10)
#define LABEL_SPLIT		(1<<11)

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
		if ( DBINX > 0 && logTable( DBINX ).level >= DBLVL ) { \
				LogPrintf DBMSG ; \
		}
#define LOG1( DBINX, DBMSG ) LOG( DBINX, 1, DBMSG )
#define LOGNAME( DBNAME, DBMSG ) LOG( LogFindIndex( DBNAME ), DBMSG )

#define NUM_FILELIST (5)
#define lprintf LogPrintf

/* track.c */
extern void EnumerateTracks( void * unused );

#endif
