/** \file print.c
 * Printing functions using GTK's print API
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2015 Martin Fischer
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


#include "wlib.h"
#define TRUE 1
#define FALSE 0
/**
 * Page setup function. Previous settings are loaded and the setup
 * dialog is shown. The settings are saved after the dialog ends.
 *
 * \param callback IN unused
 */

void wPrintSetup(wPrintSetupCallBack_p callback)
{
    printf("Not yet implemented wPrintSetup() %s:%d\n", __FILE__, __LINE__);
}

/*****************************************************************************
 *
 * 
 *
 */


const char * wPrintGetName()
{
    printf("Not yet implemented wPrintGetName() %s:%d\n", __FILE__, __LINE__);
    return(NULL);
}
/*****************************************************************************
 *
 * BASIC PRINTING
 *
 */


/**
 * Create clipping rectangle.
 *
 * \param x, y IN starting position
 * \param w, h IN width and height of rectangle
 * \return
 */

void wPrintClip(wDrawPix_t x, wDrawPix_t y, wDrawPix_t w, wDrawPix_t h)
{
    printf("Not yet implemented wPrintClip() %s:%d\n", __FILE__, __LINE__);
}

/*****************************************************************************
 *
 * PAGE FUNCTIONS
 *
 */

/**
 * Get the paper size. The size returned is the printable area of the
 * currently selected paper, ie. the physical size minus the margins.
 * \param w OUT printable width of the paper in inches
 * \param h OUT printable height of the paper in inches
 * \return
 */


void wPrintGetMargins(
	double * tMargin,
	double * rMargin,
	double * bMargin,
	double * lMargin )
{
    printf("Not yet implemented wPrintGetMargins() %s:%d\n", __FILE__, __LINE__);
}

/**
 * Get the paper size. The size returned is the physical size of the
 * currently selected paper.
 * \param w OUT physical width of the paper in inches
 * \param h OUT physical height of the paper in inches
 * \return
 */

void wPrintGetPageSize(
    double * w,
    double * h)
{
    printf("Not yet implemented wPrintGetPageSize() %s:%d\n", __FILE__, __LINE__);
}



/**
 * Initialize new page.
 * The cairo_save() / cairo_restore() cycle was added to solve problems
 * with a multi page print operation. This might actually be a bug in
 * cairo but I didn't examine that any further.
 *
 * \return   print context for the print operation
 */
wDraw_p wPrintPageStart(void)
{
    printf("Not yet implemented wPrintPageStart() %s:%d\n", __FILE__, __LINE__);
    return(NULL);
}

/**
 * End of page. This function returns the contents of printContinue. The
 * caller continues printing as long as TRUE is returned. Setting
 * printContinue to FALSE in an asynchronous handler therefore cleanly
 * terminates a print job at the end of the page.
 *
 * \param p IN ignored
 * \return    always printContinue
 */


wBool_t wPrintPageEnd(wDraw_p p)
{
        printf("Not yet implemented wPrintPageEnd() %s:%d\n", __FILE__, __LINE__);
        return(FALSE);
}

/*****************************************************************************
 *
 * PRINT START/END
 *
 */


/**
 * Start a new document
 *
 * \param title IN title of document ( name of layout )
 * \param fTotalPageCount IN number of pages to print (unused)
 * \param copiesP OUT ???
 * \return TRUE if successful, FALSE if cancelled by user
 */

wBool_t wPrintDocStart(const char * title, int fTotalPageCount, int * copiesP)
{
    printf("Not yet implemented wPrintDocStart() %s:%d\n", __FILE__, __LINE__);
    return(FALSE);
}

/**
 * Finish the print operation
 * \return
 */

void wPrintDocEnd(void)
{
        printf("Not yet implemented wPrintDocEnd() %s:%d\n", __FILE__, __LINE__);
}


wBool_t wPrintQuit(void)
{
    return FALSE;
}


wBool_t wPrintInit(void)
{
    return TRUE;
}
