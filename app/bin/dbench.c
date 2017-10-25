/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/dbench.c,v 1.3 2008-03-06 19:35:07 m_fischer Exp $
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
#include <stdio.h>
#include <string.h>

#include "i18n.h"
#include "param.h"
#include "track.h"
#include "utility.h"

/*****************************************************************************
 *
 * BENCH WORK
 *
 */


#define B_RECT			(0)
#define B_LGRIDER		(1)
#define B_TGRIDER		(2)

static char *benchTypeS[] = { "", N_(" L-Girder"), N_(" T-Girder") };

#include "bitmaps/bo_edge.xpm"
#include "bitmaps/bo_flat.xpm"
#include "bitmaps/bo_ll.xpm"
#include "bitmaps/bo_lr.xpm"
#include "bitmaps/bo_lld.xpm"
#include "bitmaps/bo_lrd.xpm"
#include "bitmaps/bo_llu.xpm"
#include "bitmaps/bo_lru.xpm"
#include "bitmaps/bo_lli.xpm"
#include "bitmaps/bo_lri.xpm"
#include "bitmaps/bo_t.xpm"
#include "bitmaps/bo_tr.xpm"
#include "bitmaps/bo_tl.xpm"
#include "bitmaps/bo_ti.xpm"

typedef struct {
		char * name;
		char ** xpm;
		wIcon_p icon;
		} orientData_t;
static orientData_t rectOrientD[] = {
		{ N_("On Edge"), bo_edge_xpm },
		{ N_("Flat"), bo_flat_xpm } };
static orientData_t lgirderOrientD[] = {
		{ N_("Left"), bo_ll_xpm },
		{ N_("Right"), bo_lr_xpm },
		{ N_("Left-Down"), bo_lld_xpm },
		{ N_("Right-Down"), bo_lrd_xpm },
		{ N_("Left-Up"), bo_llu_xpm },
		{ N_("Right-Up"), bo_lru_xpm },
		{ N_("Left-Inverted"), bo_lli_xpm },
		{ N_("Right-Inverted"), bo_lri_xpm } };
static orientData_t tgirderOrientD[] = {
		{ N_("Normal"), bo_t_xpm },
		{ N_("Right"), bo_tr_xpm },
		{ N_("Left"), bo_tl_xpm },
		{ N_("Inverted"), bo_ti_xpm } };

static struct {
		int cnt;
		orientData_t *data;
		} orientD[] = { {2, rectOrientD}, {8, lgirderOrientD}, {4, tgirderOrientD} };

		
/*										L-N R-N L-D R-D L-U R-U L-I R-I */
static BOOL_T lgirderFlangeLeft[]	= {	 1,	 0,	 0,	 1,	 1,	 0,	 0,	 1 };
static BOOL_T lgirderFlangeDashed[] = {	 1,	 1,	 1,	 1,	 0,	 0,	 0,	 0 };
static BOOL_T lgirderNarrow[]		= {	 1,	 1,	 0,	 0,	 0,	 0,	 1,	 1 };

EXPORT void BenchUpdateOrientationList(
		long benchData,
		wList_p list )
{
	long type;
	orientData_t *op;
	int cnt;

	type = (benchData>>24)&0xff;
	wListClear( list );
	op = orientD[type].data;
	for (cnt=orientD[type].cnt-1; cnt>=0; cnt--,op++) {
#ifdef WINDOWS
		if (op->icon == NULL)
			op->icon = wIconCreatePixMap( op->xpm );
		wListAddValue( list, NULL, op->icon, op );
#else
		/* gtk_combo_find is upset if we try to put anything other that a label on a list */
		wListAddValue( list, _(op->name), NULL, op );
#endif
   }
   wListSetIndex( list, 0 );
}

typedef struct {
		long type;
		long width;
		long height0, height1;
		} benchType_t, *benchType_p;
static dynArr_t benchType_da;
#define benchType(N) DYNARR_N( benchType_t, benchType_da, N )

static void AddBenchTypes(
		long type,
		char * key,
		char * defvalue )
{
	benchType_p bt;
	char *value, *cp, *cq;
	value = CAST_AWAY_CONST wPrefGetString( "misc", key );
	if ( value == NULL ) {
		value = defvalue;
		wPrefSetString( "misc", key, value );
	}
	cp = value;
	while ( *cp ) {
		DYNARR_APPEND( benchType_t, benchType_da, 10 );
		bt = &benchType(benchType_da.cnt-1);
		bt->type = type;
		bt->width = strtol( cq=cp, &cp, 10 );
		bt->height0 = strtol( cq=cp, &cp, 10 );
		bt->height1 = strtol( cq=cp, &cp, 10 );
		if ( cp == cq ) {
			NoticeMessage( _("Bad BenchType for %s:\n%s"), _("Continue"), NULL, key, value );
			benchType_da.cnt--;
			return;
		}
	}
}


EXPORT void BenchLoadLists( wList_p choiceL, wList_p orientL )
{
	int inx;
	long height;
	long benchData;
	benchType_p bt;
	char * cp;

	wListClear( choiceL );
	wListClear( orientL );
	if ( benchType_da.cnt <= 0 ) {
		Reset();
		return;
	}
	for ( inx=0; inx<benchType_da.cnt; inx++ ) {
		bt = &benchType(inx);
		for (height=bt->height0; height<=bt->height1; height++ ) {
			benchData = bt->type<<24 | bt->width<<17 | height<<9;
			sprintf( message, "%s", (bt->type==B_LGRIDER?"L-":bt->type==B_TGRIDER?"T-":"") );
			cp = message+strlen(message);
			if ( units==UNITS_ENGLISH )
				sprintf( cp, "%ld\"x%ld\"", bt->width, height );
			else
				sprintf( cp, "%ldmm x %ldmm", height*25, bt->width*25 );
			wListAddValue( choiceL, message, NULL, (void*)benchData );
		}
	}
	BenchUpdateOrientationList( benchType(0).type<<24, orientL );
	wListSetIndex( choiceL, 0 );
}


EXPORT long GetBenchData(
		long benchData,
		long orient )
{
	return (benchData&0xFFFFFF00)|(orient&0xFF);
}


EXPORT wIndex_t GetBenchListIndex(
		long benchData )
{
	wIndex_t inx, cnt;
	benchType_p bt;
	long type;
	long iwidth, iheight;

	iheight = (benchData>>9)&0xff;
	iwidth = (benchData>>17)&0x7f;
	type = (benchData>>24)&0xff;

	for ( inx=cnt=0; inx<benchType_da.cnt; inx++ ) {
		bt = &benchType(inx);
		if ( bt->type == type &&
			 bt->width == iwidth ) {
			if ( iheight < bt->height0 )
				bt->height0 = iheight;
			else if ( iheight > bt->height1 )
				bt->height1 = iheight;
			cnt += (wIndex_t)(iheight - bt->height0);
			return cnt;
		}
		cnt += (wIndex_t)(bt->height1 - bt->height0 + 1);
	}
	DYNARR_APPEND( benchType_t, benchType_da, 10 );
	bt = &benchType(benchType_da.cnt-1);
	bt->type = type;
	bt->width = iwidth;
	bt->height0 = bt->height1 = iheight;
	return cnt;
}


EXPORT void DrawBench(
		drawCmd_p d,
		coOrd p0,
		coOrd p1,
		wDrawColor color1,
		wDrawColor color2,
		long option,
		long benchData )
{
	long orient;
	coOrd pp[4];
	ANGLE_T a;
	DIST_T width, thickness=0.75;
	long type;
	long oldOptions;
	long lwidth;

	orient = benchData&0xFF;
	type = (benchData>>24)&0xff;
	width = BenchGetWidth(benchData);
	lwidth = (long)floor( width*d->dpi/d->scale+0.5 );

	if ( lwidth <= 3 ) {
		DrawLine( d, p0, p1, (wDrawWidth)lwidth, color1 );
	} else {
		width /= 2.0;
		a = FindAngle( p0, p1 );
		Translate( &pp[0], p0, a+90, width );
		Translate( &pp[1], p0, a-90, width );
		Translate( &pp[2], p1, a-90, width );
		Translate( &pp[3], p1, a+90, width );
		DrawFillPoly( d, 4, pp, color1 );
		/* Draw Outline */
		if ( /*color1 != color2 &&*/
			 ( ( d->scale < ((d->options&DC_PRINT)?(twoRailScale*2+1):twoRailScale) ) ||	/* big enough scale */
			   ( d->funcs == &tempSegDrawFuncs ) ) ) {										/* DrawFillPoly didn't draw */
			DrawLine( d, pp[0], pp[1], 0, color2 );
			DrawLine( d, pp[1], pp[2], 0, color2 );
			DrawLine( d, pp[2], pp[3], 0, color2 );
			DrawLine( d, pp[3], pp[0], 0, color2 );
			if ( color1 != color2 && type != B_RECT ) {
				oldOptions = d->options;
				if ( type == B_LGRIDER || orient == 1 || orient == 2 ) {
					if ( type == B_LGRIDER && lgirderFlangeDashed[orient] )
						d->options |= DC_DASH;
					if ( (type == B_LGRIDER && lgirderFlangeLeft[orient]) ||
						 (type == B_TGRIDER && orient == 1) ) {
						Translate( &pp[0], pp[1], a+90, thickness );
						Translate( &pp[3], pp[2], a+90, thickness );
					} else {
						Translate( &pp[0], pp[0], a-90, thickness );
						Translate( &pp[3], pp[3], a-90, thickness );
					}
					DrawLine( d, pp[0], pp[3], 0, color2 );
				} else {
				   Translate( &pp[0], p0, a+90, thickness/2.0 );
				   Translate( &pp[1], p0, a-90, thickness/2.0 );
				   Translate( &pp[2], p1, a-90, thickness/2.0 );
				   Translate( &pp[3], p1, a+90, thickness/2.0 );
				   if ( orient == 0 )
					   d->options |= DC_DASH;
				   DrawLine( d, pp[0], pp[3], 0, color2 );
				   DrawLine( d, pp[1], pp[2], 0, color2 );
				}
				d->options = oldOptions;
			}
		}
	}
}


EXPORT addButtonCallBack_t InitBenchDialog( void )
{
	AddBenchTypes( B_RECT, "benchtype-rect", "1 1 6 2 2 4 2 6 6 2 8 8 4 4 4" );
	AddBenchTypes( B_LGRIDER, "benchtype-lgrider", "2 4 5 3 4 6 4 5 8" );
	AddBenchTypes( B_TGRIDER, "benchtype-tgrider", "2 4 4 3 4 7 4 5 8" );
	return NULL;
}


EXPORT void BenchGetDesc(
		long benchData,
		char * desc )
{
	long orient;
	long type;
	long iwidth, iheight;
	char name[40];

	orient = benchData&0xFF;
	iheight = (benchData>>9)&0xff;
	iwidth = (benchData>>17)&0x7f;
	type = (benchData>>24)&0xff;

	if ( units==UNITS_ENGLISH )
		sprintf( name, "%ld\"x%ld\"", iwidth, iheight );
	else
		sprintf( name, "%ldmm x %ldmm", iheight*25, iwidth*25 );

	sprintf( desc, "%s%s %s",
		(type==B_LGRIDER?"L - ":type==B_TGRIDER?"T - ":""),
		name,
		_(orientD[type].data[(int)orient].name) );
}

typedef struct {
		long type;
		long width;
		long height;
		DIST_T length;
		} benchEnum_t, *benchEnum_p;
static dynArr_t benchEnum_da;
#define benchEnum(N) DYNARR_N( benchEnum_t, benchEnum_da, N )

static void PrintBenchLine(
		char * line,
		benchEnum_p bp )
{
	char name[40];
	if ( units==UNITS_ENGLISH )
		sprintf( name, "%ld\"x%ld\"", bp->width, bp->height );
	else
		sprintf( name, "%ldmm x %ldmm", bp->height*25, bp->width*25 );
	sprintf( line, "%s - %s%s", FormatDistance(bp->length), name, benchTypeS[bp->type] );
}

EXPORT void CountBench(
		long benchData,
		DIST_T length )
{
	int inx;
	long orient;
	long type;
	long iwidth, iheight;
	benchEnum_p bp;

	orient = benchData&0xFF;
	iheight = (benchData>>9)&0xff;
	iwidth = (benchData>>17)&0x7f;
	type = (benchData>>24)&0xff;

	for ( inx=0; inx<benchEnum_da.cnt; inx++ ) {
		bp = &benchEnum(inx);
		if ( bp->type == type &&
			 bp->width == iwidth &&
			 bp->height == iheight ) {
			bp->length += length;
			goto foundBenchEnum;
		}
	}
	DYNARR_APPEND( benchEnum_t, benchEnum_da, 10 );
	bp = &benchEnum(benchEnum_da.cnt-1);
	bp->type = type;
	bp->width = iwidth;
	bp->height = iheight;
	bp->length = length;
foundBenchEnum:
	PrintBenchLine( message, bp );
	iwidth = strlen(message);
	if ( iwidth > enumerateMaxDescLen)
		enumerateMaxDescLen = (int)iwidth;
}

static int Cmp_benchEnum(
		const void *p1,
		const void *p2 )
{
	benchEnum_p bp1 = (benchEnum_p)p1;
	benchEnum_p bp2 = (benchEnum_p)p2;
	long diff;
	if ( ( diff = bp1->type-bp2->type ) != 0 ) return (int)diff;
	if ( ( diff = bp1->width-bp2->width ) != 0 ) return (int)diff;
	if ( ( diff = bp1->height-bp2->height ) != 0 ) return (int)diff;
	return 0;
}

EXPORT void TotalBench( void )
{
	int inx;
	char title[STR_SIZE];
	benchEnum_p bp;

	qsort( benchEnum_da.ptr, benchEnum_da.cnt, sizeof *bp, Cmp_benchEnum );
	for ( inx=0; inx<benchEnum_da.cnt; inx++ ) {
		bp = &benchEnum(inx);
		if ( bp->length > 0 ) {
			PrintBenchLine( title, bp );
			EnumerateList( 1, 0, title );
			bp->length = 0;
		}
	}
}

EXPORT long BenchInputOption( long option )
{
	return option;
}


EXPORT long BenchOutputOption( long benchData )
{
	return benchData;
}


EXPORT DIST_T BenchGetWidth( long benchData )
{
	long orient;
	long type;
	long iwidth, iheight;
	DIST_T width;

	orient = benchData&0xFF;
	iheight = (benchData>>9)&0xff;
	iwidth = (benchData>>17)&0x7f;
	type = (benchData>>24)&0xff;

	switch (type) {
	case B_LGRIDER:
		width = lgirderNarrow[orient]?iwidth-0.25:iheight-0.5;
		break;
	case B_TGRIDER:
		width = (orient==0||orient==3)?iwidth-0.25:iheight-0.5;
		break;
	case B_RECT:
		width = (orient==0)?iwidth-0.25:iheight-0.25;
		break;
	default:
		width = 1.0;
	}
	return width;
}
