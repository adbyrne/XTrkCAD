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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef HAVE_LAYOUT_H
#define HAVE_LAYOUT_H

#include "common.h"

void SetLayoutFullPath(const char *fileName);
void LoadLayoutMinRadiusPref(char *scaleName, double defaultValue);
void SetLayoutTitle(char *title);
void SetLayoutSubtitle(char *title);
void SetLayoutMinTrackRadius(DIST_T radius);
void SetLayoutMaxTrackGrade(ANGLE_T angle);
void SetLayoutRoomSize(coOrd size);
void SetLayoutCurScale(SCALEINX_T scale);
void SetLayoutCurScaleDesc(SCALEDESCINX_T desc);
void SetLayoutCurGauge(GAUGEINX_T gauge);
void SetLayoutScaleGauge(SCALEDESCINX_T desc, GAUGEINX_T gauge);

char *GetLayoutFullPath(void);
char *GetLayoutFilename(void);
char *GetLayoutTitle(void);
char *GetLayoutSubtitle(void);
DIST_T GetLayoutMinTrackRadius(void);
SCALEINX_T GetLayoutCurScale(void );
SCALEDESCINX_T GetLayoutCurScaleDesc(void);
//GAUGEINX_T GetLayoutCurGauge(void);


ANGLE_T GetLayoutMaxTrackGrade(void);
SCALEDESCINX_T GetLayoutCurScaleDesc(void);

void DoLayout(void * junk);
#endif