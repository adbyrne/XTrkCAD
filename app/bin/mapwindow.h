#ifndef MAPWINDOW_H
#define MAPWINDOW_H

wControl_p MapWindowCreate();
void MapWindowShow(int state);
void MapWindowToggleShow(void* unused);
void MapDrawBoundingBox(BOOL_T set);
void MapChangeScale(void);
void MapRefresh(void);
wBool_t MapGetVisiblePref(void);
#endif