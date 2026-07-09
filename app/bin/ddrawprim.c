/** \file ddrawprim.c
 * Low-level drawing primitives.
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

#include "draw.h"
#include "common-ui.h"
#include "layout.h"
#include "mapwindow.h"
#include "misc.h"
#include "track.h"

/* Accessor functions for border widths, defined in draw.c */
extern int GetLBorder(void);
extern int GetBBorder(void);

/* Forward declaration needed by DDrawString */
static void DDrawPoly(drawCmd_p d, int cnt, coOrd *pts, int *types,
                      wDrawColor color, wDrawWidth width, drawFill_e eFillOpt);

EXPORT long maxArcSegStraightLen = 100;
static wFontSize_t drawMaxTextFontSize = 100;

/****************************************************************************
 *
 * COORDINATE CONVERSION
 *
 */

static void MainCoOrd2Pix(drawCmd_p d, coOrd p, wDrawPix_t *x, wDrawPix_t *y)
{
	DIST_T t;
	int lborder = GetLBorder();
	int bborder = GetBBorder();
	if (d->angle != 0.0) {
		Rotate(&p, d->orig, -d->angle);
	}
	p.x = (p.x - d->orig.x) / d->scale;
	p.y = (p.y - d->orig.y) / d->scale;
	t = p.x * d->dpi;
	if (t > 0.0) {
		t += 0.5;
	} else {
		t -= 0.5;
	}
	*x = ((wDrawPix_t)t) + ((d->options & DC_TICKS) ? lborder : 0);
	t = p.y * d->dpi;
	if (t > 0.0) {
		t += 0.5;
	} else {
		t -= 0.5;
	}
	*y = ((wDrawPix_t)t) + ((d->options & DC_TICKS) ? bborder : 0);
}

static int Pix2CoOrd_interpolate = 0;

static void MainPix2CoOrd(drawCmd_p d, wDrawPix_t px, wDrawPix_t py,
                          coOrd *posR)
{
	DIST_T x, y;
	DIST_T bins = pixelBins;
	int lborder = GetLBorder();
	int bborder = GetBBorder();
	x = ((((POS_T)((px)-lborder)) / d->dpi)) * d->scale;
	y = ((((POS_T)((py)-bborder)) / d->dpi)) * d->scale;
	x = (long)(x * bins) / bins;
	y = (long)(y * bins) / bins;
	if (Pix2CoOrd_interpolate) {
		DIST_T x1, y1;
		x1 = ((((POS_T)((px - 1) - lborder)) / d->dpi)) * d->scale;
		y1 = ((((POS_T)((py - 1) - bborder)) / d->dpi)) * d->scale;
		x1 = (long)(x1 * bins) / bins;
		y1 = (long)(y1 * bins) / bins;
		if (x == x1) {
			x += 1 / bins / 2;
			printf("px=%0.1f x1=%0.6f x=%0.6f\n", px, x1, x);
		}
		if (y == y1) {
			y += 1 / bins / 2;
		}
	}
	x += d->orig.x;
	y += d->orig.y;
	posR->x = x;
	posR->y = y;
}

#define DRAWOPTS(D)                                                            \
  (((D->options & DC_TEMP) ? wDrawOptTemp : 0) |                               \
   ((D->options & DC_OUTLINE) ? wDrawOutlineFont : 0))

/****************************************************************************
 *
 * SCREEN / PRINT DRAWING PRIMITIVES
 *
 */

static void DDrawLine(drawCmd_p d, coOrd p0, coOrd p1, wDrawWidth width,
                      wDrawColor color)
{
	wDrawPix_t x0, y0, x1, y1;
	BOOL_T in0 = FALSE, in1 = FALSE;
	coOrd orig, size;
	int lborder = GetLBorder();
	int bborder = GetBBorder();
	if (d == &mapD && !mapVisible) {
		return;
	}
	if ((d->options & DC_NOCLIP) == 0) {
		if (d->angle == 0.0) {
			in0 = (p0.x >= d->orig.x && p0.x <= d->orig.x + d->size.x &&
			       p0.y >= d->orig.y && p0.y <= d->orig.y + d->size.y);
			in1 = (p1.x >= d->orig.x && p1.x <= d->orig.x + d->size.x &&
			       p1.y >= d->orig.y && p1.y <= d->orig.y + d->size.y);
		}
		if ((!in0) || (!in1)) {
			orig = d->orig;
			size = d->size;
			if (d->options & DC_TICKS) {
				orig.x -= lborder / d->dpi * d->scale;
				orig.y -= bborder / d->dpi * d->scale;
				size.x += (lborder + RBORDER) / d->dpi * d->scale;
				size.y += (bborder + TBORDER) / d->dpi * d->scale;
			}
			if (!ClipLine(&p0, &p1, orig, d->angle, size)) {
				return;
			}
		}
	}
	d->CoOrd2Pix(d, p0, &x0, &y0);
	d->CoOrd2Pix(d, p1, &x1, &y1);
	drawCount++;
	wDrawLineType_e lineOpt = wDrawLineSolid;
	unsigned long NotSolid = DC_NOTSOLIDLINE;
	unsigned long opt = d->options & NotSolid;
	if (opt == DC_DASH) {
		lineOpt = wDrawLineDash;
	} else if (opt == DC_DOT) {
		lineOpt = wDrawLineDot;
	} else if (opt == DC_DASHDOT) {
		lineOpt = wDrawLineDashDot;
	} else if (opt == DC_DASHDOTDOT) {
		lineOpt = wDrawLineDashDotDot;
	} else if (opt == DC_CENTER) {
		lineOpt = wDrawLineCenter;
	} else if (opt == DC_PHANTOM) {
		lineOpt = wDrawLinePhantom;
	}

	if (drawEnable) {
		wDrawLine(d->d, x0, y0, x1, y1, width, lineOpt, color, DRAWOPTS(d));
	}
}

static void DDrawArc(drawCmd_p d, coOrd p, DIST_T r, ANGLE_T angle0,
                     ANGLE_T angle1, BOOL_T drawCenter, wDrawWidth width,
                     wDrawColor color)
{
	wDrawPix_t x, y;
	ANGLE_T da;
	coOrd p0, p1;
	DIST_T rr;
	int i, cnt;

	if (d == &mapD && !mapVisible) {
		return;
	}
	rr = (r / d->scale) * d->dpi + 0.5;
	if (rr > wDrawGetMaxRadius(d->d)) {
		da = (maxArcSegStraightLen * 180) / (M_PI * rr);
		cnt = (int)(angle1 / da) + 1;
		da = angle1 / cnt;
		coOrd min, max;
		min = d->orig;
		max.x = min.x + d->size.x;
		max.y = min.y + d->size.y;
		PointOnCircle(&p0, p, r, angle0);
		for (i = 1; i <= cnt; i++) {
			angle0 += da;
			PointOnCircle(&p1, p, r, angle0);
			if (d->angle == 0.0 &&
			    ((p0.x >= min.x && p0.x <= max.x && p0.y >= min.y && p0.y <= max.y) ||
			     (p1.x >= min.x && p1.x <= max.x && p1.y >= min.y &&
			      p1.y <= max.y))) {
				DrawLine(d, p0, p1, width, color);
			} else {
				coOrd clip0 = p0, clip1 = p1;
				if (ClipLine(&clip0, &clip1, d->orig, d->angle, d->size)) {
					DrawLine(d, clip0, clip1, width, color);
				}
			}

			p0 = p1;
		}
		return;
	}
	if (d->angle != 0.0 && angle1 < 360.0) {
		angle0 = NormalizeAngle(angle0 - d->angle);
	}
	d->CoOrd2Pix(d, p, &x, &y);
	drawCount++;
	wDrawLineType_e lineOpt = wDrawLineSolid;
	unsigned long NotSolid = DC_NOTSOLIDLINE;
	unsigned long opt = d->options & NotSolid;
	if (opt == DC_DASH) {
		lineOpt = wDrawLineDash;
	} else if (opt == DC_DOT) {
		lineOpt = wDrawLineDot;
	} else if (opt == DC_DASHDOT) {
		lineOpt = wDrawLineDashDot;
	} else if (opt == DC_DASHDOTDOT) {
		lineOpt = wDrawLineDashDotDot;
	} else if (opt == DC_CENTER) {
		lineOpt = wDrawLineCenter;
	} else if (opt == DC_PHANTOM) {
		lineOpt = wDrawLinePhantom;
	}
	if (drawEnable) {
		int sizeCenter =
		        (int)(drawCenter ? ((d->options & DC_PRINT) ? (d->dpi / BASE_DPI) : 1)
		              : 0);
		wDrawArc(d->d, x, y, (wDrawPix_t)(rr), angle0, angle1, sizeCenter, width,
		         lineOpt, color, DRAWOPTS(d));
	}
}

static void DDrawString(drawCmd_p d, coOrd p, ANGLE_T a, char *s, wFont_p fp,
                        FONTSIZE_T fontSize, wDrawColor color)
{
	wDrawPix_t x, y;
	if (d == &mapD && !mapVisible) {
		return;
	}
	d->CoOrd2Pix(d, p, &x, &y);
	if (color == wDrawColorWhite) {
		wDrawPix_t width, height, descent, ascent;
		coOrd pos[4], size;
		double scale = 1.0;
		wDrawGetTextSize(&width, &height, &descent, &ascent, d->d, s, fp, fontSize);
		pos[0] = p;
		size.x = SCALEX(mainD, width) * scale;
		size.y = SCALEY(mainD, height) * scale;
		pos[1].x = p.x + size.x;
		pos[1].y = p.y;
		pos[2].x = p.x + size.x;
		pos[2].y = p.y + size.y;
		pos[3].x = p.x;
		pos[3].y = p.y + size.y;
		Rotate(&pos[1], pos[0], a);
		Rotate(&pos[2], pos[0], a);
		Rotate(&pos[3], pos[0], a);
		DDrawPoly(d, 4, pos, NULL, color, 0, DRAW_FILL);
	} else {
		fontSize /= d->scale;
		wDrawString(d->d, x, y, d->angle - a, s, fp, fontSize, color, DRAWOPTS(d));
	}
}

static void DDrawPoly(drawCmd_p d, int cnt, coOrd *pts, int *types,
                      wDrawColor color, wDrawWidth width, drawFill_e eFillOpt)
{
	typedef wDrawPix_t wPos2[2];
	static dynArr_t wpts_da;
	static dynArr_t wpts_type_da;
	int inx;
	int fill = 0;
	int open = 0;
	wDrawPix_t x, y;
	DYNARR_SET(wPos2, wpts_da, cnt * 2);
	DYNARR_SET(int, wpts_type_da, cnt);
#define wpts(N) DYNARR_N(wPos2, wpts_da, N)
#define wtype(N) DYNARR_N(wPolyLine_e, wpts_type_da, N)
	for (inx = 0; inx < cnt; inx++) {
		d->CoOrd2Pix(d, pts[inx], &x, &y);
		wpts(inx)[0] = x;
		wpts(inx)[1] = y;
		if (!types) {
			wtype(inx) = 0;
		} else {
			wtype(inx) = (wPolyLine_e)types[inx];
		}
	}
	wDrawLineType_e lineOpt = wDrawLineSolid;
	unsigned long NotSolid = DC_NOTSOLIDLINE;
	unsigned long opt = d->options & NotSolid;
	if (opt == DC_DASH) {
		lineOpt = wDrawLineDash;
	} else if (opt == DC_DOT) {
		lineOpt = wDrawLineDot;
	} else if (opt == DC_DASHDOT) {
		lineOpt = wDrawLineDashDot;
	} else if (opt == DC_DASHDOTDOT) {
		lineOpt = wDrawLineDashDotDot;
	} else if (opt == DC_CENTER) {
		lineOpt = wDrawLineCenter;
	} else if (opt == DC_PHANTOM) {
		lineOpt = wDrawLinePhantom;
	}

	wDrawOpts drawOpts = DRAWOPTS(d);
	switch (eFillOpt) {
	case DRAW_OPEN:
		open = 1;
		break;
	case DRAW_CLOSED:
		break;
	case DRAW_FILL:
		fill = 1;
		break;
	case DRAW_TRANSPARENT:
		fill = 1;
		drawOpts |= wDrawOptTransparent;
		break;
	default:
		CHECK(FALSE);
	}
	wDrawPolygon(d->d, &wpts(0), &wtype(0), cnt, color, width, lineOpt, drawOpts,
	             fill, open);
}

static void DDrawFillCircle(drawCmd_p d, coOrd p, DIST_T r, wDrawColor color)
{
	wDrawPix_t x, y;
	DIST_T rr;
	int lborder = GetLBorder();
	int bborder = GetBBorder();

	if (d == &mapD && !mapVisible) {
		return;
	}
	rr = (r / d->scale) * d->dpi + 0.5;
	if (rr > wDrawGetMaxRadius(d->d)) {
		/* Circle too big */
		return;
	}
	d->CoOrd2Pix(d, p, &x, &y);
	wWinPix_t w, h;
	wDrawGetSize(d->d, &w, &h);
	if (d->options & DC_TICKS) {
		if (x + rr < lborder || x - rr > w - RBORDER || y + rr < bborder ||
		    y - rr > h - TBORDER) {
			return;
		}
	} else {
		if (x + rr < 0 || x - rr > w || y + rr < 0 || y - rr > h) {
			return;
		}
	}
	drawCount++;
	if (drawEnable) {
		wDrawFilledCircle(d->d, x, y, (wDrawPix_t)(rr), color, DRAWOPTS(d));
	}
}

static void DDrawRectangle(drawCmd_p d, coOrd orig, coOrd size,
                           wDrawColor color, drawFill_e eFillOpt)
{
	wDrawPix_t x, y, w, h;

	if (d == &mapD && !mapVisible) {
		return;
	}
	d->CoOrd2Pix(d, orig, &x, &y);
	w = (wDrawPix_t)((size.x / d->scale) * d->dpi + 0.5);
	h = (wDrawPix_t)((size.y / d->scale) * d->dpi + 0.5);
	drawCount++;
	if (drawEnable) {
		wDrawOpts opts = DRAWOPTS(d);
		coOrd p1, p2;
		switch (eFillOpt) {
		case DRAW_CLOSED:
			/* 1 2 */
			/* 0 3 */
			p1.x = orig.x;
			p1.y = orig.y + size.y;
			DrawLine(d, orig, p1, 0, color);
			p2.x = orig.x + size.x;
			p2.y = p1.y;
			DrawLine(d, p1, p2, 0, color);
			p1.x = p2.x;
			p1.y = orig.y;
			DrawLine(d, p2, p1, 0, color);
			DrawLine(d, p1, orig, 0, color);
			break;
		case DRAW_TRANSPARENT:
			opts |= wDrawOptTransparent;
		/* Fallthru */
		case DRAW_FILL:
			if (d->options & DC_ROUND) {
				x = round(x);
				y = round(y);
			}
			wDrawFilledRectangle(d->d, x, y, w, h, color, opts);
			break;
		default:
			CHECK(FALSE);
		}
	}
}

/****************************************************************************
 *
 * EXPORTED HIGHLIGHT / COMPOSITE DRAWING FUNCTIONS
 *
 */

EXPORT void DrawHilight(drawCmd_p d, coOrd p, coOrd s, BOOL_T add)
{
	unsigned long options = d->options;
	d->options |= DC_TEMP;
	wBool_t bTemp = wDrawSetTempMode(d->d, TRUE);
	DrawRectangle(d, p, s, add ? drawColorPowderedBlue : selectedColor,
	              DRAW_TRANSPARENT);
	wDrawSetTempMode(d->d, bTemp);
	d->options = options;
}

EXPORT void DrawHilightPolygon(drawCmd_p d, coOrd *p, int cnt)
{
	CHECK(cnt <= 4);
	static wDrawColor color = 0;
	if (color == 0) {
		color = wDrawColorGray(70);
	}
	DrawPoly(d, cnt, p, NULL, color, 0, DRAW_TRANSPARENT);
}

EXPORT void DrawMultiString(drawCmd_p d, coOrd pos, char *text, wFont_p fp,
                            wFontSize_t fs, wDrawColor color, BOOL_T boxed,
                            BOOL_T filled, wDrawColor bg_color, ANGLE_T a,
                            coOrd *lo, coOrd *hi)
{
	char *cp;
	char *cp1;
	POS_T lineH;
	coOrd size, size2, posl, orig;
	POS_T descent, ascent;
	char *line;
	coOrd p[4];

	if (!text || !*text) {
		return; /* No string or blank */
	}
	line = malloc(strlen(text) + 1);

	DrawMultiLineTextSize(&mainD, text, fp, fs, FALSE, &size, &posl);

	DrawTextSize2(&mainD, "Aqjlp", fp, fs, TRUE, &size2, &descent, &ascent);

	/* set up the corners of the rectangle */
	p[0].x = p[3].x = pos.x;
	p[1].x = p[2].x = pos.x + size.x;
	p[0].y = p[1].y = pos.y + posl.y - descent;
	p[2].y = p[3].y = pos.y + size2.y;

	orig.x = pos.x;
	orig.y = pos.y;
	for (int i = 0; i < 4; i++) {
		Rotate(&p[i], orig, a);
	}
	if (filled && (d != &mapD)) {
		DrawPoly(d, 4, p, NULL, bg_color, 0, DRAW_FILL);
	}

	lineH = (ascent + descent) * 1.0;
	cp = line;
	while (*text) {
		cp1 = cp;
		while (*text != '\0' && *text != '\n') {
			*cp++ = *text++;
		}
		*cp = '\0';
		posl.x = pos.x;
		posl.y = pos.y;
		Rotate(&posl, orig, a);
		DrawString(d, posl, a, cp1, fp, fs, color);
		pos.y -= lineH;
		if (*text == '\0') {
			break;
		}
		text++;
		cp++;
	}
	if (boxed && (d != &mapD)) {
		DrawPoly(d, 4, p, NULL, color, 0, DRAW_CLOSED);
	}

	free(line);
}

/**
 * Draw some text inside a box. The layout of the box can be defined using the
 * style parameter. Possibilities are complete frame, underline only, omit
 * background or draw inversed. The background is drawn in white if not disabled
 *
 * \param style	style of box framed, underlined, no background, inverse
 * \param d		drawing command
 * \param pos	position
 * \param text	text to draw
 * \param fp	font
 * \param fs	font size
 * \param color	text color
 * \param a		angle
 */

EXPORT void DrawBoxedString(int style, drawCmd_p d, coOrd pos, char *text,
                            wFont_p fp, wFontSize_t fs, wDrawColor color,
                            ANGLE_T a)
{
	coOrd size, p[4], p0 = pos, p1, p2;
	static int br = 0, bl = 1, bt = 2, bb = -1;
	static double arrowScale = 0.5;
	POS_T descent, ascent;
	if (fs < 2 * d->scale) {
		return;
	}
#ifndef WINDOWS
	if ((d->options & DC_PRINT) != 0) {
		double scale = ((FLOAT_T)fs) / ((FLOAT_T)drawMaxTextFontSize) / mainD.dpi;
		wDrawPix_t w, h, d, a;
		wDrawGetTextSize(&w, &h, &d, &a, mainD.d, text, fp, drawMaxTextFontSize);
		size.x = w * scale;
		size.y = h * scale;
		descent = d * scale;
		ascent = a * scale;
	} else
#endif
		DrawTextSize2(&mainD, text, fp, fs, TRUE, &size, &descent, &ascent);
	if (style & BOX_POS_TOP_CENTER) {
		p0.x -= size.x / 2.0;
	}
	style &= ~BOX_POS_TOP_CENTER;

	if (style & BOX_POS_LEFT_CENTER) {
		p0.y -= size.y / 2.0;
	}
	style &= ~BOX_POS_LEFT_CENTER;

	if(style&BOX_POS_BOTTOM_LEFT )
		p0.y += descent;

	if ((style & BOX_POS_BOTTOM_LEFT) == 0) {
		if ((style & BOX_POS_BOTTOM_RIGHT) != 0) {
			p0.x -= size.x - 1;
			p0.y -= size.y / 2.0 - 1.5;
		} else {
			p0.x -= size.x / 2.0;
			p0.y -= size.y / 2.0;
		}
	}
	style &= ~(BOX_POS_BOTTOM_LEFT | BOX_POS_BOTTOM_RIGHT);

	if (style == BOX_NONE || d == &mapD) {
		DrawString(d, p0, 0.0, text, fp, fs, color);
		return;
	}
	p[0].x = p[3].x = p0.x - bl * d->scale / d->dpi;
	p[2].x = p[1].x = p0.x + br * d->scale / d->dpi + size.x;
	p[0].y = p[1].y = p0.y + bt * d->scale / d->dpi + ascent;
	p[3].y = p[2].y = p0.y - bb * d->scale / d->dpi - descent;

	d->options &= ~DC_DASH;
	switch (style) {
	case BOX_ARROW:
	case BOX_ARROW_BACKGROUND:
		size.x = p[2].x - p[0].x;
		size.y = p[0].y - p[2].y;
		Translate(&p1, pos, a, size.x + size.y);
		ClipLine(&pos, &p1, p[3], 0.0, size);
		Translate(&p2, p1, a, size.y * arrowScale);
		DrawLine(d, p1, p2, 0, color);
		Translate(&p1, p2, a + 150, size.y * 0.7 * arrowScale);
		DrawLine(d, p1, p2, 0, color);
		Translate(&p1, p2, a - 150, size.y * 0.7 * arrowScale);
		DrawLine(d, p1, p2, 0, color);
	/* no break */
	case BOX_BOX:
	case BOX_BOX_BACKGROUND:
		if (style == BOX_ARROW_BACKGROUND || style == BOX_BOX_BACKGROUND) {
			DrawPoly(d, 4, p, NULL, wDrawColorWhite, 0, DRAW_FILL);
		}
		DrawLine(d, p[1], p[2], 0, color);
		DrawLine(d, p[2], p[3], 0, color);
		DrawLine(d, p[3], p[0], 0, color);
	/* no break */
	case BOX_UNDERLINE:
		DrawLine(d, p[0], p[1], 0, color);
		DrawString(d, p0, 0.0, text, fp, fs, color);
		break;
	case BOX_INVERT:
		DrawPoly(d, 4, p, NULL, color, 0, DRAW_FILL);
		if (color != wDrawColorWhite) {
			DrawString(d, p0, 0.0, text, fp, fs, wDrawColorGray(94));
		}
		break;
	case BOX_BACKGROUND:
		DrawPoly(d, 4, p, NULL, wDrawColorWhite, 0, DRAW_FILL);
		DrawString(d, p0, 0.0, text, fp, fs, color);
		break;
	}
}

EXPORT void DrawTextSize2(drawCmd_p dp, char *text, wFont_p fp, wFontSize_t fs,
                          BOOL_T relative, coOrd *size, POS_T *descent,
                          POS_T *ascent)
{
	wDrawPix_t w, h, d, a;
	FLOAT_T scale = 1.0;
	if (relative) {
		fs /= dp->scale;
	}
	if (fs > drawMaxTextFontSize) {
		scale = ((FLOAT_T)fs) / ((FLOAT_T)drawMaxTextFontSize);
		fs = drawMaxTextFontSize;
	}
	wDrawGetTextSize(&w, &h, &d, &a, dp->d, text, fp, fs);
	size->x = SCALEX(mainD, w) * scale;
	size->y = SCALEY(mainD, h) * scale;
	*descent = SCALEY(mainD, d) * scale;
	*ascent = SCALEY(mainD, a) * scale;
	if (relative) {
		size->x *= dp->scale;
		size->y *= dp->scale;
		*descent *= dp->scale;
		*ascent *= dp->scale;
	}
}

EXPORT void DrawTextSize(drawCmd_p dp, char *text, wFont_p fp, wFontSize_t fs,
                         BOOL_T relative, coOrd *size)
{
	POS_T descent, ascent;
	DrawTextSize2(dp, text, fp, fs, relative, size, &descent, &ascent);
}

EXPORT void DrawMultiLineTextSize(drawCmd_p dp, char *text, wFont_p fp,
                                  wFontSize_t fs, BOOL_T relative, coOrd *size,
                                  coOrd *lastline)
{
	POS_T descent, ascent, lineW, lineH;
	coOrd textsize, blocksize;

	char *cp;
	char *line = malloc(strlen(text) + 1);

	DrawTextSize2(&mainD, "Aqlip", fp, fs, TRUE, &textsize, &descent, &ascent);
	lineH = (ascent + descent) * 1.0;
	blocksize.x = 0;
	blocksize.y = 0;
	lastline->x = 0;
	lastline->y = 0;
	while (text && *text != '\0') {
		cp = line;
		while (*text != '\0' && *text != '\n') {
			*cp++ = *text++;
		}
		*cp = '\0';
		blocksize.y += lineH;
		DrawTextSize2(&mainD, line, fp, fs, TRUE, &textsize, &descent, &ascent);
		lineW = textsize.x;
		if (lineW > blocksize.x) {
			blocksize.x = lineW;
		}
		lastline->x = textsize.x;
		if (*text == '\n') {
			blocksize.y += lineH;
			lastline->y -= lineH;
			lastline->x = 0;
		}
		if (*text == '\0') {
			blocksize.y += textsize.y;
			break;
		}
		text++;
	}
	size->x = blocksize.x;
	size->y = blocksize.y;
	free(line);
}

/****************************************************************************
 *
 * BITMAP
 *
 */

static void DDrawBitMap(drawCmd_p d, coOrd p, wDrawBitMap_p bm,
                        wDrawColor color)
{
	wDrawPix_t x, y;
	d->CoOrd2Pix(d, p, &x, &y);
	wDrawBitMap(d->d, bm, x, y, color, DRAWOPTS(d));
}

/****************************************************************************
 *
 * TEMP-SEG (record-mode) DRAWING FUNCTIONS
 *
 */

static void TempSegLine(drawCmd_p d, coOrd p0, coOrd p1, wDrawWidth width,
                        wDrawColor color)
{
	DYNARR_APPEND(trkSeg_t, tempSegs_da, 10);
	tempSegs(tempSegs_da.cnt - 1).type = SEG_STRLIN;
	tempSegs(tempSegs_da.cnt - 1).color = color;
	if (d->options & DC_SIMPLE) {
		tempSegs(tempSegs_da.cnt - 1).lineWidth = 0;
	} else if (width < 0) {
		tempSegs(tempSegs_da.cnt - 1).lineWidth = width;
	} else {
		tempSegs(tempSegs_da.cnt - 1).lineWidth = width * d->scale / d->dpi;
	}
	tempSegs(tempSegs_da.cnt - 1).u.l.pos[0] = p0;
	tempSegs(tempSegs_da.cnt - 1).u.l.pos[1] = p1;
}

static void TempSegArc(drawCmd_p d, coOrd p, DIST_T r, ANGLE_T angle0,
                       ANGLE_T angle1, BOOL_T drawCenter, wDrawWidth width,
                       wDrawColor color)
{
	DYNARR_APPEND(trkSeg_t, tempSegs_da, 10);
	tempSegs(tempSegs_da.cnt - 1).type = SEG_CRVLIN;
	tempSegs(tempSegs_da.cnt - 1).color = color;
	if (d->options & DC_SIMPLE) {
		tempSegs(tempSegs_da.cnt - 1).lineWidth = 0;
	} else if (width < 0) {
		tempSegs(tempSegs_da.cnt - 1).lineWidth = width;
	} else {
		tempSegs(tempSegs_da.cnt - 1).lineWidth = width * d->scale / d->dpi;
	}
	tempSegs(tempSegs_da.cnt - 1).u.c.center = p;
	tempSegs(tempSegs_da.cnt - 1).u.c.radius = r;
	tempSegs(tempSegs_da.cnt - 1).u.c.a0 = angle0;
	tempSegs(tempSegs_da.cnt - 1).u.c.a1 = angle1;
}

static void TempSegString(drawCmd_p d, coOrd p, ANGLE_T a, char *s, wFont_p fp,
                          FONTSIZE_T fontSize, wDrawColor color)
{
	DYNARR_APPEND(trkSeg_t, tempSegs_da, 10);
	tempSegs(tempSegs_da.cnt - 1).type = SEG_TEXT;
	tempSegs(tempSegs_da.cnt - 1).color = color;
	tempSegs(tempSegs_da.cnt - 1).u.t.boxed = FALSE;
	tempSegs(tempSegs_da.cnt - 1).lineWidth = 0;
	tempSegs(tempSegs_da.cnt - 1).u.t.pos = p;
	tempSegs(tempSegs_da.cnt - 1).u.t.angle = a;
	tempSegs(tempSegs_da.cnt - 1).u.t.fontP = fp;
	tempSegs(tempSegs_da.cnt - 1).u.t.fontSize = fontSize;
	tempSegs(tempSegs_da.cnt - 1).u.t.string = MyStrdup(s);
}

static void TempSegPoly(drawCmd_p d, int cnt, coOrd *pts, int *types,
                        wDrawColor color, wDrawWidth width,
                        drawFill_e eFillOpt)
{
	int fill = 0;
	int open = 0;
	switch (eFillOpt) {
	case DRAW_OPEN:
		open = 1;
		break;
	case DRAW_CLOSED:
		break;
	case DRAW_FILL:
		fill = 1;
		break;
	case DRAW_TRANSPARENT:
		fill = 1;
		break;
	default:
		CHECK(FALSE);
	}
	DYNARR_APPEND(trkSeg_t, tempSegs_da, 1);
	tempSegs(tempSegs_da.cnt - 1).type = fill ? SEG_FILPOLY : SEG_POLY;
	tempSegs(tempSegs_da.cnt - 1).color = color;
	if (d->options & DC_SIMPLE) {
		tempSegs(tempSegs_da.cnt - 1).lineWidth = 0;
	} else if (width < 0) {
		tempSegs(tempSegs_da.cnt - 1).lineWidth = width;
	} else {
		tempSegs(tempSegs_da.cnt - 1).lineWidth = width * d->scale / d->dpi;
	}
	tempSegs(tempSegs_da.cnt - 1).u.p.polyType = open ? POLYLINE : FREEFORM;
	tempSegs(tempSegs_da.cnt - 1).u.p.cnt = cnt;
	tempSegs(tempSegs_da.cnt - 1).u.p.orig = zero;
	tempSegs(tempSegs_da.cnt - 1).u.p.angle = 0.0;
	tempSegs(tempSegs_da.cnt - 1).u.p.pts =
	        (pts_t *)MyMalloc(cnt * sizeof(pts_t));
	for (int i = 0; i <= cnt - 1; i++) {
		tempSegs(tempSegs_da.cnt - 1).u.p.pts[i].pt = pts[i];
		tempSegs(tempSegs_da.cnt - 1).u.p.pts[i].pt_type =
		        ((d->options & DC_SIMPLE) == 0 && (types != 0)) ? types[i]
		        : wPolyLineStraight;
	}
}

static void TempSegFillCircle(drawCmd_p d, coOrd p, DIST_T r,
                              wDrawColor color)
{
	DYNARR_APPEND(trkSeg_t, tempSegs_da, 10);
	tempSegs(tempSegs_da.cnt - 1).type = SEG_FILCRCL;
	tempSegs(tempSegs_da.cnt - 1).color = color;
	tempSegs(tempSegs_da.cnt - 1).lineWidth = 0;
	tempSegs(tempSegs_da.cnt - 1).u.c.center = p;
	tempSegs(tempSegs_da.cnt - 1).u.c.radius = r;
	tempSegs(tempSegs_da.cnt - 1).u.c.a0 = 0.0;
	tempSegs(tempSegs_da.cnt - 1).u.c.a1 = 360.0;
}

static void TempSegRectangle(drawCmd_p d, coOrd orig, coOrd size,
                             wDrawColor color, drawFill_e eOpts)
{
	coOrd p[4];
	/* p1 p2 */
	/* p0 p3 */
	p[0].x = p[1].x = orig.x;
	p[2].x = p[3].x = orig.x + size.x;
	p[0].y = p[3].y = orig.y;
	p[1].y = p[2].y = orig.y + size.y;
	TempSegPoly(d, 4, p, NULL, color, 0, eOpts);
}

static void NoDrawBitMap(drawCmd_p d, coOrd p, wDrawBitMap_p bm,
                         wDrawColor color) {}

/****************************************************************************
 *
 * BITMAP DRAW FUNCTIONS
 * Call wBasicXxx directly; used as the bitmapDrawFuncs vtable entries.
 *
 */

/* Forward declaration (BDrawString calls BDrawPoly for white-text boxes) */
static void BDrawPoly(drawCmd_p d, int cnt, coOrd *pts, int *types,
                      wDrawColor color, wDrawWidth width, drawFill_e eFillOpt);

static void BDrawLine(drawCmd_p d, coOrd p0, coOrd p1, wDrawWidth width,
                      wDrawColor color)
{
	wDrawPix_t x0, y0, x1, y1;
	BOOL_T in0 = FALSE, in1 = FALSE;
	coOrd orig, size;
	int lborder = GetLBorder();
	int bborder = GetBBorder();
	if (d == &mapD && !mapVisible) return;
	if ((d->options & DC_NOCLIP) == 0) {
		if (d->angle == 0.0) {
			in0 = (p0.x >= d->orig.x && p0.x <= d->orig.x + d->size.x &&
			       p0.y >= d->orig.y && p0.y <= d->orig.y + d->size.y);
			in1 = (p1.x >= d->orig.x && p1.x <= d->orig.x + d->size.x &&
			       p1.y >= d->orig.y && p1.y <= d->orig.y + d->size.y);
		}
		if ((!in0) || (!in1)) {
			orig = d->orig;
			size = d->size;
			if (d->options & DC_TICKS) {
				orig.x -= lborder / d->dpi * d->scale;
				orig.y -= bborder / d->dpi * d->scale;
				size.x += (lborder + RBORDER) / d->dpi * d->scale;
				size.y += (bborder + TBORDER) / d->dpi * d->scale;
			}
			if (!ClipLine(&p0, &p1, orig, d->angle, size)) return;
		}
	}
	d->CoOrd2Pix(d, p0, &x0, &y0);
	d->CoOrd2Pix(d, p1, &x1, &y1);
	drawCount++;
	wDrawLineType_e lineOpt = wDrawLineSolid;
	unsigned long opt = d->options & DC_NOTSOLIDLINE;
	if (opt == DC_DASH)            lineOpt = wDrawLineDash;
	else if (opt == DC_DOT)        lineOpt = wDrawLineDot;
	else if (opt == DC_DASHDOT)    lineOpt = wDrawLineDashDot;
	else if (opt == DC_DASHDOTDOT) lineOpt = wDrawLineDashDotDot;
	else if (opt == DC_CENTER)     lineOpt = wDrawLineCenter;
	else if (opt == DC_PHANTOM)    lineOpt = wDrawLinePhantom;
	if (drawEnable)
		wBasicDrawLine(d->d, x0, y0, x1, y1, width, MINLINEWIDTHBITMAP,
		               lineOpt, color, DRAWOPTS(d));
}

static void BDrawArc(drawCmd_p d, coOrd p, DIST_T r, ANGLE_T angle0,
                     ANGLE_T angle1, BOOL_T drawCenter, wDrawWidth width,
                     wDrawColor color)
{
	wDrawPix_t x, y;
	ANGLE_T da;
	coOrd p0, p1;
	DIST_T rr;
	int i, cnt;

	if (d == &mapD && !mapVisible) return;
	rr = (r / d->scale) * d->dpi + 0.5;
	if (rr > wDrawGetMaxRadius(d->d)) {
		da = (maxArcSegStraightLen * 180) / (M_PI * rr);
		cnt = (int)(angle1 / da) + 1;
		da = angle1 / cnt;
		coOrd min, max;
		min = d->orig;
		max.x = min.x + d->size.x;
		max.y = min.y + d->size.y;
		PointOnCircle(&p0, p, r, angle0);
		for (i = 1; i <= cnt; i++) {
			angle0 += da;
			PointOnCircle(&p1, p, r, angle0);
			if (d->angle == 0.0 &&
			    ((p0.x >= min.x && p0.x <= max.x && p0.y >= min.y && p0.y <= max.y) ||
			     (p1.x >= min.x && p1.x <= max.x && p1.y >= min.y && p1.y <= max.y))) {
				DrawLine(d, p0, p1, width, color);
			} else {
				coOrd clip0 = p0, clip1 = p1;
				if (ClipLine(&clip0, &clip1, d->orig, d->angle, d->size))
					DrawLine(d, clip0, clip1, width, color);
			}
			p0 = p1;
		}
		return;
	}
	if (d->angle != 0.0 && angle1 < 360.0)
		angle0 = NormalizeAngle(angle0 - d->angle);
	d->CoOrd2Pix(d, p, &x, &y);
	drawCount++;
	wDrawLineType_e lineOpt = wDrawLineSolid;
	unsigned long opt = d->options & DC_NOTSOLIDLINE;
	if (opt == DC_DASH)            lineOpt = wDrawLineDash;
	else if (opt == DC_DOT)        lineOpt = wDrawLineDot;
	else if (opt == DC_DASHDOT)    lineOpt = wDrawLineDashDot;
	else if (opt == DC_DASHDOTDOT) lineOpt = wDrawLineDashDotDot;
	else if (opt == DC_CENTER)     lineOpt = wDrawLineCenter;
	else if (opt == DC_PHANTOM)    lineOpt = wDrawLinePhantom;
	if (drawEnable) {
		int sizeCenter = (int)(drawCenter ?
		        ((d->options & DC_PRINT) ? (d->dpi / BASE_DPI) : 1) : 0);
		wBasicDrawArc(d->d, x, y, (wDrawPix_t)(rr), angle0, angle1,
		              sizeCenter, width, MINLINEWIDTHBITMAP,
		              lineOpt, color, DRAWOPTS(d));
	}
}

static void BDrawString(drawCmd_p d, coOrd p, ANGLE_T a, char *s, wFont_p fp,
                        FONTSIZE_T fontSize, wDrawColor color)
{
	wDrawPix_t x, y;
	if (d == &mapD && !mapVisible) return;
	d->CoOrd2Pix(d, p, &x, &y);
	if (color == wDrawColorWhite) {
		wDrawPix_t width, height, descent, ascent;
		coOrd pos[4], size;
		double scale = 1.0;
		wDrawGetTextSize(&width, &height, &descent, &ascent, d->d, s, fp, fontSize);
		pos[0] = p;
		size.x = SCALEX(mainD, width) * scale;
		size.y = SCALEY(mainD, height) * scale;
		pos[1].x = p.x + size.x; pos[1].y = p.y;
		pos[2].x = p.x + size.x; pos[2].y = p.y + size.y;
		pos[3].x = p.x;          pos[3].y = p.y + size.y;
		Rotate(&pos[1], pos[0], a);
		Rotate(&pos[2], pos[0], a);
		Rotate(&pos[3], pos[0], a);
		BDrawPoly(d, 4, pos, NULL, color, 0, DRAW_FILL);
	} else {
		fontSize /= d->scale;
		wBasicDrawString(d->d, x, y, d->angle - a, s, fp, fontSize,
		                 MINLINEWIDTHBITMAP, MINLINEWIDTHBITMAP,
		                 color, DRAWOPTS(d));
	}
}

static void BDrawPoly(drawCmd_p d, int cnt, coOrd *pts, int *types,
                      wDrawColor color, wDrawWidth width, drawFill_e eFillOpt)
{
	typedef wDrawPix_t bPos2[2];
	static dynArr_t bwpts_da;
	static dynArr_t bwpts_type_da;
	int inx, fill = 0, open = 0;
	wDrawPix_t x, y;
	DYNARR_SET(bPos2, bwpts_da, cnt * 2);
	DYNARR_SET(int, bwpts_type_da, cnt);
#define bwpts(N) DYNARR_N(bPos2, bwpts_da, N)
#define bwtype(N) DYNARR_N(wPolyLine_e, bwpts_type_da, N)
	for (inx = 0; inx < cnt; inx++) {
		d->CoOrd2Pix(d, pts[inx], &x, &y);
		bwpts(inx)[0] = x;
		bwpts(inx)[1] = y;
		bwtype(inx) = types ? (wPolyLine_e)types[inx] : 0;
	}
	wDrawOpts drawOpts = DRAWOPTS(d);
	switch (eFillOpt) {
	case DRAW_OPEN:        open = 1; break;
	case DRAW_CLOSED:      break;
	case DRAW_FILL:        fill = 1; break;
	case DRAW_TRANSPARENT: fill = 1; drawOpts |= wDrawOptTransparent; break;
	default: CHECK(FALSE);
	}
	wBasicDrawFillPolygon(d->d, &bwpts(0), &bwtype(0), cnt, color, drawOpts,
	                      fill, open);
}

static void BDrawFillCircle(drawCmd_p d, coOrd p, DIST_T r, wDrawColor color)
{
	wDrawPix_t x, y;
	DIST_T rr;
	int lborder = GetLBorder();
	int bborder = GetBBorder();

	if (d == &mapD && !mapVisible) return;
	rr = (r / d->scale) * d->dpi + 0.5;
	if (rr > wDrawGetMaxRadius(d->d)) return;
	d->CoOrd2Pix(d, p, &x, &y);
	wWinPix_t w, h;
	wDrawGetSize(d->d, &w, &h);
	if (d->options & DC_TICKS) {
		if (x + rr < lborder || x - rr > w - RBORDER ||
		    y + rr < bborder || y - rr > h - TBORDER) return;
	} else {
		if (x + rr < 0 || x - rr > w || y + rr < 0 || y - rr > h) return;
	}
	drawCount++;
	if (drawEnable)
		wBasicDrawFillCircle(d->d, x, y, (wDrawPix_t)(rr), color, DRAWOPTS(d));
}

static void BDrawRectangle(drawCmd_p d, coOrd orig, coOrd size,
                            wDrawColor color, drawFill_e eFillOpt)
{
	wDrawPix_t x, y, w, h;

	if (d == &mapD && !mapVisible) return;
	d->CoOrd2Pix(d, orig, &x, &y);
	w = (wDrawPix_t)((size.x / d->scale) * d->dpi + 0.5);
	h = (wDrawPix_t)((size.y / d->scale) * d->dpi + 0.5);
	drawCount++;
	if (drawEnable) {
		wDrawOpts opts = DRAWOPTS(d);
		coOrd p1, p2;
		switch (eFillOpt) {
		case DRAW_CLOSED:
			p1.x = orig.x; p1.y = orig.y + size.y;
			DrawLine(d, orig, p1, 0, color);
			p2.x = orig.x + size.x; p2.y = p1.y;
			DrawLine(d, p1, p2, 0, color);
			p1.x = p2.x; p1.y = orig.y;
			DrawLine(d, p2, p1, 0, color);
			DrawLine(d, p1, orig, 0, color);
			break;
		case DRAW_TRANSPARENT:
			opts |= wDrawOptTransparent;
		/* Fallthru */
		case DRAW_FILL:
			if (d->options & DC_ROUND) { x = round(x); y = round(y); }
			wBasicDrawFillRectangle(d->d, x, y, w, h, color, opts);
			break;
		default:
			CHECK(FALSE);
		}
	}
}

/****************************************************************************
 *
 * DRAW FUNCTION TABLES AND COMMAND DESCRIPTORS
 *
 */

EXPORT drawFuncs_t screenDrawFuncs = {DDrawLine,     DDrawArc,  DDrawString,
                                      DDrawBitMap,   DDrawPoly, DDrawFillCircle,
                                      DDrawRectangle
                                     };

EXPORT drawFuncs_t printDrawFuncs = {DDrawLine,     DDrawArc,  DDrawString,
                                     NoDrawBitMap,  DDrawPoly, DDrawFillCircle,
                                     DDrawRectangle
                                    };

EXPORT drawFuncs_t bitmapDrawFuncs = {BDrawLine,     BDrawArc,  BDrawString,
                                      NoDrawBitMap,  BDrawPoly, BDrawFillCircle,
                                      BDrawRectangle
                                     };

EXPORT drawFuncs_t tempSegDrawFuncs = {
	TempSegLine, TempSegArc,        TempSegString,   NoDrawBitMap,
	TempSegPoly, TempSegFillCircle, TempSegRectangle
};

/* mainD and tempD are defined in draw.c; the CoOrd2Pix/Pix2CoOrd function
 * pointers are patched in by InitDrawCmds(), called from DrawInit(). */
extern drawCmd_t mainD;
extern drawCmd_t tempD;

/**
 * Wire the static CoOrd2Pix / Pix2CoOrd helpers (which are private to this
 * translation unit) into the mainD / tempD command descriptors.  Must be
 * called from DrawInit() before any drawing takes place.
 */
EXPORT void InitDrawCmds(void)
{
	mainD.CoOrd2Pix  = MainCoOrd2Pix;
	mainD.Pix2CoOrd  = MainPix2CoOrd;
	mainD.funcs      = &screenDrawFuncs;
	tempD.CoOrd2Pix  = MainCoOrd2Pix;
	tempD.Pix2CoOrd  = MainPix2CoOrd;
	tempD.funcs      = &screenDrawFuncs;
}
