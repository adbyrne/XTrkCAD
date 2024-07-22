/** \file common.h
 * Defnitions of basic types
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef COMMON_H
#define COMMON_H

// INCLUDES
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "include\dynarray.h"
#include "include\xtctypes.h"
#include "wlib.h"

#ifndef WINDOWS
// Unix/Mac
#include <dirent.h>
#include <unistd.h>

#define PATH_SEPARATOR "/"

#else
// Windows
#include <io.h>
#include <process.h>
#include "include/dirent.h"
#include "direct.h"
#include "getopt.h"

// DEFINES
#define UTFCONVERT
#define M_PI 3.14159265358979323846

#define F_OK (00)
#define W_OK (02)
#define R_OK (04)
#define PATH_SEPARATOR "\\"

// ALIASES for WINDOWS
#define access _access
#define unlink(a) _unlink((a))
#define rmdir(a) _rmdir((a))
#define open(name, flag, mode) _open((name), (flag), (mode))
#define close(file) _close((file))
#define getpid() _getpid()
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define mkdir( DIR, MODE ) _mkdir( (DIR) )
#if _MSC_VER >1300
#define strnicmp _strnicmp
#define stricmp _stricmp
#define strdup _strdup
#endif
// starting from Visual Studio 2015 round is in the runtime library, fake otherwise
#if ( _MSC_VER < 1900 )
#define round(x) floor((x)+0.5)
#endif

/* suppress warning from *.bmp about conversion of int to char */
#pragma warning( disable : 4305 )
/* suppress warning about array references */
#pragma warning( disable : 6385 )
#endif

// We need to pass integer values via void* objects
// typically context vars (declared as void*) which sometimes pass pointers and some times integers
// For example see paramData_t.context.
// Also some wlib calls take a generic parameter, declared as void* but somethings used to pass integers
// These are used in logical pairs; VP=I2VP(INTEGER); ...a lot of code...; INTEGER=VP2L(VP);
// Note: we never use VP2L to manipulate integer-ized values of a pointer and all integer values we use fit in a long
#define I2VP(VAL) ((void*)(intptr_t)(VAL))
#define VP2L(VAL) ((long)(intptr_t)(VAL))


#ifndef TRUE
#define TRUE	(1)
#define FALSE	(0)
#endif

#define DIST_INF 2.0E9

#define NUM_LAYERS		(99)

#define SCANF_FLOAT_FORMAT "%lf"

enum paramFileState { PARAMFILE_UNLOADED = 0, PARAMFILE_NOTUSABLE, PARAMFILE_COMPATIBLE, PARAMFILE_FIT, PARAMFILE_MAXSTATE };

// Base DotsPerInch
#define BASE_DPI	(75.0)

// FILE VERSIONS - non-backward file format changes
// Descriptions added for Bezier, Cornu, Joint
#define VERSION_DESCRIPTION2	(12)
// Inline quoted text replaces multiline text in Notes and Cars
#define VERSION_INLINENOTE	(12)
// END is replaced by END$SEGS, END$TRK, ...
#define VERSION_NONAKEDENDS	(12)


// FORWARD TYPE DECLS
typedef struct drawCmd_t * drawCmd_p;
typedef struct track_t * track_p;
typedef struct track_t * track_cp;
typedef struct trkSeg_t * trkSeg_p;
typedef struct traverseTrack_t * traverseTrack_p;
typedef struct trkEndPt_t * trkEndPt_p;
typedef void (*doSaveCallBack_p)( void );
typedef void (*addButtonCallBack_t)(void*);
typedef STATUS_T (*procCommand_t) (wAction_t, coOrd);


// base class for extraData*_t: each of which must include this struct as the first element
typedef struct extraDataBase_t {
	TRKTYP_T trkType;
} extraDataBase_t;
// We check if TRKTYP_T in trk, trk->extraDataBase and the code context (TRKTYP) match.
// If TRKTYP is T_NOTRACK then we are dealing with T_TURNOUT/T_STRUCTURE or T_BEZIER/T_BEZLIN which
// share a log of code and have the same extraData*_t structure.
#define GET_EXTRA_DATA(TRK,TRKTYP,TYPE) \
	((TYPE*)GetTrkExtraData( (TRK), (TRKTYP) ))
extraDataBase_t * GetTrkExtraData( track_p, TRKTYP_T );


typedef struct {
	BOOL_T valid;
	DIST_T length;
	DIST_T width;
	DIST_T spacing;
} tieData_t, *tieData_p;

// Syntactic suger for exported (non-static) objects
#define EXPORT

#define COUNT(A) (sizeof(A)/sizeof(A[0]))

#define STR_SIZE		(256)
#define STR_SHORT_SIZE	(80)
#define STR_LONG_SIZE	(1024)
#define STR_HUGE_SIZE	(10240)

#define CAST_AWAY_CONST (char*)

#define TITLEMAXLEN (40)

// COMMON INCLUDES
// If you add includes here, please remove them elsewhere

#include "i18n.h"
#include "utility.h"
#include "acclkeys.h"
#include "misc.h"

// TODO - move these includes to the files that need them
#include "dlayer.h"
#include "scale.h"
#include "command.h"
#include "menu.h"

#endif

