/** \file dxfimport.c
 * Import DXF file to XTI
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

#include <cJSON.h>

// #include "archive.h"
#include "common.h"
#include "compound.h"
#include "cselect.h"
#include "cundo.h"
#include "custom.h"
#include "directory.h"
// #include "draw.h"
#include "fileio.h"
#include "fcntl.h"
// #include "layout.h"
// #include "manifest.h"
#include "misc.h"
#include "param.h"
// #include "include/paramfile.h"
// #include "include/paramfilelist.h"
#include "paths.h"
#include "include/stringxtc.h"
// #include "track.h"
#include "version.h"
#include "dynstring.h"
#include "common-ui.h"
// #include "ctrain.h"
#include "form.h"

#ifdef UTFCONVERT
#include "include/utf8convert.h"
#endif // UTFCONVERT

// int wFilSelect(struct wFilSel_t* fs, const char* dirName);

/*******************************************************************************
 *
 * Import DXF Dialog
 *
 */

static struct wFilSel_t* importDxf_fs;
static char* nameOfFile;

static void ImportDxfFileSel(void* unused);
static int ImportDxf(int cnt, char** fileName, void* data);
static void ProcessDxfFile(char** filePath, char* fileName,
                           BOOL_T complain);

static int importDxfTrack = 0;
static int importDxfXti = 0;

static char* importDxfTrackLabels[] = { N_("Layer 0 Track"), NULL };
static char* importDxfXtiLabels[] = { N_("No Import"), N_("Import XTI"), N_("Import Module"), NULL };

static paramData_t importDxfPLs[] = {
	/*0*/ { PD_TOGGLE, &importDxfTrack, "track", PDO_NOPREF, &importDxfTrackLabels, NULL, BC_NOBORDER },
	/*1*/ { PD_RADIO, &importDxfXti, "xti", PDO_NOPREF, &importDxfXtiLabels, NULL }
};
static paramGroup_t importDxfPG = { "importDxf", 0, importDxfPLs, COUNT(importDxfPLs) };
static wWin_p importDxfW;

/* Called from File menu */
EXPORT void DoImportDxf(void* unused)
{
	if (!importDxfW) {
		FormRegister(&importDxfPG);
		importDxfW = FormCreateDialog( &importDxfPG,
		                               MakeWindowTitle(_("Import Dxf")),
		                               _("Ok"), ImportDxfFileSel,
		                               NULL, FormCancel_Current,
		                               TRUE, 0, NULL);
		// blockD.dpi = mainD.dpi;
	}
	FormLoadControls(&importDxfPG);
	wShow(importDxfW);
}

/* Called from Param Dialog above */
static void ImportDxfFileSel(void* unused)
{
	wHide(importDxfW);

	if (importDxf_fs == NULL)
		importDxf_fs = wFilSelCreate(mainW, FS_LOAD, 0,
		                             _("Import Dxf"),
		                             sDxfFilePattern, ImportDxf, NULL);

	wFilSelect(importDxf_fs, GetCurrentPath( LAYOUTPATHKEY ));
}

/* Called from File Select above */
static int ImportDxf(
        int cnt,
        char** fileName,
        void* data)
{
	long paramVersionOld = paramVersion;

	CHECK(fileName != NULL);
	CHECK(cnt == 1);

	nameOfFile = FindFilename(fileName[0]);
	paramVersion = -1;
	wSetCursor(mainD.d, wCursorWait);
	Reset();
	SetAllTrackSelect(FALSE);

	ImportStart();
	UndoStart(_("Import Dxf"), "importDxf");
	useCurrentLayer = TRUE;

	ProcessDxfFile(fileName, nameOfFile, true);

	ImportEnd(zero, TRUE, FALSE);

	/*DoRedraw();*/
	EnableCommands();
	wSetCursor(mainD.d, defaultCursor);
	paramVersion = paramVersionOld;
	DoCommandB(I2VP(selectCmdInx));
	SelectRecount();

	return TRUE;
}

char dxfCode[20];
char dxfValue[50];

// Hex values (RGB) for all the 256 ACI Color indexes
static long aci[256] = {
	0x000000, 0xFF0000, 0xFFFF00, 0x00FF00, 0x00FFFF, 0x0000FF, 0xFF00FF, 0xFFFFFF, //007
	0x414141, 0x808080, 0xFF0000, 0xFFAAAA, 0xBD0000, 0xBD7E7E, 0x810000, 0x815656, //015
	0x680000, 0x684545, 0x4F0000, 0x4F3535, 0xFF3F00, 0xFFBFAA, 0xBD2E00, 0xBD8D7E, //023
	0x811F00, 0x816056, 0x681900, 0x684E45, 0x4F1300, 0x4F3B35, 0xFF7F00, 0xFFD4AA, //031
	0xBD5E00, 0xBD9D7E, 0x814000, 0x816B56, 0x683400, 0x685645, 0x4F2700, 0x4F4235, //039
	0xFFBF00, 0xFFEAAA, 0xBD8D00, 0xBDAD7E, 0x816000, 0x817656, 0x684E00, 0x685F45, //047
	0x4F3B00, 0x4F4935, 0xFFFF00, 0xFFFFAA, 0xBDBD00, 0xBDBD7E, 0x818100, 0x818156, //055
	0x686800, 0x686845, 0x4F4F00, 0x4F4F35, 0xBFFF00, 0xEAFFAA, 0x8DBD00, 0xADBD7E, //063
	0x608100, 0x768156, 0x4E6800, 0x5F6845, 0x3B4F00, 0x494F35, 0x7FFF00, 0xD4FFAA, //071
	0x5EBD00, 0x9DBD7E, 0x408100, 0x6B8156, 0x346800, 0x566845, 0x274F00, 0x424F35, //079
	0x3FFF00, 0xBFFFAA, 0x2EBD00, 0x8DBD7E, 0x1F8100, 0x608156, 0x196800, 0x4E6845, //087
	0x134F00, 0x3B4F35, 0x00FF00, 0xAAFFAA, 0x00BD00, 0x7EBD7E, 0x008100, 0x568156, //095
	0x006800, 0x456845, 0x004F00, 0x354F35, 0x00FF3F, 0xAAFFBF, 0x00BD2E, 0x7EBD8D, //103
	0x00811F, 0x568160, 0x006819, 0x45684E, 0x004F13, 0x354F3B, 0x00FF7F, 0xAAFFD4, //111
	0x00BD5E, 0x7EBD9D, 0x008140, 0x56816B, 0x006834, 0x456856, 0x004F27, 0x354F42, //119
	0x00FFBF, 0xAAFFEA, 0x00BD8D, 0x7EBDAD, 0x008160, 0x568176, 0x00684E, 0x45685F, //127
	0x004F3B, 0x354F49, 0x00FFFF, 0xAAFFFF, 0x00BDBD, 0x7EBDBD, 0x008181, 0x568181, //135
	0x006868, 0x456868, 0x004F4F, 0x354F4F, 0x00BFFF, 0xAAEAFF, 0x008DBD, 0x7EADBD, //143
	0x006081, 0x567681, 0x004E68, 0x455F68, 0x003B4F, 0x35494F, 0x007FFF, 0xAAD4FF, //151
	0x005EBD, 0x7E9DBD, 0x004081, 0x566B81, 0x003468, 0x455668, 0x00274F, 0x35424F, //159
	0x003FFF, 0xAABFFF, 0x002EBD, 0x7E8DBD, 0x001F81, 0x566081, 0x001968, 0x454E68, //167
	0x00134F, 0x353B4F, 0x0000FF, 0xAAAAFF, 0x0000BD, 0x7E7EBD, 0x000081, 0x565681, //175
	0x000068, 0x454568, 0x00004F, 0x35354F, 0x3F00FF, 0xBFAAFF, 0x2E00BD, 0x8D7EBD, //183
	0x1F0081, 0x605681, 0x190068, 0x4E4568, 0x13004F, 0x3B354F, 0x7F00FF, 0xD4AAFF, //191
	0x5E00BD, 0x9D7EBD, 0x400081, 0x6B5681, 0x340068, 0x564568, 0x27004F, 0x42354F, //199
	0xBF00FF, 0xEAAAFF, 0x8D00BD, 0xAD7EBD, 0x600081, 0x765681, 0x4E0068, 0x5F4568, //207
	0x3B004F, 0x49354F, 0xFF00FF, 0xFFAAFF, 0xBD00BD, 0xBD7EBD, 0x810081, 0x815681, //215
	0x680068, 0x684568, 0x4F004F, 0x4F354F, 0xFF00BF, 0xFFAAEA, 0xBD008D, 0xBD7EAD, //223
	0x810060, 0x815676, 0x68004E, 0x68455F, 0x4F003B, 0x4F3549, 0xFF007F, 0xFFAAD4, //231
	0xBD005E, 0xBD7E9D, 0x810040, 0x81566B, 0x680034, 0x684556, 0x4F0027, 0x4F3542, //239
	0xFF003F, 0xFFAABF, 0xBD002E, 0xBD7E8D, 0x81001F, 0x815660, 0x680019, 0x68454E, //247
	0x4F0013, 0x4F353B, 0x333333, 0x505050, 0x696969, 0x828282, 0xBEBEBE, 0xFFFFFF  //255
};

// Read the code/value pair that DXF uses
static bool ReadDxfPair(FILE* dxfFile)
{
	memset(dxfCode, 0, sizeof(dxfCode));
	memset(dxfValue, 0, sizeof(dxfValue));

	if (fgets(dxfCode, sizeof dxfCode, dxfFile) != NULL) {
		if (fgets(dxfValue, sizeof dxfValue, dxfFile) != NULL) {
			return true;
		}
	}
	return false;
}

// Get the line type code from the DXF text
static wDrawLineType_e dxfLineType(char dxfValue[50])
{
	wDrawLineType_e lineType = wDrawLineSolid;

	if (strncmp(dxfValue, "continuous", 10) == 0) {
		lineType = wDrawLineSolid;
	} else if (strncmp(dxfValue, "divide", 6) == 0) {
		lineType = wDrawLineDashDotDot;
	} else if (strncmp(dxfValue, "dashdot", 7) == 0) {
		lineType = wDrawLineDashDot;
	} else if (strncmp(dxfValue, "dot", 3) == 0) {
		lineType = wDrawLineDot;
	} else if (strncmp(dxfValue, "dashed", 6) == 0) {
		lineType = wDrawLineDash;
	} else if ((strncmp(dxfValue, "border", 6) == 0) ||
	           (strncmp(dxfValue, "center", 6) == 0)) {
		lineType = wDrawLineCenter;
	}

	return lineType;
}

// These differ from the XTC version as it changes
// the angle from DXF to XTC at the same time.
static double toDegrees(double a)
{
	return 90.0 + a * 180.0 / M_PI;
}

static double toRadians(double a)
{
	return (90.0 - a) * M_PI / 180.0;
}

#define MAX_DXF_LAYER 50		// Layer list size
#define DXF_ENDPT_ALLOC 64		// End point list
#define DXF_OUTPUT_ALLOC 64		// Amount to grow the output

int DxfEndPtAlloc;				// Current size
int DxfEndPtCount;
struct sEndPt* DxfEndPt;		// End points

int DxfOutputAlloc;				// Current size
int DxfOutputCount;
char** DxfOutput;				// Track Output lines

// Allocate memory for a line of output and assign to output
// Reallocate array space if needed
static BOOL_T dxfAddOutput(char tmp[])
{
	size_t len = strlen(tmp) + 1;
	char* outpt = MyMalloc(len * sizeof(char));

	strncpy(outpt, tmp, len);

	if (DxfOutputCount == DxfOutputAlloc) {
		DxfOutputAlloc += DXF_OUTPUT_ALLOC;
		int mSize = DxfOutputAlloc * sizeof(char*);
		DxfOutput = MyRealloc(DxfOutput, mSize);
	}
	DxfOutput[DxfOutputCount++] = outpt;

	return TRUE;
}

struct sEndPt {
	int entity;
	int line;
	coOrd coord;
	double angle;
};

// Allocate memory for an End Pt
// Reallocate array space if needed
static BOOL_T dxfAddEndPt(struct sEndPt endPt)
{
	size_t len = sizeof(endPt);
	// char* endpt = MyMalloc(len);

	if (DxfEndPtCount == DxfEndPtAlloc) {
		DxfEndPtAlloc += DXF_ENDPT_ALLOC;
		size_t eSize = DxfEndPtAlloc * len;
		DxfEndPt = MyRealloc(DxfEndPt, eSize);
	}
	memcpy(&DxfEndPt[DxfEndPtCount], &endPt, len);
	DxfEndPtCount++;

	return TRUE;
}

#define TMP_SIZE 120	// tmp buffer size
#define DXF_DEBUG TRUE

static void dxfDebugMessage(int ok, const char* fileName)
{
	if (ok < 0) {
		NoticeMessage(MSG_DXF_LINE_ERROR, _("Ok"), NULL, fileName);
	} else if (ok > TMP_SIZE) {
		NoticeMessage(MSG_DXF_LINE_LEN, _("Ok"), NULL, fileName, ok, TMP_SIZE);
	}
}

// The main ReadDxfFile function
static void ProcessDxfFile(
        char** pathName,
        char* fileName,
        BOOL_T complain)
{
	FILE* dxfFile;
	FILE* xtiFile;

	time_t clock;

	int count;

	char tmp[TMP_SIZE];
	int ok;

	char dxfSection[20];
	char dxfGroup[20];
	enum enumSection {
		eNone,
		eLine,
		eArc,
		eCircle,
		ePoly,
		eText
	};
	enum enumSection eSection;

	DxfOutputAlloc = DXF_OUTPUT_ALLOC;
	DxfOutputCount = 0;
	int mSize = DxfOutputAlloc * sizeof(char*);
	DxfOutput = MyMalloc(mSize);


	struct sEndPt endPt;

	DxfEndPtAlloc = DXF_ENDPT_ALLOC;
	DxfEndPtCount = 0;
	int eSize = DxfEndPtAlloc * sizeof(endPt);
	DxfEndPt = MyMalloc(eSize);

	bool inEntities = false;
	bool inLayers = false;

	struct sLayer {
		int line;
		char* name;
		int color;
		int lineType;
		double thick;
	};
	struct sLayer layer[MAX_DXF_LAYER];


	int layerCount = 0;
	int entityCount = 0;

	char dxfLayer[50];

	int color = 0;
	int colorRGB = -1;
	wDrawLineType_e lineType;
	double thick = 0.0;
	double radius = 0.0;
	double startAngle = 0.0;
	double endAngle = 0.0;
	int closed = 0;
	double x1 = 0.0;
	double y1 = 0.0;
	double x2 = 0.0;
	double y2 = 0.0;
	int layerIdx = 0;
	int visibility = 2;

	double vrt_x[20];
	double vrt_y[20];
	int vertices = 0;

	bool dxfDirty = false;

	memset(dxfSection, 0, sizeof(dxfSection));

	dxfFile = fopen(pathName[0], "r");
	if (dxfFile == NULL) {
		if (complain) {
			NoticeMessage(MSG_OPEN_FAIL, _("Continue"), NULL, sProdName, pathName,
			              strerror(errno));
		}
		return;
	}

	// ParamSetInReadTracks(TRUE);
	SetCLocale();
	// checkPtFileNameBackup = NULL;

	// Header
	time(&clock);
	sprintf(tmp, "#%s Version: %s, Date: %s", sProdName, sVersion, ctime(&clock));
	dxfAddOutput(tmp);
	sprintf(tmp, "VERSION %d %s\n", iParamVersion, PARAMVERSIONVERSION);
	dxfAddOutput(tmp);

	eSection = eNone;

	count = 0;
	while (dxfFile
	       && ReadDxfPair(dxfFile)) {
		count += 2;
		// BOOL_T old_skip = skip;
		// skip = FALSE;
		if (count % 10 == 0) {
			InfoMessage("%d", count);
			wFlush();
		}

		Stripcr(dxfCode);
		Stripcr(dxfValue);

		if (strncmp(dxfCode, "  0", 3) == 0) {
			// Write previous entity
			if (dxfDirty) {

				// Write the layer data into a list
				if (inLayers) {
					if (layerCount < MAX_DXF_LAYER) {
						layer[layerCount].line = count;
						size_t len = strlen(dxfGroup);
						layer[layerCount].name = MyMalloc((len + 1) * sizeof(char));
						strncpy(layer[layerCount].name, dxfGroup, len);
						layer[layerCount].color = (colorRGB >= 0 ? colorRGB : (color == 7 ? 0 :
						                           (color > 0 && color < 256 ? color : 0)));
						layer[layerCount].lineType = lineType;
						layer[layerCount].thick = thick;

						layerCount++;
					} else {
						NoticeMessage(MSG_TOO_MANY_LAYERS, _("Ok"), NULL, fileName, MAX_DXF_LAYER);
						return;
					}
				}

				// Write the Entity data into the list
				else if (inEntities) {
					BOOL_T isTrack = (strncmp(dxfLayer, layer[0].name, sizeof dxfLayer) == 0);

					layerIdx = curLayer;

					if (color < 0 || lineType < 0 || thick < 0) {
						for (int i = 0; i < layerCount; i++) {
							if (strncmp(dxfLayer, layer[i].name, sizeof dxfLayer) == 0) {
								if (color < 0) {
									color = layer[i].color;
								}
								if (lineType < 0) {
									lineType = layer[i].lineType;
								}
								if (thick < 0) {
									thick = layer[i].thick;
								}
							}
						}
					}

					// Create the entity. Start numbering with 1
					entityCount++;

					int useColor = (colorRGB >= 0 ? colorRGB : color);

					if (eSection == eLine) {
						// Save Entity data
						if (isTrack && importDxfTrack == 1) {
							// STRAIGHT index layer line-width 0 0 scale descshow&visibility&no_ties&bridge&roadbed Desc-x Desc-y
							//STRAIGHT 1 1 0 0 0 HO 2 0.000000 0.000000
							//	E4 0.000000 0.000000 270.000000 0 0.0 0.0 0.0 0.0 0 0 0 0.000000
							//	T4 2 21.000000 0.000000 90.000000 0 0.0 0.0 0.0 0.0 0 0 0 0.000000
							//	END$SEGS
							ok = snprintf(tmp, TMP_SIZE, "%s %d %d %d 0 0 %s %d %.6f %.6f",
							              "STRAIGHT", entityCount, layerIdx, 0, curScaleName, visibility, 0.0, 0.0);
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							// End points
							double a1 = NormalizeAngle(-toDegrees(atan2((y2 - y1), (x2 - x1))));
							double a2 = NormalizeAngle(180 + a1);

							ok = snprintf(tmp, TMP_SIZE,
							              "\t%s %.6f %.6f %.6f 0 0.000000 0.000000 0.000000 0.000000 0 0 0 0.000000",
							              "E4", x1, y1, a1);

							endPt.entity = entityCount;
							endPt.line = DxfOutputCount;
							endPt.coord.x = x1; endPt.coord.y = y1;
							endPt.angle = a1;
							dxfAddEndPt(endPt);

							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							ok = snprintf(tmp, TMP_SIZE,
							              "\t%s %.6f %.6f %.6f 0 0.000000 0.000000 0.000000 0.000000 0 0 0 0.000000",
							              "E4", x2, y2, a2);

							endPt.entity = entityCount;
							endPt.line = DxfOutputCount;
							endPt.coord.x = x2; endPt.coord.y = y2;
							endPt.angle = a2;
							dxfAddEndPt(endPt);

							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							ok = snprintf(tmp, TMP_SIZE, "\t%s", END_SEGS);
							dxfAddOutput(tmp);

						} else {
							// DRAW 1 0 0 0 0 -2.000000 -4.500000 0 0.000000
							ok = snprintf(tmp, TMP_SIZE, "%s %d %d %d %d 0 %.6f %.6f 0 %.6f",
							              "DRAW", entityCount, layerIdx, lineType, 0, 0.0, 0.0, 0.0);
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							ok = snprintf(tmp, TMP_SIZE, "\t%s %d %f %f %f 0 %f %f 0",
							              "L3", useColor, thick, x1, y1, x2, y2);
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							ok = snprintf(tmp, TMP_SIZE, "\t%s", END_SEGS);
							dxfAddOutput(tmp);
						}

					} else if (eSection == eCircle) {
						// DRAW 1 0 0 0 0 -2.000000 -4.500000 0 0.000000
						ok = snprintf(tmp, TMP_SIZE, "%s %d %d %d %d 0 %.6f %.6f 0 %.6f",
						              "DRAW", entityCount, layerIdx, lineType, 0, 0.0, 0.0, 0.0);
						dxfAddOutput(tmp);
						dxfDebugMessage(ok, fileName);

						ok = snprintf(tmp, TMP_SIZE, "\t%s %d %.6f %.6f %.6f %.6f 0",
						              "G3", useColor, thick, radius, x1, y1);
						dxfAddOutput(tmp);
						dxfDebugMessage(ok, fileName);

						ok = snprintf(tmp, TMP_SIZE, "\t%s", END_SEGS);
						dxfAddOutput(tmp);

					} else if (eSection == eArc) {
						// Save Entity data
						if (isTrack && importDxfTrack == 1) {
							//CURVE index layer line-width 0 0 scale visibility&no_ties&bridge&roadbed center-X centerY 0 radius helix-turns desc-X desc-Y
							//CURVE 2 1 0 0 0 OO 2 0.000000 12.000000 0 12.000000 0 0.000000 0.000000
							//	E4 0.000000 0.000000 270.000000 0 0.0 0.0 0.0 0.0 0 0 0 0.000000
							//	T4 2 21.000000 0.000000 90.000000 0 0.0 0.0 0.0 0.0 0 0 0 0.000000
							//	END$SEGS

							// Save Entity data
							// End points
							//double end = NormalizeAngle(startAngle + endAngle);

							double xEndAngle = NormalizeAngle(-startAngle);
							double xStartAngle = NormalizeAngle(-endAngle);
							//double xCurveStart = NormalizeAngle(90.0 - endAngle);

							double x3 = x1 + radius * sin(toRadians(xStartAngle));
							double y3 = y1 - radius * cos(toRadians(xStartAngle));
							double x4 = x1 + radius * sin(toRadians(xEndAngle));
							double y4 = y1 - radius * cos(toRadians(xEndAngle));
							//double dx = x3 - x4;

							double e3 = NormalizeAngle(xStartAngle);
							double e4 = NormalizeAngle(xEndAngle - 180.0);
							//double xCurveAngle = NormalizeAngle(endAngle - startAngle);

							ok = snprintf(tmp, TMP_SIZE,
							              "%s %d %d %d 0 0 %s %d %.6f %.6f 0 %.6f 0 %.6f %.6f",
							              "CURVE", entityCount, layerIdx, 0, curScaleName, visibility, x1, y1, radius,
							              0.0, 0.0);
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							ok = snprintf(tmp, TMP_SIZE,
							              "\t%s %f %f %f 0 0.000000 0.000000 0.000000 0.000000 0 0 0 0.000000",
							              "E4", x3, y3, e3);

							endPt.entity = entityCount;
							endPt.line = DxfOutputCount;
							endPt.coord.x = x3; endPt.coord.y = y3;
							endPt.angle = e3;
							dxfAddEndPt(endPt);

							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							ok = snprintf(tmp, TMP_SIZE,
							              "\t%s %f %f %f 0 0.000000 0.000000 0.000000 0.000000 0 0 0 0.000000",
							              "E4", x4, y4, e4);

							endPt.entity = entityCount;
							endPt.line = DxfOutputCount;
							endPt.coord.x = x4; endPt.coord.y = y4;
							endPt.angle = e4;
							dxfAddEndPt(endPt);

							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							ok = snprintf(tmp, TMP_SIZE, "\t%s", END_SEGS);
							dxfAddOutput(tmp);

						} else {
							double xStartAngle = NormalizeAngle(90.0 - endAngle);
							double xCurveAngle = endAngle - startAngle;

							// DRAW 1 0 0 0 0 -2.000000 -4.500000 0 0.000000
							ok = snprintf(tmp, TMP_SIZE, "%s %d %d %d %d 0 %.6f %.6f 0 %.6f",
							              "DRAW", entityCount, layerIdx, lineType, 0, 0.0, 0.0, 0.0);
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							ok = snprintf(tmp, TMP_SIZE, "\t%s %d %.6f %.6f %.6f %.6f 0 %.6f %.6f",
							              "A3", useColor, thick, radius, x1, y1, xStartAngle, xCurveAngle);
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							ok = snprintf(tmp, TMP_SIZE, "\t%s", END_SEGS);
							dxfAddOutput(tmp);
						}
					} else

						if (eSection == ePoly) {
							// Save Entity data
							ok = snprintf(tmp, TMP_SIZE, "%s %d %d %d %d 0 %.6f %.6f 0 %.6f",
							              "DRAW", entityCount, layerIdx, lineType, 0, 0.0, 0.0, 0.0);
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							// Detect polylines that are actually cloesd
							if (vertices > 1 && vrt_x[0] == vrt_x[vertices - 1]
							    && vrt_y[0] == vrt_y[vertices - 1]) {
								closed = 1;
								--vertices;
							}

							if (closed == 1)
								ok = snprintf(tmp, TMP_SIZE, "\t%s %d %.6f %d 0",
								              "F4", useColor, thick, vertices);
							else
								ok = snprintf(tmp, TMP_SIZE, "\t%s %d %.6f %d 2",
								              "Y4", useColor, thick, vertices);
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							for (int v = 0; v < vertices; v++) {
								ok = snprintf(tmp, TMP_SIZE, "\t\t%.6f %.6f 0", vrt_x[v], vrt_y[v]);
								dxfAddOutput(tmp);
							}

							ok = snprintf(tmp, TMP_SIZE, "\t%s", END_SEGS);
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

						} else

							if (eSection == eText) {
								// Process Text data
							}
				}

				dxfDirty = false;

				// Make sure previous values are not used
				vertices = 0;
				color = 0;
				colorRGB = -1;
				lineType = 0;
				thick = 0.0;
				radius = 0.0;
				startAngle = 0.0;
				endAngle = 0.0;
				closed = 0;
				x1 = 0.0;
				y1 = 0.0;
				x2 = 0.0;
				y2 = 0.0;
			}

			if (inEntities) {
				if (strncmp(dxfValue, "ARC", 3) == 0) {
					eSection = eArc;
				} else if (strncmp(dxfValue, "CIRCLE", 6) == 0) {
					eSection = eCircle;
				} else if (strncmp(dxfValue, "LINE", 4) == 0) {
					eSection = eLine;
				} else if ((strncmp(dxfValue, "POLYLINE", 8) == 0) ||
				           (strncmp(dxfValue, "LWPOLYLINE", 10) == 0)) {
					eSection = ePoly;
				} else if (strncmp(dxfValue, "MTEXT", 5) == 0) {
					eSection = eText;
				} else {
					eSection = eNone;
				}

				if (eSection != eNone) {
					strncpy(dxfSection, dxfValue, 20);
					dxfDirty = true;
				}
			} else {
				strncpy(dxfSection, dxfValue, sizeof dxfSection);
				strncpy(dxfGroup, "X", 2);

				//if (strncmp(dxfValue, "EOF", 3) == 0)
				//	break;
			}
		}

		if (strncmp(dxfCode, "  2", 3) == 0) {
			if ((inLayers) &&
			    (strncmp(dxfSection, "LAYER", 5) == 0)) {
				strncpy(dxfGroup, dxfValue, sizeof dxfGroup);
				dxfDirty = true;
			}

			// This signals the start of layer definitions
			if ((strncmp(dxfSection, "TABLE", 5) == 0) &&
			    (strncmp(dxfValue, "LAYER", 5) == 0)) {
				inLayers = true;
			}

			// This signals the start of the entity definitions
			if ((strncmp(dxfSection, "SECTION", 7) == 0) &&
			    (strncmp(dxfValue, "ENTITIES", 8) == 0)) {
				strncpy(dxfGroup, dxfValue, sizeof dxfGroup);
				inEntities = true;
				inLayers = false;
			}
		}

		// Mundane data
		if (strncmp(dxfCode, "  6", 3) == 0) {
			for (int i = 0; dxfValue[i]; i++) {
				dxfValue[i] = tolower(dxfValue[i]);
			}
			if (strncmp(dxfValue, "bylayer", 7) == 0) {
				lineType = -1;
			} else {
				lineType = dxfLineType(dxfValue);
			}
		} else if (strncmp(dxfCode, "  8", 3) == 0) {
			strncpy(dxfLayer, dxfValue, sizeof dxfLayer);
		} else if (strncmp(dxfCode, " 10", 3) == 0) {
			if (eSection == ePoly) {
				vrt_x[vertices] = strtod(dxfValue, NULL);
			} else {
				x1 = strtod(dxfValue, NULL);
			}
		} else if (strncmp(dxfCode, " 11", 3) == 0) {
			x2 = strtod(dxfValue, NULL);
		} else if (strncmp(dxfCode, " 20", 3) == 0) {
			if (eSection == ePoly) {
				vrt_y[vertices] = strtod(dxfValue, NULL);
				vertices++;
			} else {
				y1 = strtod(dxfValue, NULL);
			}
		} else if (strncmp(dxfCode, " 21", 3) == 0) {
			y2 = strtod(dxfValue, NULL);
		} else if (strncmp(dxfCode, " 39", 3) == 0) {
			if (strncmp(dxfValue, "bylayer", 7) == 0) {
				thick = -1;
			} else {
				thick = strtod(dxfValue, NULL) / 250;
			}
		} else if (strncmp(dxfCode, " 40", 3) == 0) {
			radius = strtod(dxfValue, NULL);
		} else if (strncmp(dxfCode, " 50", 3) == 0) {
			startAngle = strtod(dxfValue, NULL);
		} else if (strncmp(dxfCode, " 51", 3) == 0) {
			endAngle = strtod(dxfValue, NULL);
		} else if (strncmp(dxfCode, " 62", 3) == 0) {
			long c = strtol(dxfValue, NULL, 10);
			if ((c > 0) && (c < 256)) {
				if (c == 7) {
					color = 0;        // Black
				} else {
					color = aci[c];
				}
			} else {
				color = -1; // ByLayer
			}
		} else if (strncmp(dxfCode, " 70", 3) == 0) {
			closed = strtol(dxfValue, NULL, 10);
		} else if (strncmp(dxfCode, "370", 3) == 0) {
			thick = strtod(dxfValue, NULL);
			if (thick > 0) {
				thick = thick / 250;
			}
		} else if (strncmp(dxfCode, "420", 3) == 0) {
			colorRGB = strtol(dxfValue, NULL, 10);
		}
	}

	ok = snprintf(tmp, TMP_SIZE, "\t%s", END_TRK_FILE);
	dxfAddOutput(tmp);

	// Find and fix connected end points
	int i;
	int j;
	for (i = 0; i < DxfEndPtCount; i++)
		for (j = 0; j < DxfEndPtCount; j++) {

			if ((i != j) && (DxfEndPt[i].entity != DxfEndPt[j].entity)) {
				double a = NormalizeAngle(180.0 + DxfEndPt[i].angle - DxfEndPt[j].angle +
				                          connectAngle / 2.0);
				double d = FindDistance(DxfEndPt[i].coord, DxfEndPt[j].coord);

				if ((d <= connectDistance) && (a <= connectAngle)) {
					// Connect i to j
					int li = DxfEndPt[i].line;
					char substr[TMP_SIZE];
					size_t len = strlen(DxfOutput[li]);
					strncpy(substr, DxfOutput[li], len);
					substr[len] = '\0';
					sprintf(tmp, "\tT4 %d %s", DxfEndPt[j].entity, substr + 4);
					len = strlen(tmp) + 1;
					DxfOutput[li] = MyRealloc(DxfOutput[li], len);
					strncpy(DxfOutput[li], tmp, len);
				}
			}
		}

	if (dxfFile) {
		fclose(dxfFile);
		dxfFile = NULL;
	}

	// Change the extension to create the XTI file
	MakeFullpath(&pathName, workingDir, nameOfFile, NULL);

	MakeFullpath(pathName, workingDir, nameOfFile, NULL);

	char* p = strstr(pathName[0], ".dxf");
	if (p != NULL) {
		memcpy(p, ".xti", 4);
	}

	xtiFile = fopen(pathName[0], "w");
	if (xtiFile == NULL) {
		if (complain) {
			NoticeMessage(MSG_OPEN_FAIL, _("Ok"), NULL, sProdName, pathName[0],
			              strerror(errno));
		}
		return;
	}
	for (int i = 0; i < DxfOutputCount; i++) {
		fprintf(xtiFile, "%s\n", DxfOutput[i]);
	}
	if (xtiFile) {
		fclose(xtiFile);
	}


	// Clean up
	SetUserLocale();

	for (int i = 0; i < DxfOutputCount; i++) {
		MyFree(DxfOutput[i]);
	}
	MyFree(DxfOutput);

	for (int i = 0; i < layerCount; i++) {
		MyFree(layer[i].name);
	}


	// Import the XTI file
	if (importDxfXti >= 1) {

		int saveLayer = curLayer;
		int layer = 0;

		if (importDxfXti == 2) {
			layer = FindUnusedLayer(0);
			if (layer == -1) {
				NoticeMessage(MSG_NO_EMPTY_LAYER, _("Ok"), NULL, fileName, 0);
				return;
			}
			char LayerName[80];
			LayerName[0] = '\0';
			sprintf(LayerName, _("Module - %s"), fileName);
			SetCurrLayer(layer, NULL, 0, NULL, NULL);
			SetLayerName(layer, LayerName);
		}

		ParamSetInReadTracks(TRUE);
		BOOL_T ret = ReadTrackFile(pathName[0], fileName, FALSE, TRUE, TRUE);

		if (ret) {
			if (importDxfXti == 2) { SetLayerModule(layer, TRUE); }
			useCurrentLayer = FALSE;
		}
		SetCurrLayer(saveLayer, NULL, 0, NULL, NULL);
	}

	InfoMessage("%d", count);
	return;
}
