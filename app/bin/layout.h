/** \file layout.h
 * Layout data
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2017 Martin Fischer
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

#ifndef HAVE_LAYOUT_H
#define HAVE_LAYOUT_H

#include "common.h"

extern wButton_p backgroundB;		/** background visibility control */
void SetLayoutFullPath(const char *fileName);
void LoadLayoutMinRadiusPref(char *scaleName, double defaultValue);
void LoadLayoutMaxGradePref(char *scaleName, double defaultValue);
void SetLayoutTitle(char *title);
void SetLayoutSubtitle(char *title);
void SetLayoutMinTrackRadius(DIST_T radius);
void SetLayoutMaxTrackGrade(ANGLE_T angle);
void SetLayoutRoomSize(coOrd size);
void SetLayoutCurScale(SCALEINX_T scale);
void SetLayoutCurScaleDesc(SCALEDESCINX_T desc);
void SetLayoutCurGauge(GAUGEINX_T gauge);
void SetLayoutBackGroundFullPath(const char *fileName);
void SetLayoutBackGroundSize(double size);
void SetLayoutBackGroundPos(coOrd pos);
void SetLayoutBackGroundAngle(ANGLE_T angle);
void SetLayoutBackGroundScreen(int screen);

int DoSettingsRead(int files, char ** fileName, void * data );

char *GetLayoutFullPath(void);
char *GetLayoutFilename(void);
char *GetLayoutTitle(void);
char *GetLayoutSubtitle(void);
DIST_T GetLayoutMinTrackRadius(void);
ANGLE_T GetLayoutMaxTrackGrade(void);
SCALEINX_T GetLayoutCurScale(void );
SCALEDESCINX_T GetLayoutCurScaleDesc(void);
void GetLayoutRoomSize(coOrd *roomSize);

SCALEDESCINX_T GetLayoutCurScaleDesc(void);
char *GetLayoutBackGroundFullPath(void);
double GetLayoutBackGroundSize(void);
coOrd GetLayoutBackGroundPos(void);
ANGLE_T GetLayoutBackGroundAngle(void);
int GetLayoutBackGroundScreen(void);
int GetLayoutBackGroundVisible(void);
bool HasBackGround(void);
void LayoutBackGroundInit(BOOL_T clear);
void LayoutBackGroundLoad(void);
void LayoutBackGroundSave(void);
void BackgroundToggleShow(void * unused);
void DoLayout(void * unused);
int LoadImageFile(int files,char ** fileName,void * data );
#endif
