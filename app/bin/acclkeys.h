/** \file acclkeys.h
 * Definition of accelerator keys
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

/*
 * use 'sort +2 acclkeys.h' to check usage
 */

#ifndef ACCLKEYS_H
#define ACCLKEYS_H

/* commands */
#define ACCL_DESCRIBE	(WCTL+'?')
#define ACCL_SELECT		(WCTL+'e')
#define ACCL_STRAIGHT	(WCTL+'g')
#define ACCL_CURVE1		(WCTL+'4')
#define ACCL_CURVE2		(WCTL+'5')
#define ACCL_CURVE3		(WCTL+'6')
#define ACCL_CURVE4		(WCTL+'7')
#define ACCL_CIRCLE1	(WCTL+'8')
#define ACCL_CIRCLE2	(WCTL+'9')
#define ACCL_CIRCLE3	(WCTL+'0')
#define ACCL_TURNOUT	(WCTL+'t')
#define ACCL_TURNTABLE	(WCTL+WSHIFT+'n')
#define ACCL_PARALLEL	(WCTL+WSHIFT+'p')
#define ACCL_MOVE		(WCTL+WSHIFT+'m')
#define ACCL_ROTATE		(WCTL+WSHIFT+'r')
#define ACCL_FLIP		(0)
#define ACCL_MOVEDESC	(WCTL+WSHIFT+'z')
#define ACCL_MODIFY		(WCTL+'m')
#define ACCL_JOIN		(WCTL+'j')
#define ACCL_CONNECT	(WCTL+WSHIFT+'j')
#define ACCL_HELIX		(WCTL+WSHIFT+'h')
#define ACCL_SPLIT		(WCTL+WSHIFT+'s')
#define ACCL_ELEVATION	(WCTL+WSHIFT+'e')
#define ACCL_PROFILE	(WCTL+WSHIFT+'f')
#define ACCL_DELETE		(WCTL+'d')
#define ACCL_TUNNEL		(WCTL+WSHIFT+'t')
#define ACCL_HNDLDTO	(WCTL+WSHIFT+'i')
#define ACCL_TEXT		(WCTL+WSHIFT+'x')
#define ACCL_DRAWLINE	(WCTL+WSHIFT+'1')
#define ACCL_DRAWDIMLINE		(WCTL+WSHIFT+'d')
#define ACCL_DRAWBENCH	(WCTL+'b')
#define ACCL_DRAWTBLEDGE		(WCTL+WSHIFT+'3')
#define ACCL_DRAWCURVE1 (WCTL+WSHIFT+'4')
#define ACCL_DRAWCURVE2 (WCTL+WSHIFT+'5')
#define ACCL_DRAWCURVE3 (WCTL+WSHIFT+'6')
#define ACCL_DRAWCURVE4 (WCTL+WSHIFT+'7')
#define ACCL_DRAWCIRCLE1		(WCTL+WSHIFT+'8')
#define ACCL_DRAWCIRCLE2		(WCTL+WSHIFT+'9')
#define ACCL_DRAWCIRCLE3		(WCTL+WSHIFT+'0')
#define ACCL_DRAWFILLCIRCLE1	(WALT+WCTL+'8')
#define ACCL_DRAWFILLCIRCLE2	(WALT+WCTL+'9')
#define ACCL_DRAWFILLCIRCLE3	(WALT+WCTL+'0')
#define ACCL_DRAWBOX	(WCTL+WSHIFT+'[')
#define ACCL_DRAWFILLBOX		(WALT+WCTL+'[')
#define ACCL_DRAWPOLYLINE		(WCTL+WSHIFT+'2')
#define ACCL_DRAWPOLYGON		(WALT+WCTL+'2')
#define ACCL_NOTE		(WALT+WCTL+'n')
#define ACCL_STRUCTURE	(WCTL+WSHIFT+'c')
#define ACCL_ABOVE		(WCTL+WSHIFT+'b')
#define ACCL_BELOW		(WCTL+WSHIFT+'w')
#define ACCL_RULER		(0)

/* fileM */
#define ACCL_NEW		(WCTL+'n')
#define ACCL_OPEN		(WCTL+'o')
#define ACCL_SAVE		(WCTL+'s')
#define ACCL_SAVEAS		(WCTL+'a')
#define ACCL_REVERT  (0)
#define ACCL_PARAMFILES (WALT+WCTL+'s')
#define ACCL_PRICELIST	(WALT+WCTL+'q')
#define ACCL_PRINT		(WCTL+'p')
#define ACCL_PRINTSETUP (0)
#define ACCL_PRINTBM	(WCTL+WSHIFT+'q')
#define ACCL_PARTSLIST	(WALT+WCTL+'l')
#define ACCL_NOTES		(WALT+WCTL+'t')
#define ACCL_REGISTER	(0)

/* editM */
#define ACCL_UNDO		(WCTL+'z')
#define ACCL_REDO		(WCTL+'r')
#define ACCL_COPY		(WCTL+'c')
#define ACCL_CUT		(WCTL+'x')
#define ACCL_PASTE		(WCTL+'v')
#define ACCL_SELECTALL	(WCTL+WSHIFT+'a')
#define ACCL_DESELECTALL	(0)
#define ACCL_THIN		(WCTL+'1')
#define ACCL_MEDIUM		(WCTL+'2')
#define ACCL_THICK		(WCTL+'3')
#define ACCL_EXPORT		(WALT+WCTL+'x')
#define ACCL_IMPORT		(WALT+WCTL+'i')
#define ACCL_EXPORTDXF	(0)
#define ACCL_LOOSEN		(WCTL+WSHIFT+'k')
#define ACCL_GROUP		(WCTL+WSHIFT+'g')
#define ACCL_UNGROUP	(WCTL+WSHIFT+'u')
#define ACCL_CUSTMGM	(WALT+WCTL+'u')
#define ACCL_CARINV		(WALT+WCTL+'v')
#define ACCL_LAYERS		(WALT+WCTL+'y')
#define ACCL_SETCURLAYER		(0)
#define ACCL_MOVCURLAYER		(0)
#define ACCL_CLRELEV	(0)
#define ACCL_CHGELEV	(0)

/* viewM */
#define ACCL_REDRAW		(WCTL+'l')
#define ACCL_REDRAWALL	(WCTL+WSHIFT+'l')
#define ACCL_ZOOMIN		(WCTL+'+')
#define ACCL_ZOOMOUT	(WCTL+'-')
#define ACCL_SNAPSHOW	(WCTL+']')
#define ACCL_SNAPENABLE (WCTL+'[')
#define ACCL_MAPSHOW	(WCTL+' ')

/* optionsM */
#define ACCL_LAYOUTW	(WALT+WCTL+'a')
#define ACCL_DISPLAYW	(WALT+WCTL+'d')
#define ACCL_CMDOPTW	(WALT+WCTL+'m')
#define ACCL_EASEW		(WALT+WCTL+'e')
#define ACCL_FONTW		(WALT+WCTL+'f')
#define ACCL_GRIDW		(WALT+WCTL+'g')
#define ACCL_STICKY		(WALT+WCTL+'k')
#define ACCL_PREFERENCES		(WALT+WCTL+'p')
#define ACCL_COLORW		(WALT+WCTL+'c')

/* macroM */
#define ACCL_RECORD		(WALT+WCTL+'r')
#define ACCL_PLAYBACK	(WALT+WCTL+'b')

#define ACCL_BRIDGE		(0)

/* Blocks */
#define ACCL_BLOCK1	(WALT+WSHIFT+'b')
#define ACCL_BLOCK2	(WALT+WCTL+WSHIFT+'b')
#define ACCL_BLOCK3	(0)
/* Switch Motors */
#define ACCL_SWITCHMOTOR1 (WSHIFT+'s')
#define ACCL_SWITCHMOTOR2 (WALT+WSHIFT+'s')
#define ACCL_SWITCHMOTOR3 (0)

#endif
