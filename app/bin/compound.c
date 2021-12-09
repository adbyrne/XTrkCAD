/** \file compound.c
 * Compound tracks: Turnouts and Structures
 *
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

#include "tbezier.h"
#include "cjoin.h"
#include "common.h"
#include "compound.h"
#include "cundo.h"
#include "dynstring.h"
#include "fileio.h"
#include "shrtpath.h"
#include "track.h"
#include "draw.h"
#include "include/paramfile.h"
#include "common-ui.h"

/*****************************************************************************
 *
 * Paths
 *
 */


#ifndef NEWPATH
/* GetPaths()
 *
 * Return the paths for 'trk'.
 *
 * \param trk IN Get paths for track 'trk'
 */
EXPORT PATHPTR_T GetPaths( track_p trk )
{
	struct extraDataCompound_t * xx = GET_EXTRA_DATA( trk, T_NOTRACK, extraDataCompound_t );
	if ( GetTrkType(trk) == T_STRUCTURE && xx->paths != NULL ) {
		LogPrintf( "GetPaths( STRUCTURE, paths!=NULL )\n" );
	}
	if ( GetTrkType(trk) == T_TURNOUT && xx->paths == NULL ) {
		LogPrintf( "GetPaths( TURNOUT, paths==NULL )\n" );
	}
	return xx->paths;
}
#endif

/* GetPathsLength()
 *
 * Return the length of the paths object
 *
 * \param paths IN paths object
 */
EXPORT wIndex_t GetPathsLength( PATHPTR_T paths )
{
	PATHPTR_T pp;
	ASSERT( paths != NULL );
	for ( pp = paths; pp[0]; pp+=2 )
		for ( pp += strlen( (char*)pp ); pp[0] || pp[1]; pp++ );
	return (wIndex_t)(pp - paths + 1);
}


#ifndef NEWPATH
/* SetPaths()
 *
 * Set the paths for 'trk'.
 * Called when paths are read from a layout file, copied from a param def'n or
 * from a Spilt turnout.
 *
 * \param trk IN
 * \param paths IN
 */
EXPORT void SetPaths( track_p trk, PATHPTR_T paths )
{
	if ( GetTrkType(trk) == T_STRUCTURE && paths != NULL ) {
		LogPrintf( "SetPaths( STRUCTURE, paths!=NULL )\n" );
	}
	if ( GetTrkType(trk) == T_TURNOUT && paths == NULL ) {
		LogPrintf( "SetPaths( TURNOUT, paths==NULL )\n" );
	}

	struct extraDataCompound_t * xx = GET_EXTRA_DATA( trk, T_NOTRACK, extraDataCompound_t );
	if ( xx->paths )
		MyFree( xx->paths );
	if ( paths == NULL ) {
		xx->paths = NULL;
	} else {
		wIndex_t pathLen = GetPathsLength( paths );
		xx->paths = memdup( paths, pathLen * sizeof *xx->paths );
	}
	xx->currPath = NULL;
	xx->currPathIndex = 0;
}
#endif


/* GetCurrPath()
 *
 * Return the current path for 'trk'.
 * Current path is the .currPathIndex'th path
 * If the .currPathIndex is greater then the number of paths, return the first
 *
 * \param trk IN
 */
EXPORT PATHPTR_T GetCurrPath( track_p trk )
{
	struct extraDataCompound_t * xx = GET_EXTRA_DATA( trk, T_TURNOUT, extraDataCompound_t );
	if ( xx->currPath )
		return xx->currPath;
	PATHPTR_T path = GetPaths( trk );
	for ( wIndex_t position = xx->currPathIndex;
		position > 0 && path[0];
		path+=2, position-- ) {
		for ( path += strlen( (char*)path ); path[0] || path[1]; path++ );
	}
	if ( !path[0] ) {
		xx->currPathIndex = 0;
		path = GetPaths( trk );
	}
	xx->currPath = path;
	return xx->currPath;
}


EXPORT long GetCurrPathIndex( track_p trk )
{
	if ( GetTrkType( trk ) != T_TURNOUT )
		return 0;
	struct extraDataCompound_t * xx = GET_EXTRA_DATA( trk, T_TURNOUT, extraDataCompound_t );
	return xx->currPathIndex;
}


EXPORT void SetCurrPathIndex( track_p trk, long position )
{
	if ( GetTrkType( trk ) != T_TURNOUT )
		return;
	struct extraDataCompound_t * xx = GET_EXTRA_DATA( trk, T_TURNOUT, extraDataCompound_t );
	xx->currPathIndex = position;
	xx->currPath = NULL;
}

#ifndef NEWPATH
/* GetParamPaths()
 *
 * Return the paths for turnout parameter 'to'.
 *
 * \param to IN
 */
PATHPTR_T GetParamPaths( turnoutInfo_t * to )
{
	return to->paths;
}

/* SetParamPaths()
 *
 * Set paths for a Turnout Parameter 'to'
 * Used when creating a new turnout def'n
 * 
 * \param to IN
 * \param paths IN
 */
void SetParamPaths( turnoutInfo_t * to, PATHPTR_T paths )
{
	if ( paths ) {
		wIndex_t len = GetPathsLength(paths);
		to->paths = (PATHPTR_T)memdup( paths, len * ( sizeof * to->paths ) );
	} else {
		to->paths = NULL;
	}
}
#endif

/*****************************************************************************
 *
 *
 *
 */

EXPORT BOOL_T WriteCompoundPathsEndPtsSegs(
		FILE * f,
		PATHPTR_T paths,
		wIndex_t segCnt,
		trkSeg_p segs,
		EPINX_T endPtCnt,
		trkEndPt_t * endPts )
{
	int i;
	PATHPTR_T pp;

	BOOL_T rc = TRUE;
	if ( paths ) {
		for ( pp=paths; *pp; pp+=2 ) {
			rc &= fprintf( f, "\tP \"%s\"", pp )>0;
			for ( pp+=strlen((char *)pp)+1; pp[0]!=0 || pp[1]!=0; pp++ )
				rc &= fprintf( f, " %d", pp[0] )>0;
			rc &= fprintf( f, "\n" )>0;
		}
	}
	for ( i=0; i<endPtCnt; i++ )
		rc &= fprintf( f, "\tE %0.6f %0.6f %0.6f\n",
				endPts[i].pos.x, endPts[i].pos.y, endPts[i].angle )>0;
	rc &= WriteSegs( f, segCnt, segs )>0;
	return rc;
}


EXPORT void ParseCompoundTitle(
		char * title,
		char * * manufP,
		int * manufL,
		char * * nameP,
		int * nameL,
		char * * partnoP,
		int * partnoL )
{
	char * cp1, *cp2;
	size_t len;
	*manufP = *nameP = *partnoP = NULL;
	*manufL = *nameL = *partnoL = 0;
	len = strlen( title );
	cp1 = strchr( title, '\t' );
	if ( cp1 ) {
		cp2 = strchr( cp1+1, '\t' );
		if ( cp2 ) {
			cp2++;
			*partnoP = cp2;
			*partnoL = (int)(title+len-cp2);
			len = cp2-title-1;
		}
		cp1++;
		*nameP = cp1;
		*nameL = (int)(title+len-cp1);
		*manufP = title;
		*manufL = (int)(cp1-title-1);
	} else {
		*nameP = title;
		*nameL = (int)len;
	}
}


void FormatCompoundTitle(
		long format,
		char * title )
{
	char *cp1, *cp2=NULL, *cq;
	size_t len;
	FLOAT_T price;
	BOOL_T needSep;
	cq = message;
	if (format&LABEL_COST) {
		FormatCompoundTitle( LABEL_MANUF|LABEL_DESCR|LABEL_PARTNO, title );
		wPrefGetFloat( "price list", message, &price, 0.0 );
		if (price > 0.00) {
			sprintf( cq, "%7.2f\t", price );
		} else {
			strcpy( cq, "\t" );
		}
		cq += strlen(cq);
	}
	cp1 = strchr( title, '\t' );
	if ( cp1 != NULL )
		cp2 = strchr( cp1+1, '\t' );
	if (cp2 == NULL) {
		if ( (format&LABEL_TABBED) ) {
			*cq++ = '\t';
			*cq++ = '\t';
		}
		strcpy( cq, title );
	} else {
		len = 0;
		needSep = FALSE;
		if ((format&LABEL_MANUF) && cp1-title>1) {
			len = cp1-title;
			memcpy( cq, title, len );
			cq += len;
			needSep = TRUE;
		}
		if ( (format&LABEL_TABBED) ) {
			*cq++ = '\t';
			needSep = FALSE;
		}
		if ((format&LABEL_PARTNO) && *(cp2+1)) {
			if ( needSep ) {
				*cq++ = ' ';
				needSep = FALSE;
			}
			strcpy( cq, cp2+1 );
			cq += strlen( cq );
			needSep = TRUE;
		}
		if ( (format&LABEL_TABBED) ) {
			*cq++ = '\t';
			needSep = FALSE;
		}
		if ((format&LABEL_DESCR) || !(format&LABEL_PARTNO)) {
			if ( needSep ) {
				*cq++ = ' ';
				needSep = FALSE;
			}
			if ( (format&LABEL_FLIPPED) ) {
				memcpy( cq, "Flipped ", 8 );
				cq += 8;
			}
			if ( (format&LABEL_UNGROUPED) ) {
				memcpy( cq, "Ungrouped ", 10 );
				cq += 10;
			}
			if ( (format&LABEL_SPLIT) ) {
				memcpy( cq, "Split ", 6 );
				cq += 6;
			}
			memcpy( cq, cp1+1, cp2-cp1-1 );
			cq += cp2-cp1-1;
			needSep = TRUE;
		}
		*cq = '\0';
	}
}



void ComputeCompoundBoundingBox(
		track_p trk )
{
	struct extraDataCompound_t *xx;
	coOrd hi, lo;

	xx = GET_EXTRA_DATA(trk, T_NOTRACK, extraDataCompound_t);

	GetSegBounds( xx->orig, xx->angle, xx->segCnt, xx->segs, &lo, &hi );
	hi.x += lo.x;
	hi.y += lo.y;
	SetBoundingBox( trk, hi, lo );
}


turnoutInfo_t * FindCompound( long type, char * scale, char * title )
{
	turnoutInfo_t * to;
	wIndex_t inx;
	SCALEINX_T scaleInx;

	if ( scale )
		scaleInx = LookupScale( scale );
	else
		scaleInx = -1;
	if ( type&FIND_TURNOUT )
	for (inx=0; inx<turnoutInfo_da.cnt; inx++) {
		to = turnoutInfo(inx);
		if ( IsParamValid(to->paramFileIndex) &&
			 to->segCnt > 0 &&
			 (scaleInx == -1 || to->scaleInx == scaleInx ) &&
			 to->segCnt != 0 &&
			 strcmp( to->title, title ) == 0 ) {
			return to;
		}
	}
	if ( type&FIND_STRUCT )
	for (inx=0; inx<structureInfo_da.cnt; inx++) {
		to = structureInfo(inx);
		if ( IsParamValid(to->paramFileIndex) &&
			 to->segCnt > 0 &&
			 (scaleInx == -1 || to->scaleInx == scaleInx ) &&
			 to->segCnt != 0 &&
			 strcmp( to->title, title ) == 0 ) {
			return to;
		}
	}
	return NULL;
}


char * CompoundGetTitle( turnoutInfo_t * to )
{
	return to->title;
}


EXPORT void CompoundClearDemoDefns( void )
{
	turnoutInfo_t * to;
	wIndex_t inx;

	for (inx=0; inx<turnoutInfo_da.cnt; inx++) {
		to = turnoutInfo(inx);
		if ( to->paramFileIndex == PARAM_CUSTOM && strcasecmp( GetScaleName(to->scaleInx), "DEMO" ) == 0 )
			to->segCnt = 0;
	}
	for (inx=0; inx<structureInfo_da.cnt; inx++) {
		to = structureInfo(inx);
		if ( to->paramFileIndex == PARAM_CUSTOM	 && strcasecmp( GetScaleName(to->scaleInx), "DEMO" ) == 0 )
			to->segCnt = 0;
	}
}

/*****************************************************************************
 *
 * Descriptions
 *
 */

void SetDescriptionOrig(
		track_p trk )
{
	struct extraDataCompound_t *xx = GET_EXTRA_DATA(trk, T_NOTRACK, extraDataCompound_t);
	int i, j;
	coOrd p0, p1;

	for (i=0,j=-1;i<xx->segCnt;i++) {
		if ( IsSegTrack( &xx->segs[i] ) ) {
			if (j == -1) {
				j = i;
			} else {
				j = -1;
				break;
			}
		}
	}
	if (j != -1 && xx->segs[j].type == SEG_CRVTRK) {
		REORIGIN( p0, xx->segs[j].u.c.center, xx->angle, xx->orig )
		Translate( &p0, p0,
				xx->segs[j].u.c.a0 + xx->segs[j].u.c.a1/2.0 + xx->angle,
				fabs(xx->segs[j].u.c.radius) );

	} else {
		GetBoundingBox( trk, (&p0), (&p1) );
		p0.x = (p0.x+p1.x)/2.0;
		p0.y = (p0.y+p1.y)/2.0;
	}
	Rotate( &p0, xx->orig, -xx->angle );
	xx->descriptionOrig.x = p0.x - xx->orig.x;
	xx->descriptionOrig.y = p0.y - xx->orig.y;
}


void DrawCompoundDescription(
		track_p trk,
		drawCmd_p d,
		wDrawColor color )
{
	wFont_p fp;
	coOrd p1;
	struct extraDataCompound_t *xx = GET_EXTRA_DATA(trk, T_NOTRACK, extraDataCompound_t);
	char * desc;
	long layoutLabelsOption = layoutLabels;

	if (layoutLabels == 0)
		return;
	if ((labelEnable&LABELENABLE_TRKDESC)==0)
		return;
	if ( (d->options&(DC_SIMPLE|DC_SEGTRACK)) )
		return;
	if ( xx->special == TOpier ) {
		desc = xx->u.pier.name;
	} else {
		if ( xx->flipped )
			layoutLabelsOption |= LABEL_FLIPPED;
		if ( xx->ungrouped )
			layoutLabelsOption |= LABEL_UNGROUPED;
		if ( xx->split )
			layoutLabelsOption |= LABEL_SPLIT;
		FormatCompoundTitle( layoutLabelsOption, xtitle(xx) );
		desc = message;
	}
	p1 = xx->descriptionOrig;
	Rotate( &p1, zero, xx->angle );
	coOrd p0;
	p0.x = p1.x+xx->orig.x;
	p0.y = p1.y+xx->orig.y;
	p1.x += xx->orig.x + xx->descriptionOff.x;
	p1.y += xx->orig.y + xx->descriptionOff.y;
	if (color == drawColorPreviewSelected) {
		DrawLine( d, p0, p1, 0, color );
	}
	fp = wStandardFont( F_TIMES, FALSE, FALSE );
	DrawBoxedString( (xx->special==TOpier)?BOX_INVERT:BOX_NONE, d, p1, desc, fp, (wFontSize_t)descriptionFontSize, color, 0.0 );
}


DIST_T CompoundDescriptionDistance(
		coOrd pos,
		track_p trk,
		coOrd * dpos,
		BOOL_T show_hidden,
		BOOL_T * hidden)
{
	coOrd p1;
	if (GetTrkType(trk) != T_TURNOUT && GetTrkType(trk) != T_STRUCTURE)
		return DIST_INF;
	struct extraDataCompound_t *xx = GET_EXTRA_DATA(trk, T_NOTRACK, extraDataCompound_t);
	if ( ((GetTrkBits( trk ) & TB_HIDEDESC) != 0 ) && !show_hidden)
		return DIST_INF;
	p1 = xx->descriptionOrig;
	coOrd offset = xx->descriptionOff;
	if ( (GetTrkBits( trk ) & TB_HIDEDESC) != 0 ) offset = zero;
	Rotate( &p1, zero, xx->angle );
	p1.x += xx->orig.x + offset.x;
	p1.y += xx->orig.y + offset.y;
	if (hidden) *hidden = (GetTrkBits( trk ) & TB_HIDEDESC);
	*dpos = p1;

	coOrd tpos = pos;
	if (DistanceCompound(trk,&tpos)<FindDistance( p1, pos ))
		return DistanceCompound(trk,&pos);
	return FindDistance( p1, pos );
}


STATUS_T CompoundDescriptionMove(
		track_p trk,
		wAction_t action,
		coOrd pos )
{
	struct extraDataCompound_t *xx = GET_EXTRA_DATA(trk, T_NOTRACK, extraDataCompound_t);
	static coOrd p0, p1;
	static BOOL_T editMode;
	wDrawColor color;

	switch (action) {
	case C_DOWN:
		editMode = TRUE;
		REORIGIN( p0, xx->descriptionOrig, xx->angle, xx->orig )
		DrawCompoundDescription( trk, &mainD, wDrawColorWhite );

	case C_MOVE:
	case C_UP:
		color = GetTrkColor( trk, &mainD );
		xx->descriptionOff.x = (pos.x-p0.x);
		xx->descriptionOff.y = (pos.y-p0.y);
		p1 = xx->descriptionOrig;
		Rotate( &p1, zero, xx->angle );
		p1.x += xx->orig.x + xx->descriptionOff.x;
		p1.y += xx->orig.y + xx->descriptionOff.y;
		if (action == C_UP) {
			editMode = FALSE;
		}
		if ( action == C_UP ) {
			DrawCompoundDescription( trk, &mainD, color );
		}
		return action==C_UP?C_TERMINATE:C_CONTINUE;
		break;
	case C_REDRAW:
		if (editMode) {
			DrawCompoundDescription( trk, &tempD, wDrawColorBlue );
			DrawLine( &tempD, p0, p1, 0, wDrawColorBlue );
		}
	}


	return C_CONTINUE;
}



/*****************************************************************************
 *
 * Generics
 *
 */

EXPORT void SetSegInxEP(
		signed char * segChar,
		int segInx,
		EPINX_T segEP )
{
	if (segEP == 1) {
		* segChar = -(segInx+1);
	} else {
		* segChar = (segInx+1);
	}

}

EXPORT void GetSegInxEP(
		signed char segChar,
		int * segInx,
		EPINX_T * segEP )
{
	int inx;
	inx = segChar;
	if (inx > 0 ) {
		*segInx = (inx)-1;
		*segEP = 0;
	} else {
		*segInx = (-inx)-1;
		*segEP = 1;
	}
}


DIST_T DistanceCompound(
		track_p t,
		coOrd * p )
{
	struct extraDataCompound_t *xx = GET_EXTRA_DATA(t, T_NOTRACK, extraDataCompound_t);
	EPINX_T ep;
	DIST_T d0, d1;
	coOrd p0, p2;
	PATHPTR_T path;
	int segInx;
	EPINX_T segEP;
	segProcData_t segProcData;

	if ( onTrackInSplit && GetTrkEndPtCnt(t) > 0 ) {
		d0 = DistanceSegs( xx->orig, xx->angle, xx->segCnt, xx->segs, p, NULL );
	} else if ( programMode != MODE_TRAIN || GetTrkEndPtCnt(t) <= 0 ) {
		d0 = DistanceSegs( xx->orig, xx->angle, xx->segCnt, xx->segs, p, NULL );
		if (programMode != MODE_TRAIN && GetTrkEndPtCnt(t) > 0 && d0 < DIST_INF) {
			ep = PickEndPoint( *p, t );
			*p = GetTrkEndPos(t,ep);
		}
	} else {
		p0 = *p;
		Rotate( &p0, xx->orig, -xx->angle );
		p0.x -= xx->orig.x;
		p0.y -= xx->orig.y;
		d0 = DIST_INF;
		path = GetCurrPath( t );
		for ( path += strlen((char *)path)+1; path[0] || path[1]; path++ ) {
			if ( path[0] != 0 ) {
				d1 = DIST_INF;
				GetSegInxEP( *path, &segInx, &segEP );
				segProcData.distance.pos1 = p0;
				SegProc( SEGPROC_DISTANCE, &xx->segs[segInx], &segProcData );
				if ( segProcData.distance.dd < d0 ) {
					d0 = segProcData.distance.dd;
					p2 = segProcData.distance.pos1;
				}
			}
		}
		if ( d0 < DIST_INF ) {
			p2.x += xx->orig.x;
			p2.y += xx->orig.y;
			Rotate( &p2, xx->orig, xx->angle );
			*p = p2;
		}
	}
	return d0;
}


static struct {
		coOrd endPt[4];
		ANGLE_T endAngle[4];
		DIST_T endRadius[4];
		coOrd endCenter[4];
		FLOAT_T elev[4];
		coOrd orig;
		ANGLE_T angle;
		descPivot_t pivot;
		char manuf[STR_SIZE];
		char name[STR_SIZE];
		char partno[STR_SIZE];
		long epCnt;
		long segCnt;
		long pathCnt;
		FLOAT_T grade;
		DIST_T length;
		drawLineType_e linetype;
		unsigned int layerNumber;
		} compoundData;
typedef enum { E0, A0, C0, R0, Z0, E1, A1, C1, R1, Z1, E2, A2, C2, R2, Z2, E3, A3, C3, R3, Z3, GR, OR, AN, PV, MN, NM, PN, LT, SC, LY } compoundDesc_e;
static descData_t compoundDesc[] = {
/*E0*/	{ DESC_POS, N_("End Pt 1: X,Y"), &compoundData.endPt[0] },
/*A0*/  { DESC_ANGLE, N_("Angle"), &compoundData.endAngle[0] },
/*C0*/  { DESC_POS, N_("Center X,Y"), &compoundData.endCenter[0] },
/*R0*/	{ DESC_DIM, N_("Radius"), &compoundData.endRadius[0] },
/*Z0*/	{ DESC_DIM, N_("Z1"), &compoundData.elev[0] },
/*E1*/	{ DESC_POS, N_("End Pt 2: X,Y"), &compoundData.endPt[1] },
/*A1*/  { DESC_ANGLE, N_("Angle"), &compoundData.endAngle[1] },
/*C1*/  { DESC_POS, N_("Center X,Y"), &compoundData.endCenter[1] },
/*R1*/	{ DESC_DIM, N_("Radius"), &compoundData.endRadius[1] },
/*Z1*/	{ DESC_DIM, N_("Z2"), &compoundData.elev[1] },
/*E2*/	{ DESC_POS, N_("End Pt 3: X,Y"), &compoundData.endPt[2] },
/*A2*/  { DESC_ANGLE, N_("Angle"), &compoundData.endAngle[2] },
/*C2*/  { DESC_POS, N_("Center X,Y"), &compoundData.endCenter[2] },
/*R2*/	{ DESC_DIM, N_("Radius"), &compoundData.endRadius[2] },
/*Z2*/	{ DESC_DIM, N_("Z3"), &compoundData.elev[2] },
/*E3*/	{ DESC_POS, N_("End Pt 4: X,Y"), &compoundData.endPt[3] },
/*A3*/  { DESC_ANGLE, N_("Angle"), &compoundData.endAngle[3] },
/*C3*/  { DESC_POS, N_("Center X,Y"), &compoundData.endCenter[3] },
/*R3*/	{ DESC_DIM, N_("Radius"), &compoundData.endRadius[3] },
/*Z3*/	{ DESC_DIM, N_("Z4"), &compoundData.elev[3] },
/*GR*/	{ DESC_FLOAT, N_("Grade"), &compoundData.grade },
/*OR*/	{ DESC_POS, N_("Origin: X,Y"), &compoundData.orig },
/*AN*/	{ DESC_ANGLE, N_("Angle"), &compoundData.angle },
/*PV*/	{ DESC_PIVOT, N_("Pivot"), &compoundData.pivot },
/*MN*/	{ DESC_STRING, N_("Manufacturer"), &compoundData.manuf, sizeof(compoundData.manuf)},
/*NM*/	{ DESC_STRING, N_("Name"), &compoundData.name, sizeof(compoundData.name) },
/*PN*/	{ DESC_STRING, N_("Part No"), &compoundData.partno, sizeof(compoundData.partno)},
/*LT*/  { DESC_LIST, N_("LineType"), &compoundData.linetype },
/*SC*/	{ DESC_LONG, N_("# Segments"), &compoundData.segCnt },
/*LY*/	{ DESC_LAYER, N_("Layer"), &compoundData.layerNumber },
		{ DESC_NULL } };
#define MAX_DESCRIBE_ENDS 4


static void UpdateCompound( track_p trk, int inx, descData_p descUpd, BOOL_T needUndoStart )
{
	struct extraDataCompound_t *xx = GET_EXTRA_DATA(trk, T_NOTRACK, extraDataCompound_t);
	const char * manufS, * nameS, * partnoS;
	char * mP, *nP, *pP;
	int mL, nL, pL;
	coOrd hi, lo;
	coOrd pos;
	EPINX_T ep;
	BOOL_T titleChanged, flipped, ungrouped, split;
	char * newTitle;

	switch ( inx ) {
	case -1:
	case MN:
	case NM:
	case PN:
		titleChanged = FALSE;
		ParseCompoundTitle( xtitle(xx), &mP, &mL, &nP, &nL, &pP, &pL );
		if (mP == NULL) mP = "";
		if (nP == NULL) nP = "";
		if (pP == NULL) pP = "";
		manufS = wStringGetValue( (wString_p)compoundDesc[MN].control0 );
		size_t max_manustr = 256, max_partstr = 256, max_namestr = 256;
		if (compoundDesc[MN].max_string)
			max_manustr = compoundDesc[MN].max_string-1;
		if (strlen(manufS)>max_manustr) {
			NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED, _("Ok"), NULL, max_manustr-1);
		}
		message[0] = '\0';
 		strncat( message, manufS, max_manustr-1 );
		if ( strncmp( manufS, mP, mL ) != 0 || mL != strlen(manufS) ) {
			titleChanged = TRUE;
		}
		flipped = xx->flipped;
		ungrouped = xx->ungrouped;
		split = xx->split;
		nameS = wStringGetValue( (wString_p)compoundDesc[NM].control0 );
		max_namestr = 256;
		if (compoundDesc[NM].max_string)
			max_namestr = compoundDesc[NM].max_string;
		if (strlen(nameS)>max_namestr) {
			NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED, _("Ok"), NULL, max_namestr-1);
		}
		if ( strncmp( nameS, "Flipped ", 8 ) == 0 ) {
			nameS += 8;
			flipped = TRUE;
		} else {
			flipped = FALSE;
		}
		if ( strncmp( nameS, "Ungrouped ", 10 ) == 0 ) {
			nameS += 10;
			ungrouped = TRUE;
		} else {
			ungrouped = FALSE;
		}
		if ( strncmp( nameS, "Split ", 6 ) == 0 ) {
			nameS += 6;
			split = TRUE;
		} else {
			split = FALSE;
		}
		if ( strncmp( nameS, nP, nL ) != 0 || nL != strlen(nameS) ||
			 xx->flipped != flipped ||
			 xx->ungrouped != ungrouped ||
			 xx->split != split ) {
			titleChanged = TRUE;
		}
		strcat( message, "\t" );
		strncat( message, nameS, max_namestr-1 );
		partnoS = wStringGetValue( (wString_p)compoundDesc[PN].control0 );
		max_partstr = 256;
		if (compoundDesc[PN].max_string)
			max_partstr = compoundDesc[PN].max_string;
		if (strlen(partnoS)>max_partstr) {
			NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED, _("Ok"), NULL, max_partstr-1);
		}
		strcat( message, "\t");
		strncat( message, partnoS, max_partstr-1 );
		newTitle = MyStrdup( message );
		if ( strncmp( partnoS, pP, pL ) != 0 || pL != strlen(partnoS) ) {
			titleChanged = TRUE;
		}
		if ( ! titleChanged ) {
			MyFree(newTitle);
			return;
		}
		if ( needUndoStart )
		   UndoStart( _("Change Track"), "Change Track" );
		UndoModify( trk );
		GetBoundingBox( trk, &hi, &lo );
		if ( labelScale >= mainD.scale &&
			 !OFF_MAIND( lo, hi ) ) {
			DrawCompoundDescription( trk, &mainD, wDrawColorWhite );
		}
		/*sprintf( message, "%s\t%s\t%s", manufS, nameS, partnoS );*/
		if (xx->title) MyFree(xx->title);
		xx->title = newTitle;
		xx->flipped = flipped;
		xx->ungrouped = ungrouped;
		xx->split = split;
		if ( labelScale >= mainD.scale &&
			 !OFF_MAIND( lo, hi ) ) {
			DrawCompoundDescription( trk, &mainD, GetTrkColor(trk,&tempD) );
		}
		return;
	}

	UndrawNewTrack( trk );
	coOrd orig;
	switch ( inx ) {
	case OR:
		pos.x = compoundData.orig.x - xx->orig.x;
		pos.y = compoundData.orig.y - xx->orig.y;
		MoveTrack( trk, pos );
		ComputeCompoundBoundingBox( trk );
		break;
	case A0:
	case A1:
	case A2:
	case A3:
		if (inx==A3) ep=3;
		else if (inx==A2) ep=2;
		else if (inx==A1) ep=1;
		else ep=0;
		RotateTrack( trk, GetTrkEndPos(trk,ep), NormalizeAngle( compoundData.endAngle[ep]-GetTrkEndAngle(trk,ep) ) );
		ComputeCompoundBoundingBox( trk );
		break;
	case AN:
		orig = xx->orig;
		GetBoundingBox(trk,&hi,&lo);
		switch (compoundData.pivot) {
			case DESC_PIVOT_MID:
				orig.x = (hi.x-lo.x)/2+lo.x;
				orig.y = (hi.y-lo.y)/2+lo.y;
				break;
			case DESC_PIVOT_SECOND:
				orig.x = (hi.x-lo.x)/2+lo.x;
				orig.y = (hi.y-lo.y)/2+lo.y;
				orig.x = (orig.x - xx->orig.x)*2+xx->orig.x;
				orig.y = (orig.y - xx->orig.y)*2+xx->orig.y;
				break;
			default:
				break;
		}
		RotateTrack( trk, orig, NormalizeAngle( compoundData.angle-xx->angle ) );
		ComputeCompoundBoundingBox( trk );
		break;
	case E0:
	case E1:
	case E2:
	case E3:
		if (inx==E3) ep=3;
		else if (inx==E2) ep=2;
		else if (inx==E1) ep=1;
		else ep=0;
		pos = GetTrkEndPos(trk,ep);
		pos.x = compoundData.endPt[ep].x - pos.x;
		pos.y = compoundData.endPt[ep].y - pos.y;
		MoveTrack( trk, pos );
		ComputeCompoundBoundingBox( trk );
		break;
	case Z0:
	case Z1:
	case Z2:
	case Z3:
		ep = (inx==Z0?0:(inx==Z1?1:(inx==Z2?2:3)));
		UpdateTrkEndElev( trk, ep, GetTrkEndElevUnmaskedMode(trk,ep), compoundData.elev[ep], NULL );
		if ( GetTrkEndPtCnt(trk) == 1 )
			 break;
		for (int i=0;i<compoundData.epCnt;i++) {
			if (i==ep) continue;
			ComputeElev( trk, i, FALSE, &compoundData.elev[i], NULL, TRUE );
		}
		if ( compoundData.length > minLength )
			compoundData.grade = fabs( (compoundData.elev[0]-compoundData.elev[1])/compoundData.length )*100.0;
		else
			compoundData.grade = 0.0;
		compoundDesc[GR].mode |= DESC_CHANGE;
		compoundDesc[Z0+(E1-E0)*inx].mode |= DESC_CHANGE;
		break;
	case LY:
		SetTrkLayer( trk, compoundData.layerNumber);
		break;
	default:
		break;
	}
    switch ( inx ) {
    case A0:
    case A1:
    case A2:
    case A3:
    case E0:
    case E1:
    case E2:
    case E3:
    case AN:
    case OR:
		for (int i=0;(i<compoundData.epCnt)&&(i<MAX_DESCRIBE_ENDS);i++) {
			compoundData.endPt[i] = GetTrkEndPos(trk,i);
			compoundDesc[i*(E1-E0)+E0].mode |= DESC_CHANGE;
			trackParams_t params;
			compoundData.endAngle[i] = GetTrkEndAngle(trk,i);
			compoundDesc[i*(E1-E0)+A0].mode |= DESC_CHANGE;
			GetTrackParams(PARAMS_CORNU,trk,compoundData.endPt[i],&params);
			compoundData.endRadius[i] = params.arcR;
			if (params.arcR != 0.0) {
				  compoundData.endCenter[i] = params.arcP;
				  compoundDesc[i*(E1-E0)+C0].mode |= DESC_CHANGE;
			}
		}
		compoundData.orig = xx->orig;
		compoundDesc[OR].mode |= DESC_CHANGE;
		compoundData.angle = xx->angle;
		compoundDesc[AN].mode |= DESC_CHANGE;
    	break;
    case LT:
    	xx->lineType = compoundData.linetype;
    	break;
    default:
    	break;
	};

	DrawNewTrack( trk );

}


void DescribeCompound(
		track_p trk,
		char * str,
		CSIZE_T len )
{
	struct extraDataCompound_t *xx = GET_EXTRA_DATA(trk, T_NOTRACK, extraDataCompound_t);
	int fix;
	EPINX_T ep, epCnt;
	char * mP, *nP, *pP, *cnP;
	int mL, nL, pL;
	long mode;
	long listLabelsOption = listLabels;
	DynString description;
	char *trackType; 

	if ( xx->flipped )
		listLabelsOption |= LABEL_FLIPPED;
	if ( xx->ungrouped )
		listLabelsOption |= LABEL_UNGROUPED;
	if ( xx->split )
		listLabelsOption |= LABEL_SPLIT;
	FormatCompoundTitle( listLabelsOption, xtitle(xx) );
	if (message[0] == '\0')
		FormatCompoundTitle( listLabelsOption|LABEL_DESCR, xtitle(xx) );

	if (GetTrkEndPtCnt(trk) <= 1) {
		trackType = _("Structure");
	} else {
		trackType = GetTrkEndPtCnt(trk) > 2 ? _("Turnout") : _("Sectional Track");
	}
	DynStringMalloc(&description, len);
	DynStringPrintf(&description,
		_("%s (%d) Layer= %d %s"),
		trackType,
		GetTrkIndex(trk),
		GetTrkLayer(trk) + 1,
		message);
	
	if (DynStringSize(&description) > (unsigned)len) {
		strncpy(str, DynStringToCStr(&description), len - 1);
		strcpy(str + len - 4, "...");
	} else {
		strcpy(str, DynStringToCStr(&description));
	}

	DynStringFree(&description);

	epCnt = GetTrkEndPtCnt(trk);
	fix = 0;
	mode = 0;
	for ( ep=0; ep<epCnt; ep++ ) {
		if (GetTrkEndTrk(trk,ep)) {
			fix = 1;
			mode = DESC_RO;
			break;
		}
	}
	compoundData.orig = xx->orig;
	compoundData.angle = xx->angle;
	ParseCompoundTitle( xtitle(xx), &mP, &mL, &nP, &nL, &pP, &pL );
	if (mP) {
		memcpy( compoundData.manuf, mP, mL );
		compoundData.manuf[mL] = 0;
	} else {
		compoundData.manuf[0] = 0;
	}
	if (nP) {
		cnP = compoundData.name;
		if ( xx->flipped ) {
			memcpy( cnP, "Flipped ", 8 );
			cnP += 8;
		}
		if ( xx->ungrouped ) {
			memcpy( cnP, "Ungrouped ", 10 );
			cnP += 10;
		}
		if ( xx->split ) {
			memcpy( cnP, "Split ", 6 );
			cnP += 6;
		}
		memcpy( cnP, nP, nL );
		cnP[nL] = 0;
	} else {
		compoundData.name[0] = 0;
	}
	if (pP) {
		memcpy( compoundData.partno, pP, pL );
		compoundData.partno[pL] = 0;
	} else {
		compoundData.partno[0] = 0;
	}
	compoundData.epCnt = GetTrkEndPtCnt(trk);
	compoundData.segCnt = xx->segCnt;
	compoundData.length = 0;
	compoundData.layerNumber = GetTrkLayer( trk );

	for ( int i=0 ; i<4 ; i++) {
		compoundDesc[E0+(E1-E0)*i].mode = DESC_IGNORE;
        compoundDesc[A0+(E1-E0)*i].mode = DESC_IGNORE;
		compoundDesc[R0+(E1-E0)*i].mode = DESC_IGNORE;
		compoundDesc[C0+(E1-E0)*i].mode = DESC_IGNORE;
		compoundDesc[Z0+(E1-E0)*i].mode = DESC_IGNORE;
	}

	compoundDesc[GR].mode = DESC_IGNORE;
	compoundDesc[OR].mode =
	compoundDesc[AN].mode = fix?DESC_RO:0;
	compoundDesc[MN].mode =
	compoundDesc[NM].mode =
	compoundDesc[PN].mode = 0 /*DESC_NOREDRAW*/;
	compoundDesc[SC].mode = DESC_RO;
	compoundDesc[LY].mode = DESC_NOREDRAW;
	compoundDesc[PV].mode = 0;
	compoundData.pivot = DESC_PIVOT_FIRST;
	if (compoundData.epCnt >0) {
		for (int i=0;(i<compoundData.epCnt)&&(i<MAX_DESCRIBE_ENDS);i++) {
			compoundDesc[A0+(E1-E0)*i].mode = (int)mode;
			compoundDesc[R0+(E1-E0)*i].mode = DESC_RO;
			compoundDesc[C0+(E1-E0)*i].mode = DESC_RO;
			compoundDesc[E0+(E1-E0)*i].mode = (int)mode;
			compoundData.endPt[i] = GetTrkEndPos(trk,i);
			compoundData.endAngle[i] = GetTrkEndAngle(trk,i);
			trackParams_t params;
			GetTrackParams(PARAMS_CORNU,trk,compoundData.endPt[i],&params);
			compoundData.endRadius[i] = params.arcR;
			if (params.arcR != 0.0) {
				compoundData.endCenter[i] = params.arcP;
			} else {
				compoundDesc[C0+(E1-E0)*i].mode = DESC_IGNORE;
				compoundDesc[R0+(E1-E0)*i].mode = DESC_IGNORE;
			}
			ComputeElev( trk, i, FALSE, &compoundData.elev[i], NULL, FALSE );
			compoundDesc[Z0+(E1-E0)*i].mode = (EndPtIsDefinedElev(trk,i)?0:DESC_RO)|DESC_NOREDRAW;
		}
		compoundDesc[GR].mode = DESC_RO;
	}
	if ( compoundData.epCnt == 2 )
		compoundData.length = GetTrkLength( trk, 0, 1 );
	if ( compoundData.length > minLength && compoundData.epCnt > 1)
		compoundData.grade = fabs( (compoundData.elev[0]-compoundData.elev[1])/compoundData.length )*100.0;
	else
		compoundData.grade = 0.0;

	if (GetTrkEndPtCnt(trk) == 0) {
		compoundDesc[LT].mode = 0;
	} else
		compoundDesc[LT].mode = DESC_IGNORE;

	DoDescribe(trackType, trk, compoundDesc, UpdateCompound);

	if (  compoundDesc[LT].control0!=NULL) {
		wListClear( (wList_p)compoundDesc[LT].control0 );
		wListAddValue( (wList_p)compoundDesc[LT].control0, _("Solid"), NULL, I2VP(0) );
		wListAddValue( (wList_p)compoundDesc[LT].control0, _("Dash"), NULL, I2VP(1) );
		wListAddValue( (wList_p)compoundDesc[LT].control0, _("Dot"), NULL, I2VP(2) );
		wListAddValue( (wList_p)compoundDesc[LT].control0, _("DashDot"), NULL, I2VP(3) );
		wListAddValue( (wList_p)compoundDesc[LT].control0, _("DashDotDot"), NULL, I2VP(4) );
		wListAddValue( (wList_p)compoundDesc[LT].control0, _("CenterDot"), NULL, I2VP(5) );
		wListAddValue( (wList_p)compoundDesc[LT].control0, _("PhantomDot"), NULL, I2VP(6) );
		wListSetIndex( (wList_p)compoundDesc[LT].control0, compoundData.linetype );
	}

}


void DeleteCompound(
		track_p t )
{
	struct extraDataCompound_t *xx = GET_EXTRA_DATA(t, T_NOTRACK, extraDataCompound_t);
	FreeFilledDraw( xx->segCnt, xx->segs );
	if (xx->segCnt>0) MyFree( xx->segs );
	xx->segs = NULL;
}


BOOL_T WriteCompound(
		track_p t,
		FILE * f )
{
	struct extraDataCompound_t *xx = GET_EXTRA_DATA(t, T_NOTRACK, extraDataCompound_t);
	EPINX_T ep, epCnt;
	int bits;
	long options;
	long position = 0;
	drawLineType_e lineType = 0;
	BOOL_T rc = TRUE;

	options = (long)GetTrkWidth(t);
	if (xx->handlaid)
		options |= COMPOUND_OPTION_HANDLAID;
	if (xx->flipped)
		options |= COMPOUND_OPTION_FLIPPED;
	if (xx->ungrouped)
		options |= COMPOUND_OPTION_UNGROUPED;
	if (xx->split)
		options |= COMPOUND_OPTION_SPLIT;
	if (xx->pathOverRide)
		options |= COMPOUND_OPTION_PATH_OVERRIDE;
	if (xx->pathNoCombine)
		options |= COMPOUND_OPTION_PATH_NOCOMBINE;
	if ( ( GetTrkBits( t ) & TB_HIDEDESC ) != 0 )
		options |= COMPOUND_OPTION_HIDEDESC;
	epCnt = GetTrkEndPtCnt(t);
	lineType = xx->lineType;
	bits = GetTrkVisible(t)|(GetTrkNoTies(t)?1<<2:0)|(GetTrkBridge(t)?1<<3:0)|(GetTrkRoadbed(t)?1<<4:0);
	rc &= fprintf(f, "%s %d %d %ld %ld %d %s %d %0.6f %0.6f 0 %0.6f \"%s\"\n",
				GetTrkTypeName(t),
				GetTrkIndex(t), GetTrkLayer(t), options,
				GetCurrPathIndex(t), lineType,
				GetTrkScaleName(t), bits,
				xx->orig.x, xx->orig.y, xx->angle,
				PutTitle(xtitle(xx)) )>0;
	for (ep=0; ep<epCnt; ep++ )
		WriteEndPt( f, t, ep );
	switch ( xx->special ) {
	case TOadjustable:
		rc &= fprintf( f, "\tX %s %0.3f %0.3f\n", ADJUSTABLE,
				xx->u.adjustable.minD, xx->u.adjustable.maxD )>0;
		break;
	case TOpier:
		rc &= fprintf( f, "\tX %s %0.6f \"%s\"\n", PIER, xx->u.pier.height, xx->u.pier.name )>0;
		break;

	default:
		;
	}
	rc &= fprintf( f, "\tD %0.6f %0.6f\n", xx->descriptionOff.x, xx->descriptionOff.y )>0;
	rc &= WriteCompoundPathsEndPtsSegs( f, GetPaths( t ), xx->segCnt, xx->segs, 0, NULL );
	return rc;
}




/*****************************************************************************
 *
 * Generic Functions
 *
 */

EXPORT void SetCompoundLineType( track_p trk, int width ) {
	struct extraDataCompound_t * xx = GET_EXTRA_DATA(trk, T_NOTRACK, extraDataCompound_t);
	switch(width) {
	case 0:
		xx->lineType = DRAWLINESOLID;
		break;
	case 1:
		xx->lineType = DRAWLINEDASH;
		break;
	case 2:
		xx->lineType = DRAWLINEDOT;
		break;
	case 3:
		xx->lineType = DRAWLINEDASHDOT;
		break;
	case 4:
		xx->lineType = DRAWLINEDASHDOTDOT;
		break;
	case 5:
		xx->lineType = DRAWLINECENTER;
		break;
	case 6:
		xx->lineType = DRAWLINEPHANTOM;
		break;
	}
}



EXPORT track_p NewCompound(
		TRKTYP_T trkType,
		TRKINX_T index,
		coOrd pos,
		ANGLE_T angle,
		char * title,
		EPINX_T epCnt,
		trkEndPt_t * epp,
		PATHPTR_T paths,
		wIndex_t segCnt,
		trkSeg_p segs )
{
	track_p trk;
	struct extraDataCompound_t * xx;
	EPINX_T ep;

	trk = NewTrack( index, trkType, epCnt, sizeof (*xx) + 1 );
	xx = GET_EXTRA_DATA(trk, trkType, extraDataCompound_t);
	xx->orig = pos;
	xx->angle = angle;
	xx->handlaid = FALSE;
	xx->flipped = FALSE;
	xx->ungrouped = FALSE;
	xx->split = FALSE;
	xx->descriptionOff = zero;
	xx->descriptionSize = zero;
	xx->title = MyStrdup( title );
	xx->customInfo = NULL;
	xx->special = TOnormal;
	SetPaths( trk, paths );
	xx->segCnt = segCnt;
	xx->segs = memdup( segs, segCnt * sizeof *segs );
	trkSeg_p p = xx->segs;
	CopyPoly(xx->segs, xx->segCnt);
	FixUpBezierSegs(xx->segs,xx->segCnt);
	ComputeCompoundBoundingBox( trk );
	SetDescriptionOrig( trk );
	for ( ep=0; ep<epCnt; ep++ ) {
		SetTrkEndPoint( trk, ep, epp[ep].pos, epp[ep].angle );
	}
	return trk;
}


BOOL_T ReadCompound(
		char * line,
		TRKTYP_T trkType )
{
	track_p trk;
	struct extraDataCompound_t *xx;
	TRKINX_T index;
	BOOL_T visible;
	coOrd orig;
	DIST_T elev;
	ANGLE_T angle;
	char scale[10];
	char *title;
	wIndex_t layer;
	char *cp;
	long options = 0;
	long position = 0;
	long lineType = 0;

	if (paramVersion<3) {
		if ( !GetArgs( line, "dXsdpfq",
			&index, &layer, scale, &visible, &orig, &angle, &title ) )
			return FALSE;
	} else if (paramVersion <= 5 && trkType == T_STRUCTURE) {
		if ( !GetArgs( line, "dL00sdpfq",
			&index, &layer, scale, &visible, &orig, &angle, &title ) )
			return FALSE;
	} else {
		if ( !GetArgs( line, paramVersion<9?"dLlldsdpYfq":"dLlldsdpffq",
			&index, &layer, &options, &position, &lineType, scale, &visible, &orig, &elev, &angle, &title ) )
			return FALSE;
	}
	if (paramVersion >=3 && paramVersion <= 5 && trkType == T_STRUCTURE)
		strcpy( scale, curScaleName );
	DYNARR_RESET( trkEndPt_t, tempEndPts_da );
	pathCnt = 0;
	if ( !ReadSegs() )
		return FALSE;
	if ( trkType == T_TURNOUT ) {
		if ( tempEndPts_da.cnt <= 0 ) {
			InputError( "Turnout defn without EndPoints", TRUE );
			return FALSE;
		}
		if ( pathCnt <= 1 ) {
			InputError( "Turnout defn without a Path", TRUE );
			return FALSE;
		}
	}
	if (paramVersion<6 && strlen( title ) > 2) {
		cp = strchr( title, '\t' );
		if (cp != NULL) {
			cp = strchr( cp, '\t' );
		}
		if (cp == NULL) {
			UpdateTitleMark( title, LookupScale(scale) );
		}
	}
	trk = NewCompound( trkType, index, orig, angle, title, 0, NULL,
			pathCnt > 1 ? pathPtr : NULL,
			tempSegs_da.cnt, &tempSegs(0) );
	SetEndPts( trk, 0 );
	if ( paramVersion < 3 ) {
		SetTrkVisible(trk, visible!=0);
		SetTrkNoTies(trk, FALSE);
		SetTrkBridge(trk, FALSE);
		SetTrkRoadbed(trk, FALSE);
	} else {
		SetTrkVisible(trk, visible&2);
		SetTrkNoTies(trk, visible&4);
		SetTrkBridge(trk, visible&8);
		SetTrkRoadbed(trk, visible&16);
	}
	SetTrkScale(trk, LookupScale( scale ));
	SetTrkLayer(trk, layer);
	SetTrkWidth(trk, (int)(options&3));
	xx = GET_EXTRA_DATA(trk, trkType, extraDataCompound_t);
	xx->handlaid = (int)((options&COMPOUND_OPTION_HANDLAID)!=0);
	xx->flipped = (int)((options&COMPOUND_OPTION_FLIPPED)!=0);
	xx->ungrouped = (int)((options&COMPOUND_OPTION_UNGROUPED)!=0);
	xx->split = (int)((options&COMPOUND_OPTION_SPLIT)!=0);
	xx->pathOverRide = (int)((options&COMPOUND_OPTION_PATH_OVERRIDE)!=0);
	xx->pathNoCombine = (int)((options&COMPOUND_OPTION_PATH_NOCOMBINE)!=0);
	xx->lineType = lineType;
	xx->descriptionOff = descriptionOff;
	if ( ( options & COMPOUND_OPTION_HIDEDESC ) != 0 )
		SetTrkBits( trk, TB_HIDEDESC );

	if (tempSpecial[0] != '\0') {
		if (strncmp( tempSpecial, ADJUSTABLE, strlen(ADJUSTABLE) ) == 0) {
			xx->special = TOadjustable;
			if ( !GetArgs( tempSpecial+strlen(ADJUSTABLE), "ff",
						&xx->u.adjustable.minD, &xx->u.adjustable.maxD ) )
				return FALSE;

		} else if (strncmp( tempSpecial, PIER, strlen(PIER) ) == 0) {
			xx->special = TOpier;
			if ( !GetArgs( tempSpecial+strlen(PIER), "fq",
						&xx->u.pier.height, &xx->u.pier.name ) )
				return FALSE;

		} else {
			InputError("Unknown special case", TRUE);
			return FALSE;
		}
	}
	SetCurrPathIndex( trk, position );
	return TRUE;
}

void MoveCompound(
		track_p trk,
		coOrd orig )
{
	struct extraDataCompound_t *xx = GET_EXTRA_DATA(trk, T_NOTRACK, extraDataCompound_t);
	xx->orig.x += orig.x;
	xx->orig.y += orig.y;
	ComputeCompoundBoundingBox( trk );
}


void RotateCompound(
		track_p trk,
		coOrd orig,
		ANGLE_T angle )
{
	struct extraDataCompound_t *xx = GET_EXTRA_DATA(trk, T_NOTRACK, extraDataCompound_t);
	Rotate( &xx->orig, orig, angle );
	xx->angle = NormalizeAngle( xx->angle + angle );
	Rotate( &xx->descriptionOff, zero, angle );
	ComputeCompoundBoundingBox( trk );
}


void RescaleCompound(
		track_p trk,
		FLOAT_T ratio )
{
	struct extraDataCompound_t *xx = GET_EXTRA_DATA(trk, T_NOTRACK, extraDataCompound_t);
	xx->orig.x *= ratio;
	xx->orig.y *= ratio;
	xx->descriptionOff.x *= ratio;
	xx->descriptionOff.y *= ratio;
	xx->segs = (trkSeg_p)memdup( xx->segs, xx->segCnt * sizeof xx->segs[0] );
	CloneFilledDraw( xx->segCnt, xx->segs, TRUE );
	RescaleSegs( xx->segCnt, xx->segs, ratio, ratio, ratio );
}


void FlipCompound(
		track_p trk,
		coOrd orig,
		ANGLE_T angle )
{
	struct extraDataCompound_t *xx = GET_EXTRA_DATA(trk, T_NOTRACK, extraDataCompound_t);
	EPINX_T ep, epCnt;
	char * mP, *nP, *pP;
	int mL, nL, pL;
	char *type, *mfg, *descL, *partL, *descR, *partR, *cp;
	wIndex_t inx;
	turnoutInfo_t *to, *toBest;
	coOrd endPos[4];
	ANGLE_T endAngle[4];
	DIST_T d2, d1, d0;
	ANGLE_T a2, a1;
#define SMALLVALUE (0.001)

	FlipPoint( &xx->orig, orig, angle );
	xx->angle = NormalizeAngle( 2*angle - xx->angle + 180.0 );
	xx->segs = memdup( xx->segs, xx->segCnt * sizeof xx->segs[0] );
	FlipSegs( xx->segCnt, xx->segs, zero, angle );
	xx->descriptionOrig.y = - xx->descriptionOrig.y;
	ComputeCompoundBoundingBox( trk );
	epCnt = GetTrkEndPtCnt( trk );
	if ( epCnt >= 1 && epCnt <= 2 )
		return;
	ParseCompoundTitle( xtitle(xx), &mP, &mL, &nP, &nL, &pP, &pL );
	to = FindCompound( epCnt==0?FIND_STRUCT:FIND_TURNOUT, GetScaleName(GetTrkScale(trk)), xx->title );
	if ( epCnt!=0 && to && to->customInfo ) {
		if ( GetArgs( to->customInfo, "qc", &type, &cp ) ) {
			if ( strcmp( type, "Regular Turnout" ) == 0 ||
				 strcmp( type, "Curved Turnout" ) == 0 ) {
				if ( GetArgs( cp, "qqqqq", &mfg, &descL, &partL, &descR, &partR ) &&
					 mP && strcmp( mP, mfg ) == 0 && nP && pP ) {
					if ( strcmp( nP, descL ) == 0 && strcmp( pP, partL ) == 0 ) {
						sprintf( message, "%s\t%s\t%s", mfg, descR, partR );
						xx->title = MyStrdup( message );
						return;
					}
					if ( strcmp( nP, descR ) == 0 && strcmp( pP, partR ) == 0 ) {
						sprintf( message, "%s\t%s\t%s", mfg, descL, partL );
						xx->title = MyStrdup( message );
						return;
					}
				}
		   }
		}
	}
	if ( epCnt == 3 || epCnt == 4 ) {
		for ( ep=0; ep<epCnt; ep++ ) {
			endPos[ep] = GetTrkEndPos( trk, ep );
			endAngle[ep] = NormalizeAngle( GetTrkEndAngle( trk, ep ) - xx->angle );
			Rotate( &endPos[ep], xx->orig, -xx->angle );
			endPos[ep].x -= xx->orig.x;
			endPos[ep].y -= xx->orig.y;
		}
		if ( epCnt == 3 ) {
			/* Wye? */
			if ( fabs(endPos[1].x-endPos[2].x) < SMALLVALUE &&
				 fabs(endPos[1].y+endPos[2].y) < SMALLVALUE )
				return;
		} else {
			/* Crossing */
			if ( fabs( (endPos[1].x-endPos[3].x) - (endPos[2].x-endPos[0].x ) ) < SMALLVALUE &&
				 fabs( (endPos[2].y+endPos[3].y) ) < SMALLVALUE &&
				 fabs( (endPos[0].y-endPos[1].y) ) < SMALLVALUE &&
				 NormalizeAngle( (endAngle[2]-endAngle[3]-180+0.05) ) < 0.10 )
				return;
			/* 3 way */
			if ( fabs( (endPos[1].x-endPos[2].x) ) < SMALLVALUE &&
				 fabs( (endPos[1].y+endPos[2].y) ) < SMALLVALUE &&
				 fabs( (endPos[0].y-endPos[3].y) ) < SMALLVALUE &&
				 NormalizeAngle( (endAngle[1]+endAngle[2]-180+0.05) ) < 0.10 )
				return;
		}
		toBest = NULL;
		d0 = 0.0;
		for (inx=0; inx<turnoutInfo_da.cnt; inx++) {
			to = turnoutInfo(inx);
			if ( IsParamValid(to->paramFileIndex) &&
				 to->segCnt > 0 &&
				 to->scaleInx == GetTrkScale(trk) &&
				 to->segCnt != 0 &&
				 to->endCnt == epCnt ) {
				d1 = 0;
				a1 = 0;
				for ( ep=0; ep<epCnt; ep++ ) {
					d2 = FindDistance( endPos[ep], to->endPt[ep].pos );
					if ( d2 > SMALLVALUE )
						break;
					if ( d2 > d1 )
						d1 = d2;
					a2 = NormalizeAngle( endAngle[ep] - to->endPt[ep].angle + 0.05 );
					if ( a2 > 0.1 )
						break;
					if ( a2 > a1 )
						a1 = a2;
				}
				if ( ep<epCnt )
					continue;
				if ( toBest == NULL || d1 < d0 )
					toBest = to;
			}
		}
		if ( toBest ) {
			if ( strcmp( xx->title, toBest->title ) != 0 )
				xx->title = MyStrdup( toBest->title );
			return;
		}
	}
	xx->flipped = !xx->flipped;
}


typedef struct {
		long count;
		char * type;
		char * name;
		FLOAT_T price;
		DynString indexes;
		} enumCompound_t;
static dynArr_t enumCompound_da;
#define EnumCompound(N) DYNARR_N( enumCompound_t,enumCompound_da,N)

BOOL_T EnumerateCompound( track_p trk )
{
	struct extraDataCompound_t *xx;
	INT_T inx, inx2;
	int cmp;
	long listLabelsOption = listLabels;
	char * index = MyMalloc(10);

	if ( trk != NULL ) {
		xx = GET_EXTRA_DATA(trk, T_NOTRACK, extraDataCompound_t);
		if ( xx->flipped )
			listLabelsOption |= LABEL_FLIPPED;
#ifdef LATER
		if ( xx->ungrouped )
			listLabelsOption |= LABEL_UNGROUPED;
		if ( xx->split )
			listLabelsOption |= LABEL_SPLIT;
#endif
		FormatCompoundTitle( listLabelsOption, xtitle(xx) );
		if (message[0] == '\0')
			return FALSE;  //No content
		for (inx = 0; inx < enumCompound_da.cnt; inx++ ) {
			cmp =  strcmp( EnumCompound(inx).name, message );
			if ( cmp == 0 ) {
				EnumCompound(inx).count++;
				sprintf(index,",%d",GetTrkIndex(trk));
				DynStringCatCStr(&(EnumCompound(inx).indexes),index);
				MyFree(index);
				return TRUE;
			} else if ( cmp > 0 ) {
				break;
			}
		}
		DYNARR_APPEND( enumCompound_t, enumCompound_da, 10 );
		for ( inx2 = enumCompound_da.cnt-1; inx2 > inx; inx2-- )
			EnumCompound(inx2) = EnumCompound(inx2-1);
		EnumCompound(inx).name = MyStrdup( message );
		if (strlen(message) > (size_t)enumerateMaxDescLen)
			enumerateMaxDescLen = (int)strlen(message);
		EnumCompound(inx).type = GetTrkTypeName( trk );
		EnumCompound(inx).count = 1;
		DynStringMalloc(&(EnumCompound(inx).indexes),100);
		DynStringPrintf(&(EnumCompound(inx).indexes),"%d",GetTrkIndex(trk));
		FormatCompoundTitle( LABEL_MANUF|LABEL_DESCR|LABEL_PARTNO, xtitle(xx) );
		wPrefGetFloat( "price list", message, &(EnumCompound(inx).price), 0.0 );
	} else {
		char * type;
		for ( type="TS"; *type; type++ ) {
			for (inx = 0; inx < enumCompound_da.cnt; inx++ ) {
				if (EnumCompound(inx).type[0] == *type) {
					EnumerateList( EnumCompound(inx).count,
						EnumCompound(inx).price,
						EnumCompound(inx).name,
						DynStringSize(&(EnumCompound(inx).indexes))?DynStringToCStr(&(EnumCompound(inx).indexes)):NULL);
				}
				DynStringFree(&(EnumCompound(inx).indexes));
			}
		}
		DYNARR_RESET( enumCompound_t, enumCompound_da );
	}
	MyFree(index);
	return TRUE;
}

