/** \file ctrain.c
 * Functions related to running trains
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

#ifndef WINDOWS
#include <errno.h>
#endif
#include <ctype.h>
#include <math.h>
#include <string.h>

#define PRIVATE_EXTRADATA

#include "compound.h"
#include "ctrain.h"
#include "cundo.h"
#include "custom.h"
#include "fileio.h"
#include "i18n.h"
#include "layout.h"
#include "messages.h"
#include "param.h"
#include "track.h"
#include "trackx.h"
#include "utility.h"

long programMode;
long maxCouplingSpeed = 100;
long hideTrainsInTunnels;

extern int doDrawTurnoutPosition;
extern void NextTurnoutPosition(track_p);

static TRKTYP_T T_CAR = -1;

typedef enum { ST_NotOnTrack, ST_StopManual, ST_EndOfTrack, ST_OpenTurnout, ST_NoRoom, ST_Crashed } trainStatus_e;

struct extraData {
    traverseTrack_t trvTrk;
    long            state;
    carItem_p       item;
    double          speed;
    BOOL_T          direction;
    BOOL_T          autoReverse;
    trainStatus_e   status;
    DIST_T          distance;
    coOrd           couplerPos[2];
    LAYER_T         trkLayer;
};
#define NOTALAYER						(127)

#define CAR_STATE_IGNORED				(1L<<17)
#define CAR_STATE_PROCESSED				(1L<<18)
#define CAR_STATE_LOCOISMASTER			(1L<<19)
#define CAR_STATE_ONHIDENTRACK			(1L<<20)

#define COUPLERCONNECTIONANGLE			45.0
#define CRASHSPEEDDECAY					5

#define IsOnTrack( XX )					((XX)->trvTrk.trk!=NULL)
#define IsIgnored( XX )					(((XX)->state&CAR_STATE_IGNORED)!=0)
#define SetIgnored( XX )				(XX)->state |= CAR_STATE_IGNORED
#define ClrIgnored( XX )				(XX)->state &= ~CAR_STATE_IGNORED

#define IsLocoMaster( XX )				CarItemIsLocoMaster((XX)->item)
#define SetLocoMaster( XX )				CarItemSetLocoMaster((XX)->item,TRUE)
#define ClrLocoMaster( XX )				CarItemSetLocoMaster((XX)->item,FALSE)
#define IsProcessed( XX )				(((XX)->state&CAR_STATE_PROCESSED)!=0)
#define SetProcessed( XX )				(XX)->state |= CAR_STATE_PROCESSED
#define ClrProcessed( XX )				(XX)->state &= ~CAR_STATE_PROCESSED

static wButton_p newcarB;

static void ControllerDialogSyncAll(void);
static STATUS_T CmdTrain(wAction_t, coOrd);
static wMenu_p trainPopupM;
static wMenuPush_p trainPopupMI[8];
static track_p followTrain;
static coOrd followCenter;
static BOOL_T trainsTimeoutPending;
static enum { TRAINS_STOP, TRAINS_RUN, TRAINS_IDLE, TRAINS_PAUSE } trainsState;
static wIcon_p stopI, goI;
static void RestartTrains(void);
static void DrawAllCars(void);
static void UncoupleCars(track_p, track_p);
static void TrainTimeEndPause(void);
static void TrainTimeStartPause(void);

static int log_trainMove;
static int log_trainPlayback;

static void PlaceCar(track_p);


#define WALK_CARS_START( CAR, XX, DIR ) \
	while (1) { \
		(XX) = GetTrkExtraData(CAR);\
		{ \

#define WALK_CARS_END( CAR, XX, DIR ) \
		} \
		{ \
			track_p walk_cars_temp1; \
			if ( (walk_cars_temp1=GetTrkEndTrk(CAR,DIR)) == NULL ) break; \
			(DIR)=(GetTrkEndTrk(walk_cars_temp1,0)==(CAR)?1:0); \
			(CAR)=walk_cars_temp1; \
		} \
	}


/*
 *  Generic Commands
 */

void CarGetPos(
    track_p car,
    coOrd * posR,
    ANGLE_T * angleR)
{
    struct extraData * xx = GetTrkExtraData(car);

    if (GetTrkType(car) != T_CAR) {
        AbortProg("getCarPos");
    }

    *posR = xx->trvTrk.pos;
    *angleR = xx->trvTrk.angle;
}

void CarSetVisible(
    track_p car)
{
    struct extraData * xx;
    int dir;
    dir = 0;
    WALK_CARS_START(car, xx, dir)

    if (GetTrkType(car) != T_CAR) {
        AbortProg("carSetVisible");
    }

    WALK_CARS_END(car, xx, dir)
    dir = 1-dir;
    WALK_CARS_START(car, xx, dir) {
        xx->state &= ~(CAR_STATE_ONHIDENTRACK);
        xx->trkLayer = NOTALAYER;
    }
    WALK_CARS_END(car, xx, dir)
}


static struct {
    long index;
    coOrd pos;
    ANGLE_T angle;
    DIST_T length;
    DIST_T width;
    char desc[STR_SIZE];
    char number[STR_SIZE];
} carData;
typedef enum { IT, PN, AN, LN, WD, DE, NM } carDesc_e;
static descData_t carDesc[] = {
    /*IT*/	{ DESC_LONG, N_("Index"), &carData.index },
    /*PN*/	{ DESC_POS, N_("Position"), &carData.pos },
    /*AN*/	{ DESC_ANGLE, N_("Angle"), &carData.angle },
    /*LN*/	{ DESC_DIM, N_("Length"), &carData.length },
    /*WD*/	{ DESC_DIM, N_("Width"), &carData.width },
    /*DE*/	{ DESC_STRING, N_("Description"), &carData.desc },
    /*NM*/	{ DESC_STRING, N_("Rep Marks"), &carData.number },
    { DESC_NULL }
};

static void UpdateCar(
    track_p trk,
    int inx,
    descData_p descUpd,
    BOOL_T needUndoStart)
{
    if (inx == -1) {
        BOOL_T titleChanged;
        const char * cp;
        titleChanged = FALSE;
        cp = wStringGetValue((wString_p)carDesc[NM].control0);

        if (cp && strcmp(carData.number, cp) != 0) {
            titleChanged = TRUE;
            strcpy(carData.number, cp);
        }

        if (!titleChanged) {
            return;
        }

        if (needUndoStart) {
            UndoStart(_("Change Track"), "Change Track");
        }

        UndoModify(trk);
        UndrawNewTrack(trk);
        DrawNewTrack(trk);
        return;
    }

    UndrawNewTrack(trk);

    switch (inx) {
    case NM:
        break;

    default:
        break;
    }

    DrawNewTrack(trk);
}


static void DescribeCar(
    track_p trk,
    char * str,
    CSIZE_T len)
{
    struct extraData *xx = GetTrkExtraData(trk);
    char * cp;
    coOrd size;
    CarItemSize(xx->item, &size);
    carData.length = size.x;
    carData.width = size.y;
    cp = CarItemDescribe(xx->item, 0, &carData.index);
    strcpy(carData.number, CarItemNumber(xx->item));
    strncpy(str, cp, len);
    carData.pos = xx->trvTrk.pos;
    carData.angle = xx->trvTrk.angle;
    cp = CarItemDescribe(xx->item, -1, NULL);
    strncpy(carData.desc, cp, sizeof carData.desc);
    carDesc[IT].mode =
        carDesc[PN].mode =
            carDesc[AN].mode =
                carDesc[LN].mode =
                    carDesc[WD].mode = DESC_RO;
    carDesc[DE].mode =
        carDesc[NM].mode = DESC_RO;
    DoDescribe(_("Car"), trk, carDesc, UpdateCar);
}


void FlipTraverseTrack(
    traverseTrack_p trvTrk)
{
    trvTrk->angle = NormalizeAngle(trvTrk->angle + 180.0);

    if (trvTrk->length > 0) {
        trvTrk->dist = trvTrk->length - trvTrk->dist;
    }
}


BOOL_T TraverseTrack2(
    traverseTrack_p trvTrk0,
    DIST_T dist0)
{
    traverseTrack_t trvTrk = *trvTrk0;
    DIST_T			dist = dist0;

    if (dist0 < 0) {
        dist = -dist;
        FlipTraverseTrack(&trvTrk);
    }

    if (trvTrk.trk==NULL ||
            (!TraverseTrack(&trvTrk,&dist)) ||
            trvTrk.trk==NULL ||
            dist!=0.0) {
        Translate(&trvTrk.pos, trvTrk.pos, trvTrk.angle, dist);
    }

    if (dist0 < 0) {
        FlipTraverseTrack(&trvTrk);
    }

    *trvTrk0 = trvTrk;
    return TRUE;
}



static BOOL_T drawCarEnable = TRUE;
static BOOL_T noCarDraw = FALSE;

static void DrawCar(
    track_p car,
    drawCmd_p d,
    wDrawColor color)
{
    struct extraData * xx = GetTrkExtraData(car);
    int dir;
    vector_t coupler[2];
    struct extraData * xx1;
    int dir1;

    if (drawCarEnable == FALSE) {
        return;
    }

    if (d == &mapD) {
        return;
    }

    if (noCarDraw) {
        return;
    }

    if (hideTrainsInTunnels &&
            ((((xx->state&CAR_STATE_ONHIDENTRACK)!=0) && drawTunnel==0) ||
             (xx->trkLayer!=NOTALAYER && !GetLayerVisible(xx->trkLayer)))) {
        return;
    }

    for (dir=0; dir<2; dir++) {
        track_p car1;
        coupler[dir].pos = xx->couplerPos[dir];

        if ((car1 = GetTrkEndTrk(car,dir))) {
            xx1 = GetTrkExtraData(car1);
            dir1 = (GetTrkEndTrk(car1,0)==car)?0:1;
            coupler[dir].angle = FindAngle(xx->couplerPos[dir], xx1->couplerPos[dir1]);
        } else {
            coupler[dir].angle = NormalizeAngle(xx->trvTrk.angle+(dir==0?0.0:180.0)-15.0);
        }
    }

    CarItemDraw(d, xx->item, color, xx->direction, IsLocoMaster(xx), coupler);
}


static DIST_T DistanceCar(
    track_p trk,
    coOrd * pos)
{
    struct extraData * xx = GetTrkExtraData(trk);
    DIST_T dist;
    coOrd pos1;
    coOrd size;

    if (IsIgnored(xx)) {
        return 10000.0;
    }

    CarItemSize(xx->item,
                &size);   /* TODO assumes xx->trvTrk.pos is the car center */
    dist = FindDistance(*pos, xx->trvTrk.pos);

    if (dist < size.x/2.0) {
        pos1 = *pos;
        Rotate(&pos1, xx->trvTrk.pos, -xx->trvTrk.angle);
        pos1.x += -xx->trvTrk.pos.x + size.y/2.0; /* TODO: why not size.x? */
        pos1.y += -xx->trvTrk.pos.y + size.x/2.0;

        if (pos1.x >= 0 && pos1.x <= size.y &&
                pos1.y >= 0 && pos1.y <= size.x) {
            dist = 0;
        }
    }

    *pos = xx->trvTrk.pos;
    return dist;
}


static void SetCarBoundingBox(
    track_p car)
{
    struct extraData * xx = GetTrkExtraData(car);
    coOrd lo, hi, p[4];
    int inx;
    coOrd size;
    /* TODO: should be bounding box of all pieces aligned on track */
    CarItemSize(xx->item,
                &size);   /* TODO assumes xx->trvTrk.pos is the car center */
    Translate(&p[0], xx->trvTrk.pos, xx->trvTrk.angle, size.x/2.0);
    Translate(&p[1], p[0], xx->trvTrk.angle+90, size.y/2.0);
    Translate(&p[0], p[0], xx->trvTrk.angle-90, size.y/2.0);
    Translate(&p[2], xx->trvTrk.pos, xx->trvTrk.angle+180, size.x/2.0);
    Translate(&p[3], p[2], xx->trvTrk.angle+90, size.y/2.0);
    Translate(&p[2], p[2], xx->trvTrk.angle-90, size.y/2.0);
    lo = hi = p[0];

    for (inx = 1; inx < 4; inx++) {
        if (p[inx].x < lo.x) {
            lo.x = p[inx].x;
        }

        if (p[inx].y < lo.y) {
            lo.y = p[inx].y;
        }

        if (p[inx].x > hi.x) {
            hi.x = p[inx].x;
        }

        if (p[inx].y > hi.y) {
            hi.y = p[inx].y;
        }
    }

    SetBoundingBox(car, hi, lo);
}


track_p NewCar(
    wIndex_t index,
    carItem_p item,
    coOrd pos,
    ANGLE_T angle)
{
    track_p trk;
    struct extraData * xx;
    trk = NewTrack(index, T_CAR, 2, sizeof(*xx));
    /*SetEndPts( trk, 0 );*/
    xx = GetTrkExtraData(trk);
    /*SetTrkVisible( trk, IsVisible(xx) );*/
    xx->item = item;
    xx->trvTrk.pos = pos;
    xx->trvTrk.angle = angle;
    xx->state = 0;
    SetCarBoundingBox(trk);
    CarItemSetTrack(item, trk);
    PlaceCar(trk);
    return trk;
}


static void DeleteCar(
    track_p trk)
{
    struct extraData * xx = GetTrkExtraData(trk);
    CarItemSetTrack(xx->item, NULL);
}


static void ReadCar(
    char * line)
{
    CarItemRead(line);
}


static BOOL_T WriteCar(
    track_p trk,
    FILE * f)
{
    BOOL_T rc = TRUE;
    return rc;
}


static void MoveCar(
    track_p car,
    coOrd pos)
{
    struct extraData *xx = GetTrkExtraData(car);
    xx->trvTrk.pos.x += pos.x;
    xx->trvTrk.pos.y += pos.y;
    xx->trvTrk.trk = NULL;
    PlaceCar(car);
    SetCarBoundingBox(car);
}


static void RotateCar(
    track_p car,
    coOrd pos,
    ANGLE_T angle)
{
    struct extraData *xx = GetTrkExtraData(car);
    Rotate(&xx->trvTrk.pos, pos, angle);
    xx->trvTrk.angle = NormalizeAngle(xx->trvTrk.angle + angle);
    xx->trvTrk.trk = NULL;
    PlaceCar(car);
    SetCarBoundingBox(car);
}


static BOOL_T QueryCar(track_p trk, int query)
{
    switch (query) {
    case Q_NODRAWENDPT:
        return TRUE;

    default:
        return FALSE;
    }
}


static trackCmd_t carCmds = {
    "CAR ",
    DrawCar, /* draw */
    DistanceCar, /* distance */
    DescribeCar, /* describe */
    DeleteCar, /* delete */
    WriteCar, /* write */
    ReadCar, /* read */
    MoveCar, /* move */
    RotateCar, /* rotate */
    NULL, /* rescale */
    NULL, /* audit */
    NULL, /* getAngle */
    NULL, /* split */
    NULL, /* traverse */
    NULL, /* enumerate */
    NULL, /* redraw*/
    NULL, /* trim*/
    NULL, /* merge*/
    NULL, /* modify */
    NULL, /* getLength */
    NULL, /* getParams */
    NULL, /* moveEndPt */
    QueryCar, /* query */
    NULL, /* ungroup */
    NULL, /* flip */
};

/*
 *
 */


static int numTrainDlg;


#define SLIDER_WIDTH			(20)
#define SLIDER_HEIGHT			(200)
#define SLIDER_THICKNESS		(10)
#define MAX_SPEED				(100.0)

typedef struct {
    wWin_p win;
    wIndex_t inx;
    track_p train;
    long direction;
    long followMe;
    long autoReverse;
    coOrd pos;
    char posS[STR_SHORT_SIZE];
    DIST_T speed;
    char speedS[10];
    paramGroup_p trainPGp;
} trainControlDlg_t, * trainControlDlg_p;
static trainControlDlg_t * curTrainDlg;


static void SpeedRedraw(wDraw_p, void *, wPos_t, wPos_t);
static void SpeedAction(wAction_t, coOrd);
static void LocoListChangeEntry(track_p, track_p);
static void CmdTrainExit(void *);

drawCmd_t speedD = {
    NULL,
    &screenDrawFuncs,
    0,
    1.0,
    0.0,
    { 0.0, 0.0 },
    { 0.0, 0.0 },
    Pix2CoOrd,
    CoOrd2Pix
};
static paramDrawData_t speedParamData = { SLIDER_WIDTH, SLIDER_HEIGHT, SpeedRedraw, SpeedAction, &speedD };
#ifndef WINDOWS
static paramListData_t listData = { 3, 120 };
#endif
static char * trainFollowMeLabels[] = { N_("Follow"), NULL };
static char * trainAutoReverseLabels[] = { N_("Auto Reverse"), NULL };
static paramData_t trainPLs[] = {
#define I_LIST				(0)
#ifdef WINDOWS
    /*0*/ { PD_DROPLIST, NULL, "list", PDO_NOPREF|PDO_NOPSHUPD, (void*)120, NULL, 0 },
#else
    /*0*/ { PD_LIST, NULL, "list", PDO_NOPREF|PDO_NOPSHUPD, &listData, NULL, 0 },
#endif
#define I_STATUS			(1)
    { PD_MESSAGE, NULL, NULL, 0, (void*)120 },
#define I_POS				(2)
    { PD_MESSAGE, NULL, NULL, 0, (void*)120 },
#define I_SLIDER			(3)
    { PD_DRAW, NULL, "speed", PDO_NOPSHUPD|PDO_DLGSETY, &speedParamData },
#define I_DIST				(4)
    { PD_STRING, NULL, "distance", PDO_DLGNEWCOLUMN, (void*)(100-SLIDER_WIDTH), NULL, BO_READONLY },
#define I_ZERO				(5)
    { PD_BUTTON, NULL, "zeroDistance", PDO_NOPSHUPD|PDO_NOPREF|PDO_DLGHORZ, NULL, NULL, BO_ICON },
#define I_GOTO				(6)
    { PD_BUTTON, NULL, "goto", PDO_NOPSHUPD|PDO_NOPREF|PDO_DLGWIDE, NULL, N_("Find") },
#define I_FOLLOW			(7)
    { PD_TOGGLE, NULL, "follow", PDO_NOPREF|PDO_DLGWIDE, trainFollowMeLabels, NULL, BC_HORZ|BC_NOBORDER },
#define I_AUTORVRS			(8)
    { PD_TOGGLE, NULL, "autoreverse", PDO_NOPREF, trainAutoReverseLabels, NULL, BC_HORZ|BC_NOBORDER },
#define I_DIR				(9)
    { PD_BUTTON, NULL, "direction", PDO_NOPREF|PDO_DLGWIDE, NULL, N_("Forward"), 0 },
#define I_STOP				(10)
    { PD_BUTTON, NULL, "stop", PDO_DLGWIDE, NULL, N_("Stop") },
#define I_SPEED				(11)
    { PD_MESSAGE, NULL, NULL, PDO_DLGIGNOREX, (void *)120 }
};

static paramGroup_t trainPG = { "train", 0, trainPLs, sizeof trainPLs/sizeof trainPLs[0] };


typedef struct {
    track_p loco;
    BOOL_T running;
} locoList_t;
dynArr_t locoList_da;
#define locoList(N) DYNARR_N( locoList_t, locoList_da, N )

static wIndex_t FindLoco(
    track_p loco)
{
    wIndex_t inx;

    for (inx = 0; inx<locoList_da.cnt; inx++) {
        if (locoList(inx).loco == loco) {
            return inx;
        }
    }

    return -1;
}

/**
 * Update the speed display when running trains. Draw the slider in the
 * correct position and update the odometer.
 *
 * \param d IN drawing area for slider
 * \param d IN the dialog
 * \param w, h IN unused?
 * \return    describe the return value
 */

static void SpeedRedraw(
    wDraw_p d,
    void * context,
    wPos_t w,
    wPos_t h)
{
    wPos_t y, pts[4][2];
    trainControlDlg_p dlg = (trainControlDlg_p)context;
    struct extraData * xx;
    wDrawColor drawColor;
    wDrawClear(d);

    if (dlg == NULL || dlg->train == NULL) {
        return;
    }

    xx = GetTrkExtraData(dlg->train);

    if (xx->speed > MAX_SPEED) {
        xx->speed = MAX_SPEED;
    }

    if (xx->speed < 0) {
        xx->speed = 0;
    }

    y = (wPos_t)(xx->speed/MAX_SPEED*((SLIDER_HEIGHT-SLIDER_THICKNESS))
                 +SLIDER_THICKNESS/2);
    drawColor  = wDrawFindColor(wRGB(160, 160, 160));
    pts[0][1] = pts[1][1] = y-SLIDER_THICKNESS/2;
    pts[2][1] = pts[3][1] = y+SLIDER_THICKNESS/2;
    pts[0][0] = pts[3][0] = 0;
    pts[1][0] = pts[2][0] = SLIDER_WIDTH;
    wDrawFilledPolygon(d, pts, 4, drawColor, 0);
    drawColor  = wDrawFindColor(wRGB(220, 220, 220));
    pts[0][1] = pts[1][1] = y+SLIDER_THICKNESS/2;
    pts[2][1] = pts[3][1] = y;
    pts[0][0] = pts[3][0] = 0;
    pts[1][0] = pts[2][0] = SLIDER_WIDTH;
    wDrawFilledPolygon(d, pts, 4, drawColor, 0);
    wDrawLine(d, 0, y, SLIDER_WIDTH, y, 1, wDrawLineSolid, drawColorRed, 0);
    wDrawLine(d, 0, y+SLIDER_THICKNESS/2, SLIDER_WIDTH, y+SLIDER_THICKNESS/2, 1,
              wDrawLineSolid, drawColorBlack, 0);
    wDrawLine(d, 0, y-SLIDER_THICKNESS/2, SLIDER_WIDTH, y-SLIDER_THICKNESS/2, 1,
              wDrawLineSolid, drawColorBlack, 0);
    sprintf(dlg->speedS, "%3d %s",
            (int)(units==UNITS_ENGLISH?xx->speed:xx->speed*1.6),
            (units==UNITS_ENGLISH?"mph":"km/h"));
    ParamLoadMessage(dlg->trainPGp, I_SPEED, dlg->speedS);
    LOG(log_trainPlayback, 3, ("Speed = %d\n", (int)xx->speed));
}


static void SpeedAction(
    wAction_t action,
    coOrd pos)
{
    trainControlDlg_p dlg = curTrainDlg;
    struct extraData * xx;
    FLOAT_T speed;
    BOOL_T startStop;

    if (dlg == NULL || dlg->train == NULL) {
        return;
    }

    xx = GetTrkExtraData(dlg->train);

    switch (action) {
    case C_DOWN:
        InfoMessage("");

    case C_MOVE:
    case C_UP:
        TrainTimeEndPause();

        if (IsOnTrack(xx)) {
            speed = ((FLOAT_T)((pos.y*speedD.dpi)-SLIDER_THICKNESS/2))/
                    (SLIDER_HEIGHT-SLIDER_THICKNESS)*MAX_SPEED;
        } else {
            speed = 0;
        }

        if (speed > MAX_SPEED) {
            speed = MAX_SPEED;
        }

        if (speed < 0) {
            speed = 0;
        }

        startStop = (xx->speed == 0) != (speed == 0);
        xx->speed = speed;
        SpeedRedraw((wDraw_p)dlg->trainPGp->paramPtr[I_SLIDER].control, dlg,
                    SLIDER_WIDTH, SLIDER_HEIGHT);

        if (startStop) {
            if (xx->speed == 0) {
                xx->status = ST_StopManual;
            }

            LocoListChangeEntry(dlg->train, dlg->train);
        }

        TrainTimeStartPause();

        if (trainsState == TRAINS_IDLE) {
            RestartTrains();
        }

        break;

    default:
        break;
    }
}


static void ControllerDialogSync(
    trainControlDlg_p dlg)
{
    struct extraData * xx=NULL;
    wIndex_t inx;
    BOOL_T dir;
    BOOL_T followMe;
    BOOL_T autoReverse;
    coOrd pos;

    if (dlg == NULL) {
        return;
    }

    inx = wListGetIndex((wList_p)dlg->trainPGp->paramPtr[I_LIST].control);

    if (dlg->train) {
        if (inx >= 0 && inx < locoList_da.cnt && dlg->train &&
                dlg->train != locoList(inx).loco) {
            inx = FindLoco(dlg->train);

            if (inx >= 0) {
                wListSetIndex((wList_p)dlg->trainPGp->paramPtr[I_LIST].control, inx);
            }
        }
    } else {
        wListSetIndex((wList_p)dlg->trainPGp->paramPtr[I_LIST].control, -1);
    }

    if (dlg->train) {
        char * statusMsg;
        DIST_T speed;
        xx = GetTrkExtraData(dlg->train);
        dir = xx->direction==0?0:1;
        speed = xx->speed;
        pos = xx->trvTrk.pos;
        followMe = followTrain == dlg->train;
        autoReverse = xx->autoReverse;

        if (xx->trvTrk.trk == NULL) {
            if (xx->status == ST_Crashed) {
                statusMsg = _("Crashed");
            } else {
                statusMsg = _("Not on Track");
            }
        } else if (xx->speed > 0) {
            if (trainsState == TRAINS_STOP) {
                statusMsg = _("Trains Paused");
            } else {
                statusMsg = _("Running");
            }
        } else {
            switch (xx->status) {
            case ST_EndOfTrack:
                statusMsg = _("End of Track");
                break;

            case ST_OpenTurnout:
                statusMsg = _("Open Turnout");
                break;

            case ST_StopManual:
                statusMsg = _("Manual Stop");
                break;

            case ST_NoRoom:
                statusMsg = _("No Room");
                break;

            case ST_Crashed:
                statusMsg = _("Crashed");
                break;

            default:
                statusMsg = _("Unknown Status");
                break;
            }
        }

        ParamLoadMessage(dlg->trainPGp, I_STATUS, statusMsg);
    } else {
        dir = 0;
        followMe = FALSE;
        autoReverse = FALSE;
        ParamLoadMessage(dlg->trainPGp, I_STATUS, _("No trains"));
    }

    if (dlg->followMe != followMe) {
        dlg->followMe = followMe;
        ParamLoadControl(dlg->trainPGp, I_FOLLOW);
    }

    if (dlg->autoReverse != autoReverse) {
        dlg->autoReverse = autoReverse;
        ParamLoadControl(dlg->trainPGp, I_AUTORVRS);
    }

    if (dlg->direction != dir) {
        dlg->direction = dir;
        wButtonSetLabel((wButton_p)dlg->trainPGp->paramPtr[I_DIR].control,
                        (dlg->direction?_("Reverse"):_("Forward")));
    }

    if (dlg->train) {
        if (dlg->posS[0] == '\0' ||
                dlg->pos.x != xx->trvTrk.pos.x ||
                dlg->pos.y != xx->trvTrk.pos.y) {
            long format;
            dlg->pos = xx->trvTrk.pos;
            format = GetDistanceFormat();
            format &= ~DISTFMT_DECS;
            sprintf(dlg->posS, "X:%s Y:%s",
                    FormatDistanceEx(xx->trvTrk.pos.x, format),
                    FormatDistanceEx(xx->trvTrk.pos.y, format));
            ParamLoadMessage(dlg->trainPGp, I_POS, dlg->posS);
        }

        if (dlg->speed != xx->speed) {
            dlg->speed = xx->speed;
            sprintf(dlg->speedS, "%3d",
                    (int)(units==UNITS_ENGLISH?xx->speed:xx->speed*1.6));
            ParamLoadMessage(dlg->trainPGp, I_SPEED, dlg->speedS);
            SpeedRedraw((wDraw_p)dlg->trainPGp->paramPtr[I_SLIDER].control, dlg,
                        SLIDER_WIDTH, SLIDER_HEIGHT);
        }

        ParamLoadMessage(dlg->trainPGp, I_DIST, FormatDistance(xx->distance));
    } else {
        if (dlg->posS[0] != '\0') {
            dlg->posS[0] = '\0';
            ParamLoadMessage(dlg->trainPGp, I_POS, dlg->posS);
        }

        if (dlg->speed >= 0) {
            dlg->speed = -1;
            dlg->speedS[0] = '\0';
            ParamLoadMessage(dlg->trainPGp, I_SPEED, dlg->speedS);
            wDrawClear((wDraw_p)dlg->trainPGp->paramPtr[I_SLIDER].control);
        }

        ParamLoadMessage(dlg->trainPGp, I_DIST, "");
    }
}


static void ControllerDialogSyncAll(void)
{
    if (curTrainDlg) {
        ControllerDialogSync(curTrainDlg);
    }
}


static void LocoListChangeEntry(
    track_p oldLoco,
    track_p newLoco)
{
    wIndex_t inx = -1;
    struct extraData * xx;

    if (curTrainDlg == NULL) {
        return;
    }

    if (oldLoco && (inx=FindLoco(oldLoco))>=0) {
        if (newLoco) {
            locoList(inx).loco = newLoco;
            xx = GetTrkExtraData(newLoco);
            locoList(inx).running = IsOnTrack(xx) && xx->speed > 0;
            wListSetValues((wList_p)curTrainDlg->trainPGp->paramPtr[I_LIST].control, inx,
                           CarItemNumber(xx->item), locoList(inx).running?goI:stopI, newLoco);
        } else {
            wListDelete((wList_p)curTrainDlg->trainPGp->paramPtr[I_LIST].control, inx);

            for (; inx<locoList_da.cnt-1; inx++) {
                locoList(inx) = locoList(inx+1);
            }

            locoList_da.cnt -= 1;

            if (inx >= locoList_da.cnt) {
                inx--;
            }
        }
    } else if (newLoco) {
        inx = locoList_da.cnt;
        DYNARR_APPEND(locoList_t, locoList_da, 10);
        locoList(inx).loco = newLoco;
        xx = GetTrkExtraData(newLoco);
        locoList(inx).running = IsOnTrack(xx) && xx->speed > 0;
        wListAddValue((wList_p)curTrainDlg->trainPGp->paramPtr[I_LIST].control,
                      CarItemNumber(xx->item), locoList(inx).running?goI:stopI, newLoco);
    }

    if (curTrainDlg->train == oldLoco) {
        if (newLoco || locoList_da.cnt <= 0) {
            curTrainDlg->train = newLoco;
        } else {
            curTrainDlg->train = wListGetItemContext((wList_p)
                                 curTrainDlg->trainPGp->paramPtr[I_LIST].control, inx);
        }
    }

    ControllerDialogSync(curTrainDlg);
}


static void LocoListInit(void)
{
    track_p train;
    struct extraData * xx;
    locoList_da.cnt = 0;

    for (train=NULL; TrackIterate(&train);) {
        if (GetTrkType(train) != T_CAR) {
            continue;
        }

        xx = GetTrkExtraData(train);

        if (!CarItemIsLoco(xx->item)) {
            continue;
        }

        if (!IsLocoMaster(xx)) {
            continue;
        }

        LocoListChangeEntry(NULL, train);
    }
}


static void SetCurTrain(
    track_p train)
{
    curTrainDlg->train = train;
    ControllerDialogSync(curTrainDlg);
}


static void StopTrain(
    track_p train,
    trainStatus_e status)
{
    struct extraData * xx;

    if (train == NULL) {
        return;
    }

    xx = GetTrkExtraData(train);
    xx->speed = 0;
    xx->status = status;
    LocoListChangeEntry(train, train);
}


static void MoveMainWindow(
    coOrd pos,
    ANGLE_T angle)
{
    DIST_T dist;
    static DIST_T factor = 0.5;
    ANGLE_T angle1 = angle, angle2;

    if (angle1 > 180.0) {
        angle1 = 360.0 - angle1;
    }

    if (angle1 > 90.0) {
        angle1 = 180.0 - angle1;
    }

    angle2 = R2D(atan2(mainD.size.x,mainD.size.y));

    if (angle1 < angle2) {
        dist = mainD.size.y/2.0/cos(D2R(angle1));
    } else {
        dist = mainD.size.x/2.0/cos(D2R(90.0-angle1));
    }

    dist *= factor;
    Translate(&pos, pos, angle, dist);
    DrawMapBoundingBox(FALSE);
    mainCenter = pos;
    mainD.orig.x = pos.x-mainD.size.x/2;;
    mainD.orig.y = pos.y-mainD.size.y/2;;
    MainRedraw();
    DrawMapBoundingBox(TRUE);
}


static void SetTrainDirection(
    track_p train)
{
    struct extraData *xx, *xx0=GetTrkExtraData(train);
    int dir0;
    track_p car;
    car = train;

    for (dir0 = 0; dir0 < 2; dir0++) {
        int dir;
        dir = dir0;
        WALK_CARS_START(car, xx, dir)

        if (car != train) {
            if (CarItemIsLoco(xx->item)) {
                xx->direction = (dir==dir0?xx0->direction:!xx0->direction);
            }
        }

        WALK_CARS_END(car, xx, dir)
    }
}


static void ControllerDialogUpdate(
    paramGroup_p pg,
    int inx,
    void * valueP)
{
    trainControlDlg_p dlg = curTrainDlg;
    track_p train;
    struct extraData * xx;

    if (dlg == NULL) {
        return;
    }

    TrainTimeEndPause();

    switch (inx) {
    case I_LIST:
        train = (track_p)wListGetItemContext((wList_p)pg->paramPtr[inx].control,
                                             (wIndex_t)*(long*)valueP);

        if (train == NULL) {
            return;
        }

        dlg->train = train;
        ControllerDialogSync(dlg);
        break;

    case I_ZERO:
        if (dlg->train == NULL) {
            return;
        }

        TrainTimeEndPause();
        xx = GetTrkExtraData(dlg->train);
        xx->distance = 0.0;
        ParamLoadMessage(dlg->trainPGp, I_DIST, FormatDistance(xx->distance));
        ParamLoadControl(curTrainDlg->trainPGp, I_DIST);
        TrainTimeStartPause();
        break;

    case I_GOTO:
        if (dlg->train == NULL) {
            return;
        }

        TrainTimeEndPause();
        xx = GetTrkExtraData(dlg->train);
        followTrain = NULL;
        dlg->followMe = FALSE;
        ParamLoadControl(curTrainDlg->trainPGp, I_FOLLOW);
        CarSetVisible(dlg->train);
        MoveMainWindow(xx->trvTrk.pos, xx->trvTrk.angle);
        TrainTimeStartPause();
        break;

    case I_FOLLOW:
        if (dlg->train == NULL) {
            return;
        }

        if (*(long*)valueP) {
            followTrain = dlg->train;
            xx = GetTrkExtraData(dlg->train);

            if (OFF_MAIND(xx->trvTrk.pos, xx->trvTrk.pos)) {
                MoveMainWindow(xx->trvTrk.pos, xx->trvTrk.angle);
            }

            followCenter = mainCenter;
        } else {
            followTrain = NULL;
        }

        break;

    case I_AUTORVRS:
        if (dlg->train == NULL) {
            return;
        }

        xx = GetTrkExtraData(dlg->train);
        xx->autoReverse = *(long*)valueP!=0;
        break;

    case I_DIR:
        if (dlg->train == NULL) {
            return;
        }

        xx = GetTrkExtraData(dlg->train);
        dlg->direction = xx->direction = !xx->direction;
        wButtonSetLabel((wButton_p)pg->paramPtr[I_DIR].control,
                        (dlg->direction?_("Reverse"):_("Forward")));
        SetTrainDirection(dlg->train);
        DrawAllCars();
        break;

    case I_STOP:
        if (dlg->train == NULL) {
            return;
        }

        TrainTimeEndPause();
        StopTrain(dlg->train, ST_StopManual);
        TrainTimeStartPause();
        break;

    case -1:
        /* Close window */
        CmdTrainExit(NULL);
        break;
    }

    /*ControllerDialogSync( dlg );*/
    TrainTimeStartPause();
}


static trainControlDlg_p CreateTrainControlDlg(void)
{
    trainControlDlg_p dlg;
    char * title;
    paramData_p PLp;
    dlg = (trainControlDlg_p)MyMalloc(sizeof *dlg);
    PLp = trainPLs;
    dlg->posS[0] = '\0';
    dlg->speedS[0] = '\0';
    PLp[I_LIST].valueP = &dlg->inx;
    PLp[I_LIST].context = dlg;
    PLp[I_POS].valueP = &dlg->posS;
    PLp[I_POS].context = dlg;
    /*PLp[I_GOTO].valueP = NULL;*/
    PLp[I_GOTO].context = dlg;
    PLp[I_SLIDER].context = dlg;
    PLp[I_SPEED].valueP = &dlg->speedS;
    PLp[I_SPEED].context = dlg;
    PLp[I_DIR].context = dlg;
    /*PLp[I_STOP].valueP = NULL;*/
    PLp[I_STOP].context = dlg;
    PLp[I_FOLLOW].valueP = &dlg->followMe;
    PLp[I_FOLLOW].context = dlg;
    PLp[I_AUTORVRS].valueP = &dlg->autoReverse;
    PLp[I_AUTORVRS].context = dlg;
    title = MyStrdup(_("Train Control XXX"));
    sprintf(title, _("Train Control %d"), ++numTrainDlg);
    dlg->trainPGp = &trainPG;
    dlg->win = ParamCreateDialog(dlg->trainPGp, _("Train Control"), NULL, NULL,
                                 NULL, FALSE, NULL, 0, ControllerDialogUpdate);
    return dlg;
}



/*
 * STATE INFO
 */

static struct {
    STATE_T state;
    coOrd pos0;
} Dtrain;


long trainPause = 200;
static track_p followTrain = NULL;

static void DrawAllCars(void)
{
    track_p car;
    struct extraData * xx;
    coOrd size, lo, hi;
    BOOL_T drawCarEnable1 = drawCarEnable;
    drawCarEnable = TRUE;
    wDrawDelayUpdate(mainD.d, TRUE);
    wDrawRestoreImage(mainD.d);
    DrawMarkers();
    DrawPositionIndicators();

    for (car=NULL; TrackIterate(&car);) {
        if (GetTrkType(car) == T_CAR) {
            xx = GetTrkExtraData(car);
            CarItemSize(xx->item,
                        &size);   /* TODO assumes xx->trvTrk.pos is the car center */
            lo.x = xx->trvTrk.pos.x - size.x/2.0;
            lo.y = xx->trvTrk.pos.y - size.x/2.0;
            hi.x = lo.x + size.x;
            hi.y = lo.y + size.x;

            if (!OFF_MAIND(lo, hi)) {
                DrawCar(car, &mainD, wDrawColorBlack);
            }
        }
    }

    wDrawDelayUpdate(mainD.d, FALSE);
    drawCarEnable = drawCarEnable1;
}


static DIST_T GetTrainLength2(
    track_p * car0,
    BOOL_T * dir)
{
    DIST_T length = 0, carLength;
    struct extraData * xx;
    WALK_CARS_START(*car0, xx, *dir)
    carLength = CarItemCoupledLength(xx->item);

    if (length == 0) {
        length = carLength/2.0;    /* TODO assumes xx->trvTrk.pos is the car center */
    } else {
        length += carLength;
    }

    WALK_CARS_END(*car0, xx, *dir)
    return length;
}


static DIST_T GetTrainLength(
    track_p car0,
    BOOL_T dir)
{
    return GetTrainLength2(&car0, &dir);
}


static void PlaceCar(
    track_p car)
{
    struct extraData *xx = GetTrkExtraData(car);
    DIST_T dists[2];
    int dir;
    CarItemPlace(xx->item, &xx->trvTrk, dists);

    for (dir=0; dir<2; dir++) {
        xx->couplerPos[dir] = CarItemFindCouplerMountPoint(xx->item, xx->trvTrk, dir);
    }

    car->endPt[0].angle = xx->trvTrk.angle;
    Translate(&car->endPt[0].pos, xx->trvTrk.pos, car->endPt[0].angle, dists[0]);
    car->endPt[1].angle = NormalizeAngle(xx->trvTrk.angle + 180.0);
    Translate(&car->endPt[1].pos, xx->trvTrk.pos, car->endPt[1].angle, dists[1]);
    LOG(log_trainMove, 4, ("%s @ [%0.3f,%0.3f] A%0.3f\n", CarItemNumber(xx->item),
                           xx->trvTrk.pos.x, xx->trvTrk.pos.y, xx->trvTrk.angle))
    SetCarBoundingBox(car);
    xx->state &= ~(CAR_STATE_ONHIDENTRACK);
    xx->trkLayer = NOTALAYER;

    if (xx->trvTrk.trk) {
        if (!GetTrkVisible(xx->trvTrk.trk)) {
            xx->state |= CAR_STATE_ONHIDENTRACK;
        }

        xx->trkLayer = GetTrkLayer(xx->trvTrk.trk);
    }
}


static track_p FindCar(
    coOrd * pos)
{
    coOrd pos0, pos1;
    track_p trk, trk1;
    DIST_T dist1 = 100000, dist;
    struct extraData * xx;
    trk1 = NULL;

    for (trk=NULL; TrackIterate(&trk);) {
        if (GetTrkType(trk) == T_CAR) {
            xx = GetTrkExtraData(trk);

            if (IsIgnored(xx)) {
                continue;
            }

            pos0 = *pos;
            dist = DistanceCar(trk, &pos0);

            if (dist < dist1) {
                dist1 = dist;
                trk1 = trk;
                pos1 = pos0;
            }
        }
    }

    if (dist1 < 10) {
        *pos = pos1;
        return trk1;
    } else {
        return NULL;
    }
}


static track_p FindMasterLoco(
    track_p train,
    int * dirR)
{
    struct extraData *xx0;
    int dir;

    for (dir = 0; dir<2; dir++) {
        track_p car0;
        int  dir0;
        car0 = train;
        dir0 = dir;
        WALK_CARS_START(car0, xx0, dir0)

        if (CarItemIsLoco(xx0->item) && IsLocoMaster(xx0)) {
            if (dirR) {
                *dirR = 1-dir0;
            }

            return car0;
        }

        WALK_CARS_END(car0, xx0, dir0)
    }

    return NULL;
}


static track_p PickMasterLoco(
    track_p car,
    int dir)
{
    track_p loco=NULL;
    struct extraData *xx;
    WALK_CARS_START(car, xx, dir)

    if (CarItemIsLoco(xx->item)) {
        if (IsLocoMaster(xx)) {
            return car;
        }

        if (loco == NULL) {
            loco = car;
        }
    }

    WALK_CARS_END(car, xx, dir)

    if (loco == NULL) {
        return NULL;
    }

    xx = GetTrkExtraData(loco);
    SetLocoMaster(xx);
    xx->speed = 0;
    LOG(log_trainMove, 1, ("%s becomes master\n", CarItemNumber(xx->item)))
    return loco;
}


static void UncoupleCars(
    track_p car1,
    track_p car2)
{
    track_p loco;
    int dir1, dir2;

    if (GetTrkEndTrk(car1,0) == car2) {
        dir1 = 0;
    } else if (GetTrkEndTrk(car1,1) == car2) {
        dir1 = 1;
    } else {
        ErrorMessage("uncoupleCars - not coupled");
        return;
    }

    if (GetTrkEndTrk(car2,0) == car1) {
        dir2 = 0;
    } else if (GetTrkEndTrk(car2,1) == car1) {
        dir2 = 1;
    } else {
        ErrorMessage("uncoupleCars - not coupled");
        return;
    }

    loco = FindMasterLoco(car1, NULL);
    car1->endPt[dir1].track = NULL;
    car2->endPt[dir2].track = NULL;

    if (loco) {
        track_p  loco1, loco2;
        loco1 = PickMasterLoco(car1, 1-dir1);

        if (loco1 != loco) {
            LocoListChangeEntry(NULL, loco1);
        }

        loco2 = PickMasterLoco(car2, 1-dir2);

        if (loco2 != loco) {
            LocoListChangeEntry(NULL, loco2);
        }
    }
}

static void CoupleCars(
    track_p car1,
    int dir1,
    track_p car2,
    int dir2)
{
    struct extraData * xx1, * xx2;
    track_p loco1, loco2;
    track_p car;
    int dir;
    xx1 = GetTrkExtraData(car1);
    xx2 = GetTrkExtraData(car2);

    if (GetTrkEndTrk(car1,dir1) != NULL || GetTrkEndTrk(car2,dir2) != NULL) {
        LOG(log_trainMove, 1, ("coupleCars - already coupled\n"))
        return;
    }

    car = car1;
    dir = 1-dir1;
    WALK_CARS_START(car, xx1, dir)

    if (car == car2) {
        LOG(log_trainMove, 1, ("coupleCars - already coupled\n"))
        ErrorMessage("Car coupling loop");
        return;
    }

    WALK_CARS_END(car, xx1, dir)
    car = car2;
    dir = 1-dir2;
    WALK_CARS_START(car, xx2, dir)

    if (car == car1) {
        LOG(log_trainMove, 1, ("coupleCars - already coupled\n"))
        ErrorMessage("Car coupling loop");
        return;
    }

    WALK_CARS_END(car, xx1, dir)
    loco1 = FindMasterLoco(car1, NULL);
    loco2 = FindMasterLoco(car2, NULL);
    car1->endPt[dir1].track = car2;
    car2->endPt[dir2].track = car1;

    /*ConnectTracks( car1, dir1, car2, dir2 );*/
    if (logTable(log_trainMove).level >= 2) {
        LogPrintf("Coupling %s[%d] ", CarItemNumber(xx1->item), dir1);
        LogPrintf(" and %s[%d]\n", CarItemNumber(xx2->item), dir2);
    }

    if ((loco1 != NULL && loco2 != NULL)) {
        xx1 = GetTrkExtraData(loco1);
        xx2 = GetTrkExtraData(loco2);

        if (xx1->speed == 0) {
            ClrLocoMaster(xx1);
            LOG(log_trainMove, 2, ("%s loses master\n", CarItemNumber(xx1->item)))

            if (followTrain == loco1) {
                followTrain = loco2;
            }

            LocoListChangeEntry(loco1, NULL);
            loco1 = loco2;
        } else {
            ClrLocoMaster(xx2);
            xx1->speed = (xx1->speed + xx2->speed)/2.0;

            if (xx1->speed < 0) {
                xx1->speed = 0;
            }

            if (xx1->speed > 100) {
                xx1->speed = 100;
            }

            LOG(log_trainMove, 2, ("%s loses master\n", CarItemNumber(xx2->item)))

            if (followTrain == loco2) {
                followTrain = loco1;
            }

            LocoListChangeEntry(loco2, NULL);
        }

        SetTrainDirection(loco1);
    }
}



long crashDistFactor=60;
static void PlaceCars(
    track_p car0,
    int dir0,
    long crashSpeed,
    BOOL_T crashFlip)
{
    struct extraData *xx0 = GetTrkExtraData(car0), *xx;
    int dir;
    traverseTrack_t trvTrk;
    DIST_T length;
    track_p car_curr;
    DIST_T flipflop = 1;

    if (crashFlip) {
        flipflop = -1;
    }

    dir = dir0;
    trvTrk = xx0->trvTrk;

    if (dir0) {
        FlipTraverseTrack(&trvTrk);
    }

    length = CarItemCoupledLength(xx0->item)/2.0;
    car_curr = car0;
    ClrIgnored(xx0);
    WALK_CARS_START(car_curr, xx, dir)

    if (car_curr != car0) {
        DIST_T  dist, length1;
        ClrIgnored(xx);
        length1 = CarItemCoupledLength(xx->item)/2.0;
        dist = length + length1;
        crashSpeed = crashSpeed*CRASHSPEEDDECAY/10;

        if (crashSpeed > 0) {
            dist -= dist * crashSpeed/crashDistFactor;
        }

        TraverseTrack2(&trvTrk, dist);
        xx->trvTrk = trvTrk;

        if (crashSpeed > 0) {
            xx->trvTrk.angle = NormalizeAngle(xx->trvTrk.angle + flipflop*crashSpeed);
            xx->trvTrk.trk = NULL;
        }

        flipflop = -flipflop;

        if (dir != 0) {
            FlipTraverseTrack(&xx->trvTrk);
        }

        PlaceCar(car_curr);
        length = length1;
    }

    WALK_CARS_END(car_curr, xx, dir)
}


static void CrashTrain(
    track_p car,
    int dir,
    traverseTrack_p trvTrkP,
    long speed,
    BOOL_T flip)
{
    track_p loco;
    struct extraData *xx;
    loco = FindMasterLoco(car,NULL);

    if (loco != NULL) {
        StopTrain(loco, ST_Crashed);
    }

    xx = GetTrkExtraData(car);
    xx->trvTrk = *trvTrkP;

    if (dir) {
        FlipTraverseTrack(&xx->trvTrk);
    }

    PlaceCars(car, 1-dir, speed, flip);

    if (flip) {
        speed = - speed;
    }

    xx->trvTrk.angle = NormalizeAngle(xx->trvTrk.angle - speed);
    xx->trvTrk.trk = NULL;
    PlaceCar(car);
}


static BOOL_T CheckCoupling(
    track_p car0,
    int dir00,
    BOOL_T doCheckCrash)
{
    track_p car1;
    struct extraData *xx0, *xx1;
    coOrd pos1;
    DIST_T dist0, distc, dist=100000.0;
    int dir0, dir1, dirl;
    ANGLE_T angle;
    traverseTrack_t trvTrk0, trvTrk1;
    xx0 = xx1 = GetTrkExtraData(car0);
    /* find length of train from loco to start and end */
    dir0 = dir00;
    dist0 = GetTrainLength2(&car0, &dir0);
    trvTrk0 = xx0->trvTrk;

    if (dir00) {
        FlipTraverseTrack(&trvTrk0);
    }

    TraverseTrack2(&trvTrk0, dist0);
    pos1 = trvTrk0.pos;
    car1 = FindCar(&pos1);

    if (!car1) {
        return TRUE;
    }

    xx1 = GetTrkExtraData(car1);

    if (!IsOnTrack(xx1)) {
        return TRUE;
    }

    /* determine which EP of the found car to couple to */
    angle = NormalizeAngle(trvTrk0.angle-xx1->trvTrk.angle);

    if (angle > 90 && angle < 270) {
        dir1 = 0;
        angle = NormalizeAngle(angle+180);
    } else {
        dir1 = 1;
    }

    /* already coupled? */
    if (GetTrkEndTrk(car1,dir1) != NULL) {
        return TRUE;
    }

    /* are we close to aligned? */
    if (angle > COUPLERCONNECTIONANGLE && angle < 360.0-COUPLERCONNECTIONANGLE) {
        return TRUE;
    }

    /* find pos of found car's coupler, and dist btw couplers */
    distc = CarItemCoupledLength(xx1->item);
    Translate(&pos1, xx1->trvTrk.pos, xx1->trvTrk.angle+(dir1?180.0:0.0),
              distc/2.0);
    dist = FindDistance(trvTrk0.pos, pos1);

    if (dist < trackGauge/10) {
        return TRUE;
    }

    /* not real close: are we overlapped? */
    angle = FindAngle(trvTrk0.pos, pos1);
    angle = NormalizeAngle(angle - trvTrk0.angle);

    if (angle < 90 || angle > 270) {
        return TRUE;
    }

    /* are we beyond the end of the found car? */
    if (dist > distc) {
        return TRUE;
    }

    /* are we on the same track? */
    trvTrk1 = xx1->trvTrk;

    if (dir1) {
        FlipTraverseTrack(&trvTrk1);
    }

    TraverseTrack2(&trvTrk1, distc/2.0-dist);

    if (trvTrk1.trk != trvTrk0.trk) {
        return TRUE;
    }

    if (doCheckCrash) {
        track_p loco1;
        long speed, speed0, speed1;
        speed0 = (long)xx0->speed;

        if ((xx0->direction==0) != (dir00==0)) {
            speed0 = - speed0;
        }

        loco1 = FindMasterLoco(car1, &dirl);
        xx1 = NULL;

        if (loco1) {
            xx1 = GetTrkExtraData(loco1);
            speed1 = (long)xx1->speed;

            if (car1 == loco1) {
                dirl = IsAligned(xx1->trvTrk.angle, FindAngle(trvTrk0.pos,
                                 xx1->trvTrk.pos))?1:0;
            }

            if ((xx1->direction==1) != (dirl==1)) {
                speed1 = -speed1;
            }
        } else {
            speed1 = 0;
        }

        speed = labs(speed0 + speed1);
        LOG(log_trainMove, 2, ("coupling speed=%ld\n", speed))

        if (speed > maxCouplingSpeed) {
            CrashTrain(car0, dir0, &trvTrk0, speed, FALSE);
            CrashTrain(car1, dir1, &trvTrk1, speed, TRUE);
            return FALSE;
        }
    }

    if (dir00) {
        dist = -dist;
    }

    TraverseTrack2(&xx0->trvTrk, dist);
    CoupleCars(car0, dir0, car1, dir1);
    LOG(log_trainMove, 3, ("  -> %0.3f\n", dist))
    return TRUE;
}


static void PlaceTrain(
    track_p car0,
    BOOL_T doCheckCrash,
    BOOL_T doCheckCoupling)
{
    track_p car_curr;
    struct extraData *xx0;
    int dir0;
    xx0 = GetTrkExtraData(car0);
    LOG(log_trainMove, 2, ("  placeTrain: %s [%0.3f %0.3f] A%0.3f",
                           CarItemNumber(xx0->item), xx0->trvTrk.pos.x, xx0->trvTrk.pos.y,
                           xx0->trvTrk.angle))
    car_curr = car0;

    for (dir0=0; dir0<2; dir0++) {
        int dir;
        struct extraData  *xx;
        car_curr = car0;
        dir = dir0;
        xx = xx0;
        WALK_CARS_START(car_curr, xx, dir)
        SetIgnored(xx);
        WALK_CARS_END(car_curr, xx, dir);
    }

    /* check for coupling to other cars */
    if (doCheckCoupling) {
        if (xx0->trvTrk.trk)
            if (!CheckCoupling(car0, 0, doCheckCrash)) {
                return;
            }

        if (xx0->trvTrk.trk)
            if (!CheckCoupling(car0, 1, doCheckCrash)) {
                return;
            }
    }

    PlaceCar(car0);

    for (dir0=0; dir0<2; dir0++) {
        PlaceCars(car0, dir0, 0, FALSE);
    }
}


static void PlaceTrainInit(
    track_p car0,
    track_p trk0,
    coOrd pos0,
    ANGLE_T angle0,
    BOOL_T doCheckCoupling)
{
    struct extraData * xx = GetTrkExtraData(car0);
    xx->trvTrk.trk = trk0;
    xx->trvTrk.dist = xx->trvTrk.length = -1;
    xx->trvTrk.pos = pos0;
    xx->trvTrk.angle = angle0;
    PlaceTrain(car0, FALSE, doCheckCoupling);
}


static void FlipTrain(
    track_p train)
{
    DIST_T d0, d1;
    struct extraData * xx;

    if (train == NULL) {
        return;
    }

    d0 = GetTrainLength(train, 0);
    d1 = GetTrainLength(train, 1);
    xx = GetTrkExtraData(train);
    TraverseTrack2(&xx->trvTrk, d0-d1);
    FlipTraverseTrack(&xx->trvTrk);
    xx->trvTrk.length = -1;
    PlaceTrain(train, FALSE, TRUE);
}


static BOOL_T MoveTrain(
    track_p train,
    long timeD)
{
    DIST_T ips, dist0, dist1;
    struct extraData *xx, *xx1;
    traverseTrack_t trvTrk;
    DIST_T length;
    track_p car1;
    int dir1;
    int measured;		/* make sure the distance is only measured once per train */

    if (train == NULL) {
        return FALSE;
    }

    xx = GetTrkExtraData(train);

    if (xx->speed <= 0) {
        return FALSE;
    }

    ips = ((xx->speed*5280.0*12.0)/(60.0*60.0*GetScaleRatio(GetLayoutCurScale())));
    dist0 = ips * timeD/1000.0;
    length = GetTrainLength(train, xx->direction);
    dist1 = length + dist0;
    trvTrk = xx->trvTrk;

    if (trvTrk.trk->index == 7) {
        puts("Buffer");
    }

    if (trvTrk.trk == NULL) {
        return FALSE;
    }

    LOG(log_trainMove, 1,
        ("moveTrain: %s t%ld->%0.3f S%0.3f D%d [%0.3f %0.3f] A%0.3f T%d\n",
         CarItemNumber(xx->item), timeD, dist0, xx->speed, xx->direction,
         xx->trvTrk.pos.x, xx->trvTrk.pos.y, xx->trvTrk.angle,
         xx->trvTrk.trk?GetTrkIndex(xx->trvTrk.trk):-1))

    if (xx->direction) {
        FlipTraverseTrack(&trvTrk);
    }

    TraverseTrack(&trvTrk, &dist1);

    if (dist1 > 0.0) {
        if (dist1 > dist0) {
            /*ErrorMessage( "%s no room: L%0.3f D%0.3f", CarItemNumber(xx->item), length, dist1 );*/
            StopTrain(train, ST_NoRoom);
            return FALSE;
        } else {
            dist0 -= dist1;
            LOG(log_trainMove, 1, ("   %s STOP D%d [%0.3f %0.3f] A%0.3f D%0.3f\n",
                                   CarItemNumber(xx->item), xx->direction, xx->trvTrk.pos.x, xx->trvTrk.pos.y,
                                   xx->trvTrk.angle, dist0))
        }

        /*ErrorMessage( "%s stopped at End Of Track", CarItemNumber(xx->item) );*/
        if (xx->autoReverse) {
            xx->direction = !xx->direction;
            SetTrainDirection(train);
        } else {
            if (xx->speed > maxCouplingSpeed) {
                car1 = train;
                dir1 = xx->direction;
                GetTrainLength2(&car1, &dir1);
                CrashTrain(car1, dir1, &trvTrk, (long)xx->speed, FALSE);
                return TRUE;
            } else {
                StopTrain(train, trvTrk.trk->endCnt > 1?ST_OpenTurnout:ST_EndOfTrack);
                return (FALSE);
            }
        }
    }

    trvTrk = xx->trvTrk;
    TraverseTrack2(&xx->trvTrk, xx->direction==0?dist0:-dist0);
    car1 = train;
    dir1 = 0;
    GetTrainLength2(&car1, &dir1);
    dir1 = 1-dir1;
    measured = FALSE;
    WALK_CARS_START(car1, xx1, dir1);

    if (CarItemIsLoco(xx1->item) && !measured) {
        xx->distance += dist0;
        measured = TRUE;
    }

    WALK_CARS_END(car1, xx1, dir1);

    if (train == followTrain) {
        if (followCenter.x != mainCenter.x ||
                followCenter.y != mainCenter.y) {
            if (curTrainDlg->train == followTrain) {
                curTrainDlg->followMe = FALSE;
                ParamLoadControl(curTrainDlg->trainPGp, I_FOLLOW);
            }

            followTrain = NULL;
        } else if (OFF_MAIND(xx->trvTrk.pos, xx->trvTrk.pos)) {
            MoveMainWindow(xx->trvTrk.pos,
                           NormalizeAngle(xx->trvTrk.angle+(xx->direction?180.0:0.0)));
            followCenter = mainCenter;
        }
    }

    PlaceTrain(train, TRUE, TRUE);
    return TRUE;
}


static BOOL_T MoveTrains(long timeD)
{
    BOOL_T trains_moved = FALSE;
    track_p train;
    struct extraData * xx;

    for (train=NULL; TrackIterate(&train);) {
        if (GetTrkType(train) != T_CAR) {
            continue;
        }

        xx = GetTrkExtraData(train);

        if (!CarItemIsLoco(xx->item)) {
            continue;
        }

        if (!IsLocoMaster(xx)) {
            continue;
        }

        if (xx->speed == 0) {
            continue;
        }

        trains_moved |= MoveTrain(train, timeD);
    }

    ControllerDialogSyncAll();
    DrawAllCars();
    return trains_moved;
}


static void MoveTrainsLoop(void)
{
    long time1, timeD;
    static long time0 = 0;
    trainsTimeoutPending = FALSE;

    if (trainsState != TRAINS_RUN) {
        time0 = 0;
        return;
    }

    if (time0 == 0) {
        time0 = wGetTimer();
    }

    time1 = wGetTimer();
    timeD = time1-time0;
    time0 = time1;

    if (timeD > 1000) {
        timeD = 1000;
    }

    if (MoveTrains(timeD)) {
        wAlarm(trainPause, MoveTrainsLoop);
        trainsTimeoutPending = TRUE;
    } else {
        time0 = 0;
        trainsState = TRAINS_IDLE;
        TrainTimeEndPause();
    }
}


static void RestartTrains(void)
{
    if (trainsState != TRAINS_RUN) {
        TrainTimeStartPause();
    }

    trainsState = TRAINS_RUN;

    if (!trainsTimeoutPending) {
        MoveTrainsLoop();
    }
}


static long trainTime0 = 0;
static long playbackTrainPause = 0;
static drawCmd_t trainMovieD = {
    NULL,
    &screenDrawFuncs,
    0,
    16.0,
    0,
    {0,0}, {1,1},
    Pix2CoOrd, CoOrd2Pix
};
static long trainMovieFrameDelay;
static long trainMovieFrameNext;

static void TrainTimeEndPause(void)
{
    if (recordF) {
        if (trainTime0 != 0) {
            long delay;
            delay = wGetTimer()-trainTime0;

            if (delay > 0) {
                fprintf(recordF, "TRAINPAUSE %ld\n", delay);
            }
        }

        trainTime0 = 0;
    }
}

static void TrainTimeStartPause(void)
{
    if (trainTime0 == 0) {
        trainTime0 = wGetTimer();
    }
}


static BOOL_T TrainTimeDoPause(char * line)
{
    BOOL_T drawCarEnable2;
    playbackTrainPause = atol(line);
    LOG(log_trainPlayback, 1, ("DoPause %ld\n", playbackTrainPause));
    trainsState = TRAINS_RUN;

    if (trainMovieFrameDelay > 0) {
        drawCarEnable2 = drawCarEnable;
        drawCarEnable = TRUE;
        TakeSnapshot(&trainMovieD);
        drawCarEnable = drawCarEnable2;
        LOG(log_trainPlayback, 1, ("SNAP 0\n"));
        trainMovieFrameNext = trainMovieFrameDelay;
    }

    /*MoveTrains();*/
    while (playbackTrainPause > 0) {
        if (playbackTrainPause > trainPause) {
            wPause(trainPause);
            MoveTrains(trainPause);
            playbackTrainPause -= trainPause;

            if (trainMovieFrameDelay > 0) {
                trainMovieFrameNext -= trainPause;
            }
        } else {
            wPause(playbackTrainPause);
            MoveTrains(playbackTrainPause);

            if (trainMovieFrameDelay > 0) {
                trainMovieFrameNext -= playbackTrainPause;
            }

            playbackTrainPause = 0;
        }

        if (trainMovieFrameDelay > 0 &&
                trainMovieFrameNext <= 0) {
            drawCarEnable2 = drawCarEnable;
            drawCarEnable = TRUE;
            TakeSnapshot(&trainMovieD);
            drawCarEnable = drawCarEnable2;
            LOG(log_trainPlayback, 1, ("SNAP %ld\n", trainMovieFrameNext));
            trainMovieFrameNext = trainMovieFrameDelay;
        }
    }

    return TRUE;
}


static BOOL_T TrainDoMovie(char * line)
{
    /* on/off, scale, orig, size */
    long fps;

    if (trainMovieD.dpi == 0) {
        trainMovieD.dpi = mainD.dpi;
    }

    if (!GetArgs(line, "lfpp", &fps, &trainMovieD.scale, &trainMovieD.orig,
                 &trainMovieD.size)) {
        return FALSE;
    }

    if (fps > 0) {
        trainMovieFrameDelay = 1000/fps;
    } else {
        trainMovieFrameDelay = 0;
    }

    trainMovieFrameNext = 0;
    return TRUE;
}

void AttachTrains(void)
{
    track_p car;
    track_p loco;
    struct extraData * xx;
    coOrd pos;
    track_p trk;
    ANGLE_T angle;
    EPINX_T ep0, ep1;
    int dir;

    for (car=NULL; TrackIterate(&car);) {
        ClrTrkBits(car, TB_CARATTACHED);

        if (GetTrkType(car) != T_CAR) {
            continue;
        }

        xx = GetTrkExtraData(car);
        ClrProcessed(xx);
    }

    for (car=NULL; TrackIterate(&car);) {
        if (GetTrkType(car) != T_CAR) {
            continue;
        }

        xx = GetTrkExtraData(car);

        if (IsProcessed(xx)) {
            continue;
        }

        loco = FindMasterLoco(car, NULL);

        if (loco != NULL) {
            xx = GetTrkExtraData(loco);
        } else {
            loco = car;
        }

        pos = xx->trvTrk.pos;

        if (xx->status == ST_Crashed) {
            continue;
        }

        TRK_ITERATE(trk) {
            if (trk == xx->trvTrk.trk) {
                break;
            }
        }

        if (trk!=NULL && !QueryTrack(trk, Q_ISTRACK)) {
            trk = NULL;
        }

        if (trk==NULL || GetTrkDistance(trk,pos)>trackGauge*2.0) {
            trk = OnTrack2(&pos, FALSE, TRUE, FALSE);
        }

        if (trk!=NULL) {
            /*if ( trk == xx->trvTrk.trk )
            	continue;*/
            angle = GetAngleAtPoint(trk, pos, &ep0, &ep1);

            if (NormalizeAngle(xx->trvTrk.angle-angle+90) > 180) {
                angle = NormalizeAngle(angle+180);
            }

            PlaceTrainInit(loco, trk, pos, angle, TRUE);
        } else {
            PlaceTrainInit(loco, NULL, xx->trvTrk.pos, xx->trvTrk.angle, FALSE);
        }

        dir = 0;
        WALK_CARS_START(loco, xx, dir)
        WALK_CARS_END(loco, xx, dir)
        dir = 1-dir;
        WALK_CARS_START(loco, xx, dir)
        SetProcessed(xx);

        if (xx->trvTrk.trk) {
            SetTrkBits(xx->trvTrk.trk, TB_CARATTACHED);
            xx->status = ST_StopManual;
        } else {
            xx->status = ST_NotOnTrack;
        }

        WALK_CARS_END(loco, xx, dir)
    }

    for (car=NULL; TrackIterate(&car);) {
        if (GetTrkType(car) != T_CAR) {
            continue;
        }

        xx = GetTrkExtraData(car);
        ClrProcessed(xx);
    }
}


static void UpdateTrainAttachment(void)
{
    track_p trk;
    struct extraData * xx;

    for (trk=NULL; TrackIterate(&trk);) {
        ClrTrkBits(trk, TB_CARATTACHED);
    }

    for (trk=NULL; TrackIterate(&trk);) {
        if (GetTrkType(trk) == T_CAR) {
            xx = GetTrkExtraData(trk);

            if (xx->trvTrk.trk != NULL) {
                SetTrkBits(xx->trvTrk.trk, TB_CARATTACHED);
            }
        }
    }
}


static BOOL_T TrainOnMovableTrack(
    track_p trk,
    track_p *trainR)
{
    track_p train;
    struct extraData * xx;
    int dir;

    for (train=NULL; TrackIterate(&train);) {
        if (GetTrkType(train) != T_CAR) {
            continue;
        }

        xx = GetTrkExtraData(train);

        if (IsOnTrack(xx)) {
            if (xx->trvTrk.trk == trk) {
                break;
            }
        }
    }

    *trainR = train;

    if (train == NULL) {
        return TRUE;
    }

    dir = 0;
    WALK_CARS_START(train, xx, dir)
    WALK_CARS_END(train, xx, dir)
    dir = 1-dir;
    WALK_CARS_START(train, xx, dir)

    if (xx->trvTrk.trk != trk) {
        ErrorMessage(MSG_CANT_MOVE_UNDER_TRAIN);
        return FALSE;
    }

    WALK_CARS_END(train, xx, dir)
    train = FindMasterLoco(train, NULL);

    if (train != NULL) {
        *trainR = train;
    }

    return TRUE;
}

/*
 *
 */

#define DO_UNCOUPLE		(0)
#define DO_FLIPCAR		(1)
#define DO_FLIPTRAIN	(2)
#define DO_DELCAR		(3)
#define DO_DELTRAIN		(4)
#define DO_MUMASTER		(5)
#define DO_CHANGEDIR	(6)
#define DO_STOP			(7)
static track_p trainFuncCar;
static coOrd trainFuncPos;
static wButton_p trainPauseB;

static STATUS_T CmdTrain(wAction_t action, coOrd pos)
{
    track_p trk0, trk1;
    static track_p currCar;
    coOrd pos0, pos1;
    static coOrd delta;
    ANGLE_T angle1;
    EPINX_T ep0, ep1;
    int dir;
    struct extraData * xx=NULL;
    wPos_t w, h;

    switch (action) {
    case C_START:
        /*UndoStart( "Trains", "Trains" );*/
        UndoSuspend();
        programMode = MODE_TRAIN;
        drawCarEnable = FALSE;
        doDrawTurnoutPosition = 1;
        DoChangeNotification(CHANGE_PARAMS|CHANGE_TOOLBAR);

        if (CarAvailableCount() <= 0) {
            if (NoticeMessage(MSG_NO_CARS, _("Yes"), _("No")) > 0) {
                DoCarDlg();
                DoChangeNotification(CHANGE_PARAMS);
            }
        }

        EnableCommands();

        if (curTrainDlg == NULL) {
            curTrainDlg = CreateTrainControlDlg();
        }

        curTrainDlg->train = NULL;
        wListClear((wList_p)curTrainDlg->trainPGp->paramPtr[I_LIST].control);
        Dtrain.state = 0;
        trk0 = NULL;
        tempSegs_da.cnt = 0;
        DYNARR_SET(trkSeg_t, tempSegs_da, 8);
        RestartTrains();
        wButtonSetLabel(trainPauseB, (char*)goI);
        trainTime0 = 0;
        AttachTrains();
        DrawAllCars();
        curTrainDlg->train = NULL;
        curTrainDlg->speed = -1;
        wDrawClear((wDraw_p)curTrainDlg->trainPGp->paramPtr[I_SLIDER].control);
        LocoListInit();
        ControllerDialogSync(curTrainDlg);
        wShow(curTrainDlg->win);
        wControlShow((wControl_p)newcarB, (toolbarSet&(1<<BG_HOTBAR)) == 0);
        currCarItemPtr = NULL;
        return C_CONTINUE;

    case C_TEXT:
        if (Dtrain.state == 0) {
            return C_CONTINUE;
        } else {
            return C_CONTINUE;
        }

    case C_DOWN:
        /*trainEnable = FALSE;*/
        InfoMessage("");

        if (trainsState == TRAINS_RUN) {
            trainsState = TRAINS_PAUSE;
            TrainTimeEndPause();
        }

        pos0 = pos;

        if (currCarItemPtr != NULL) {
            DIST_T dist;
            currCar = NewCar(-1, currCarItemPtr, zero, 0.0);
            CarItemUpdate(currCarItemPtr);
            HotBarCancel();

            if (currCar == NULL) {
                LOG1(log_error, ("Train: currCar became NULL 1\n"))
                return C_CONTINUE;
            }

            xx = GetTrkExtraData(currCar);
            dist = CarItemCoupledLength(xx->item)/2.0;
            Translate(&pos, xx->trvTrk.pos, xx->trvTrk.angle, dist);
            SetTrkEndPoint(currCar, 0, pos, xx->trvTrk.angle);
            Translate(&pos, xx->trvTrk.pos, xx->trvTrk.angle+180.0, dist);
            SetTrkEndPoint(currCar, 1, pos, NormalizeAngle(xx->trvTrk.angle+180.0));
            /*xx->state |= (xx->item->options&CAR_DESC_BITS);*/
            ClrLocoMaster(xx);

            if (CarItemIsLoco(xx->item)) {
                SetLocoMaster(xx);
                LocoListChangeEntry(NULL, currCar);
            }

            if ((trk0 = OnTrack(&pos0, FALSE, TRUE))) {
                xx->trvTrk.angle = GetAngleAtPoint(trk0, pos0, &ep0, &ep1);

                if (NormalizeAngle(FindAngle(pos, pos0) - xx->trvTrk.angle) > 180.0) {
                    xx->trvTrk.angle = NormalizeAngle(xx->trvTrk.angle + 180);
                }

                xx->status = ST_StopManual;
            } else {
                xx->trvTrk.angle = 90;
            }

            PlaceTrainInit(currCar, trk0, pos0, xx->trvTrk.angle,
                           (MyGetKeyState()&WKEY_SHIFT) == 0);
            /*DrawCars( &tempD, currCar, TRUE );*/
        } else {
            currCar = FindCar(&pos);
            delta.x = pos.x - pos0.x;
            delta.y = pos.y - pos0.y;

            if (logTable(log_trainMove).level >= 1) {
                if (currCar) {
                    xx = GetTrkExtraData(currCar);
                    LogPrintf("selected %s\n", CarItemNumber(xx->item));

                    for (dir=0; dir<2; dir++) {
                        int dir1 = dir;
                        track_p car1 = currCar;
                        struct extraData * xx1 = GetTrkExtraData(car1);
                        LogPrintf("dir=%d\n", dir1);
                        WALK_CARS_START(car1, xx1, dir1)
                        LogPrintf("  %s [%0.3f,%d]\n", CarItemNumber(xx1->item), xx1->trvTrk.angle,
                                  dir1);
                        WALK_CARS_END(car1, xx1, dir1)
                    }
                }
            }
        }

        if (currCar == NULL) {
            return C_CONTINUE;
        }

        trk0 = FindMasterLoco(currCar, NULL);

        if (trk0) {
            SetCurTrain(trk0);
        }

        DrawAllCars();
        return C_CONTINUE;

    case C_MOVE:
        if (currCar == NULL) {
            return C_CONTINUE;
        }

        pos.x += delta.x;
        pos.y += delta.y;
        pos0 = pos;
        /*DrawCars( &tempD, currCar, FALSE );*/
        xx = GetTrkExtraData(currCar);
        trk0 = OnTrack(&pos0, FALSE, TRUE);

        if (/*currCarItemPtr != NULL &&*/ trk0) {
            angle1 = GetAngleAtPoint(trk0, pos0, &ep0, &ep1);

            if (currCarItemPtr != NULL) {
                if (NormalizeAngle(FindAngle(pos, pos0) - angle1) > 180.0) {
                    angle1 = NormalizeAngle(angle1 + 180);
                }
            } else {
                if (NormalizeAngle(xx->trvTrk.angle - angle1 + 90.0) > 180.0) {
                    angle1 = NormalizeAngle(angle1 + 180);
                }
            }

            xx->trvTrk.angle = angle1;
        }

        tempSegs_da.cnt = 1;
        PlaceTrainInit(currCar, trk0, pos0, xx->trvTrk.angle,
                       (MyGetKeyState()&WKEY_SHIFT) == 0);
        ControllerDialogSync(curTrainDlg);
        DrawAllCars();
        return C_CONTINUE;

    case C_UP:
        if (currCar != NULL) {
            trk0 = FindMasterLoco(currCar, NULL);

            if (trk0) {
                xx = GetTrkExtraData(trk0);

                if (!IsOnTrack(xx) || xx->speed <= 0) {
                    StopTrain(trk0, ST_StopManual);
                }
            }

            Dtrain.state = 1;
            /*MainRedraw();*/
            ControllerDialogSync(curTrainDlg);
        }

        DrawAllCars();
        InfoSubstituteControls(NULL, NULL);
        currCar = trk0 = NULL;
        currCarItemPtr = NULL;

        /*trainEnable = TRUE;*/
        if (trainsState == TRAINS_PAUSE) {
            RestartTrains();
        }

        return C_CONTINUE;

    case C_LCLICK:
        if (MyGetKeyState() & WKEY_SHIFT) {
            pos0 = pos;
            programMode = MODE_DESIGN;

            if ((trk0=OnTrack(&pos,FALSE,TRUE)) &&
                    QueryTrack(trk0, Q_CAN_NEXT_POSITION) &&
                    TrainOnMovableTrack(trk0, &trk1)) {
                if (trk1) {
                    xx = GetTrkExtraData(trk1);
                    pos1 = xx->trvTrk.pos;
                    angle1 = xx->trvTrk.angle;
                } else {
                    pos1 = pos0;
                    angle1 = 0;
                }

                AdvancePositionIndicator(trk0, pos0, &pos1, &angle1);

                if (trk1) {
                    xx->trvTrk.pos = pos1;
                    xx->trvTrk.angle = angle1;
                    PlaceTrain(trk1, FALSE, TRUE);
                    DrawAllCars();
                }
            }

            programMode = MODE_TRAIN;
            trk0 = NULL;
            MainRedraw(); //Make sure track is redrawn after switch thrown
        } else {
            trk0 = FindCar(&pos);

            if (trk0 == NULL) {
                return C_CONTINUE;
            }

            trk0 = FindMasterLoco(trk0, NULL);

            if (trk0 == NULL) {
                return C_CONTINUE;
            }

            SetCurTrain(trk0);
        }

        return C_CONTINUE;

    case C_RCLICK:
        trainFuncPos = pos;
        trainFuncCar = FindCar(&pos);

        if (trainFuncCar == NULL ||
                GetTrkType(trainFuncCar) != T_CAR) {
            return C_CONTINUE;
        }

        xx = GetTrkExtraData(trainFuncCar);
        trk0 = FindMasterLoco(trainFuncCar,NULL);
        dir = IsAligned(xx->trvTrk.angle, FindAngle(xx->trvTrk.pos,
                        trainFuncPos)) ? 0 : 1;
        wMenuPushEnable(trainPopupMI[DO_UNCOUPLE], GetTrkEndTrk(trainFuncCar,
                        dir)!=NULL);
        wMenuPushEnable(trainPopupMI[DO_MUMASTER], CarItemIsLoco(xx->item) &&
                        !IsLocoMaster(xx));

        if (trk0) {
            xx = GetTrkExtraData(trk0);
        }

        wMenuPushEnable(trainPopupMI[DO_CHANGEDIR], trk0!=NULL);
        wMenuPushEnable(trainPopupMI[DO_STOP], trk0!=NULL && xx->speed>0);
        /*trainEnable = FALSE;*/
        trk0 = FindMasterLoco(trainFuncCar, NULL);

        if (trk0) {
            SetCurTrain(trk0);
        }

        if (!inPlayback) {
            wMenuPopupShow(trainPopupM);
        }

        return C_CONTINUE;

    case C_REDRAW:
        wDrawSaveImage(mainD.d);
        DrawAllCars();
        wWinGetSize(mainW, &w, &h);
        w -= wControlGetPosX(newCarControls[0]) + 4;

        if (w > 20) {
            wListSetSize((wList_p)newCarControls[0], w,
                         wControlGetHeight(newCarControls[0]));
        }

        return C_CONTINUE;

    case C_CANCEL:
        /*trainEnable = FALSE;*/
        trainsState = TRAINS_STOP;
        TrainTimeEndPause();
        LOG(log_trainMove, 1, ("Train Cancel\n"))
        Dtrain.state = 0;
        doDrawTurnoutPosition = 0;
        drawCarEnable = TRUE;
        programMode = MODE_DESIGN;
        UpdateTrainAttachment();
        UndoResume();
        DoChangeNotification(CHANGE_PARAMS|CHANGE_TOOLBAR);

        if (curTrainDlg->win) {
            wHide(curTrainDlg->win);
        }

        MainRedraw();
        curTrainDlg->train = NULL;
        return C_CONTINUE;

    case C_CONFIRM:

        /*trainEnable = FALSE;*/
        if (trainsState != TRAINS_STOP) {
            trainsState = TRAINS_STOP;
            wButtonSetLabel(trainPauseB, (char*)stopI);
            TrainTimeEndPause();
        }

        currCar = NULL;
        currCarItemPtr = NULL;
        HotBarCancel();
        InfoSubstituteControls(NULL, NULL);
        return C_TERMINATE;
    }

    return C_CONTINUE;
}


/*
 *
 */

STATUS_T CmdCarDescAction(
    wAction_t action,
    coOrd pos)
{
    return CmdTrain(action, pos);
}

#include "bitmaps/train.xpm"
#include "bitmaps/exit.xpm"
#include "bitmaps/newcar.xpm"
#include "bitmaps/zero.xpm"
#include "bitmaps/ballgreen.xpm"
#include "bitmaps/ballred.xpm"


static void CmdTrainStopGo(void * junk)
{
    wIcon_p icon;

    if (trainsState == TRAINS_STOP) {
        icon = goI;
        RestartTrains();
    } else {
        trainsState = TRAINS_STOP;
        icon = stopI;
        TrainTimeEndPause();
    }

    ControllerDialogSync(curTrainDlg);
    wButtonSetLabel(trainPauseB, (char*)icon);

    if (recordF) {
        fprintf(recordF, "TRAINSTOPGO %s\n", trainsState==TRAINS_STOP?"STOP":"GO");
    }
}

static BOOL_T TrainStopGoPlayback(char * line)
{
    while (*line && isspace((unsigned char)*line)) {
        line++;
    }

    if ((strcasecmp(line, "STOP") == 0) != (trainsState == TRAINS_STOP)) {
        CmdTrainStopGo(NULL);
    }

    return TRUE;
}


static void CmdTrainExit(void * junk)
{
    Reset();
    InfoSubstituteControls(NULL, NULL);
    MainRedraw();
}


static void TrainFunc(
    void * action)
{
    struct extraData * xx, *xx1;
    ANGLE_T angle;
    int dir;
    track_p loco;
    track_p temp0, temp1;
    coOrd pos0, pos1;
    ANGLE_T angle0, angle1;
    EPINX_T ep0=-1, ep1=-1;

    if (trainFuncCar == NULL) {
        fprintf(stderr, "trainFunc: trainFuncCar==NULL\n");
        return;
    }

    xx = GetTrkExtraData(trainFuncCar);
    angle = FindAngle(xx->trvTrk.pos, trainFuncPos);
    angle = NormalizeAngle(angle-xx->trvTrk.angle);
    dir = (angle>90&&angle<270);

    switch ((int)(long)action) {
    case DO_UNCOUPLE:
        if (GetTrkEndTrk(trainFuncCar,dir)) {
            UncoupleCars(trainFuncCar, GetTrkEndTrk(trainFuncCar,dir));
        }

        break;

    case DO_FLIPCAR:
        temp0 = GetTrkEndTrk(trainFuncCar,0);
        pos0 = GetTrkEndPos(trainFuncCar,0);
        angle0 = GetTrkEndAngle(trainFuncCar,0);
        temp1 = GetTrkEndTrk(trainFuncCar,1);
        pos1 = GetTrkEndPos(trainFuncCar,1);
        angle1 = GetTrkEndAngle(trainFuncCar,1);

        if (temp0) {
            ep0 = GetEndPtConnectedToMe(temp0,trainFuncCar);
            trainFuncCar->endPt[0].track = NULL;
            temp0->endPt[ep0].track = NULL;
        }

        if (temp1) {
            ep1 = GetEndPtConnectedToMe(temp1,trainFuncCar);
            trainFuncCar->endPt[1].track = NULL;
            temp1->endPt[ep1].track = NULL;
        }

        xx->direction = !xx->direction;
        FlipTraverseTrack(&xx->trvTrk);
        SetTrkEndPoint(trainFuncCar, 0, pos1, angle1);
        SetTrkEndPoint(trainFuncCar, 1, pos0, angle0);

        if (temp0) {
            trainFuncCar->endPt[1].track = temp0;
            temp0->endPt[ep0].track = trainFuncCar;
        }

        if (temp1) {
            trainFuncCar->endPt[0].track = temp1;
            temp1->endPt[ep1].track = trainFuncCar;
        }

        ControllerDialogSync(curTrainDlg);
        PlaceCar(trainFuncCar);
        break;

    case DO_FLIPTRAIN:
        FlipTrain(trainFuncCar);
        /*PlaceTrain( trainFuncCar, xx->trk, xx->trvTrk.pos, xx->trvTrk.angle );*/
        break;

    case DO_DELCAR:
        for (dir=0; dir<2; dir++)
            if (GetTrkEndTrk(trainFuncCar,dir)) {
                UncoupleCars(trainFuncCar, GetTrkEndTrk(trainFuncCar,dir));
            }

        if (CarItemIsLoco(xx->item)) {
            LocoListChangeEntry(trainFuncCar, NULL);
        }

        trainFuncCar->deleted = TRUE;
        /*DeleteTrack( trainFuncCar, FALSE );*/
        CarItemUpdate(xx->item);
        HotBarCancel();
        InfoSubstituteControls(NULL, NULL);
        break;

    case DO_DELTRAIN:
        dir = 0;
        loco = FindMasterLoco(trainFuncCar, NULL);
        WALK_CARS_START(trainFuncCar, xx, dir)
        WALK_CARS_END(trainFuncCar, xx, dir)
        dir = 1-dir;
        temp0 = NULL;
        WALK_CARS_START(trainFuncCar, xx, dir)

        if (temp0) {
            xx1 = GetTrkExtraData(temp0);
            temp0->deleted = TRUE;
            /*DeleteTrack( temp0, FALSE );*/
            CarItemUpdate(xx1->item);
        }

        temp0 = trainFuncCar;
        WALK_CARS_END(trainFuncCar, xx, dir)

        if (temp0) {
            xx1 = GetTrkExtraData(temp0);
            temp0->deleted = TRUE;
            /*DeleteTrack( temp0, FALSE );*/
            CarItemUpdate(xx1->item);
        }

        if (loco) {
            LocoListChangeEntry(loco, NULL);
        }

        HotBarCancel();
        InfoSubstituteControls(NULL, NULL);
        break;

    case DO_MUMASTER:
        if (CarItemIsLoco(xx->item)) {
            loco = FindMasterLoco(trainFuncCar, NULL);

            if (loco != trainFuncCar) {
                SetLocoMaster(xx);
                LOG(log_trainMove, 1, ("%s gets master\n", CarItemNumber(xx->item)))

                if (loco) {
                    xx1 = GetTrkExtraData(loco);
                    ClrLocoMaster(xx1);
                    LOG(log_trainMove, 1, ("%s looses master\n", CarItemNumber(xx1->item)))
                    xx->speed = xx1->speed;
                    xx1->speed = 0;
                }

                LocoListChangeEntry(loco, trainFuncCar);
            }
        }

        break;

    case DO_CHANGEDIR:
        loco = FindMasterLoco(trainFuncCar, NULL);

        if (loco) {
            xx = GetTrkExtraData(loco);
            xx->direction = !xx->direction;
            SetTrainDirection(loco);
            ControllerDialogSync(curTrainDlg);
        }

        break;

    case DO_STOP:
        loco = FindMasterLoco(trainFuncCar, NULL);

        if (loco) {
            StopTrain(loco, ST_StopManual);
            ControllerDialogSync(curTrainDlg);
        }

        break;
    }

    MainRedraw();  //Redraw if Train altered

    if (trainsState == TRAINS_PAUSE) {
        RestartTrains();
    } else {
        DrawAllCars();
    }
}


void InitCmdTrain(wMenu_p menu)
{
    log_trainMove = LogFindIndex("trainMove");
    log_trainPlayback = LogFindIndex("trainPlayback");
    trainPLs[I_ZERO].winLabel = (char*)wIconCreatePixMap(zero_xpm);
    ParamRegister(&trainPG);
    AddMenuButton(menu, CmdTrain, "cmdTrain", _("Train"),
                  wIconCreatePixMap(train_xpm), LEVEL0_50, IC_POPUP2|IC_LCLICK|IC_RCLICK, 0,
                  NULL);
    stopI = wIconCreatePixMap(ballred);
    goI = wIconCreatePixMap(ballgreen);
    trainPauseB = AddToolbarButton("cmdTrainPause", stopI, IC_MODETRAIN_ONLY,
                                   CmdTrainStopGo, NULL);
    AddToolbarButton("cmdTrainExit", wIconCreatePixMap(exit_xpm), IC_MODETRAIN_ONLY,
                     CmdTrainExit, NULL);
    newcarB = AddToolbarButton("cmdTrainNewCar", wIconCreatePixMap(newcar_xpm),
                               IC_MODETRAIN_ONLY, CarItemLoadList, NULL);
    T_CAR = InitObject(&carCmds);
    trainPopupM = MenuRegister("Train Commands");
    trainPopupMI[DO_UNCOUPLE]   = wMenuPushCreate(trainPopupM, "", _("Uncouple"), 0,
                                  TrainFunc, (void*)DO_UNCOUPLE);
    trainPopupMI[DO_FLIPCAR]    = wMenuPushCreate(trainPopupM, "", _("Flip Car"), 0,
                                  TrainFunc, (void*)DO_FLIPCAR);
    trainPopupMI[DO_FLIPTRAIN]  = wMenuPushCreate(trainPopupM, "", _("Flip Train"),
                                  0, TrainFunc, (void*)DO_FLIPTRAIN);
    trainPopupMI[DO_MUMASTER]   = wMenuPushCreate(trainPopupM, "", _("MU Master"),
                                  0, TrainFunc, (void*)DO_MUMASTER);
    trainPopupMI[DO_CHANGEDIR]  = wMenuPushCreate(trainPopupM, "",
                                  _("Change Direction"), 0, TrainFunc, (void*)DO_CHANGEDIR);
    trainPopupMI[DO_STOP]       = wMenuPushCreate(trainPopupM, "", _("Stop"), 0,
                                  TrainFunc, (void*)DO_STOP);
    wMenuSeparatorCreate(trainPopupM);
    trainPopupMI[DO_DELCAR]     = wMenuPushCreate(trainPopupM, "", _("Remove Car"),
                                  0, TrainFunc, (void*)DO_DELCAR);
    trainPopupMI[DO_DELTRAIN]   = wMenuPushCreate(trainPopupM, "",
                                  _("Remove Train"), 0, TrainFunc, (void*)DO_DELTRAIN);
    AddPlaybackProc("TRAINSTOPGO", (playbackProc_p)TrainStopGoPlayback, NULL);
    AddPlaybackProc("TRAINPAUSE", (playbackProc_p)TrainTimeDoPause, NULL);
    AddPlaybackProc("TRAINMOVIE", (playbackProc_p)TrainDoMovie, NULL);
}

