#ifndef HAVE_TRANSFORM_PTS_H
#define HAVE_TRANSFORM_PTS_H

#include <common.h>

void RotatePts(int cnt, coOrd* pts, coOrd orig, ANGLE_T angle);

void RescalePts(int cnt, coOrd* pts, FLOAT_T scale_x, FLOAT_T scale_y);
#endif