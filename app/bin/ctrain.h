/** \file ctrain.h
 * Definitions and prototypes for train operations
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

#ifndef HAVE_CTRAIN_H
#define HAVE_CTRAIN_H

#include "common.h"
#include "track.h" //- traverseTrack
#include "include/cars.h"


extern wIndex_t trainCmdInx;

extern long trainPause;

typedef struct vector_s {
	coOrd pos;
	ANGLE_T angle;
} vector_t;


extern wControl_p newCarControls[2];
void DoCarDlg( void * unused );


track_p NewCar( wIndex_t, carItem_p, coOrd, ANGLE_T );
void UncoupleCars( track_p, int );
void CarGetPos( track_p, coOrd *, ANGLE_T * );
void CarSetVisible( track_p );
void CarItemUpdate( carItem_p );

void CarUpdateHotbarList();

void ClearCars( void );
void CarDlgAddProto( void );
void CarDlgAddDesc( void );
void AttachTrains( void );

BOOL_T StoreCarItem (carItem_p item, void **data,long *len);
BOOL_T ReplayCarItem(carItem_p item, void *data,long len);

int CarAvailableCount( void );
BOOL_T TraverseTrack2( traverseTrack_p, DIST_T );
void FlipTraverseTrack( traverseTrack_p );
void CheckCarTraverse( track_p trk);

void LocoListChangeEntry( track_p, track_p );

#endif // !HAVE_CTRAIN_H
