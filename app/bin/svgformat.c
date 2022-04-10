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
*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
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
#include "include/utlist.h"
#include "common.h"

#define SVGDPIFACTOR 90.0					/**< the assumed resolution of the svg, 90 is what Inkscape uses */
#define ROUND2PIXEL( value ) ((int)floor(value * SVGDPIFACTOR + 0.5))

typedef struct sCssStyle {
    DynString name;
    DynString style;
    struct sCssStyle *next;
} sCssStyle;

static sCssStyle *styleCache = NULL;
static unsigned cacheCount;

static char *lineStyleCSS[] = {				/**< The css class names for line styles */
    NULL,										// no style needed for solid line
    "linedash",
    "linedot",
    "linedashdot",
    "linedashdot",
    "linecenter",
    "linephantom"
};

#define LINESTYLECLASSES \
    ".linedash{ stroke-dasharray: 25px 15px; } \n" \
    ".linedot{ stroke-dasharray: 5px 10px; } \n" \
    ".linedashdot{ stroke-dasharray: 25px 10px 5px 10px; } \n" \
    ".linedashdotdot{ stroke-dasharray: 25px 10px 5px 10px 5px 10px; } \n" \
    ".linecenter{ stroke-dasharray: 40px 15px 25px 15px; } \n" \
    ".linephantom{ stroke-dasharray: 40px 15px 25px 15px 25px 15px; } \n"


/**
 * Initialize style cache. Memory is allocated and the default style added
 */

static void
SvgInitStyleCache(void)
{
    sCssStyle *style;

    style = malloc(sizeof(sCssStyle));
    DynStringMalloc(&(style->name), 2);
    DynStringCatCStr(&(style->name), "*");

    DynStringMalloc(&(style->style), 16);
    DynStringCatCStr(&(style->style), "stroke-width:1; stroke:#000000; fill:none;");
    LL_APPEND(styleCache, style);

    cacheCount = 1;
}

static int
CompareStyle(sCssStyle *a, sCssStyle *b)
{
    return (strcmp(DynStringToCStr(&(a->style)), DynStringToCStr(&(b->style))));
}

/**
 * Add style to cache. If an identical style definition can be found in the
 * cache, the class name is returned.
 * If no previous definition is in the cache, a new one is contructed and
 * stored in the cache. The new class name is returned
 *
 * \param [in] styleDef style definition.
 *
 * \returns Null if default style can be used, else the class name
 */

static char *
SvgAddStyleToCache(DynString *styleDef)
{
    sCssStyle *style;
    sCssStyle *result;

    style = malloc(sizeof(sCssStyle));
    style->style = *styleDef;

    LL_SEARCH(styleCache, result, style, CompareStyle);
    if (result) {
        if (!strcmp(DynStringToCStr(&(result->name)), "*")) {
            return (NULL);
        } else {
            return (DynStringToCStr(&(result->name)) + 1);
        }
    } else {
        DynString className;
        DynStringMalloc(&className, 16);
        DynStringPrintf(&className, ".xtc%u", cacheCount++);
        style->name = className;
        LL_APPEND(styleCache, style);
        return (DynStringToCStr(&className) + 1);		//skip leading dot in class name
    }
}

/**
 * destroy style cache freeing all memory allocated
 */

static void
SvgDestroyStyleCache(void)
{
    sCssStyle *style;
    sCssStyle *tmp;

    LL_FOREACH_SAFE(styleCache, style, tmp) {
        DynStringFree(&(style->name));
        DynStringFree(&(style->style));
        free(style);
    }

    styleCache = NULL;
}

/**
 * Svg create style, add to the cache and the associated CSS class name to the element
 *
 * \param element	SVG element
 * \param colorRGB	RGB value
 * \param width     line width
 * \param fill		true to fill
 */

static void
SvgCreateStyle(mxml_node_t *element, unsigned long colorRGB, double width,
               bool fill, unsigned lineStyle)
{
    DynString style;
    char color[10];
    char *className = NULL;
    char *classLineStyle = NULL;

    CHECK(lineStyle < 7);

    sprintf(color, "#%2.2x%2.2x%2.2x", ((unsigned int)colorRGB >> 16) & 0xFF,
            ((unsigned int)colorRGB >> 8) & 0xFF, (unsigned int)colorRGB & 0xFF);

    DynStringMalloc(&style, 32);

    DynStringPrintf(&style,
                    "stroke-width:%d; stroke:%s; fill:%s;",
                    (int)(width + 0.5),
                    color,
                    (fill ? color: "none"));

    className = SvgAddStyleToCache(&style);
    classLineStyle = lineStyleCSS[lineStyle];

    if (className && classLineStyle) {
        mxmlElementSetAttrf(element, "class", "%s %s", className, classLineStyle);
    } else {
        if (className || classLineStyle) {
            mxmlElementSetAttr(element, "class", (className? className:classLineStyle));
        } else {
            // strict default, nothing to add
        }
    }
}

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
    mxmlElementSetAttrf(node, name, "%d", (int)(value+0.5));
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
 * Svg line command
 * \param [in] svg		 the svg parent.
 * \param 	   x0		 The x coordinate 0.
 * \param 	   y0		 The y coordinate 0.
 * \param 	   x1		 The first x value.
 * \param 	   y1		 The first y value.
 * \param 	   w		 A wDrawWidth to process.
 * \param 	   c		 RGB color definition.
 * \param 	   lineStyle line style.
 */

void
SvgLineCommand(SVGParent *svg, double x0,
               double y0, double x1, double y1, double w, long c, unsigned lineStyle)
{
    mxml_node_t *xmlData;

    xmlData = mxmlNewElement(svg, "line");

    // line end points
    SvgAddCoordinate(xmlData, "x1", x0);
    SvgAddCoordinate(xmlData, "y1", y0);
    SvgAddCoordinate(xmlData, "x2", x1);
    SvgAddCoordinate(xmlData, "y2", y1);

    SvgCreateStyle(xmlData, c, w, false, lineStyle);
}

/**
 * Svg rectangle command
 *
 * \param [in]    svg	   If non-null, the svg.
 * \param 		   x0	   The x coordinate 0.
 * \param 		   y0	   The y coordinate 0.
 * \param 		   x1	   The first x value.
 * \param 		   y1	   The first y value.
 * \param 		   color   The color.
 * \param 		   fill		Specifies the fill options.
 */

void
SvgRectCommand(SVGParent *svg, double x0, double y0, double x1, double y1,
               int color, unsigned lineStyle)
{
    mxml_node_t *xmlData;

    xmlData = mxmlNewElement(svg, "rect");

    // line end points
    SvgAddCoordinate(xmlData, "x1", x0);
    SvgAddCoordinate(xmlData, "y1", y0);
    SvgAddCoordinate(xmlData, "x2", x1);
    SvgAddCoordinate(xmlData, "y2", y1);

    SvgCreateStyle(xmlData, color, 1, false, lineStyle);

}

/**
 * Svg polygon line command
 *
 * \param [in] svg		 If non-null, the svg.
 * \param 	   cnt		 Number of point.
 * \param [in] points    If non-null, the points.
 * \param 	   color	 The line and fill color.
 * \param 	   width	 The line width.
 * \param 	   lineStyle The line style.
 * \param 	   fill		 True to fill.
 */

void
SvgPolyLineCommand(SVGParent *svg, int cnt, double *points, int color,
                   double width, bool fill, unsigned lineStyle)
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

    SvgCreateStyle(xmlData, color, width, fill, lineStyle);

    DynStringFree(&pos);
    DynStringFree(&pointList);
}

/**
 * Format a complete CIRCLE command
 *
 * \param [in]     svg		 OUT buffer for the completed command.
 * \param 		   x		 x position of center.
 * \param 		   y		 y position of center point.
 * \param 		   r		 radius.
 * \param 		   w		 width
 * \param 		   c		 color
 * \param 		   lineStyle The line style.
 * \param 		   fill		 True to fill.
 */

void
SvgCircleCommand(SVGParent *svg, double x,
                 double y, double r, double w, long c, bool fill, unsigned lineStyle)
{
    mxml_node_t *xmlData;

    xmlData = mxmlNewElement(svg, "circle");

    // line end points
    SvgAddCoordinate(xmlData, "cx", x);
    SvgAddCoordinate(xmlData, "cy", y);

    SvgAddCoordinate(xmlData, "r", r);

    SvgCreateStyle(xmlData, c, w, fill, lineStyle);

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
PolarToCartesian(double cx, double cy, double radius, double angle, double *px,
                 double *py)
{
    double angleInRadians = ((angle) * M_PI) / 180.0;

    *px = cx + (radius * cos(angleInRadians));
    *py = cy + (radius * sin(angleInRadians));
}

/**
 * Format an arc as a SVG path See
 * https://stackoverflow.com/questions/5736398/how-to-calculate-the-svg-path-for-an-arc-of-a-circle
 *
 * \param [in]     svg		 the svg document.
 * \param 		   x		 y IN center point.
 * \param 		   y		 The y coordinate.
 * \param 		   r		 IN radius.
 * \param 		   a0		 IN starting angle.
 * \param 		   a1		 IN ending angle.
 * \param 		   center    IN draw center mark if true.
 * \param 		   w		 line width.
 * \param 		   c		 line color.
 * \param 		   lineStyle line style.
 */

void
SvgArcCommand(SVGParent *svg, double x, double y,
              double r, double a0, double a1, bool center, double w, long c,
              unsigned lineStyle)
{
    double startX;
    double startY;
    double endX;
    double endY;
    char largeArcFlag = (a1 - a0 <= 180 ? '0' : '1');
    DynString pathArc;
    mxml_node_t *xmlData;

    xmlData = mxmlNewElement(svg, "path");

    PolarToCartesian(x, y, r, a0+a1-90, &startX, &startY);
    PolarToCartesian(x, y, r, a0-90, &endX, &endY);

    DynStringMalloc(&pathArc, 64);
    DynStringPrintf(&pathArc,
                    "M %d %d A %d %d 0 %c 0 %d %d",
                    ROUND2PIXEL(startX),
                    ROUND2PIXEL(startY),
                    ROUND2PIXEL(r),
                    ROUND2PIXEL(r),
                    largeArcFlag,
                    ROUND2PIXEL(endX),
                    ROUND2PIXEL(endY));

    mxmlElementSetAttr(xmlData, "d", DynStringToCStr(&pathArc));

    DynStringFree(&pathArc);

    SvgCreateStyle(xmlData, c, w, false, lineStyle);
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

    SvgCreateStyle(xmlData, c, 1, 1, 0);

    SvgAddRealUnit(xmlData, "font-size", size);

    mxmlNewText(xmlData, false, text);
}

/**
 * Add title to SVG document
 *
 * \param [in] svg   svg
 * \param [in] title If non-null, the title.
 */

void
SvgAddTitle(SVGParent *svg, char *title)
{
    mxml_node_t *titleNode;
    if (title) {
        titleNode = mxmlNewElement(MXML_NO_PARENT, "title");
        mxmlNewText(titleNode, false, title);

        mxmlAdd(svg, MXML_ADD_BEFORE, MXML_ADD_TO_PARENT, titleNode);
    }
}

/**
 * Add CSS style definitions to the SVG file. CSS definitions are
 * created from the options of the drawing commands. As a final step
 * in creation of the SVG file, these definitions have to be added.
 * For compatibility reasons the styles have to be defined before
 * first use.
 *
 * \param [in] svg the svg.
 */

void
SvgAddCSSStyle(SVGParent *svg)
{
    mxml_node_t *cssNode;
    DynString cssDefs;
    DynString tmp;
    sCssStyle *style;

    cssNode = mxmlNewElement(MXML_NO_PARENT, "style");
    mxmlElementSetAttr(cssNode, "type", "text/css");

    DynStringMalloc(&cssDefs, 64);
    DynStringMalloc(&tmp, 64);
    LL_FOREACH(styleCache, style) {
        DynStringPrintf(&tmp, "%s { %s }\n",
                        DynStringToCStr(&(style->name)),
                        DynStringToCStr(&(style->style)));

        DynStringCatStr(&cssDefs, &tmp);
    }

    DynStringCatCStr(&cssDefs, LINESTYLECLASSES);
    mxmlNewCDATA(cssNode, DynStringToCStr(&cssDefs));

    mxmlAdd(svg, MXML_ADD_BEFORE, MXML_ADD_TO_PARENT, cssNode);
    DynStringFree(&tmp);
    DynStringFree(&cssDefs);
}

/**
 * Svg create document
 *
 * \returns An XMLDocument.
 */

SVGDocument *
SvgCreateDocument()
{
    SvgInitStyleCache();

    return ((SVGDocument *)mxmlNewXML("1.0"));
}

/**
 * Svg destroy document freeing the memory used by the XML tree
 *
 * \param [in] xml the XML document
 */

void
SvgDestroyDocument(SVGDocument *xml)
{
    mxmlDelete((mxml_node_t *)xml);

    SvgDestroyStyleCache();
}

/**
 * Create the complete prologue for a SVG file.
 *
 * \param [in]     parent	  the document handle.
 * \param [in]     id		  If non-null, the identifier.
 * \param 		   layerCount IN count of defined layers.
 * \param 		   x0		  y0 IN minimum (left bottom) position.
 * \param 		   y0		  y1 IN maximum (top right) position.
 * \param 		   x1		  The first x value.
 * \param 		   y1		  The first y value.
 *
 * \returns Null if it fails, else a pointer to a SVGParent.
 */

SVGParent *
SvgPrologue(SVGDocument *parent, char *id, int layerCount, double x0, double y0,
            double x1,
            double y1)
{
    mxml_node_t *xmlData;

    xmlData = mxmlNewElement(parent,
                             "!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\""
                             " \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\"");
    xmlData = mxmlNewElement(parent, "svg");
    mxmlElementSetAttr(xmlData, "xmlns", "http://www.w3.org/2000/svg");

    if (id) {
        mxmlElementSetAttr(xmlData, "id", id);
    }
    SvgAddCoordinate(xmlData, "x", x0);
    SvgAddCoordinate(xmlData, "y", y0);
    SvgAddCoordinate(xmlData, "width", x1);
    SvgAddCoordinate(xmlData, "height", y1);

    return ((SVGParent *)xmlData);
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
            !strncmp(element, "!DOCTYPE", strlen("!DOCTYPE"))) {
        /*
         * Newlines before open and after close...
         */

        if (where == MXML_WS_BEFORE_OPEN ||
                where == MXML_WS_BEFORE_CLOSE) {
            return ("\n");
        }
    } else {
        if (!strcmp(element, "line") ||
                !strcmp(element, "circle") ||
                !strcmp(element, "path") ||
                !strcmp(element, "polyline")) {
            if (where == MXML_WS_BEFORE_OPEN ||
                    where == MXML_WS_AFTER_CLOSE) {
                return ("\n\t");
            }
        } else {
            if (!strcmp(element, "style") ||
                    !strcmp(element, "title") ||
                    !strcmp(element, "text")) {
                if (where == MXML_WS_BEFORE_OPEN) {
                    return ("\n\t");
                } else {
                    if (where == MXML_WS_AFTER_OPEN) {
                        return ("\n\t\t");
                    } else {
                        if (where == MXML_WS_AFTER_CLOSE) {
                            return ("");
                        } else {
                            return ("\n\t");
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

        return (true);
    }
    return (false);
}
