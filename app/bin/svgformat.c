/** \file svgformat.c
* Formating of SVG commands and parameters
*/

/*  XTrkCad - Model Railroad CAD
*  Copyright (C)2020 Martin Fischer
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

#include <xtrkcad-config.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include <dynstring.h>
#include "fileio.h"
#include "mxml.h"
#include "include/svgformat.h"


extern char *sProdNameUpper;
extern long units;						   /**< meaning is 0 = English, 1 = metric */


/**
* Format a dimension and add to XML node as an attribute. 
*
* \PARAM [in, out]	node	the XML node
* \PARAM [in]		name	name of attribute
* \param [in]		value	size		 
*/

static void
SvgAddCoordinate(mxml_node_t *node, char *name, double value)
{
	mxmlElementSetAttrf(node, name, "%f in", value);
}

/**
 * Svg add color
 *
 * \param [in,out] node  If non-null, the node.
 * \param 		   color The color.
 */

static void 
SvgAddColor(mxml_node_t *node, int color)
{
	mxmlElementSetAttrf(node, "stroke", "#%x", color);
}

/**
 * Build and format layer name. The name is created by appending the layer number to the basic
 * layer name.
 *
 * \param [out]	   result OUT buffer for result.
 * \param [in,out] name   IN base part of  name.
 * \param 		   layer  IN layer number.
 */

void SvgLayerName(DynString *result, char *name, int layer)
{
    //DynStringPrintf(result, SVG_INDENT "8\n%s%d\n", name, layer);
}

/**
 * Build and format a position. If it specifies a point the value is assumed to be in inches and
 * will be converted to millimeters if the metric system is active. If it is an angle (group
 * codes 50 to 59) it is not converted.
 *
 * \param [out] result OUT buffer for result.
 * \param 	    type   IN type of position following DXF specs.
 * \param 	    value  IN position.
 */

void SvgFormatPosition(DynString *result, int type, double value)
{
	if (units == 1)
	{
		if( type < 50 || type > 58 )
			value *= 25.4;
	}
		
    //DynStringPrintf(result, SVG_INDENT "%d\n%0.6f\n", type, value);
}

/**
* Build and format the line style definition
*
* \param result OUT buffer for result
* \param type IN line style TRUE for dashed, FALSE for solid lines
*/

void SvgLineStyle(DynString *result, int isDashed)
{
    //DynStringPrintf(result, SVG_INDENT "6\n%s\n",
    //                (isDashed ? "DASHED" : "CONTINUOUS"));
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
SvgAppendLayerName(DynString *output, int layer)
{
    //DynString formatted = NaS;
    //DynStringMalloc(&formatted, 0);
    //SvgLayerName(&formatted, sProdNameUpper, layer);
    //DynStringCatStr(output, &formatted);
    //DynStringFree(&formatted);
}

/**
* Build and format a position. The result is appended to the existing result buffer.
*
* \param output OUT buffer for result
* \param type IN type of position following DXF specs
* \param value IN position
*/

static void
SvgAppendPosition(DynString *output, int type, double value)
{
    //DynString formatted = NaS;
    //DynStringMalloc(&formatted, 0);
    //SvgFormatPosition(&formatted, type, value);
    //DynStringCatStr(output, &formatted);
    //DynStringFree(&formatted);
}

/**
* Build and format the line style definition. The result is appended to the existing result buffer.
*
* \param result OUT buffer for result
* \param type IN line style TRUE for dashed, FALSE for solid lines
*/

static void
SvgAppendLineStyle(DynString *output, int style)
{
    //DynString formatted = NaS;
    //DynStringMalloc(&formatted, 0);
    //SvgLineStyle(&formatted, style);
    //DynStringCatStr(output, &formatted);
    //DynStringFree(&formatted);
}

/**
 * Svg line command
 *
 * \param [in]	   svg the svg parent.
 * \param 		   x0  The x coordinate 0.
 * \param 		   y0  The y coordinate 0.
 * \param 		   x1  The first x value.
 * \param 		   y1  The first y value.
 * \param 		   w   A wDrawWidth to process.
 * \param 		   c   A wDrawColor to process.
 */

void
SvgLineCommand(SVGParent *svg, double x0,
               double y0, double x1, double y1, wDrawWidth w, wDrawColor c)
{
	mxml_node_t *xmlData;

	xmlData = mxmlNewElement(svg, "line");

	// line end points
	SvgAddCoordinate(xmlData, "x1", x0);
	SvgAddCoordinate(xmlData, "y1", y0); 
	SvgAddCoordinate(xmlData, "x2", x1); 
	SvgAddCoordinate(xmlData, "y2", y1);

	// line width
	SvgAddCoordinate(xmlData, "stroke-width", (double)w);

	// color
	SvgAddColor(xmlData, c);
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
SvgCircleCommand(DynString *result, int layer, double x,
                 double y, double r, int style)
{
    //DynStringCatCStr(result, SVG_INDENT "0\nCIRCLE\n");
    //SvgAppendPosition(result, 10, x);
    //SvgAppendPosition(result, 20, y);
    //SvgAppendPosition(result, 40, r);
    //SvgAppendLayerName(result, layer);
    //SvgAppendLineStyle(result, style);
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
SvgArcCommand(DynString *result, int layer, double x, double y,
              double r, double a0, double a1, int style)
{
    //DynStringCatCStr(result, SVG_INDENT "0\nARC\n");
    //SvgAppendPosition(result, 10, x);
    //SvgAppendPosition(result, 20, y);
    //SvgAppendPosition(result, 40, r);
    //SvgAppendPosition(result, 50, a0);
    //SvgAppendPosition(result, 51, a0+a1);
    //SvgAppendLayerName(result, layer);
    //SvgAppendLineStyle(result, style);
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
SvgTextCommand(DynString *result, int layer, double x,
               double y, double size, char *text)
{
    //DynStringCatCStr(result, SVG_INDENT "0\nTEXT\n");
    //DynStringCatCStrs(result, SVG_INDENT "1\n", text, "\n", NULL);
    //SvgAppendPosition(result, 10, x);
    //SvgAppendPosition(result, 20, y);
    //SvgAppendPosition(result, 40, size/72.0);
    //SvgAppendLayerName(result, layer);
}

/**
 * Append the header lines needed to define the measurement system. This includes the
 * definition of the measurement system (metric or English) vie the $MEASUREMENT variable
 * and the units i.e. inches for English and mm for metric.
 *
 * \PARAM result OUT buffer for the completed command
 */

void
SvgUnits(DynString *result)
{
    //char *value;
    //DynStringCatCStr(result, SVG_INDENT "9\n$MEASUREMENT\n  70\n");

    //if (units == 1) {
    //    value = "1\n";
    //} else {
    //    value = "0\n";
    //}

    //DynStringCatCStr(result, value);
    //DynStringCatCStr(result, SVG_INDENT "9\n$INSUNITS\n  70\n");

    //if (units == 1) {
    //    value = "4\n";
    //} else {
    //    value = "1\n";
    //}

    //DynStringCatCStr(result, value);
}


/**
 * Svg create document
 *
 * \returns An XMLDocument.
 */

SVGDocument *
SvgCreateDocument()
{
	return((SVGDocument *)mxmlNewXML("1.0"));
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

SVGParent *
SvgPrologue(SVGDocument *parent, int layerCount, double x0, double y0, double x1,
            double y1)
{
	mxml_node_t *xmlData;

	xmlData = mxmlNewElement(parent, 
							"!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\""  
							" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\"");
	xmlData = mxmlNewElement(parent, "svg");
	mxmlElementSetAttr(xmlData, "version", "1.1");
	SvgAddCoordinate(xmlData, "x", x0);
	SvgAddCoordinate(xmlData, "y", y0);
	SvgAddCoordinate(xmlData, "width", x1);
	SvgAddCoordinate(xmlData, "height", y1);

	return((SVGParent *)xmlData);
}

/**
* Create the file footer for a DXF file. Closes the open section and places
* an end-of-file marker
*
* \param result OUT buffer for the completed command
*/

void
SvgEpilogue(DynString *result)
{
    DynStringCatCStr(result, "</svg>\n");
}

const char *
whitespace_cb(mxml_node_t *node, int where)
{
	const char *element;

	/*
	 * We can conditionally break to a new line before or after
	 * any element.  These are just common HTML elements...
	 */

	element = mxmlGetElement(node);

	if (!strcmp(element, "svg") ||
		!strncmp(element, "!DOCTYPE", strlen("!DOCTYPE")))
	{
		/*
		 * Newlines before open and after close...
		 */

		if (where == MXML_WS_BEFORE_OPEN ||
			where == MXML_WS_AFTER_CLOSE)
			return ("\n");
	} 

	/*
	 * Otherwise return NULL for no added whitespace...
	 */

	return (NULL);
}

/**
 * Svg save file
 *
 * \param [in] svg	    the svg document.
 * \param [in] filename filename of the file.
 *
 * \returns True if it succeeds, false if it fails.
 */

bool
SvgSaveFile(SVGDocument *svg, char *filename)
{
	FILE *svgF;

	svgF = fopen(filename, "w");
	if (svgF) {

		mxmlSaveFile(svg, svgF, whitespace_cb);
		fclose(svgF);

		return(true);
	}
	return(false);
}
