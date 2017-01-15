/** \file dxfformat.c
* Formating of DXF commands and parameters
*/

/*  XTrkCad - Model Railroad CAD
*  Copyright (C)2017 Martin Fischer
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
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>

#include <dynstring.h>
#include "dxfformat.h"

extern char *sProdNameUpper;

/**
* Build and format layer name. The name is created by appending the layer number
* to the basic layer name.
*
* \param result OUT buffer for result
* \param name IN base part of  name
* \param layer IN layer number
*/

void DxfLayerName(DynString *result, char *name, int layer)
{
    DynStringPrintf(result, DXF_INDENT "8\n%s%d\n", name, layer);
}

/**
* Build and format a position.
*
* \param result OUT buffer for result
* \param type IN type of position following DXF specs
* \param value IN position
*/

void DxfFormatPosition(DynString *result, int type, double value)
{
    DynStringPrintf(result, DXF_INDENT "%d\n%0.6f\n", type, value);
}

/**
* Build and format the line style definition
*
* \param result OUT buffer for result
* \param type IN line style TRUE for dashed, FALSE for solid lines
*/

void DxfLineStyle(DynString *result, int isDashed)
{
    DynStringPrintf(result, DXF_INDENT "6\n%s\n",
                    (isDashed ? "DASHED" : "CONTINUOUS"));
}

/**
* Build and format layer name. The name is created by appending the layer number
* to the basic layer name. The result is appended to the existing result buffer.
*
* \param output OUT buffer for result
* \param basename IN base part of  name
* \param layer IN layer number
*/

static void
DxfAppendLayerName(DynString *output, int layer)
{
    DynString formatted = NaS;
    DynStringMalloc(&formatted, 0);
    DxfLayerName(&formatted, sProdNameUpper, layer);
    DynStringCatStr(output, &formatted);
    DynStringFree(&formatted);
}

/**
* Build and format a position. The result is appended to the existing result buffer.
*
* \param output OUT buffer for result
* \param type IN type of position following DXF specs
* \param value IN position
*/

static void
DxfAppendPosition(DynString *output, int type, double value)
{
    DynString formatted = NaS;
    DynStringMalloc(&formatted, 0);
    DxfFormatPosition(&formatted, type, value);
    DynStringCatStr(output, &formatted);
    DynStringFree(&formatted);
}

/**
* Build and format the line style definition. The result is appended to the existing result buffer.
*
* \param result OUT buffer for result
* \param type IN line style TRUE for dashed, FALSE for solid lines
*/

static void
DxfAppendLineStyle(DynString *output, int style)
{
    DynString formatted = NaS;
    DynStringMalloc(&formatted, 0);
    DxfLineStyle(&formatted, style);
    DynStringCatStr(output, &formatted);
    DynStringFree(&formatted);
}

/**
* Format a complete LINE command after DXF spec
*
* \param result OUT buffer for the completed command
* \param layer IN number part of the layer
* \param x0, y0 IN first endpoint
* \param x1, y1 IN second endpoint
* \param style IN line style, TRUE for dashed, FALSE for continuous
*/

void
DxfLineCommand(DynString *result, int layer, double x0,
               double y0, double x1, double y1, int style)
{
    DynStringCatCStr(result, DXF_INDENT "0\nLINE\n");
    DxfAppendLayerName(result, layer);
    DxfAppendPosition(result, 10, x0);
    DxfAppendPosition(result, 20, y0);
    DxfAppendPosition(result, 11, x1);
    DxfAppendPosition(result, 21, y1);
    DxfAppendLineStyle(result, style);
}

/**
* Format a complete CIRCLE command after DXF spec
*
* \param result OUT buffer for the completed command
* \param layer IN number part of the layer
* \param x, y IN center point
* \param r IN radius
* \param style IN line style, TRUE for dashed, FALSE for continuous
*/

void
DxfCircleCommand(DynString *result, int layer, double x,
                 double y, double r, int style)
{
    DynStringCatCStr(result, DXF_INDENT "0\nCIRCLE\n");
    DxfAppendPosition(result, 10, x);
    DxfAppendPosition(result, 20, y);
    DxfAppendPosition(result, 40, r);
    DxfAppendLayerName(result, layer);
    DxfAppendLineStyle(result, style);
}

/**
* Format a complete ARC command after DXF spec
*
* \param result OUT buffer for the completed command
* \param layer IN number part of the layer
* \param x, y IN center point
* \param r IN radius
* \param a0 IN starting angle
* \param a1 IN ending angle
* \param style IN line style, TRUE for dashed, FALSE for continuous
*/

void
DxfArcCommand(DynString *result, int layer, double x, double y,
              double r, double a0, double a1, int style)
{
    DynStringCatCStr(result, DXF_INDENT "0\nARC\n");
    DxfAppendPosition(result, 10, x);
    DxfAppendPosition(result, 20, y);
    DxfAppendPosition(result, 40, r);
    DxfAppendPosition(result, 50, a0);
    DxfAppendPosition(result, 51, a0+a1);
    DxfAppendLayerName(result, layer);
    DxfAppendLineStyle(result, style);
}

/**
* Format a complete TEXT command after DXF spec
*
* \param result OUT buffer for the completed command
* \param layer IN number part of the layer
* \param x, y IN text position
* \param size IN font size
* \param text IN text
*/

void
DxfTextCommand(DynString *result, int layer, double x,
               double y, double size, char *text)
{
    DynStringCatCStr(result, DXF_INDENT "0\nTEXT\n");
    DynStringCatCStrs(result, DXF_INDENT "1\n", text, "\n", NULL);
    DxfAppendPosition(result, 10, x);
    DxfAppendPosition(result, 20, y);
    DxfAppendPosition(result, 40, size/72.0);
    DxfAppendLayerName(result, layer);
}

/**
* Create the complete prologue for a DXF file. Includes the header section,
* a table for line styles and a table for layers.
*
* \param result OUT buffer for the completed command
* \param layerCount IN count of defined layers
* \param x0, y0 IN minimum (left bottom) position
* \param x1, y1 IN maximum (top right) position
*/

void
DxfPrologue(DynString *result, int layerCount, double x0, double y0, double x1, double y1)
{
	int i;
	DynString layer = NaS;
	DynStringMalloc(&layer, 0);

	DynStringCatCStr(result, "\
  0\nSECTION\n\
  2\nHEADER\n\
  9\n$ACADVER\n  1\nAC1009\n\
  9\n$EXTMIN\n");

	DxfAppendPosition(result, 10, x0);
	DxfAppendPosition(result, 20, y0);
	DynStringCatCStr(result, "  9\n$EXTMAX\n");
	DxfAppendPosition(result, 10, x1);
	DxfAppendPosition(result, 20, y1);

	DynStringCatCStr(result, "\
  9\n$TEXTSTYLE\n  7\nSTANDARD\n\
  0\nENDSEC\n\
  0\nSECTION\n\
  2\nTABLES\n\
  0\nTABLE\n\
  2\nLTYPE\n\
  0\nLTYPE\n  2\nCONTINUOUS\n  70\n0\n\
  3\nSolid line\n\
  72\n65\n  73\n0\n  40\n0\n\
  0\nLTYPE\n  2\nDASHED\n  70\n0\n\
  3\n__ __ __ __ __ __ __ __ __ __ __ __ __ __ __\n\
  72\n65\n  73\n2\n  40\n0.15\n  49\n0.1\n  49\n-0.05\n\
  0\nLTYPE\n  2\nDOT\n  70\n0\n\
  3\n...............................................\n\
  72\n65\n  73\n2\n  40\n0.1\n  49\n0\n  49\n-0.05\n\
  0\nENDTAB\n\
  0\nTABLE\n\
  2\nLAYER\n\
  70\n0\n");

	for (i = 0; i < layerCount; i++)
	{
		DynStringPrintf(&layer, DXF_INDENT"0\nLAYER\n  2\n%s%d\n  6\nCONTINUOUS\n  62\n7\n  70\n0\n", sProdNameUpper, i + 1);
		DynStringCatStr(result, &layer);
	}

	DynStringCatCStr(result, "\
  0\nENDTAB\n\
  0\nENDSEC\n\
  0\nSECTION\n\
  2\nENTITIES\n");
}

/**
* Create the file footer for a DXF file. Closes the open section and places 
* an end-of-file marker 
*
* \param result OUT buffer for the completed command
*/

void
DxfEpilogue(DynString *result)
{
	DynStringCatCStr(result, DXF_INDENT "0\nENDSEC\n" DXF_INDENT "0\nEOF\n");
}
