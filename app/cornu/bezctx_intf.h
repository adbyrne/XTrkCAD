typedef struct _bezctx bezctx;

#include "common.h"
#include "wlib.h"

bezctx * new_bezctx_xtrkcad(dynArr_t *, BOOL_T, wDrawColor, DIST_T , int[2] );

void
bezctx_moveto(bezctx *bc, double x, double y, int is_open);

void
bezctx_lineto(bezctx *bc, double x, double y);

void
bezctx_quadto(bezctx *bc, double x1, double y1, double x2, double y2);

void
bezctx_curveto(bezctx *bc, double x1, double y1, double x2, double y2,
	       double x3, double y3);

void
bezctx_mark_knot(bezctx *bc, int knot_idx);
