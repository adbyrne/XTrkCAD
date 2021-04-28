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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef CSELECT_H
#define CSELECT_H

#include "common.h"

#define defaultCursor wCursorCross

extern wIndex_t selectCmdInx;
extern wIndex_t moveCmdInx;
extern wIndex_t rotateCmdInx;
extern int incrementalDrawLimit;
extern long selectedTrackCount;

void InvertTrackSelect( void * );
void OrphanedTrackSelect( void * );
void SetAllTrackSelect( BOOL_T );
void SelectTunnel( void * junk );
void SelectBridge( void * junk );
void SelectTies( void * junk );
void SelectRecount( void );
void SelectTrackWidth( void* );
int SelectDelete( void );
void MoveToJoin( track_p, EPINX_T, track_p, EPINX_T );
void MoveSelectedTracksToCurrentLayer( void * junk );
void SelectCurrentLayer( void * junk );
void DeselectLayer( unsigned int );
void SelectByIndex( void* string);
void ClearElevations( void * junk );
void AddElevations( DIST_T );
void DoRefreshCompound( void * junk );
void WriteSelectedTracksToTempSegs( void );
void DoRescale( void *junk );
STATUS_T CmdMoveDescription( wAction_t, coOrd );
void DrawHighlightBoxes(BOOL_T, BOOL_T,track_p);
void HighlightSelectedTracks(track_p trk_ignore, BOOL_T keep, BOOL_T invert );

#endif
