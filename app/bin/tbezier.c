/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/tbezier.c,v 1.0 2015-07-01 tynewydd Exp $
 *
 * BEZIER TRACK
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

#include "track.h"
#include "ccurve.h"
#include "cstraigh.h"
#include "cjoin.h"
#include "i18n.h"

static TRKTYP_T T_BEZIER = -1;

struct extraData {
		coOrd pos[4];
		DIST_T min_radius;
        DIST_T length;
		coOrd descriptionOff;
		};

static int log_bezier = 0;

static DIST_T GetLengthBezier( track_p );

/****************************************
 *
 * UTILITIES
 *
 */


static void GetBezierAngles( ANGLE_T *a0, ANGLE_T *a1, track_p trk )
{
    struct extraData *xx = GetTrkExtraData(trk);
    assert( trk != NULL );
    
        *a0 = NormalizeAngle( GetTrkEndAngle(trk,0) + 90 );
        *a1 = NormalizeAngle(
                             GetTrkEndAngle(trk,1) - GetTrkEndAngle(trk,0) + 180 );
    
    LOG( log_bezier, 4, ( "getBezierAngles: = %0.3f %0.3f\n", *a0, *a1 ) )
}


static void ComputeBezierBoundingBox( track_p trk, struct extraData * xx )
{
    coOrd hi, lo;
    if (GetTrkType(trk) != T_BEZIER) return;
    hi.x = lo.x = xx->pos[0].x;
    hi.y = lo.y = xx->pos[0].y;
    
    for (int i =1; i<=3;i++) {
        hi.x = hi.x < xx->pos[i].x ? xx->pos[i].x : hi.x;
        hi.y = hi.y < xx->pos[i].y ? xx->pos[i].y : hi.y;
        lo.x = hi.x > xx->pos[i].x ? xx->pos[i].x : lo.x;
        lo.y = hi.y > xx->pos[i].y ? xx->pos[i].y : lo.y;
    }
    SetBoundingBox( trk, hi, lo );
}


DIST_T BezierDescriptionDistance(
		coOrd pos,
		track_p trk )
{
	struct extraData *xx = GetTrkExtraData(trk);
	coOrd p1;

	if ( GetTrkType( trk ) != T_BEZIER || ( GetTrkBits( trk ) & TB_HIDEDESC ) != 0 )
		return 100000;
	
		p1.x = xx->pos[0].x + (xx->pos[3].x-xx->pos[0].x)/2 + xx->descriptionOff.x;
		p1.y = xx->pos[0].y + (xx->pos[3].y-xx->pos[0].y)/2 + xx->descriptionOff.y;
	
	return FindDistance( p1, pos );
}


static void DrawBezierDescription(
		track_p trk,
		drawCmd_p d,
		wDrawColor color )
{
	struct extraData *xx = GetTrkExtraData(trk);
	wFont_p fp;
    coOrd pos;

	if (layoutLabels == 0)
		return;
	if ((labelEnable&LABELENABLE_TRKDESC)==0)
		return;
    pos.x = xx->pos[0].x + (xx->pos[3].x - xx->pos[0].x)/2;
    pos.y = xx->pos[0].y + (xx->pos[3].y - xx->pos[0].y)/2;
    pos.x += xx->descriptionOff.x;
    pos.y += xx->descriptionOff.y;
    fp = wStandardFont( F_TIMES, FALSE, FALSE );
    sprintf( message, _("Bezier Curve: length=%s min radius=%s"),
				FormatDistance(xx->length), FormatDistance(xx->min_radius));
    DrawBoxedString( BOX_BOX, d, pos, message, fp, (wFontSize_t)descriptionFontSize, color, 0.0 );
}


STATUS_T BezierDescriptionMove(
		track_p trk,
		wAction_t action,
		coOrd pos )
{
	struct extraData *xx = GetTrkExtraData(trk);
	static coOrd p0;
	wDrawColor color;
	ANGLE_T a, a0, a1;
	DIST_T d;

	switch (action) {
	case C_DOWN:
	case C_MOVE:
	case C_UP:
		color = GetTrkColor( trk, &mainD );
		DrawBezierDescription( trk, &tempD, color );
    
        if (action != C_DOWN)
            DrawLine( &tempD, xx->pos[0], p0, 0, wDrawColorBlack );
        xx->descriptionOff.x = p0.x - xx->pos[0].x + (xx->pos[3].x-xx->pos[0].x)/2;
        xx->descriptionOff.y = p0.y - xx->pos[0].x + (xx->pos[3].y-xx->pos[0].y)/2;
        p0 = pos;
        if (action != C_UP)
            DrawLine( &tempD, xx->pos[0], p0, 0, wDrawColorBlack );
        DrawBezierDescription( trk, &tempD, color );
        MainRedraw();
		return action==C_UP?C_TERMINATE:C_CONTINUE;

	case C_REDRAW:
		break;
		
	}
	return C_CONTINUE;
}

/****************************************
 *
 * GENERIC FUNCTIONS
 *
 */

static struct {
		coOrd pos[4];
		FLOAT_T elev[2];
		FLOAT_T length;
		DIST_T radius;
		FLOAT_T grade;
		LAYER_T layerNumber;
		} crvData;
typedef enum { P0, Z0, C1, C2, P1, Z1, RA, LN, GR, LY, A1, A2 } crvDesc_e;
static descData_t crvDesc[] = {
/*P0*/	{ DESC_POS, N_("End Pt 1: X"), &crvData.pos[0] },
/*Z0*/	{ DESC_DIM, N_("Z"), &crvData.elev[0] },
/*C1*/	{ DESC_POS, N_("Ctl Pt 1: X"), &crvData.pos[1] },
/*C1*/	{ DESC_POS, N_("Ctl Pt 2: X"), &crvData.pos[2] },
/*P1*/	{ DESC_POS, N_("End Pt 2: X"), &crvData.pos[3] },
/*Z3*/	{ DESC_DIM, N_("Z"), &crvData.elev[1] },
/*RA*/	{ DESC_DIM, N_("MinRadius"), &crvData.radius },
/*LN*/	{ DESC_DIM, N_("Length"), &crvData.length },
/*GR*/	{ DESC_FLOAT, N_("Grade"), &crvData.grade },
/*LY*/	{ DESC_LAYER, N_("Layer"), &crvData.layerNumber },
		{ DESC_NULL } };

static void UpdateBezier( track_p trk, int inx, descData_p descUpd, BOOL_T final )
{
	struct extraData *xx = GetTrkExtraData(trk);
	BOOL_T updateEndPts;
	ANGLE_T a0, a1;
	EPINX_T ep;
	struct extraData xx0;
	FLOAT_T turns;

	if ( inx == -1 )
		return;
	xx0 = *xx;
	updateEndPts = FALSE;
	switch ( inx ) {
    case P0:
        updateEndPts = TRUE;
        xx->pos[0] = crvData.pos[0];
        crvDesc[P0].mode |= DESC_CHANGE;
    case P1:
        updateEndPts = TRUE;
        xx->pos[3]= crvData.pos[3];
        crvDesc[P1].mode |= DESC_CHANGE;
        break;
    case C1:
        xx->pos[1] = crvData.pos[1];
        crvDesc[C1].mode |= DESC_CHANGE;
        break;
    case C2:
        xx->pos[2] = crvData.pos[2];
        crvDesc[C2].mode |= DESC_CHANGE;
        break;
    case Z0:
	case Z1:
		ep = (inx==Z0?0:1);
		UpdateTrkEndElev( trk, ep, GetTrkEndElevUnmaskedMode(trk,ep), crvData.elev[ep], NULL );
		ComputeElev( trk, 1-ep, FALSE, &crvData.elev[1-ep], NULL );
		if ( crvData.length > minLength )
			crvData.grade = fabs( (crvData.elev[0]-crvData.elev[1])/crvData.length )*100.0;
		else
			crvData.grade = 0.0;
		crvDesc[GR].mode |= DESC_CHANGE;
		crvDesc[inx==Z0?Z1:Z0].mode |= DESC_CHANGE;
        return;
	case LY:
		SetTrkLayer( trk, crvData.layerNumber);
		break;
	default:
		AbortProg( "updateBezier: Bad inx %d", inx );
	}
	UndrawNewTrack( trk );
	*xx = xx0;
    crvData.length = BezierLength( xx->pos[0], xx->pos[1], xx->pos[2], xx->pos[3], 0.01 );
    crvData.radius = BezierMinRadius( xx->pos[0], xx->pos[1], xx->pos[2], xx->pos[3] );
	ComputeBezierBoundingBox( trk, xx );
	DrawNewTrack( trk );
}

static void DescribeBezier( track_p trk, char * str, CSIZE_T len )
{
	struct extraData *xx = GetTrkExtraData(trk);
	ANGLE_T a0, a1;
	DIST_T d;
	int fix0, fix1;
	FLOAT_T turns;

	
	d = xx->length;
    sprintf( str, _("Bezier Track(%d): Layer=%d MinRadius=%s Length=%s EP=[%0.3f,%0.3f] [%0.3f,%0.3f] CP1=[%0.3f,%0.3f] CP2=[%0.3f, %0.3f]"),
				GetTrkIndex(trk),
				GetTrkLayer(trk)+1,
				FormatDistance(xx->min_radius),
				FormatDistance(d),
				PutDim(xx->pos[0].x),PutDim(xx->pos[0].y),
				PutDim(xx->pos[3].x),PutDim(xx->pos[3].y),
                PutDim(xx->pos[1].x),PutDim(xx->pos[1].y),
                PutDim(xx->pos[2].x),PutDim(xx->pos[2].y));

	fix0 = GetTrkEndTrk(trk,0)!=NULL;
	fix1 = GetTrkEndTrk(trk,1)!=NULL;

	crvData.length = GetLengthBezier(trk);
	crvData.radius = xx->min_radius;
    crvData.layerNumber = GetTrkLayer(trk);
		ComputeElev( trk, 0, FALSE, &crvData.elev[0], NULL );
		ComputeElev( trk, 1, FALSE, &crvData.elev[1], NULL );

	ComputeElev( trk, 0, FALSE, &crvData.elev[0], NULL );
	ComputeElev( trk, 1, FALSE, &crvData.elev[1], NULL );
	if ( crvData.length > minLength )
		crvData.grade = fabs( (crvData.elev[0]-crvData.elev[1])/crvData.length )*100.0;
	else
		crvData.grade = 0.0;
	
	crvDesc[P0].mode =
	crvDesc[P1].mode =
	crvDesc[LN].mode =
		DESC_RO;
    crvDesc[C1].mode = fix0?DESC_RO:0;
    crvDesc[C2].mode = fix1?DESC_RO:0;
	crvDesc[Z0].mode = (EndPtIsDefinedElev(trk,0)?0:DESC_RO)|DESC_NOREDRAW;
	crvDesc[Z1].mode = (EndPtIsDefinedElev(trk,1)?0:DESC_RO)|DESC_NOREDRAW;
	crvDesc[GR].mode = DESC_RO;
    crvDesc[RA].mode = DESC_RO;
	crvDesc[LY].mode = DESC_NOREDRAW;
			
    DoDescribe( _("Bezier Track"), trk, crvDesc, UpdateBezier );
	
}

static DIST_T DistanceBezier( track_p t, coOrd * p )
{
	struct extraData *xx = GetTrkExtraData(t);
	ANGLE_T a0, a1;
	DIST_T d;
    d = BezierDistance( p, xx->pos[0], xx->pos[1], xx->pos[2], xx->pos[1], 100, NULL );
	return d;
}

static void DrawBezier( track_p t, drawCmd_p d, wDrawColor color )
{
	struct extraData *xx = GetTrkExtraData(t);
		track_p tt = t;
	long widthOptions = DTS_LEFT|DTS_RIGHT|DTS_TIES;

	if (GetTrkWidth(t) == 2)
		widthOptions |= DTS_THICK2;
	if (GetTrkWidth(t) == 3)
		widthOptions |= DTS_THICK3;
	
	if ( ((d->funcs->options&wDrawOptTemp)==0) &&
		 (labelWhen == 2 || (labelWhen == 1 && (d->options&DC_PRINT))) &&
		 labelScale >= d->scale &&
		 ( GetTrkBits( t ) & TB_HIDEDESC ) == 0 ) {
		DrawBezierDescription( t, d, color );
	}
	DrawBezierTrack( d, xx->pos[0], xx->pos[1], xx->pos[2], xx->pos[3], GetTrkGauge(t), color, widthOptions );
	if ( (d->funcs->options & wDrawOptTemp) == 0 &&
		 (d->options&DC_QUICK) == 0 ) {
		DrawEndPt( d, t, 0, color );
		DrawEndPt( d, t, 1, color );
	}
}

static void DeleteBezier( track_p t )
{
}

static BOOL_T WriteBezier( track_p t, FILE * f )
{
	struct extraData *xx = GetTrkExtraData(t);
	long options;
	BOOL_T rc = TRUE;
	options = GetTrkWidth(t) & 0x0F;
	rc &= fprintf(f, "BEZIER %d %d %ld 0 0 %s %d %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f 0 %0.6f %0.6f\n",
		GetTrkIndex(t), GetTrkLayer(t), (long)options,
                  GetTrkScaleName(t), GetTrkVisible(t), xx->pos[0].x, xx->pos[0].y, xx->pos[1].x, xx->pos[1].y, xx->pos[2].x, xx->pos[2].y, xx->pos[3].x, xx->pos[3].y, xx->descriptionOff.x, xx->descriptionOff.y )>0;
	rc &= WriteEndPt( f, t, 0 );
	rc &= WriteEndPt( f, t, 1 );
	rc &= fprintf(f, "\tEND\n" )>0;
	return rc;
}

static void ReadBezier( char * line )
{
	struct extraData *xx;
	track_p t;
	wIndex_t index;
	BOOL_T visible;
	DIST_T r;
	coOrd p0, c1, c2, p1, dp;
	DIST_T elev;
	char scale[10];
	wIndex_t layer;
	long options;
	char * cp = NULL;

	if (!GetArgs( line+6, "dLl00sdpppp0pc",
		&index, &layer, &options, scale, &visible, &p0, &c1, &c2, &p1, &elev, &dp ) ) {
		return;
	}
	t = NewTrack( index, T_BEZIER, 0, sizeof *xx );
	xx = GetTrkExtraData(t);
	SetTrkVisible(t, visible);
	SetTrkScale(t, LookupScale(scale));
	SetTrkLayer(t, layer );
	SetTrkWidth(t, (int)(options&3));
	xx->pos[0] = p0;
    xx->pos[1] = c1;
    xx->pos[2] = c2;
    xx->pos[3] = p1;
    xx->descriptionOff = dp;
	ReadSegs();
	SetEndPts(t,2);
    ComputeBezierBoundingBox( t, xx );
    xx->min_radius = BezierMinRadius(p0,c1,c2,p1);
    xx->length = BezierLength(p0,c1,c2,p1, 0.01);
}

static void MoveBezier( track_p trk, coOrd orig )
{
	struct extraData *xx = GetTrkExtraData(trk);
    for (int i=0;i<4;i++) {
        xx->pos[i].x += orig.x;
        xx->pos[i].y += orig.y;
    }
	ComputeBezierBoundingBox( trk, xx );
}

static void RotateBezier( track_p trk, coOrd orig, ANGLE_T angle )
{
	struct extraData *xx = GetTrkExtraData(trk);
    for (int i=0;i<5;i++) {
        Rotate( &xx->pos[i], orig, angle );
    }
	ComputeBezierBoundingBox( trk, xx );
}

static void RescaleBezier( track_p trk, FLOAT_T ratio )
{
	struct extraData *xx = GetTrkExtraData(trk);
	xx->pos[0].x *= ratio;
	xx->pos[0].y *= ratio;
    xx->pos[1].x *= ratio;
    xx->pos[1].y *= ratio;
    xx->pos[2].x *= ratio;
    xx->pos[2].y *= ratio;
    xx->pos[3].x *= ratio;
    xx->pos[3].y *= ratio;
	xx->min_radius = BezierMinRadius(xx->pos[0],xx->pos[1],xx->pos[2],xx->pos[3]);
    xx->length = BezierLength(xx->pos[0],xx->pos[1],xx->pos[2],xx->pos[3], 0.01);
    ComputeBezierBoundingBox(trk, xx);
}

/**
 * Split the Track at approximately the point pos.
 */
static BOOL_T SplitBezier( track_p trk, coOrd pos, EPINX_T ep, track_p *leftover, EPINX_T * ep0, EPINX_T * ep1 )
{
	struct extraData *xx = GetTrkExtraData(trk);
	track_p trk1;
    double t;
    
    coOrd old[4], newl[4], newr[4];
    
    double dd = BezierDistance(&pos, xx->pos[0], xx->pos[1], xx->pos[2], xx->pos[3], 100, &t );
    
    if (dd>minLength) return FALSE;
    
    BezierSplit(&old[0], &newl[0], &newr[0], t);
    
	trk1 = NewBezierTrack( newr[0], newr[1], newr[2], newr[3] );
    
    for (int i=0;i<4;i++) {
        xx->pos[i] = newl[i];
    }
    
    xx->length = BezierLength(xx->pos[0], xx->pos[1], xx->pos[2], xx->pos[3], 0.01);
    
    ComputeBezierBoundingBox( trk, xx);
	
	*leftover = trk1;
	*ep0 = 1-ep;
	*ep1 = ep;

	return TRUE;
}

static BOOL_T TraverseBezier( traverseTrack_p trvTrk, DIST_T * distR )
{
    track_p trk = trvTrk->trk;
	struct extraData *xx = GetTrkExtraData(trk);
	ANGLE_T a, a0, a1;
	DIST_T arcDist;
	DIST_T dist;
    double t;
    GetBezierAngles( &a0, &a1, trk );
    
	a = NormalizeAngle( (a1-90.0) - trvTrk->angle );
	if ( a>270 || a<90 ) {
		// CCW
		if ( xx->length < *distR ) {
			trvTrk->pos = xx->pos[3];
			*distR -= xx->length;
			trvTrk->angle = NormalizeAngle( a0-90.0 );
			trk = GetTrkEndTrk( trk, 0 );
		} else {
			trvTrk->dist += *distR;
            t = 1-(double)*distR/xx->length;
			trvTrk->pos = BezierPointByParameter(xx->pos[3],xx->pos[2],xx->pos[1],xx->pos[0], t);
			*distR = 0;
            coOrd derivative = BezierFirstDerivative(xx->pos[3],xx->pos[2],xx->pos[1],xx->pos[0], t );
			trvTrk->angle = atan2(  derivative.x, derivative.y );
		}
	} else {
		// CW //
		if ( xx->length < *distR ) {
			trvTrk->pos = xx->pos[0];
			*distR -= xx->length;
			trvTrk->angle = NormalizeAngle( a0+a1+90.0 );
			trk = GetTrkEndTrk( trk, 1 );
		} else {
			trvTrk->dist += *distR;
            t = (double)*distR/xx->length;
			trvTrk->pos = BezierPointByParameter(xx->pos[0],xx->pos[1],xx->pos[2],xx->pos[3], t );
			*distR = 0;
            coOrd derivative = BezierFirstDerivative(xx->pos[0],xx->pos[1],xx->pos[2],xx->pos[3], t );
			trvTrk->angle = atan2( derivative.x, derivative.y );
		}
	}
	trvTrk->trk = trk;
    
	return TRUE;

}


static BOOL_T EnumerateBezier( track_p trk )
{
	struct extraData *xx;
	ANGLE_T a0, a1;
	DIST_T d;
	if (trk != NULL) {
		xx = GetTrkExtraData(trk);
		d = xx->min_radius;
		ScaleLengthIncrement( GetTrkScale(trk), d );
	}
	return TRUE;
}

static BOOL_T MergeBezier(
		track_p trk0,
		EPINX_T ep0,
		track_p trk1,
		EPINX_T ep1 )
{
	struct extraData *xx0 = GetTrkExtraData(trk0);
	struct extraData *xx1 = GetTrkExtraData(trk1);
	DIST_T d;
	track_p trk2;
	EPINX_T ep2=-1;
	coOrd pos;

	if (ep0 == ep1)
		return FALSE;
    
	UndoStart( _("Merge Bezier"), "MergeBezier( T%d[%d] T%d[%d] )", GetTrkIndex(trk0), ep0, GetTrkIndex(trk1), ep1 );
	UndoModify( trk0 );
	UndrawNewTrack( trk0 );
	trk2 = GetTrkEndTrk( trk1, 1-ep1 );
	if (trk2) {
		ep2 = GetEndPtConnectedToMe( trk2, trk1 );
		DisconnectTracks( trk1, 1-ep1, trk2, ep2 );
	}
	if (ep0 == 0) {
        xx0->pos[3] = xx1->pos[3];
        xx0->pos[2] = xx1->pos[2];
	} else {
        xx0->pos[0] = xx1->pos[0];
        xx0->pos[1] = xx1->pos[1];
	}
	DeleteTrack( trk1, FALSE );
	if (trk2) {
		ConnectTracks( trk0, ep0, trk2, ep2 );
	}
	DrawNewTrack( trk0 );
    xx0->length = BezierLength(xx0->pos[0],xx0->pos[1],xx0->pos[2],xx0->pos[3],0.01);
    xx0->min_radius = BezierMinRadius(xx0->pos[0],xx0->pos[1],xx0->pos[2],xx0->pos[3]);
	ComputeBezierBoundingBox( trk0, GetTrkExtraData(trk0) );
	return TRUE;
}


static STATUS_T ModifyBezier( track_p trk, wAction_t action, coOrd pos )
{
    static int state;

	DIST_T r, d;
    coOrd pos0, pos1;
    int selectedPos;
	track_p trk1;
    EPINX_T ep;
	struct extraData *xx = GetTrkExtraData(trk);
    
    if (GetTrkType(trk) != T_BEZIER) return C_ERROR;

	switch ( action ) {

    case C_START:
        state = 0;
    case C_DOWN:
		ep = PickUnconnectedEndPoint( pos, trk );
		if ( ep == -1 )
			return C_ERROR;
		UndrawNewTrack( trk );
        selectedPos = -1;
        if (state == 1) {
            d = FindDistance(pos, xx->pos[0]);
            for (int i=0;i<4;i++) {
                if( d>FindDistance(pos, xx->pos[i])) {
                    d = FindDistance(pos,xx->pos[i]);
                    if (d < maxDistance) selectedPos = i;
                }
            }
        }
        if (selectedPos>=0) {
            tempSegs(0).type = SEG_BZRTRK;
            tempSegs(0).width = 0;
            tempSegs(0).u.b.pos[0] = xx->pos[0];
            tempSegs(0).u.b.pos[1] = xx->pos[1];
            tempSegs(0).u.b.pos[2] = xx->pos[2];
            tempSegs(0).u.b.pos[3] = xx->pos[3];
            if (selectedPos == 1) drawControlArm(xx->pos[0], xx->pos[1], TRUE);
            if (selectedPos == 3) drawControlArm(xx->pos[3], xx->pos[2], TRUE);
            tempSegs_da.cnt = 6;
            InfoMessage( _("Drag Selected Point") );
            state = 2;
        } else {
            tempSegs(0).type = SEG_BZRTRK;
            tempSegs(0).width = 0;
            tempSegs(0).u.b.pos[0] = xx->pos[0];
            tempSegs(0).u.b.pos[1] = xx->pos[1];
            tempSegs(0).u.b.pos[2] = xx->pos[2];
            tempSegs(0).u.b.pos[3] = xx->pos[3];
            drawControlArm(xx->pos[0], xx->pos[1], FALSE);
            drawControlArm(xx->pos[3], xx->pos[2], FALSE);
            tempSegs_da.cnt = 6;
            InfoMessage( _("Select End Point or Control Point, Enter to finish") );
            state = 1;
        }
        break;
	case C_MOVE:
        if (state == 2) {
            pos0 = xx->pos[0];
            pos1 = xx->pos[3];
            if (selectedPos == 0) {
                pos0 = pos;
            } else if (selectedPos == 3) {
                pos1 = pos;
            }
            d = FindDistance( pos1, pos0 );
            if (d<=minLength) {
                ErrorMessage( MSG_TRK_TOO_SHORT, _("Bezier "), PutDim( fabs(minLength-d) ) );
                return C_CONTINUE;
            }
            xx->pos[selectedPos] = pos;
            
            xx->min_radius = BezierMinRadius(xx->pos[0], xx->pos[1], xx->pos[2], xx->pos[3]);
            xx->length = BezierLength(xx->pos[0], xx->pos[1], xx->pos[2], xx->pos[3], 0.01);
            tempSegs(0).type = SEG_BZRTRK;
            tempSegs(0).width = 0;
            tempSegs(0).u.b.pos[0] = xx->pos[0];
            tempSegs(0).u.b.pos[1] = xx->pos[1];
            tempSegs(0).u.b.pos[2] = xx->pos[2];
            tempSegs(0).u.b.pos[3] = xx->pos[3];
            if (selectedPos == 1) drawControlArm(xx->pos[0], xx->pos[1], TRUE);
            if (selectedPos == 2) drawControlArm(xx->pos[3], xx->pos[2], TRUE);
            tempSegs_da.cnt = 6;
            state = 2;
            InfoMessage( _("Bezier: MinRadius=%s Length=%s"),
                            FormatDistance( xx->min_radius ), FormatDistance( xx->length ) );
        }
        return C_CONTINUE;
	case C_UP:
        DrawNewTrack( trk );
        InfoMessage( _("Select End Point or Control Point, Enter to finish") );
        state = 1;
		return C_CONTINUE;
    case C_OK:
        DrawNewTrack( trk );
        state = 0;
        return C_TERMINATE;
	default:
		;
	}
	return C_ERROR;
}


static DIST_T GetLengthBezier( track_p trk )
{
	struct extraData *xx = GetTrkExtraData(trk);
	return BezierLength(xx->pos[0], xx->pos[1], xx->pos[2], xx->pos[3], 0.01);
}


static BOOL_T GetParamsBezier( int inx, track_p trk, coOrd pos, trackParams_t * params )
{
	struct extraData *xx = GetTrkExtraData(trk);
	params->type = curveTypeBezier;
    GetBezierAngles( &params->arcA0, &params->arcA1, trk );
	
    params->len = xx->length;
    params->min_radius = xx->min_radius;
    params->ep = PickUnconnectedEndPoint( pos, trk );
    if (params->ep == -1)
			return FALSE;
	
	return TRUE;
}

static void AdjustBezierEndPt( track_p t, EPINX_T inx, coOrd pos ) {
    struct extraData *xx = GetTrkExtraData(t);
    if (inx ==0 ) {
        xx->pos[1].x += -xx->pos[0].x+pos.x;
        xx->pos[1].y += -xx->pos[0].y+pos.y;
        xx->pos[0] = pos;
    }
    else {
        xx->pos[2].x += -xx->pos[3].x+pos.x;
        xx->pos[2].y += -xx->pos[3].y+pos.y;
        xx->pos[3] = pos;
    }
}


static BOOL_T MoveEndPtBezier( track_p *trk, EPINX_T *ep, coOrd pos, DIST_T d0 )
{
    struct extraData *xx = GetTrkExtraData(trk);
	//TODO
    /*
    coOrd posCen;
	DIST_T r;
	ANGLE_T angle0;
	ANGLE_T aa;
	
	GetTrkCurveCenter( *trk, &posCen, &r );
	angle0 = FindAngle( posCen, pos );
	aa = R2D( d0/r );
	if ( *ep==0 )
		angle0 += aa - 90.0;
	else
		angle0 -= aa - 90.0;
	AdjustCurveEndPt( *trk, *ep, angle0 );
     */
    AdjustBezierEndPt(*trk, *ep, *pos);
	return TRUE;
}


static BOOL_T QueryBezier( track_p trk, int query )
{
	struct extraData * xx = GetTrkExtraData(trk);
	switch ( query ) {
	case Q_CAN_GROUP:
	case Q_FLIP_ENDPTS:
	case Q_ISTRACK:
	case Q_HAS_DESC:
		return TRUE;
	case Q_EXCEPTION:
		return xx->min_radius < minTrackRadius;
	default:
		return FALSE;
	}
}


static void FlipBezier(
		track_p trk,
		coOrd orig,
		ANGLE_T angle )
{
	struct extraData * xx = GetTrkExtraData(trk);
	FlipPoint( &xx->pos[0], orig, angle );
	FlipPoint( &xx->pos[1], orig, angle );
    FlipPoint( &xx->pos[2], orig, angle );
    FlipPoint( &xx->pos[3], orig, angle );
	ComputeCurveBoundingBox( trk, xx );
}


static BOOL_T MakeParallelBezier(
		track_p trk,
		coOrd pos,
		DIST_T sep,
		track_p * newTrkR,
		coOrd * p0R,
		coOrd * p1R )
{
	struct extraData * xx = GetTrkExtraData(trk);
	struct extraData * xx1;
    coOrd np0,np1,nc1,nc2, p;
    ANGLE_T a,a2;

	//Produce bezier that is translated parallel to the existing Bezier
    // - not a precise result if the bezier end angles are not in the same general direction.
    
    a = FindAngle(xx->pos[0],xx->pos[3]);
    p = BezierFindNearestPoint(&pos, xx->pos[0], xx->pos[1], xx->pos[2], xx->pos[3], 100);
    a2 = FindAngle(pos,p);
    //find parallel move x and y for points
    if ( a2 < 180 ) {
        TRANSLATE(&np0,sep,a+90);
        TRANSLATE(&nc1,sep,a+90);
        TRANSLATE(&nc2,sep,a+90);
        TRANSLATE(&np1,sep,a+90);
    } else {
        TRANSLATE(xx->pos[0],sep,a-90);
        TRANSLATE(xx->pos[1],sep,a-90);
        TRANSLATE(xx->pos[2],sep,a-90);
        TRANSLATE(xx->pos[3],sep,a-90);
    }
	if ( newTrkR ) {
		*newTrkR = NewBezierTrack( np0,nc1,nc2,np1 );
		xx1 = GetTrkExtraData(*newTrkR);
		ComputeBezierBoundingBox( *newTrkR, xx1 );
	} else {
		tempSegs(0).color = wDrawColorBlack;
		tempSegs(0).width = 0;
		tempSegs_da.cnt = 1;
		tempSegs(0).type = SEG_BZRTRK;
		tempSegs(0).u.b.pos[0] = np0;
		tempSegs(0).u.b.pos[1] = nc1;
        tempSegs(0).u.b.pos[2] = nc2;
        tempSegs(0).u.b.pos[3] = np1;
	}
	if ( p0R ) p0R = &np0;
	if ( p1R ) p1R = &np1;
	return TRUE;
}


static trackCmd_t bezierCmds = {
		"BEZIER",
		DrawBezier,
		DistanceBezier,
		DescribeBezier,
		DeleteBezier,
		WriteBezier,
		ReadBezier,
		MoveBezier,
		RotateBezier,
		RescaleBezier,
		NULL,
		NULL,
		SplitBezier,
		TraverseBezier,
		EnumerateBezier,
		NULL,	/* redraw */
		NULL,   /* trim   */
		MergeBezier,
		ModifyBezier,
		GetLengthBezier,
		GetParamsBezier,
		MoveEndPtBezier,
		QueryBezier,
		NULL,	/* ungroup */
		FlipBezier,
		NULL,
		NULL,
		NULL,
		MakeParallelBezier };


EXPORT void BezierSegProc(
		segProc_e cmd,
		trkSeg_p segPtr,
		segProcData_p data )
{
	ANGLE_T a0, a1, a2;
	DIST_T d, circum, d0;
	coOrd p0;
	wIndex_t s0, s1;

	/*
    switch (cmd) {

	case SEGPROC_TRAVERSE1:
        //TODO workout distance to point on a curve
        
		a1 = FindAngle( segPtr->u.c.center, data->traverse1.pos );
		a1 += (segPtr->u.c.radius>0?90.0:-90.0);
		a2 = NormalizeAngle( data->traverse1.angle+a1 );
		data->traverse1.backwards = (a2 < 270 && a2 > 90 );
		a2 = FindAngle( segPtr->u.c.center, data->traverse1.pos );
		if ( data->traverse1.backwards == (segPtr->u.c.radius<0) ) {
			a2 = NormalizeAngle( a2-segPtr->u.c.a0 );
		} else {
			a2 = NormalizeAngle( segPtr->u.c.a0+segPtr->u.c.a1-a2 );
		}
		data->traverse1.dist = a2/360.0*2*M_PI*fabs(segPtr->u.c.radius);
		break;

	case SEGPROC_TRAVERSE2:
        //TODO workout remaining distance to point on a curve
		circum = 2*M_PI*segPtr->u.c.radius;
		if ( circum < 0 )
				circum = - circum;
		d = segPtr->u.c.a1/360.0*circum;
		if ( d > data->traverse2.dist ) {
			a2 = (data->traverse2.dist)/circum*360.0;
			if ( data->traverse2.segDir == (segPtr->u.c.radius<0) ) {
				a2 = NormalizeAngle( segPtr->u.c.a0+a2 );
				a1 = a2+90;
			} else {
				a2 = NormalizeAngle( segPtr->u.c.a0+segPtr->u.c.a1-a2 );
				a1 = a2-90;
			}
			PointOnCircle( &data->traverse2.pos, segPtr->u.c.center, fabs(segPtr->u.c.radius), a2 );
			data->traverse2.dist = 0;
			data->traverse2.angle = a1;
		} else {
			data->traverse2.dist -= d;
		}
		break;

	case SEGPROC_DRAWROADBEDSIDE:
        //TODO - needs parallel bezier problem solved...
		break;

	case SEGPROC_DISTANCE:
        //TODO distance to point
		data->distance.dd = CircleDistance( &data->distance.pos1, segPtr->u.c.center, fabs(segPtr->u.c.radius), segPtr->u.c.a0, segPtr->u.c.a1 );
		break;

	case SEGPROC_FLIP:
        coOrd temp0,temp1,temp3,temp4;
        temp0 = segPtr->u.b.pos[0]
		segPtr->u.b.pos[0] = segPtr;
		break;

	case SEGPROC_NEWTRACK:
		data->newTrack.trk = NewBezierTrack( segPtr->u.b.pos[0], segPtr->u.b.pos[1], segPtr->u.b.pos[2], segPtr->u.b.pos[3]);
		data->newTrack.ep[0] = segPtr->ep[0]);
		data->newTrack.ep[1] = segPtr->.ep[1];
		break;

	case SEGPROC_LENGTH:
		data->length.length = segPtr->u.b.length);
		break;

	case SEGPROC_SPLIT:
		d = segPtr->u.c.a1/360.0 * 2*M_PI * fabs(segPtr->u.c.radius);
		a2 = FindAngle( segPtr->u.c.center, data->split.pos );
		a2 = NormalizeAngle( a2 - segPtr->u.c.a0 );
		if ( a2 > segPtr->u.c.a1 ) {
			if ( a2-segPtr->u.c.a1 < (360-segPtr->u.c.a1)/2.0 )
				a2 = segPtr->u.c.a1;
			else
				a2 = 0.0;
		}
		s0 = 0;
		if ( segPtr->u.c.radius<0 )
			s0 = 1-s0;
		s1 = 1-s0;
		data->split.length[s0] = a2/360.0 * 2*M_PI * fabs(segPtr->u.c.radius);
		data->split.length[s1] = d-data->split.length[s0];
		data->split.newSeg[0] = *segPtr;
		data->split.newSeg[1] = *segPtr;
		data->split.newSeg[s0].u.c.a1 = a2;
		data->split.newSeg[s1].u.c.a0 = NormalizeAngle( data->split.newSeg[s1].u.c.a0 + a2 );
		data->split.newSeg[s1].u.c.a1 -= a2;
		break;

	case SEGPROC_GETANGLE:
		data->getAngle.angle = NormalizeAngle( FindAngle( data->getAngle.pos, segPtr->u.c.center ) + 90 );
		break;
    
	}
     */
}


/****************************************
 *
 * GRAPHICS COMMANDS
 *
 */



EXPORT void PlotBezier(
		long mode,
		coOrd pos0,
		coOrd pos1,
		coOrd pos2,
		BezierData_t * BezierData,
		BOOL_T constrain )
{
	DIST_T d0, d2, r;
	ANGLE_T angle, a0, a1, a2;
	coOrd posx;
    //TODO
    /*
	switch ( mode ) {
	case crvCmdFromEP1:
            
            
			}
     */
}

EXPORT track_p NewBezierTrack( coOrd pos0, coOrd ctl1, coOrd ctl2, coOrd pos1 )
{
	struct extraData *xx;
	track_p p;
	p = NewTrack( 0, T_BEZIER, 2, sizeof *xx );
	xx = GetTrkExtraData(p);
    xx->pos[0] = pos0;
    xx->pos[1] = ctl1;
    xx->pos[2] = ctl2;
    xx->pos[3] = pos1;
    xx->min_radius = BezierMinRadius(pos0,ctl1,ctl2,pos1);
    xx->length = BezierLength(pos0,ctl1,ctl2,pos1, 0.01);
LOG( log_bezier, 1, ( "NewBezierTrack( %0.3f, %0.3f, %0.3f )  = %d\n", pos0.x, pos0.y, GetTrkIndex(p) ) )
	ComputeCurveBoundingBox( p, xx );
	CheckTrackLength( p );
	return p;
}



EXPORT void InitTrkBezier( void )
{
	T_BEZIER = InitObject( &bezierCmds );
	log_bezier = LogFindIndex( "Bezier" );
}
