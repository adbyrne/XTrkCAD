/** \file csnap.c
 * Draw Snap Grid
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

#include <math.h>

#include "custom.h"
#include "fileio.h"
#include "i18n.h"
#include "messages.h"
#include "param.h"
#include "track.h"
#include "utility.h"

#define bigdot_width 3

/*****************************************************************************
 *
 
 *
 */

EXPORT long minGridSpacing = 3;

#define CROSSTICK
#ifdef CROSSTICK
#include "bitmaps/cross0.xbm"
static wDrawBitMap_p cross0_bm;
#endif

#define DEFAULTGRIDSPACING (1.0)

EXPORT void MapGrid(
		coOrd orig,
		coOrd size,
		ANGLE_T angle,
		coOrd gridOrig,
		ANGLE_T gridAngle,
		POS_T Xspacing,
		POS_T Yspacing,
		int * x0,
		int * x1,
		int * y0,
		int * y1 )
{
	coOrd p[4], hi, lo;
	int i;

	p[0] = p[1] = p[2] = p[3] = orig;
	p[1].x += size.x;
	p[2].x += size.x;
	p[2].y += size.y;
	p[3].y += size.y;
	for (i=1; i<4; i++) {
		Rotate( &p[i], orig, angle );
	}
	for (i=0; i<4; i++) {
		p[i].x -= gridOrig.x;
		p[i].y -= gridOrig.y;
		Rotate( &p[i], zero, -gridAngle );
	}
	hi = lo = p[0];
	for (i=1; i<4; i++) {
		if (hi.x < p[i].x)
			hi.x = p[i].x;
		if (hi.y < p[i].y)
			hi.y = p[i].y;
		if (lo.x > p[i].x)
			lo.x = p[i].x;
		if (lo.y > p[i].y)
			lo.y = p[i].y;
	}
	*x0 = (int)floor( lo.x / Xspacing );
	*y0 = (int)floor( lo.y / Yspacing );
	*x1 = (int)ceil( hi.x / Xspacing );
	*y1 = (int)ceil( hi.y / Yspacing );
}


static DIST_T Gdx, Gdy, Ddx, Ddy;
static coOrd GDorig;
static wPos_t lborder, bborder;

void static DrawGridPoint(
		drawCmd_p D,
		wDrawColor Color,
		coOrd orig,
		coOrd * size,
		DIST_T dpi,
		coOrd p0,
		BOOL_T bigdot )
{
	wPos_t x0, y0;
	POS_T x;
	x = (p0.x*Gdx + p0.y*Gdy) + orig.x;
	p0.y = (p0.y*Gdx - p0.x*Gdy) + orig.y;
	p0.x = x;
	if (size &&
		( p0.x < 0.0 || p0.x > size->x ||
		  p0.y < 0.0 || p0.y > size->y ) )
		return;
	p0.x -= D->orig.x;
	p0.y -= D->orig.y;
	x = (p0.x*Ddx + p0.y*Ddy);
	p0.y = (p0.y*Ddx - p0.x*Ddy);
	p0.x = x;
	if ( p0.x < 0.0 || p0.x > D->size.x ||
		 p0.y < 0.0 || p0.y > D->size.y )
		return;
	x0 = (wPos_t)(p0.x*dpi+0.5) + lborder;
	y0 = (wPos_t)(p0.y*dpi+0.5) + bborder;
	if ( bigdot )
		wDrawFilledCircle(D->d, x0, y0, (wPos_t)(bigdot_width+0.5)/2, Color, (wDrawOpts)D->funcs->options );
	else
		wDrawPoint( D->d, x0, y0, Color, (wDrawOpts)D->funcs->options );
}


static void DrawGridLine(
		drawCmd_p D,
		wDrawColor Color,
		coOrd orig,
		coOrd * size,
		DIST_T dpi,
		BOOL_T clip,
		coOrd p0,
		coOrd p1 )
{
	wPos_t x0, y0, x1, y1;
	POS_T x;
	x = (p0.x*Gdx + p0.y*Gdy) + orig.x;
	p0.y = (p0.y*Gdx - p0.x*Gdy) + orig.y;
	p0.x = x;
	x = (p1.x*Gdx + p1.y*Gdy) + orig.x;
	p1.y = (p1.y*Gdx - p1.x*Gdy) + orig.y;
	p1.x = x;
	if (size && clip && !ClipLine( &p0, &p1, zero, 0.0, *size ))
		return;
	p0.x -= D->orig.x;
	p0.y -= D->orig.y;
	p1.x -= D->orig.x;
	p1.y -= D->orig.y;
	x = (p0.x*Ddx + p0.y*Ddy);
	p0.y = (p0.y*Ddx - p0.x*Ddy);
	p0.x = x;
	x = (p1.x*Ddx + p1.y*Ddy);
	p1.y = (p1.y*Ddx - p1.x*Ddy);
	p1.x = x;
	if (clip && !ClipLine( &p0, &p1, zero, 0.0, D->size ))
		return;
	x0 = (wPos_t)(p0.x*dpi+0.5) + lborder;
	y0 = (wPos_t)(p0.y*dpi+0.5) + bborder;
	x1 = (wPos_t)(p1.x*dpi+0.5) + lborder;
	y1 = (wPos_t)(p1.y*dpi+0.5) + bborder;
	wDrawLine( D->d, x0, y0, x1, y1, 0, wDrawLineSolid, Color, (wDrawOpts)D->funcs->options );
}


#ifdef WINDOWS
#define WONE (1)
#else
#define WONE (0)
#endif

EXPORT void DrawGrid(
		drawCmd_p D,
		coOrd * size,
		POS_T hMajSpacing,
		POS_T vMajSpacing,
		long Hdivision,
		long Vdivision,
		coOrd Gorig,
		ANGLE_T Gangle,
		wDrawColor Color,
		BOOL_T clip )
{
	int hMaj, hMajCnt0, hMajCnt1, vMaj, vMajCnt0, vMajCnt1;
	coOrd p0, p1;
	DIST_T dpi;
	int hMin, hMinCnt1, vMin, vMinCnt1;
	DIST_T hMinSpacing=0, vMinSpacing=0;
	long f;
	POS_T hMajSpacing_dpi, vMajSpacing_dpi;
	BOOL_T bigdot;

	if (hMajSpacing <= 0 && vMajSpacing <= 0)
		return;

#ifdef CROSSTICK
	if (!cross0_bm)
		cross0_bm = wDrawBitMapCreate( mainD.d, cross0_width, cross0_height, 2, 2, cross0_bits );
#endif

	wSetCursor( wCursorWait );
	dpi = D->dpi/D->scale;
	Gdx = cos(D2R(Gangle));
	Gdy = sin(D2R(Gangle));
	Ddx = cos(D2R(-D->angle));
	Ddy = sin(D2R(-D->angle));
	if (D->options&DC_TICKS) {
		lborder = LBORDER;
		bborder = BBORDER;
	} else {
		lborder = bborder = 0;
	}
	GDorig.x = Gorig.x-D->orig.x;
	GDorig.y = Gorig.y-D->orig.y;
	hMajSpacing_dpi = hMajSpacing*dpi;
	vMajSpacing_dpi = vMajSpacing*dpi;

	MapGrid( D->orig, D->size, D->angle, Gorig, Gangle,
				(hMajSpacing>0?hMajSpacing:vMajSpacing),
				(vMajSpacing>0?vMajSpacing:hMajSpacing),
				&hMajCnt0, &hMajCnt1, &vMajCnt0, &vMajCnt1 );

	hMinCnt1 = vMinCnt1 = 0;

	if (hMajSpacing_dpi >= minGridSpacing) {
		p0.y = vMajCnt0*(vMajSpacing>0?vMajSpacing:hMajSpacing);
		p1.y = vMajCnt1*(vMajSpacing>0?vMajSpacing:hMajSpacing);
		p0.x = p1.x = hMajCnt0*hMajSpacing;
		for ( hMaj=hMajCnt0; hMaj<hMajCnt1; hMaj++ ) {
			p0.x += hMajSpacing;
			p1.x += hMajSpacing;
			DrawGridLine( D, Color, Gorig, size, dpi, clip, p0, p1 );
		}
		if ( Hdivision > 0 ) {
			hMinSpacing = hMajSpacing/Hdivision;
			if (hMinSpacing*dpi > minGridSpacing)
				hMinCnt1 = (int)Hdivision;
		}
	}

	if (vMajSpacing_dpi >= minGridSpacing) {
		p0.x = hMajCnt0*(hMajSpacing>0?hMajSpacing:vMajSpacing);
		p1.x = hMajCnt1*(hMajSpacing>0?hMajSpacing:vMajSpacing);
		p0.y = p1.y = vMajCnt0*vMajSpacing;
		for ( vMaj=vMajCnt0; vMaj<vMajCnt1; vMaj++ ) {
			p0.y += vMajSpacing;
			p1.y += vMajSpacing;
			DrawGridLine( D, Color, Gorig, size, dpi, clip, p0, p1 );
		}
		if ( Vdivision > 0 ) {
			vMinSpacing = vMajSpacing/Vdivision;
			if (vMinSpacing*dpi > minGridSpacing)
				vMinCnt1 = (int)Vdivision;
		}
	}

	if (hMinCnt1 <= 0 && vMinCnt1 <= 0)
		goto done;

	if (hMajSpacing <= 0) {
		hMinCnt1 = vMinCnt1+1;
		hMinSpacing = vMinSpacing;
		hMajSpacing = vMajSpacing;
	} else if (hMajSpacing_dpi < minGridSpacing) {
		hMinCnt1 = 1;
		hMinSpacing = 0;
		f = (long)ceil(minGridSpacing/hMajSpacing);
		hMajSpacing *= f;
		hMajCnt0 = (int)(hMajCnt0>=0?ceil(hMajCnt0/f):floor(hMajCnt0/f));
		hMajCnt1 = (int)(hMajCnt1>=0?ceil(hMajCnt1/f):floor(hMajCnt1/f));
	} else if (Hdivision <= 0) {
		hMinCnt1 = (int)(hMajSpacing/vMinSpacing);
		if (hMinCnt1 <= 0) {
			goto done;
		}
		hMinSpacing = hMajSpacing/hMinCnt1;
	} else if (hMinSpacing*dpi < minGridSpacing) {
		f = (long)ceil(minGridSpacing/hMinSpacing);
		hMinCnt1 = (int)(Hdivision/f);
		hMinSpacing *= f;
	}

	if (vMajSpacing <= 0) {
		vMinCnt1 = hMinCnt1+1;
		vMinSpacing = hMinSpacing;
		vMajSpacing = hMajSpacing;
	} else if (vMajSpacing_dpi < minGridSpacing) {
		vMinCnt1 = 1;
		vMinSpacing = 0;
		f = (long)ceil(minGridSpacing/vMajSpacing);
		vMajSpacing *= f;
		vMajCnt0 = (int)(vMajCnt0>=0?ceil(vMajCnt0/f):floor(vMajCnt0/f));
		vMajCnt1 = (int)(vMajCnt1>=0?ceil(vMajCnt1/f):floor(vMajCnt1/f));
	} else if (Vdivision <= 0) {
		vMinCnt1 = (int)(vMajSpacing/hMinSpacing);
		if (vMinCnt1 <= 0) {
			goto done;
		}
		vMinSpacing = vMajSpacing/vMinCnt1;
	} else if (vMinSpacing*dpi < minGridSpacing) {
		f = (long)ceil(minGridSpacing/vMinSpacing);
		vMinCnt1 = (int)(Vdivision/f);
		vMinSpacing *= f;
	}
    
	bigdot = ( hMinSpacing*dpi > 10 && vMinSpacing*dpi > 10 );
	for ( hMaj=hMajCnt0; hMaj<hMajCnt1; hMaj++ ) {
		for ( vMaj=vMajCnt0; vMaj<vMajCnt1; vMaj++ ) {
			for ( hMin=1; hMin<hMinCnt1; hMin++ ) {
				for ( vMin=1; vMin<vMinCnt1; vMin++ ) {
					p0.x = hMaj*hMajSpacing + hMin*hMinSpacing;
					p0.y = vMaj*vMajSpacing + vMin*vMinSpacing;
					DrawGridPoint( D, Color, Gorig, size, dpi, p0, bigdot );
				}
			}
		}
	}
    

done:
	wSetCursor( wCursorNormal );
}



static void DrawBigCross( coOrd pos, ANGLE_T angle )
{
	coOrd p0, p1;
	DIST_T d;
	if (angleSystem!=ANGLE_POLAR)
		angle += 90.0;
	d = max( mainD.size.x, mainD.size.y );
	Translate( &p0, pos, angle, d );
	Translate( &p1, pos, angle+180, d );
	if (ClipLine( &p0, &p1, mainD.orig, 0.0, mainD.size )) {
		DrawLine( &tempD, pos, p0, 0, crossMajorColor );
		DrawLine( &tempD, pos, p1, 0, crossMinorColor );
	}
	Translate( &p0, pos, angle+90, d );
	Translate( &p1, pos, angle+270, d );
	if (ClipLine( &p0, &p1, mainD.orig, 0.0, mainD.size )) {
		DrawLine( &tempD, p0, p1, 0, crossMinorColor );
	}
}


EXPORT STATUS_T GridAction(
		wAction_t action,
		coOrd pos,
		coOrd *orig,
		DIST_T *angle )
{

	static coOrd pos0, pos1;
	static ANGLE_T newAngle, oldAngle;

	switch (action) {
	case C_DOWN:
		pos1 = pos;
		DrawBigCross( pos1, *angle );
		return C_CONTINUE;

	case C_MOVE:
		DrawBigCross( pos1, *angle );
		*orig = pos1 = pos;
		DrawBigCross( pos1, *angle );
		return C_CONTINUE;

	case C_UP:
		DrawBigCross( pos1, *angle );
		*orig = pos1;
		return C_CONTINUE;

	case C_RDOWN:
		pos0 = pos1 = pos;
		oldAngle = newAngle = *angle;
		DrawBigCross( pos0, newAngle );
		return C_CONTINUE;

	case C_RMOVE:
		if ( FindDistance(pos0, pos) > 0.1*mainD.scale ) {
			DrawBigCross( pos0, newAngle );
			pos1 = pos;
			newAngle = FindAngle( pos0, pos1 );
			if (angleSystem!=ANGLE_POLAR)
				newAngle = newAngle-90.0;
			newAngle = NormalizeAngle( floor( newAngle*10.0 ) / 10.0 );
			*angle = newAngle;
			DrawBigCross( pos0, newAngle );
		}
		return C_CONTINUE;

	case C_RUP:
		DrawBigCross( pos0, newAngle );
		Rotate( orig, pos0, newAngle-oldAngle );
		*orig = pos0;
		*angle = newAngle;
		return C_CONTINUE;
	}
	return C_CONTINUE;
}

/*****************************************************************************
 *
 * Snap Grid Command
 *
 */

EXPORT wDrawColor snapGridColor;

typedef struct {
		DIST_T Spacing;
		long Division;
		long Enable;
		} gridData;
typedef struct {
		gridData Horz;
		gridData Vert;
		coOrd Orig;
		ANGLE_T Angle;
		long Show;
		} gridHVData;

static gridHVData grid = { { 1.0, 0, 1 },
						   { 1.0, 0, 1 } };

EXPORT void SnapPos( coOrd * pos )
{
	coOrd p;
	DIST_T spacing;
	if ( grid.Vert.Enable == FALSE && grid.Horz.Enable == FALSE )
		return;
	p = *pos;
	p.x -= grid.Orig.x;
	p.y -= grid.Orig.y;
	Rotate( &p, zero, -grid.Angle );
	if ( grid.Horz.Enable ) {
		if ( grid.Horz.Division > 0 )
			spacing = grid.Horz.Spacing / grid.Horz.Division;
		else
			spacing = grid.Horz.Spacing;
		if (spacing > 0.001)
			p.x = floor(p.x/spacing+0.5) * spacing;
	}
	if ( grid.Vert.Enable ) {
		if ( grid.Vert.Division > 0 )
			spacing = grid.Vert.Spacing / grid.Vert.Division;
		else
			spacing = grid.Vert.Spacing;
		if (spacing > 0.001)
			p.y = floor(p.y/spacing+0.5) * spacing;
	}
	REORIGIN1( p, grid.Angle, grid.Orig );
	*pos = p;
	InfoPos( p );
}


static void DrawASnapGrid( gridHVData * gridP, drawCmd_p d, coOrd size, BOOL_T drawDivisions )
{
	if (gridP->Horz.Spacing <= 0.0 && gridP->Vert.Spacing <= 0.0)
		return;
	if (gridP->Show == FALSE)
		return;
	DrawGrid( d, &size,
				gridP->Horz.Spacing, gridP->Vert.Spacing,
				drawDivisions?gridP->Horz.Division:0,
				drawDivisions?gridP->Vert.Division:0,
				gridP->Orig, gridP->Angle, snapGridColor, TRUE );
}


EXPORT void DrawSnapGrid( drawCmd_p d, coOrd size, BOOL_T drawDivisions )
{
	DrawASnapGrid( &grid, d, size, drawDivisions );
}


EXPORT BOOL_T GridIsVisible( void )
{
	return (BOOL_T)grid.Show;
}

/*****************************************************************************
 *
 * Snap Grid Dialog
 *
 */

static wWin_p gridW;
static wMenu_p snapGridPopupM;
static wButton_p snapGridEnable_b;
static wButton_p snapGridShow_b;
EXPORT wMenuToggle_p snapGridEnableMI;
EXPORT wMenuToggle_p snapGridShowMI;

static gridHVData oldGrid;

#define CHK_HENABLE		(1<<0)
#define CHK_VENABLE		(1<<1)
#define CHK_SHOW		(1<<2)

static paramFloatRange_t r0_999999		= { 0.0, 999999.0, 60 };
static paramIntegerRange_t i0_1000		= { 0, 1000, 30 };
static paramFloatRange_t r_1000_1000	= { -1000.0, 1000.0, 80 };
static paramFloatRange_t r0_360			= { 0.0, 360.0, 80 };
static char *gridLabels[]				= { "", NULL };
static paramData_t gridPLs[] = {
	{	PD_MESSAGE, N_("Horz"), NULL, 0, (void*)60 },
#define I_HORZSPACING	(1)
	{	PD_FLOAT, &grid.Horz.Spacing, "horzspacing", PDO_DIM, &r0_999999, N_("Spacing") },
#define I_HORZDIVISION	(2)
	{	PD_LONG, &grid.Horz.Division, "horzdivision", 0, &i0_1000, N_("Divisions") },
#define I_HORZENABLE	(3)
#define gridHorzEnableT ((wChoice_p)gridPLs[I_HORZENABLE].control)
	{	PD_TOGGLE, &grid.Horz.Enable, "horzenable", 0, gridLabels, N_("Enable"), BC_HORZ|BC_NOBORDER },
	{	PD_MESSAGE, N_("Vert"), NULL, PDO_DLGNEWCOLUMN|PDO_DLGWIDE, (void*)60},
#define I_VERTSPACING	(5)
	{	PD_FLOAT, &grid.Vert.Spacing, "vertspacing", PDO_DIM, &r0_999999, NULL },
#define I_VERTDIVISION	(6)
	{	PD_LONG, &grid.Vert.Division, "vertdivision", 0, &i0_1000, NULL },
#define I_VERTENABLE	(7)
#define gridVertEnableT ((wChoice_p)gridPLs[I_VERTENABLE].control)
	{	PD_TOGGLE, &grid.Vert.Enable, "vertenable", 0, gridLabels, NULL, BC_HORZ|BC_NOBORDER },
#define I_VALUEX		(8)
	{	PD_FLOAT, &grid.Orig.x, "origx", PDO_DIM|PDO_DLGNEWCOLUMN|PDO_DLGWIDE, &r_1000_1000, N_("X") },
#define I_VALUEY		(9)
	{	PD_FLOAT, &grid.Orig.y, "origy", PDO_DIM, &r_1000_1000, N_("Y") },
#define I_VALUEA		(10)
	{	PD_FLOAT, &grid.Angle, "origa", PDO_ANGLE, &r0_360, N_("A") },
#define I_SHOW			(11)
#define gridShowT		((wChoice_p)gridPLs[I_SHOW].control)
	{	PD_TOGGLE, &grid.Show, "show", PDO_DLGIGNORELABELWIDTH, gridLabels, N_("Show"), BC_HORZ|BC_NOBORDER } };

static paramGroup_t gridPG = { "grid", PGO_RECORD, gridPLs, sizeof gridPLs/sizeof gridPLs[0] };


static BOOL_T GridChanged( void )
{
	return
		grid.Horz.Spacing != oldGrid.Horz.Spacing ||
		grid.Horz.Division != oldGrid.Horz.Division ||
		grid.Vert.Spacing != oldGrid.Vert.Spacing ||
		grid.Vert.Division != oldGrid.Vert.Division ||
		grid.Orig.x != oldGrid.Orig.x ||
		grid.Orig.y != oldGrid.Orig.y ||
		grid.Angle != oldGrid.Angle ||
		grid.Horz.Division != oldGrid.Horz.Division;
}

static void RedrawGrid( void )
{
	if (grid.Show != oldGrid.Show ||
		GridChanged() ) {
		wDrawDelayUpdate( tempD.d, TRUE );
		DrawASnapGrid( &oldGrid, &tempD, mapD.size, TRUE );
		DrawASnapGrid( &grid, &tempD, mapD.size, TRUE );
		wDrawDelayUpdate( tempD.d, FALSE );
	}
}


static void GridOk( void * junk )
{
	long changes;

	ParamLoadData( &gridPG );
	if ( ( grid.Horz.Enable && grid.Horz.Spacing <= 0.0) ||
		 ( grid.Vert.Enable && grid.Vert.Spacing <= 0.0) ) {
		NoticeMessage( MSG_GRID_ENABLE_SPACE_GTR_0, _("Ok"), NULL );
		return;
	}
	if ( grid.Horz.Spacing <= 0.0 &&
		 grid.Vert.Spacing <= 0.0 )
		grid.Show = FALSE;

	changes = 0;
	if ( GridChanged() )
		changes |= CHANGE_GRID;
	if (grid.Show != oldGrid.Show || changes != 0)
		changes |= CHANGE_MAIN;
	DoChangeNotification( changes );
	oldGrid = grid;
	Reset();
}


static void GridButtonUpdate( long mode0 )
{
	long mode1;
	mode1 = 0;
	if ( grid.Show &&
		 grid.Horz.Spacing <= 0.0 &&
		 grid.Vert.Spacing <= 0.0 ) {
		grid.Show = FALSE;
		if ( mode0&CHK_SHOW )
			ErrorMessage( MSG_GRID_SHOW_SPACE_GTR_0 );
	}
	if ( grid.Horz.Enable &&
		 grid.Horz.Spacing <= 0.0 ) {
		grid.Horz.Enable = FALSE;
		if ( mode0&CHK_HENABLE )
			mode1 |= CHK_HENABLE;
	}
	if ( grid.Vert.Enable &&
		 grid.Vert.Spacing <= 0.0 ) {
		grid.Vert.Enable = FALSE;
		if ( mode0&CHK_VENABLE )
			mode1 |= CHK_VENABLE;
	}
	if ( mode1 &&
		(mode0&(CHK_HENABLE|CHK_VENABLE)) == mode1 )
		ErrorMessage( MSG_GRID_ENABLE_SPACE_GTR_0 );
	if ( gridShowT &&
		 grid.Show != (wToggleGetValue( gridShowT ) != 0) )
		ParamLoadControl( &gridPG, I_SHOW );
	if ( gridHorzEnableT &&
		 grid.Horz.Enable != (wToggleGetValue( gridHorzEnableT ) != 0) )
		ParamLoadControl( &gridPG, I_HORZENABLE );
	if ( gridVertEnableT &&
		 grid.Vert.Enable != (wToggleGetValue( gridVertEnableT ) != 0) )
		ParamLoadControl( &gridPG, I_VERTENABLE );
	if (snapGridEnable_b)
		wButtonSetBusy( snapGridEnable_b, grid.Horz.Enable||grid.Vert.Enable );
	if (snapGridShow_b)
		wButtonSetBusy( snapGridShow_b, (wBool_t)grid.Show );
	if (snapGridEnableMI)
		wMenuToggleSet( snapGridEnableMI, grid.Horz.Enable||grid.Vert.Enable );
	if (snapGridShowMI)
		wMenuToggleSet( snapGridShowMI, (wBool_t)grid.Show );

	if ( mode0&CHK_SHOW ) {
		RedrawGrid();
	}
	oldGrid = grid;
}


static void GridChange( long changes )
{
	if ( (changes&(CHANGE_GRID|CHANGE_UNITS))==0 )
		return;
	GridButtonUpdate( 0 );
	if (gridW==NULL || !wWinIsVisible(gridW))
		return;
	ParamLoadControls( &gridPG );
}


static void GridDlgUpdate(
		paramGroup_p pg,
		int inx,
		void * valueP )
{
	switch ( inx ) {
	case I_HORZENABLE:
		GridButtonUpdate( CHK_HENABLE );
		break;
	case I_VERTENABLE:
		GridButtonUpdate( CHK_VENABLE );
		break;
	case I_SHOW:
		GridButtonUpdate( CHK_SHOW );
		break;
	default:
		wDrawDelayUpdate( tempD.d, TRUE );
		DrawASnapGrid( &oldGrid, &tempD, mapD.size, TRUE );
		ParamLoadData( &gridPG );
		GridButtonUpdate( 0 );
		DrawASnapGrid( &grid, &tempD, mapD.size, TRUE );
		wDrawDelayUpdate( tempD.d, FALSE );
	}
}


static void SnapGridRotate( void * pangle )
{
	ANGLE_T angle = (ANGLE_T)(long)pangle;
	wDrawDelayUpdate( tempD.d, TRUE );
	DrawASnapGrid( &oldGrid, &tempD, mapD.size, TRUE );
	grid.Orig = cmdMenuPos;
	grid.Angle += angle;
	oldGrid = grid;
	DrawASnapGrid( &grid, &tempD, mapD.size, TRUE );
	wDrawDelayUpdate( tempD.d, FALSE );
	ParamLoadControls( &gridPG );
}


EXPORT STATUS_T CmdGrid(
		wAction_t action,
		coOrd pos )
{
	STATUS_T rc;
#ifdef TIMEDRAWGRID
	unsigned long time0, time1, time2;
#endif

	switch (action) {

	case C_START:
		if (gridW == NULL) {
			gridW = ParamCreateDialog( &gridPG, MakeWindowTitle(_("Snap Grid")), _("Ok"), GridOk, (paramActionCancelProc)Reset, TRUE, NULL, 0, GridDlgUpdate );
		}
		oldGrid = grid;
		ParamLoadControls( &gridPG );
		wShow( gridW );
		return C_CONTINUE;

	case C_REDRAW:
		return C_TERMINATE;

	case C_CANCEL:
		grid = oldGrid;
		wHide( gridW );
		MainRedraw();
		return C_TERMINATE;

	case C_OK:
		GridOk( NULL );
		return C_TERMINATE;

	case C_CONFIRM:
		if (GridChanged() ||
			grid.Show != oldGrid.Show ) 
			return C_ERROR;
		else
			return C_CONTINUE;

	case C_DOWN:
	case C_RDOWN:
		oldGrid = grid;
		rc = GridAction( action, pos, &grid.Orig, &grid.Angle );
		return rc;
	case C_MOVE:
	case C_RMOVE:
		rc = GridAction( action, pos, &grid.Orig, &grid.Angle );
		ParamLoadControls( &gridPG );
		return rc;
	case C_UP:
	case C_RUP:
#ifdef TIMEDRAWGRID
		time0 = wGetTimer();
#endif
#ifdef TIMEDRAWGRID
		time1 = wGetTimer();
#endif
		rc = GridAction( action, pos, &grid.Orig, &grid.Angle );
		ParamLoadControls( &gridPG );
		RedrawGrid();
		oldGrid = grid;
#ifdef TIMEDRAWGRID
		time2 = wGetTimer();
		InfoMessage( "undraw %ld, draw %ld", (long)(time1-time0), (long)(time2-time1) );
#endif
		return rc;

	case C_CMDMENU:
		wMenuPopupShow( snapGridPopupM );
		break;
	}

	return C_CONTINUE;
}


/**
 * Initialize the user interface for the grid functions. 
 *
 * \param menu IN pulldown to which the grid function will be added
 * \return    created command button
*/

EXPORT wIndex_t InitGrid( wMenu_p menu )
{
	ParamRegister( &gridPG );
	RegisterChangeNotification( GridChange );
	if ( grid.Horz.Enable && grid.Horz.Spacing <= 0.0 )
		grid.Horz.Enable = FALSE;
	if ( grid.Vert.Enable && grid.Vert.Spacing <= 0.0 )
		grid.Vert.Enable = FALSE;
	if ( grid.Horz.Spacing <= 0.0 &&
		 grid.Vert.Spacing <= 0.0 )
		grid.Show = FALSE;
	snapGridPopupM = MenuRegister( "Snap Grid Rotate" );
	AddRotateMenu( snapGridPopupM, SnapGridRotate );
	GridButtonUpdate( 0 );
	return InitCommand( menu, CmdGrid, N_("Change Grid..."), NULL, LEVEL0, IC_CMDMENU, ACCL_GRIDW );
}


EXPORT void SnapGridEnable( void )
{
	grid.Vert.Enable = grid.Horz.Enable = !( grid.Vert.Enable || grid.Horz.Enable );
	GridButtonUpdate( (CHK_HENABLE|CHK_VENABLE) );
}


EXPORT void SnapGridShow( void )
{
	grid.Show = !grid.Show;
	GridButtonUpdate( CHK_SHOW );
}

#include "bitmaps/snapcurs.xbm"
#include "bitmaps/snapvis.xbm"

EXPORT void InitSnapGridButtons( void )
{
	snapGridEnable_b = AddToolbarButton( "cmdGridEnable", wIconCreateBitMap(snapcurs_width, snapcurs_height, snapcurs_bits, wDrawColorBlack), 0, (addButtonCallBack_t)SnapGridEnable, NULL );
	snapGridShow_b = AddToolbarButton( "cmdGridShow", wIconCreateBitMap(snapvis_width, snapvis_height, snapvis_bits, wDrawColorBlack), IC_MODETRAIN_TOO, (addButtonCallBack_t)SnapGridShow, NULL );
}
