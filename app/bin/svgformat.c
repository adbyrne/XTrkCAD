/** \file svgformat.c
* Formatting of SVG commands and parameters. 
*/

/*  XTrkCad - Model Railroad CAD
*  Copyright (C)2021 Martin Fischer
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

#define _USE_MATH_DEFINES 
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "dynstring.h"
#include "mxml.h"
#include "include/svgformat.h"

#define SVGDPIFACTOR 90.0				   /**< the assumed resolution of the svg, 90 is what Inkscape uses */
#define ROUND2PIXEL( value ) ((int)floor(value * SVGDPIFACTOR + 0.5))

/**
 * Hashes the given string, taken from http://www.cse.yorku.ca/~oz/hash.html
 *
 * \param str the string to be hashed
 *
 * \returns the hash
 */

//static unsigned long
//hash(unsigned char *str)
//{
//	unsigned long hash = 5381;
//	int c;
//
//	while (c = *str++)
//		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
//
//	return hash;
//}

/**
 * Utility macros to set colors
 *
 * \param  node  The node.
 * \param  color The color.
 */

#define SvgLineColor(node, color)	SvgAddColor(node, "stroke", color )
#define SvgFillColor(node, color)	SvgAddColor(node, "fill", color )

/**
 * add real unit, ie. units that are specified in pixels. Rounding is performed 
 *
 * \param [in,out] node  If non-null, the node.
 * \param [in,out] name  If non-null, the name.
 * \param 		   value the dimension in pixels
 */

static void
SvgAddRealUnit(mxml_node_t *node, char *name, double value)
{
	mxmlElementSetAttrf(node, name, "%d",  (int)(value+0.5));
}

/**
* Format a dimension and add to XML node as an attribute. 
* A fictional value for the resolution is assumed. As final 
* rendering is done by the client, this is not really relevant. 
*
* \PARAM [in, out]	node	the XML node
* \PARAM [in]		name	name of attribute
* \param [in]		value	size		 
*/

static void
SvgAddCoordinate(mxml_node_t *node, char *name, double value)
{
	mxmlElementSetAttrf(node, name, "%d", ROUND2PIXEL(value));
}

/**
 * Svg add color
 *
 * \param [in,out] node  If non-null, the node.
 * \param 		   color color in 8 bit RGB format.
 */

static void 
SvgAddColor(mxml_node_t *node, char *attr, long colorRBG)
{
	mxmlElementSetAttrf(node, attr, "#%02.2x%02.2x%02.2x", (colorRBG >> 16) & 0xFF, (colorRBG >> 8) & 0xFF, colorRBG & 0xFF);
}

static void
SvgFillNone(mxml_node_t *node)
{
	mxmlElementSetAttr(node, "fill", "none");
}


/**
 * Build and format layer name. The name is created by appending the layer number to the basic
 * layer name.
 *
 * \param [out]	   result OUT buffer for result.
 * \param [in,out] name   IN base part of  name.
 * \param 		   layer  IN layer number.
 */

void SvgLayerName(mxml_node_t *result, char *name, int layer)
{

}

/**
* Build and format the line style definition
*
* \param result OUT buffer for result
* \param type IN line style TRUE for dashed, FALSE for solid lines
*/

void SvgLineStyle(mxml_node_t *result, int isDashed)
{

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
SvgAppendLayerName(mxml_node_t *result, int layer)
{

}

/**
* Build and format the line style definition. The result is appended to the existing result buffer.
*
* \param result OUT buffer for result
* \param type IN line style TRUE for dashed, FALSE for solid lines
*/

static void
SvgAppendLineStyle(SVGParent *output, int style)
{

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
 * \param 		   c   RGB color definition
 */

void
SvgLineCommand(SVGParent *svg, double x0,
               double y0, double x1, double y1, double w, long c)
{
	mxml_node_t *xmlData;

	xmlData = mxmlNewElement(svg, "line");

	// line end points
	SvgAddCoordinate(xmlData, "x1", x0);
	SvgAddCoordinate(xmlData, "y1", y0); 
	SvgAddCoordinate(xmlData, "x2", x1); 
	SvgAddCoordinate(xmlData, "y2", y1);

	SvgAddRealUnit(xmlData, "stroke-width", w);

	// color
	SvgLineColor(xmlData, c );
}

/**
 * Svg rectangle command
 *
 * \param [in,out] svg	   If non-null, the svg.
 * \param 		   x0	   The x coordinate 0.
 * \param 		   y0	   The y coordinate 0.
 * \param 		   x1	   The first x value.
 * \param 		   y1	   The first y value.
 * \param 		   color   The color.
 * \param 		   fill		Specifies the fill options.
 */

void
SvgRectCommand(SVGParent *svg, double x0, double y0, double x1, double y1, int color, int linestyle)
{
	mxml_node_t *xmlData;

	xmlData = mxmlNewElement(svg, "rect");

	// line end points
	SvgAddCoordinate(xmlData, "x1", x0);
	SvgAddCoordinate(xmlData, "y1", y0);
	SvgAddCoordinate(xmlData, "x2", x1);
	SvgAddCoordinate(xmlData, "y2", y1);

	SvgFillColor(xmlData, color );
}

/**
 * Svg polygon line command
 *
 * \param [in]	svg    If non-null, the svg.
 * \param 		cnt    Number of point.
 * \param [in]	points If non-null, the points.
 * \param 		color  The line and fill color.
 * \param 		width  The line width.
 * \param 		fill   True to fill.
 */

void
SvgPolyLineCommand(SVGParent *svg, int cnt, double *points, int color, double width, bool fill)
{
	mxml_node_t *xmlData;
	DynString pointList;
	DynString pos;

	DynStringMalloc(&pointList, 64);
	DynStringMalloc(&pos, 20);


	for (int i = 0; i < cnt; i++) {
		DynStringPrintf(&pos,
						"%d,%d ", 
						(int)floor(points[i * 2] * SVGDPIFACTOR + 0.5), 
						(int)floor(points[ i * 2 + 1] * SVGDPIFACTOR + 0.5));

		DynStringCatStr(&pointList, &pos);
		DynStringClear(&pos);
	}

	xmlData = mxmlNewElement(svg, "polyline");
	mxmlElementSetAttr(xmlData, "points", DynStringToCStr(&pointList));

	SvgAddRealUnit(xmlData, "stroke-width", width);

	SvgLineColor(xmlData, color);
	if (fill) {
		SvgFillColor(xmlData, color);
	} else {
		SvgFillNone(xmlData);
	}
	DynStringFree(&pos);
	DynStringFree(&pointList);
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
SvgCircleCommand(SVGParent *svg, double x,
                 double y, double r, double w, long c, bool fill)
{
	mxml_node_t *xmlData;

	xmlData = mxmlNewElement(svg, "circle");

	// line end points
	SvgAddCoordinate(xmlData, "cx", x);
	SvgAddCoordinate(xmlData, "cy", y);

	SvgAddCoordinate(xmlData, "r", r);

	// color
	SvgLineColor(xmlData, c);
	if (fill) {
		SvgFillColor(xmlData, c);
	} else {
		SvgFillNone(xmlData);
	}
}

/**
 * Polar to cartesian
 *
 * \param 		cx	  x coordinate of center.
 * \param 		cy	  y coordinate of center
 * \param 		radius radius.
 * \param 		angle  angle.
 * \param [out] px	  resulting x coordinate
 * \param [out] py	  resulting y coordinate
 */

static void
PolarToCartesian(double cx, double cy, double radius, double angle, double *px, double *py )
{
	double angleInRadians = ((angle ) * M_PI) / 180.0;

	*px = cx + (radius * cos(angleInRadians));
	*py = cy + (radius * sin(angleInRadians));
}

/**
* Format an arc as a SVG path
* See https://stackoverflow.com/questions/5736398/how-to-calculate-the-svg-path-for-an-arc-of-a-circle
*
* \param svg the svg document
* \param x, y IN center point
* \param r IN radius
* \param a0 IN starting angle
* \param a1 IN ending angle
* \param center IN draw center mark if true
* \param w line width							
* \param c line color 
*/

void
SvgArcCommand(SVGParent *svg, double x, double y,
              double r, double a0, double a1, bool center, double w, long c)
{
	double startX;
	double startY;
	double endX;
	double endY;
	char largeArcFlag = (a1 - a0 <= 180 ? '0' : '1');
	DynString pathArc;
	mxml_node_t *xmlData;

	xmlData = mxmlNewElement(svg, "path");

	PolarToCartesian(x, y, r, a0+a1-90, &startX, &startY );
	PolarToCartesian(x, y, r, a0-90, &endX, &endY );

	DynStringMalloc( &pathArc, 64);
	DynStringPrintf( &pathArc, 
					"M %d %d A %d %d 0 %c 0 %d %d", 
					ROUND2PIXEL(startX), 
					ROUND2PIXEL(startY), 
					ROUND2PIXEL(r), 
					ROUND2PIXEL(r), 
					largeArcFlag, 
					ROUND2PIXEL(endX), 
					ROUND2PIXEL(endY));

	mxmlElementSetAttr(xmlData, "d", DynStringToCStr( &pathArc));

	DynStringFree(&pathArc);

	SvgLineColor(xmlData, c);
	SvgFillNone(xmlData);
	SvgAddRealUnit(xmlData, "stroke-width", w);
}

/**
 *  Create SVG text command 
 *
 * \param [in] svg  If non-null, the svg.
 * \param 		   x    The x coordinate.
 * \param 		   y    The y coordinate.
 * \param 		   size The fontsize.
 * \param 		   c    the text color
 * \param [in]	   text text in UTF-8 format
 */

void
SvgTextCommand(SVGParent *svg, double x,
               double y, double size, long c, char *text)
{
	mxml_node_t *xmlData;

	xmlData = mxmlNewElement(svg, "text");
	// starting point
	SvgAddCoordinate(xmlData, "x", x);
	SvgAddCoordinate(xmlData, "y", y);

	SvgFillColor(xmlData, c);
	SvgLineColor(xmlData, c);

	SvgAddRealUnit(xmlData, "font-size", size);

	mxmlNewText(xmlData, false, text);
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
 * Svg destroy document freeing the memory used by the XML tree
 *
 * \param [in,out] xml If non-null, the XML.
 */

void
SvgDestroyDocument(SVGDocument *xml)
{
	mxmlDelete((mxml_node_t *)xml);
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
	mxmlElementSetAttr(xmlData, "xmlns", "http://www.w3.org/2000/svg");
	SvgAddCoordinate(xmlData, "x", x0);
	SvgAddCoordinate(xmlData, "y", y0);
	SvgAddCoordinate(xmlData, "width", x1);
	SvgAddCoordinate(xmlData, "height", y1);

	return((SVGParent *)xmlData);
}

/**
* Create the file footer for a SVG file.
*
* \param result 
*/

void
SvgEpilogue(SVGParent *result)
{
    
}

/**
 * Add formatting to the resulting document by adding whitespace
 *
 * \param  node to be formatted
 * \param  see minixml docu, position in XML tag
 *
 * \returns Null if it no character to add, else a pointer to the additional chars.
 */

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
			where == MXML_WS_BEFORE_CLOSE)
			return ("\n");
	} else {
		if (!strcmp(element, "line") ||
			!strcmp(element, "circle") ||
			!strcmp(element, "path") ||
			!strcmp(element, "polyline"))
		{
			if (where == MXML_WS_BEFORE_OPEN ||
				where == MXML_WS_AFTER_CLOSE) {
				return("\n\t");
			}
		} else {
			if (!strcmp(element, "text")) {
				if (where == MXML_WS_BEFORE_OPEN) {
					return("\n\t");
				} else {
					if (where == MXML_WS_AFTER_OPEN) {
						return("\n\t\t");
					} else {
						if (where == MXML_WS_AFTER_CLOSE) {
							return("\n");
						} else {
							return("\n\t");
						}
					}
				}

			}
		}
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
		mxmlSetWrapMargin(0);
		mxmlSaveFile(svg, svgF, whitespace_cb);
		fclose(svgF);

		return(true);
	}
	return(false);
}
