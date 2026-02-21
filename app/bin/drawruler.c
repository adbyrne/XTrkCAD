/** @file drawruler.c
 * @brief Ruler rendering and border decoration for the main drawing window.
 *
 * Owns the ruler font, the lborder/bborder/charWidth metrics, and all
 * functions that draw the tick-mark strips around the canvas edge.
 * DrawRulerInit() must be called once from DrawInit() before any drawing
 * occurs.
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
#include "cselect.h"
#include "custom.h"
#include "fileio.h"
#include "form.h"
#include "icons.h"
#include "layout.h"
#include "mapwindow.h"
#include "misc.h"
#include "track.h"

#include "drawruler.h"

/* @brief Suppress the header-file #define so we can use lowercase variables. */
#undef LBORDER
#undef BBORDER

/** @brief Left border width in pixels, set by DrawRulerInit(). */
int lborder;

/** @brief Bottom border height in pixels, set by DrawRulerInit(). */
int bborder;

/** @brief Single character width in pixels for the ruler font. */
int charWidth;

/** @brief Monospace font used for ruler tick labels. */
static wFont_p rulerFp;

/** @brief Point size for ruler label text. */
static DIST_T rulerFontSize = 12.0;

/** @brief Maximum number of characters in a ruler label ("300.0" = 5). */
#define MAXLABELCHARS 5

static void DrawTicks(drawCmd_p d, coOrd size);

/**
 * @brief Initialize ruler font metrics and compute border sizes.
 * @param d  Main drawing widget, used for Pango font metric queries.
 */
void DrawRulerInit(wDraw_p d) {
    rulerFp  = wStandardFont(F_MONO, FALSE, FALSE);
    charWidth = wFontGetCharWidth(d, rulerFp, rulerFontSize);
    lborder   = charWidth * MAXLABELCHARS + 8;
    bborder   = wFontGetCharHeight(d, rulerFp, rulerFontSize) + 8;
}

/*****************************************************************************
 *
 * ROOM WALLS AND MARKERS
 *
 */

/**
 * @brief Draw the room boundary walls and, optionally, the ruler strips.
 * @param drawBackground  TRUE to fill the background before drawing walls.
 */
void DrawRoomWalls(wBool_t drawBackground) {
  if (mainD.d == NULL) {
    return;
  }

  coOrd p0, p1;

  if (drawBackground) {
    // @brief Fill the entire canvas area including border strips
    p0.x = mainD.orig.x - (lborder + RBORDER) / mainD.dpi * mainD.scale;
    p0.y = mainD.orig.y - (bborder + TBORDER) / mainD.dpi * mainD.scale;
    p1.x = mainD.size.x + (lborder + RBORDER) / mainD.dpi * mainD.scale;
    p1.y = mainD.size.y + (bborder + TBORDER) / mainD.dpi * mainD.scale;
    DrawRectangle(&mainD, p0, p1, drawColorGrey90, DRAW_FILL);
    // @brief Draw the usable canvas area in white
    DrawRectangle(&mainD, mainD.orig, mainD.size, wDrawColorWhite, DRAW_FILL);
  }

  // @brief Draw the four room boundary walls
  p0.x = 0.0;
  p0.y = 0.0;
  p1.x = mapD.size.x;
  p1.y = 0.0;
  DrawLine(&mainD, p0, p1, 3, borderColor);
  p0.x = mapD.size.x;
  p0.y = mapD.size.y;
  DrawLine(&mainD, p0, p1, 3, borderColor);
  p1.x = 0.0;
  p1.y = mapD.size.y;
  DrawLine(&mainD, p0, p1, 3, borderColor);
  p0.x = 0.0;
  p0.y = 0.0;
  DrawLine(&mainD, p0, p1, 3, borderColor);

  if (drawBackground) {
    return;
  }

  // @brief Draw the ruler strips on all four edges
  DrawTicks(&mainD, mainD.size);
}

/**
 * @brief Draw the cross-hair position markers on the canvas.
 */
void DrawMarkers(void) {
  coOrd pos;
  DIST_T d;
  if (mainD.d == NULL) {
    return;
  }
  pos = oldMarker;
  d = 0.1 * mainD.scale;

  coOrd p0, p1;
  p0.x = pos.x - d;
  p0.y = pos.y;
  p1.x = pos.x + d;
  p1.y = pos.y;
  DrawLine(&tempD, p0, p1, 0, markerColor);
  p0.x = pos.x;
  p0.y = pos.y - d;
  p1.x = pos.x;
  p1.y = pos.y + d;
  DrawLine(&tempD, p0, p1, 0, markerColor);
}

/*****************************************************************************
 *
 * RULER DRAWING
 *
 */

/**
 * @brief Draw tick marks and labels along a single ruler edge.
 *
 * @param d         Drawing context.
 * @param pos0      World coordinate of ruler start point.
 * @param pos1      World coordinate of ruler end point.
 * @param offset    Offset from pos0/pos1 toward the ruler strip centre.
 * @param number    TRUE to draw numeric labels at major ticks.
 * @param tickSide  TRUE if ticks point in the -90 direction from the ruler line.
 * @param color     Color used for lines and labels.
 */
EXPORT void DrawRuler(drawCmd_p d, coOrd pos0, coOrd pos1, DIST_T offset,
                      int number, int tickSide, wDrawColor color) {
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
                             2.0, 4.0, 2.0, 6.0, 2.0, 4.0, 2.0, 0.0};
  int fraction, incr, firstFraction, lastFraction;
  int majorLength;
  coOrd p0, p1;

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

  DrawLine(d, pos0, pos1, 3, wDrawColorWhite);
  DrawLine(d, pos0, pos1, 0, color);

  /**
   * @brief Position label anchor for a vertical ruler (a == 0.0).
   *
   * Translates in @p aa direction by (lborder - 2) pixels so the anchor
   * sits 2px from the far edge of the border strip.
   *   tickSide=TRUE  (left ruler)  -> BOX_POS_LEFT_CENTER
   *   tickSide=FALSE (right ruler) -> BOX_POS_BOTTOM_LEFT
   */
#define LABEL_POS_VERTICAL(p0, tickSide, boxStyle)                             \
  do {                                                                          \
    Translate(&(p0), (p0), aa, (lborder - 2) * d->scale / mainD.dpi);         \
    Translate(&(p0), (p0), a, rulerFontSize / 72.0 * d->scale);               \
    (boxStyle) |= ((tickSide) ? BOX_POS_LEFT_CENTER : BOX_POS_BOTTOM_LEFT);   \
  } while (0)

  /**
   * @brief Position label anchor for a horizontal ruler (a == 90.0).
   *
   * Translates in @p aa direction to vertically centre the label in the
   * space between the tick tip and the border edge, then shifts right by
   * one character width and up by 6 pixels.
   */
#define LABEL_POS_HORIZONTAL(p0, tickLen, boxStyle)                            \
  do {                                                                          \
    Translate(&(p0), (p0), aa,                                                 \
              ((bborder + (tickLen)) / 2.0) * d->scale / mainD.dpi);           \
    Translate(&(p0), (p0), aa, -6.0 * d->scale / mainD.dpi);                  \
    Translate(&(p0), (p0), a, (double)charWidth * d->scale / mainD.dpi);      \
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

    // Label interval for scale > 40
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
          Translate(&p1, p0, aa, len * d->scale / mainD.dpi);
          DrawLine(d, p0, p1, 3, wDrawColorWhite);
          DrawLine(d, p0, p1, 0, color);
          if (!number || (d->scale > 40 && mm % skip != 0.0)) {
            continue;
          }
          if ((power >= 1000) || (d->scale <= 8 && power >= 100) ||
              (d->scale <= 1 && power >= 10)) {
            int boxStyle = BOX_BACKGROUND;
            if (mm % 100 != 0) {
              // @brief Sub-metre digit label at smaller font size
              sprintf(message, "%ld", mm / 10 % 10);
              fs = rulerFontSize * 2 / 3;
            } else {
              // @brief Main metre label formatted as e.g. "1.5"
              sprintf(message, "%0.1f", mm / 1000.0);
              fs = rulerFontSize;
            }
            if (a == 0.0) {
              LABEL_POS_VERTICAL(p0, tickSide, boxStyle);
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
      incr = 1;         // @brief 16ths of an inch
    } else if (d->scale <= 3) {
      incr = 2;         // @brief 8ths of an inch
    } else if (d->scale <= 5) {
      incr = 4;         // @brief 4ths of an inch
    } else if (d->scale <= 7) {
      incr = 8;         // @brief half inches
    } else if (d->scale <= 48) {
      incr = 32;
    } else {
      incr = 16;        // @brief whole inches
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
        // @brief Suppress ticks at large zoom-out scales
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
          Translate(&p1, p0, aa, lengths[fraction] * d->scale / mainD.dpi);
          DrawLine(d, p0, p1, 3, wDrawColorWhite);
          DrawLine(d, p0, p1, 0, color);
        }
        if (fraction == 0) {
          // @brief Suppress labels at large zoom-out scales
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
              // @brief Reset p0 to tick base then apply border-relative offset
              Translate(&p0, orig, a, inch);
              if (a == 0.0) {
                LABEL_POS_VERTICAL(p0, tickSide, boxStyle);
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
 * @brief Draw the ruler tick strips on all four edges of the main canvas.
 * @param d     Drawing context.
 * @param size  Unused; present for historical compatibility.
 */
static void DrawTicks(drawCmd_p d, coOrd size) {
    coOrd p0, p1;
    DIST_T offset;

    offset = 0.0;

    double blank_zone = 40 * d->scale / mainD.dpi;

    if (d->orig.x < 0.0 - blank_zone) {
      p0.y = 0.0;
      p1.y = mapD.size.y;
      p0.x = p1.x = 0.0;
      DrawRuler(d, p0, p1, offset, FALSE, TRUE, borderColor);
    }
    if (d->orig.x + d->size.x > mapD.size.x + blank_zone) {
      p0.y = 0.0;
      p1.y = mapD.size.y;
      p0.x = p1.x = mapD.size.x;
      DrawRuler(d, p0, p1, offset, FALSE, FALSE, borderColor);
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
      DrawRuler(d, p0, p1, offset, FALSE, FALSE, borderColor);
    }
    if (d->orig.y + d->size.y > mapD.size.y + blank_zone) {
      p0.x = 0.0;
      p1.x = mapD.size.x;
      p0.y = p1.y = mapD.size.y;
      DrawRuler(d, p0, p1, offset, FALSE, TRUE, borderColor);
    }
    p0.y = 0.0;
    p1.y = d->size.y;
    offset = d->orig.y;
    p0.x = p1.x = d->orig.x;
    DrawRuler(d, p0, p1, offset, TRUE, TRUE, borderColor);
    p0.x = p1.x = d->size.x + d->orig.x;
    DrawRuler(d, p0, p1, offset, FALSE, FALSE, borderColor);
}
