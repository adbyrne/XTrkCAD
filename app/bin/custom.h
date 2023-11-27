/** \file custom.h
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef CUSTOM_H
#define CUSTOM_H

#include "common.h"

extern char * sProdName;
extern char * sProdNameLower;
extern char * sProdNameUpper;

extern char * sEnvExtra;

extern char * sTurnoutDesignerW;

extern char * sAboutProd;

extern char * sCustomF;
extern char * sCheckPointF;
extern char * sCheckPoint1F;
extern char * sClipboardF;
extern char * sParamQF;
extern char * sUndoF;
extern char * sAuditF;
extern char * sTipF;

extern char * sSourceFilePattern;
extern char * sSaveFilePattern;
extern char * sImageFilePattern;
extern char * sImportFilePattern;
extern char * sDXFFilePattern;
extern char * sSVGFilePattern;
extern char * sRecordFilePattern;
extern char * sNoteFilePattern;
extern char * sLogFilePattern;
extern char * sPartsListFilePattern;

extern char * sVersion;
extern int iParamVersion;
extern int iMinParamVersion;
extern long lParamKey;

//extern int bEnablePrices;

void InitCustom( void );
void CleanupCustom( void );

void InitTrkCurve( void );
void InitTrkBezier( void );
void InitTrkDraw( void );
void InitTrkEase( void );
void InitTrkCornu( void );
void InitTrkNote(wMenu_p menu);
void InitTrkStraight( void );
void InitTrkStruct( void );
void InitTrkText( void );
void InitTrkTrack( void );
void InitTrkTurnout( void );
void InitTrkTurntable( void );
void InitTrkBlock( void );
void InitTrkSwitchMotor( void );
void InitTrkSignal ( void );
void InitTrkControl ( void );
void InitTrkSensor ( void );

void InitCmdCurve( wMenu_p menu );
void InitCmdCornu( wMenu_p menu);
void InitCmdHelix( wMenu_p menu );
void InitCmdDraw( wMenu_p menu );
void InitCmdElevation( wMenu_p menu );
void InitCmdJoin( wMenu_p menu );
void InitCmdProfile( wMenu_p menu );
void InitCmdPull( wMenu_p menu );
void InitCmdModify( wMenu_p menu );
void InitCmdMove( wMenu_p menu );
void InitCmdMoveDescription( wMenu_p menu );
void InitCmdStraight( wMenu_p menu );
void InitCmdDescribe( wMenu_p menu );
void InitCmdDescribe2( wMenu_p menu );
void InitCmdSelect( wMenu_p menu );
void InitCmdSelect2( wMenu_p menu );
void InitCmdPan( wMenu_p menu );
void InitCmdPan2( wMenu_p menu );
void InitCmdDelete( void );
void InitCmdSplit( wMenu_p menu );
void InitCmdTies( void );
void InitCmdTunnel( void );
void InitCmdBridge( void );
void InitCmdRoadbed( void );
void InitCmdRuler( wMenu_p menu );

void InitCmdParallel( wMenu_p menu );
wIndex_t InitCmdPrint( wMenu_p menu );
void InitCmdText( wMenu_p menu );
void InitCmdTrain( wMenu_p menu );
void InitCmdTurnout( wMenu_p menu );
void InitCmdHandLaidTurnout( wMenu_p menu );
void InitCmdTurntable( wMenu_p menu );
void InitCmdNote();
void InitCmdUndo( void );
void InitCmdStruct( wMenu_p menu );
void InitCmdAboveBelow( void );
//void InitCmdEnumerate( void );
void InitCmdEasement( void );

char * MakeWindowTitle( char * );
addButtonCallBack_t EasementInit( void );

void InitLayers( int cmdGroup );
void InitHotBar( void );
void InitCarDlg( void );
BOOL_T Initialize( void );
void DoEasementRedir( void * unused );
void DoStructDesignerRedir( void );
void InitNewTurnRedir( wMenu_p );

void InitAppDefaults(void);

#endif
