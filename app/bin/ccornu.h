/*
 * ccornu.h
 *
 *  Created on: May 28, 2017
 *      Author: richardsa
 */

#ifndef APP_BIN_CCORNU_H_
#define APP_BIN_CCORNU_H_


typedef void (*cornuMessageProc)( char *, ... );

#define cornuCmdNone   		  (0)
#define cornuJoinTrack        (1)
#define cornuCmdCreateTrack   (2)
#define cornuCmdHotBar		  (3)


#endif /* APP_BIN_CCORNU_H_ */

STATUS_T CmdCornu( wAction_t action, coOrd pos );
BOOL_T CallCornu0(coOrd pos[2], coOrd center[2], ANGLE_T angle[2], DIST_T radius[2], dynArr_t * array_p, BOOL_T spots);
DIST_T CornuMinRadius(coOrd pos[4],dynArr_t segs);
DIST_T CornuMaxRateofChangeofCurvature(coOrd pos[4],dynArr_t segs,DIST_T * last_c);
DIST_T CornuLength(coOrd pos[4],dynArr_t segs);
DIST_T CornuOffsetLength(dynArr_t segs, double offset);
DIST_T CornuTotalWindingArc(coOrd pos[4],dynArr_t segs);

STATUS_T CmdCornuModify (track_p trk, wAction_t action, coOrd pos, DIST_T trackG);

void InitCmdCornu( wMenu_p menu );

void AddHotBarCornu( void );
