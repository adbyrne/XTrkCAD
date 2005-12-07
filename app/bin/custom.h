/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/custom.h,v 1.1 2005-12-07 15:47:10 rc-flyer Exp $
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

extern int bEnableFlex;
extern int bParamFiles;
extern int bEnablePrices;

void InitCustom( void );

void InitTrkCurve( void );
void InitTrkDraw( void );
void InitTrkEase( void );
void InitTrkNote( void );
void InitTrkStraight( void );
void InitTrkStruct( void );
void InitTrkTableEdge( void );
void InitTrkText( void );
void InitTrkTrack( void );
void InitTrkTurnout( void );
void InitTrkTurntable( void );

void InitCmdCurve( void );
void InitCmdDraw( void );
void InitCmdElevation( void );
void InitCmdJoin( void );
void InitCmdProfile( void );
void InitCmdPull( void );
void InitCmdTighten( void );
void InitCmdModify( void );
void InitCmdMove( void );
void InitCmdMoveDescription( void );
void InitCmdStraight( void );
void InitCmdDescribe( void );
void InitCmdSelect( void );
void InitCmdDelete( void );
void InitCmdSplit( void );
void InitCmdTunnel( void );
void InitCmdRuler( void );
void InitCmdMove( void );
void InitCmdParallel( void );
wIndex_t InitCmdPrint( void );
void InitCmdTableEdge( void );
void InitCmdText( void );
void InitCmdTrain( void );
void InitCmdTurnout( void );
void InitCmdHandLaidTurnout( void );
void InitCmdTurntable( void );
void InitCmdNote( void );
void InitCmdUndo( void );
void InitCmdStruct( void );
void InitCmdAboveBelow( void );
void InitCmdEnumerate( void );
void InitCmdExport( void );

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
