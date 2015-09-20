/** \file misc2.c
 * Management of information about scales and gauges plus rprintf.
 *
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/misc2.c,v 1.9 2009-10-04 16:46:39 m_fischer Exp $
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

#include <stdlib.h>
#include <stdio.h>
#ifndef WINDOWS
#include <unistd.h>
#include <dirent.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include <stdint.h>

#include "track.h"
#include "common.h"
#include "utility.h"
#include "draw.h"
#include "misc.h"
#include "cjoin.h"
#include "compound.h"
#include "i18n.h"

EXPORT long units = 0;
EXPORT long checkPtInterval = 10;

EXPORT DIST_T curScaleRatio;
EXPORT char * curScaleName;
EXPORT DIST_T trackGauge;
EXPORT char Title1[40] = "";
EXPORT char Title2[40] = "Title line 2";
EXPORT long labelScale = 8;
EXPORT long labelEnable = ((1<<0)|LABELENABLE_LENGTHS|LABELENABLE_ENDPT_ELEV|LABELENABLE_CARS);
EXPORT long labelWhen = 2;
EXPORT long colorLayers = 0;
EXPORT long hideSelectionWindow = 0;
EXPORT long angleSystem = 0;
EXPORT DIST_T minLength = 0.1;
EXPORT DIST_T connectDistance = 0.1;
EXPORT ANGLE_T connectAngle = 1.0;
EXPORT long twoRailScale = 16;
EXPORT long mapScale = 64;
EXPORT long liveMap = 0;
EXPORT long preSelect = 0;
EXPORT long listLabels = 7;
EXPORT long layoutLabels = 1;
EXPORT long descriptionFontSize = 72;
EXPORT long enableListPrices = 1;
EXPORT void ScaleLengthEnd(void);
static char minTrackRadiusPrefS[STR_SHORT_SIZE] = "minTrackRadius";

/****************************************************************************
 *
 * RPRINTF
 *
 */


#define RBUFF_SIZE (8192)
static char rbuff[RBUFF_SIZE+1];
static int roff;
static int rbuff_record = 0;

EXPORT void Rdump( FILE * outf )
{
	fprintf( outf, "Record Buffer:\n" );
	rbuff[RBUFF_SIZE] = '\0';
	fprintf( outf, rbuff+roff );
	rbuff[roff] = '\0';
	fprintf( outf, rbuff );
	memset( rbuff, 0, sizeof rbuff );
	roff = 0;
}


EXPORT void Rprintf(
		char * format,
		... )
{
	static char buff[STR_SIZE];
	char * cp;
	va_list ap;
	va_start( ap, format );
	vsprintf( buff, format, ap );
	va_end( ap );
	if (rbuff_record >= 1)
		lprintf( buff );
	for ( cp=buff; *cp; cp++ ) {
		rbuff[roff] = *cp;
		roff++;
		if (roff>=RBUFF_SIZE)
			roff=0;
	}
}

/****************************************************************************
 *
 * CHANGE NOTIFICATION
 *
 */


static changeNotificationCallBack_t changeNotificationCallBacks[20];
static int changeNotificationCallBackCnt = 0;
		
EXPORT void RegisterChangeNotification(
		changeNotificationCallBack_t action )
{
	changeNotificationCallBacks[changeNotificationCallBackCnt] = action;
	changeNotificationCallBackCnt++;
}


EXPORT void DoChangeNotification( long changes )
{
	int inx;
	for (inx=0;inx<changeNotificationCallBackCnt;inx++)
		changeNotificationCallBacks[inx](changes);
}


/****************************************************************************
 *
 * SCALE
 *
 */


#define SCALE_ANY	(-2)
#define SCALE_DEMO	(-1)

typedef struct {
		char * scale;
		DIST_T ratio;
		DIST_T gauge;
		DIST_T R[3];
		DIST_T X[3];
		DIST_T L[3];
		wIndex_t index;
		DIST_T length;
		BOOL_T tieDataValid;
		tieData_t tieData;
		} scaleInfo_t;
EXPORT typedef scaleInfo_t * scaleInfo_p;
static dynArr_t scaleInfo_da;
#define scaleInfo(N) DYNARR_N( scaleInfo_t, scaleInfo_da, N )
static tieData_t tieData_demo = {
		96.0/160.0,
		16.0/160.0,
		32.0/160.0 };

EXPORT SCALEINX_T curScaleInx = -1;
static scaleInfo_p curScale;
EXPORT long includeSameGaugeTurnouts = FALSE;
static SCALEINX_T demoScaleInx = -1;


/** this struct holds a gauge description */
typedef struct {
		char * gauge;		/** ptr to textual description eg. 'n3' */
		SCALEINX_T scale;	/** index of complete information in scaleInfo_da */
		wIndex_t index;
		} gaugeInfo_t;

EXPORT typedef gaugeInfo_t * gaugeInfo_p;

EXPORT GAUGEINX_T curGaugeInx = -1;

/** this struct holds a scale description */
typedef struct {
		char *scaleDesc;	/** ptr to textual description eg. 'HO' */
		SCALEINX_T scale;	/** index of complete information (standard gauge) in scaleInfo_da  */
		wIndex_t index;
		dynArr_t gauges_da;	/** known gauges to this scale */
		} scaleDesc_t;

EXPORT typedef scaleDesc_t *scaleDesc_p;
static dynArr_t scaleDesc_da;
#define scaleDesc(N) DYNARR_N( scaleDesc_t, scaleDesc_da, N )

EXPORT SCALEDESCINX_T curScaleDescInx;

/**
 * Get the ratio from a scale description. Each member in the list of scale descriptions is
 * linked to an entry in the simple linear list of all scales/gauges. From there the ratio is
 * fetched and returned. Note that there is no error checking on parameters!
 *
 * \param IN sdi index into list of scale descriptions
 * \return ratio for scale
 */

EXPORT DIST_T GetScaleDescRatio( SCALEDESCINX_T sdi )
{
	return GetScaleRatio( scaleDesc(sdi).scale );
}

/**
 * Get the index into the linear list from a scale description and a gauge. All information about a 
 * scale/ gauge combination is stored in a linear list. The index in that list for a given scale and the
 * gauge is returned by this function. Note that there is no error checking on parameters!
 *
 * \param IN scaleInx index into list of scale descriptions
 * \param IN gaugeInx index into list of gauges available for this scale 
 * \return  index into master list of scale/gauge combinations
 */

EXPORT SCALEINX_T GetScaleInx( SCALEDESCINX_T scaleInx, GAUGEINX_T gaugeInx )
{
	scaleDesc_t s;
	gaugeInfo_p g;

	s = scaleDesc(scaleInx);
   g = &(DYNARR_N(gaugeInfo_t, s.gauges_da, gaugeInx));

	return g->scale;

}
EXPORT DIST_T GetScaleTrackGauge( SCALEINX_T si )
{
	return scaleInfo(si).gauge;
}

EXPORT DIST_T GetScaleRatio( SCALEINX_T si )
{
	return scaleInfo(si).ratio;
}

EXPORT char * GetScaleName( SCALEINX_T si )
{
	if ( si == -1 )
		return "DEMO";
	if ( si == SCALE_ANY )
		return "*";
	else if ( si < 0 || si >= scaleInfo_da.cnt )
		return "Unknown";
	else
		return scaleInfo(si).scale;
}

EXPORT void GetScaleEasementValues( DIST_T * R, DIST_T * L )
{
	wIndex_t i;
	for (i=0;i<3;i++) {
		*R++ = curScale->R[i];
		*L++ = curScale->L[i];
	}
}


EXPORT tieData_p GetScaleTieData( SCALEINX_T si )
{
	scaleInfo_p s;
	DIST_T defLength;

	if ( si == -1 )
		return &tieData_demo;
	else if ( si < 0 || si >= scaleInfo_da.cnt )
		return &tieData_demo;
	s = &scaleInfo(si);
	if ( !s->tieDataValid ) {
		sprintf( message, "tiedata-%s", s->scale );
		defLength = (96.0-54.0)/s->ratio+s->gauge;
		wPrefGetFloat( message, "length", &s->tieData.length, defLength );
		wPrefGetFloat( message, "width", &s->tieData.width, 16.0/s->ratio );
		wPrefGetFloat( message, "spacing", &s->tieData.spacing, 2*s->tieData.width );
	}
	return &scaleInfo(si).tieData;
}

EXPORT char *GetScaleDesc( SCALEDESCINX_T inx )
{
	return scaleDesc(inx).scaleDesc;
}

EXPORT char *GetGaugeDesc( SCALEDESCINX_T scaleInx, GAUGEINX_T gaugeInx )
{
	scaleDesc_t s;
	gaugeInfo_p g;

	s = scaleDesc(scaleInx);
   g = &(DYNARR_N(gaugeInfo_t, s.gauges_da, gaugeInx));

	return g->gauge;
}

EXPORT SCALEINX_T LookupScale( const char * name )
{
	wIndex_t si;
	DIST_T gauge;
	if ( strcmp( name, "*" ) == 0 )
		return SCALE_ANY;
	for ( si=0; si<scaleInfo_da.cnt; si++ ) {
		if (strcmp( scaleInfo(si).scale, name ) == 0)
			return si;
	}
	if ( isdigit(name[0]) ) {
		gauge = atof( name );
		for ( si=0; si<scaleInfo_da.cnt; si++ ) {
			if (scaleInfo(si).gauge == gauge)
				return si;
		}
	}
	NoticeMessage( MSG_BAD_SCALE_NAME, "Ok", NULL, name, sProdNameLower ); 
	si = scaleInfo_da.cnt;
	DYNARR_APPEND( scaleInfo_t, scaleInfo_da, 10 );
	scaleInfo(si) = scaleInfo(0);
	scaleInfo(si).scale = MyStrdup( name );
	return si;
}


EXPORT BOOL_T CompatibleScale(
	BOOL_T isTurnout,
	SCALEINX_T scale1,
	SCALEINX_T scale2 )
{
	if ( scale1 == scale2 )
		return TRUE;
	if ( scale1 == SCALE_DEMO || scale2 == SCALE_DEMO )
		return FALSE;
	if ( scale1 == demoScaleInx || scale2 == demoScaleInx )
		return FALSE;
	if ( isTurnout ) {
		if ( includeSameGaugeTurnouts &&
			 scaleInfo(scale1).gauge == scaleInfo(scale2).gauge )
			return TRUE;
	} else {
		if ( scale1 == SCALE_ANY )
			return TRUE;
		if ( scaleInfo(scale1).ratio == scaleInfo(scale2).ratio )
			return TRUE;
	}
	return FALSE;
}

/** Split the scale and the gauge description for a given combination. Eg HOn3 will be 
 * split to HO and n3.
 * \param scaleInx IN  scale/gauge combination
 * \param scaleDescInx OUT  scale part
 * \param  gaugeInx OUT gauge part
 * \return TRUE
 */

EXPORT BOOL_T
GetScaleGauge( SCALEINX_T scaleInx, SCALEDESCINX_T *scaleDescInx, GAUGEINX_T *gaugeInx)
{
	int i, j;
	char *scaleName = GetScaleName( scaleInx );
	DIST_T scaleRatio = GetScaleRatio( scaleInx );
	dynArr_t gauges_da;
	
	for( i = 0; i < scaleDesc_da.cnt; i++ ) {
		char *t = strchr( scaleDesc(i).scaleDesc, ' ' );
		/* are the first characters (which describe the scale) identical? */
		if( !strncmp( scaleDesc(i).scaleDesc, scaleName, t - scaleDesc(i).scaleDesc )) {
			/* if yes, are we talking about the same ratio */
		 	if( scaleInfo(scaleDesc(i).scale).ratio == scaleRatio ) {
				/* yes, we found the right scale descriptor, so now look for the gauge */
				*scaleDescInx = i;
				gauges_da = scaleDesc(i).gauges_da;
				*gaugeInx = 0;
				for( j = 0; j < gauges_da.cnt; j++ ) {
					gaugeInfo_p ptr = &(DYNARR_N( gaugeInfo_t, gauges_da, j ));
					if( scaleInfo(ptr->scale).gauge == GetScaleTrackGauge( scaleInx )) {
						*gaugeInx = j;
						break;
					}	
				}
				break;
			}	
		}
	}	
	
	return TRUE;
}

/**
 * Setup XTrkCad for the newly selected scale/gauge combination.
 *
 * \param newScaleInx IN the index of the selected scale/gauge combination
 */
 
static void SetScale(
		SCALEINX_T newScaleInx )
{	
	if (newScaleInx < 0 && newScaleInx >= scaleInfo_da.cnt) {
		NoticeMessage( MSG_BAD_SCALE_INDEX, _("Ok"), NULL, (int)newScaleInx );
		return;
	}
	curScaleInx = (SCALEINX_T)newScaleInx;
	curScale = &scaleInfo(curScaleInx);
	trackGauge = curScale->gauge;
	curScaleRatio = curScale->ratio;
	curScaleName = curScale->scale;
	
	curScaleDescInx = 0;
	
	GetScaleGauge( curScaleInx, &curScaleDescInx, &curGaugeInx );
	
	wPrefSetString( "misc", "scale", curScaleName );

	// now load the minimum radius for the newly selected scale
	sprintf( minTrackRadiusPrefS, "minTrackRadius-%s", curScaleName );
	wPrefGetFloat( "misc", minTrackRadiusPrefS, &minTrackRadius, curScale->R[0] );
}

/**
 * Check the new scale value and update the program if a valid scale was passed
 *
 * \param newScale IN the name of the new scale
 * \returns TRUE if valid, FALSE otherwise
 */

EXPORT BOOL_T DoSetScale(
		const char * newScale )
{
	SCALEINX_T scale;
	char * cp;
	BOOL_T found = FALSE;
 
	if ( newScale != NULL ) {
		cp = CAST_AWAY_CONST newScale+strlen(newScale)-1;
		while ( *cp=='\n' || *cp==' ' || *cp=='\t' ) cp--; 
		cp[1] = '\0';
		while (isspace(*newScale)) newScale++;
		for (scale = 0; scale<scaleInfo_da.cnt; scale++) {
			if (strcasecmp( scaleInfo(scale).scale, newScale ) == 0) {
				curScaleInx = scale;
				found = TRUE;
				break;
			}
		}
		// was a valid scale given?
		if( found ) {
			DoChangeNotification( CHANGE_SCALE );
		}
	}
	
	return found;
}

/** 
 * Setup the data structures for scale and gauge. XTC reads 'scales' into an dynamic array, 
 * but doesn't differentiate between scale and gauge.
 * This da is split into an dynamic array of scales. Each scale holds a dynamic array of gauges,
 * with at least one gauge per scale (ie standard gauge)
 *
 * For usage in the dialogs, a textual description for each scale or gauge is provided
 *
 * \return TRUE
 */
 
EXPORT BOOL_T DoSetScaleDesc( void )
{
	SCALEINX_T scaleInx;
	SCALEINX_T work;
	SCALEDESCINX_T descInx;
	scaleDesc_p s = NULL;
	gaugeInfo_p g; 
	char *cp;
	DIST_T ratio;
	BOOL_T found;
	char buf[ 80 ];
	int len;
	
	for( scaleInx = 0; scaleInx < scaleInfo_da.cnt; scaleInx++ ) {
		ratio = DYNARR_N( scaleInfo_t, scaleInfo_da, scaleInx ).ratio;
		
		/* do we already have a description for this scale? */
		found = 0;
		
		if( scaleDesc_da.cnt > 0 ) {
			for( descInx = 0; descInx < scaleDesc_da.cnt; descInx++ ) {
				work = scaleDesc(descInx).scale;
				if( scaleInfo(work).ratio == scaleInfo(scaleInx).ratio ) {
						if( !strncmp( scaleInfo(work).scale, scaleInfo(scaleInx).scale,	strlen(scaleInfo(work).scale)))
							found = TRUE;
				}
			}
		}
		
				
		if( !found ) {
			/* if no, add as new scale */
	
			DYNARR_APPEND( scaleDesc_t, scaleDesc_da, 1 );
			
			s = &(scaleDesc( scaleDesc_da.cnt-1 ));
			
			s->scale = scaleInx;
			
			sprintf( buf, "%s (1/%.1f)", scaleInfo(scaleInx).scale, scaleInfo(scaleInx).ratio );
			s->scaleDesc = MyStrdup( buf );
			
			/* initialize the array with standard gauge */
			
			DYNARR_APPEND( gaugeInfo_t, s->gauges_da, 10 );
			
			g = &(DYNARR_N( gaugeInfo_t, s->gauges_da, (s->gauges_da).cnt - 1 ));
			g->scale = scaleInx;
			sprintf( buf, "Standard (%.1fmm)", scaleInfo(scaleInx).gauge*25.4 );
			g->gauge = MyStrdup( buf );
			
		} else {	
			/* if yes, is this a new gauge to the scale? */
			DYNARR_APPEND( gaugeInfo_t, s->gauges_da, 10 );			
			g = &(DYNARR_N( gaugeInfo_t, s->gauges_da, (s->gauges_da).cnt - 1 ));
			g->scale = scaleInx;
			cp = strchr( s->scaleDesc, ' ' );
			if( cp )
				len = cp - s->scaleDesc;
			else
				len = strlen(s->scaleDesc);
			sprintf( buf, "%s (%.1fmm)", scaleInfo(scaleInx).scale+len,   scaleInfo(scaleInx).gauge*25.4 );
			g->gauge = MyStrdup( buf );
		}
	}
		
	return( TRUE );
}

void
SetScaleGauge( SCALEDESCINX_T scaleDesc, GAUGEINX_T gauge )
{
	dynArr_t gauges_da;

	gauges_da = (scaleDesc(scaleDesc)).gauges_da;
	curScaleInx = ((gaugeInfo_p)gauges_da.ptr)[ gauge ].scale;
}

static BOOL_T AddScale(
		char * line )
{
	wIndex_t i;
	BOOL_T rc;
	DIST_T R[3], X[3], L[3];
	DIST_T ratio, gauge;
	char scale[40];
	scaleInfo_p s;

	if ( (rc=sscanf( line, "SCALE %[^,]," SCANF_FLOAT_FORMAT "," SCANF_FLOAT_FORMAT "",
				scale, &ratio, &gauge )) != 3) {
		SyntaxError( "SCALE", rc, 3 );
		return FALSE;
	}
	for (i=0;i<3;i++) {
		line = GetNextLine();
		if ( (rc=sscanf( line, "" SCANF_FLOAT_FORMAT "," SCANF_FLOAT_FORMAT "," SCANF_FLOAT_FORMAT "",
				&R[i], &X[i], &L[i] )) != 3 ) {
			SyntaxError( "SCALE easement", rc, 3 );
			return FALSE;
		}
	}

	DYNARR_APPEND( scaleInfo_t, scaleInfo_da, 10 );
	s = &scaleInfo(scaleInfo_da.cnt-1);
	s->scale = MyStrdup( scale );
	s->ratio = ratio;
	s->gauge = gauge;
	s->index = -1;
	for (i=0; i<3; i++) {
		s->R[i] = R[i]/ratio;
		s->X[i] = X[i]/ratio;
		s->L[i] = L[i]/ratio;
	}
	s->tieDataValid = FALSE;
	if ( strcmp( scale, "DEMO" ) == 0 )
		demoScaleInx = scaleInfo_da.cnt-1;
	return TRUE;
}


EXPORT void ScaleLengthIncrement(
		SCALEINX_T scale,
		DIST_T length )
{
	char * cp;
	int len;
	if (scaleInfo(scale).length == 0.0) {
		if (units == UNITS_METRIC)
			cp = "999.99m SCALE Flex Track";
		else
			cp = "999' 11\" SCALE Flex Track";
		len = strlen( cp )+1;
		if (len > enumerateMaxDescLen)
			enumerateMaxDescLen = len;
	}
	scaleInfo(scale).length += length;
}

EXPORT void ScaleLengthEnd( void )
{
	wIndex_t si;
	int count;
	DIST_T length;
	char tmp[STR_SIZE];
	FLOAT_T flexLen;
	long flexUnit;
	FLOAT_T flexCost;
	for (si=0; si<scaleInfo_da.cnt; si++) {
		sprintf( tmp, "price list %s", scaleInfo(si).scale );
		wPrefGetFloat( tmp, "flex length", &flexLen, 0.0 );
		wPrefGetInteger( tmp, "flex unit", &flexUnit, 0 );
		wPrefGetFloat( tmp, "flex cost", &flexCost, 0.0 );
		tmp[0] = '\0';
		if ((length=scaleInfo(si).length) != 0) {
			sprintf( tmp, "%s %s Flex Track", FormatDistance(length), scaleInfo(si).scale );
			for (count = strlen(tmp); count<enumerateMaxDescLen; count++)
				tmp[count] = ' ';
			tmp[enumerateMaxDescLen] = '\0';
			count = 0;
			if (flexLen > 0.0) {
				count = (int)ceil( length / (flexLen/(flexUnit?2.54:1.00)));
			}
			EnumerateList( count, flexCost, tmp );
		}
		scaleInfo(si).length = 0;
	}
}



EXPORT void LoadScaleList( wList_p scaleList )
{
	wIndex_t inx;
	for (inx=0; inx<scaleDesc_da.cnt-(extraButtons?0:1); inx++) {
		scaleDesc(inx).index =
				wListAddValue( scaleList, scaleDesc(inx).scaleDesc, NULL, (void*)(intptr_t)inx );
	}
}

EXPORT void LoadGaugeList( wList_p gaugeList, SCALEDESCINX_T scale )
{
	wIndex_t inx;
	scaleDesc_t s;
	gaugeInfo_p g;
	dynArr_t *gauges_da_p;
	
	s = scaleDesc(scale);
	gauges_da_p = &(s.gauges_da);
	g = gauges_da_p->ptr;
	g = s.gauges_da.ptr;

	wListClear( gaugeList );			/* remove old list in case */	
	for (inx=0; inx<gauges_da_p->cnt; inx++) {		
		(g[inx]).index = wListAddValue( gaugeList, (g[inx]).gauge, NULL, (void*)(intptr_t)(g[inx]).scale ); 
	}
}

static void ScaleChange( long changes )
{
	if (changes & CHANGE_SCALE) {
		SetScale( curScaleInx );
	}
}

/*****************************************************************************
 *
 * 
 *
 */

EXPORT void Misc2Init( void )
{
	AddParam( "SCALE ", AddScale );
	wPrefGetInteger( "draw", "label-when", &labelWhen, labelWhen );
	RegisterChangeNotification( ScaleChange );
	wPrefGetInteger( "misc", "include same gauge turnouts", &includeSameGaugeTurnouts, 1 );
}
