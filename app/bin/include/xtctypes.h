
#ifndef XTCTYPES_H
#define XTCTYPES_H

// TYPEDEFS

typedef double FLOAT_T;
typedef double POS_T;
typedef double DIST_T;
typedef double ANGLE_T;
typedef double LWIDTH_T;

typedef double DOUBLE_T;
typedef double WDOUBLE_T;
typedef double FONTSIZE_T;

typedef struct {
	POS_T x, y;
} coOrd;

typedef struct {
	coOrd pt;
	int pt_type;
} pts_t;

typedef int INT_T;

typedef int BOOL_T;
typedef int EPINX_T;
typedef int CSIZE_T;
#ifndef WIN32
typedef int SIZE_T;
#endif
typedef int STATE_T;
typedef int STATUS_T;
typedef signed char TRKTYP_T;
typedef int TRKINX_T;
typedef long DEBUGF_T;
typedef int REGION_T;
typedef long SCALEINX_T;
typedef long GAUGEINX_T;
typedef long SCALEDESCINX_T;

#endif
