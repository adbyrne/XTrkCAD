/** \file layout.c
 * Layout data and dialog
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2017 Martin Fischer
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

#include <dynstring.h>

#include "custom.h"
#include "layout.h"
#include "param.h"
#include "paths.h"
#include "track.h"
#include "fileio.h"
#include "cselect.h"

#define MINTRACKRADIUSPREFS "minTrackRadius"
#define MAXTRACKGRADEPREFS "maxTrackGrade"

#define TEXT_FIELD_LEN 40

/**
 * @brief Layout properties in Dialog
*/
struct sLayoutProps {
	char			title1[TITLEMAXLEN];
	char			title2[TITLEMAXLEN];
	SCALEINX_T		curScaleInx;
	SCALEDESCINX_T	curScaleDescInx;
	GAUGEINX_T		curGaugeInx;
	DIST_T			minTrackRadius;
	DIST_T			maxTrackGrade;
	coOrd			roomSize;
	char            backgroundTextBox[TEXT_FIELD_LEN+1];
	coOrd			backgroundPos;
	ANGLE_T			backgroundAngle;
	int				backgroundScreen;
	double 			backgroundSize;
};


/**
 * @brief Layout properties not in dialog, and save values for Cancel
*/
struct sDataLayout {
	struct sLayoutProps props;
	DynString	fullFileName;
	DynString
	backgroundFileName; /** This is used to hold the entire path string */
	struct sLayoutProps
		*copyOfLayoutProps; /** Copy of props used to restore initial values in the event of Cancel */
	DynString
	copyBackgroundFileName; /** Copy of Background Path used to restore initial value in the event of Cancel */
};

static struct sDataLayout thisLayout = {
	{ "", "", 0, 0, 0, 0.0, 0.0, {0.0, 0.0}, "", {0.0, 0.0}, 0.0, 0, 0.0},
	NaS,
	NaS,
	NULL,
	NaS
};

EXPORT wIndex_t changed = 0;

static paramFloatRange_t r0_90 = { 0, 90 };
// static paramFloatRange_t r0o05_100 = { 0.05, 100 };
static paramFloatRange_t r0_10000 = { 0, 10000 };
static paramFloatRange_t r0_9999999 = { 0, 9999999 };
static paramFloatRange_t r1_9999999 = { 1, 9999999 };
static paramFloatRange_t r360_360 = { -360, 360 };
static paramFloatRange_t rN_9999999 = { -99999, 99999 };
static paramIntegerRange_t i0_100 = { 0, 100 };

static void SettingsWrite( void  );
static void SettingsRead( void  );

static void LayoutDlgUpdate(paramGroup_p pg, int inx, void * valueP);

/**
* Update the full file name. Do not do anything if the new filename is identical to the old one.
*
* \param filename IN the new filename
*/

void
SetLayoutFullPath(const char *fileName)
{
	if (fileName && fileName[0]) {
		if (DynStringSize(&thisLayout.fullFileName)) {
			if (strcmp(DynStringToCStr(&thisLayout.fullFileName),fileName)==0) {
				return;
			}
			DynStringClear(&thisLayout.fullFileName);
		}
		DynStringMalloc(&thisLayout.fullFileName, strlen(fileName) + 1);
		DynStringCatCStr(&thisLayout.fullFileName, fileName);
	} else {
		DynStringMalloc(&thisLayout.fullFileName, 2);
		DynStringCatCStr(&thisLayout.fullFileName, "");
	}
}

/**
* Set the minimum radius for the selected scale/gauge into the dialog
*
* \param scaleName IN name of the scale/gauge eg. HOn3
* \param defaltValue IN default value will be used if no preference is set
*/
void
LoadLayoutMinRadiusPref(char *scaleName, double defaultValue)
{
	DynString prefString = { NULL };

	DynStringPrintf(&prefString, MINTRACKRADIUSPREFS "-%s", scaleName);
	wPrefGetFloat("misc", DynStringToCStr(&prefString),
	              &thisLayout.props.minTrackRadius, defaultValue);
	DynStringFree(&prefString);
}

/**
* Set the maximum grade for the selected scale/gauge into the dialog
*
* \param scaleName IN name of the scale/gauge eg. HOn3
* \param defaltValue IN default value will be used if no preference is set
*/
void
LoadLayoutMaxGradePref(char* scaleName, double defaultValue)
{
	DynString prefString = { NULL };

	DynStringPrintf(&prefString, MAXTRACKGRADEPREFS "-%s", scaleName);
	wPrefGetFloat("misc", DynStringToCStr(&prefString),
	              &thisLayout.props.maxTrackGrade, defaultValue);
	DynStringFree(&prefString);
}

/**
 * @brief Copy Layout title making sure there is a null at the end
 * @param dest The destination to copy to
 * @param src The source of the copy
*/
static void
CopyLayoutTitle(char* dest, char *src)
{
	strncpy(dest, src, TITLEMAXLEN);
	*(dest + TITLEMAXLEN - 1) = '\0';
}


/**
* Set the file's changed flag and update the window title.
*/

void
SetFileChanged(void)
{
	changed++;
	SetWindowTitle();
}

/**
 * @brief Set the Layout title
 * @param title The Layout title
*/
void
SetLayoutTitle(char *title)
{
	CopyLayoutTitle(thisLayout.props.title1, title);
}

/**
 * @brief Set the layout Subtitle
 * @param title The Layout subtitle
*/
void
SetLayoutSubtitle(char *title)
{
	CopyLayoutTitle(thisLayout.props.title2, title);
}

/**
 * @brief Set the Layout minimum track radius.
 * @param radius
*/
void
SetLayoutMinTrackRadius(DIST_T radius)
{
	thisLayout.props.minTrackRadius = radius;
}

/**
 * @brief Set the Layout maximum track grade.
 * @param angle The maximum track grade.
*/
void
SetLayoutMaxTrackGrade(ANGLE_T angle)
{
	thisLayout.props.maxTrackGrade = angle;
}

/**
 * @brief Set the layout room size
 * @param size The room size (coOrd)
*/
void
SetLayoutRoomSize(coOrd size)
{
	thisLayout.props.roomSize = size;
}

/**
 * @brief Set the Layout scale (index). Also set the default Tie Data.
 * @param scale The Layout scale index.
*/
void
SetLayoutCurScale(SCALEINX_T scale)
{
	thisLayout.props.curScaleInx = scale;
}

/**
 * @brief Set the Layout scale description (index)
 * @param desc The Layout scale description index.
*/
void
SetLayoutCurScaleDesc(SCALEDESCINX_T desc)
{
	thisLayout.props.curScaleDescInx = desc;
}

/**
 * @brief Set the Layout current gauge (index)
 * @param gauge The Layout gauge index.
*/
void
SetLayoutCurGauge(GAUGEINX_T gauge)
{
	thisLayout.props.curGaugeInx = gauge;
}

/**
 * @brief Set the Layout background full path.
 * @param fileName The Layout background full path string.
*/
void SetLayoutBackGroundFullPath(const char *fileName)
{
	if (fileName && fileName[0]) {
		if (DynStringSize(&thisLayout.backgroundFileName)) {
			if (strcmp(DynStringToCStr(&thisLayout.backgroundFileName),fileName)==0) {
				return;
			}
			DynStringClear(&thisLayout.backgroundFileName);
		}
		DynStringMalloc(&thisLayout.backgroundFileName, strlen(fileName) + 1);
		DynStringCatCStr(&thisLayout.backgroundFileName, fileName);
	} else {
		DynStringClear(&thisLayout.backgroundFileName);
		DynStringMalloc(&thisLayout.backgroundFileName, 1);
		DynStringCatCStr(&thisLayout.backgroundFileName, "");
	}
}

/**
 * @brief Set the Layout background size (relative to the layout width)
 * @param size The Layout background size.
*/
void SetLayoutBackGroundSize(double size)
{
	thisLayout.props.backgroundSize = size;
}

/**
 * @brief Set the Layout background position (origin).
 * @param pos The Layout background origin (coOrd).
*/
void SetLayoutBackGroundPos(coOrd pos)
{
	thisLayout.props.backgroundPos = pos;

}

/**
 * @brief Set the Layout Background angle.
 * @param angle The Layout Background angle (ANGLE_T).
*/
void SetLayoutBackGroundAngle(ANGLE_T angle)
{
	thisLayout.props.backgroundAngle = angle;

}

/**
 * @brief Set the Layout Background screen (percent of transparency)
 * @param screen The Screen value (0-100)
*/
void SetLayoutBackGroundScreen(int screen)
{
	thisLayout.props.backgroundScreen = screen;

}

/**
* Return the full filename.
*
* \return    pointer to the full filename, should not be modified or freed
*/

char *
GetLayoutFullPath()
{
	char * s = DynStringToCStr(&thisLayout.fullFileName);
	return s;
}

/**
* Return the filename part of the full path
*
* \return    pointer to the filename part, NULL is no filename is set
*/

char *
GetLayoutFilename()
{
	char *string = DynStringToCStr(&thisLayout.fullFileName);

	if (string) {
		return FindFilename(string);
	} else {
		return (NULL);
	}
}

/**
 * @brief Return the layout title
 * @return The title string
*/
char *
GetLayoutTitle()
{
	return (thisLayout.props.title1);
}

/**
 * @brief Return the layout subtitle
 * @return The subtitle string
*/
char *
GetLayoutSubtitle()
{
	return (thisLayout.props.title2);
}

/**
 * @brief Returns the layout minimum radius
 * @return The minimum radius (DIST_T)
*/
DIST_T
GetLayoutMinTrackRadius()
{
	return (thisLayout.props.minTrackRadius);
}

/**
 * @brief Returns the layout maximum track grade
 * @return The Maximum grade (ANGLE_T)
*/
ANGLE_T
GetLayoutMaxTrackGrade()
{
	return (thisLayout.props.maxTrackGrade);
}

/**
 * @brief Returns the scale index of layout scale description
 * @return The Scale Description index
*/
SCALEDESCINX_T
GetLayoutCurScaleDesc()
{
	return (thisLayout.props.curScaleDescInx);
}

/**
* @brief Returns the scale index of layout scale setting
* @return The Scale Index
*/
SCALEINX_T
GetLayoutCurScale()
{
	return (thisLayout.props.curScaleInx);
}

/**
 * @brief Returns the Layout Background full path
 * @return The Background full path
*/
char *
GetLayoutBackGroundFullPath()
{
	char * s = DynStringToCStr(&thisLayout.backgroundFileName);
	return s;
}

/**
 * @brief Returns the layout background size.
 * @return The background size, or room size if zero
*/
double
GetLayoutBackGroundSize()
{
	if (thisLayout.props.backgroundSize > 0.0) {
		return (thisLayout.props.backgroundSize);
	} else {
		return (thisLayout.props.roomSize.x);
	}
}

/**
 * @brief Returns the background position (origin)
 * @return The background position (coOrd)
*/
coOrd
GetLayoutBackGroundPos()
{
	return (thisLayout.props.backgroundPos);
}

/**
 * @brief Returns the background angle
 * @return The background angle (ANGLE_T)
*/
ANGLE_T
GetLayoutBackGroundAngle()
{
	return (thisLayout.props.backgroundAngle);
}

/**
 * @brief Returns the background screen percent (the amount of transparency)
 * @return The background Screen value (0-100)
*/
int GetLayoutBackGroundScreen()
{
	return (thisLayout.props.backgroundScreen);
}

/**
 * Gets layout room size
 *
 * \param [out] roomSize size of the room.
 */

void
GetLayoutRoomSize(coOrd *roomSize)
{
	*roomSize = thisLayout.props.roomSize;
}

/****************************************************************************
*
* Layout Dialog
*
*/
static wWin_p layoutW;
static void LayoutChange(long changes);

/**
* Show only the filename in the Dialog
*/
void SetName()
{
	char *name = GetLayoutBackGroundFullPath();
	if (name && name[0]) {									//Ignore ""
		char *f = FindFilename(name);
		if ( f ) {
			strncpy( thisLayout.props.backgroundTextBox,f,TEXT_FIELD_LEN );
		} else {
			thisLayout.props.backgroundTextBox[0] = '\0';
		}
	} else { thisLayout.props.backgroundTextBox[0] = '\0'; }
}

static struct wFilSel_t * imageFile_fs;

static paramData_p layout_p;
static paramGroup_t * layout_pg_p;
static wBool_t file_changed;

bool haveBackground = false;
BOOL_T backgroundVisible = TRUE;

char * noname = "";

EXPORT wButton_p backgroundB;

/**
 * @brief Enable background visibility toggle from Menu or Button
 * @param unused
*/
void
BackgroundToggleShow( void * unused )
{
	backgroundVisible = !backgroundVisible;
	wButtonSetBusy(backgroundB, backgroundVisible);
	MainRedraw();
}

/**
 * @brief Returns status of backgroundVisible
 * @return backgroundVisible
*/
int GetLayoutBackGroundVisible()
{
	return(backgroundVisible);
}

/**
 * @brief Returns status of layout background.
 * @return true if a background is defined, false otherwise
*/
bool HasBackGround()
{
	return(haveBackground);
}

/**
* Try to load the background image file. Display notice if failed to load.
* @return TRUE if successful, FALSE if not.
*/
wBool_t
LoadBackGroundImage(void)
{
	char * error;
	char * background = GetLayoutBackGroundFullPath();
	if (wDrawSetBackground(  mainD.d, background, &error)==-1) {
		NoticeMessage(_("Unable to load Image File - %s"),_("Ok"),NULL,error);
		return FALSE;
	}
	wControlActive((wControl_p)backgroundB, backgroundVisible);
	wButtonSetBusy(backgroundB, backgroundVisible);

	return TRUE;
}

#define BACKGROUNDFILEENTRY (8)

/**
* Callback from File Select for Background Image File
*
* \param files number of files selected (only first file is used)
* \param fileName array of pointers to filenames
* \param data unused
* \return FALSE
*/
EXPORT int LoadImageFile(
        int files,
        char ** fileName,
        void * data )
{
	if (files >0) {
		SetLayoutBackGroundFullPath( strdup(fileName[0]) );

		if (!LoadBackGroundImage()) {
			SetLayoutBackGroundFullPath(noname);
			backgroundVisible = FALSE;
		} else {
			backgroundVisible = TRUE;
			SetCurrentPath(BACKGROUNDPATHKEY, fileName[0]);

			file_changed = TRUE;
			haveBackground = TRUE;
			ParamLoadControl(layout_pg_p, BACKGROUNDFILEENTRY);

			MainRedraw();
		}
	} else {
		SetLayoutBackGroundFullPath(noname);
		backgroundVisible = FALSE;
	}
	wControlActive((wControl_p)backgroundB, backgroundVisible);
	wButtonSetBusy(backgroundB, backgroundVisible);

	SetName();
	file_changed = TRUE;
	ParamLoadControl(layout_pg_p, BACKGROUNDFILEENTRY);
	LayoutChange( CHANGE_BACKGROUND );

	return FALSE;
}

/**
 * Save the Layout Background Parms in section [layout]. Force a write.
*/
void LayoutBackGroundSave(void)
{
	char * background = GetLayoutBackGroundFullPath();
	wPrefSetString("layout", "BackgroundPath", background);
	wPrefSetFloat("layout", "BackgroundPosX", thisLayout.props.backgroundPos.x);
	wPrefSetFloat("layout", "BackgroundPosY", thisLayout.props.backgroundPos.y);
	wPrefSetFloat("layout", "BackgroundAngle", thisLayout.props.backgroundAngle);
	wPrefSetInteger("layout", "BackgroundScreen",
	                thisLayout.props.backgroundScreen);
	wPrefSetFloat("layout", "BackgroundSize", thisLayout.props.backgroundSize);

	wPrefFlush("");
}

/**
 * Run File Select for the Background Image File
*/
static void ImageFileBrowse( void * unused )
{
	imageFile_fs = wFilSelCreate( mainW, FS_LOAD, FS_PICTURES, _("Load Background"),
	                              sImageFilePattern, LoadImageFile, NULL );

	wFilSelect( imageFile_fs, GetCurrentPath( BACKGROUNDPATHKEY ) );
	return;
}

/**
 * Remove the background Image File
*/
static void ImageFileClear( void * unused)
{
	char * noname = "";
	SetLayoutBackGroundFullPath(noname);
	wDrawSetBackground(  mainD.d, NULL, NULL);
	SetName();
	wControlActive((wControl_p)backgroundB, FALSE);
	file_changed = TRUE;
	haveBackground = false;
	ParamLoadControl(layout_pg_p, BACKGROUNDFILEENTRY);

	LayoutChange( CHANGE_BACKGROUND );

	MainRedraw();
}

static paramData_t layoutPLs[] = {
	{ PD_FLOAT, &thisLayout.props.roomSize.x, "roomsizeX", PDO_NOPREF | PDO_DIM | PDO_NOPSHUPD | PDO_DRAW, &r1_9999999, N_("Room Width"), 0, I2VP(CHANGE_MAIN | CHANGE_MAP) },
	{ PD_FLOAT, &thisLayout.props.roomSize.y, "roomsizeY", PDO_NOPREF | PDO_DIM | PDO_NOPSHUPD | PDO_DRAW | PDO_DLGHORZ, &r1_9999999, N_("    Height"), 0, I2VP(CHANGE_MAIN | CHANGE_MAP) },
	{ PD_STRING, &thisLayout.props.title1, "title1", PDO_NOPSHUPD | PDO_STRINGLIMITLENGTH, NULL, N_("Layout Title"), 0, 0, sizeof(thisLayout.props.title1)},
	{ PD_STRING, &thisLayout.props.title2, "title2", PDO_NOPSHUPD | PDO_STRINGLIMITLENGTH, NULL, N_("Subtitle"), 0, 0, sizeof(thisLayout.props.title2)},
#define SCALEINX (4)
	{ PD_DROPLIST, &thisLayout.props.curScaleDescInx, "scale", PDO_NOPREF | PDO_NOPSHUPD | PDO_NORECORD | PDO_NOUPDACT, I2VP(180), N_("Scale"), 0, I2VP(CHANGE_SCALE) },
#define GAUGEINX (5)
	{ PD_DROPLIST, &thisLayout.props.curGaugeInx, "gauge", PDO_NOPREF | PDO_NOPSHUPD | PDO_NORECORD | PDO_NOUPDACT | PDO_DLGHORZ, I2VP(180), N_("     Gauge"), 0, I2VP(CHANGE_SCALE) },
#define MINRADIUSENTRY (6)
	{ PD_FLOAT, &thisLayout.props.minTrackRadius, "mintrackradius", PDO_DIM | PDO_NOPSHUPD | PDO_NOPREF, &r0_10000, N_("Min Track Radius"), 0, I2VP(CHANGE_MAIN | CHANGE_LIMITS) },
	{ PD_FLOAT, &thisLayout.props.maxTrackGrade, "maxtrackgrade", PDO_NOPSHUPD | PDO_DLGHORZ, &r0_90, N_("  Max Track Grade (%)"), 0, I2VP(CHANGE_MAIN) },
#define BACKGROUNDFILECHECK (8)  //Note this value used in the file section routines above - if it changes, they will need to change
	{ PD_STRING, &thisLayout.props.backgroundTextBox, "backgroundfile", PDO_NOPSHUPD|PDO_NOPREF|PDO_NORECORD|PDO_STRINGLIMITLENGTH,  NULL, N_("Background File Path"), 0, I2VP(CHANGE_BACKGROUND), TEXT_FIELD_LEN },
	{ PD_BUTTON, ImageFileBrowse, "browse", PDO_DLGHORZ, NULL, N_("Browse ...") },
	{ PD_BUTTON, ImageFileClear, "clear", PDO_DLGHORZ, NULL, N_("Clear") },
#define BACKGROUNDPOSX (11)
	{ PD_FLOAT, &thisLayout.props.backgroundPos.x, "backgroundposX", PDO_DIM | PDO_NOPSHUPD | PDO_DRAW, &rN_9999999, N_("Background PosX,Y"), 0, I2VP(CHANGE_BACKGROUND) },
#define BACKGROUNDPOSY (12)
	{ PD_FLOAT, &thisLayout.props.backgroundPos.y, "backgroundposY", PDO_DIM | PDO_NOPSHUPD | PDO_DRAW | PDO_DLGHORZ, &rN_9999999, NULL, 0, I2VP(CHANGE_BACKGROUND) },
#define BACKGROUNDWIDTH (13)
	{ PD_FLOAT, &thisLayout.props.backgroundSize, "backgroundWidth", PDO_DIM | PDO_NOPSHUPD | PDO_DRAW, &r0_9999999, N_("Background Size"), 0, I2VP(CHANGE_BACKGROUND) },
#define BACKGROUNDSCREEN (14)
	{ PD_LONG, &thisLayout.props.backgroundScreen, "backgroundScreen", PDO_NOPSHUPD | PDO_DRAW, &i0_100, N_("Background Screen %"), 0, I2VP(CHANGE_BACKGROUND) },
#define BACKGROUNDANGLE (15)
	{ PD_FLOAT, &thisLayout.props.backgroundAngle, "backgroundAngle", PDO_NOPSHUPD | PDO_DRAW | PDO_DLGBOXEND, &r360_360, N_("Background Angle"), 0, I2VP(CHANGE_BACKGROUND) },
	{ PD_MESSAGE, N_("Named Settings File"), NULL, PDO_DLGRESETMARGIN, I2VP(180) },
	{ PD_BUTTON, SettingsWrite, "write",  PDO_DLGHORZ, 0, N_("Write"), 0, I2VP(0) },
	{ PD_BUTTON, SettingsRead, "read", PDO_DLGHORZ | PDO_DLGBOXEND, 0, N_("Read"), 0, I2VP(0) }
};

static paramGroup_t layoutPG = { "layout", PGO_RECORD | PGO_PREFMISC, layoutPLs, COUNT( layoutPLs ) };

/**
 * @brief Handle the Layout changes, setting the values of changed items from dialog.
*/
static void ChangeLayout()
{

	long changes;

	changes = GetChanges(&layoutPG);

	/* [mf Nov. 15, 2005] Get the gauge/scale settings */
	if (changes & CHANGE_SCALE) {
		SetScaleGauge(thisLayout.props.curScaleDescInx, thisLayout.props.curGaugeInx);
		file_changed = TRUE;
	}

	/* [mf Nov. 15, 2005] end */

	if (changes & CHANGE_MAP) {
		SetRoomSize(thisLayout.props.roomSize);
		file_changed = TRUE;
	}

	DoChangeNotification(changes);

	if (changes & CHANGE_LIMITS) {
		char prefString[30];
		// now set the minimum track radius
		sprintf(prefString, "minTrackRadius-%s", curScaleName);
		wPrefSetFloat("misc", prefString, thisLayout.props.minTrackRadius);
		file_changed = TRUE;
	}

	if (changes & CHANGE_BACKGROUND) {

		LayoutBackGroundSave();
		file_changed = TRUE;
	}
}

/**
* Apply the changes entered to settings
*
* \param unused IN unused
*/

static void LayoutOk(void * unused)
{
	ChangeLayout();
	if(file_changed) {
		LayoutBackGroundSave();
		SetFileChanged();
		file_changed = FALSE;
	}

	free(thisLayout.copyOfLayoutProps);
	DynStringFree(&thisLayout.copyBackgroundFileName);

	wHide(layoutW);

	MainLayout( TRUE, TRUE );
}



/**
* Discard the changes entered and replace with earlier values
*
* \param unused IN unused
*/

static void LayoutCancel(struct wWin_t *unused)
{
	// thisLayout.props = *(thisLayout.copyOfLayoutProps);
	char* curr = DynStringToCStr(&thisLayout.backgroundFileName);
	char* prev = DynStringToCStr(&thisLayout.copyBackgroundFileName);
	if ((curr == NULL && prev != NULL) || (curr != NULL && prev == NULL)
	    || (strcmp(curr, prev) != 0) ) {
		if (DynStringSize(&thisLayout.backgroundFileName)) {
			DynStringClear(&thisLayout.backgroundFileName);
		}
		size_t bgSize = DynStringSize(&thisLayout.copyBackgroundFileName);
		if ( bgSize ) {
			DynStringMalloc(&thisLayout.backgroundFileName, bgSize + 1);
			char* fileName = DynStringToCStr(&thisLayout.copyBackgroundFileName);
			DynStringCatCStr(&thisLayout.backgroundFileName, fileName);
			haveBackground = TRUE;
			wDrawSetBackground(  mainD.d, fileName, NULL);
		} else {
			haveBackground = FALSE;
			wDrawSetBackground(  mainD.d, NULL, NULL);
		}
		SetName();
		// ChangeLayout();
	}

	ParamLoadControls(&layoutPG);

	free(thisLayout.copyOfLayoutProps);
	DynStringFree(&thisLayout.copyBackgroundFileName);

	wHide(layoutW);

	MainLayout( TRUE, TRUE );
}

/**
 * @brief Reload Layout parameters if changes
 * @param changes
*/
static void LayoutChange(long changes)
{
	if (changes & (CHANGE_SCALE | CHANGE_UNITS | CHANGE_BACKGROUND))
		if (layoutW != NULL && wWinIsVisible(layoutW)) {
			ParamLoadControls(&layoutPG);
		}
}

/**
 * @brief Initialize Layout dialog
 * @param unused
*/
void DoLayout(void * unused)
{
	CHECK(BACKGROUNDFILEENTRY == BACKGROUNDFILECHECK);

	SetLayoutRoomSize(mapD.size);

	if (layoutW == NULL) {
		layoutW = ParamCreateDialog(&layoutPG, MakeWindowTitle(_("Layout Options")),
		                            _("Ok"), LayoutOk, LayoutCancel, TRUE, NULL, 0, LayoutDlgUpdate);
		LoadScaleList((wList_p)layoutPLs[4].control);
	}

	ParamControlActive(&layoutPG, BACKGROUNDFILEENTRY, FALSE);

	LoadGaugeList((wList_p)layoutPLs[5].control,
	              thisLayout.props.curScaleDescInx); /* set correct gauge list here */
	thisLayout.copyOfLayoutProps = malloc(sizeof(struct sLayoutProps));

	if (!thisLayout.copyOfLayoutProps) {
		exit(1);
	}
	SetName();
	*(thisLayout.copyOfLayoutProps) = thisLayout.props;

	char* curr = DynStringToCStr(&thisLayout.backgroundFileName);
	size_t bgSize = DynStringSize(&thisLayout.backgroundFileName);
	if ( bgSize ) {
		DynStringMalloc(&thisLayout.copyBackgroundFileName, bgSize + 1);
		DynStringCatCStr(&thisLayout.copyBackgroundFileName, curr);
	}

	ParamLoadControls(&layoutPG);
	wShow(layoutW);
}

/**
 * @brief Callback for Menu and Shortcut key to open Layout Dialog.
 * @param void
 * @return Address of layout dialog (DoLayout)
*/
EXPORT addButtonCallBack_t LayoutInit(void)
{
	ParamRegister(&layoutPG);
	RegisterChangeNotification(LayoutChange);
	layout_p = layoutPLs;
	layout_pg_p = &layoutPG;
	return &DoLayout;
}

/**
* Update the dialog when scale was changed. The list of possible gauges for the selected scale is
* updated and the first entry is selected (usually standard gauge). After this the minimum gauge
* is set from the preferences.
*
* \param pg IN dialog
* \param inx IN changed entry field
* \param valueP IN new value
*/

static void
LayoutDlgUpdate(
        paramGroup_p pg,
        int inx,
        void * valueP)
{
	/* did the scale change ? */
	if (inx == SCALEINX) {
		char prefString[130];
		char scaleDesc[100];

		LoadGaugeList((wList_p)layoutPLs[GAUGEINX].control, *((int *)valueP));
		// set the first entry as default, usually the standard gauge for a scale
		wListSetIndex((wList_p)layoutPLs[GAUGEINX].control, 0);

		// get the minimum radius
		// get the selected scale first
		wListGetValues((wList_p)layoutPLs[SCALEINX].control, scaleDesc, 99, NULL, NULL);
		strtok(scaleDesc, " ");

		// now get the minimum track radius
		sprintf(prefString, "minTrackRadius-%s", scaleDesc);
		wPrefGetFloat("misc", prefString, &thisLayout.props.minTrackRadius, 0.0);

		// put the scale's minimum value into the dialog
		wStringSetValue((wString_p)layoutPLs[MINRADIUSENTRY].control,
		                FormatDistance(thisLayout.props.minTrackRadius));
	}
	if (inx == BACKGROUNDFILEENTRY) {
		SetName();
		MainRedraw();
	}
	if (inx == BACKGROUNDPOSX) {
		coOrd pos;
		pos.x = *(double *)valueP;
		pos.y = GetLayoutBackGroundPos().y;
		SetLayoutBackGroundPos(pos);
		MainRedraw();
	}
	if (inx == BACKGROUNDPOSY) {
		coOrd pos;
		pos.y = *(double *)valueP;
		pos.x = GetLayoutBackGroundPos().x;
		SetLayoutBackGroundPos(pos);
		MainRedraw();
	}
	if (inx == BACKGROUNDWIDTH) {
		SetLayoutBackGroundSize(*(double *)valueP);
		MainRedraw();
	}
	if (inx == BACKGROUNDSCREEN) {
		SetLayoutBackGroundScreen(*(int *)valueP);
		MainRedraw();
	}
	if (inx == BACKGROUNDANGLE) {

		ANGLE_T angle = NormalizeAngle(*(double *)valueP);
		wStringSetValue((wString_p)layoutPLs[BACKGROUNDANGLE].control,
		                FormatFloat(angle));
		SetLayoutBackGroundAngle(angle);
		MainRedraw();
	}

}
/**
 * Load Background Options from Saved Parms section [layout]
*/
void
LayoutBackGroundLoad(void)
{
	SetLayoutBackGroundFullPath(wPrefGetString("layout", "BackgroundPath"));

	wPrefGetFloat("layout", "BackgroundPosX", &thisLayout.props.backgroundPos.x,
	              0.0);
	wPrefGetFloat("layout", "BackgroundPosY", &thisLayout.props.backgroundPos.y,
	              0.0);
	wPrefGetFloat("layout", "BackgroundAngle", &thisLayout.props.backgroundAngle,
	              0.0);
	long screen_long;
	wPrefGetInteger("layout", "BackgroundScreen", &screen_long, 0L);
	thisLayout.props.backgroundScreen = screen_long;
	wPrefGetFloat("layout", "BackgroundSize", &thisLayout.props.backgroundSize,
	              0.0);
}

static wBool_t inited;

/**
 * @brief Either Clear Background Parms or (if the first time called) Load from Saved Parms
 * @param clear TRUE: Background is cleared, FALSE: Background is loaded (if defined)
*/
void
LayoutBackGroundInit(BOOL_T clear)
{
	if (clear) {
		SetLayoutBackGroundFullPath(noname);
		SetLayoutBackGroundPos(zero);
		SetLayoutBackGroundAngle(0.0);
		SetLayoutBackGroundScreen(0);
		SetLayoutBackGroundSize(0.0);
		LayoutBackGroundSave();
	} else {      //First Time and not "Clear"
		inited = TRUE;
		LayoutBackGroundLoad();
	}
	char * str = GetLayoutBackGroundFullPath();
	if (str && str[0]) {
		haveBackground = true;
		if (!LoadBackGroundImage()) {    //Failed -> Wipe Out
			SetLayoutBackGroundFullPath(noname);
			SetLayoutBackGroundPos(zero);
			SetLayoutBackGroundAngle(0.0);
			SetLayoutBackGroundScreen(0);
			SetLayoutBackGroundSize(0.0);
			LayoutBackGroundSave();
			haveBackground = false;
		}
	} else {
		haveBackground = false;
	}

	if ( haveBackground ) {
		char *error;
		if (wDrawSetBackground(  mainD.d, str, &error) == -1) {
			NoticeMessage(_("Unable to load Image File - %s"),_("Ok"),NULL,error);
			haveBackground = false;
		}
	} else {
		wDrawSetBackground(  mainD.d, NULL, NULL);
	}
	SetName();
}

/**
 * Read the settings defined in the file from sections [misc] and [DialogItem]
 * @param files Number of files chosen
 * @param fileName Filename(s) chosen. Only the first is used
 * @param data Not used
 * @return TRUE (always)
*/
EXPORT int DoSettingsRead(
        int files,
        char ** fileName,
        void * data )
{
	char * pref;
	CHECK( files == 1 );
	if (fileName == NULL) { wPrefsLoad(NULL); }
	else { wPrefsLoad(fileName[0]); }
	// get the preferred scale from the new configuration file
	pref = wPrefGetString("misc", "scale");
	if (pref) {
		char buffer[STR_SHORT_SIZE];
		strcpy(buffer, pref);
		DoSetScale(buffer);
	}
	//Get command options
	wPrefGetInteger("DialogItem","cmdopt-preselect",&preSelect,preSelect);
	wPrefGetInteger("DialogItem","cmdopt-rightclickmode",&rightClickMode,
	                rightClickMode);
	wPrefGetInteger("DialogItem","cmdopt-selectmode",&selectMode,selectMode);
	wPrefGetInteger("DialogItem","cmdopt-selectzero",&selectZero,selectZero);

	//Get Toolbar showing
	ToolbarLoadConfig();

	LayoutBackGroundInit( FALSE );

	//Redraw the screen to reflect changes
	MainProc( mainW, wResize_e, NULL, NULL );
	return TRUE;
}

static struct wFilSel_t * settingsRead_fs;

/**
 * @brief Read button in Layout. File Open dialog to read a Settings file (*.xset)
 * @param void
 */
static void SettingsRead( void )
{
	if (settingsRead_fs == NULL)
		settingsRead_fs = wFilSelCreate( mainW, FS_LOAD, 0, _("Read Settings"),
		                                 _("Settings File (*.xset)|*.xset"), DoSettingsRead, NULL );
	bExample = FALSE;
	wFilSelect( settingsRead_fs, wGetAppWorkDir());
}

/**
 * @brief Write the settings file (after Dialog)
 * @param files Number of Files selected (must be 1)
 * @param fileName The selected Filename
 * @param data Not used
 * @return TRUE (always)
*/
static int DoSettingsWrite(
        int files,
        char ** fileName,
        void * data )
{
	CHECK( fileName != NULL );
	CHECK( files == 1 );
	wPrefFlush(fileName[0]);
	return TRUE;
}

static struct wFilSel_t * settingsWrite_fs;

/**
 * @brief Write button in Layout. File Save dialog for a settings file (*.xset)
 * @param void
*/
static void SettingsWrite( void  )
{
	ChangeLayout();
	if ( settingsWrite_fs == NULL )
		settingsWrite_fs  = wFilSelCreate( mainW, FS_UPDATE, 0, _("Write Settings"),
		                                   _("Settings File (*.xset)|*.xset"), DoSettingsWrite, NULL );
	wFilSelect( settingsWrite_fs, wGetAppWorkDir());
}



