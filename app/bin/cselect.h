/** \file cselect.h
 * Definitions and function prototypes for operations on selected elements
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

#ifndef CSELECT_H
#define CSELECT_H

#include "common.h"

#define defaultCursor wCursorCross

extern wIndex_t selectCmdInx;
extern wIndex_t moveCmdInx;
extern wIndex_t rotateCmdInx;
extern wIndex_t flipCmdInx;

extern long selectMode;
extern long selectZero;
extern int incrementalDrawLimit;
extern long selectedTrackCount;

void InvertTrackSelect( void * unused );
void OrphanedTrackSelect( void * unused );
void SetAllTrackSelect( BOOL_T );
void SelectTunnel( void * unused );
void SelectBridge( void * unused );
void SelectRoadbed( void * unused );
void SelectTies( void * unused );
void SelectRecount( void );
void SelectTrackWidth( void* );
int SelectDelete( void );
void TrySelectDelete( void );
void MoveToJoin( track_p, EPINX_T, track_p, EPINX_T );
void MoveSelectedTracksToCurrentLayer( void * unused );
void SelectCurrentLayer( void * unused );
void DeselectLayer( unsigned int );
void SelectByIndex( void* string);
void ClearElevations( void * unused );
void AddElevations( DIST_T );
void DoRefreshCompound( void * unused );
void WriteSelectedTracksToTempSegs( void );
STATUS_T CmdMoveDescription( wAction_t, coOrd );
void DrawHighlightBoxes(BOOL_T, BOOL_T,track_p);
void HighlightSelectedTracks(track_p trk_ignore, BOOL_T keep, BOOL_T invert );

#endif
