/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/custom.h,v 1.7 2010-01-01 13:24:59 m_fischer Exp $
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

#ifndef CUSTOM_H
#define CUSTOM_H

#define ICON_WIDTH		(64)
#define ICON_HEIGHT		(64)

#define BG_SELECT		(0)
#define BG_ZOOM			(1)
#define BG_UNDO			(2)
#define BG_EASE			(3)
#define BG_TRKCRT		(4)
#define BG_TRKMOD		(5)
#define BG_TRKGRP		(6)
#define BG_MISCCRT		(7)
#define BG_RULER		(8)
#define BG_LAYER		(9)
#define BG_HOTBAR		(10)
#define BG_SNAP			(11)
#define BG_TRAIN		(12)
#define BG_COUNT		(13)
#define BG_FILE			(14)
#define BG_CONTROL		(15)
#define BG_BIGGAP		(1<<8)
extern int cmdGroup;

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

extern char * sSourceFilePattern;
extern char * sImportFilePattern;
extern char * sDXFFilePattern;
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
void InitTrkNote( void );
void InitTrkStraight( void );
void InitTrkStruct( void );
void InitTrkTableEdge( void );
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
void InitCmdHelix( wMenu_p menu );
void InitCmdDraw( wMenu_p menu );
void InitCmdElevation( wMenu_p menu );
void InitCmdJoin( wMenu_p menu );
void InitCmdProfile( wMenu_p menu );
void InitCmdPull( wMenu_p menu );
void InitCmdTighten( void );
void InitCmdModify( wMenu_p menu );
void InitCmdMove( wMenu_p menu );
void InitCmdMoveDescription( wMenu_p menu );
void InitCmdStraight( wMenu_p menu );
void InitCmdDescribe( wMenu_p menu );
void InitCmdSelect( wMenu_p menu );
void InitCmdDelete( void );
void InitCmdSplit( wMenu_p menu );
void InitCmdTunnel( void );
void InitCmdRuler( wMenu_p menu );

void InitCmdParallel( wMenu_p menu );
wIndex_t InitCmdPrint( wMenu_p menu );
void InitCmdTableEdge( void );
void InitCmdText( wMenu_p menu );
void InitCmdTrain( wMenu_p menu );
void InitCmdTurnout( wMenu_p menu );
void InitCmdHandLaidTurnout( wMenu_p menu );
void InitCmdTurntable( wMenu_p menu );
void InitCmdNote( wMenu_p menu );
void InitCmdUndo( void );
void InitCmdStruct( wMenu_p menu );
void InitCmdAboveBelow( void );
void InitCmdEnumerate( void );
void InitCmdExport( void );
void InitCmdEasement( void );

char * MakeWindowTitle( char * );
addButtonCallBack_t EasementInit( void );
addButtonCallBack_t StructDesignerInit( void );

void InitLayers( void );
void InitHotBar( void );
void InitCarDlg( void );
BOOL_T Initialize( void );
void DoEasementRedir( void );
void DoStructDesignerRedir( void );
void InitNewTurnRedir( wMenu_p );
void RedrawAbout( wDraw_p, void *, wPos_t, wPos_t );
void DoKeycheck( char * );

#endif
