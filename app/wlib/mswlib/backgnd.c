/** \file backgnd.c
* Layout background image
*/

/*  XTrkCad - Model Railroad CAD
*  Copyright (C) 2018 Martin Fischer
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

#include <windows.h>

#include <FreeImage.h>
#include "i18n.h"
#include "mswint.h"

int
wDrawSetBackground(wDraw_p bd, char * path, char ** error)
{
    FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;

    if (path) {
        // check the file signature and deduce its format
        // (the second argument is currently not used by FreeImage)
        fif = FreeImage_GetFileType(path, 0);

        if (fif == FIF_UNKNOWN) {
            // no signature ?
            // try to guess the file format from the file extension
            fif = FreeImage_GetFIFFromFilename(path);
        }

        // check that the plugin has reading capabilities ...
        if ((fif != FIF_UNKNOWN) && FreeImage_FIFSupportsReading(fif)) {
            // ok, let's load the file
            bd->background = FreeImage_Load(fif, path, 0);

            // unless a bad file format, we are done !
            if (!bd->background) {
                *error = _("Could not load the requested image file!");
                return (-1);
            } else {
                return (0);
            }
        } else {
            return (-1);
        }
    } else {
        if (bd->background) {
            FreeImage_Unload(bd->background);
            bd->background = 0;
        }

        return (1);
    }
}


void
wDrawShowBackground(wDraw_p bd, wPos_t pos_x, wPos_t pos_y, wPos_t size,
                    wAngle_t angle, int screen)
{
    if (bd->background) {
        double scale;
        FIBITMAP *tmp;
        FIBITMAP *rotated;

        if (size == 0) {
            scale = 1.0;
        } else {
            scale = (double)size / FreeImage_GetWidth(bd->background);
        }

        tmp = FreeImage_RescaleRect(bd->background,
                                    (int)((double)FreeImage_GetWidth(bd->background) * scale),
                                    (int)((double)FreeImage_GetHeight(bd->background) * scale),
                                    0,
                                    0,
                                    FreeImage_GetWidth(bd->background),
                                    FreeImage_GetHeight(bd->background),
                                    FILTER_BILINEAR,
                                    0);
        FreeImage_AdjustColors(tmp, screen, -screen, 1.0, FALSE);
        FREE_IMAGE_TYPE image_type = FreeImage_GetImageType(tmp);

        switch (image_type) {
        case FIT_BITMAP:
            switch (FreeImage_GetBPP(tmp)) {
            case 8: {
                BYTE color = 255;
                rotated = FreeImage_Rotate(tmp, angle, &color);
            }
            break;

            case 24: // we could also use 'RGBTRIPLE color' here
            case 32: {
                RGBQUAD color = { 255, 255, 255, 0 };
                // for 24-bit images, the first 3 bytes will be read
                // for 32-bit images, the first 4 bytes will be read
                rotated = FreeImage_Rotate(tmp, angle, &color);
            }
            break;
            }

            break;

        case FIT_UINT16: {
            WORD color = 255;
            rotated = FreeImage_Rotate(tmp, angle, &color);
        }
        break;

        case FIT_RGB16: // we could also use 'FIRGB16 color' here
        case FIT_RGBA16: {
            FIRGBA16 color = { 255, 255, 255, 0 };
            // for RGB16 images, the first 3 WORD will be read
            // for RGBA16 images, the first 4 WORD will be read
            rotated = FreeImage_Rotate(tmp, angle, &color);
        }
        break;

        case FIT_FLOAT: {
            float color = 1.0F;
            rotated = FreeImage_Rotate(tmp, angle, &color);
        }
        break;

        case FIT_RGBF: // we could also use 'FIRGBF color' here
        case FIT_RGBAF: {
            FIRGBAF color = { 1, 1, 1, 0 };
            // for RGBF images, the first 3 float will be read
            // for RGBAF images, the first 4 float will be read
            rotated = FreeImage_Rotate(tmp, angle, &color);
        }
        break;
        }

        SetDIBitsToDevice(bd->hDc,
                          pos_x,
                          bd->h - pos_y - FreeImage_GetHeight(rotated),
                          FreeImage_GetWidth(rotated),
                          FreeImage_GetHeight(rotated),
                          0, 0,
                          0,
                          FreeImage_GetHeight(rotated),
                          FreeImage_GetBits(rotated),
                          FreeImage_GetInfo(rotated),
                          DIB_RGB_COLORS);
        FreeImage_Unload(tmp);
        FreeImage_Unload(rotated);
    }
}