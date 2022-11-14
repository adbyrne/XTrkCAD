/** \file svgformat.h
 * Definitions and prototypes for SVG export
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

#ifndef HAVE_SVGFORMAT_H
#define HAVE_SVGFORMAT_H
#include <stdbool.h>
#include <mxml.h>

#define MINIMUMLINEWIDTH 4.0

typedef  mxml_node_t SVGParent;
typedef  mxml_node_t SVGDocument;

void SvgAddCSSStyle(SVGParent *svg);
void SvgLineCommand(SVGParent *svg, double x0, double y0, double x1, double y1,
                    double w, long c, unsigned lineOpt);
void SvgPolyLineCommand(SVGParent *svg, int cnt, double *points, int color,
                        double width, bool fill, unsigned lineStyle);
void SvgRectCommand(SVGParent *svg, double x0, double y0, double x1, double y1,
                    int color, unsigned linestyle);
void SvgCircleCommand(SVGParent *svg, double x, double y, double r, double w,
                      long c, bool fill, unsigned lineStyle);
void SvgArcCommand(SVGParent *svg, double x, double y, double r, double a0,
                   double a1, bool center, double w, long c, unsigned lineStyle);
void SvgTextCommand(SVGParent *svg, double x, double y, double size, long c,
                    char *text);
void SvgAddTitle(SVGParent *svg, char *title);

SVGDocument *SvgCreateDocument(void);
SVGParent *SvgPrologue(SVGDocument *result, char *id, int layerCount, double x0,
                       double y0, double x1, double y1);

bool SvgSaveFile(SVGDocument *svg, char *filename);
void SvgDestroyDocument(SVGDocument *svg);
#endif // !HAVE_SVGFORMAT_H

