
/** \file dlayer.h
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

#ifndef DLAYER_H
#define DLAYER_H

#include "common.h"
extern unsigned int curLayer;
extern long layerCount;
void SetCurrLayer(wIndex_t inx, const char * name, wIndex_t op,
                  void * listContext, void * arg);
wDrawColor GetLayerColor( unsigned int );
BOOL_T GetLayerUseDefault( unsigned int );
SCALEINX_T GetLayerScale( unsigned int );
BOOL_T GetLayerUseColor( unsigned int);
BOOL_T GetLayerVisible( unsigned int );
void FlipLayer( void * layerVP );
BOOL_T GetLayerFrozen( unsigned int );
BOOL_T GetLayerOnMap( unsigned int );
BOOL_T GetLayerModule( unsigned int );
BOOL_T GetLayerHidden( unsigned int);
tieData_t GetLayerTieData( unsigned int );
DIST_T GetLayerMinTrackRadius( unsigned int layer );
ANGLE_T GetLayerMaxTrackGrade( unsigned int layer );
void SetLayerModule(unsigned int, BOOL_T);
char * GetLayerName( unsigned int );
void SetLayerName(unsigned int layer, char* name);
BOOL_T ReadLayers( char * );
BOOL_T WriteLayers( FILE * );
char * FormatLayerName(unsigned int layerNumber);
// void UpdateLayerLists( void );
void DefaultLayerProperties(void);
void UpdateLayerDlg( unsigned int );
void ResetLayers( void );
void SaveLayers( void );
void RestoreLayers( void );
void LoadLayerLists( void );
addButtonCallBack_t InitLayersDialog( void );
addButtonCallBack_t InitDrawOrderDialog( void );
void FillLayerList(wList_p layerList);

void LayerAllDefaults();
void LayerSetCounts();
int FindUnusedLayer(unsigned int start);
void DecrementLayerObjects(unsigned int index);
void IncrementLayerObjects(unsigned int index);


#endif
