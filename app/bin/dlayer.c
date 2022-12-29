/** \file dlayer.c
 * Functions and dialogs for handling layers.
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis and (C) 2007 Martin Fischer
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

#include "common.h"
#include "cselect.h"
#include "custom.h"
#include "paths.h"
#include "dynstring.h"
#include "fileio.h"
#include "layout.h"
#include "param.h"
#include "track.h"
#include "include/partcatalog.h"
#include "include/stringxtc.h"
#include "common-ui.h"

/*****************************************************************************
 *
 * LAYERS
 *
 */

#define NUM_BUTTONS		(99)
#define LAYERPREF_FROZEN  (1)
#define LAYERPREF_ONMAP	  (2)
#define LAYERPREF_VISIBLE (4)
#define LAYERPREF_MODULE  (8)
#define LAYERPREF_NOBUTTON (16)
#define LAYERPREF_DEFAULT (32)
#define LAYERPREF_SECTION ("Layers")
#define LAYERPREF_NAME 	"name"
#define LAYERPREF_COLOR "color"
#define LAYERPREF_FLAGS "flags"
#define LAYERPREF_SCALEINX "scaleInx"
#define LAYERPREF_SCLDESCINX "sclDescInx"
#define LAYERPREF_GAUGEINX "gaugeInx"
#define LAYERPREF_MINRADIUS "minRadius"
#define LAYERPREF_MAXGRADE "maxGrade"
#define LAYERPREF_TIELENGTH "tieLength"
#define LAYERPREF_TIEWIDTH "tieWidth"
#define LAYERPREF_TIESPACING "tieSpacing"
#define LAYERPREF_LIST "list"
#define LAYERPREF_SETTINGS "settings"

static paramIntegerRange_t r_nocheck = { 0, 1, 100, PDO_NORANGECHECK_LOW | PDO_NORANGECHECK_HIGH };
static paramFloatRange_t r_tieData = { 0.05, 100.0, 100, PDO_NORANGECHECK_LOW | PDO_NORANGECHECK_HIGH };

static paramFloatRange_t r0_10000 = { 0.0, 10000.0 };
static paramFloatRange_t r0_90 = { 0.0, 90.0 };

EXPORT unsigned int maxLayer;

unsigned int curLayer;
long layerCount = 10;
static long newLayerCount = 10;
static unsigned int layerSelected = 0;


static BOOL_T layoutLayerChanged = FALSE;

static wIcon_p show_layer_bmps[NUM_BUTTONS];
static wButton_p layer_btns[NUM_BUTTONS];	/**< layer buttons on toolbar */

/** Layer selector on toolbar */
static wList_p setLayerL;

/** Describe the properties of a layer 
  * Defaults for layout track grade and min radius are in scale.c: SetScale
  */
typedef struct {
	char name[STR_SHORT_SIZE];			/**< Layer name */
	wDrawColor color;					/**< layer color, is an index into a color table */
	BOOL_T useColor;					/**< Use Layer color */
	BOOL_T frozen;						/**< Frozen flag */
	BOOL_T visible;						/**< visible flag */
	BOOL_T onMap;						/**< is layer shown map */
	BOOL_T module;						/**< is layer a module (all or nothing) */
	BOOL_T button_off;					/**< hide button */
	BOOL_T inherit;					    /**< inherit layout defaults */
	SCALEINX_T scaleInx;                /**< scale override */
	SCALEDESCINX_T scaleDescInx;        /**< the scale description */
	GAUGEINX_T gaugeInx;                /**< the gauge desc index */
	DIST_T minTrackRadius;              /**< minimum track radius */
	ANGLE_T maxTrackGrade;              /**< maximum track grade */
	tieData_t tieData;                  /**< tie data structure */
	long objCount;						/**< number of objects on layer */
	dynArr_t layerLinkList;				/**< other layers that show/hide with this one */
	char settingsName[STR_SHORT_SIZE];  /**< name of settings file to load when this is current */
} layer_t;

static layer_t layers[NUM_LAYERS];
static layer_t *layers_save = NULL;

static Catalog * settingsCatalog;


static int oldColorMap[][3] = {
	{ 255, 255, 255 },		/* White */
	{   0,   0,   0 },      /* Black */
	{ 255,   0,   0 },      /* Red */
	{   0, 255,   0 },      /* Green */
	{   0,   0, 255 },      /* Blue */
	{ 255, 255,   0 },      /* Yellow */
	{ 255,   0, 255 },      /* Purple */
	{   0, 255, 255 },      /* Aqua */
	{ 128,   0,   0 },      /* Dk. Red */
	{   0, 128,   0 },      /* Dk. Green */
	{   0,   0, 128 },      /* Dk. Blue */
	{ 128, 128,   0 },      /* Dk. Yellow */
	{ 128,   0, 128 },      /* Dk. Purple */
	{   0, 128, 128 },      /* Dk. Aqua */
	{  65, 105, 225 },      /* Royal Blue */
	{   0, 191, 255 },      /* DeepSkyBlue */
	{ 125, 206, 250 },      /* LightSkyBlue */
	{  70, 130, 180 },      /* Steel Blue */
	{ 176, 224, 230 },      /* Powder Blue */
	{ 127, 255, 212 },      /* Aquamarine */
	{  46, 139,  87 },      /* SeaGreen */
	{ 152, 251, 152 },      /* PaleGreen */
	{ 124, 252,   0 },      /* LawnGreen */
	{  50, 205,  50 },      /* LimeGreen */
	{  34, 139,  34 },      /* ForestGreen */
	{ 255, 215,   0 },      /* Gold */
	{ 188, 143, 143 },      /* RosyBrown */
	{ 139, 69, 19 },        /* SaddleBrown */
	{ 245, 245, 220 },      /* Beige */
	{ 210, 180, 140 },      /* Tan */
	{ 210, 105, 30 },       /* Chocolate */
	{ 165, 42, 42 },        /* Brown */
	{ 255, 165, 0 },        /* Orange */
	{ 255, 127, 80 },       /* Coral */
	{ 255, 99, 71 },        /* Tomato */
	{ 255, 105, 180 },      /* HotPink */
	{ 255, 192, 203 },      /* Pink */
	{ 176, 48, 96 },        /* Maroon */
	{ 238, 130, 238 },      /* Violet */
	{ 160, 32, 240 },       /* Purple */
	{  16,  16,  16 },      /* Gray */
	{  32,  32,  32 },      /* Gray */
	{  48,  48,  48 },      /* Gray */
	{  64,  64,  64 },      /* Gray */
	{  80,  80,  80 },      /* Gray */
	{  96,  96,  96 },      /* Gray */
	{ 112, 112, 122 },      /* Gray */
	{ 128, 128, 128 },      /* Gray */
	{ 144, 144, 144 },      /* Gray */
	{ 160, 160, 160 },      /* Gray */
	{ 176, 176, 176 },      /* Gray */
	{ 192, 192, 192 },      /* Gray */
	{ 208, 208, 208 },      /* Gray */
	{ 224, 224, 224 },      /* Gray */
	{ 240, 240, 240 },      /* Gray */
	{   0,   0,   0 }       /* BlackPixel */
};

static void DoLayerOp(void * data);
void UpdateLayerDlg(unsigned int);

static void InitializeLayers(void LayerInitFunc(void), int newCurrLayer);
static void LayerPrefSave(void);
static void LayerPrefLoad(void);

int IsLayerValid(unsigned int layer)
{
	return (layer <= NUM_LAYERS && layer != -1);
}

BOOL_T GetLayerVisible(unsigned int layer)
{
	if (!IsLayerValid(layer)) {
		return TRUE;
	} else {
		return layers[layer].visible;
	}
}

BOOL_T GetLayerHidden(unsigned int layer)
{
	if (!IsLayerValid(layer)) {
		return TRUE;
	} else {
		return layers[layer].button_off;
	}
}


BOOL_T GetLayerFrozen(unsigned int layer)
{
	if (!IsLayerValid(layer)) {
		return TRUE;
	} else {
		return layers[layer].frozen;
	}
}

BOOL_T GetLayerOnMap(unsigned int layer)
{
	if (!IsLayerValid(layer)) {
		return TRUE;
	} else {
		return layers[layer].onMap;
	}
}

EXPORT BOOL_T GetLayerUseDefault(unsigned int layer)
{
	if (!IsLayerValid(layer)) {
		return TRUE;
	} else {
		return layers[layer].inherit;
	}
}


EXPORT SCALEINX_T GetLayerScale( unsigned int layer )
{
	if ( IsLayerValid(layer) && !GetLayerUseDefault(layer) ) {
		return layers[layer].scaleInx;
	}
	return GetLayoutCurScale(); // layout scale
}

EXPORT DIST_T GetLayerMinTrackRadius( unsigned int layer )
{
	if ( IsLayerValid(layer) && !GetLayerUseDefault(layer) ) {
		return layers[layer].minTrackRadius;
	}
	return GetLayoutMinTrackRadius();
}

EXPORT ANGLE_T GetLayerMaxTrackGrade( unsigned int layer )
{
	if ( IsLayerValid(layer) && !GetLayerUseDefault(layer) ) {
		return layers[layer].maxTrackGrade;
	}
	return GetLayoutMaxTrackGrade();
}

EXPORT tieData_t GetLayerTieData( unsigned int layer )
{
	if ( IsLayerValid(layer) && !GetLayerUseDefault(layer) && layers[layer].tieData.valid ) {
		return layers[layer].tieData;
	}
	return GetScaleTieData(GetLayoutCurScale()); // layout scale default tie data
}

BOOL_T GetLayerModule(unsigned int layer)
{
	if (!IsLayerValid(layer)) {
		return TRUE;
	} else {
		return layers[layer].module;
	}
}

void SetLayerModule(unsigned int layer, BOOL_T module)
{
	if (IsLayerValid(layer)) {
		layers[layer].module = module;
	}
}

void SetLayerDefault(unsigned int layer, BOOL_T inherit)
{
	if (IsLayerValid(layer)) {
		layers[layer].inherit = inherit;
	}
}


char * GetLayerName(unsigned int layer)
{
	if (!IsLayerValid(layer)) {
		return NULL;
	} else {
		return layers[layer].name;
	}
}

void SetLayerName(unsigned int layer, char* name)
{
	if (IsLayerValid(layer)) {
		strcpy(layers[layer].name, name);
	}
}

BOOL_T GetLayerUseColor(unsigned int layer)
{
	return layers[layer].useColor;
}

wDrawColor GetLayerColor(unsigned int layer)
{
	return layers[layer].color;
}

static void RedrawLayer( unsigned int l, BOOL_T draw )
{
	DoRedraw();
}


EXPORT void FlipLayer( void * layerVP )
{
	unsigned int layer = (unsigned int)VP2L(layerVP);
	wBool_t visible;

	if (!IsLayerValid(layer)) {
		return;
	}

	if (layer == curLayer && layers[layer].visible) {
		if (!layers[layer].button_off) {
			wButtonSetBusy(layer_btns[layer], layers[layer].visible);
		}
		NoticeMessage(MSG_LAYER_HIDE, _("Ok"), NULL);
		return;
	}

	RedrawLayer(layer, FALSE);
	visible = !layers[layer].visible;
	layers[layer].visible = visible;

	if (layer < NUM_BUTTONS) {
		if (!layers[layer].button_off) {
			wButtonSetBusy(layer_btns[layer], visible != 0);
			wButtonSetLabel(layer_btns[layer], (char *)show_layer_bmps[layer]);
		}
	}

	/* Set visible on related layers other than current */
	for (int i = 0; i < layers[layer].layerLinkList.cnt; i++) {
		int l = DYNARR_N(int, layers[layer].layerLinkList, i) - 1;
		if ((l != curLayer) && (l >= 0) && (l < NUM_LAYERS)) {
			layers[l].visible = layers[layer].visible;
			if (!layers[l].button_off) {
				wButtonSetBusy(layer_btns[l], layers[l].visible);
			}
		}
	}

	RedrawLayer(layer, TRUE);
}

static char lastSettings[STR_SHORT_SIZE];
void SetCurrLayer(wIndex_t inx, const char * name, wIndex_t op,
                  void * listContext, void * arg)
{
	unsigned int newLayer = (unsigned int)inx;

	if (layers[newLayer].frozen) {
		NoticeMessage(MSG_LAYER_SEL_FROZEN, _("Ok"), NULL);
		wListSetIndex(setLayerL, curLayer);
		return;
	}

	char *array[1];
	if (!layers[inx].settingsName[0]
	    || strcmp(layers[inx].settingsName, " ") == 0) {
		if (lastSettings[0]) {
			DoSettingsRead(1, NULL, NULL);
		}
		lastSettings[0] = '\0';
	} else {
		if (strcmp(layers[inx].settingsName, lastSettings) != 0) {
			if (!lastSettings[0]) { wPrefFlush(""); }  // Save Last Settings for no settings file
			array[0] = layers[inx].settingsName;
			DoSettingsRead(1, array, NULL);
		}
		strcpy(lastSettings, layers[inx].settingsName);
	}


	curLayer = newLayer;

	if (!IsLayerValid(curLayer)) {
		curLayer = 0;							//Too big or -1
		layers[curLayer].frozen = FALSE;        //Make sure the layer is not frozen
	}


	if (!layers[curLayer].visible) {
		FlipLayer(I2VP(inx));
	}

	/* Set visible on related layers other than current */
	for (int i = 0; i < layers[curLayer].layerLinkList.cnt; i++) {
		int l = DYNARR_N(int, layers[curLayer].layerLinkList, i) - 1;
		if (l != curLayer && l >= 0 && l < NUM_LAYERS) {
			layers[l].visible = layers[curLayer].visible;
			if (!layers[l].button_off) {
				wButtonSetBusy(layer_btns[l], layers[l].visible);
			}
		}
	}

	if (recordF) {
		fprintf(recordF, "SETCURRLAYER %d\n", inx);
	}
}

static void PlaybackCurrLayer(char * line)
{
	wIndex_t layer;
	layer = atoi(line);
	wListSetIndex(setLayerL, layer);
	SetCurrLayer(layer, NULL, 0, NULL, NULL);
}

/**
 * Change the color of a layer.
 *
 * \param inx IN layer to change
 * \param color IN new color
 */

static void SetLayerColor(unsigned int inx, wDrawColor color)
{
	if (color != layers[inx].color) {
		if (inx < NUM_BUTTONS) {
			wIconSetColor(show_layer_bmps[inx], color);
			wButtonSetLabel(layer_btns[inx], (char*)show_layer_bmps[inx]);
		}

		layers[inx].color = color;
		layoutLayerChanged = TRUE;
	}
}

static void SetLayerHideButton(unsigned int inx, wBool_t hide)
{
	if (hide != layers[inx].button_off) {
		if (inx < NUM_BUTTONS) {
			wControlShow((wControl_p)layer_btns[inx], !hide);
			if (!hide) { wButtonSetBusy(layer_btns[inx], layers[inx].visible); }
		}
		layers[inx].button_off = hide;
		layoutLayerChanged = TRUE;
	}
}

char *
FormatLayerName(unsigned int layerNumber)
{
	DynString string;// = NaS;
	char *result;
	DynStringMalloc(&string, 0);
	DynStringPrintf(&string,
	                "%2d %c %s",
	                layerNumber + 1,
	                (layers[layerNumber].frozen ? '*' : layers[layerNumber].module ? 'm' :
	                 layers[layerNumber].objCount > 0 ? '+' : '-'),
	                layers[layerNumber].name);
	result = strdup(DynStringToCStr(&string));
	DynStringFree(&string);
	return result;
}

static int lbits_width[3] = {14, 20, 26};
static int lbits_height[3] = {16, 24, 32};

#include "bitmaps/l1.xbm"
#include "bitmaps/l2.xbm"
#include "bitmaps/l3.xbm"
#include "bitmaps/l4.xbm"
#include "bitmaps/l5.xbm"
#include "bitmaps/l6.xbm"
#include "bitmaps/l7.xbm"
#include "bitmaps/l8.xbm"
#include "bitmaps/l9.xbm"
#include "bitmaps/l10.xbm"
#include "bitmaps/l11.xbm"
#include "bitmaps/l12.xbm"
#include "bitmaps/l13.xbm"
#include "bitmaps/l14.xbm"
#include "bitmaps/l15.xbm"
#include "bitmaps/l16.xbm"
#include "bitmaps/l17.xbm"
#include "bitmaps/l18.xbm"
#include "bitmaps/l19.xbm"
#include "bitmaps/l20.xbm"
#include "bitmaps/l21.xbm"
#include "bitmaps/l22.xbm"
#include "bitmaps/l23.xbm"
#include "bitmaps/l24.xbm"
#include "bitmaps/l25.xbm"
#include "bitmaps/l26.xbm"
#include "bitmaps/l27.xbm"
#include "bitmaps/l28.xbm"
#include "bitmaps/l29.xbm"
#include "bitmaps/l30.xbm"
#include "bitmaps/l31.xbm"
#include "bitmaps/l32.xbm"
#include "bitmaps/l33.xbm"
#include "bitmaps/l34.xbm"
#include "bitmaps/l35.xbm"
#include "bitmaps/l36.xbm"
#include "bitmaps/l37.xbm"
#include "bitmaps/l38.xbm"
#include "bitmaps/l39.xbm"
#include "bitmaps/l40.xbm"
#include "bitmaps/l41.xbm"
#include "bitmaps/l42.xbm"
#include "bitmaps/l43.xbm"
#include "bitmaps/l44.xbm"
#include "bitmaps/l45.xbm"
#include "bitmaps/l46.xbm"
#include "bitmaps/l47.xbm"
#include "bitmaps/l48.xbm"
#include "bitmaps/l49.xbm"
#include "bitmaps/l50.xbm"
#include "bitmaps/l51.xbm"
#include "bitmaps/l52.xbm"
#include "bitmaps/l53.xbm"
#include "bitmaps/l54.xbm"
#include "bitmaps/l55.xbm"
#include "bitmaps/l56.xbm"
#include "bitmaps/l57.xbm"
#include "bitmaps/l58.xbm"
#include "bitmaps/l59.xbm"
#include "bitmaps/l60.xbm"
#include "bitmaps/l61.xbm"
#include "bitmaps/l62.xbm"
#include "bitmaps/l63.xbm"
#include "bitmaps/l64.xbm"
#include "bitmaps/l65.xbm"
#include "bitmaps/l66.xbm"
#include "bitmaps/l67.xbm"
#include "bitmaps/l68.xbm"
#include "bitmaps/l69.xbm"
#include "bitmaps/l70.xbm"
#include "bitmaps/l71.xbm"
#include "bitmaps/l72.xbm"
#include "bitmaps/l73.xbm"
#include "bitmaps/l74.xbm"
#include "bitmaps/l75.xbm"
#include "bitmaps/l76.xbm"
#include "bitmaps/l77.xbm"
#include "bitmaps/l78.xbm"
#include "bitmaps/l79.xbm"
#include "bitmaps/l80.xbm"
#include "bitmaps/l81.xbm"
#include "bitmaps/l82.xbm"
#include "bitmaps/l83.xbm"
#include "bitmaps/l84.xbm"
#include "bitmaps/l85.xbm"
#include "bitmaps/l86.xbm"
#include "bitmaps/l87.xbm"
#include "bitmaps/l88.xbm"
#include "bitmaps/l89.xbm"
#include "bitmaps/l90.xbm"
#include "bitmaps/l91.xbm"
#include "bitmaps/l92.xbm"
#include "bitmaps/l93.xbm"
#include "bitmaps/l94.xbm"
#include "bitmaps/l95.xbm"
#include "bitmaps/l96.xbm"
#include "bitmaps/l97.xbm"
#include "bitmaps/l98.xbm"
#include "bitmaps/l99.xbm"


static char * show_layer_bits[3][NUM_BUTTONS] = {
	{
		l1_x16, l2_x16, l3_x16, l4_x16, l5_x16, l6_x16, l7_x16, l8_x16, l9_x16, l10_x16,
		l11_x16, l12_x16, l13_x16, l14_x16, l15_x16, l16_x16, l17_x16, l18_x16, l19_x16, l20_x16,
		l21_x16, l22_x16, l23_x16, l24_x16, l25_x16, l26_x16, l27_x16, l28_x16, l29_x16, l30_x16,
		l31_x16, l32_x16, l33_x16, l34_x16, l35_x16, l36_x16, l37_x16, l38_x16, l39_x16, l40_x16,
		l41_x16, l42_x16, l43_x16, l44_x16, l45_x16, l46_x16, l47_x16, l48_x16, l49_x16, l50_x16,
		l51_x16, l52_x16, l53_x16, l54_x16, l55_x16, l56_x16, l57_x16, l58_x16, l59_x16, l60_x16,
		l61_x16, l62_x16, l63_x16, l64_x16, l65_x16, l66_x16, l67_x16, l68_x16, l69_x16, l70_x16,
		l71_x16, l72_x16, l73_x16, l74_x16, l75_x16, l76_x16, l77_x16, l78_x16, l79_x16, l80_x16,
		l81_x16, l82_x16, l83_x16, l84_x16, l85_x16, l86_x16, l87_x16, l88_x16, l89_x16, l90_x16,
		l91_x16, l92_x16, l93_x16, l94_x16, l95_x16, l96_x16, l97_x16, l98_x16, l99_x16,
	},
	{
		l1_x24, l2_x24, l3_x24, l4_x24, l5_x24, l6_x24, l7_x24, l8_x24, l9_x24, l10_x24,
		l11_x24, l12_x24, l13_x24, l14_x24, l15_x24, l16_x24, l17_x24, l18_x24, l19_x24, l20_x24,
		l21_x24, l22_x24, l23_x24, l24_x24, l25_x24, l26_x24, l27_x24, l28_x24, l29_x24, l30_x24,
		l31_x24, l32_x24, l33_x24, l34_x24, l35_x24, l36_x24, l37_x24, l38_x24, l39_x24, l40_x24,
		l41_x24, l42_x24, l43_x24, l44_x24, l45_x24, l46_x24, l47_x24, l48_x24, l49_x24, l50_x24,
		l51_x24, l52_x24, l53_x24, l54_x24, l55_x24, l56_x24, l57_x24, l58_x24, l59_x24, l60_x24,
		l61_x24, l62_x24, l63_x24, l64_x24, l65_x24, l66_x24, l67_x24, l68_x24, l69_x24, l70_x24,
		l71_x24, l72_x24, l73_x24, l74_x24, l75_x24, l76_x24, l77_x24, l78_x24, l79_x24, l80_x24,
		l81_x24, l82_x24, l83_x24, l84_x24, l85_x24, l86_x24, l87_x24, l88_x24, l89_x24, l90_x24,
		l91_x24, l92_x24, l93_x24, l94_x24, l95_x24, l96_x24, l97_x24, l98_x24, l99_x24,
	},
	{
		l1_x32, l2_x32, l3_x32, l4_x32, l5_x32, l6_x32, l7_x32, l8_x32, l9_x32, l10_x32,
		l11_x32, l12_x32, l13_x32, l14_x32, l15_x32, l16_x32, l17_x32, l18_x32, l19_x32, l20_x32,
		l21_x32, l22_x32, l23_x32, l24_x32, l25_x32, l26_x32, l27_x32, l28_x32, l29_x32, l30_x32,
		l31_x32, l32_x32, l33_x32, l34_x32, l35_x32, l36_x32, l37_x32, l38_x32, l39_x32, l40_x32,
		l41_x32, l42_x32, l43_x32, l44_x32, l45_x32, l46_x32, l47_x32, l48_x32, l49_x32, l50_x32,
		l51_x32, l52_x32, l53_x32, l54_x32, l55_x32, l56_x32, l57_x32, l58_x32, l59_x32, l60_x32,
		l61_x32, l62_x32, l63_x32, l64_x32, l65_x32, l66_x32, l67_x32, l68_x32, l69_x32, l70_x32,
		l71_x32, l72_x32, l73_x32, l74_x32, l75_x32, l76_x32, l77_x32, l78_x32, l79_x32, l80_x32,
		l81_x32, l82_x32, l83_x32, l84_x32, l85_x32, l86_x32, l87_x32, l88_x32, l89_x32, l90_x32,
		l91_x32, l92_x32, l93_x32, l94_x32, l95_x32, l96_x32, l97_x32, l98_x32, l99_x32,
	}
};


static  long layerRawColorTab[] = {
	wRGB(0,  0, 255),       /* blue */
	wRGB(0,  0, 128),       /* dk blue */
	wRGB(0, 128,  0),       /* dk green */
	wRGB(255, 255,  0),     /* yellow */
	wRGB(0, 255,  0),       /* green */
	wRGB(0, 255, 255),      /* lt cyan */
	wRGB(128,  0,  0),      /* brown */
	wRGB(128,  0, 128),     /* purple */
	wRGB(128, 128,  0),     /* green-brown */
	wRGB(255,  0, 255)
};     /* lt-purple */
static  wDrawColor layerColorTab[COUNT(layerRawColorTab)];


static wWin_p layerW;
static char layerName[STR_SHORT_SIZE];
static char layerLinkList[STR_LONG_SIZE];
static char settingsName[STR_SHORT_SIZE];
static wDrawColor layerColor;
static long layerUseColor = TRUE;
static long layerVisible = TRUE;
static long layerFrozen = FALSE;
static long layerOnMap = TRUE;
static long layerModule = FALSE;
static long layerNoButton = FALSE;
static long layerInherit = FALSE;

static SCALEINX_T layerScaleInx;
static SCALEDESCINX_T layerScaleDescInx;
static GAUGEINX_T layerGaugeInx;
static DIST_T layerMinRadius;
static ANGLE_T layerMaxGrade;
static tieData_t layerTieData;

static long layerObjectCount;
static void LayerOk(void * unused);
static BOOL_T layerRedrawMap = FALSE;

#define ENUMLAYER_RELOAD (1)
#define ENUMLAYER_SAVE	(2)
#define ENUMLAYER_CLEAR (3)
#define ENUMLAYER_ADD (4)
#define ENUMLAYER_DELETE (5)
#define ENUMLAYER_DEFAULT (6)

static char *visibleLabels[] = { "", NULL };
static char *frozenLabels[] = { "", NULL };
static char *onMapLabels[] = { "", NULL };
static char *moduleLabels[] = { "", NULL };
static char *noButtonLabels[] = { "", NULL };
static char *defaultLabels[] = { "", NULL };
static char *layerColorLabels[] = { "", NULL };
static paramIntegerRange_t i0_20 = { 0, NUM_BUTTONS };
//static paramListData_t layerUiListData = { 10, 370, 0 };

static paramData_t layerPLs[] = {
#define I_LIST	(0)
	{ PD_DROPLIST, NULL, "layer", PDO_LISTINDEX, I2VP(250), N_("Select Layer:") },
#define I_NAME	(1)
	{ PD_STRING, layerName, "name", PDO_NOPREF | PDO_STRINGLIMITLENGTH | PDO_DLGBOXEND, I2VP(250 - 54), N_("Name"), 0, 0, sizeof(layerName) },
#define I_COLOR	(2)
	{ PD_COLORLIST, &layerColor, "color", PDO_NOPREF, NULL, N_("Color") },
#define I_USE_COLOR (3)
	{ PD_TOGGLE, &layerUseColor, "layercolor", PDO_NOPREF | PDO_DLGHORZ, layerColorLabels, N_("Use Color"), BC_HORZ | BC_NOBORDER },
#define I_VIS	(4)
	{ PD_TOGGLE, &layerVisible, "visible", PDO_NOPREF, visibleLabels, N_("Visible"), BC_HORZ | BC_NOBORDER },
#define I_FRZ	(5)
	{ PD_TOGGLE, &layerFrozen, "frozen", PDO_NOPREF | PDO_DLGHORZ, frozenLabels, N_("Frozen"), BC_HORZ | BC_NOBORDER },
#define I_MAP	(6)
	{ PD_TOGGLE, &layerOnMap, "onmap", PDO_NOPREF | PDO_DLGHORZ, onMapLabels, N_("On Map"), BC_HORZ | BC_NOBORDER },
#define I_MOD 	(7)
	{ PD_TOGGLE, &layerModule, "module", PDO_NOPREF | PDO_DLGHORZ, moduleLabels, N_("Module"), BC_HORZ | BC_NOBORDER },
#define I_BUT   (8)
	{ PD_TOGGLE, &layerNoButton, "button", PDO_NOPREF | PDO_DLGHORZ, noButtonLabels, N_("No Button"), BC_HORZ | BC_NOBORDER },
#define I_DEF   (9)
	{ PD_TOGGLE, &layerInherit, "inherit", PDO_NOPREF | PDO_DLGHORZ | PDO_DLGBOXEND, defaultLabels, N_("Inherit"), BC_HORZ | BC_NOBORDER },
#define I_SCALE (10)
	{ PD_DROPLIST, &layerScaleDescInx, "scale", PDO_NOPREF | PDO_NOPSHUPD | PDO_NORECORD | PDO_NOUPDACT, I2VP(180), N_("Scale"), 0, I2VP(CHANGE_LAYER) },
#define I_GAUGE (11)
	{ PD_DROPLIST, &layerGaugeInx, "gauge", PDO_NOPREF | PDO_NOPSHUPD | PDO_NORECORD | PDO_NOUPDACT | PDO_DLGHORZ, I2VP(180), N_("     Gauge") },
#define I_MINRADIUSENTRY (12)
	{ PD_FLOAT, &layerMinRadius, "mintrackradius", PDO_DIM | PDO_NOPSHUPD | PDO_NOPREF, &r0_10000, N_("Min Track Radius"), 0, I2VP(CHANGE_MAIN | CHANGE_LIMITS) },
#define I_MAXGRADEENTRY (13)
	{ PD_FLOAT, &layerMaxGrade, "maxtrackgrade", PDO_NOPSHUPD | PDO_DLGHORZ | PDO_NOPREF, &r0_90, N_("  Max Track Grade (%)"), 0, I2VP(CHANGE_MAIN) },
#define I_TIELEN (14)
	{ PD_FLOAT, &layerTieData.length, "tielength", PDO_NOPREF, &r_tieData, N_( "Tie Length" ), 0, I2VP( CHANGE_MAIN ) },
#define I_TIEWID (15)
	{ PD_FLOAT, &layerTieData.width, "tiewidth", PDO_NOPREF | PDO_DLGHORZ, &r_tieData, N_( "  Width" ), 0, I2VP( CHANGE_MAIN ) },
#define I_TIESPC (16)
	{ PD_FLOAT, &layerTieData.spacing, "tiespacing", PDO_NOPREF | PDO_DLGHORZ | PDO_DLGBOXEND, &r_tieData, N_( "  Spacing" ), 0, I2VP( CHANGE_MAIN ) },

	{ PD_MESSAGE, N_("Layer Actions"), NULL, PDO_DLGRESETMARGIN, I2VP(180) },
#define I_ADD (18)
	{ PD_BUTTON, DoLayerOp, "add", PDO_DLGRESETMARGIN, 0, N_("Add Layer"), 0, I2VP(ENUMLAYER_ADD) },
#define I_DELETE (19)
	{ PD_BUTTON, DoLayerOp, "delete", PDO_DLGHORZ, 0, N_("Delete Layer"), 0, I2VP(ENUMLAYER_DELETE) },
#define I_DEFAULT (20)
	{ PD_BUTTON, DoLayerOp, "default", PDO_DLGHORZ | PDO_DLGBOXEND, 0, N_("Default Values"), 0, I2VP(ENUMLAYER_DEFAULT) },
	{ PD_LONG, &newLayerCount, "button-count", PDO_DLGBOXEND | PDO_DLGRESETMARGIN, &i0_20, N_("Number of Layer Buttons") },
#define I_LINKLIST (22)
	{ PD_STRING, layerLinkList, "layerlist", PDO_NOPREF | PDO_STRINGLIMITLENGTH, I2VP(250 - 54), N_("Linked Layers"), 0, 0, sizeof(layerLinkList) },
#define I_SETTINGS (23)
	{ PD_DROPLIST, NULL, "settings", PDO_LISTINDEX, I2VP(250), N_("Settings when Current") },
#define I_COUNT (24)
	{ PD_LONG, &layerObjectCount, "objectCount", PDO_DLGBOXEND, &r_nocheck, N_("Object Count:"), 0, 0 },
	{ PD_MESSAGE, N_("All Layer Preferences"), NULL, PDO_DLGRESETMARGIN, I2VP(180) },
	{ PD_BUTTON, DoLayerOp, "load", PDO_DLGRESETMARGIN, 0, N_("Load"), 0, I2VP(ENUMLAYER_RELOAD) },
	{ PD_BUTTON, DoLayerOp, "save", PDO_DLGHORZ, 0, N_("Save"), 0, I2VP(ENUMLAYER_SAVE) },
	{ PD_BUTTON, DoLayerOp, "clear", PDO_DLGHORZ | PDO_DLGBOXEND, 0, N_("Defaults"), 0, I2VP(ENUMLAYER_CLEAR) },
};

#define settingsListL	((wList_p)layerPLs[I_SETTINGS].control)

static paramGroup_t layerPG = { "layer", 0, layerPLs, COUNT( layerPLs ) };

/**
 * Reload the listbox showing the current catalog
 */
static int LoadFileListLoad(Catalog *catalog, char * name)
{
	CatalogEntry *currentEntry = catalog->head;
	DynString description;
	DynStringMalloc(&description, STR_SHORT_SIZE);

	wControlShow((wControl_p)settingsListL, FALSE);
	wListClear(settingsListL);

	int currset = 0;

	int i = 0;

	wListAddValue(settingsListL, " ", NULL, " ");

	while (currentEntry) {
		i++;
		DynStringClear(&description);
		DynStringCatCStr(&description,
		                 currentEntry->contents) ;
		wListAddValue(settingsListL,
		              DynStringToCStr(&description),
		              NULL,
		              currentEntry->fullFileName[0]);
		if (strcmp(currentEntry->fullFileName[0], name) == 0) { currset = i; }
		currentEntry = currentEntry->next;
	}


	wListSetIndex(settingsListL, currset);

	wControlShow((wControl_p)settingsListL, TRUE);

	DynStringFree(&description);

	if (currset == 0 && strcmp(" ", name) != 0) { return FALSE; }
	return TRUE;
}

#define layerL	((wList_p)layerPLs[I_LIST].control)

#define layerS  ((wList_p)layerPLs[I_SETTINGS].control)

#define scaleL	((wList_p)layerPLs[I_SCALE].control)

#define gaugeL	((wList_p)layerPLs[I_GAUGE].control)

/**
* @brief Reload Layer parameters if changes
* @param changes
*/
static void LayerChange(long changes)
{
	if (changes & (CHANGE_LAYER))
		if (layerW != NULL && wWinIsVisible(layerW))
		{
			ParamLoadControls(&layerPG);
		}
}

void GetLayerLinkString(int inx, char * list)
{
	char * cp = &list[0];
	cp[0] = '\0';
	int len = 0;
	for (int i = 0; i < layers[inx].layerLinkList.cnt
	     && len < STR_LONG_SIZE - 5; i++) {
		int l = DYNARR_N(int, layers[inx].layerLinkList, i);
		if (i == 0) {
			cp += sprintf(cp, "%d", l);
		} else {
			cp += sprintf(cp, ";%d", l);
		}
		cp[0] = '\0';
	}
}

void PutLayerListArray(int inx, char * list)
{
	char * cp = &list[0];
	DYNARR_RESET(int, layers[inx].layerLinkList);
	while (cp) {
		cp = strpbrk(list, ",; ");
		if (cp) {
			cp[0] = '\0';
			int i =  abs((int)strtol(list, &list, 0));
			if (i > 0 && i != inx - 1 && i < NUM_LAYERS) {
				DYNARR_APPEND(int, layers[inx].layerLinkList, 1);
				DYNARR_LAST(int, layers[inx].layerLinkList) = i;
			}
			cp[0] = ';';
			list = cp + 1;
		} else {
			int i =  abs((int)strtol(list, &list, 0));
			if (i > 0 && i != inx - 1 && i < NUM_LAYERS) {
				DYNARR_APPEND(int, layers[inx].layerLinkList, 1);
				DYNARR_LAST(int, layers[inx].layerLinkList) = i;
			}
			cp = 0;
		}
	}
}

/**
 * Set a Layer to System Default
 */
void LayerSystemDefault( unsigned int inx )
{

	strcpy(layers[inx].name, inx == 0 ? _("Main") : "");
	layers[inx].visible = TRUE;
	layers[inx].frozen = FALSE;
	layers[inx].onMap = TRUE;
	layers[inx].module = FALSE;
	layers[inx].button_off = FALSE;
	layers[inx].inherit = TRUE;
	layers[inx].scaleInx = 0;
	GetScaleGauge(layers[inx].scaleInx, &layers[inx].scaleDescInx, 
		&layers[inx].gaugeInx);
	layers[inx].minTrackRadius = GetLayoutMinTrackRadius();
	layers[inx].maxTrackGrade = GetLayoutMaxTrackGrade();
	layers[inx].tieData.valid = FALSE;
	layers[inx].tieData.length = 0.0;
	layers[inx].tieData.width = 0.0;
	layers[inx].tieData.spacing = 0.0;
	layers[inx].objCount = 0;
	DYNARR_RESET(int, layers[inx].layerLinkList);
	SetLayerColor(inx, layerColorTab[inx % COUNT(layerColorTab)]);
}

/**
 * Is a layer the system default?
 */
BOOL_T IsLayerDefault( unsigned int inx )
{
	return (!layers[inx].name[0]) &&
	       layers[inx].visible &&
	       !layers[inx].frozen &&
	       layers[inx].onMap &&
	       !layers[inx].module &&
	       !layers[inx].button_off &&
		   layers[inx].inherit &&
		   (!layers[inx].layerLinkList.cnt) &&
	       (layers[inx].color == layerColorTab[inx % COUNT(layerColorTab)]) &&
	       layers[inx].scaleInx == 0 &&
	       //layers[inx].scaleDescInx != layerScaleDescInx ||
	       //layers[inx].gaugeInx != layerGaugeInx ||
	       layers[inx].minTrackRadius == GetLayoutMinTrackRadius() &&
	       layers[inx].maxTrackGrade == GetLayoutMaxTrackGrade() &&
		   layers[inx].tieData.valid == FALSE &&
	       layers[inx].tieData.length == 0.0 &&
	       layers[inx].tieData.width == 0.0 &&
	       layers[inx].tieData.spacing == 0.0;
}

/**
 * Load the layer settings to hard coded system defaults
 */
EXPORT void LayerAllDefaults(void)
{
	int inx;

	for (inx = 0; inx < NUM_LAYERS; inx++) {
		LayerSystemDefault(inx);
	}
}

/**
 * Load the layer listboxes in Manage Layers and the Toolbar with up-to-date information.
 */
void LoadLayerLists(void)
{
	int inx;
	/* clear both lists */
	wListClear(setLayerL);

	if (layerL) {
		wListClear(layerL);
	}

	if (layerS) {
		wListClear(layerS);
	}


	/* add all layers to both lists */
	for (inx = 0; inx < NUM_LAYERS; inx++) {
		char *layerLabel;
		layerLabel = FormatLayerName(inx);

		if (layerL) {
			wListAddValue(layerL, layerLabel, NULL, NULL);
		}

		wListAddValue(setLayerL, layerLabel, NULL, NULL);
		free(layerLabel);
	}

	/* set current layer to selected */
	wListSetIndex(setLayerL, curLayer);

	if (layerL) {
		wListSetIndex(layerL, curLayer);
	}
}

/**
 * Add a layer after selected layer
 */
static void LayerAdd( )
{
	unsigned int inx;
	unsigned int newLayer = layerSelected + 1;

	// UndoStart( _("Add Layer"), "addlayer" );
	maxLayer++;
	for ( inx = maxLayer; inx > newLayer; inx-- ) {
		layers[inx] = layers[inx - 1];
	}

	TrackInsertLayer(newLayer);

	LayerSystemDefault(newLayer);
	strcpy(layers[newLayer].name, "New Layer");
	layers[newLayer].objCount = 0;

	UpdateLayerDlg( newLayer );

	layerSelected = newLayer;
	layoutLayerChanged = TRUE;
	// UndoEnd();
}

/**
* Delete the selected layer
*/
static void LayerDelete( )
{
	unsigned int inx;

	if (layers[layerSelected].objCount > 0) {
		NoticeMessage(_("Layer must not have any objects in it."), _("Ok"), NULL);
		return;
	}

	if (layerSelected <= maxLayer) {
		for ( inx = layerSelected; inx < maxLayer; inx++ ) {
			layers[inx] = layers[inx + 1];
		}
		LayerSystemDefault(maxLayer);

		if (maxLayer > 0) {
			maxLayer--;
		}
	}

	TrackDeleteLayer( layerSelected );

	UpdateLayerDlg( layerSelected );

	layoutLayerChanged = TRUE;
}

/**
* Set the Min Radius, Max Grade and Tie values to Layout or Scale defaults
*/
static void LayerDefault( )
{
	if ( layers[layerSelected].inherit ){
		layers[layerSelected].scaleInx = GetLayoutCurScale( );
		GetScaleGauge(layers[layerSelected].scaleInx, &layers[layerSelected].scaleDescInx,
			&layers[layerSelected].gaugeInx);
		layers[layerSelected].minTrackRadius = GetLayoutMinTrackRadius();
		layers[layerSelected].maxTrackGrade = GetLayoutMaxTrackGrade();
	}
	else {
		layers[layerSelected].scaleInx = GetLayerScale( layerSelected );
		GetScaleGauge(layers[layerSelected].scaleInx, &layers[layerSelected].scaleDescInx,
			&layers[layerSelected].gaugeInx);
		layers[layerSelected].minTrackRadius = GetLayoutMinTrackRadius();
		layers[layerSelected].maxTrackGrade = GetLayoutMaxTrackGrade();
	}
	layers[layerSelected].tieData = GetScaleTieData(layers[layerSelected].scaleInx);

	UpdateLayerDlg( layerSelected );

	layoutLayerChanged = TRUE;
}

/**
 * Handle button presses for the layer dialog. For all button presses in the layer
 *	dialog, this function is called. The parameter identifies the button pressed and
 * the operation is performed.
 *
 * \param[IN] data identifier for the button pressed
 * \return
 */

static void DoLayerOp(void * data)
{
	switch (VP2L(data)) {
	case ENUMLAYER_CLEAR:
		InitializeLayers(LayerAllDefaults, -1);
		break;

	case ENUMLAYER_SAVE:
		LayerPrefSave();
		break;

	case ENUMLAYER_RELOAD:
		LayerPrefLoad();
		break;

	case ENUMLAYER_ADD:
		LayerAdd();
		break;

	case ENUMLAYER_DELETE:
		LayerDelete();
		break;

	case ENUMLAYER_DEFAULT:
		LayerDefault();
		break;
	}

	// UpdateLayerDlg(curLayer);   //Reset to current Layer
	ParamControlActive( &layerPG, I_DELETE, (layerSelected > 0) ? TRUE : FALSE);

	if (layoutLayerChanged) {
		MainProc(mainW, wResize_e, NULL, NULL);
		layoutLayerChanged = FALSE;
		SetFileChanged();
	}
}

/**
 * Update all dialogs and dialog elements after changing layers preferences. Once the global array containing
 * the settings for the labels has been changed, this function needs to be called to update all the user interface
 * elements to the new settings.
 */
EXPORT void UpdateLayerDlg(unsigned int layer)
{
	int inx;
	/* update the globals for the layer dialog */
	layerVisible = layers[layer].visible;
	layerFrozen = layers[layer].frozen;
	layerOnMap = layers[layer].onMap;
	layerModule = layers[layer].module;
	layerColor = layers[layer].color;
	layerUseColor = layers[layer].useColor;
	layerNoButton = layers[layer].button_off;
	layerInherit = layers[layer].inherit;
	layerScaleInx = layers[layer].scaleInx;
	layerScaleDescInx = layers[layer].scaleDescInx;
	layerGaugeInx = layers[layer].gaugeInx;
	layerMinRadius = layers[layer].minTrackRadius;
	layerMaxGrade = layers[layer].maxTrackGrade;
	layerTieData = layers[layer].tieData;
	layerObjectCount = layers[layer].objCount;
	strcpy(layerName, layers[layer].name);
	strcpy(settingsName, layers[layer].settingsName);
	GetLayerLinkString(layer, layerLinkList);

	layerSelected = layer;

	/* now re-load the layer list boxes */
	LoadLayerLists();

	/* Sync Scale and lists */
	if (gaugeL) {
		LoadGaugeList(gaugeL, layerScaleDescInx);
		wListSetIndex(gaugeL, layerGaugeInx);
	}
	/* Sync Scale and lists */
	GetScaleGauge(layerScaleInx, &layerScaleDescInx, &layerGaugeInx);

	/* force update of the 'manage layers' dialogbox */
	if (layerL) {
		wListSetIndex(layerL, layer);
		ParamLoadControls(&layerPG);
	}

	if (layerS) {
		if (!LoadFileListLoad(settingsCatalog, settingsName)) {
			layers[layer].settingsName[0] = '\0';
		}
	}

	ParamControlActive( &layerPG, I_DELETE, (layerSelected > 0) ? TRUE : FALSE);
	ParamControlActive( &layerPG, I_COUNT, FALSE );

	ParamControlActive( &layerPG, I_SCALE, !layerInherit);
	ParamControlActive( &layerPG, I_GAUGE, !layerInherit);
	ParamControlActive( &layerPG, I_MINRADIUSENTRY, !layerInherit);
	ParamControlActive( &layerPG, I_MAXGRADEENTRY, !layerInherit);
	ParamControlActive( &layerPG, I_TIELEN, !layerInherit);
	ParamControlActive( &layerPG, I_TIEWID, !layerInherit);
	ParamControlActive( &layerPG, I_TIESPC, !layerInherit);

	/* finally show the layer buttons with balloon text */
	for (inx = 0; inx < NUM_BUTTONS; inx++) {
		if (!layers[inx].button_off) {
			wButtonSetBusy(layer_btns[inx], layers[inx].visible != 0);
			wControlSetBalloonText((wControl_p)layer_btns[inx],
			                       (layers[inx].name[0] != '\0' ? layers[inx].name : _("Show/Hide Layer")));
		}
	}
}

/**
 * Fill a layer dropbox with the current layer settings
 *
 * \param listLayers the dropbox
 * \return
 */
void FillLayerList( wList_p listLayers)
{
	wListClear(listLayers);  // Rebuild list on each invocation

	for (int inx = 0; inx < NUM_LAYERS; inx++) {
		char *layerFormattedName;
		layerFormattedName = FormatLayerName(inx);
		wListAddValue((wList_p)listLayers, layerFormattedName, NULL, I2VP(inx));
		free(layerFormattedName);
	}

	/* set current layer to selected */
	wListSetIndex(listLayers, curLayer);

	ParamControlActive( &layerPG, I_DELETE, (curLayer > 0) ? TRUE : FALSE);
	if ( layerInherit ) {
		ParamControlActive( &layerPG, I_TIELEN, FALSE);
		ParamControlActive( &layerPG, I_TIEWID, FALSE);
		ParamControlActive( &layerPG, I_TIESPC, FALSE);
	}
}

/**
 * Initialize the layer lists.
 *
 * \param IN pointer to function that actually initialize tha data structures
 * \param IN current layer (0...NUM_LAYERS), (-1) for no change
 */
static void
InitializeLayers(void LayerInitFunc(void), int newCurrLayer)
{
	/* reset the data structures to default valuses */
	LayerInitFunc();
	/* count the objects on each layer */
	LayerSetCounts();

	/* Switch the current layer when requested or the first above not frozen*/
	if (newCurrLayer != -1) {
		curLayer = -1;
		for (int i = newCurrLayer; i < NUM_LAYERS; i++) {
			if (!layers[i].frozen) {
				curLayer = i;
				break;
			}
		}
		if (curLayer == -1) {
			ErrorMessage( MSG_NO_EMPTY_LAYER );
			layers[0].frozen = FALSE;
			curLayer = 0;
		}
	}
}

/**
 * Save an integer to Prefs
 */
static void
layerSetInteger( unsigned int inx, char prefName[], int value )
{
	char buffer[80];
	char name[80];
	strcpy(name, prefName);
	strcat(name, ".%0u");
	sprintf( buffer, name, inx );
	wPrefSetInteger( LAYERPREF_SECTION, buffer, value );
}
/**
* Save a float to Prefs
*/
static void
layerSetFloat( unsigned int inx, char prefName[], double value )
{
	char buffer[80];
	char name[20];
	strcpy(name, prefName);
	strcat(name, ".%0u");
	sprintf( buffer, name, inx );
	wPrefSetFloat( LAYERPREF_SECTION, buffer, value );
}

/**
 * Save the customized layer information to preferences.
 */
static void
LayerPrefSave(void)
{
	unsigned int inx;
	int flags;
	char buffer[ 80 ];
	char links[STR_LONG_SIZE];
	char layersSaved[ 3 * NUM_LAYERS + 1 ];			/* 0..99 plus separator */
	/* FIXME: values for layers that are configured to default now should be overwritten in the settings */
	layersSaved[ 0 ] = '\0';

	for (inx = 0; inx < NUM_LAYERS; inx++) {
		/* if a name is set that is not the default value or a color different from the default has been set,
		    information about the layer needs to be saved */
		if (inx == 0 || !IsLayerDefault(inx)) {
			sprintf(buffer, LAYERPREF_NAME ".%0u", inx);
			wPrefSetString(LAYERPREF_SECTION, buffer, layers[inx].name);

			layerSetInteger(inx, LAYERPREF_COLOR, wDrawGetRGB(layers[inx].color));

			flags = 0;
			if (layers[inx].frozen) {
				flags |= LAYERPREF_FROZEN;
			}
			if (layers[inx].onMap) {
				flags |= LAYERPREF_ONMAP;
			}
			if (layers[inx].visible) {
				flags |= LAYERPREF_VISIBLE;
			}
			if (layers[inx].module) {
				flags |= LAYERPREF_MODULE;
			}
			if (layers[inx].button_off) {
				flags |= LAYERPREF_NOBUTTON;
			}
			if (layers[inx].inherit) {
				flags |= LAYERPREF_DEFAULT;
			}
			layerSetInteger(inx, LAYERPREF_FLAGS, flags);

			layers[inx].scaleInx = GetScaleInx( layers[inx].scaleDescInx,
			                                    layers[inx].gaugeInx );
			layerSetInteger(inx, LAYERPREF_SCALEINX, layers[inx].scaleInx);
			layerSetInteger(inx, LAYERPREF_SCLDESCINX, layers[inx].scaleDescInx);
			layerSetInteger(inx, LAYERPREF_GAUGEINX, layers[inx].gaugeInx);
			layerSetFloat(inx, LAYERPREF_MINRADIUS, layers[inx].minTrackRadius);
			layerSetFloat(inx, LAYERPREF_MAXGRADE, layers[inx].maxTrackGrade);
			layerSetFloat(inx, LAYERPREF_TIELENGTH, layers[inx].tieData.length);
			layerSetFloat(inx, LAYERPREF_TIEWIDTH, layers[inx].tieData.width);
			layerSetFloat(inx, LAYERPREF_TIESPACING, layers[inx].tieData.spacing);

			if (layers[inx].layerLinkList.cnt > 0) {
				sprintf(buffer, LAYERPREF_LIST ".%0u", inx);
				GetLayerLinkString(inx, links);
				wPrefSetString(LAYERPREF_SECTION, buffer, links);

				if (settingsName[0] && strcmp(settingsName, " ") != 0) {
					sprintf(buffer, LAYERPREF_SETTINGS ".%0u", inx);
					wPrefSetString(LAYERPREF_SECTION, buffer, layers[inx].settingsName);
				}
			}

			/* extend the list of layers that are set up via the preferences */
			if (layersSaved[ 0 ]) {
				strcat(layersSaved, ",");
			}

			sprintf(buffer, "%u", inx);
			strcat(layersSaved, buffer);
		}
	}

	wPrefSetString(LAYERPREF_SECTION, "layers", layersSaved);
}


/**
* Load an integer from Prefs
*/
static void
layerGetInteger( unsigned int inx, char prefName[], long *value, int deflt )
{
	char buffer[80];
	char name[80];
	strcpy(name, prefName);
	strcat(name, ".%0u");
	sprintf( buffer, name, inx );
	wPrefGetInteger( LAYERPREF_SECTION, buffer, value, deflt );
}
/**
* Load a float from Prefs
*/
static void
layerGetFloat( unsigned int inx, char prefName[], double *value, double deflt )
{
	char buffer[80];
	char name[20];
	strcpy(name, prefName);
	strcat(name, ".%0u");
	sprintf( buffer, name, inx );
	wPrefGetFloat( LAYERPREF_SECTION, buffer, value, deflt );
}

/**
 * Load the settings for all layers from the preferences.
 */

static void
LayerPrefLoad(void)
{
	const char *prefString;
	long rgb;
	long flags;
	/* reset layer preferences to system default */
	LayerAllDefaults();
	prefString = wPrefGetString(LAYERPREF_SECTION, "layers");

	if (prefString && prefString[ 0 ]) {
		char layersSaved[3 * NUM_LAYERS];
		strncpy(layersSaved, prefString, sizeof(layersSaved));
		prefString = strtok(layersSaved, ",");

		while (prefString) {
			int inx;
			char layerOption[20];
			const char *layerValue;
			char listValue[STR_LONG_SIZE];
			int color;
			inx = atoi(prefString);
			sprintf(layerOption, LAYERPREF_NAME ".%d", inx);
			layerValue = wPrefGetString(LAYERPREF_SECTION, layerOption);

			if (layerValue) {
				strcpy(layers[inx].name, layerValue);
			} else {
				*(layers[inx].name) = '\0';
			}

			/* get and set the color, using the system default color in case color is not available from prefs */
			layerGetInteger(inx, LAYERPREF_COLOR, &rgb,
			                layerColorTab[inx % COUNT(layerColorTab)]);
			color = wDrawFindColor(rgb);
			SetLayerColor(inx, color);
			/* get and set the flags */
			layerGetInteger(inx, LAYERPREF_FLAGS, &flags,
			                LAYERPREF_ONMAP | LAYERPREF_VISIBLE);
			layers[inx].frozen = ((flags & LAYERPREF_FROZEN) != 0);
			layers[inx].onMap = ((flags & LAYERPREF_ONMAP) != 0);
			layers[inx].visible = ((flags & LAYERPREF_VISIBLE) != 0);
			layers[inx].module = ((flags & LAYERPREF_MODULE) != 0);
			layers[inx].button_off = ((flags & LAYERPREF_NOBUTTON) != 0);
			layers[inx].inherit = ((flags & LAYERPREF_DEFAULT) != 0);

			layerGetInteger(inx, LAYERPREF_SCALEINX, &layers[inx].scaleInx,
			                GetLayoutCurScale());
			layerGetInteger(inx, LAYERPREF_SCLDESCINX, &layers[inx].scaleDescInx,
			                GetLayoutCurScaleDesc());
			layerGetInteger(inx, LAYERPREF_GAUGEINX, &layers[inx].gaugeInx, 0);

			layerGetFloat(inx, LAYERPREF_MINRADIUS, &layers[inx].minTrackRadius, GetLayoutMinTrackRadius());
			layerGetFloat(inx, LAYERPREF_MAXGRADE, &layers[inx].maxTrackGrade, GetLayoutMaxTrackGrade());
			layerGetFloat(inx, LAYERPREF_TIELENGTH, &layers[inx].tieData.length, 0.0);
			layerGetFloat(inx, LAYERPREF_TIEWIDTH, &layers[inx].tieData.width, 0.0);
			layerGetFloat(inx, LAYERPREF_TIESPACING, &layers[inx].tieData.spacing, 0.0);

			sprintf(layerOption, LAYERPREF_LIST ".%d", inx);
			layerValue = wPrefGetString(LAYERPREF_SECTION, layerOption);
			if (layerValue) {
				strcpy(listValue, layerValue);
				PutLayerListArray(inx, listValue);
			} else {
				listValue[0] = '\0';
				PutLayerListArray(inx, listValue);
			}
			sprintf(layerOption, LAYERPREF_SETTINGS ".%d", inx);
			layerValue = wPrefGetString(LAYERPREF_SECTION, layerOption);
			if (layerValue) {
				strcpy(layers[inx].settingsName, layerValue);
			} else {
				layers[inx].settingsName[0] = '\0';
			}

			prefString = strtok(NULL, ",");
		}
	}
	//Make sure curLayer not frozen
	for (int i = curLayer; i < NUM_LAYERS; i++) {
		if (!layers[i].frozen)  {
			curLayer = i;
			break;
		}
	}
	if (layers[curLayer].frozen) {
		ErrorMessage( MSG_NO_EMPTY_LAYER );
		layers[0].frozen = FALSE;
		curLayer = 0;
	}
}

/**
 * Increment the count of objects on a given layer.
 *
 * \param index IN the layer to change
 */

void IncrementLayerObjects(unsigned int layer)
{
	CHECK(layer <= NUM_LAYERS);
	layers[layer].objCount++;
}

/**
* Decrement the count of objects on a given layer.
*
* \param index IN the layer to change
*/

void DecrementLayerObjects(unsigned int layer)
{
	CHECK(layer <= NUM_LAYERS);
	layers[layer].objCount--;
}

/**
 *	Count the number of objects on each layer and store result in layers data structure.
 */

void LayerSetCounts(void)
{
	int inx;
	track_p trk;

	for (inx = 0; inx < NUM_LAYERS; inx++) {
		layers[inx].objCount = 0;
	}

	for (trk = NULL; TrackIterate(&trk);) {
		inx = GetTrkLayer(trk);

		if (inx >= 0 && inx < NUM_LAYERS) {
			layers[inx].objCount++;
		}
	}
}

int FindUnusedLayer(unsigned int start)
{
	int inx;
	for (inx = start; inx < NUM_LAYERS; inx++) {
		if (layers[inx].objCount == 0 && !layers[inx].frozen) { return inx; }
	}
	ErrorMessage( MSG_NO_EMPTY_LAYER );
	return -1;
}

/**
 * Reset layer options to their default values. The default values are loaded
 * from the preferences file.
 */

void
DefaultLayerProperties(void)
{
	InitializeLayers(LayerPrefLoad, 0);
	UpdateLayerDlg(curLayer);			//Use Current Layer

	if (layoutLayerChanged) {
		MainProc(mainW, wResize_e, NULL, NULL);
		layoutLayerChanged = FALSE;
	}
}

/**
 * Update all UI elements after selecting a layer.
 *
 */

static void LayerUpdate(void)
{
	BOOL_T redraw;
	char *layerFormattedName;
	ParamLoadData(&layerPG);

	if (!IsLayerValid(layerSelected)) {
		return;
	}

	if (layerSelected == curLayer && layerFrozen) {
		NoticeMessage(MSG_LAYER_FREEZE, _("Ok"), NULL);
		layerFrozen = FALSE;
		ParamLoadControl(&layerPG, I_FRZ);
	}

	if (layerSelected == curLayer && !layerVisible) {
		NoticeMessage(MSG_LAYER_HIDE, _("Ok"), NULL);
		layerVisible = TRUE;
		ParamLoadControl(&layerPG, I_VIS);
	}

	if (layerSelected == curLayer && layerModule) {
		NoticeMessage(MSG_LAYER_MODULE, _("Ok"), NULL);
		layerModule = FALSE;
		ParamLoadControl(&layerPG, I_MOD);
	}
	char oldLinkList[STR_LONG_SIZE];
	GetLayerLinkString((int)layerSelected, oldLinkList);

	if (strcmp(layers[(int)layerSelected].name, layerName) ||
	    layerColor != layers[(int)layerSelected].color ||
	    layers[(int)layerSelected].useColor != (BOOL_T)layerUseColor ||
	    layers[(int)layerSelected].visible != (BOOL_T)layerVisible ||
	    layers[(int)layerSelected].frozen != (BOOL_T)layerFrozen ||
	    layers[(int)layerSelected].onMap != (BOOL_T)layerOnMap ||
	    layers[(int)layerSelected].module != (BOOL_T)layerModule ||
	    layers[(int)layerSelected].button_off != (BOOL_T)layerNoButton ||
		layers[(int)layerSelected].inherit != (BOOL_T)layerInherit ||
		layers[(int)layerSelected].scaleInx != layerScaleInx ||
	    layers[(int)layerSelected].scaleDescInx != layerScaleDescInx ||
	    layers[(int)layerSelected].gaugeInx != layerGaugeInx ||
	    layers[(int)layerSelected].minTrackRadius != layerMinRadius ||
	    layers[(int)layerSelected].maxTrackGrade != layerMaxGrade ||
	    layers[(int)layerSelected].tieData.length != layerTieData.length ||
	    layers[(int)layerSelected].tieData.width != layerTieData.width ||
	    layers[(int)layerSelected].tieData.spacing != layerTieData.spacing ||
	    strcmp(layers[(int)layerSelected].settingsName, settingsName) ||
	    strcmp(oldLinkList, layerLinkList)) {
		SetFileChanged();
	}

	if (layerL) {
		strncpy(layers[(int)layerSelected].name, layerName,
		        sizeof layers[(int)layerSelected].name);
		layerFormattedName = FormatLayerName(layerSelected);
		wListSetValues(layerL, layerSelected, layerFormattedName, NULL, NULL);
		free(layerFormattedName);
	}


	layerFormattedName = FormatLayerName(layerSelected);
	wListSetValues(setLayerL, layerSelected, layerFormattedName, NULL, NULL);
	free(layerFormattedName);

	if (layerSelected < NUM_BUTTONS && !layers[(int)layerSelected].button_off) {
		if (strlen(layers[(int)layerSelected].name) > 0) {
			wControlSetBalloonText((wControl_p)layer_btns[(int)layerSelected],
			                       layers[(int)layerSelected].name);
		} else {
			wControlSetBalloonText((wControl_p)layer_btns[(int)layerSelected],
			                       _("Show/Hide Layer"));
		}
	}

	redraw = (layerColor != layers[(int)layerSelected].color ||
	          layers[(int)layerSelected].useColor != (BOOL_T)layerUseColor ||
	          (BOOL_T)layerVisible != layers[(int)layerSelected].visible);

	SetLayerColor(layerSelected, layerColor);

	if (layerSelected < NUM_BUTTONS &&
	    layers[(int)layerSelected].visible != (BOOL_T)layerVisible
	    && !layers[(int)layerSelected].button_off) {
		wButtonSetBusy(layer_btns[(int)layerSelected], layerVisible);
	}

	layers[(int)layerSelected].useColor = (BOOL_T)layerUseColor;
	if (layers[(int)layerSelected].visible != (BOOL_T)layerVisible) {
		FlipLayer(I2VP(layerSelected));
	}
	layers[(int)layerSelected].visible = (BOOL_T)layerVisible;
	layers[(int)layerSelected].frozen = (BOOL_T)layerFrozen;
	if (layers[(int)layerSelected].frozen) { DeselectLayer(layerSelected); }
	layers[(int)layerSelected].onMap = (BOOL_T)layerOnMap;
	layers[(int)layerSelected].scaleDescInx = layerScaleDescInx;
	layers[(int)layerSelected].gaugeInx = layerGaugeInx;
	layers[(int)layerSelected].scaleInx = GetScaleInx( layerScaleDescInx,
	                                      layerGaugeInx );
	layers[(int)layerSelected].minTrackRadius = layerMinRadius;
	layers[(int)layerSelected].maxTrackGrade = layerMaxGrade;
	layers[(int)layerSelected].tieData = layerTieData;
	layers[(int)layerSelected].module = (BOOL_T)layerModule;
	layers[(int)layerSelected].inherit = (BOOL_T)layerInherit;
	strcpy(layers[(int)layerSelected].settingsName, settingsName);

	PutLayerListArray((int)layerSelected, layerLinkList);

	SetLayerHideButton(layerSelected, layerNoButton);

	MainProc( mainW, wResize_e, NULL, NULL );

	if (layerRedrawMap) {
		DoRedraw();
	} else if (redraw) {
		RedrawLayer(layerSelected, TRUE);
	}

	layerRedrawMap = FALSE;
}


static void LayerSelect(
        wIndex_t inx)
{
	LayerUpdate();

	if (inx < 0 || inx >= NUM_LAYERS) {
		return;
	}

	layerSelected = (unsigned int)inx;
	strcpy(layerName, layers[inx].name);
	strcpy(settingsName, layers[inx].settingsName);
	layerVisible = layers[inx].visible;
	layerFrozen = layers[inx].frozen;
	layerOnMap = layers[inx].onMap;
	layerModule = layers[inx].module;
	layerColor = layers[inx].color;
	layerUseColor = layers[inx].useColor;
	layerNoButton = layers[inx].button_off;
	layerInherit = layers[inx].inherit;
	layerScaleInx = layers[inx].scaleInx;
	layerScaleDescInx = layers[inx].scaleDescInx;
	layerGaugeInx = layers[inx].gaugeInx;
	layerMinRadius = layers[inx].minTrackRadius;
	layerMaxGrade = layers[inx].maxTrackGrade;
	layerTieData.valid = layers[inx].tieData.valid;
	layerTieData.length = layers[inx].tieData.length;
	layerTieData.width = layers[inx].tieData.width;
	layerTieData.spacing = layers[inx].tieData.spacing;
	layerObjectCount = layers[inx].objCount;

	GetLayerLinkString(inx, layerLinkList);
	// sprintf(message, "%ld", layers[inx].objCount);
	// ParamLoadMessage(&layerPG, I_COUNT, message);
	ParamLoadControls(&layerPG);

	ParamControlActive( &layerPG, I_DELETE, (layerSelected > 0) ? TRUE : FALSE);

	ParamControlActive( &layerPG, I_SCALE, !layerInherit);
	ParamControlActive( &layerPG, I_GAUGE, !layerInherit);
	ParamControlActive( &layerPG, I_MINRADIUSENTRY, !layerInherit);
	ParamControlActive( &layerPG, I_MAXGRADEENTRY, !layerInherit);
	ParamControlActive( &layerPG, I_TIELEN, !layerInherit);
	ParamControlActive( &layerPG, I_TIEWID, !layerInherit);
	ParamControlActive( &layerPG, I_TIESPC, !layerInherit);

	if (layerS) {
		if (!LoadFileListLoad(settingsCatalog, settingsName)) {
			settingsName[0] = '\0';
			layers[inx].settingsName[0] = '\0';
		}

	}
}

void ResetLayers(void)
{
	int inx;

	for (inx = 0; inx < NUM_LAYERS; inx++) {
		strcpy(layers[inx].name, inx == 0 ? _("Main") : "");
		layers[inx].visible = TRUE;
		layers[inx].frozen = FALSE;
		layers[inx].onMap = TRUE;
		layers[inx].module = FALSE;
		layers[inx].button_off = FALSE;
		layers[inx].inherit = TRUE;
		layers[inx].objCount = 0;
		strcpy(layers[inx].settingsName, "");
		DYNARR_RESET(int, layers[inx].layerLinkList);
		SetLayerColor(inx, layerColorTab[inx % COUNT(layerColorTab)]);


		if (inx < NUM_BUTTONS) {
			wButtonSetLabel(layer_btns[inx], (char*)show_layer_bmps[inx]);
		}
	}

	wControlSetBalloonText((wControl_p)layer_btns[0], _("Main"));

	for (inx = 1; inx < NUM_BUTTONS; inx++) {
		wControlSetBalloonText((wControl_p)layer_btns[inx], _("Show/Hide Layer"));
	}

	curLayer = -1;

	for (int i = 0; i < NUM_LAYERS; i++) {
		if (!layers[i].frozen) {
			curLayer = i;
			break;
		}
	}

	if (curLayer == -1) {
		ErrorMessage( MSG_NO_EMPTY_LAYER );
		layers[0].frozen = FALSE;
		curLayer = 0;
	}

	layerVisible = TRUE;
	layerFrozen = FALSE;
	layerOnMap = TRUE;
	layerModule = FALSE;
	layerInherit = FALSE; 
	layerColor = layers[0].color;
	layerUseColor = TRUE;
	strcpy(layerName, layers[0].name);
	strcpy(settingsName, layers[0].settingsName);

	LoadLayerLists();

	if (layerL) {
		ParamLoadControls(&layerPG);
		// ParamLoadMessage(&layerPG, I_COUNT, "0");
	}
}


void SaveLayers(void)
{
	layers_save = malloc(NUM_LAYERS * sizeof(layers[0]));

	CHECK(layers_save != NULL);

	for (int i = 0; i < NUM_LAYERS; i++) {
		layers[i].settingsName[0] = '\0';
	}

	memcpy(layers_save, layers, NUM_LAYERS * sizeof layers[0]);
	ResetLayers();
}

void RestoreLayers(void)
{
	int inx;
	char * label;
	wDrawColor color;
	CHECK(layers_save != NULL);
	memcpy(layers, layers_save, NUM_LAYERS * sizeof layers[0]);
	free(layers_save);

	for (inx = 0; inx < NUM_BUTTONS; inx++) {
		color = layers[inx].color;
		layers[inx].color = -1;
		SetLayerColor(inx, color);

		if (layers[inx].name[0] == '\0') {
			if (inx == 0) {
				label = _("Main");
			} else {
				label = _("Show/Hide Layer");
			}
		} else {
			label = layers[inx].name;
		}

		wControlSetBalloonText((wControl_p)layer_btns[inx], label);
	}

	if (layerL) {
		ParamLoadControls(&layerPG);
		//ParamLoadMessage(&layerPG, I_COUNT, "0");
	}

	LoadLayerLists();
}


/**
 * Scan opened directory for the next settings file
 *
 * \param dir IN opened directory handle
 * \param dirName IN name of directory
 * \param fileName OUT fully qualified filename
 *
 * \return TRUE if file found, FALSE if not
 */

static bool
GetNextSettingsFile(DIR *dir, const char *dirName, char **fileName)
{
	bool done = false;
	bool res = false;

	/*
	* get all files from the directory
	*/
	while (!done) {
		struct stat fileState;
		struct dirent *ent;

		ent = readdir(dir);

		if (ent) {
			if (!XtcStricmp(FindFileExtension(ent->d_name), "xset")) {
				/* create full file name and get the state for that file */
				MakeFullpath(fileName, dirName, ent->d_name, NULL);

				if (stat(*fileName, &fileState) == -1) {
					fprintf(stderr, "Error getting file state for %s\n", *fileName);
					continue;
				}

				/* ignore any directories */
				if (!(fileState.st_mode & S_IFDIR)) {
					done = true;
					res = true;
				}
			}
		} else {
			done = true;
			res = false;
		}
	}
	return (res);
}


/*
 * Get all the settings files in the working directory
 */

static CatalogEntry *
ScanSettingsDirectory(Catalog *catalog, const char *dirName)
{
	DIR *d;
	CatalogEntry *newEntry = catalog->head;
	char contents[STR_SHORT_SIZE];

	d = opendir(dirName);
	if (d) {
		char *fileName = NULL;

		while (GetNextSettingsFile(d, dirName, &fileName)) {
			char *contents_start = strrchr(fileName, PATH_SEPARATOR[0]);
			if (contents_start[0] == '/') { contents_start++; }
			char *contents_end = strchr(contents_start, '.');
			if (contents_end[0] == '.') { contents_end[0] = '\0'; }
			strcpy(contents, contents_start);
			contents_end[0] = '.';
			newEntry = InsertInOrder(catalog, contents, NULL);
			UpdateCatalogEntry(newEntry, fileName, contents, NULL);
			free(fileName);
			fileName = NULL;
		}
		closedir(d);
	}

	return (newEntry);
}



/*****************************************************************************
*
* FILE READ/WRITE
*
*/

BOOL_T ReadLayers(char * line)
{
	char *name, *layerLinkList, *layerSettingsName, *extra;
	int inx, visible, frozen, color, onMap, sclInx, module, dontUseColor,
	    ColorFlags, button_off, inherit;
	double minRad, maxGrd, tieLen, tieWid, tieSpc;
	unsigned long rgb;

	/* older files didn't support layers */

	if (paramVersion < 7) {
		return TRUE;
	}

	/* set the current layer */

	if (strncmp(line, "CURRENT", 7) == 0) {
		curLayer = atoi(line + 7) - 1;

		if (!IsLayerValid(curLayer)) {

			curLayer = 0;
		}

		if (layers[curLayer].frozen) {
			ErrorMessage( MSG_NOT_UNFROZEN_LAYER );
			layers[curLayer].frozen = FALSE;
		}

		if (layerL) {
			wListSetIndex(layerL, curLayer);
		}

		if (setLayerL) {
			wListSetIndex(setLayerL, curLayer);
		}

		return TRUE;
	}

	if (strncmp(line, "LINK", 4) == 0) {
		if (!GetArgs(line + 4, "dq", &inx, &layerLinkList)) {
			return FALSE;
		}
		PutLayerListArray(inx - 1, layerLinkList);
		return TRUE;
	}

	if (strncmp(line, "SET", 3) == 0) {
		if (!GetArgs(line + 3, "dq", &inx, &layerSettingsName)) {
			return FALSE;
		}
		strcpy(layers[inx].settingsName, layerSettingsName);
		return TRUE;
	}

	/* get the properties for a layer from the file and update the layer accordingly */
	/* No Scale/tie data version */
	if (!GetArgs(line, "dddduddddqc", &inx, &visible, &frozen, &onMap, &rgb,
			&module, &dontUseColor, &ColorFlags, &button_off, &name, &extra)) {
		return FALSE;
	}
	/* Check for old version: name here */
	if (extra && strlen(extra) > 0) {
		/* tie data version */
		if (!GetArgs(extra, "dufffff", &inherit, &sclInx, &minRad, &maxGrd, 
				&tieLen, &tieWid, &tieSpc)) {
			return FALSE;
		}
	}
	else {
		sclInx = GetLayoutCurScale();
		inherit = TRUE;
		minRad = 0.0;
		maxGrd = 0.0;
		tieLen = 0.0;
		tieWid = 0.0;
		tieSpc = 0.0;
	}

	// Provide defaults
	if ( minRad < EPSILON ) {
		minRad = GetScaleMinRadius(sclInx);
	}

	if (paramVersion < 9) {
		if ((int)rgb < COUNT( oldColorMap ) ) {
			rgb = wRGB(oldColorMap[(int)rgb][0], oldColorMap[(int)rgb][1],
			           oldColorMap[(int)rgb][2]);
		} else {
			rgb = 0;
		}
	}

	if (inx < 0 || inx >= NUM_LAYERS) {
		return FALSE;
	}

	tieData_t td = {TRUE, tieLen, tieWid, tieSpc};
	ValidateTieData(&td);
	if ( !td.valid ) {
		td = GetScaleTieData(sclInx);
	}
	color = wDrawFindColor(rgb);
	SetLayerColor(inx, color);
	strncpy(layers[inx].name, name, sizeof layers[inx].name);
	layers[inx].visible = visible;
	layers[inx].frozen = frozen;
	layers[inx].onMap = onMap;
	layers[inx].scaleInx = sclInx;
	layers[inx].minTrackRadius = minRad;
	layers[inx].maxTrackGrade = maxGrd;
	layers[inx].tieData = td;
	layers[inx].module = module;
	layers[inx].color = color;
	layers[inx].useColor = !dontUseColor;
	layers[inx].button_off = button_off;
	layers[inx].inherit = inherit;
	GetScaleGauge(sclInx, &layers[inx].scaleDescInx, &layers[inx].gaugeInx);

	colorTrack = ColorFlags & 1; //Make sure globals are set
	colorDraw = ColorFlags & 2;

	if (inx < NUM_BUTTONS && !layers[inx].button_off) {
		if (strlen(name) > 0) {
			wControlSetBalloonText((wControl_p)layer_btns[(int)inx], layers[inx].name);
		}
		wButtonSetBusy(layer_btns[(int)inx], visible);
	}
	MyFree(name);

	// The last layer will set this correctly
	maxLayer = inx;

	return TRUE;
}

/**
 * Find out whether layer information should be saved to the layout file.
 * Usually only layers where settings are off from the default are written.
 * NOTE: as a fix for a problem with XTrkCadReader a layer definition is
 * written for each layer that is used.
 *
 * \param layerNumber IN index of the layer
 * \return TRUE if configured, FALSE if not
 */

BOOL_T
IsLayerConfigured(unsigned int layerNumber)
{
	return (layers[layerNumber].name[0] ||
	        !layers[layerNumber].visible ||
	        layers[layerNumber].frozen ||
	        !layers[layerNumber].onMap ||
	        layers[layerNumber].module ||
	        layers[layerNumber].button_off ||
	        layers[layerNumber].color != layerColorTab[layerNumber % (COUNT(
	                                layerColorTab))] ||
	        layers[layerNumber].layerLinkList.cnt > 0 ||
	        layers[layerNumber].objCount);
}


/**
 * Save the layer information to the file.
 *
 * \paran f IN open file handle
 * \return always TRUE
 */

BOOL_T WriteLayers(FILE * f)
{
	unsigned int inx;

	int ColorFlags = 0;

	if (colorTrack) { ColorFlags |= 1; }
	if (colorDraw) { ColorFlags |= 2; }

	for (inx = 0; inx < NUM_LAYERS; inx++) {
		if (IsLayerConfigured(inx)) {
			fprintf(f,
				"LAYERS %u %d %d %d %ld %d %d %d %d \"%s\" %d %lu %.6f %.6f %.6f %.6f %.6f\n",
				inx,
				layers[inx].visible,
				layers[inx].frozen,
				layers[inx].onMap,
				wDrawGetRGB(layers[inx].color),
				layers[inx].module,
				layers[inx].useColor ? 0 : 1, 
				ColorFlags,
				layers[inx].button_off, 
				PutTitle(layers[inx].name), 
				layers[inx].inherit,
				layers[inx].scaleInx, 
				layers[inx].minTrackRadius,
				layers[inx].maxTrackGrade, 
				layers[inx].tieData.length,
				layers[inx].tieData.width, 
				layers[inx].tieData.spacing
			);
		}
	}

	fprintf(f, "LAYERS CURRENT %u\n", (curLayer + 1));

	for (inx = 0; inx < NUM_LAYERS; inx++) {
		unsigned int layerInx = inx + 1;
		GetLayerLinkString(inx, layerLinkList);
		if (IsLayerConfigured(inx) && strlen(layerLinkList) > 0) {
			fprintf(f, "LAYERS LINK %u \"%s\"\n", layerInx, layerLinkList);
		}
		if (IsLayerConfigured(inx) && layers[inx].settingsName[0]) {
			fprintf(f, "LAYERS SET %u \"%s\"\n", layerInx, layers[inx].settingsName);
		}
	}
	return TRUE;
}

/*****************************************************************************
*
* DIALOG & MENU
*
*/

/**
* This function is called when the Done button on the layer dialog is pressed. It hides the layer dialog and
* updates the layer information.
*
* \param IN ignored
*
*/
static void LayerOk(void * unused)
{
	LayerSelect(layerSelected);

	if (newLayerCount != layerCount) {
		layoutLayerChanged = TRUE;

		if (newLayerCount > NUM_BUTTONS) {
			newLayerCount = NUM_BUTTONS;
		}

		layerCount = newLayerCount;
	}

	if (layoutLayerChanged) {
		MainProc(mainW, wResize_e, NULL, NULL);
	}

	wHide(layerW);
}


static void LayerDlgUpdate(
	paramGroup_p pg,
	int inx,
	void * valueP)
{
	switch (inx) {
	case I_LIST:
		LayerSelect((wIndex_t) * (long*)valueP);
		break;

	case I_NAME:
		LayerUpdate();
		break;

	case I_MAP:
		layerRedrawMap = TRUE;
		/* No Break */
	case I_VIS:
	case I_FRZ:
	case I_MOD:
	case I_BUT:
	case I_DEF:
		LayerUpdate();
		UpdateLayerDlg(layerSelected);
		break;

	case I_SCALE:
		LoadGaugeList((wList_p)layerPLs[I_GAUGE].control, *((int *)valueP));
		// set the first entry as default, usually the standard gauge for a scale
		wListSetIndex((wList_p)layerPLs[I_GAUGE].control, 0);
		break;

	case I_TIELEN:
	case I_TIEWID:
	case I_TIESPC:
		ValidateTieData(&layerTieData);
		r_tieData.rangechecks = layerTieData.valid ? PDO_NORANGECHECK_LOW | PDO_NORANGECHECK_HIGH : 0;
		break;

	case I_SETTINGS:
		if (strcmp((char*)wListGetItemContext(settingsListL,
			(wIndex_t) * (long*)valueP), " ") == 0) {
			settingsName[0] = '\0';
		} else {
			strcpy(settingsName, (char*)wListGetItemContext(settingsListL,
				(wIndex_t) * (long*)valueP));
		}
		break;
	}
}


static void DoLayer(void * unused)
{
	if (layerW == NULL) {
		layerW = ParamCreateDialog(&layerPG, MakeWindowTitle(_("Layers")), _("Done"),
			LayerOk, wHide, TRUE, NULL, 0, LayerDlgUpdate);
		GetScaleGauge(layerScaleInx, &layerScaleDescInx, &layerGaugeInx);
		LoadScaleList(scaleL);
		LoadGaugeList(gaugeL, layerScaleDescInx); 	
	}

	if (settingsCatalog) { CatalogDiscard(settingsCatalog); }
	else { settingsCatalog = InitCatalog(); }
	ScanSettingsDirectory(settingsCatalog, wGetAppWorkDir());


	/* set the globals to the values for the current layer */
	UpdateLayerDlg(curLayer);
	layerRedrawMap = FALSE;
	wShow(layerW);
	layoutLayerChanged = FALSE;
}

#include "bitmaps/background.xpm"

void InitLayers(void)
{
	unsigned int i;
	wPrefGetInteger(PREFSECT, "layer-button-count", &layerCount, layerCount);

	for (i = 0; i < COUNT(layerRawColorTab); i++) {
		layerColorTab[i] = wDrawFindColor(layerRawColorTab[i]);
	}

	/* create the bitmaps for the layer buttons */
	/* all bitmaps have to have the same dimensions */
	for (int i = 0; i < NUM_LAYERS; i++) {
		// char *bits = (show_layer_bits[i] + iconSize * sizeof(char*)));
		show_layer_bmps[i] = wIconCreateBitMap(lbits_width[iconSize],
		                                       lbits_height[iconSize],
		                                       show_layer_bits[iconSize][i],
		                                       layerColorTab[i % (COUNT(layerColorTab))]);
		layers[i].color = layerColorTab[i % (COUNT(layerColorTab))];
		layers[i].useColor = TRUE;
	}

	/* layer list for toolbar */
	setLayerL = wDropListCreateForToolbar(mainW, 0, 0, "cmdLayerSet", NULL, 0, 10, 200, NULL,
	                            SetCurrLayer, NULL);
	wControlSetBalloonText((wControl_p)setLayerL, GetBalloonHelpStr("cmdLayerSet"));
	AddToolbarControl((wControl_p)setLayerL, IC_MODETRAIN_TOO);

	backgroundB = AddToolbarButton("cmdBackgroundShow",
	                               wIconCreatePixMap(background_xpm[iconSize]), 0,
	                               BackgroundToggleShow, NULL);
	/* add the help text */
	wControlSetBalloonText((wControl_p)backgroundB, _("Show/Hide Background"));
	wControlActive((wControl_p)backgroundB, FALSE);

	for (int i = 0; i < NUM_LAYERS; i++) {
		char *layerName;

		if (i < NUM_BUTTONS) {
			/* create the layer button */
			sprintf(message, "cmdLayerShow%u", i);
			layer_btns[i] = AddToolbarButton(message, show_layer_bmps[i], IC_MODETRAIN_TOO, FlipLayer, I2VP(i) );
			/* set state of button */
			wButtonSetBusy(layer_btns[i], 1);
		}

		layerName = FormatLayerName(i);
		wListAddValue(setLayerL, layerName, NULL, I2VP(i));
		free(layerName);
	}

	AddPlaybackProc("SETCURRLAYER", PlaybackCurrLayer, NULL);
	AddPlaybackProc("LAYERS", (playbackProc_p)ReadLayers, NULL);
}

addButtonCallBack_t InitLayersDialog(void)
{
	ParamRegister(&layerPG);
	RegisterChangeNotification(LayerChange);
	return &DoLayer;
}
