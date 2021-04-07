/** \file cswitchmotor.c
 * Switch Motors
 */

/* Created by Robert Heller on Sat Mar 14 10:39:56 2009
 * ------------------------------------------------------------------
 * Modification History: $Log: not supported by cvs2svn $
 * Modification History: Revision 1.5  2009/11/23 19:46:16  rheller
 * Modification History: Block and Switchmotor updates
 * Modification History:
 * Modification History: Revision 1.4  2009/09/16 18:32:24  m_fischer
 * Modification History: Remove unused locals
 * Modification History:
 * Modification History: Revision 1.3  2009/09/05 16:40:53  m_fischer
 * Modification History: Make layout control commands a build-time choice
 * Modification History:
 * Modification History: Revision 1.2  2009/07/08 19:13:58  m_fischer
 * Modification History: Make compile under MSVC
 * Modification History:
 * Modification History: Revision 1.1  2009/07/08 18:40:27  m_fischer
 * Modification History: Add switchmotor and block for layout control
 * Modification History:
 * Modification History: Revision 1.1  2002/07/28 14:03:50  heller
 * Modification History: Add it copyright notice headers
 * Modification History:
 * ------------------------------------------------------------------
 * Contents:
 * ------------------------------------------------------------------
 *
 *     Generic Project
 *     Copyright (C) 2005  Robert Heller D/B/A Deepwoods Software
 * 			51 Locke Hill Road
 * 			Wendell, MA 01379-9728
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * ------------------------------------------------------------------
 *     Philip Cameron -- Automatic Block Signaling
 *
 *     This uses lcc producer/consumer events.
 *
 *     A switch machine is placed on every turnout that is part of a
 *     route between two blocks. The switches can be both manual (left
 *     click on turnout) and external (consume event). In either case
 *     an event is produced whenever the position changes.
 *
 *     There is on event for each position that the turnout can have.
 *     The name of the event is the name of path position.
 *
 *     The switch motor can be locked (prevent position change) by
 *     consuming a lock event or when the switch becomes occupied.
 *
 * ------------------------------------------------------------------
 */

#include "compound.h"
#include "cselect.h"
#include "cundo.h"
#include "custom.h"
#include "fileio.h"
#include "param.h"
#include "track.h"
#include "trackx.h"
#include "layout.h"
#include "clcc.h"
#include "common-ui.h"
#ifdef UTFCONVERT
#include "include/utf8convert.h"
#endif // UTFCONVERT

EXPORT TRKTYP_T T_SWITCHMOTOR = -1;

static int log_switchmotor = 0;

static track_p FindSwitchMotor (track_p trk);

static drawCmd_t switchmotorD = {
	NULL,
	&screenDrawFuncs,
	0,
	1.0,
	0.0,
	{0.0,0.0}, {0.0,0.0},
	Pix2CoOrd, CoOrd2Pix };

static char switchmotorName[STR_SHORT_SIZE];
static char switchmotorUnlocked[STR_LONG_SIZE];
static char switchmotorLocked[STR_LONG_SIZE];
static long int switchmotorTonum;
static track_p switchmotorTurnout;

static track_p last_motor;
static track_p first_motor;

static paramIntegerRange_t r0_999999 = { 0, 999999 };

static paramData_t switchmotorPLs[] = {
/*0*/ { PD_STRING, switchmotorName, "name", PDO_NOPREF|PDO_STRINGLIMITLENGTH, I2VP(200), N_("Name"), 0, 0, sizeof(switchmotorName)},
/*1*/ { PD_LONG,   &switchmotorTonum, "turnoutNumber", PDO_NOPREF, &r0_999999, N_("Turnout Number"), BO_READONLY },
/*2*/ { PD_STRING, switchmotorUnlocked, "unlocked", PDO_NOPREF|PDO_STRINGLIMITLENGTH, I2VP(350), N_("Unlocked"), 0, 0, sizeof(switchmotorUnlocked)},
/*3*/ { PD_STRING, switchmotorLocked, "locked", PDO_NOPREF | PDO_STRINGLIMITLENGTH, I2VP(350), N_("Locked"), 0, 0, sizeof(switchmotorLocked)},
};

static paramGroup_t switchmotorPG = { "switchmotor", 0, switchmotorPLs, sizeof switchmotorPLs/sizeof switchmotorPLs[0] };
static wWin_p switchmotorW;

static char switchmotorEditName[STR_SHORT_SIZE];
static char switchmotorEditScript[STR_LONG_SIZE];
static char switchmotorEditUnlocked[STR_LONG_SIZE];
static char switchmotorEditLocked[STR_LONG_SIZE];
static long switchmotorEditIndex;
static long int switchmotorEditTonum;
static track_p switchmotorEditTrack;

static paramIntegerRange_t r1_3 = {1, 3};
static wWinPix_t smotorListWidths[] = { STR_SHORT_SIZE, 150 };
static const char * smotorListTitles[] = { N_("Position"), N_("Script") };
static paramListData_t smotorListData = {10, 400, 2, smotorListWidths, smotorListTitles};

static void SmotorEdit( void * action );


static paramData_t switchmotorEditPLs[] = {
    /*0*/ { PD_STRING, switchmotorEditName, "name", PDO_NOPREF | PDO_STRINGLIMITLENGTH,
	    I2VP(200), N_("Name"), 0, 0, sizeof(switchmotorEditName)},
    /*1*/ { PD_LONG,   &switchmotorEditTonum, "turnoutNumber", PDO_NOPREF,
	    &r0_999999, N_("Turnout Number"), BO_READONLY }, 
    /*2*/ { PD_STRING, switchmotorEditUnlocked, "unlocked", PDO_NOPREF | PDO_STRINGLIMITLENGTH,
	    I2VP(350), N_("Unlocked"), 0, 0, sizeof(switchmotorEditUnlocked)},
    /*3*/ { PD_STRING, switchmotorEditLocked, "locked", PDO_NOPREF | PDO_STRINGLIMITLENGTH,
	    I2VP(350), N_("Locked"), 0, 0, sizeof(switchmotorEditLocked)},
#define I_SWITCHMOTORLIST (4)
#define smotorSelL ((wList_p)switchmotorEditPLs[I_SWITCHMOTORLIST].control)
    /*5*/ { PD_LIST, NULL, "inx", PDO_DLGRESETMARGIN|PDO_DLGRESIZE, &smotorListData, NULL, BL_MANY },
#define I_SWITCHMOTOREDIT (5)
    /*6*/ { PD_BUTTON, (void*)SmotorEdit, "edit", PDO_DLGCMDBUTTON, NULL, N_("Edit Position") },
};

static paramGroup_t switchmotorEditPG = { "switchmotorEdit", 0, switchmotorEditPLs, sizeof switchmotorEditPLs/sizeof switchmotorEditPLs[0] };
static wWin_p switchmotorEditW;

static char smotorEditName[STR_SHORT_SIZE];
static char smotorEditScript[STR_LONG_SIZE];
static long smotorEditIndex;
static paramIntegerRange_t rm1_999999 = { -1, 999999 };

static paramData_t smotorEditPLs[] = {
    /*0*/ { PD_STRING, smotorEditName, "name", PDO_NOPREF|PDO_STRINGLIMITLENGTH,
	    I2VP(200),  N_("Name"), 0, 0, sizeof(smotorEditName)},
    /*1*/ { PD_STRING, smotorEditScript, "script", PDO_NOPREF|PDO_STRINGLIMITLENGTH,
	    I2VP(350), N_("Script"), 0, 0, sizeof(smotorEditScript)},
};

static paramGroup_t smotorEditPG = { "smotorEdit", 0, smotorEditPLs, sizeof smotorEditPLs/sizeof smotorEditPLs[0] };
static wWin_p smotorEditW;

static void SwitchMotorOk ( void * junk );
static void initSwitchMotorData(track_p trk);

/*
static dynArr_t switchmotorTrk_da;
#define switchmotorTrk(N) DYNARR_N( track_p , switchmotorTrk_da, N )
*/

typedef struct turnoutPos_t {
    char * posName;
    char * posScript;
} turnoutPos_t, *turnoutPos_p;

static dynArr_t turnoutPos_da;
#define turnoutPos(N) DYNARR_N( turnoutPos_t, turnoutPos_da, N )

typedef struct switchmotorData_t {
    extraDataBase_t base;
    char * name;
    evtBlk_p prodLockEvt;
    evtBlk_p prodPosEvt;
    BOOL_T IsHilite;
    BOOL_T AutoGenerated;
    TRKINX_T turnindx;
    track_p turnout;
    track_p next_motor;
    wIndex_t numPos;
    turnoutPos_t turnoutPosList; // Must be last
} switchmotorData_t, *switchmotorData_p;

static void EditSwitchMotor (track_p trk);

static switchmotorData_p GetswitchmotorData ( track_p trk )
{
	return GET_EXTRA_DATA( trk, T_SWITCHMOTOR, switchmotorData_t );
}

#if 0
#include "bitmaps/switchmotormark.xbm"
static wDrawBitMap_p switchmotormark_bm = NULL;
#endif

static coOrd switchmotorPolyLk_Pix[] = {
	{16,13}, {16,20}, {24,20}, {24,13}, {16,13} };
#define switchmotorPolyLk_CNT (sizeof(switchmotorPolyLk_Pix)/sizeof(switchmotorPolyLk_Pix[0]))
static coOrd switchmotorPoly_Pix[] = {
    {6,0}, {6,13}, {4,13}, {4,19}, {6,19}, {6,23}, {9,23}, {9,19}, {13,19},
    {13,23}, {27,23}, {27,10}, {13,10}, {13,13}, {9,13}, {9,0}, {6,0} };
#define switchmotorPoly_CNT (sizeof(switchmotorPoly_Pix)/sizeof(switchmotorPoly_Pix[0]))
#define switchmotorPoly_SF (3.0)

// locate the switch machine near the turnout
static void locateSwitchMotor( track_p t_trk, coOrd *orig, ANGLE_T *angle )
{
	// Place to left side (headed into turnout) of track
	*orig = GetTrkEndPos( t_trk, 0 );
	*angle = GetTrkEndAngle( t_trk, 0 );
	Translate ( orig, *orig, *angle -90.0, 2.5*trackGauge );
	Translate ( orig, *orig, *angle + 180.0, 2.0*trackGauge );
}

static void ComputeSwitchMotorBoundingBox (track_p t)
{
    coOrd hi, lo, p;
    switchmotorData_p data_p = GetswitchmotorData(t);
    track_p t_trk = data_p->turnout;
    struct extraDataCompound_t *xx = GET_EXTRA_DATA(t_trk, T_TURNOUT, extraDataCompound_t);
    coOrd orig;
    ANGLE_T angle;
    SCALEINX_T s = GetTrkScale(data_p->turnout);
    DIST_T scaleRatio = GetScaleRatio(s);
    int iPoint;
    ANGLE_T x_angle, y_angle;
    
    LOG( log_switchmotor, 1, ("*** ComputeSwitchMotorBoundingBox(): T%d %s\n",GetTrkIndex(t),GetTrkTypeName(t)))

    locateSwitchMotor( t_trk, &orig, &angle );

    x_angle = 90-(360-angle);
    if (x_angle < 0) x_angle += 360;
    y_angle = -(360-angle);
    if (y_angle < 0) y_angle += 360;
    
    
    for (iPoint = 0; iPoint < switchmotorPoly_CNT; iPoint++) {
        Translate (&p, orig, x_angle, switchmotorPoly_Pix[iPoint].x * switchmotorPoly_SF / scaleRatio );
        Translate (&p, p, y_angle, (10+switchmotorPoly_Pix[iPoint].y) * switchmotorPoly_SF / scaleRatio );
        if (iPoint == 0) {
            lo = p;
            hi = p;
        } else {
            if (p.x < lo.x) lo.x = p.x;
            if (p.y < lo.y) lo.y = p.y;
            if (p.x > hi.x) hi.x = p.x;
            if (p.y > hi.y) hi.y = p.y;
        }
    }
    SetBoundingBox(t, hi, lo);
}
    
    
static void DrawSwitchMotor (track_p t, drawCmd_p d, wDrawColor color )
{
    coOrd p[switchmotorPoly_CNT], pl[switchmotorPolyLk_CNT];
    switchmotorData_p data_p = GetswitchmotorData(t);
    track_p t_trk = data_p->turnout;
    struct extraDataCompound_t *xx = GET_EXTRA_DATA(t_trk, T_TURNOUT, extraDataCompound_t);
    coOrd orig;
    ANGLE_T angle;
    SCALEINX_T s = GetTrkScale(data_p->turnout);
    DIST_T scaleRatio = GetScaleRatio(s);
    int iPoint;
    ANGLE_T x_angle, y_angle;

#if 0
    LOG( log_switchmotor, 1, ("*** DrawSwitchMotor(): T%d %s\n",GetTrkIndex(t),GetTrkTypeName(t)))
#endif
    
    locateSwitchMotor( t_trk, &orig, &angle );

    x_angle = 90-(360-angle);
    if (x_angle < 0) x_angle += 360;
    y_angle = -(360-angle);
    if (y_angle < 0) y_angle += 360;
    
    
    for (iPoint = 0; iPoint < switchmotorPoly_CNT; iPoint++) {
        Translate (&p[iPoint], orig, x_angle,
			switchmotorPoly_Pix[iPoint].x * switchmotorPoly_SF / scaleRatio );
        Translate (&p[iPoint], p[iPoint], y_angle,
			(10+switchmotorPoly_Pix[iPoint].y) * switchmotorPoly_SF / scaleRatio );
    }
    for (iPoint = 0; iPoint < switchmotorPolyLk_CNT; iPoint++) {
        Translate (&pl[iPoint], orig, x_angle,
			switchmotorPolyLk_Pix[iPoint].x * switchmotorPoly_SF / scaleRatio );
        Translate (&pl[iPoint], pl[iPoint], y_angle,
			(10+switchmotorPolyLk_Pix[iPoint].y) * switchmotorPoly_SF / scaleRatio );
    }
    DrawPoly(d, switchmotorPoly_CNT, p, NULL, drawColorBlack, 0, 1, 0);
    if ( t_trk->conBlock && t_trk->conBlock->occupied )
    	DrawPoly(d, switchmotorPolyLk_CNT, pl, NULL, drawColorWhite, 0, 1, 0);
}

static struct {
	char name[STR_SHORT_SIZE];
	char unlocked[STR_LONG_SIZE];
	char locked[STR_LONG_SIZE];
	long turnout;
} switchmotorData;

typedef enum { NM, ULK, LK, TO } switchmotorDesc_e;
static descData_t switchmotorDesc[] = {
/*NM */  { DESC_STRING, N_("Name"), &switchmotorData.name, sizeof(switchmotorData.name) },
/*ULK*/  { DESC_STRING, N_("Unlocked"), &switchmotorData.unlocked, sizeof(switchmotorData.unlocked) },
/*LK*/   { DESC_STRING, N_("Locked"), &switchmotorData.locked, sizeof(switchmotorData.locked) },
/*TO */  { DESC_LONG, N_("Turnout"), &switchmotorData.turnout },
	 { DESC_NULL } };

static void UpdateSwitchMotor (track_p trk, int inx, descData_p descUpd, BOOL_T needUndoStart )
{
	switchmotorData_p xx = GetswitchmotorData(trk);
	const char * thename, *theunlocked, *thelocked;
	char *newName, *newUnlocked, *newLocked, *newNormal=NULL, *newReverse=NULL, *newPointSense=NULL;
	unsigned int max_str;
	BOOL_T changed, nChanged, ulkChanged, lkChanged, norChanged, revChanged, psChanged;

	LOG( log_switchmotor, 1, ("*** UpdateSwitchMotor(): needUndoStart = %d\n",needUndoStart))

	if ( inx == -1 ) {
		nChanged = ulkChanged = lkChanged = norChanged = revChanged = psChanged = changed = FALSE;
		thename = wStringGetValue( (wString_p)switchmotorDesc[NM].control0 );
		if ( strcmp( thename, xx->name ) != 0 ) {
			nChanged = changed = TRUE;
			max_str = switchmotorDesc[NM].max_string;
			if (max_str && strlen(thename)>max_str-1) {
				newName = MyMalloc(max_str);
				newName[max_str-1] = '\0';
				strncat(newName,thename,max_str-1);
				NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED, _("Ok"), NULL, max_str-1);
			} else newName = MyStrdup(thename);
		}

		theunlocked = wStringGetValue( (wString_p)switchmotorDesc[ULK].control0 );
		if ( strcmp( theunlocked, xx->prodLockEvt->eId[0].script ) != 0 ) {
			ulkChanged = changed = TRUE;
			max_str = switchmotorDesc[ULK].max_string;
			if ( GetLayoutOpenLCBmode() && ! VerifyAsciiEvent( theunlocked ) ) {
				NoticeMessage( MSG_LCC_FORMAT_ERROR, _("Ok"), NULL);
			} else if (max_str && strlen(theunlocked)>max_str) {
				newUnlocked = MyMalloc(max_str);
				newUnlocked[max_str-1] = '\0';
				strncat(newUnlocked, theunlocked, max_str-1);
				NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED, _("Ok"), NULL, max_str-1);
			} else newUnlocked = MyStrdup(theunlocked);
		}

		thelocked = wStringGetValue( (wString_p)switchmotorDesc[LK].control0 );
		if ( strcmp( thelocked, xx->prodLockEvt->eId[1].script ) != 0 ) {
			lkChanged = changed = TRUE;
			max_str = switchmotorDesc[LK].max_string;
			if ( GetLayoutOpenLCBmode() && ! VerifyAsciiEvent( thelocked ) ) {
				NoticeMessage( MSG_LCC_FORMAT_ERROR, _("Ok"), NULL);
			} else if (max_str && strlen(thelocked)>max_str) {
				newLocked = MyMalloc(max_str);
				newLocked[max_str-1] = '\0';
				strncat(newLocked, thelocked, max_str-1);
				NoticeMessage2(0, MSG_ENTERED_STRING_TRUNCATED, _("Ok"), NULL, max_str-1);
			} else newLocked = MyStrdup(thelocked);
		}


		if ( ! changed ) return;
		xx->AutoGenerated = FALSE;
		if ( needUndoStart )
			UndoStart( _("Change Switch Motor"), "Change Switch Motor" );
		UndoModify( trk );
		if (nChanged) {
			MyFree(xx->name);
			xx->name = newName;
		}
		if (ulkChanged) {
			UpdateEvent( &xx->prodLockEvt->eId[0], NULL, newUnlocked );
		}
		if (lkChanged) {
			UpdateEvent( &xx->prodLockEvt->eId[1], NULL, newLocked );
		}
		ComputeSwitchMotorBoundingBox( trk );
		return;
	}
}

static DIST_T DistanceSwitchMotor (track_p t, coOrd * p )
{
	switchmotorData_p xx = GetswitchmotorData(t);
        if (xx->turnout == NULL) return 0;
	coOrd center,hi,lo;
	GetBoundingBox(t,&hi,&lo);
	center.x = (hi.x+lo.x)/2;
	center.y = (hi.y+lo.y)/2;
	DIST_T d = FindDistance(center,*p);
	*p = center;
	return d;
}

static void DescribeSwitchMotor (track_p trk, char * str, CSIZE_T len )
{
	switchmotorData_p xx = GetswitchmotorData(trk);
	long listLabelsOption = listLabels;

	LOG( log_switchmotor, 1, ("*** DescribeSwitchMotor(): trk is T%d\n",GetTrkIndex(trk)))
	FormatCompoundTitle( listLabelsOption, xx->name );
	if (message[0] == '\0')
		FormatCompoundTitle( listLabelsOption|LABEL_DESCR, xx->name );
	sprintf( str, _("(%d): Layer=%u %s"),
		GetTrkIndex(trk), GetTrkLayer(trk)+1, message );
	strncpy(switchmotorData.name,xx->name,STR_SHORT_SIZE-1);
	switchmotorData.name[STR_SHORT_SIZE-1] = '\0';
	if ( xx->prodLockEvt ) {
		strncpy( switchmotorData.unlocked, xx->prodLockEvt->eId[0].script, sizeof( switchmotorData.unlocked) );
		strncpy( switchmotorData.locked, xx->prodLockEvt->eId[1].script, sizeof( switchmotorData.locked) );
	}
        if (xx->turnout == NULL) switchmotorData.turnout = 0;
        else switchmotorData.turnout = GetTrkIndex(xx->turnout);
	switchmotorDesc[TO].mode = DESC_RO;
	switchmotorDesc[NM].mode =
	switchmotorDesc[ULK].mode =
	switchmotorDesc[LK].mode = DESC_NOREDRAW;
	DoDescribe(_("Switchmotor"), trk, switchmotorDesc, UpdateSwitchMotor );
}

static void switchmotorDebug (track_p trk)
{
	switchmotorData_p xx = GetswitchmotorData(trk);
	LOG( log_switchmotor, 1, ("*** switchmotorDebug(): SM%03d trk = %08x\n",GetTrkIndex(trk),trk))
	LOG( log_switchmotor, 1, ("*** switchmotorDebug(): Index = %d\n",GetTrkIndex(trk)))
	LOG( log_switchmotor, 1, ("*** switchmotorDebug(): name = \"%s\"\n",xx->name))
	if ( xx->prodLockEvt ) DisplayEvtBlk( "*** switchmotorDebug():", xx->prodLockEvt );
	else LOG( log_switchmotor, 1, ("*** switchmotorDebug(): prodLockEvt not set\n"))
	if ( xx->prodPosEvt ) DisplayEvtBlk( "*** switchmotorDebug():", xx->prodPosEvt );
	else LOG( log_switchmotor, 1, ("*** switchmotorDebug(): prodPosEvt not set\n"))
        LOG( log_switchmotor, 1, ("*** switchmotorDebug(): turnindx = %d\n",xx->turnindx))
        LOG( log_switchmotor, 1, ("*** switchmotorDebug(): AutoGenerated = %d\n",xx->AutoGenerated))
        if (xx->turnout != NULL) {       
             LOG( log_switchmotor, 1, ("*** switchmotorDebug(): turnout = T%d, %s\n",
                                       GetTrkIndex(xx->turnout), GetTrkTypeName(xx->turnout)))
        }
}

// called by FreeTrack
static void DeleteSwitchMotor ( track_p trk )
{

	track_p trk1; 
	switchmotorData_p xx1;

	if ( ! trk || IsTrackDeleted( trk ) ) return;
	if ( GetTrkType(trk) != T_SWITCHMOTOR ) return;

	LOG( log_switchmotor, 1,("*** DeleteSwitchMotor() SM%03d\n",GetTrkIndex(trk)))
	switchmotorData_p xx = GetswitchmotorData(trk);
	LOG( log_switchmotor, 1,("*** DeleteSwitchMotor(): xx = %p, xx->name = %s\n",
                xx,xx->name))
	MyFree(xx->name); xx->name = NULL;
	if ( xx->prodPosEvt ) {
		DisplayEvtBlk( "prodPosEvt", xx->prodPosEvt );
		DeleteEvtBlk( xx->prodPosEvt );
		xx->prodPosEvt = NULL;
	}
	if ( xx->prodLockEvt ) {
		DisplayEvtBlk( "prodLockEvt", xx->prodLockEvt );
		DeleteEvtBlk( xx->prodLockEvt );
		xx->prodLockEvt = NULL;
	}
//    	DeleteTrack (trk, FALSE);
	LOG( log_switchmotor, 1,("*** DeleteSwitchMotor() SM%03d, first T%d next T%d last T%d\n",
		GetTrkIndex(trk),GetTrkIndex(first_motor),GetTrkIndex(xx->next_motor),GetTrkIndex(last_motor)))
	if (first_motor == trk)
	    first_motor = xx->next_motor;
	trk1 = first_motor;
	while(trk1) {
		xx1 = GetswitchmotorData (trk1);
		if (xx1->next_motor == trk) {
			xx1->next_motor = xx->next_motor;
			break;
		}
		trk1 = xx1->next_motor;
	}
	if (trk == last_motor)
	    last_motor = trk1;
}

static void SmotorEditOk( void * action )
{
	track_p trk = switchmotorEditTrack;
	switchmotorData_p xx = GetswitchmotorData(trk);
	int ia;

	LOG( log_switchmotor, 1, ("*** SmotorEditOk: name '%s' script '%s' inx '%d'\n",
		smotorEditName, smotorEditScript, smotorEditIndex ))

	strcpy( (&(xx->turnoutPosList))[smotorEditIndex].posName, smotorEditName );
	strcpy( (&(xx->turnoutPosList))[smotorEditIndex].posScript, smotorEditScript );
	UpdateEvent( &xx->prodPosEvt->eId[smotorEditIndex], smotorEditName, smotorEditScript );

//	wListClear( smotorSelL );
	for (ia = 0; ia < xx->numPos; ia++) {
		snprintf(message,sizeof(message),"%s\t%s",(&(xx->turnoutPosList))[ia].posName,
			(&(xx->turnoutPosList))[ia].posScript);
	LOG( log_switchmotor, 1, ("*** SmotorEditOk: message '%s' ia '%d'\n",
		message, ia ))
//		wListAddValue( smotorSelL, message, NULL, NULL );
	}

	DisplayEvtBlk( "SmotorEditOk", xx->prodPosEvt);
	DoRedraw();
	wHide( smotorEditW );
}

static void SmotorEdit( void * action )
{
	wIndex_t selcnt = wListGetSelectedCount( smotorSelL );
	wIndex_t inx, cnt;
	track_p trk = switchmotorEditTrack;
	switchmotorData_p xx = GetswitchmotorData(trk);

	if ( selcnt != 1) return;
	cnt = wListGetCount( smotorSelL );
	for ( inx=0;
	inx<cnt && wListGetItemSelected( smotorSelL, inx ) != TRUE;
	            inx++ );
	if ( inx >= cnt ) return;

	if (inx < 0) {
		smotorEditName[0] = '\0';
		smotorEditScript[0] = '\0';
	} else {
		strncpy(smotorEditName,xx->prodPosEvt->eId[inx].name,STR_SHORT_SIZE);
		strncpy(smotorEditScript,xx->prodPosEvt->eId[inx].script,STR_LONG_SIZE);
	}
	smotorEditIndex = inx;
	LOG( log_switchmotor, 1, ("*** SmotorEdit: name '%s' script '%s' inx '%d'\n",
		smotorEditName, smotorEditScript, smotorEditIndex ))
	if ( !smotorEditW ) {
		ParamRegister( &smotorEditPG );
		smotorEditW = ParamCreateDialog (&smotorEditPG,
			MakeWindowTitle(_("Edit position")),
			_("Ok"), SmotorEditOk,
	 		wHide, TRUE, NULL, F_BLOCK, NULL);
	}
	ParamLoadControls( &smotorEditPG );
	wShow( smotorEditW );
}

static BOOL_T WriteSwitchMotor ( track_p t, FILE * f )
{
	BOOL_T rc = TRUE;
	switchmotorData_p xx = GetswitchmotorData(t);
	char *switchMotorName = MyStrdup(xx->name);
	int ip;
    
#ifdef UTFCONVERT
	switchMotorName = Convert2UTF8(switchMotorName);
#endif // UTFCONVERT
	xx->AutoGenerated = FALSE;

	if (xx->turnout == NULL)
		return FALSE;
	rc &= fprintf(f, "SWITCHMOTOR %d %d \"%s\"\n",
		GetTrkIndex(t), GetTrkIndex(xx->turnout), switchMotorName)>0;
	if ( xx->prodLockEvt ) {
        	rc &= fprintf(f, "\tSCRIPT \"UNLOCKED\" \"%s\"\n",
				xx->prodLockEvt->eId[0].script )>0;
        	rc &= fprintf(f, "\tSCRIPT \"LOCKED\" \"%s\"\n",
				xx->prodLockEvt->eId[1].script )>0;
	}
	if ( xx->prodPosEvt ) {
		for ( ip = 0; ip < xx->numPos; ip++ ) {
			rc &= fprintf(f, "\tSCRIPT \"%s\" \"%s\"\n",
				xx->prodPosEvt->eId[ip].name, xx->prodPosEvt->eId[ip].script )>0;
		}
	}

        rc &= fprintf( f, "\t%s\n", END_SWITCHMOTOR )>0;
	MyFree(switchMotorName);
	return rc;
}

static BOOL_T ReadSwitchMotor ( char * line )
{
	TRKINX_T trkindex;
	wIndex_t index, ia;
	track_p sm_trk,last_trk;
	switchmotorData_p xx,xx1;
	char *name;
	char * cp = NULL;
	char *bufName, *bufScript;

	switchmotorUnlocked[0] = '\0';
	switchmotorLocked[0] = '\0';

	LOG( log_switchmotor, 1, ("*** ReadSwitchMotor: line is '%s'\n",line))
	if (!GetArgs(line+12,"ddq",&index,&trkindex,&name)) {
		return FALSE;
	}
#ifdef UTFCONVERT
	ConvertUTF8ToSystem(name);
#endif // UTFCONVERT
	DYNARR_RESET( turnoutPos_p, turnoutPos_da );
        while ( (cp = GetNextLine()) != NULL ) {
#if 0
		LOG( log_switchmotor, 1, ("*** ReadSwitchMotor: line is '%s'\n",cp))
#endif
                while (isspace((unsigned char)*cp)) cp++;
                if ( strncmp( cp, "END", 3 ) == 0 ) {
                        break;
                }
                if ( *cp == '\n' || *cp == '#' ) {
                        continue;
		}
                if ( strncmp( cp, "SCRIPT", 6 ) == 0 ) {
                        if ( ! GetArgs( cp+7, "qq", &bufName, &bufScript ) ) return FALSE;
                        if ( strncmp( bufName, "UNLOCKED", 8 ) == 0 ) {
                                strncpy( switchmotorUnlocked, bufScript, sizeof(switchmotorUnlocked) );
                                switchmotorUnlocked[sizeof(switchmotorUnlocked)-1] = 0;
#if 0
                                LOG( log_switchmotor, 1, ("*** ReadSwitchMotor: unlocked %s\n",
                                        switchmotorUnlocked))
#endif
                        } else
                        if ( strncmp( bufName, "LOCKED", 6 ) == 0 ) {
                                strncpy( switchmotorLocked, bufScript, sizeof(switchmotorLocked) );
                                switchmotorLocked[sizeof(switchmotorLocked)-1] = 0;
#if 0
                                LOG( log_switchmotor, 1, ("*** ReadSwitchMotor: locked %s\n",
                                        switchmotorLocked))
#endif
                        } else {
				DYNARR_APPEND( turnoutPos_p *, turnoutPos_da, 10 );
				turnoutPos(turnoutPos_da.cnt-1).posName = bufName;
				turnoutPos(turnoutPos_da.cnt-1).posScript = bufScript;
			}
		}
	}

	UndoStart( _("Create Switch Motor"), "Create Switch Motor" );
	sm_trk = NewTrack(index, T_SWITCHMOTOR, 0, sizeof(switchmotorData_t)+
			(sizeof(turnoutPos_t)*(turnoutPos_da.cnt-1))+1);
	xx = GetswitchmotorData( sm_trk );
	xx->name = name;
	xx->turnout = FindTrack(trkindex);
	xx->numPos = turnoutPos_da.cnt;

	if ( ! xx->prodLockEvt && switchmotorUnlocked[0] != '\0' && switchmotorLocked[0] != '\0' ) {
		xx->prodLockEvt = NewSwmotorLockEvt( sm_trk, name );
		UpdateEvent( &xx->prodLockEvt->eId[0], NULL, switchmotorUnlocked );
		UpdateEvent( &xx->prodLockEvt->eId[1], NULL, switchmotorLocked );
	}

	if ( ! xx->prodPosEvt ) {
		xx->prodPosEvt = NewSwmotorPosEvt( sm_trk, xx->turnout, xx->name, xx->numPos );
		for (ia = 0; ia < xx->numPos; ia++) {
			(&(xx->turnoutPosList))[ia].posName = turnoutPos(ia).posName;
			(&(xx->turnoutPosList))[ia].posScript = turnoutPos(ia).posScript;
			if ( xx->prodPosEvt ) {
				UpdateEvent( &xx->prodPosEvt->eId[ia], turnoutPos(ia).posName, turnoutPos(ia).posScript );
			}
		}
	}

	xx->turnindx = trkindex;
	xx->AutoGenerated = FALSE;
	if (last_motor) {
    		last_trk = last_motor;
    		xx1 = GetswitchmotorData(last_trk);
		xx1->next_motor = sm_trk;
	} else first_motor = sm_trk;
	xx->next_motor = NULL;
	last_motor = sm_trk;

	UndoEnd();
	ComputeSwitchMotorBoundingBox(sm_trk);

        LOG( log_switchmotor, 1,("*** ReadSwitchMotor(): sm_trk = %p (%d), xx = %p\n",sm_trk,GetTrkIndex(sm_trk),xx))
        LOG( log_switchmotor, 1,("*** ReadSwitchMotor(): name = %s\n", name))

        switchmotorDebug(sm_trk);
	DisplayEvtBlk( "ReadSwitchMotor lock:", xx->prodLockEvt );
	DisplayEvtBlk( "ReadSwitchMotor  pos:", xx->prodPosEvt );
	return TRUE;
}

EXPORT BOOL_T ResolveSwitchmotorTurnout ( track_p trk )
{
#if 0
    LOG( log_switchmotor, 1,("*** ResolveSwitchmotorTurnout(T%d)\n",GetTrkIndex(trk)))
#endif
    switchmotorData_p xx;
    track_p t_trk;
    if (GetTrkType(trk) != T_SWITCHMOTOR) return TRUE;
    LOG( log_switchmotor, 1,("*** ResolveSwitchmotorTurnout(T%d)\n",GetTrkIndex(trk)))
    xx = GetswitchmotorData(trk);
    LOG( log_switchmotor, 1, ("*** ResolveSwitchmotorTurnout(xx T%d)\n",GetTrkIndex(trk)))
    t_trk = FindTrack(xx->turnindx);
    if (t_trk == NULL) {
        NoticeMessage( _("ResolveSwitchmotor: Turnout T%d: T%d doesn't exist"), _("Continue"), NULL, GetTrkIndex(trk), xx->turnindx );
    }
    xx->turnout = t_trk;
    ComputeSwitchMotorBoundingBox(trk);
    LOG( log_switchmotor, 1,("*** ResolveSwitchmotorTurnout(): t_trk = (%d) %p\n",xx->turnindx,t_trk))
    return TRUE;
}

EXPORT void AddMissingSwitchMotor( void )
{
    track_p trk, sm_trk;
    switchmotorData_p xx, xx1;
    EPINX_T ep;

    LOG( log_switchmotor, 1, ("*** AddMissingSwitchMotor() -- enter \n"))

    // Loop through all turnout segs
    // Create switchmotor for them.
    TRK_ITERATE(trk) {
	// seg already in a block
	if ( ! IsTrack(trk) || GetTrkType( trk ) != T_TURNOUT ||
		GetTrkEndPtCnt(trk) <= 2 ) continue;

	if ( FindSwitchMotor (trk) ) continue;

	for (ep = 0; ep < GetTrkEndPtCnt(trk); ep++) {
		if ( trk->endPt[ep].onRoute ) break;
	}
	if ( ep == GetTrkEndPtCnt(trk) ) continue;

#if 0
	LOG( log_switchmotor, 1, ("*** AddMissingSwitchMotor(T%03d)\n", GetTrkIndex(trk )))
#endif

//	wDrawDelayUpdate( mainD.d, TRUE );
	UndoStart( _("Create Switch Motor"), "Create Switch Motor" );

	sm_trk = NewTrack(0, T_SWITCHMOTOR, 0, sizeof(switchmotorData_t) +
		       (sizeof(turnoutPos_t) * ( GetNumPos( trk ) - 1)) + 1);

	sprintf(switchmotorName,"SM%03d",GetTrkIndex(trk));
	switchmotorTurnout = trk;

	initSwitchMotorData( sm_trk );

	xx = GetswitchmotorData( sm_trk );
	xx->AutoGenerated = TRUE;

	UndoEnd();
//	wHide( switchmotorW );
	ComputeSwitchMotorBoundingBox(sm_trk);
	DrawNewTrack(sm_trk);
    }
//    wHide( switchmotorW );
//    MainRedraw();
    LOG( log_switchmotor, 1, ("*** AddMissingSwitchMotor() -- exit \n"))
}

// Delete all auto generated switch motors
EXPORT void DeleteAllSwitchMotors( void )
{
	track_p swMot;
	switchmotorData_p xx;

	LOG( log_switchmotor, 1, ("*** DeleteAllSwitchMotors() -- enter\n"))

	// Loop through all track segs
	TRK_ITERATE(swMot) {
		if ( ! IsTrackDeleted(swMot) && GetTrkType(swMot) == T_SWITCHMOTOR ) {
#if 0
			LOG( log_switchmotor, 1, ("*** DeleteAllSwitchMotors() next switch motor SM%03d\n",
				GetTrkIndex(swMot)))
#endif
			xx = GetswitchmotorData(swMot);
			if ( ! xx->AutoGenerated ) continue;

			DeleteSwitchMotor( swMot );
		}
	}
}

static void MoveSwitchMotor (track_p trk, coOrd orig ) {}
static void RotateSwitchMotor (track_p trk, coOrd orig, ANGLE_T angle ) {}
static void RescaleSwitchMotor (track_p trk, FLOAT_T ratio ) {}


static trackCmd_t switchmotorCmds = {
	"SWITCHMOTOR",
	DrawSwitchMotor,
	DistanceSwitchMotor,
	DescribeSwitchMotor,
	DeleteSwitchMotor,
	WriteSwitchMotor,
	ReadSwitchMotor,
	MoveSwitchMotor,
	RotateSwitchMotor,
	RescaleSwitchMotor,
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
	NULL, /* flip */
	NULL, /* drawPositionIndicator */
	NULL, /* advancePositionIndicator */
	NULL, /* checkTraverse */
	NULL, /* makeParallel */
	NULL  /* drawDesc */
};

static track_p FindSwitchMotor (track_p trk)
{
	track_p a_trk;
	switchmotorData_p xx;

	a_trk = first_motor;
	while (a_trk) {
		xx =  GetswitchmotorData(a_trk);
		if (!IsTrackDeleted(a_trk)) {
			if (xx->turnout == trk) return a_trk;
		}
		a_trk = xx->next_motor;
	}
	return NULL;
}

static track_p FindSwitchMotorByName (char *name)
{
	track_p a_trk;
	switchmotorData_p xx;

	a_trk = first_motor;
	while (a_trk) {
		xx =  GetswitchmotorData(a_trk);
		if (strcmp(xx->name, name) == 0) {
			return a_trk;
		}
		a_trk = xx->next_motor;
	}
	return NULL;
}

// sm_trk is the switch motor
static void initSwitchMotorData( track_p sm_trk )
{
	track_p trk1;
	switchmotorData_p xx, xx1;
	int ia;

	xx = GetswitchmotorData( sm_trk );
	xx->name = MyStrdup(switchmotorName);
	xx->turnout = switchmotorTurnout;
	xx->prodLockEvt = NewSwmotorLockEvt( sm_trk, xx->name );
	xx->prodPosEvt = NewSwmotorPosEvt( sm_trk, xx->turnout, xx->name, 0 );
	xx->numPos = xx->prodPosEvt->numEvt;
	for (ia = 0; ia < xx->numPos; ia++) {
		(&(xx->turnoutPosList))[ia].posName = xx->prodPosEvt->eId[ia].name;
		(&(xx->turnoutPosList))[ia].posScript = MyStrdup( xx->prodPosEvt->eId[ia].script );
	}
	xx->turnindx = GetTrkIndex( xx->turnout );
	switchmotorTonum = xx->turnindx;
	xx->AutoGenerated = FALSE;
	trk1 = last_motor;
	if (trk1) {
		xx1 = GetswitchmotorData( trk1 );
		xx1->next_motor = sm_trk;
	} else first_motor = sm_trk;
	xx->next_motor = NULL;
	last_motor = sm_trk;
#if 0
	LOG( log_switchmotor, 1,("*** initSwitchMotorData(): sm_trk = %p SM%03d, xx = %p\n",
			sm_trk, GetTrkIndex(sm_trk), xx))
#endif
        switchmotorDebug(sm_trk);
}

static void SwitchMotorOk ( void * junk )
{
	track_p sm_trk;

#if 0
	LOG( log_switchmotor, 1, ("*** SwitchMotorOk()\n"))
#endif
	ParamUpdate (&switchmotorPG );
	if ( switchmotorName[0]==0 ) {
		NoticeMessage( _("Switch motor must have a name!"), _("Ok"), NULL);
		return;
	}
	if ( FindSwitchMotorByName (switchmotorName) ) {
		NoticeMessage( _("Switch motor must have a unique name!"), _("Ok"), NULL);
		return;
	}
	wDrawDelayUpdate( mainD.d, TRUE );
	wHide( switchmotorW );
	ComputeSwitchMotorBoundingBox(sm_trk);
	DrawNewTrack(sm_trk);

}

static void NewSwitchMotorDialog(track_p trk)
{
	track_p sm_trk;

	LOG( log_switchmotor, 1, ("*** NewSwitchMotorDialog( T%d) type %d\n",
			GetTrkIndex(trk), GetTrkType(trk)))

	if ((sm_trk = FindSwitchMotor (trk))!=NULL) {
		EditSwitchMotor(sm_trk);
		return;
	}

	SetupControlElements();

	UndoStart( _("Create Switch Motor"), "Create Switch Motor" );
	/* Create a switchmotor object */
	sm_trk = NewTrack(0, T_SWITCHMOTOR, 0, sizeof(switchmotorData_t) +
		       (sizeof(turnoutPos_t) * ( GetNumPos( trk ) - 1)) + 1);

	sprintf(switchmotorName,"SM%03d",GetTrkIndex(trk));
	switchmotorTurnout = trk;

	initSwitchMotorData( sm_trk );

	UndoEnd();
	ComputeSwitchMotorBoundingBox(sm_trk);
	DrawNewTrack(sm_trk);

	EditSwitchMotor(sm_trk);
}

static STATUS_T CmdSwitchMotorCreate( wAction_t action, coOrd pos )
{
	track_p trk;

#if 0
	LOG( log_switchmotor, 1, ("*** CmdSwitchMotorCreate(%08x,{%f,%f})\n",action,pos.x,pos.y))
#endif
	switch (action & 0xFF) {
	case C_START:
		InfoMessage( _("Select a turnout") );
		SetAllTrackSelect( FALSE );
		return C_CONTINUE;
	case C_DOWN:
		if ((trk = OnTrack(&pos, TRUE, TRUE )) == NULL ||
				GetTrkType( trk ) != T_TURNOUT || GetTrkEndPtCnt( trk ) <= 2) {
			ErrorMessage( _("Not a turnout!") );
			NoticeMessage( _("Please select a turnout"), _("Ok"), NULL);
			return C_CONTINUE;
		}
		NewSwitchMotorDialog(trk);
		return C_CONTINUE;
	case C_REDRAW:
		return C_CONTINUE;
	case C_CANCEL:
		return C_TERMINATE;
	default:
		return C_CONTINUE;
	}
}


static void SwitchMotorEditOk ( void * junk )
{
    switchmotorData_p xx;
    track_p trk;

#if 0
    LOG( log_switchmotor, 1, ("*** SwitchMotorEditOk()\n"))
#endif
    ParamUpdate (&switchmotorEditPG );
    if ( switchmotorEditName[0]==0 ) {
        NoticeMessage( _("Switch motor must have a name!") , _("Ok"), NULL);
        return;
    }
    wDrawDelayUpdate( mainD.d, TRUE );
    trk = switchmotorEditTrack;
    xx = GetswitchmotorData( trk );
    xx->name = MyStrdup(switchmotorEditName);
    UpdateOwnerName( xx->prodLockEvt, switchmotorEditName );
    UpdateEvent( &xx->prodLockEvt->eId[0], NULL, switchmotorEditUnlocked );
    UpdateEvent( &xx->prodLockEvt->eId[1], NULL, switchmotorEditLocked );
    xx->AutoGenerated = FALSE;

    switchmotorDebug(trk);

    wHide( switchmotorEditW );
}

static void SwitchMotorEditDlgUpdate (paramGroup_p pg, int inx, void *valueP )
{
	wIndex_t selcnt = wListGetSelectedCount( smotorSelL );

	if ( inx != I_SWITCHMOTORLIST ) return;
	ParamControlActive( &switchmotorEditPG, I_SWITCHMOTOREDIT, selcnt>0 );
}

static void EditSwitchMotor (track_p trk)
{
    int ia;
    switchmotorData_p xx = GetswitchmotorData(trk);

    strncpy(switchmotorEditName,xx->name,STR_SHORT_SIZE);
    strncpy( switchmotorEditUnlocked, xx->prodLockEvt->eId[0].script, sizeof(switchmotorEditUnlocked) );
    strncpy( switchmotorEditLocked, xx->prodLockEvt->eId[1].script, sizeof(switchmotorEditLocked) );

    if (xx->turnout == NULL) switchmotorEditTonum = 0;
    else switchmotorEditTonum = GetTrkIndex(xx->turnout);
    switchmotorEditTrack = trk;
    if ( !switchmotorEditW ) {
        ParamRegister( &switchmotorEditPG );
        switchmotorEditW = ParamCreateDialog (&switchmotorEditPG, 
                                              MakeWindowTitle(_("Edit switch motor")), 
                                              _("Ok"), SwitchMotorEditOk, 
                                              wHide, TRUE, NULL, F_RESIZE|F_RECALLSIZE|F_BLOCK, 
                                              SwitchMotorEditDlgUpdate );
    }
    wListClear( smotorSelL );
    for (ia = 0; ia < xx->numPos; ia++) {
	    snprintf(message,sizeof(message),"%s\t%s",(&(xx->turnoutPosList))[ia].posName,
			                    (&(xx->turnoutPosList))[ia].posScript);
	    wListAddValue( smotorSelL, message, NULL, NULL );
    }
    ParamLoadControls( &switchmotorEditPG );
    ParamControlActive( &switchmotorEditPG, I_SWITCHMOTOREDIT, FALSE );
    sprintf( message, _("Edit switch motor %d"), GetTrkIndex(trk) );
    wWinSetTitle( switchmotorEditW, message );
    wShow (switchmotorEditW);
}

static coOrd swmhiliteOrig, swmhiliteSize;
static POS_T swmhiliteBorder;
static wDrawColor swmhiliteColor = 0;
static void DrawSWMotorTrackHilite( void )
{
	wDrawPix_t x, y, w, h;
	if (swmhiliteColor==0)
		swmhiliteColor = wDrawColorGray(87);
	w = ((swmhiliteSize.x/mainD.scale)*mainD.dpi+0.5);
	h = ((swmhiliteSize.y/mainD.scale)*mainD.dpi+0.5);
	mainD.CoOrd2Pix(&mainD,swmhiliteOrig,&x,&y);
//	wDrawFilledRectangle( mainD.d, x, y, w, h, swmhiliteColor, wDrawOptTemp|wDrawOptTransparent );
}

static int SwitchmotorMgmProc ( int cmd, void * data )
{
    track_p trk = (track_p) data;
    switchmotorData_p xx = GetswitchmotorData(trk);
    char *lk[] = { "UNLOCKED", "LOCKED" };
    /*char msg[STR_SIZE];*/
#if 0
    LOG( log_switchmotor, 1,("*** SwitchmotorMgmProc()  -- enter -- cmd %d -- T%d %s\n",
		cmd,GetTrkIndex(trk),GetTrkTypeName(trk)))
#endif

    switch ( cmd ) {
    case CONTMGM_CAN_EDIT: /* 1 */
        return TRUE;
        break;
    case CONTMGM_DO_EDIT: /* 2 */
        EditSwitchMotor (trk);
        /*inDescribeCmd = TRUE;*/
        /*DescribeTrack (trk, msg, sizeof msg );*/
        /*InfoMessage( msg );*/
        return TRUE;
        break;
    case CONTMGM_CAN_DELETE: /* 3 */
        return TRUE;
        break;
    case CONTMGM_DO_DELETE: /* 4 */
        DeleteTrack (trk, FALSE);
        return TRUE;
        break;
    case CONTMGM_DO_HILIGHT: /* 6 */
        if (xx->turnout != NULL && !xx->IsHilite) {
            swmhiliteBorder = mainD.scale*0.1;
            if ( swmhiliteBorder < trackGauge ) swmhiliteBorder = trackGauge;
            GetBoundingBox( xx->turnout, &swmhiliteSize, &swmhiliteOrig );
            swmhiliteOrig.x -= swmhiliteBorder;
            swmhiliteOrig.y -= swmhiliteBorder;
            swmhiliteSize.x -= swmhiliteOrig.x-swmhiliteBorder;
            swmhiliteSize.y -= swmhiliteOrig.y-swmhiliteBorder;
            DrawSWMotorTrackHilite();
            xx->IsHilite = TRUE;
        }
        break;
    case CONTMGM_UN_HILIGHT:  /* 7 */
        if (xx->turnout != NULL && xx->IsHilite) {
            swmhiliteBorder = mainD.scale*0.1;
            if ( swmhiliteBorder < trackGauge ) swmhiliteBorder = trackGauge;
            GetBoundingBox( xx->turnout, &swmhiliteSize, &swmhiliteOrig );
            swmhiliteOrig.x -= swmhiliteBorder;
            swmhiliteOrig.y -= swmhiliteBorder;
            swmhiliteSize.x -= swmhiliteOrig.x-swmhiliteBorder;
            swmhiliteSize.y -= swmhiliteOrig.y-swmhiliteBorder;
            DrawSWMotorTrackHilite();
            xx->IsHilite = FALSE;
        }
        break;
    case CONTMGM_GET_TITLE: /* 5 */
        if (xx->turnout == NULL) {
            sprintf( message, "\t%s\t%d", xx->name, 0);
        } else {
            sprintf( message, "\t%s\t\t%s  Pos %d", xx->name,
			    lk[xx->prodLockEvt->state], xx->prodPosEvt->state);
        }
        break;
    }
    return FALSE;
}

//#include "bitmaps/switchmotor.xpm"

//#include "bitmaps/switchmnew.xpm"
//#include "bitmaps/switchmedit.xpm"
//#include "bitmaps/switchmdel.xpm"
#include "bitmaps/switchm.xpm"

EXPORT void SwitchmotorMgmLoad( void )
{
    track_p trk;
    static wIcon_p switchmI = NULL;
#if 0
    LOG( log_switchmotor, 1,("*** SwitchmotorMgmLoad()  -- enter --\n"))
#endif
    
    if ( switchmI == NULL)
        switchmI = wIconCreatePixMap( switchm_xpm );
    
    TRK_ITERATE(trk) {
        if (GetTrkType(trk) != T_SWITCHMOTOR) continue;
        ContMgmLoad( switchmI, SwitchmotorMgmProc, (void *)trk );
    }
}

EXPORT void InitCmdSwitchMotor( wMenu_p menu )
{
#if 0
	LOG( log_switchmotor, 1,("*** InitCmdSwitchMotor()  -- enter --\n"))
#endif
	switchmotorName[0] = '\0';
	switchmotorUnlocked[0] = '\0';
	switchmotorLocked[0] = '\0';
        AddMenuButton( menu, CmdSwitchMotorCreate, "cmdSwitchMotorCreate", 
                       _("Switch Motor"), wIconCreatePixMap( switchm_xpm ),
                       LEVEL0_50, IC_STICKY|IC_POPUP2, ACCL_SWITCHMOTOR1, 
                       NULL );
	ParamRegister( &switchmotorPG );
}
#if 0
EXPORT void CheckDeleteSwitchmotor(track_p t)
{
    track_p sm;
    switchmotorData_p xx;
    LOG( log_switchmotor, 1,("*** CheckDeleteSwitchMotor() SM%03d\n",GetTrkIndex(t)))
    if (GetTrkType( t ) != T_TURNOUT && GetTrkType( t ) != T_SWITCHMOTOR ) return;   // SMs only on turnouts
    
    while ((sm = FindSwitchMotor( t ))) {	                 //Cope with multiple motors for one Turnout!
    LOG( log_switchmotor, 1,("*** CheckDeleteSwitchMotor() SM%03d T%03d\n",GetTrkIndex(sm),GetTrkIndex(t)))
    	xx = GetswitchmotorData (sm);
    	InfoMessage(_("Deleting Switch Motor %s"),xx->name);
	CheckDeleteSwitchMotor( sm );
    	DeleteTrack (sm, FALSE);
    };
}
#endif


EXPORT void InitTrkSwitchMotor( void )
{
	T_SWITCHMOTOR = InitObject ( &switchmotorCmds );
	log_switchmotor = LogFindIndex ( "switchmotor" );
}


