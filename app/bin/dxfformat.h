/** \file dxfformat.h
 * Definitions and prototypes for DXF export
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

#ifndef HAVE_DXFFORMAT_H
#define HAVE_DXFFORMAT_H

#include "dynstring.h"

enum DXF_DIMENSIONS
{
	DXF_DIMTEXTSIZE,
	DXF_DIMARROWSIZE
};

void DxfLayerName(DynString *result, char *name, int layer);
void DxfFormatPosition(DynString *result, int type, double value);
void DxfLineStyle(DynString *result, int isDashed);

void DxfLineCommand(DynString *result, int layer, double x0, double yo, double x1, double y1, int style);
void DxfCircleCommand(DynString *result, int layer, double x, double y, double r, int style);
void DxfArcCommand(DynString *result, int layer, double x, double y, double r, double a0, double a1, int style);
void DxfTextCommand(DynString *result, int layer, double x, double y, double size, char *text);
void DxfUnits(DynString *result);
void DxfDimensionSize(DynString *result, enum DXF_DIMENSIONS dimension);

void DxfPrologue(DynString *result, int layerCount, double x0, double y0, double x1, double y1);
void DxfEpilogue(DynString *result);
#define DXF_INDENT "  "

#endif // !HAVE_DXFFORMAT_H

