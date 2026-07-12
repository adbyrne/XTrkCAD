/** \file drawruler.c
 * Ruler and border decoration drawing functions.
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

/* Accessor functions for border widths and character width, defined in draw.c */
extern int GetLBorder(void);
extern int GetBBorder(void);
extern int DrawGetCharWidth(void);

static void DrawTicks(drawCmd_p d, coOrd size);


static wFont_p rulerFp;
static DIST_T rulerFontSize = 12.0;
#define MAXLABELCHARS 5

/**
 * Initialise the ruler font. Called once from DrawInit() in draw.c after the
 * main drawing context is available.
 *
 * \param fp         font handle to use for ruler labels
 * \param fontSize   point size for ruler labels
 */
void SetRulerFont(wFont_p fp, DIST_T fontSize)
{
	rulerFp = fp;
	rulerFontSize = fontSize;
}

/*****************************************************************************
 *
 * ROOM WALLS AND MARKER DECORATIONS
 *
 */

void DrawRoomWalls(wBool_t drawBackground)
{
	if (mainD.d == NULL) {
		return;
	}

	if (drawBackground) {
		DrawRectangle(&mainD, mainD.orig, mainD.size, drawColorGrey80, DRAW_FILL);
		DrawRectangle(&mainD, zero, mapD.size, wDrawColorWhite, DRAW_FILL);
	} else {
		coOrd p[4];
		p[0].x = p[0].y = p[1].x = p[3].y = 0.0;
		p[2].x = p[3].x = mapD.size.x;
		p[1].y = p[2].y = mapD.size.y;
		DrawPoly(&mainD, 4, p, NULL, borderColor, 3, DRAW_CLOSED);
	}
}

/**
 * Draw ruler tick marks and labels in the border areas.
 * Must be called without clipping so rulers in the margins are visible.
 */
void DrawRulers(void)
{
	if (mainD.d == NULL) {
		return;
	}
	DrawTicks(&mainD, mapD.size);
}

void DrawMarkers(void)
{
	coOrd p0, p1;
	int lborder = GetLBorder();
	int bborder = GetBBorder();
	p0.x = p1.x = oldMarker.x;
	p0.y = mainD.orig.y;
	p1.y = mainD.orig.y - bborder * mainD.scale / mainD.dpi;
//	DrawLine(&tempD, p0, p1, 3, wDrawColorWhite);
	DrawLine(&tempD, p0, p1, 0, markerColor);
	p0.y = p1.y = oldMarker.y;
	p0.x = mainD.orig.x;
	p1.x = mainD.orig.x - lborder * mainD.scale / mainD.dpi;
//	DrawLine(&tempD, p0, p1, 3, wDrawColorWhite);
	DrawLine(&tempD, p0, p1, 0, markerColor);
}

/*****************************************************************************
 *
 * RULER DRAWING
 *
 */

/**
 * @brief Draw a white filled rectangle behind the ruler to clear antialiasing
 *        artifacts in one call, replacing per-tick white line erasure.
 *
 * @param d         Drawing command context
 * @param pos0      Clipped spine start (layout coords)
 * @param pos1      Clipped spine end   (layout coords)
 * @param a         Ruler angle (0.0 = vertical, 90.0 = horizontal)
 * @param tickSide  Which side the ticks extend to
 * @param maxTickPx Maximum tick length in pixels
 */
static void DrawRulerBackground(drawCmd_p d, coOrd pos0, coOrd pos1,
                                wAngle_t a, int tickSide, int maxTickPx,
                                wDrawColor bgColor)
{
	wDrawPix_t px0, py0, px1, py1;

	px0 = (wDrawPix_t)((pos0.x - d->orig.x) * d->dpi / d->scale);
	py0 = (wDrawPix_t)((pos0.y - d->orig.y) * d->dpi / d->scale);
	px1 = (wDrawPix_t)((pos1.x - d->orig.x) * d->dpi / d->scale);
	py1 = (wDrawPix_t)((pos1.y - d->orig.y) * d->dpi / d->scale);

	int lborder = GetLBorder();
	int bborder = GetBBorder();

	if (a == 90.0) {
		/* Horizontal ruler */
		wDrawPix_t x = (px0 < px1) ? px0 : px1;
		wDrawPix_t w = fabs(px1 - px0);
		wDrawPix_t y, h;
		h = maxTickPx;
		if (tickSide == 0) {
			y = (py0 < py1) ? py0 : py1;
		} else {
			y = ((py0 < py1) ? py0 : py1) + maxTickPx;
		}
		wDrawFilledRectangle(d->d, x + lborder, y + bborder - maxTickPx,
		                     w, h, bgColor, 0);
	} else if (a == 0.0) {
		/* Vertical ruler */
		wDrawPix_t y = (py0 < py1) ? py0 : py1;
		wDrawPix_t h = fabs(py1 - py0);
		wDrawPix_t x, w;
		w = maxTickPx;
		if (tickSide == 0) {
			x = (px0 < px1) ? px0 : px1;
		} else {
			x = ((px0 < px1) ? px0 : px1) - maxTickPx;
		}
		wDrawFilledRectangle(d->d, x + lborder, y + bborder,
		                     w, h, bgColor, 0);
	}
}

static void DrawRulerWithBackground(drawCmd_p d, coOrd pos0, coOrd pos1,
                                    DIST_T offset, int number, int tickSide,
                                    wDrawColor color, wDrawColor bgColor)
{
	coOrd orig = pos0;
	wAngle_t a, aa;
	DIST_T start, end;
	long inch, lastInch;
	DIST_T len;
	int digit;
	char quote = ' ';
	char message[STR_SHORT_SIZE];
	coOrd d_orig, d_size;
	wFontSize_t fs;
	long mm, mm0, mm1, power, skip;

	static double lengths[] = {0,   2.0, 4.0, 2.0, 6.0, 2.0, 4.0, 2.0, 8.0,
	                           2.0, 4.0, 2.0, 6.0, 2.0, 4.0, 2.0, 0.0
	                          };
	int fraction, incr, firstFraction, lastFraction;
	int majorLength;
	coOrd p0, p1;

	/* Fetch border metrics for label positioning */
	int lborder   = GetLBorder();
	int bborder   = GetBBorder();
	int charWidth = DrawGetCharWidth();

	a = FindAngle(pos0, pos1);
	Translate(&pos0, pos0, a, offset);
	Translate(&pos1, pos1, a, offset);
	aa = NormalizeAngle(a + (tickSide == 0 ? +90 : -90));

	end = FindDistance(pos0, pos1);
	if (end < 0.1) {
		return;
	}
	d_orig.x = d->orig.x - 0.1;
	d_orig.y = d->orig.y - 0.1;
	d_size.x = d->size.x + 0.2;
	d_size.y = d->size.y + 0.2;
	if (!ClipLine(&pos0, &pos1, d_orig, d->angle, d_size)) {
		return;
	}

	start = FindDistance(orig, pos0);
	if (offset < 0) {
		start = -start;
	}
	end = FindDistance(orig, pos1);

	/*
	 * Determine the maximum tick length in pixels so we can clear
	 * the entire ruler area with a single white rectangle.
	 */
	int maxTickPx;
	if (units == UNITS_METRIC) {
		maxTickPx = 10;  /* longest metric tick is 10px (at power==1000) */
	} else {
		maxTickPx = 16;  /* longest imperial tick (majorLength) */
	}

	/* Draw white background rectangle to clear antialiasing artifacts */
	DrawRulerBackground(d, pos0, pos1, a, tickSide, maxTickPx, bgColor);

	/* Draw the ruler spine */
	DrawLine(d, pos0, pos1, 0, color);

	/**
	 * @brief Position label anchor and select box alignment flag.
	 *
	 * Vertical ruler (a == 0.0):
	 *   Translate in aa direction by (lborder - 2) pixels so the anchor
	 *   sits 2px from the far edge of the border strip.
	 *   tickSide=TRUE  (left ruler)  -> BOX_POS_LEFT_CENTER
	 *   tickSide=FALSE (right ruler) -> BOX_POS_BOTTOM_LEFT
	 *
	 * Horizontal ruler (a == 90.0):
	 *   Translate in aa direction by (bborder + tickLen) / 2 pixels to
	 *   vertically centre the label in the space below the tick tip.
	 *   -> BOX_POS_TOP_CENTER (horizontally centred on tick)
	 */
#define LABEL_POS_VERTICAL(p0, tickSide, boxStyle)                             \
  do {                                                                          \
    Translate(&(p0), (p0), aa, (lborder - 24) * mainD.scale / mainD.dpi);      \
    Translate(&(p0), (p0), a, rulerFontSize / 72.0 * d->scale);                \
    (boxStyle) |= ((tickSide) ? BOX_POS_LEFT_CENTER : BOX_POS_BOTTOM_LEFT);    \
  } while (0)

#define LABEL_POS_HORIZONTAL(p0, tickLen, boxStyle)                            \
  do {                                                                          \
    Translate(&(p0), (p0), aa,                                                 \
              ((bborder + (tickLen) - 6) / 2.0) * mainD.scale / mainD.dpi);    \
    Translate(&(p0), (p0), a, ((double)charWidth * 1.5) * mainD.scale / mainD.dpi); \
    (boxStyle) |= BOX_POS_TOP_CENTER;                                          \
  } while (0)

	if (units == UNITS_METRIC) {
		mm0 = (int)ceil(start * 25.4 - 0.5);
		mm1 = (int)floor(end * 25.4 + 0.5);
		len = 5;
		if (d->scale <= 1) {
			power = 1;
		} else if (d->scale <= 8) {
			power = 10;
		} else if (d->scale <= 32) {
			power = 100;
		} else {
			power = 1000;
		}

		/* Label interval for scale > 40 */
		if (d->scale <= 200) {
			skip = 2000;
		} else if (d->scale <= 400) {
			skip = 5000;
		} else {
			skip = 10000;
		}

		for (; power <= 1000; power *= 10, len += 3) {
			if (power == 1000) {
				len = 10;
			}
			for (mm = ((mm0 + (mm0 > 0 ? power - 1 : 0)) / power) * power;
			     mm <= mm1; mm += power) {
				if (power == 1000 || mm % (power * 10) != 0) {
					Translate(&p0, orig, a, mm / 25.4);
					Translate(&p1, p0, aa, len * mainD.scale / mainD.dpi);
					DrawLine(d, p0, p1, 0, color);
					if (!number || (d->scale > 40 && mm % skip != 0.0)) {
						continue;
					}
					if ((power >= 1000) || (d->scale <= 8 && power >= 100) ||
					    (d->scale <= 1 && power >= 10)) {
						int boxStyle = BOX_BACKGROUND;
						if (mm % 100 != 0) {
							/* @brief Sub-metre digit label at smaller font size */
							sprintf(message, "%ld", mm / 10 % 10);
							fs = rulerFontSize * 2 / 3;
						} else {
							/* @brief Main metre label formatted as e.g. "1.5" */
							sprintf(message, "%0.1f", mm / 1000.0);
							fs = rulerFontSize;
						}
						if (a == 0.0) {
							LABEL_POS_VERTICAL(p0, tickSide, boxStyle);
							if(boxStyle&BOX_POS_BOTTOM_LEFT) {
								p0.y -= 1.5*(rulerFontSize / 72.0 * d->scale);
							}
						} else {
							LABEL_POS_HORIZONTAL(p0, len, boxStyle);
						}
						DrawBoxedString(boxStyle, d, p0, message, rulerFp,
						                fs * d->scale, color, 0);
					}
				}
			}
		}
	} else {
		if (d->scale <= 1) {
			incr = 1;         /* @brief 16ths of an inch */
		} else if (d->scale <= 3) {
			incr = 2;         /* @brief 8ths of an inch */
		} else if (d->scale <= 5) {
			incr = 4;         /* @brief 4ths of an inch */
		} else if (d->scale <= 7) {
			incr = 8;         /* @brief half inches */
		} else if (d->scale <= 48) {
			incr = 32;
		} else {
			incr = 16;        /* @brief whole inches */
		}

		lastInch = (int)floor(end);
		lastFraction = 16;
		inch = (int)ceil(start);
		firstFraction = (((int)((inch - start) * 16)) / incr) * incr;
		if (firstFraction > 0) {
			inch--;
			firstFraction = 16 - firstFraction;
		}
		for (; inch <= lastInch; inch++) {
			if (inch % 12 == 0) {
				lengths[0] = 12;
				majorLength = 16;
				digit = (int)(inch / 12);
				fs = rulerFontSize;
				quote = '\'';
			} else if (d->scale <= 12) {
				lengths[0] = 12;
				majorLength = 16;
				digit = (int)(inch % 12);
				fs = rulerFontSize * (2.0 / 3.0);
				quote = '"';
			} else if (d->scale <= 24) {
				lengths[0] = 10;
				majorLength = 12;
				digit = (int)(inch % 12);
				fs = rulerFontSize * (1.0 / 2.0);
			} else {
				continue;
			}
			if (inch == lastInch) {
				lastFraction = (((int)((end - lastInch) * 16)) / incr) * incr;
			}
			for (fraction = firstFraction; fraction <= lastFraction;
			     fraction += incr) {
				/* @brief Suppress ticks at large zoom-out scales */
				skip = 0;
				if (d->scale > 512) {
					skip = (inch % 120 != 0);
				} else if (d->scale > 256) {
					skip = (inch % 60 != 0);
				} else if (d->scale > 128) {
					skip = (inch % 24 != 0);
				} else if (d->scale > 64) {
					skip = (inch % 12 != 0);
				}
				if (!skip) {
					Translate(&p0, orig, a, inch + fraction / 16.0);
					Translate(&p1, p0, aa, lengths[fraction] * mainD.scale / mainD.dpi);
					DrawLine(d, p0, p1, 0, color);
				}
				if (fraction == 0) {
					/* @brief Suppress labels at large zoom-out scales */
					if (d->scale <= 80) {
						skip = 2;
					} else if (d->scale <= 120) {
						skip = 5;
					} else if (d->scale <= 240) {
						skip = 10;
					} else if (d->scale <= 480) {
						skip = 20;
					} else {
						skip = 50;
					}
					if (number == TRUE &&
					    ((d->scale <= 40) || (digit % skip == 0.0))) {
						if (inch % 12 == 0 || d->scale <= 2) {
							int boxStyle = BOX_BACKGROUND;
							/* @brief Reset p0 to tick base then apply border-relative offset */
							Translate(&p0, orig, a, inch);
							if (a == 0.0) {
								LABEL_POS_VERTICAL(p0, tickSide, boxStyle);
								if (boxStyle & BOX_POS_BOTTOM_LEFT) {
									p0.y -= 1.5*(rulerFontSize / 72.0 * d->scale);
								}
							} else {
								LABEL_POS_HORIZONTAL(p0, majorLength, boxStyle);
							}
							sprintf(message, "%d%c", digit, quote);
							DrawBoxedString(boxStyle, d, p0, message, rulerFp,
							                fs * d->scale, color, 0);
						}
					}
				}
				firstFraction = 0;
			}
		}
	}

#undef LABEL_POS_VERTICAL
#undef LABEL_POS_HORIZONTAL
}

/**
 * @brief Public wrapper – draws a ruler with a white background.
 *
 * Existing callers keep their current signature unchanged.
 */
EXPORT void DrawRuler(drawCmd_p d, coOrd pos0, coOrd pos1, DIST_T offset,
                      int number, int tickSide, wDrawColor color)
{
	DrawRulerWithBackground(d, pos0, pos1, offset, number, tickSide, color,
	                        wDrawColorWhite);
}

static void DrawTicks(drawCmd_p d, coOrd size)
{
	coOrd p0, p1;
	DIST_T offset;

	offset = 0.0;

	double blank_zone = 40 * d->scale / mainD.dpi;

	if (d->orig.x < 0.0 - blank_zone) {
		p0.y = 0.0;
		p1.y = mapD.size.y;
		p0.x = p1.x = 0.0;
		DrawRulerWithBackground(d, p0, p1, offset, FALSE, TRUE, borderColor,
		                        wDrawColorGrey80);
	}
	if (d->orig.x + d->size.x > mapD.size.x + blank_zone) {
		p0.y = 0.0;
		p1.y = mapD.size.y;
		p0.x = p1.x = mapD.size.x;
		DrawRulerWithBackground(d, p0, p1, offset, FALSE, FALSE, borderColor,
		                        wDrawColorGrey80);
	}
	p0.x = 0.0;
	p1.x = d->size.x;
	offset = d->orig.x;
	p0.y = p1.y = d->orig.y;
	DrawRuler(d, p0, p1, offset, TRUE, FALSE, borderColor);
	p0.y = p1.y = d->size.y + d->orig.y;
	DrawRuler(d, p0, p1, offset, FALSE, TRUE, borderColor);

	offset = 0.0;

	if (d->orig.y < 0.0 - blank_zone) {
		p0.x = 0.0;
		p1.x = mapD.size.x;
		p0.y = p1.y = 0.0;
		DrawRulerWithBackground(d, p0, p1, offset, FALSE, FALSE, borderColor,
		                        wDrawColorGrey80);
	}
	if (d->orig.y + d->size.y > mapD.size.y + blank_zone) {
		p0.x = 0.0;
		p1.x = mapD.size.x;
		p0.y = p1.y = mapD.size.y;
		DrawRulerWithBackground(d, p0, p1, offset, FALSE, TRUE, borderColor,
		                        wDrawColorGrey80);
	}
	p0.y = 0.0;
	p1.y = d->size.y;
	offset = d->orig.y;
	p0.x = p1.x = d->orig.x;
	DrawRuler(d, p0, p1, offset, TRUE, TRUE, borderColor);
	p0.x = p1.x = d->size.x + d->orig.x;
	DrawRuler(d, p0, p1, offset, FALSE, FALSE, borderColor);
}
