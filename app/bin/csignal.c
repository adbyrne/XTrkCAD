/** \file csignal.c
 * Signals
 */

/* -*- C -*- ****************************************************************
 *
 *  System        : 
 *  Module        : 
 *  Object Name   : $RCSfile$
 *  Revision      : $Revision$
 *  Date          : $Date$
 *  Author        : $Author$
 *  Created By    : Robert Heller
 *  Created       : Sun Feb 19 13:11:45 2017
 *  Last Modified : <170314.1311>
 *
 *  Description	
 *
 *  Notes
 *
 *  History
 *	
 ****************************************************************************
 *
 *    Copyright (C) 2017  Robert Heller D/B/A Deepwoods Software
 *			51 Locke Hill Road
 *			Wendell, MA 01379-9728
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 ****************************************************************************/

static const char rcsid[] = "@(#) : $Id$";


#include <ctype.h>
#include "track.h"
#include "trackx.h"
#include "compound.h"
#include "i18n.h"

EXPORT TRKTYP_T T_SIGNAL = -1;

static int log_signal = 0;


#if 0
static drawCmd_t signalD = {
	NULL,
	&screenDrawFuncs,
	0,
	1.0,
	0.0,
	{0.0,0.0}, {0.0,0.0},
	Pix2CoOrd, CoOrd2Pix };

static char signalName[STR_SHORT_SIZE];
static int  signalHeadCount;
#endif

typedef struct signalAspect_t {
    char * aspectName;
    char * aspectScript;
} signalAspect_t, *signalAspect_p;

static dynArr_t signalAspect_da;
#define signalAspect(N) DYNARR_N( signalAspect_t, signalAspect_da, N )

typedef struct signalData_t {
    coOrd orig;
    ANGLE_T angle;
    char * name;
    wIndex_t numHeads;
    BOOL_T IsHilite;
    wIndex_t numAspects;
    signalAspect_t aspectList;
} signalData_t, *signalData_p;

static signalData_p GetsignalData ( track_p trk )
{
    return (signalData_p) GetTrkExtraData(trk);
}

#define BASEX 6
#define BASEY 0

#define MASTX 0
#define MASTY 12

#define HEADR 4


#define signal_SF (3.0)

static void DDrawSignal(drawCmd_p d, coOrd orig, ANGLE_T angle, 
                        wIndex_t numHeads, DIST_T scaleRatio, 
                        wDrawColor color )
{
    coOrd p1, p2;
    ANGLE_T x_angle, y_angle;
    DIST_T hoffset;
    wIndex_t ihead;
    
    x_angle = 90-(360-angle);
    if (x_angle < 0) x_angle += 360;
    y_angle = -(360-angle);
    if (y_angle < 0) y_angle += 360;
    
    Translate (&p1, orig, x_angle, (-BASEX) * signal_SF / scaleRatio);
    Translate (&p1, p1,   y_angle, BASEY * signal_SF / scaleRatio);
    Translate (&p2, orig, x_angle, BASEX * signal_SF / scaleRatio);
    Translate (&p2, p2,   y_angle, BASEY * signal_SF / scaleRatio);
    DrawLine(d, p1, p2, 2, color);
    p1 = orig;
    Translate (&p2, orig, x_angle, MASTX * signal_SF / scaleRatio);
    Translate (&p2, p2,   y_angle, MASTY * signal_SF / scaleRatio);
    DrawLine(d, p1, p2, 2, color);
    hoffset = MASTY;
    for (ihead = 0; ihead < numHeads; ihead++) {
        Translate (&p1, orig, x_angle, MASTX * signal_SF / scaleRatio);
        Translate (&p1, p1,   y_angle, (hoffset+HEADR) * signal_SF / scaleRatio);
        DrawFillCircle(d,p1,HEADR * signal_SF / scaleRatio,color);
        hoffset += HEADR*2;
    }
}

static void DrawSignal (track_p t, drawCmd_p d, wDrawColor color )
{
    signalData_p xx = GetsignalData(t);
    DDrawSignal(d,xx->orig, xx->angle, xx->numHeads, GetScaleRatio(GetTrkScale(t)),color);
}

static void SignalBoundingBox (coOrd orig, ANGLE_T angle,wIndex_t numHeads, 
                               DIST_T scaleRatio, coOrd *hi, coOrd *lo)
{
    coOrd p1, p2, headp1, headp2;
    ANGLE_T x_angle, y_angle;
    DIST_T hoffset,delta;
    wIndex_t ihead;
    
    x_angle = 90-(360-angle);
    if (x_angle < 0) x_angle += 360;
    y_angle = -(360-angle);
    if (y_angle < 0) y_angle += 360;
    
    Translate (&p1, orig, x_angle, (-BASEX) * signal_SF / scaleRatio);
    Translate (&p1, p1,   y_angle, BASEY * signal_SF / scaleRatio);
    Translate (&p2, orig, x_angle, BASEX * signal_SF / scaleRatio);
    Translate (&p2, p2,   y_angle, BASEY * signal_SF / scaleRatio);
    *hi = p1; *lo = p1;
    if (p2.x > hi->x) hi->x = p2.x;
    if (p2.x < lo->x) lo->x = p2.x;
    if (p2.y > hi->y) hi->y = p2.y;
    if (p2.y < lo->y) lo->y = p2.y;
    p1 = orig;
    Translate (&p2, orig, x_angle, MASTX * signal_SF / scaleRatio);
    Translate (&p2, p2,   y_angle, MASTY * signal_SF / scaleRatio);
    if (p1.x > hi->x) hi->x = p1.x;
    if (p1.x < lo->x) lo->x = p1.x;
    if (p1.y > hi->y) hi->y = p1.y;
    if (p1.y < lo->y) lo->y = p1.y;
    if (p2.x > hi->x) hi->x = p2.x;
    if (p2.x < lo->x) lo->x = p2.x;
    if (p2.y > hi->y) hi->y = p2.y;
    if (p2.y < lo->y) lo->y = p2.y;
    hoffset = MASTY;
    for (ihead = 0; ihead < numHeads; ihead++) {
        Translate (&p1, orig, x_angle, MASTX * signal_SF / scaleRatio);
        Translate (&p1, p1,   y_angle, (hoffset+HEADR) * signal_SF / scaleRatio);
        delta = HEADR * signal_SF / scaleRatio;
        headp1.x = p1.x - delta;
        headp1.y = p1.y - delta;
        headp2.x = p1.x + delta;
        headp2.y = p1.y + delta;
        if (headp1.x > hi->x) hi->x = headp1.x;
        if (headp1.x < lo->x) lo->x = headp1.x;
        if (headp1.y > hi->y) hi->y = headp1.y;
        if (headp1.y < lo->y) lo->y = headp1.y;
        if (headp2.x > hi->x) hi->x = headp2.x;
        if (headp2.x < lo->x) lo->x = headp2.x;
        if (headp2.y > hi->y) hi->y = headp2.y;
        if (headp2.y < lo->y) lo->y = headp2.y;
        hoffset += HEADR*2;
    }
    
}

static void ComputeSignalBoundingBox (track_p t )
{
    coOrd lo, hi;
    signalData_p xx = GetsignalData(t);
    SignalBoundingBox(xx->orig, xx->angle, xx->numHeads, 
                      GetScaleRatio(GetTrkScale(t)), &hi, &lo);
    SetBoundingBox(t, hi, lo);
}

static DIST_T DistanceSignal (track_p t, coOrd * p )
{
    signalData_p xx = GetsignalData(t);
    return FindDistance(xx->orig, *p);
}

static struct {
    char name[STR_SHORT_SIZE];
    coOrd pos;
    ANGLE_T orient;
    long heads;
} signalProperties;

typedef enum { NM, PS, OR, HD } signalDesc_e;
static descData_t signalDesc[] = {
    /* NM */ { DESC_STRING, N_("Name"),     &signalProperties.name },
    /* PS */ { DESC_POS,    N_("Position"), &signalProperties.pos },
    /* OR */ { DESC_ANGLE,  N_("Angle"),    &signalProperties.orient },
    /* HD */ { DESC_LONG,   N_("Number Of Heads"), &signalProperties.heads },
    { DESC_NULL } };

static void UpdateSignalProperties ( track_p trk, int inx, descData_p
                                     descUpd, BOOL_T needUndoStart )
{
    signalData_p xx = GetsignalData( trk );
    const char *thename;
    char *newName;
    BOOL_T changed, nChanged, pChanged, oChanged;
    
    switch (inx) {
    case NM: break;
    case PS: break;
    case OR: break;
    case HD: break;
    case -1:
        changed = nChanged = pChanged = oChanged = FALSE;
        thename = wStringGetValue( (wString_p) signalDesc[NM].control0 );
        if (strcmp(thename,xx->name) != 0) {
            nChanged = changed = TRUE;
            newName = MyStrdup(thename);
        }
        if (signalProperties.pos.x != xx->orig.x ||
            signalProperties.pos.y != xx->orig.y) {
            pChanged = changed = TRUE;
        }
        if (signalProperties.orient != xx->angle) {
            oChanged = changed = TRUE;
        }
        if (!changed) break;
        if (needUndoStart)
            UndoStart( _("Change Signal"), "Change Signal" );
        UndoModify( trk );
        if (nChanged) {
            MyFree(xx->name);
            xx->name = newName;
        }
        if (pChanged || oChanged) {
            UndrawNewTrack( trk );
        }
        if (pChanged) {
            xx->orig = signalProperties.pos;
        }
        if (oChanged) {
            xx->angle = signalProperties.orient;
        }
        if (pChanged || oChanged) { 
            ComputeSignalBoundingBox( trk );
            DrawNewTrack( trk );
        }
        break;
    }
}


static void DescribeSignal (track_p trk, char * str, CSIZE_T len ) 
{
    signalData_p xx = GetsignalData(trk);
    
    strcpy( str, _(GetTrkTypeName( trk )) );
    str++;
    while (*str) {
        *str = tolower((unsigned char)*str);
        str++;
    }
    sprintf( str, _("(%d [%s]): Layer=%d, %d heads at %0.3f,%0.3f A%0.3f"),
             GetTrkIndex(trk), 
             xx->name,GetTrkLayer(trk)+1, xx->numHeads,
             xx->orig.x, xx->orig.y,xx->angle );
    strncpy(signalProperties.name,xx->name,STR_SHORT_SIZE-1);
    signalProperties.name[STR_SHORT_SIZE-1] = '\0';
    signalProperties.pos = xx->orig;
    signalProperties.orient = xx->angle;
    signalProperties.heads = xx->numHeads;
    signalDesc[HD].mode = DESC_RO;
    signalDesc[NM].mode = DESC_NOREDRAW;
    DoDescribe( _("Signal"), trk, signalDesc, UpdateSignalProperties );
}

static void DeleteSignal ( track_p trk )
{
    wIndex_t ia;
    signalData_p xx = GetsignalData(trk);
    MyFree(xx->name); xx->name = NULL;
    for (ia = 0; ia < xx->numAspects; ia++) {
        MyFree((&(xx->aspectList))[ia].aspectName);
        MyFree((&(xx->aspectList))[ia].aspectScript);
    }
}

static BOOL_T WriteSignal ( track_p t, FILE * f )
{
    BOOL_T rc = TRUE;
    wIndex_t ia;
    signalData_p xx = GetsignalData(t);
    rc &= fprintf(f, "SIGNAL %d %d %s %d %0.6f %0.6f %0.6f %d \"%s\"\n",
                  GetTrkIndex(t), GetTrkLayer(t), GetTrkScaleName(t), 
                  GetTrkVisible(t), xx->orig.x, xx->orig.y, xx->angle, 
                  xx->numHeads, xx->name)>0;
    for (ia = 0; ia < xx->numAspects; ia++) {
        rc &= fprintf(f, "\tASPECT \"%s\" \"%s\"\n",
                      (&(xx->aspectList))[ia].aspectName,
                      (&(xx->aspectList))[ia].aspectScript)>0;
    }
    rc &= fprintf( f, "\tEND\n" )>0;
    return rc;
}

static void ReadSignal ( char * line )
{
    /*TRKINX_T trkindex;*/
    wIndex_t index;
    track_p trk;
    char * cp = NULL;
    wIndex_t ia;
    char *name;
    char *aspname, *aspscript;
    wIndex_t numHeads;
    coOrd orig;
    ANGLE_T angle;
    BOOL_T visible;
    char scale[10];
    wIndex_t layer;
    signalData_p xx;
    if (!GetArgs(line+6,"dLsdpfdq",&index,&layer,scale, &visible, &orig, 
                 &angle, &numHeads,&name)) {
        return;
    }
    DYNARR_RESET( signalAspect_p, signalAspect_da );
    while ( (cp = GetNextLine()) != NULL ) {
        while (isspace((unsigned char)*cp)) cp++;
        if ( strncmp( cp, "END", 3 ) == 0 ) {
            break;
        }
        if ( *cp == '\n' || *cp == '#' ) {
            continue;
        }
        if ( strncmp( cp, "ASPECT", 6 ) == 0 ) {
            if (!GetArgs(cp+4,"qq",&aspname,&aspscript)) return;
            DYNARR_APPEND( signalAspect_p *, signalAspect_da, 10 );
            signalAspect(signalAspect_da.cnt-1).aspectName = aspname;
            signalAspect(signalAspect_da.cnt-1).aspectScript = aspscript;
        }
    }
    trk = NewTrack(index, T_SIGNAL, 0, sizeof(signalData_t)+(sizeof(signalAspect_t)*(signalAspect_da.cnt-1))+1);
    SetTrkVisible(trk, visible);
    SetTrkScale(trk, LookupScale( scale ));
    SetTrkLayer(trk, layer);
    xx = GetsignalData ( trk );
    xx->name = name;
    xx->numHeads = numHeads;
    xx->orig = orig;
    xx->angle = angle;
    xx->numAspects = signalAspect_da.cnt;
    for (ia = 0; ia < xx->numAspects; ia++) {
        (&(xx->aspectList))[ia].aspectName = signalAspect(ia).aspectName;
        (&(xx->aspectList))[ia].aspectScript = signalAspect(ia).aspectScript;
    }
    ComputeSignalBoundingBox(trk);
}

static void MoveSignal (track_p trk, coOrd orig )
{
    signalData_p xx = GetsignalData ( trk );
    xx->orig.x += orig.x;
    xx->orig.y += orig.y;
    ComputeSignalBoundingBox(trk);
}

static void RotateSignal (track_p trk, coOrd orig, ANGLE_T angle ) 
{
    signalData_p xx = GetsignalData ( trk );
    Rotate(&(xx->orig), orig, angle);
    xx->angle = NormalizeAngle(xx->angle + angle);
    ComputeSignalBoundingBox(trk);
}

static void RescaleSignal (track_p trk, FLOAT_T ratio ) 
{
}

static void FlipSignal (track_p trk, coOrd orig, ANGLE_T angle )
{
    signalData_p xx = GetsignalData ( trk );
    FlipPoint(&(xx->orig), orig, angle);
    xx->angle = NormalizeAngle(2*angle - xx->angle);
    ComputeSignalBoundingBox(trk);
}

static trackCmd_t signalCmds = {
    "SIGNAL",
    DrawSignal,
    DistanceSignal,
    DescribeSignal,
    DeleteSignal,
    WriteSignal,
    ReadSignal,
    MoveSignal,
    RotateSignal,
    RescaleSignal,
    NULL, /* audit */
    NULL, /* getAngle */
    NULL, /* split */
    NULL, /* traverse */
    NULL, /* enumerate */
    NULL, /* redraw */
    NULL, /* trim */
    NULL, /* merge */
    NULL, /* modify */
    NULL, /* getLength */
    NULL, /* getTrkParams */
    NULL, /* moveEndPt */
    NULL, /* query */
    NULL, /* ungroup */
    FlipSignal, /* flip */
    NULL, /* drawPositionIndicator */
    NULL, /* advancePositionIndicator */
    NULL, /* checkTraverse */
    NULL, /* makeParallel */
    NULL  /* drawDesc */
};

static BOOL_T signalCreate_P;
static coOrd signalEditOrig;
static ANGLE_T signalEditAngle;
static track_p signalEditTrack;

static char signalEditName[STR_SHORT_SIZE];
static long signalEditHeadCount;
static char signalAspectEditName[STR_SHORT_SIZE];
static char signalAspectEditScript[STR_LONG_SIZE];
static long signalAspectEditIndex;

static paramIntegerRange_t r1_3 = {1, 3};
static wPos_t aspectListWidths[] = { STR_SHORT_SIZE, 150 };
static const char * aspectListTitles[] = { N_("Name"), N_("Script") };
static paramListData_t aspectListData = {10, 400, 2, aspectListWidths, aspectListTitles};

static void AspectEdit( void * action );
static void AspectAdd( void * action );
static void AspectDelete( void * action );

static paramFloatRange_t r_1000_1000    = { -1000.0, 1000.0, 80 };
static paramFloatRange_t r0_360         = { 0.0, 360.0, 80 };
static paramData_t signalEditPLs[] = {
#define I_SIGNALNAME (0)
    /*0*/ { PD_STRING, signalEditName, "name", PDO_NOPREF, (void*)200, N_("Name") },
#define I_ORIGX (1)
    /*1*/ { PD_FLOAT, &signalEditOrig.x, "origx", PDO_DIM, &r_1000_1000, N_("Orgin X") }, 
#define I_ORIGY (2)
    /*2*/ { PD_FLOAT, &signalEditOrig.y, "origy", PDO_DIM, &r_1000_1000, N_("Origin Y") },
#define I_ANGLE (3)
    /*3*/ { PD_FLOAT, &signalEditAngle, "origa", PDO_ANGLE, &r0_360, N_("Angle") },
#define I_SIGNALHEADCOUNT (4)
    /*4*/ { PD_LONG,   &signalEditHeadCount, "headCount", PDO_NOPREF, &r1_3, N_("Number of Heads") },
#define I_SIGNALASPECTLIST (5)
#define aspectSelL ((wList_p)signalEditPLs[I_SIGNALASPECTLIST].control)
    /*5*/ { PD_LIST, NULL, "inx", PDO_DLGRESETMARGIN|PDO_DLGRESIZE, &aspectListData, NULL, BL_MANY },
#define I_SIGNALASPECTEDIT (6)
    /*6*/ { PD_BUTTON, (void*)AspectEdit, "edit", PDO_DLGCMDBUTTON, NULL, N_("Edit Aspect") },
#define I_SIGNALASPECTADD (7)
    /*7*/ { PD_BUTTON, (void*)AspectAdd, "add", PDO_DLGCMDBUTTON, NULL, N_("Add Aspect") },
#define I_SIGNALASPECTDELETE (8)
    /*8*/ { PD_BUTTON, (void*)AspectDelete, "delete", 0, NULL, N_("Delete Aspect") },
};
static paramGroup_t signalEditPG = { "signalEdit", 0, signalEditPLs, sizeof signalEditPLs/sizeof signalEditPLs[0] };
static wWin_p signalEditW;

static paramIntegerRange_t rm1_999999 = { -1, 999999 };

static paramData_t aspectEditPLs[] = {
#define I_ASPECTNAME (0)
    /*0*/ { PD_STRING, signalAspectEditName, "name", PDO_NOPREF, (void*)200,  N_("Name") },
#define I_ASPECTSCRIPT (1)
    /*1*/ { PD_STRING, signalAspectEditScript, "script", PDO_NOPREF, (void*)350, N_("Script") },
#define I_ASPECTINDEX (2)
    /*2*/ { PD_LONG,   &signalAspectEditIndex, "index", PDO_NOPREF, &rm1_999999, N_("Aspect Index"), BO_READONLY },
};

static paramGroup_t aspectEditPG = { "aspectEdit", 0, aspectEditPLs, sizeof aspectEditPLs/sizeof aspectEditPLs[0] };
static wWin_p aspectEditW;


static void SignalEditOk ( void * junk )
{
    track_p trk;
    signalData_p xx;
    wIndex_t ia;
    
    if (signalCreate_P) {
        UndoStart( _("Create Signal"), "Create Signal");
        trk = NewTrack(0, T_SIGNAL, 0, sizeof(signalData_t)+(sizeof(signalAspect_t)*(signalAspect_da.cnt-1))+1);
        xx = GetsignalData(trk);
    } else {
        UndoStart( _("Modify Signal"), "Modify Signal");
        trk = signalEditTrack;
        xx = GetsignalData(trk);
        if (xx->numAspects != signalAspect_da.cnt) {
            /* We need to reallocate the extra data. */
            /* We will delete the Signal and re-create it. */
            BOOL_T visible = GetTrkVisible(trk);
            SCALEINX_T scale = GetTrkScale(trk);
            LAYER_T layer = GetTrkLayer(trk);
            wIndex_t tindx = GetTrkIndex(trk);
            FreeTrack(trk);
            trk = NewTrack(tindx, T_SIGNAL, 0, sizeof(signalData_t)+(sizeof(signalAspect_t)*(signalAspect_da.cnt-1))+1);
            SetTrkVisible(trk,visible);
            SetTrkScale(trk,scale);
            SetTrkLayer(trk,layer);
            xx = GetsignalData(trk);
        }
    }
    xx->orig = signalEditOrig;
    xx->angle = signalEditAngle;
    xx->numHeads = signalEditHeadCount;
    if ( xx->name == NULL || strncmp (xx->name, signalEditName, STR_SHORT_SIZE) != 0) {
        MyFree(xx->name);
        xx->name = MyStrdup(signalEditName);
    }
    xx->numAspects = signalAspect_da.cnt;
    for (ia = 0; ia < xx->numAspects; ia++) {
        if ((&(xx->aspectList))[ia].aspectName == NULL) {
            (&(xx->aspectList))[ia].aspectName = signalAspect(ia).aspectName;
        } else if (strcmp((&(xx->aspectList))[ia].aspectName,signalAspect(ia).aspectName) != 0) {
            MyFree((&(xx->aspectList))[ia].aspectName);
            (&(xx->aspectList))[ia].aspectName = signalAspect(ia).aspectName;
        } else {
            MyFree(signalAspect(ia).aspectName);
        }
        if ((&(xx->aspectList))[ia].aspectScript == NULL) {
            (&(xx->aspectList))[ia].aspectScript = signalAspect(ia).aspectScript;
        } else if (strcmp((&(xx->aspectList))[ia].aspectScript,signalAspect(ia).aspectScript) != 0) {
            MyFree((&(xx->aspectList))[ia].aspectScript);
            (&(xx->aspectList))[ia].aspectScript = signalAspect(ia).aspectScript;
        } else {
            MyFree(signalAspect(ia).aspectScript);
        }
    }
    UndoEnd();
    DoRedraw();
    ComputeSignalBoundingBox(trk);
    wHide( signalEditW );
}

static void SignalEditCancel ( wWin_p junk )
{
    wIndex_t ia;

    for (ia = 0; ia < signalAspect_da.cnt; ia++) {
        MyFree(signalAspect(ia).aspectName);
        MyFree(signalAspect(ia).aspectScript);
    }
    DYNARR_RESET( signalAspect_p, signalAspect_da );
    wHide( signalEditW );
}

static void SignalEditDlgUpdate (paramGroup_p pg, int inx, void *valueP )
{
    wIndex_t selcnt = wListGetSelectedCount( aspectSelL );
    
    if ( inx != I_SIGNALASPECTLIST ) return;
    ParamControlActive( &signalEditPG, I_SIGNALASPECTEDIT, selcnt>0 );
    ParamControlActive( &signalEditPG, I_SIGNALASPECTADD, TRUE );
    ParamControlActive( &signalEditPG, I_SIGNALASPECTDELETE, selcnt>0 );
}

static void aspectEditOK ( void * junk )
{
    if (signalAspectEditIndex < 0) {
        DYNARR_APPEND( signalAspect_p *, signalAspect_da, 10 );
        signalAspect(signalAspect_da.cnt-1).aspectName = MyStrdup(signalAspectEditName);
        signalAspect(signalAspect_da.cnt-1).aspectScript = MyStrdup(signalAspectEditScript);
        snprintf(message,sizeof(message),"%s\t%s",signalAspectEditName,signalAspectEditScript);
        wListAddValue( aspectSelL, message, NULL, NULL );
    } else {
        if ( strncmp( signalAspectEditName, signalAspect(signalAspectEditIndex).aspectName,STR_SHORT_SIZE ) != 0 ) {
            MyFree(signalAspect(signalAspectEditIndex).aspectName);
            signalAspect(signalAspectEditIndex).aspectName = MyStrdup(signalAspectEditName);
        }
        if ( strncmp( signalAspectEditScript, signalAspect(signalAspectEditIndex).aspectScript, STR_LONG_SIZE ) != 0 ) {
            MyFree(signalAspect(signalAspectEditIndex).aspectScript);
            signalAspect(signalAspectEditIndex).aspectScript = MyStrdup(signalAspectEditScript);
        }
        snprintf(message,sizeof(message),"%s\t%s",signalAspect(signalAspectEditIndex).aspectName,signalAspect(signalAspectEditIndex).aspectScript);
        wListSetValues( aspectSelL, signalAspectEditIndex, message, NULL, NULL );
    }
    wHide( aspectEditW );
}

static void EditAspectDialog ( wIndex_t inx )
{
    if (inx < 0) {
        signalAspectEditName[0] = '\0';
        signalAspectEditScript[0] = '\0';
    } else {
        strncpy(signalAspectEditName,signalAspect(inx).aspectName,STR_SHORT_SIZE);
        strncpy(signalAspectEditScript,signalAspect(inx).aspectScript,STR_LONG_SIZE);
    }
    signalAspectEditIndex = inx;
    if ( !aspectEditW ) {
        ParamRegister( &aspectEditPG );
        aspectEditW = ParamCreateDialog (&aspectEditPG,
                                         MakeWindowTitle(_("Edit aspect")),
                                         _("Ok"), aspectEditOK,
                                         wHide, TRUE, NULL,F_BLOCK,NULL);
    }
    ParamLoadControls( &aspectEditPG );
    wShow( aspectEditW );
}

static void AspectEdit( void * action )
{
    wIndex_t selcnt = wListGetSelectedCount( aspectSelL );
    wIndex_t inx, cnt;
    
    if ( selcnt != 1) return;
    cnt = wListGetCount( aspectSelL );
    for ( inx=0;
          inx<cnt && wListGetItemSelected( aspectSelL, inx ) != TRUE;
          inx++ );
    if ( inx >= cnt ) return;
    EditAspectDialog(inx);
}

static void AspectAdd( void * action )
{
    EditAspectDialog(-1);
}

static void MoveAspectUp (wIndex_t inx)
{
    wIndex_t cnt = signalAspect_da.cnt;
    wIndex_t ia;
    
    MyFree(signalAspect(inx).aspectName);
    MyFree(signalAspect(inx).aspectScript);
    for (ia = inx+1; ia < cnt; ia++) {
        signalAspect(ia-1).aspectName = signalAspect(ia).aspectName;
        signalAspect(ia-1).aspectScript = signalAspect(ia).aspectScript;
    }
    DYNARR_SET(signalAspect_t,signalAspect_da,cnt-1);
}

static void AspectDelete( void * action )
{
    wIndex_t selcnt = wListGetSelectedCount( aspectSelL );
    wIndex_t inx, cnt;
    
    if ( selcnt <= 0) return;
    if ( (!NoticeMessage2( 1, _("Are you sure you want to delete the %d aspect(s)"), _("Yes"), _("No"), selcnt ) ) )
        return;
    cnt = wListGetCount( aspectSelL );
    for ( inx=0; inx<cnt; inx++ ) {
        if ( !wListGetItemSelected( aspectSelL, inx ) ) continue;
        wListDelete( aspectSelL, inx );
        MoveAspectUp(inx);
        inx--;
        cnt--;
    }
    DoChangeNotification( CHANGE_PARAMS );
}

static void EditSignalDialog()
{
    signalData_p xx;
    wIndex_t ia;
    
    if ( !signalEditW ) {
        ParamRegister( &signalEditPG );
        signalEditW = ParamCreateDialog (&signalEditPG,
                                         MakeWindowTitle(_("Edit signal")),
                                         _("Ok"), SignalEditOk, 
                                         SignalEditCancel, TRUE, NULL, 
                                         F_RESIZE|F_RECALLSIZE|F_BLOCK,
                                         SignalEditDlgUpdate );
    }
    if (signalCreate_P) {
        signalEditName[0] = '\0';
        signalEditHeadCount = 1;
        wListClear( aspectSelL );
        DYNARR_RESET( signalAspect_p, signalAspect_da );
    } else {
        xx = GetsignalData ( signalEditTrack );
        strncpy(signalEditName,xx->name,STR_SHORT_SIZE);
        signalEditHeadCount = xx->numHeads;
        signalEditOrig = xx->orig;
        signalEditAngle = xx->angle;
        wListClear( aspectSelL );
        DYNARR_RESET( signalAspect_p, signalAspect_da );
        for (ia = 0; ia < xx->numAspects; ia++) {
            snprintf(message,sizeof(message),"%s\t%s",(&(xx->aspectList))[ia].aspectName,
                    (&(xx->aspectList))[ia].aspectScript);
            wListAddValue( aspectSelL, message, NULL, NULL );
            DYNARR_APPEND( signalAspect_p *, signalAspect_da, 10 );
            signalAspect(signalAspect_da.cnt-1).aspectName = MyStrdup((&(xx->aspectList))[ia].aspectName);
            signalAspect(signalAspect_da.cnt-1).aspectScript = MyStrdup((&(xx->aspectList))[ia].aspectScript);
        }
    }
    ParamLoadControls( &signalEditPG );
    ParamControlActive( &signalEditPG, I_SIGNALASPECTEDIT, FALSE );
    ParamControlActive( &signalEditPG, I_SIGNALASPECTADD, TRUE );
    ParamControlActive( &signalEditPG, I_SIGNALASPECTDELETE, FALSE );
    wShow( signalEditW );
}




static void EditSignal (track_p trk)
{
    signalCreate_P = FALSE;
    signalEditTrack = trk;
    EditSignalDialog();
}

static void CreateNewSignal (coOrd orig, ANGLE_T angle)
{
    signalCreate_P = TRUE;
    signalEditOrig = orig;
    signalEditAngle = angle;
    EditSignalDialog();
}

static coOrd pos0;
static ANGLE_T orient;

static STATUS_T CmdSignal ( wAction_t action, coOrd pos )
{
    
    
    switch (action) {
    case C_START:
        InfoMessage(_("Place base of signal"));
        return C_CONTINUE;
    case C_DOWN:
        SnapPos(&pos);
        pos0 = pos;
        InfoMessage(_("Drag to orient signal"));
        return C_CONTINUE;
    case C_MOVE:
        SnapPos(&pos);
        orient = FindAngle(pos0,pos);
        DDrawSignal( &tempD, pos0, orient, 1, GetScaleRatio(curScaleInx), wDrawColorBlack );
        return C_CONTINUE;
    case C_UP:
        SnapPos(&pos);
        orient = FindAngle(pos0,pos);
        CreateNewSignal(pos0,orient);
        return C_TERMINATE;
    case C_REDRAW:
    case C_CANCEL:
        DDrawSignal( &tempD, pos0, orient, 1, GetScaleRatio(curScaleInx), wDrawColorBlack );
        return C_CONTINUE;
    default:
        return C_CONTINUE;
    }
}

static coOrd sighiliteOrig, sighiliteSize;
static POS_T sighiliteBorder;
static wDrawColor sighiliteColor = 0;
static void DrawSignalTrackHilite( void )
{
	wPos_t x, y, w, h;
	if (sighiliteColor==0)
		sighiliteColor = wDrawColorGray(87);
	w = (wPos_t)((sighiliteSize.x/mainD.scale)*mainD.dpi+0.5);
	h = (wPos_t)((sighiliteSize.y/mainD.scale)*mainD.dpi+0.5);
	mainD.CoOrd2Pix(&mainD,sighiliteOrig,&x,&y);
	wDrawFilledRectangle( mainD.d, x, y, w, h, sighiliteColor, wDrawOptTemp );
}

static int SignalMgmProc ( int cmd, void * data )
{
    track_p trk = (track_p) data;
    signalData_p xx = GetsignalData(trk);
    /*char msg[STR_SIZE];*/
    
    switch ( cmd ) {
    case CONTMGM_CAN_EDIT:
        return TRUE;
        break;
    case CONTMGM_DO_EDIT:
        EditSignal(trk);
        return TRUE;
        break;
    case CONTMGM_CAN_DELETE:
        return TRUE;
        break;
    case CONTMGM_DO_DELETE:
        DeleteTrack(trk, FALSE);
        return TRUE;
        break;
    case CONTMGM_DO_HILIGHT:
        if (!xx->IsHilite) {
            sighiliteBorder = mainD.scale*0.1;
            if ( sighiliteBorder < trackGauge ) sighiliteBorder = trackGauge;
            GetBoundingBox( trk, &sighiliteSize, &sighiliteOrig );
            sighiliteOrig.x -= sighiliteBorder;
            sighiliteOrig.y -= sighiliteBorder;
            sighiliteSize.x -= sighiliteOrig.x-sighiliteBorder;
            sighiliteSize.y -= sighiliteOrig.y-sighiliteBorder;
            DrawSignalTrackHilite();
            xx->IsHilite = TRUE;
        }
        break;
    case CONTMGM_UN_HILIGHT:
        if (xx->IsHilite) {
            sighiliteBorder = mainD.scale*0.1;
            if ( sighiliteBorder < trackGauge ) sighiliteBorder = trackGauge;
            GetBoundingBox( trk, &sighiliteSize, &sighiliteOrig );
            sighiliteOrig.x -= sighiliteBorder;
            sighiliteOrig.y -= sighiliteBorder;
            sighiliteSize.x -= sighiliteOrig.x-sighiliteBorder;
            sighiliteSize.y -= sighiliteOrig.y-sighiliteBorder;
            DrawSignalTrackHilite();
            xx->IsHilite = FALSE;
        }
        break;
    case CONTMGM_GET_TITLE:
        sprintf(message,"\t%s\t",xx->name);
        break;
    }
    return FALSE;
}

#include "bitmaps/signal.xpm"

EXPORT void SignalMgmLoad ( void )
{
    track_p trk;
    static wIcon_p signalI = NULL;
    
    if (signalI == NULL) {
        signalI = wIconCreatePixMap( signal_xpm );
    }
    
    TRK_ITERATE(trk) {
        if (GetTrkType(trk) != T_SIGNAL) continue;
        ContMgmLoad (signalI, SignalMgmProc, (void *) trk );
    }
}

#define ACCL_SIGNAL 0

EXPORT void InitCmdSignal ( wMenu_p menu )
{
    AddMenuButton( menu, CmdSignal, "cmdSignal", _("Signal"), 
                   wIconCreatePixMap( signal_xpm ), LEVEL0_50, IC_STICKY|IC_POPUP2, ACCL_SIGNAL, NULL );
}

EXPORT void InitTrkSignal ( void )
{
    T_SIGNAL = InitObject ( &signalCmds );
    log_signal = LogFindIndex ( "signal" );
}
