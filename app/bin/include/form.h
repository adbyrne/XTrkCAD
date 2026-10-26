/** \file form.h
 * Definitions for dialog (form) handling
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

#ifndef HAVE_FORM_H
#define HAVE_FORM_H

#include "xtctypes.h"
#include "common.h"
#include "draw.h" //- playbackAction

typedef enum {
	PD_LONG,
	PD_FLOAT,
	PD_RADIO,
	PD_TOGGLE,
	PD_STRING,
	PD_LIST,
	PD_DROPLIST,
	PD_COMBOLIST,
	PD_BUTTON,
	PD_COLORLIST,
	PD_MESSAGE,					/* static text */
	PD_DRAW,
	PD_TEXT,
	PD_MENU,
	PD_MENUITEM,
	PD_BITMAP,
	PD_SCALE,
	PD_NOTEBOOK,
	PD_TAG,
	PD_EXPANDER
} parameterType;

// PD_FLOAT modifiers
#define PDO_DIM					(1L<<0)		/**< value is a dimension */
#define PDO_ANGLE				(1L<<1)		/**< value is an angle in degrees */
#define PDO_SMALLDIM			(1L<<2)
// PD_STRING modifiers
#define PDO_NOTBLANK			(1L<<3)

#define PDO_NORECORD			(1L<<6)
#define PDO_NOPSHACT			(1L<<7)
#define PDO_NOPSHUPD			(1L<<8)		/**< do not update the input variable */
#define PDO_NOUPDACT			(1L<<9)

// Override paramGroup pref group
#define PDO_NOPREF			(1L<<10)		/**< save entered value as default */
#define PDO_DRAW			(1L<<12)
#define PDO_FILE			(1L<<13)
//#define PDO_ENTER               	(1L<<14)
#define PDO_ENTER               	0

//#define PDO_STRINGLIMITLENGTH		(1L<<11)	/**< context has maximum length for string */
#define PDO_STRINGLIMITLENGTH		0	/**< context has maximum length for string */

// Ignore param
#define PDO_DLGIGNORE			(1L<<15)

// Layout options
#define PDO_DLGWIDE			(1L<<17)
#define PDO_DLGBOXEND			(1L<<19)	 /**< draw recessed frame around the controls */
#define PDO_DLGRESETMARGIN		(1L<<20)	 /**< position control on the left ?*/
#define PDO_DLGIGNORELABELWIDTH 	(1L<<21)
#define PDO_DLGHORZ			(1L<<22)  /**< arrange on same line as previous element */
#define PDO_SAMEROW			(1L<<11)		/** place control on same line as previous */
#define PDO_NEWSAMEROW			(1L<<12)		/** end any current SAMEROW group and start a new one */
#define PDO_DLGNEWCOLUMN		(1L<<23)
#define PDO_DLGNOLABELALIGN		(1L<<24)
#define PDO_LISTINDEX			(1L<<25)
#define PDO_DLGSETY			(1L<<26)
#define PDO_DLGIGNOREX			(1L<<27)
#define PDO_DLGUNDERCMDBUTT		(1L<<28)
#define PDO_DLGCMDBUTTON		(1L<<29)	/**< arrange button on the right with the default buttons */

typedef struct paramGroup_t paramGroup_t;
typedef paramGroup_t * paramGroup_p;
typedef paramGroup_t const * paramGroup_cp;

#define PDO_NORANGECHECK_LOW			(1<<0)
#define PDO_NORANGECHECK_HIGH	(1<<1)
typedef struct {
	long low;
	long high;
	wWinPix_t width;
	int rangechecks;
} paramIntegerRange_t;
typedef struct {
	FLOAT_T low;
	FLOAT_T high;
	wWinPix_t width;
	int rangechecks;
} paramFloatRange_t;
typedef struct {
	wWinPix_t width;
	wWinPix_t height;
	wDrawRedrawCallBack_p redraw;
	playbackProc action;
	drawCmd_p d;
} paramDrawData_t;
typedef struct {
	wIndex_t number;
	wWinPix_t width;
	int colCnt;
	wWinPix_t * colWidths;
	const char * * colTitles;
	wWinPix_t height;
} paramListData_t;
typedef struct {
	wWinPix_t width;
	wWinPix_t height;
} paramTextData_t;

typedef union {
	long l;
	FLOAT_T f;
	char * s;
	wDrawColor dc;
} paramOldData_t;
typedef struct {
	parameterType type;
	void * valueP;
	char * nameStr;
	long option;
	const void * winData;
	const char * winLabel;
	long winOption;
	void * context;
	unsigned int max_string;
	wControl_p control;
	paramGroup_p group;
	paramOldData_t oldD, demoD;
	wBool_t enter_pressed;
	wBool_t bInvalid;
	unsigned posColumn, posRow;
	wBool_t bShown;
} paramData_t, *paramData_p;


#define PGACT_PARAM		(2)
#define PGACT_RESTORE	(4)

#define PGO_RECORD				(1<<1)
#define PGO_NODEFAULTPROC		(1<<2)
#define PGO_PREFMISCGROUP		(1<<8)
#define PGO_PREFMISC			(1<<10)
#define PGO_FULLDIALOGFROMBUILDER (1L<<13)	/**< complete dialog is defined in XML file*/

typedef void (*paramActionOkProc)( void const * );
typedef void (*paramActionCancelProc)( paramGroup_cp group );
/**
 * Called when a control in the group changes (and with index -1 for dialog
 * events). For a PD_STRING control the result is passed back to the text
 * entry (StringPush), where TRUE means the application already stored the
 * value; today the entry has nothing further to store either way. Return
 * TRUE unless there's a reason not to. Functions registered here must have
 * exactly this type: one returning void left the result undefined
 * (Development Issues #53).
 */
typedef wBool_t (*paramChangeProc)( paramGroup_cp group, int index,
                                    void * value);

typedef struct paramGroup_t {
	char * nameStr;
	long options;
	paramData_p paramPtr;
	int paramCnt;
	paramActionOkProc okProc;
	paramActionCancelProc cancelProc;
	long winOption;
	paramChangeProc changeProc;
	long action;
	wControl_p win;
	wButton_p okB;
	wButton_p cancelB;
	wButton_p helpB;
	wWinPix_t origW;
	wWinPix_t origH;
	wBox_p * boxs;
} paramGroup_t;

#define PD_F_ALT_CANCELLABEL	(1L<<30)		/**<use Close or Cancel for the discard button */

#define ANGLE_POLAR		(0)
extern long angleSystem;
#define PutAngle(X)		((angleSystem==ANGLE_POLAR)?(X):NormalizeAngle(90.0-(X)))

#define DISTFMT_DECS			0x00FF
#define DISTFMT_FMT			0x0300
#define DISTFMT_FMT_NONE		0x0000
#define DISTFMT_FMT_SHRT		0x0100
#define DISTFMT_FMT_LONG		0x0200
#define DISTFMT_FMT_MM			0x0100
#define DISTFMT_FMT_CM			0x0200
#define DISTFMT_FMT_M			0x0300
#define DISTFMT_FRACT			0x0400
#define DISTFMT_FRACT_NUM		0x0000
#define DISTFMT_FRACT_FRC		0x0400

char * FormatLong( long );
char * FormatFloat( FLOAT_T );
char * FormatDistance( FLOAT_T );
char * FormatSmallDistance( FLOAT_T );
char * FormatDistanceEx( FLOAT_T, long );
FLOAT_T FormDecodeDistance( const char *enteredValue, BOOL_T* validP );

void ParamRestoreAll( void );
void ParamSaveAll( void );
void ParamSetInReadTracks(BOOL_T state);
void ParamTurnOffDelays(BOOL_T disable);

long GetChanges(paramGroup_p pg);

void FormInit(void);
void FormRegister(paramGroup_p pg);
void FormLoadDefaultValues(paramGroup_p pg);
void FormSaveDefaultValues(paramGroup_p pg);
void FormUpdatePrefs(void);

paramGroup_cp DialogGroupFind( const char * sName );
paramGroup_cp * DialogGroupIter( paramGroup_cp * );

wControl_p FormCreateDialog(
        paramGroup_p group,
        char* title,
        char* okLabel,
        paramActionOkProc okProc,
        char* cancelLabel,
        paramActionCancelProc cancelProc,
        BOOL_T needHelpButton,
        long winOption,
        paramChangeProc changeProc);

void FormCreateControls(paramGroup_p group);
void FormLoadControls(paramGroup_p pg);
void FormLoadSingleControl(paramGroup_p pg, int inx);
void FormLoadMessage(paramGroup_p pg, int inx, char* message);
void FormMenuPush(void* dp);

void FormStartRecord(FILE* fileHandle);
void FormMacroRecord(char* format, ...);
void FormGroupRecord(paramGroup_cp pg);
void FormErrorState( paramData_p p, wBool_t valid, const char *reason );

wBool_t FormIntegerRangeCheck(paramData_p p, long valL);
wBool_t FormFloatRangeCheck(paramData_p p, FLOAT_T valF);
wBool_t FormStringCheckValue(paramData_p data, char* value);
wBool_t FormCheckInputs(paramGroup_p group, wControl_p b);
long FormUpdate(paramGroup_p pg);

void FormDialogOkActive(paramGroup_p pg, int active);
void FormOk_Null(const void *group);
void FormCancel_Undo(paramGroup_cp group );
void FormCancel_Current(paramGroup_cp group);
void FormCancel_Reset(paramGroup_cp group);
void FormCancel_Restore(paramGroup_cp group);
void FormCancel_Null(paramGroup_cp group);

#define FormCancel_Custom( PROC ) PROC

void FormControlActive(paramGroup_p pg, int inx, BOOL_T active);
void FormControlShow(paramGroup_p pg, int inx, BOOL_T bShow);
EXPORT void FormHilite( wControl_p win, wControl_p control, BOOL_T hilite );
void FormGroupReveal(paramGroup_p pg, const char *id, BOOL_T reveal);
typedef void (*paramExpanderToggleProc)(paramGroup_p pg, const char *id,
                                        BOOL_T revealed, void *context);

void FormGroupExpanderShow(paramGroup_p pg, const char *id, BOOL_T reveal);
void FormGroupExpanderSetSummary(paramGroup_p pg, const char *id,
                                 const char *summary);
void FormGroupSetShadow(paramGroup_p pg, const char *id, BOOL_T shown);
void FormResetInvalid( wControl_p win );

void FormFetchData(paramGroup_p pg);

#endif // HAVE_FORM_H
