/** \file dbitmap.c
 *  Print to Bitmap
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "custom.h"
#include "dynstring.h"
#include "fileio.h"
#include "layout.h"
#include "param.h"
#include "paths.h"
#include "track.h"
#include "common-ui.h"

#ifdef WIN32
#ifdef _WIN64
#define BITMAPDIM 50000
#define BITMAPSIZE 500e6
#else
#define BITMAPDIM 32000
#define BITMAPSIZE 150e6
#endif
#else // Not WIN
#define BITMAPDIM 50000
#define BITMAPSIZE 500e6
#endif // WIN32

/** Option flags for bitmap export */
#define BITMAPDRAWTITLE 1
#define BITMAPDRAWFRAMEONLY (1<<1)
#define BITMAPDRAWCENTERLINE (1<<2)
#define BITMAPDRAWBACKGROUND (1<<3)

#define POINTSTOINCH 72.0
#define BITMAPEXPORTFONTSIZE 18
// line height is 20 percent larger than fontsize and converted to inches
#define LINEHEIGHT (BITMAPEXPORTFONTSIZE * 1.2 / POINTSTOINCH)

#define DEFAULTMARGIN 0.20

static long outputBitMapTogglesV = 3;
static double outputBitMapDensity = 10;

static struct wFilSel_t * bitmap_fs;
static wWinPix_t bitmap_w, bitmap_h;
static drawCmd_t bitmap_d = {
    NULL,
    &screenDrawFuncs,
    0,
    16.0,
    0.0,
    {0.0, 0.0}, {1.0,1.0},
    Pix2CoOrd, CoOrd2Pix
};

/**
 * Show string at given y position centered in x direction
 *
 * \param [in]	string   If non-null, the string.
 * \param 		font	 The font.
 * \param 		fontSize Size of the font.
 * \param 		yPos	 The position.
 */

static
DrawTextCenterXPosY(char *string, wFont_p font, wFontSize_t fontSize,
                    POS_T yPos)
{
    coOrd textSize;
    coOrd p;

    DrawTextSize(&mainD, string, font, fontSize * bitmap_d.scale, FALSE, &textSize);
	p.x = (bitmap_d.size.x - textSize.x) / 2.0 + bitmap_d.orig.x;
    p.y = mapD.size.y + yPos*bitmap_d.scale;
    DrawString(&bitmap_d, p, 0.0, string, font, fontSize*bitmap_d.scale,
               wDrawColorBlack);
}

/**
* Draw the product info to the bitmap
*
* \param [in]	preFix   preFix to add to product name
* \param 		fontSize Size of the font.
* \param 		yPos	 The position.
*/

static void
DrawProductInfo(char *preFix, wFontSize_t fontSize, POS_T yPos)
{
	wFont_p fp, fp_bi;
	coOrd textsize, textsize1;
	coOrd textPos;

	fp = wStandardFont(F_TIMES, FALSE, FALSE);
	fp_bi = wStandardFont(F_TIMES, TRUE, TRUE);
	DrawTextSize(&mainD, preFix, fp, fontSize * bitmap_d.scale, FALSE, &textsize);
	DrawTextSize(&mainD, sProdName, fp_bi, fontSize * bitmap_d.scale, FALSE,
				 &textsize1);
	textPos.x = (bitmap_d.size.x - (textsize.x + textsize1.x)) /
		2.0 + bitmap_d.orig.x;
	textPos.y = -LINEHEIGHT*bitmap_d.scale;
	DrawString(&bitmap_d, textPos, 0.0, preFix, fp,
		fontSize * bitmap_d.scale, wDrawColorBlack);
	textPos.x += textsize.x;
	DrawString(&bitmap_d, textPos, 0.0, sProdName, fp_bi,
		fontSize * bitmap_d.scale, wDrawColorBlack);
}

/**
 * Saves a bitmap file
 *
 * \param 		   files    number of files, must be 1
 * \param [in]     fileName name of the file
 * \param [in,out] data	    unused
 *
 * \returns true on success, false otherwise
 */

static int SaveBitmapFile(
    int files,
    char **fileName,
    void * data)
{
	bool result;

    assert(fileName != NULL);
    assert(files == 1);

    wSetCursor(mainD.d, wCursorWait);
    InfoMessage(_("Drawing tracks to bitmap"));

    SetCurrentPath(BITMAPPATHKEY, fileName[ 0 ]);

    bitmap_d.d = wBitMapCreate(bitmap_w, bitmap_h, 8);

    if (!bitmap_d.d) {
        NoticeMessage(MSG_WBITMAP_FAILED, _("Ok"), NULL);
        return false;
    }

    if ((outputBitMapTogglesV & BITMAPDRAWFRAMEONLY) ||
            (outputBitMapTogglesV & BITMAPDRAWTITLE)) {
        coOrd p[4];

        p[0].x = p[3].x = 0.0;
        p[1].x = p[2].x = mapD.size.x;
        p[0].y = p[1].y = 0.0;
        p[2].y = p[3].y = mapD.size.y;
        DrawPoly(&bitmap_d, 4, p, NULL, wDrawColorBlack, 2, DRAW_CLOSED);
    }

    if (outputBitMapTogglesV & BITMAPDRAWTITLE) {
		wFont_p fp;

        fp = wStandardFont(F_TIMES, FALSE, FALSE);
        DrawTextCenterXPosY(GetLayoutTitle(), fp, BITMAPEXPORTFONTSIZE, 1.4 * LINEHEIGHT );
        DrawTextCenterXPosY(GetLayoutSubtitle(), fp, BITMAPEXPORTFONTSIZE, 0.4 * LINEHEIGHT );
		DrawProductInfo(N_("Drawn with "), BITMAPEXPORTFONTSIZE, -LINEHEIGHT);
    }

    wDrawClip(bitmap_d.d,
              (wWinPix_t)(-bitmap_d.orig.x/bitmap_d.scale*bitmap_d.dpi),
              (wWinPix_t)(-bitmap_d.orig.y/bitmap_d.scale*bitmap_d.dpi),
              (wWinPix_t)(mapD.size.x/bitmap_d.scale*bitmap_d.dpi),
              (wWinPix_t)(mapD.size.y/bitmap_d.scale*bitmap_d.dpi));

    DrawSnapGrid(&bitmap_d, mapD.size, TRUE);

    if (outputBitMapTogglesV & BITMAPDRAWCENTERLINE) {
        bitmap_d.options |= DC_CENTERLINE;
    } else {
        bitmap_d.options &= ~DC_CENTERLINE;
    }

    DrawTracks(&bitmap_d, bitmap_d.scale, bitmap_d.orig, bitmap_d.size);

    InfoMessage(_("Writing bitmap to file"));

    if (!wBitMapWriteFile(bitmap_d.d, fileName[0])) {
        NoticeMessage(MSG_WBITMAP_FAILED, _("Ok"), NULL);
        result = false;
    } else {
		InfoMessage("");
		result = true;
	}

    wSetCursor(mainD.d, defaultCursor);
    wBitMapDelete(bitmap_d.d);
    return result;
}

/*******************************************************************************
 *
 * Output BitMap Dialog
 *
 */

static wWin_p outputBitMapW;

static char *bitmapTogglesLabels[] = { N_("Print Titles"), N_("Print Borders"),
                                       N_("Print Centerline"), NULL
                                     };
static paramFloatRange_t dpiRange = { 0.1, 100.0, 60 };

static paramData_t outputBitMapPLs[] = {
#define I_TOGGLES		(0)
    {   PD_TOGGLE, &outputBitMapTogglesV, "toggles", 0, bitmapTogglesLabels },
#define I_DENSITY		(1)
    {   PD_FLOAT, &outputBitMapDensity, "density", PDO_DLGRESETMARGIN, &dpiRange, N_("    dpi") },
#define I_MSG1			(2)
    {   PD_MESSAGE, N_("Bitmap : 99999 by 99999 pixels"), NULL, PDO_DLGRESETMARGIN|PDO_DLGUNDERCMDBUTT|PDO_DLGWIDE, I2VP(180) },
#define I_MSG2			(3)
    {   PD_MESSAGE, N_("Approximate file size: 999.9Mb"), NULL, PDO_DLGUNDERCMDBUTT | PDO_DLGBOXEND, I2VP(180) },
};

static paramGroup_t outputBitMapPG = { "outputbitmap", 0, outputBitMapPLs, COUNT(outputBitMapPLs) };

/**
 * The upper limit for the dpi setting is calculated. The limit is set
 * to make sute that the number of pixels in any direction is below the
 * maximum value.
 *
 * \param [in]		size	The size of the layout
 * \param [in]		marginX	The total size of margins in X direction
 * \param [in]		marginY	The total size of margins in Y direction
 * 
 * \returns the maximum allowed dpi value
 */

static double CalculateMaxDPI(coOrd size, POS_T marginX, POS_T marginY )
{
	POS_T maxSize;
	POS_T maxMargin;
	double maxDpi;

	if (size.x > size.y) {
		maxMargin = marginX;
		maxSize = size.x;
	}
	else {
		maxMargin = marginY;
		maxSize = size.y;
	}
	maxDpi = (BITMAPDIM - maxMargin * mainD.dpi) / maxSize;
    return(floor(maxDpi));
}

/**
 * Display the pixel size of the bitmap 
 */

static void 
OutputBitmapPixelSize(void)
{
	DynString message;
	DynStringMalloc(&message, 16);
	ParamLoadData(&outputBitMapPG);

	DynStringPrintf(&message, _("Bitmap : %ld by %ld pixels"), bitmap_w, bitmap_h);
	ParamLoadMessage(&outputBitMapPG, I_MSG1, DynStringToCStr(&message));
	DynStringFree(&message);
}

/**
* Display and return the file size of the bitmap
*
* \returns the estimated file size
*/

static FLOAT_T
OutputBitmapFileSize(void)
{
	DynString message;
	DynStringMalloc(&message, 16);
	ParamLoadData(&outputBitMapPG);
	FLOAT_T size;

	size = (FLOAT_T)bitmap_w * bitmap_h;

	if (size < 1e4) {
		DynStringPrintf(&message, _("Approximate file size : %0.0f"), size);
	}
	else if (size < 1e6) {
		DynStringPrintf(&message, _("Approximate file size : %0.1fKb"), (size + 50.0) / 1e3);
	}
	else if (size < 1e9) {
		DynStringPrintf(&message, _("Approximate file size : %0.1fMb"), (size + 5e4) / 1e6);
	}
	else {
		DynStringPrintf(&message, _("Approximate file size : %0.1fGb"), (size + 5e7) / 1e9);
	}

	ParamLoadMessage(&outputBitMapPG, I_MSG2, DynStringToCStr(&message));

	DynStringFree(&message);

	return(size);
}

/**
 * Compute size of bitmap maximum allowed dpi for given options
 */

static void ComputeBitmapSize(void)
{
    FLOAT_T Lborder=0.0, Rborder=0.0, Tborder=0.0, Bborder=0.0;

    bitmap_d.dpi = mainD.dpi;
	bitmap_d.scale = mainD.dpi / outputBitMapDensity;

    if (outputBitMapTogglesV & BITMAPDRAWFRAMEONLY) {
        Lborder = DEFAULTMARGIN;
        Rborder = DEFAULTMARGIN;
        Tborder = DEFAULTMARGIN;
        Bborder = DEFAULTMARGIN;
    }

    if (outputBitMapTogglesV & BITMAPDRAWTITLE) {
        Tborder += 2 * LINEHEIGHT;
        Bborder += LINEHEIGHT;
    }

	dpiRange.high = CalculateMaxDPI(mapD.size, Lborder + Rborder, Bborder + Tborder);

	bitmap_d.orig.x = -Lborder*bitmap_d.scale;
	bitmap_d.size.x = mapD.size.x + (Lborder + Rborder)*bitmap_d.scale;
	bitmap_d.orig.y = -Bborder*bitmap_d.scale;
	bitmap_d.size.y = mapD.size.y + (Bborder + Tborder)*bitmap_d.scale;

	bitmap_w = (wWinPix_t)(bitmap_d.size.x / bitmap_d.scale*bitmap_d.dpi);
	bitmap_h = (wWinPix_t)(bitmap_d.size.y / bitmap_d.scale*bitmap_d.dpi);
}

/**
 * Update the dialog for bitmap settings by calculated the new dimensions
 * and updating the dialog
 */

void
UpdateBitmapDialog(void)
{
	ComputeBitmapSize();
	if (outputBitMapDensity > dpiRange.high) {
		ParamDialogOkActive(&outputBitMapPG, false);
	}
	else {
		ParamDialogOkActive(&outputBitMapPG, true);
	}
	OutputBitmapPixelSize();
	OutputBitmapFileSize();
}

/**
 * Check input from bitmap options dialog and trigger file name selection
 *
 * \param [in,out] junk If non-null, the junk.
 */

static void OutputBitMapOk(void * unused)
{
    FLOAT_T size;

    if (bitmap_w > BITMAPDIM || bitmap_h > BITMAPDIM) {
        NoticeMessage(MSG_BITMAP_TOO_LARGE, _("Ok"), NULL);
        return;
    }

	size = OutputBitmapFileSize();

    if (size > BITMAPSIZE) {
        if (NoticeMessage(MSG_BITMAP_SIZE_WARNING, _("Continue"), _("Cancel"))==0) {
            return;
        }
    }

    wHide(outputBitMapW);

    if (!bitmap_fs) {
        bitmap_fs = wFilSelCreate(mainW, FS_SAVE, 0, _("Save Bitmap"),
                                  _("Bitmap files (*.png)|*.png"),
                                  SaveBitmapFile, NULL);
    }

    wFilSelect(bitmap_fs, GetCurrentPath(BITMAPPATHKEY));
}

/**
 * Handle changes for bitmap export. Only changes relevant here are
 * changes to the map.
 *
 * \param  changes The changes.
 */

static void OutputBitMapChange(long changes)
{
    if ((changes & CHANGE_MAP) && outputBitMapW) {
        ParamLoadControls(&outputBitMapPG);
        ComputeBitmapSize();
    }

    return;
}

/**
 * Executes the output bit map operation
 *
 * \param [in,out] unused.
 */

static void DoOutputBitMap(void * unused)
{
    if (outputBitMapW == NULL) {
        outputBitMapW = ParamCreateDialog(&outputBitMapPG,
                                          MakeWindowTitle(_("Export to bitmap")),
                                          _("Ok"),
                                          OutputBitMapOk,
                                          wHide,
                                          TRUE,
                                          NULL,
                                          0,
                                          (paramChangeProc)UpdateBitmapDialog);
    }
    ParamLoadControls(&outputBitMapPG);
    ParamGroupRecord(&outputBitMapPG);

	UpdateBitmapDialog();

	if (dpiRange.high > outputBitMapDensity) {
		outputBitMapDensity = dpiRange.high;
		ParamLoadControl(&outputBitMapPG, I_DENSITY );
	}

    wShow(outputBitMapW);
}

/**
 * Initialize bitmap output
 *
 * \returns entry point for bitmap export
 */

EXPORT addButtonCallBack_t OutputBitMapInit(void)
{
    ParamRegister(&outputBitMapPG);
    RegisterChangeNotification(OutputBitMapChange);
    return &DoOutputBitMap;
}
