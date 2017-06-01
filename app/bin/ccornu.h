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

enum CornuType {};
STATUS_T CmdCornu (wAction_t, coOrd);
BOOL_T CallCornu(coOrd[2],ANGLE_T[2],DIST_T[2],dynArr_t *);
STATUS_T CmdCornu( wAction_t action, coOrd pos );
DIST_T CornuMinRadius(coOrd pos[4],dynArr_t segs);
DIST_T CornuLength(coOrd pos[4],dynArr_t segs);
