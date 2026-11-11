/* Stubs for the DIRECTCAIRO branch (wBasicDraw*).
 *
 * This file deliberately does NOT include wlib.h. The stubs never read their
 * arguments, they only count the call. That keeps them independent of the
 * exact prototypes (parameter types of MINLINEWIDTHBITMAP etc.). With the
 * usual calling conventions (caller cleans up the arguments) this is safe.
 * Do not use this approach with Windows/stdcall. */

#include "stubs.h"

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#pragma GCC diagnostic ignored "-Wold-style-definition"
#endif

void wBasicDrawLine()          { rec.n_basic_line++; }
void wBasicDrawArc()           { rec.n_basic_arc++; }
void wBasicDrawString()        { rec.n_basic_string++; }
void wBasicDrawFillRectangle() { rec.n_basic_fillrect++; }
void wBasicDrawFillPolygon()   { rec.n_basic_fillpoly++; }
void wBasicDrawFillCircle()    { rec.n_basic_fillcircle++; }
