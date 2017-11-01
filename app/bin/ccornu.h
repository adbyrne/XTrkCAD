/*
 * ccornu.h
 *
 *  Created on: May 28, 2017
 *      Author: richardsa
 */

#ifndef APP_BIN_CCORNU_H_
#define APP_BIN_CCORNU_H_


typedef void (*cornuMessageProc)( char *, ... );


#endif /* APP_BIN_CCORNU_H_ */

STATUS_T CmdCornu( wAction_t action, coOrd pos );
DIST_T CornuMinRadius(coOrd pos[4],dynArr_t segs);
DIST_T CornuMaxRateofChangeofCurvature(coOrd pos[4],dynArr_t segs,DIST_T * last_c);
DIST_T CornuLength(coOrd pos[4],dynArr_t segs);
DIST_T CornuTotalWindingArc(coOrd pos[4],dynArr_t segs);

STATUS_T CmdCornuModify (track_p trk, wAction_t action, coOrd pos);
