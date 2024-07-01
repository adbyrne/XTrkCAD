/** \file scale.c
 * Management of information about scales and gauges plus rprintf.
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

#include "common.h"
#include "cundo.h"
#include "cjoin.h"
#include "compound.h"
#include "custom.h"
#include "draw.h"
#include "fileio.h"
#include "layout.h"
#include "param.h"
#include "track.h"
#include "cselect.h"
#include "common-ui.h"

/****************************************************************************
 *
 * SCALE
 *
 */

#define SCALE_DEMO	(-1)
#define SCALE_ANY	(-2)
#define SCALE_MULTI	(-3)
#define SCALE_UNKNOWN	(-4)
#define SCALE_ERROR	(-5)

typedef struct {
	char * scale;
	DIST_T ratio;
	DIST_T gauge;
	DIST_T R[3];
	DIST_T X[3];
	DIST_T L[3];
	DIST_T length;
	BOOL_T tieDataValid;
	tieData_t tieData;
} scaleInfo_t;
typedef scaleInfo_t * scaleInfo_p;
static dynArr_t scaleInfo_da;
#define scaleInfo(N) DYNARR_N( scaleInfo_t, scaleInfo_da, N )
static SCALEINX_T scaleInfo_cnt=0;

typedef struct {
	char *in_scales;
	SCALE_FIT_TYPE_T type;
	char *match_scales;
	SCALE_FIT_T result;
} scaleComp_t;
typedef scaleComp_t * scaleComp_p;
static dynArr_t scaleCompatible_da;
#define scaleComp(N) DYNARR_N( scaleComp_t, scaleCompatible_da, N )

static tieData_t tieData_demo = {
	TRUE,
	96.0/160.0,
	16.0/160.0,
	32.0/160.0
};

EXPORT DIST_T curScaleRatio;
EXPORT char * curScaleName;
static scaleInfo_p curScale;
/** @prefs [misc] include same gauge turnouts=1 Unknown */
EXPORT long includeSameGaugeTurnouts = FALSE;
static SCALEINX_T demoScaleInx = -1;


/** this struct holds a gauge description */
typedef struct {
	char * gaugeStr;		/** ptr to textual description eg. 'n3' */
	SCALEINX_T scaleInx;	/** index of complete information in scaleInfo_da */
} gaugeInfo_t;

EXPORT typedef gaugeInfo_t * gaugeInfo_p;

/** this struct holds a scale description */
typedef struct {
	char *scaleDescStr;	/** ptr to textual description eg. 'HO' */
	dynArr_t gauges_da;	/** known gauges to this scale */
} scaleDesc_t;

EXPORT typedef scaleDesc_t *scaleDesc_p;
static dynArr_t scaleDesc_da;
#define scaleDesc(N) DYNARR_N( scaleDesc_t, scaleDesc_da, N )


static BOOL_T DoSetScaleDesc( SCALEINX_T scaleInx );

static int log_scale = 0;

/**
 * Get the scale info for index
 *
 * This is over-engineered but we somehow end up with
 * out of range inx
 */
// Substitute layout scale if scaleInx is invalid
static SCALEINX_T altScaleInx = -1;
static scaleInfo_p GetScaleInfo( SCALEINX_T scaleInx )
{
	if ( scaleInx >= 0 && scaleInx < scaleInfo_da.cnt ) {
		// Good index
		LOG( log_scale, 5, ( "      GetScaleInfo-1(%d)\n", scaleInx ) );
		return &scaleInfo( scaleInx );
	}
	if ( altScaleInx >= 0 ) {
		// Previously seen bad index
		LOG( log_scale, 4, ( "      GetScaleInfo-2(%d)\n", scaleInx ) );
		return &scaleInfo( altScaleInx );
	}
	// Bad index
	altScaleInx = GetLayoutCurScale();
	int rc = NoticeMessage( MSG_BAD_SCALE_INDEX, curScaleName, "Unknown",
	                        scaleInx, curScaleName );
	if ( rc == 1 ) {
		// User ok with using layout scale
		LOG( log_scale, 3, ( "      GetScaleInfo-3(%d)\n", scaleInx ) );
		return &scaleInfo( altScaleInx );
	}
	// Clone existing layout scale as 'Unknown'
	DYNARR_APPEND( scaleInfo_t, scaleInfo_da, 1 );
	scaleInfo_p si = &DYNARR_LAST( scaleInfo_t, scaleInfo_da );
	*si = scaleInfo(altScaleInx);
	altScaleInx = scaleInfo_da.cnt-1;
	si->scale = MyStrdup( "Unknown" );
	LOG( log_scale, 3, ( "      GetScaleInfo-4(%d)\n", scaleInx ) );
	//DoSetScaleDesc( scaleInfo_da.cnt-1 );
	return si;
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

	if ( scaleInx < 0 || scaleInx >= scaleDesc_da.cnt ) {
		lprintf( "GetScaleInx: bad scaleInx %ld (%d)\n", scaleInx, scaleDesc_da.cnt );
		return 0;
	}
	s = scaleDesc(scaleInx);
	if ( gaugeInx < 0 || gaugeInx >= s.gauges_da.cnt ) {
		lprintf( "GetScaleInx: bad gaugeInx %ld (%d)\n", gaugeInx, s.gauges_da.cnt );
		return 0;
	}
	g = &(DYNARR_N(gaugeInfo_t, s.gauges_da, gaugeInx));

	return g->scaleInx;

}
EXPORT DIST_T GetScaleTrackGauge( SCALEINX_T si )
{
	return GetScaleInfo(si)->gauge;
}

EXPORT DIST_T GetScaleRatio( SCALEINX_T si )
{
	return GetScaleInfo(si)->ratio;
}

EXPORT char * GetScaleName( SCALEINX_T si )
{
	if ( si == SCALE_DEMO ) {
		return "DEMO";
	}
	if ( si == SCALE_ANY ) {
		return "*";
	}
	return GetScaleInfo(si)->scale;
}

EXPORT void GetScaleEasementValues( DIST_T * R, DIST_T * L )
{
	wIndex_t i;
	for (i=0; i<3; i++) {
		*R++ = curScale->R[i];
		*L++ = curScale->L[i];
	}
}

EXPORT DIST_T GetScaleMinRadius( SCALEINX_T si )
{
	return GetScaleInfo(si)->R[0];
}


EXPORT void ValidateTieData( tieData_p td )
{
	td->valid = (td->length > 0.05 && td->width > 0.05 && td->spacing > 0.05);
}

EXPORT tieData_t GetScaleTieData( SCALEINX_T si )
{
	scaleInfo_p s;
	DIST_T defLength;

	if ( si == SCALE_DEMO ) {
		return tieData_demo;
	}
	s = GetScaleInfo(si);
	if ( !s->tieDataValid ) {
		sprintf( message, "tiedata-%s", s->scale );
		defLength = (96.0-54.0)/s->ratio+s->gauge;

		/** @prefs [tiedata-<SCALE>] length, width, spacing Sets tie drawing data.
		* Example for 6"x8"x6' ties spaced 20" in HOn3 (slash separates 4 lines):
		* [tiedata-HOn3] \ length=0.83 \ width=0.07 \ spacing=0.23
		*/
		wPrefGetFloat( message, "length", &s->tieData.length, defLength );
		wPrefGetFloat( message, "width", &s->tieData.width, 16.0/s->ratio );
		wPrefGetFloat( message, "spacing", &s->tieData.spacing, 2*s->tieData.width );
		s->tieData.valid = TRUE;
		s->tieDataValid = TRUE;
	}
	return s->tieData;
}

void
SetScaleGauge(SCALEDESCINX_T desc, GAUGEINX_T gauge)
{
	SCALEINX_T scaleInx = GetScaleInx( desc, gauge );
	LOG( log_scale, 1, ( "SetScaleGauge(%d)\n", scaleInx ) );
	CHECK( 0 <= scaleInx && scaleInx < scaleInfo_cnt );
	SetLayoutCurScale( scaleInx );
}

static BOOL_T
SetScaleDescGauge(SCALEINX_T scaleInx)
{
	SCALEDESCINX_T scaleDescInx;
	GAUGEINX_T gaugeInx;
	if ( GetScaleGauge( scaleInx, &scaleDescInx, &gaugeInx ) ) {
		SetLayoutCurScaleDesc( scaleDescInx );
		SetLayoutCurGauge( gaugeInx );
	}
	return TRUE;
}

EXPORT SCALEINX_T LookupScale( const char * name )
{
	wIndex_t scaleInx;
	DIST_T gauge;
	LOG( log_scale, 2, ( "  LookupScale(%s)", name ) );
	if ( strcmp( name, "*" ) == 0 ) {
		LOG( log_scale, 2, ( " .any = %d\n", SCALE_ANY ) );
		return SCALE_ANY;
	}
	for ( scaleInx=0; scaleInx<scaleInfo_da.cnt; scaleInx++ ) {
		if (strcmp( scaleInfo(scaleInx).scale, name ) == 0) {
			LOG( log_scale, 2, ( " .name = %d\n", scaleInx ) );
			if ( scaleInx >= scaleInfo_cnt ) {
				// Bogus scale
				return GetLayoutCurScale();
			} else {
				return scaleInx;
			}
		}
	}
	if ( isdigit((unsigned char)name[0]) ) {
		gauge = atof( name );
		for ( scaleInx=0; scaleInx<scaleInfo_da.cnt; scaleInx++ ) {
			if (scaleInfo(scaleInx).gauge == gauge) {
				LOG( log_scale, 2, ( " .gauge = %d\n", scaleInx ) );
				return scaleInx;
			}
		}
	}
	if ( inPlayback ) {
		// Invalid SCALE in recording/demo: abort
		InputError( _("Invalid Scale: playback aborted\n  SCALE %s"), FALSE, name );
		return GetLayoutCurScale();
	}
	// Bad name - create a dummy scale
	NoticeMessage( MSG_BAD_SCALE_NAME, _("Ok"), NULL,
	               name, curScaleName );
	// Clone existing layout scale for dummy
	DYNARR_APPEND( scaleInfo_t, scaleInfo_da, 1 );
	scaleInfo_p scaleInfoP = &DYNARR_LAST( scaleInfo_t, scaleInfo_da );
	*scaleInfoP = scaleInfo(GetLayoutCurScale());
	scaleInfoP->scale = MyStrdup( name );
	LOG( log_scale, 2, ( " .new = %d\n", scaleInfo_da.cnt-1 ) );
	//DoSetScaleDesc( scaleInfo_da.cnt-1 );
//	return scaleInfo_da.cnt-1;
	return GetLayoutCurScale();
}

/*
 * Evaluate the fit of a part scale1 to a definition in scale2 for a type.
 *
 * The rules differ by type of object.
 *
 * Tracks need to be the same gauge to be a fit. If they are the same scale they are exact.
 * If the gauge is the same, but the scale is different they are compatible.
 * There are well known exceptions where the scale is not the same but we call them exact.
 *
 * Structures need to be the same scale to be exact. If they are within 15% they are compatible.
 *
 * Cars need to be the same gauge and scale to be exact.
 * If they are the same gauge, but within 15% of the scale they are compatible.
 *
 *\param type (FIT_TURNOUT,FIT_STRUCTURE,FIT_CAR)
 *\param scale1 the input scale
 *\param scale2 the scale to check against
 *
 *\return FIT_EXACT, FIT_COMPATIBLE, FIT_NONE
 */

EXPORT SCALE_FIT_T CompatibleScale(
        SCALE_FIT_TYPE_T type,
        SCALEINX_T scale1,
        SCALEINX_T scale2 )
{
	SCALE_FIT_T rc;
	if ( scale1 == scale2 ) {
		return FIT_EXACT;
	}
	if ( scale1 == SCALE_DEMO || scale2 == SCALE_DEMO ) {
		return FIT_NONE;
	}
	if ( scale1 == demoScaleInx || scale2 == demoScaleInx ) {
		return FIT_NONE;
	}
	switch(type) {
	case FIT_TURNOUT:
		if ( scale1 == SCALE_ANY ) {
			return FIT_EXACT;
		}
		if (scaleInfo(scale1).gauge == scaleInfo(scale2).gauge &&
		    scaleInfo(scale1).scale == scaleInfo(scale2).scale) {
			return FIT_EXACT;
		}

		rc = FindScaleCompatible(FIT_TURNOUT, scaleInfo(scale1).scale,
		                         scaleInfo(scale2).scale);
		if (rc != FIT_NONE) { return rc; }

		if ( includeSameGaugeTurnouts &&
		     scaleInfo(scale1).gauge == scaleInfo(scale2).gauge ) {
			return FIT_COMPATIBLE;
		}
		break;
	case FIT_STRUCTURE:
		if ( scale1 == SCALE_ANY ) {
			return FIT_EXACT;
		}
		if ( scaleInfo(scale1).ratio == scaleInfo(scale2).ratio ) {
			return FIT_EXACT;
		}

		rc = FindScaleCompatible(FIT_STRUCTURE, scaleInfo(scale1).scale,
		                         scaleInfo(scale2).scale);
		if (rc != FIT_NONE) { return rc; }

		//15% scale match is compatible for structures
		if (scaleInfo(scale1).ratio/scaleInfo(scale2).ratio>=0.85 &&
		    scaleInfo(scale1).ratio/scaleInfo(scale2).ratio<=1.15) {
			return FIT_COMPATIBLE;
		}
		break;
	case FIT_CAR:
		if ( scale1 == SCALE_ANY ) {
			return FIT_EXACT;
		}
		if (scaleInfo(scale1).gauge == scaleInfo(scale2).gauge &&
		    scaleInfo(scale1).scale == scaleInfo(scale2).scale) {
			return FIT_EXACT;
		}

		rc = FindScaleCompatible(FIT_CAR, scaleInfo(scale1).scale,
		                         scaleInfo(scale2).scale);
		if (rc != FIT_NONE) { return rc; }

		//Same gauge and 15% scale match is compatible for cars
		if (scaleInfo(scale1).gauge == scaleInfo(scale2).gauge) {
			if (scaleInfo(scale1).ratio/scaleInfo(scale2).ratio>=0.85 &&
			    scaleInfo(scale1).ratio/scaleInfo(scale2).ratio<=1.15) {
				return FIT_COMPATIBLE;
			}
		}
		break;

	default:;
	}

	return FIT_NONE;

}

/** Split the scale and the gauge description for a given combination. Eg HOn3 will be
 * split to HO and n3.
 * \param scaleInx IN  scale/gauge combination
 * \param scaleDescInx OUT  scale part
 * \param  gaugeInx OUT gauge part
 * \return TRUE
 */

EXPORT BOOL_T
GetScaleGauge( SCALEINX_T scaleInx, SCALEDESCINX_T *scaleDescInx,
               GAUGEINX_T *gaugeInx)
{

	if ( scaleInx > scaleInfo_cnt ) {
		*scaleDescInx = SCALE_UNKNOWN;
		*gaugeInx = SCALE_UNKNOWN;
		return FALSE;
	}

	for( SCALEDESCINX_T sdx = 0; sdx < scaleDesc_da.cnt; sdx++ ) {
		scaleDesc_p scaleDescP = &DYNARR_N(scaleDesc_t, scaleDesc_da, sdx );
		dynArr_t* gaugesDAP = &scaleDescP->gauges_da;
		for( GAUGEINX_T gx = 0; gx < gaugesDAP->cnt; gx++ ) {
			gaugeInfo_p gp = &(DYNARR_N( gaugeInfo_t, *gaugesDAP, gx ));
			if ( gp->scaleInx == scaleInx ) {
				*scaleDescInx = sdx;
				*gaugeInx = gx;
				return TRUE;
			}
		}
	}
	*scaleDescInx = SCALE_UNKNOWN;
	*gaugeInx = SCALE_UNKNOWN;
	return FALSE;
}

/**
 * Setup XTrkCad for the newly selected scale/gauge combination.
 *
 */

static void
SetScale( void )
{
	SCALEINX_T newScaleInx = GetLayoutCurScale();
	curScale = GetScaleInfo( newScaleInx );
	trackGauge = curScale->gauge;
	curScaleRatio = curScale->ratio;
	curScaleName = curScale->scale;

	SetLayoutCurScaleDesc( 0 );

	SetScaleDescGauge(newScaleInx);


	if (!inPlayback) {
		wPrefSetString( "misc", "scale", curScaleName );
	}

	// now load the minimum radius and set default max grade for the newly selected scale
	LoadLayoutMinRadiusPref(curScaleName, curScale->R[0]);
	LoadLayoutMaxGradePref(curScaleName, 5.0);
}

/**
 * Check the new scale value and update the program if a valid scale was passed
 *
 * \param newScale IN the name of the new scale
 * \returns TRUE if valid, FALSE otherwise
 */

static struct {
	SCALEINX_T altScaleInx;
	wIndex_t scaleInfo_cnt;
} setScaleState;

EXPORT BOOL_T DoSetScale(
        const char * newScale )
{
	SCALEINX_T scaleInx;

	if ( inPlayback ) {
		// Save old state in case we are in playback
		setScaleState.altScaleInx = altScaleInx;
		setScaleState.scaleInfo_cnt = scaleInfo_cnt;
		// Reset invalid scale check
		altScaleInx = -1;
		DYNARR_SET( scaleInfo_t, scaleInfo_da, scaleInfo_cnt );
	} else if ( inPlaybackQuit ) {
		// Restore state after playback
		altScaleInx = setScaleState.altScaleInx;
		scaleInfo_cnt = setScaleState.scaleInfo_cnt;
	} else {
		// Reset invalid scale check
		altScaleInx = -1;
		DYNARR_SET( scaleInfo_t, scaleInfo_da, scaleInfo_cnt );
	}

	scaleInx = LookupScale( newScale );
	if ( scaleInx == SCALE_ANY ) {
		// Don't know how this could happen
		scaleInx = GetLayoutCurScale();
	}
	if ( scaleInx >= scaleInfo_cnt ) {
		// Trying to set to unknown scale
		scaleInx = GetLayoutCurScale();
	}
	LOG( log_scale, 1, ( "DoSetScale(%s) = %d\n", newScale, scaleInx ) );
	CHECK( 0 <= scaleInx && scaleInx < scaleInfo_cnt );
	SetLayoutCurScale( scaleInx );
	DoChangeNotification( CHANGE_SCALE );
	return TRUE;
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

static BOOL_T DoSetScaleDesc( SCALEINX_T scaleInx )
{
	SCALEINX_T work;
	SCALEDESCINX_T descInx;
	scaleDesc_p scaleDescP = NULL;
	gaugeInfo_p g;
	char *cp;
//	DIST_T ratio;
	char buf[ 80 ];
	size_t len;


	for( descInx = 0; descInx < scaleDesc_da.cnt; descInx++ ) {
		work = GetScaleInx( descInx, 0 );
		if( scaleInfo(work).ratio == scaleInfo(scaleInx).ratio ) {
			if( !strncmp( scaleInfo(work).scale, scaleInfo(scaleInx).scale,
			              strlen(scaleInfo(work).scale))) {
				scaleDescP = &scaleDesc(descInx);
			}
		}
	}


	if( scaleDescP == NULL ) {
		/* if no, add as new scale */

		DYNARR_APPEND( scaleDesc_t, scaleDesc_da, 1 );

		scaleDescP = &(scaleDesc( scaleDesc_da.cnt-1 ));

		sprintf( buf, "%s (1/%.1f)", scaleInfo(scaleInx).scale,
		         scaleInfo(scaleInx).ratio );
		scaleDescP->scaleDescStr = MyStrdup( buf );

		sprintf( buf, "Standard (%.1fmm)", scaleInfo(scaleInx).gauge*25.4 );

	} else {
		/* if yes, is this a new gauge to the scale? */
		cp = strchr( scaleDescP->scaleDescStr, ' ' );
		if( cp ) {
			len = cp - scaleDescP->scaleDescStr;
		} else {
			len = strlen(scaleDescP->scaleDescStr);
		}
		sprintf( buf, "%s (%.1fmm)", scaleInfo(scaleInx).scale+len,
		         scaleInfo(scaleInx).gauge*25.4 );
	}
	DYNARR_APPEND( gaugeInfo_t, scaleDescP->gauges_da, 10 );
	g = &(DYNARR_N( gaugeInfo_t, scaleDescP->gauges_da,
	                (scaleDescP->gauges_da).cnt - 1 ));
	g->scaleInx = scaleInx;
	g->gaugeStr = MyStrdup( buf );

	return( TRUE );
}


EXPORT BOOL_T DoAllSetScaleDesc( void )
{
	for( SCALEINX_T scaleInx = 0; scaleInx < scaleInfo_da.cnt; scaleInx++ ) {
		DoSetScaleDesc( scaleInx );
	}
	return TRUE;
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

	if ( (rc=sscanf( line, "SCALE %[^,]," SCANF_FLOAT_FORMAT "," SCANF_FLOAT_FORMAT
	                 "",
	                 scale, &ratio, &gauge )) != 3) {
		SyntaxError( "SCALE", rc, 3 );
		return FALSE;
	}
	for (i=0; i<3; i++) {
		line = GetNextLine();
		if ( (rc=sscanf( line, "" SCANF_FLOAT_FORMAT "," SCANF_FLOAT_FORMAT ","
		                 SCANF_FLOAT_FORMAT "",
		                 &R[i], &X[i], &L[i] )) != 3 ) {
			SyntaxError( "SCALE easement", rc, 3 );
			return FALSE;
		}
	}

	// The increment size should be > # SCALEs in xtrkcad.xtq
	DYNARR_APPEND( scaleInfo_t, scaleInfo_da, 100 );
	s = &scaleInfo(scaleInfo_da.cnt-1);
	s->scale = MyStrdup( scale );
	s->ratio = ratio;
	s->gauge = gauge;
	// Remember last defined scale
	scaleInfo_cnt = scaleInfo_da.cnt;
	for (i=0; i<3; i++) {
		s->R[i] = R[i]/ratio;
		s->X[i] = X[i]/ratio;
		s->L[i] = L[i]/ratio;
	}
	s->tieDataValid = FALSE;
	if ( strcmp( scale, "DEMO" ) == 0 ) {
		demoScaleInx = scaleInfo_da.cnt-1;
	}
	return TRUE;
}

static BOOL_T AddScaleFit(
        char * line)
{
	char scales[STR_SIZE], matches[STR_SIZE], type[20], result[20];
	BOOL_T rc;
	scaleComp_p s;

	if ( (rc=sscanf( line, "SCALEFIT %s %s %s %s",
	                 type, result, scales, matches )) != 4) {
		SyntaxError( "SCALEFIT", rc, 4 );
		return FALSE;
	}
	DYNARR_APPEND( scaleComp_t, scaleCompatible_da, 10 );
	s = &scaleComp(scaleCompatible_da.cnt-1);
	s->in_scales = MyStrdup(scales);
	s->match_scales = MyStrdup(matches);
	if (strcmp(type,"STRUCTURE") == 0) {
		s->type = FIT_STRUCTURE;
	} else if (strcmp(type,"TURNOUT")==0) {
		s->type = FIT_TURNOUT;
	} else if (strcmp(type,"CAR")==0) {
		s->type = FIT_CAR;
	} else {
		InputError( "Invalid SCALEFIT type %s", TRUE, type );
		return FALSE;
	}
	if (strcmp(result,"COMPATIBLE")==0) {
		s->result = FIT_COMPATIBLE;
	} else if (strcmp(result,"EXACT")==0) {
		s->result = FIT_EXACT;
	} else {
		InputError( "Invalid SCALEFIT result %s", TRUE, result );
		return FALSE;
	}

	return TRUE;
}

EXPORT SCALE_FIT_T FindScaleCompatible(SCALE_FIT_TYPE_T type, char * scale1,
                                       char * scale2)
{

	char * cp, * cq;

	if (!scale1 || !scale1[0]) { return FIT_NONE; }
	if (!scale2 || !scale2[0]) { return FIT_NONE; }

	for (int i=0; i<scaleCompatible_da.cnt; i++) {
		scaleComp_p s;
		s = &scaleComp(i);
		if (s->type != type) { continue; }
		BOOL_T found = FALSE;
		cp = s->in_scales;
		//Match input scale
		while (cp) {
			//Next instance of needle in haystack
			cp = strstr(cp,scale2);
			if (!cp) { break; }
			//Check that this is start of csv string
			if (cp == s->in_scales || cp[-1] == ',') {
				//Is this end of haystack?
				if (strlen(cp) == strlen(scale2)) {
					found = TRUE;
					break;
				}
				//Is it the same until the next ','
				cq=strstr(cp,",");
				if (cq && (cq-cp == strlen(scale2))) {
					found = TRUE;
					break;
				} else { cp=cq; }
			} else { cp=strstr(cp,","); }
		}
		if (!found) { continue; }
		found = FALSE;
		cp = s->match_scales;
		//Match output scale
		while (cp) {
			//Next instance of needle in haystack
			cp = strstr(cp,scale1);
			if (!cp) { break; }
			//Check that this is start of csv string
			if (cp == s->match_scales || cp[-1] == ',') {
				//Is this end of haystack?
				if (strlen(cp) == strlen(scale1)) {
					found = TRUE;
					break;
				}
				//Is it the same until the next ','
				cq=strstr(cp,",");
				if (cq && (cq-cp == strlen(scale1))) {
					found = TRUE;
					break;
				} else { cp=cq; }
			} else { cp=strstr(cp,","); }
		}
		if (!found) { continue; }
		return s->result;
	}
	return FIT_NONE;
}

EXPORT void ScaleLengthIncrement(
        SCALEINX_T scale,
        DIST_T length )
{
	char * cp;
	size_t len;
	if (scaleInfo(scale).length == 0.0) {
		if (units == UNITS_METRIC) {
			cp = "999.99m SCALE Flex Track";
		} else {
			cp = "999' 11\" SCALE Flex Track";
		}
		len = strlen( cp )+1;
		if (len > enumerateMaxDescLen) {
			enumerateMaxDescLen = (int)len;
		}
	}
	scaleInfo(scale).length += length;
}

EXPORT void ScaleLengthEnd( void )
{
	wIndex_t si;
	size_t count;
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
			for (count = strlen(tmp); count<enumerateMaxDescLen; count++) {
				tmp[count] = ' ';
			}
			tmp[enumerateMaxDescLen] = '\0';
			count = 0;
			if (flexLen > 0.0) {
				count = (int)ceil( length / (flexLen/(flexUnit?2.54:1.00)));
			}
			EnumerateList( (long)count, flexCost, tmp, NULL );
		}
		scaleInfo(si).length = 0;
	}
}



EXPORT void LoadScaleList( wList_p scaleList )
{
	wIndex_t inx;
	for (inx=0; inx<scaleDesc_da.cnt-(extraButtons?0:1); inx++) {
		wListAddValue( scaleList, scaleDesc(inx).scaleDescStr, NULL, I2VP(inx) );
	}
}

EXPORT void LoadGaugeList( wList_p gaugeList, SCALEDESCINX_T scale )
{
	wListClear( gaugeList );			/* remove old list in case */
	for ( wIndex_t inx=0; inx<scaleDesc(scale).gauges_da.cnt; inx++ ) {
		gaugeInfo_p g = &DYNARR_N( gaugeInfo_t, scaleDesc(scale).gauges_da, inx );
		wListAddValue( gaugeList, g->gaugeStr, NULL, I2VP(g->scaleInx) );
	}
}

static void ScaleChange( long changes )
{
	if (changes & CHANGE_SCALE) {
		SetScale();
	}
}

/*****************************************************************************
 *
 * Change Scale Dlg
 *
 */

static char rescaleFromScaleStr[20];
static char rescaleFromGaugeStr[20];

static char * rescaleToggleLabels[] = { N_("Scale"), N_("Ratio"), NULL };
static long rescaleMode;
static wIndex_t rescaleFromScaleInx;
static wIndex_t rescaleFromGaugeInx;
static SCALEDESCINX_T rescaleToScaleInx;
static GAUGEINX_T rescaleToGaugeInx;
static wIndex_t rescaleToInx;
static long rescaleNoChangeDim = FALSE;
static FLOAT_T rescalePercent;
static char * rescaleChangeDimLabels[] = { N_("Do not resize track"), NULL };
static paramFloatRange_t r0o001_10000 = { 0.001, 10000.0 };
static paramData_t rescalePLs[] = {
#define I_RESCALE_MODE		(0)
	{ PD_RADIO, &rescaleMode, "toggle", PDO_NOPREF, &rescaleToggleLabels, N_("Rescale by:"), BC_HORZ|BC_NOBORDER },
#define I_RESCALE_FROM_SCALE		(1)
	{ PD_STRING, rescaleFromScaleStr, "fromS", PDO_NOPREF|PDO_STRINGLIMITLENGTH, I2VP(100), N_("From:"),0, 0, sizeof(rescaleFromScaleStr)},
#define I_RESCALE_FROM_GAUGE		(2)
	{ PD_STRING, rescaleFromGaugeStr, "fromG", PDO_NOPREF|PDO_DLGHORZ | PDO_STRINGLIMITLENGTH, I2VP(100), " / ", 0, 0, sizeof(rescaleFromGaugeStr)},
#define I_RESCALE_TO_SCALE		   (3)
	{ PD_DROPLIST, &rescaleToScaleInx, "toS", PDO_NOPREF|PDO_LISTINDEX, I2VP(100), N_("To: ") },
#define I_RESCALE_TO_GAUGE		   (4)
	{ PD_DROPLIST, &rescaleToGaugeInx, "toG", PDO_NOPREF|PDO_LISTINDEX|PDO_DLGHORZ, NULL, " / " },
#define I_RESCALE_CHANGE	(5)
	{ PD_TOGGLE, &rescaleNoChangeDim, "change-dim", 0, &rescaleChangeDimLabels, "", BC_HORZ|BC_NOBORDER },
#define I_RESCALE_PERCENT	(6)
	{ PD_FLOAT, &rescalePercent, "ratio", 0, &r0o001_10000, N_("Ratio") },
	{ PD_MESSAGE, "%", NULL, PDO_DLGHORZ }
};
static paramGroup_t rescalePG = { "rescale", 0, rescalePLs, COUNT( rescalePLs ) };


static coOrd rescaleShift;
static BOOL_T RescaleDoIt( track_p trk, BOOL_T unused )
{
	EPINX_T ep, ep1;
	track_p trk1;
	UndrawNewTrack( trk );
	UndoModify(trk);
	if ( rescalePercent != 100.0 ) {
		for (ep=0; ep<GetTrkEndPtCnt(trk); ep++) {
			if ((trk1 = GetTrkEndTrk(trk,ep)) != NULL &&
			    !GetTrkSelected(trk1)) {
				ep1 = GetEndPtConnectedToMe( trk1, trk );
				DisconnectTracks( trk, ep, trk1, ep1 );
			}
		}
		/* should the track dimensions ie. length or radius be changed as well? */
		if( rescaleNoChangeDim == 0 ) {
			RescaleTrack( trk, rescalePercent/100.0, rescaleShift );
		}
	}

	if ( rescaleMode==0 ) {
		SetTrkScale( trk, rescaleToInx );
	}
	DrawNewTrack( trk );
	return TRUE;
}


static void RescaleDlgOk(
        void * unused )
{
	coOrd center, size;
	DIST_T d;
	FLOAT_T ratio = rescalePercent/100.0;
	coOrd getboundsLo, getboundsHi;

	UndoStart( _("Rescale Tracks"), "Rescale" );
	GetSelectedBounds( &getboundsLo, &getboundsHi );
	center.x = (getboundsLo.x+getboundsHi.x)/2.0;
	center.y = (getboundsLo.y+getboundsHi.y)/2.0;
	size.x = (getboundsHi.x-getboundsLo.x)/2.0*ratio;
	size.y = (getboundsHi.y-getboundsLo.y)/2.0*ratio;
	getboundsLo.x = center.x - size.x;
	getboundsLo.y = center.y - size.y;
	getboundsHi.x = center.x + size.x;
	getboundsHi.y = center.y + size.y;
	if ( getboundsLo.x < 0 ) {
		getboundsHi.x -= getboundsLo.x;
		getboundsLo.x = 0;
	} else if ( getboundsHi.x > mapD.size.x ) {
		d = getboundsHi.x - mapD.size.x;
		if ( getboundsLo.x < d ) {
			d = getboundsLo.x;
		}
		getboundsHi.x -= d;
		getboundsLo.x -= d;
	}
	if ( getboundsLo.y < 0 ) {
		getboundsHi.y -= getboundsLo.y;
		getboundsLo.y = 0;
	} else if ( getboundsHi.y > mapD.size.y ) {
		d = getboundsHi.y - mapD.size.y;
		if ( getboundsLo.y < d ) {
			d = getboundsLo.y;
		}
		getboundsHi.y -= d;
		getboundsLo.y -= d;
	}
	if ( rescaleNoChangeDim == 0 &&
	     (getboundsHi.x > mapD.size.x ||
	      getboundsHi.y > mapD.size.y )) {
		NoticeMessage( MSG_RESCALE_TOO_BIG, _("Ok"), NULL,
		               FormatDistance(getboundsHi.x), FormatDistance(getboundsHi.y) );
	}
	rescaleShift.x = (getboundsLo.x+getboundsHi.x)/2.0 - center.x*ratio;
	rescaleShift.y = (getboundsLo.y+getboundsHi.y)/2.0 - center.y*ratio;

	rescaleToInx = GetScaleInx( rescaleToScaleInx, rescaleToGaugeInx );
	DoSelectedTracks( RescaleDoIt );

	// rescale the background if it exists and the layout is resized
	if (HasBackGround() && ratio != 1.0) {
		coOrd pos = GetLayoutBackGroundPos();
		double size = GetLayoutBackGroundSize();
		pos.x = ratio * pos.x + rescaleShift.x;
		pos.y = ratio * pos.y + rescaleShift.y;
		SetLayoutBackGroundPos(pos);

		size *= ratio;
		SetLayoutBackGroundSize(size);
	}
	DoRedraw();
	wHide( rescalePG.win );
}


static void RescaleDlgUpdate(
        paramGroup_p pg,
        int inx,
        void * valueP )
{
	switch (inx) {
	case I_RESCALE_MODE:
		wControlShow( pg->paramPtr[I_RESCALE_FROM_SCALE].control, rescaleMode==0 );
		wControlActive( pg->paramPtr[I_RESCALE_FROM_SCALE].control, FALSE );
		wControlShow( pg->paramPtr[I_RESCALE_TO_SCALE].control, rescaleMode==0 );
		wControlShow( pg->paramPtr[I_RESCALE_FROM_GAUGE].control, rescaleMode==0 );
		wControlActive( pg->paramPtr[I_RESCALE_FROM_GAUGE].control, FALSE );
		wControlShow( pg->paramPtr[I_RESCALE_TO_GAUGE].control, rescaleMode==0 );
		wControlShow( pg->paramPtr[I_RESCALE_CHANGE].control, rescaleMode==0 );
		wControlActive( pg->paramPtr[I_RESCALE_PERCENT].control, rescaleMode==1 );
		if ( rescaleMode!=0 ) {
			break;
		}
	case I_RESCALE_TO_SCALE:
		LoadGaugeList( (wList_p)rescalePLs[I_RESCALE_TO_GAUGE].control,
		               *((int *)valueP) );
		rescaleToGaugeInx = 0;
		ParamLoadControl( pg, I_RESCALE_TO_GAUGE );
		ParamLoadControl( pg, I_RESCALE_TO_SCALE );
		if ( rescaleFromScaleInx >= 0 ) {
			rescalePercent = GetScaleRatio(GetScaleInx(rescaleFromScaleInx,0))/
			                 GetScaleRatio(GetScaleInx(rescaleToScaleInx,0))*100.0;
		} else {
			rescalePercent = 100.0;
		}
		wControlActive( pg->paramPtr[I_RESCALE_CHANGE].control,
		                (rescaleFromScaleInx != rescaleToScaleInx) );
		ParamLoadControl( pg, I_RESCALE_PERCENT );
		break;
	case I_RESCALE_TO_GAUGE:
		ParamLoadControl( pg, I_RESCALE_TO_GAUGE );
		break;
	case I_RESCALE_FROM_SCALE:
		ParamLoadControl( pg, I_RESCALE_FROM_SCALE );
		break;
	case I_RESCALE_FROM_GAUGE:
		ParamLoadControl( pg, I_RESCALE_FROM_GAUGE );
		break;
	case I_RESCALE_CHANGE:
		ParamLoadControl( pg, I_RESCALE_CHANGE );
		break;
	case -1:
		break;
	}
	wBool_t bOkActive = TRUE;
	if ( rescaleMode == 0 ) {
		// Scale
		bOkActive = rescaleFromScaleInx != rescaleToScaleInx ||
		            rescaleFromGaugeInx != rescaleToGaugeInx;
	} else {
		// Ratio
		bOkActive = rescalePercent != 100.0;
	}
	ParamDialogOkActive( pg, bOkActive );
}

/**
 * Get the scale gauge information for the selected track pieces.
 * FIXME: special cases like tracks pieces with different gauges or scale need to be handled
 *
 * \param IN trk track element
 * \param IN unused
 * \return TRUE;
 */

static BOOL_T SelectedScaleGauge( track_p trk, BOOL_T unused )
{
	char *scaleName;
	SCALEINX_T scale;
	SCALEDESCINX_T scaleInx;
	GAUGEINX_T gaugeInx;

	scale = GetTrkScale( trk );
	scaleName = GetScaleName( scale );
	if( strcmp( scaleName, "*" )) {
		GetScaleGauge( scale, &scaleInx, &gaugeInx );
		// Determine scale
		if ( SCALE_ANY == rescaleFromScaleInx ) {
			// First time
			rescaleFromScaleInx = scaleInx;
			rescaleFromGaugeInx = gaugeInx;
		} else if ( SCALE_MULTI == rescaleFromScaleInx ) {
			// we 've seen Mixed scales
		} else if ( scaleInx != rescaleFromScaleInx ) {
			// mixed scales
			rescaleFromScaleInx = SCALE_MULTI;
			rescaleFromGaugeInx = SCALE_MULTI;
		} else if ( gaugeInx != rescaleFromGaugeInx ) {
			// same scale but different gauge
			rescaleFromGaugeInx = SCALE_MULTI;
		}
		CHECK( SCALE_ANY != rescaleFromScaleInx );
	}

	return TRUE;
}

/**
 * Bring up the rescale dialog. The dialog for rescaling the selected pieces
 * of track is created if necessary and shown. Handling of user input is done via
 * RescaleDlgUpdate()
 */

EXPORT void DoRescale( void * unused )
{
	if ( rescalePG.win == NULL ) {
		ParamCreateDialog( &rescalePG, MakeWindowTitle(_("Rescale")), _("Ok"),
		                   RescaleDlgOk, ParamCancel_Current, TRUE, NULL, F_BLOCK, RescaleDlgUpdate );
		LoadScaleList( (wList_p)rescalePLs[I_RESCALE_TO_SCALE].control );
		LoadGaugeList( (wList_p)rescalePLs[I_RESCALE_TO_GAUGE].control,
		               GetLayoutCurScaleDesc() ); /* set correct gauge list here */
		rescaleFromScaleInx = GetLayoutCurScale();
		rescaleToScaleInx = rescaleFromScaleInx;
		rescalePercent = 100.0;
	}

	// Get From scale and gauge
	rescaleFromScaleInx = SCALE_ANY;
	rescaleFromGaugeInx = SCALE_ANY;
	DoSelectedTracks( SelectedScaleGauge );
	if ( SCALE_ANY == rescaleFromScaleInx ) {
		strcpy( rescaleFromScaleStr, "*" );
		strcpy( rescaleFromGaugeStr, "" );
	} else if ( SCALE_MULTI == rescaleFromScaleInx ) {
		strcpy( rescaleFromScaleStr, "Multi-Scale" );
		strcpy( rescaleFromGaugeStr, "" );
		strcpy( rescaleFromGaugeStr, "" );
	} else if ( SCALE_UNKNOWN == rescaleFromScaleInx ) {
		strcpy( rescaleFromScaleStr, "Unknown" );
		strcpy( rescaleFromGaugeStr, "" );
	} else {
		scaleDesc_t* scaleDescP = &scaleDesc(rescaleFromScaleInx);
		strcpy( rescaleFromScaleStr, scaleDescP->scaleDescStr );
		CHECK( SCALE_ANY != rescaleFromGaugeInx );
		if ( SCALE_MULTI == rescaleFromGaugeInx ) {
			strcpy( rescaleFromGaugeStr, "Multi-Gauge" );
		} else {
			gaugeInfo_p gaugeInfoP = &(DYNARR_N(gaugeInfo_t, scaleDescP->gauges_da,
			                                    rescaleFromGaugeInx));
			strcpy( rescaleFromGaugeStr, gaugeInfoP->gaugeStr );
		}
	}

	// get To scale and gauge (current)
	GetScaleGauge( GetLayoutCurScale(), &rescaleToScaleInx, &rescaleToGaugeInx );

	RescaleDlgUpdate( &rescalePG, I_RESCALE_MODE, &rescaleMode );
	RescaleDlgUpdate( &rescalePG, I_RESCALE_CHANGE, &rescaleMode );

	RescaleDlgUpdate( &rescalePG, I_RESCALE_FROM_SCALE, rescaleFromScaleStr );
	RescaleDlgUpdate( &rescalePG, I_RESCALE_FROM_GAUGE, rescaleFromGaugeStr );

	// TODO: rescale demo shows blank because DEMO scale inx is beyond the drop box entries
	RescaleDlgUpdate( &rescalePG, I_RESCALE_TO_SCALE, &rescaleToScaleInx );
	RescaleDlgUpdate( &rescalePG, I_RESCALE_TO_GAUGE, &rescaleToGaugeInx );

	InfoMessage( _("%ld Objects to be rescaled"), selectedTrackCount );
	wShow( rescalePG.win );
	InfoMessage( "" );
}

/*****************************************************************************
 *
 *
 *
 */

EXPORT void ScaleInit( void )
{
	AddParam( "SCALE ", AddScale );
	AddParam( "SCALEFIT", AddScaleFit);
	RegisterChangeNotification( ScaleChange );
	wPrefGetInteger( "misc", "include same gauge turnouts",
	                 &includeSameGaugeTurnouts, 1 );
	ParamRegister( &rescalePG );
	log_scale = LogFindIndex( "scale" );
}
