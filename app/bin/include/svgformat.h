/** \file svgformat.h
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

#ifndef HAVE_SVGFORMAT_H
#define HAVE_SVGFORMAT_H
#include <stdbool.h>
#include "dynstring.h"
#include <mxml.h>

typedef  mxml_node_t SVGParent;
typedef  mxml_node_t SVGDocument;

void SvgLayerName(DynString *result, char *name, int layer);
void SvgFormatPosition(DynString *result, int type, double value);
void SvgLineStyle(DynString *result, int isDashed);

void SvgLineCommand(SVGParent *svg, double x0, double y0, double x1, double y1, wDrawWidth w, wDrawColor c);
void SvgCircleCommand(DynString *result, int layer, double x, double y, double r, int style);
void SvgArcCommand(DynString *result, int layer, double x, double y, double r, double a0, double a1, int style);
void SvgTextCommand(DynString *result, int layer, double x, double y, double size, char *text);
void SvgUnits(DynString *result);
void SvgDimensionSize(DynString *result, enum DXF_DIMENSIONS dimension);

SVGDocument *SvgCreateDocument(void);
SVGParent *SvgPrologue(SVGDocument *result, int layerCount, double x0, double y0, double x1, double y1);
void SvgEpilogue(DynString *result);
bool SvgSaveFile(SVGDocument *svg, char *filename);
#define SVG_INDENT "  "

#endif // !HAVE_SVGFORMAT_H

