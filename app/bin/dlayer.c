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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "custom.h"
#include "dynstring.h"
#include "fileio.h"
#include "i18n.h"
#include "messages.h"
#include "param.h"
#include "track.h"

/*****************************************************************************
 *
 * LAYERS
 *
 */

#define NUM_BUTTONS		(99)
#define LAYERPREF_FROZEN  (1)
#define LAYERPREF_ONMAP	  (2)
#define LAYERPREF_VISIBLE (4)
#define LAYERPREF_SECTION ("Layers")
#define LAYERPREF_NAME 	"name"
#define LAYERPREF_COLOR "color"
#define LAYERPREF_FLAGS "flags"

unsigned int curLayer;
long layerCount = 10;
static long newLayerCount = 10;
static unsigned int layerCurrent = NUM_LAYERS;


static BOOL_T layoutLayerChanged = FALSE;

static wIcon_p show_layer_bmps[NUM_BUTTONS];
static wButton_p layer_btns[NUM_BUTTONS];	/**< layer buttons on toolbar */

/** Layer selector on toolbar */
static wList_p setLayerL;

/** Describe the properties of a layer */
typedef struct {
    char name[STR_SHORT_SIZE];			/**< Layer name */
    wDrawColor color;					/**< layer color, is an index into a color table */
    BOOL_T frozen;						/**< Frozen flag */
    BOOL_T visible;						/**< visible flag */
    BOOL_T onMap;						/**< is layer shown map */
    long objCount;						/**< number of objects on layer */
} layer_t;

static layer_t layers[NUM_LAYERS];
static layer_t *layers_save = NULL;


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
static void UpdateLayerDlg(void);

static void InitializeLayers(void LayerInitFunc(void), int newCurrLayer);
static void LayerPrefSave(void);
static void LayerPrefLoad(void);

int IsLayerValid(unsigned int layer)
{
    return (layer <= NUM_LAYERS);
}

BOOL_T GetLayerVisible(unsigned int layer)
{
    if (!IsLayerValid(layer)) {
        return TRUE;
    } else {
        return layers[layer].visible;
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


char * GetLayerName(unsigned int layer)
{
    if (!IsLayerValid(layer)) {
        return NULL;
    } else {
        return layers[layer].name;
    }
}

wDrawColor GetLayerColor(unsigned int layer)
{
    return layers[layer].color;
}


static void FlipLayer(unsigned int layer)
{
    wBool_t visible;

    if (!IsLayerValid(layer)) {
        return;
    }

    if (layer == curLayer && layers[layer].visible) {
        wButtonSetBusy(layer_btns[layer], layers[layer].visible);
        NoticeMessage(MSG_LAYER_HIDE, _("Ok"), NULL);
        return;
    }

    RedrawLayer(layer, FALSE);
    visible = !layers[layer].visible;
    layers[layer].visible = visible;

    if (layer<NUM_BUTTONS) {
        wButtonSetBusy(layer_btns[layer], visible != 0);
        wButtonSetLabel(layer_btns[layer], (char *)show_layer_bmps[layer]);
    }

    RedrawLayer(layer, TRUE);
}

static void SetCurrLayer(wIndex_t inx, const char * name, wIndex_t op,
                         void * listContext, void * arg)
{
    unsigned int newLayer = (unsigned int)inx;

    if (layers[newLayer].frozen) {
        NoticeMessage(MSG_LAYER_SEL_FROZEN, _("Ok"), NULL);
        wListSetIndex(setLayerL, curLayer);
        return;
    }

    curLayer = newLayer;

    if (!IsLayerValid(curLayer)) {
        curLayer = 0;
    }

    if (!layers[curLayer].visible) {
        FlipLayer(inx);
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

char *
FormatLayerName(unsigned int layerNumber)
{
    DynString string;// = NaS;
    char *result;
    DynStringMalloc(&string, 0);
    DynStringPrintf(&string,
                    "%2d %c %s",
                    layerNumber + 1,
                    (layers[layerNumber].objCount > 0 ? '+' : '-'),
                    layers[layerNumber].name);
    result = strdup(DynStringToCStr(&string));
    DynStringFree(&string);
    return result;
}


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


static char * show_layer_bits[NUM_BUTTONS] = {
    l1_bits, l2_bits, l3_bits, l4_bits, l5_bits, l6_bits, l7_bits, l8_bits, l9_bits, l10_bits,
    l11_bits, l12_bits, l13_bits, l14_bits, l15_bits, l16_bits, l17_bits, l18_bits, l19_bits, l20_bits,
    l21_bits, l22_bits, l23_bits, l24_bits, l25_bits, l26_bits, l27_bits, l28_bits, l29_bits, l30_bits,
    l31_bits, l32_bits, l33_bits, l34_bits, l35_bits, l36_bits, l37_bits, l38_bits, l39_bits, l40_bits,
    l41_bits, l42_bits, l43_bits, l44_bits, l45_bits, l46_bits, l47_bits, l48_bits, l49_bits, l50_bits,
    l51_bits, l52_bits, l53_bits, l54_bits, l55_bits, l56_bits, l57_bits, l58_bits, l59_bits, l60_bits,
    l61_bits, l62_bits, l63_bits, l64_bits, l65_bits, l66_bits, l67_bits, l68_bits, l69_bits, l70_bits,
    l71_bits, l72_bits, l73_bits, l74_bits, l75_bits, l76_bits, l77_bits, l78_bits, l79_bits, l80_bits,
    l81_bits, l82_bits, l83_bits, l84_bits, l85_bits, l86_bits, l87_bits, l88_bits, l89_bits, l90_bits,
    l91_bits, l92_bits, l93_bits, l94_bits, l95_bits, l96_bits, l97_bits, l98_bits, l99_bits,
};


static  long layerRawColorTab[] = {
    wRGB(0,  0,255),        /* blue */
    wRGB(0,  0,128),        /* dk blue */
    wRGB(0,128,  0),        /* dk green */
    wRGB(255,255,  0),      /* yellow */
    wRGB(0,255,  0),        /* green */
    wRGB(0,255,255),        /* lt cyan */
    wRGB(128,  0,  0),      /* brown */
    wRGB(128,  0,128),      /* purple */
    wRGB(128,128,  0),      /* green-brown */
    wRGB(255,  0,255)
};     /* lt-purple */
static  wDrawColor layerColorTab[COUNT(layerRawColorTab)];


static wWin_p layerW;
static char layerName[STR_SHORT_SIZE];
static wDrawColor layerColor;
static long layerVisible = TRUE;
static long layerFrozen = FALSE;
static long layerOnMap = TRUE;
static void LayerOk(void *);
static BOOL_T layerRedrawMap = FALSE;

#define ENUMLAYER_RELOAD (1)
#define ENUMLAYER_SAVE	(2)
#define ENUMLAYER_CLEAR (3)

static char *visibleLabels[] = { "", NULL };
static char *frozenLabels[] = { "", NULL };
static char *onMapLabels[] = { "", NULL };
static paramIntegerRange_t i0_20 = { 0, NUM_BUTTONS };

static paramData_t layerPLs[] = {
#define I_LIST	(0)
    { PD_DROPLIST, NULL, "layer", PDO_LISTINDEX|PDO_DLGNOLABELALIGN, (void*)250 },
#define I_NAME	(1)
    { PD_STRING, layerName, "name", PDO_NOPREF, (void*)(250-54), N_("Name") },
#define I_COLOR	(2)
    { PD_COLORLIST, &layerColor, "color", PDO_NOPREF, NULL, N_("Color") },
#define I_VIS	(3)
    { PD_TOGGLE, &layerVisible, "visible", PDO_NOPREF, visibleLabels, N_("Visible"), BC_HORZ|BC_NOBORDER },
#define I_FRZ	(4)
    { PD_TOGGLE, &layerFrozen, "frozen", PDO_NOPREF|PDO_DLGHORZ, frozenLabels, N_("Frozen"), BC_HORZ|BC_NOBORDER },
#define I_MAP	(5)
    { PD_TOGGLE, &layerOnMap, "onmap", PDO_NOPREF|PDO_DLGHORZ, onMapLabels, N_("On Map"), BC_HORZ|BC_NOBORDER },
#define I_COUNT (6)
    { PD_STRING, NULL, "object-count", PDO_NOPREF|PDO_DLGBOXEND, (void*)(80), N_("Count"), BO_READONLY },
    { PD_MESSAGE, N_("Personal Preferences"), NULL, PDO_DLGRESETMARGIN, (void *)180 },
    { PD_BUTTON, (void*)DoLayerOp, "reset", PDO_DLGRESETMARGIN, 0, N_("Load"), 0, (void *)ENUMLAYER_RELOAD },
    { PD_BUTTON, (void*)DoLayerOp, "save", PDO_DLGHORZ, 0, N_("Save"), 0, (void *)ENUMLAYER_SAVE },
    { PD_BUTTON, (void*)DoLayerOp, "clear", PDO_DLGHORZ | PDO_DLGBOXEND, 0, N_("Defaults"), 0, (void *)ENUMLAYER_CLEAR },
    { PD_LONG, &newLayerCount, "button-count", PDO_DLGBOXEND|PDO_DLGRESETMARGIN, &i0_20, N_("Number of Layer Buttons") },
};

static paramGroup_t layerPG = { "layer", 0, layerPLs, sizeof layerPLs/sizeof layerPLs[0] };

#define layerL	((wList_p)layerPLs[I_LIST].control)

/**
 * Load the layer settings to hard coded system defaults
 */

void
LayerSystemDefaults(void)
{
    int inx;

    for (inx=0; inx<NUM_LAYERS; inx++) {
        strcpy(layers[inx].name, inx==0?_("Main"):"");
        layers[inx].visible = TRUE;
        layers[inx].frozen = FALSE;
        layers[inx].onMap = TRUE;
        layers[inx].objCount = 0;
        SetLayerColor(inx, layerColorTab[inx%COUNT(layerColorTab)]);
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

    /* add all layers to both lists */
    for (inx=0; inx<NUM_LAYERS; inx++) {
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
 * Handle button presses for the layer dialog. For all button presses in the layer
 *	dialog, this function is called. The parameter identifies the button pressed and
 * the operation is performed.
 *
 * \param[IN] data identifier for the button prerssed
 * \return
 */

static void DoLayerOp(void * data)
{
    switch ((long)data) {
    case ENUMLAYER_CLEAR:
        InitializeLayers(LayerSystemDefaults, -1);
        break;

    case ENUMLAYER_SAVE:
        LayerPrefSave();
        break;

    case ENUMLAYER_RELOAD:
        LayerPrefLoad();
        break;
    }

    UpdateLayerDlg();

    if (layoutLayerChanged) {
        MainProc(mainW, wResize_e, NULL);
        layoutLayerChanged = FALSE;
        changed = TRUE;
        SetWindowTitle();
    }
}

/**
 * Update all dialogs and dialog elements after changing layers preferences. Once the global array containing
 * the settings for the labels has been changed, this function needs to be called to update all the user interface
 * elements to the new settings.
 */

static void
UpdateLayerDlg()
{
    int inx;
    /* update the globals for the layer dialog */
    layerVisible = layers[curLayer].visible;
    layerFrozen = layers[curLayer].frozen;
    layerOnMap = layers[curLayer].onMap;
    layerColor = layers[curLayer].color;
    strcpy(layerName, layers[curLayer].name);
    layerCurrent = curLayer;
    /* now re-load the layer list boxes */
    LoadLayerLists();
    sprintf(message, "%ld", layers[curLayer].objCount);
    ParamLoadMessage(&layerPG, I_COUNT, message);

    /* force update of the 'manage layers' dialogbox */
    if (layerL) {
        ParamLoadControls(&layerPG);
    }

    /* finally show the layer buttons with ballon text */
    for (inx = 0; inx < NUM_BUTTONS; inx++) {
        wButtonSetBusy(layer_btns[inx], layers[inx].visible != 0);
        wControlSetBalloonText((wControl_p)layer_btns[inx],
                               (layers[inx].name[0] != '\0' ? layers[inx].name :_("Show/Hide Layer")));
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

    /* Switch the current layer when requested */
    if (newCurrLayer != -1) {
        curLayer = newCurrLayer;
    }
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
    char layersSaved[ 3 * NUM_LAYERS + 1 ];			/* 0..99 plus separator */
    /* FIXME: values for layers that are configured to default now should be overwritten in the settings */
    layersSaved[ 0 ] = '\0';

    for (inx = 0; inx < NUM_LAYERS; inx++) {
        /* if a name is set that is not the default value or a color different from the default has been set,
            information about the layer needs to be saved */
        if ((layers[inx].name[0] && inx != 0) ||
                layers[inx].frozen || (!layers[inx].onMap) || (!layers[inx].visible) ||
                layers[inx].color != layerColorTab[inx%COUNT(layerColorTab)]) {
            sprintf(buffer, LAYERPREF_NAME ".%0u", inx);
            wPrefSetString(LAYERPREF_SECTION, buffer, layers[inx].name);
            sprintf(buffer, LAYERPREF_COLOR ".%0u", inx);
            wPrefSetInteger(LAYERPREF_SECTION, buffer, wDrawGetRGB(layers[inx].color));
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

            sprintf(buffer, LAYERPREF_FLAGS ".%0u", inx);
            wPrefSetInteger(LAYERPREF_SECTION, buffer, flags);

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
 * Load the settings for all layers from the preferences.
 */

static void
LayerPrefLoad(void)
{
    const char *prefString;
    long rgb;
    long flags;
    /* reset layer preferences to system default */
    LayerSystemDefaults();
    prefString = wPrefGetString(LAYERPREF_SECTION, "layers");

    if (prefString && prefString[ 0 ]) {
        char layersSaved[3 * NUM_LAYERS];
        strncpy(layersSaved, prefString, sizeof(layersSaved));
        prefString = strtok(layersSaved, ",");

        while (prefString) {
            int inx;
            char layerOption[20];
            const char *layerValue;
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
            sprintf(layerOption, LAYERPREF_COLOR ".%d", inx);
            wPrefGetInteger(LAYERPREF_SECTION, layerOption, &rgb,
                            layerColorTab[inx%COUNT(layerColorTab)]);
            color = wDrawFindColor(rgb);
            SetLayerColor(inx, color);
            /* get and set the flags */
            sprintf(layerOption, LAYERPREF_FLAGS ".%d", inx);
            wPrefGetInteger(LAYERPREF_SECTION, layerOption, &flags,
                            LAYERPREF_ONMAP | LAYERPREF_VISIBLE);
            layers[inx].frozen = ((flags & LAYERPREF_FROZEN) != 0);
            layers[inx].onMap = ((flags & LAYERPREF_ONMAP) != 0);
            layers[inx].visible = ((flags & LAYERPREF_VISIBLE) != 0);
            prefString = strtok(NULL, ",");
        }
    }
}

/**
 * Increment the count of objects on a given layer.
 *
 * \param index IN the layer to change
 */

void IncrementLayerObjects(unsigned int layer)
{
    assert(layer <= NUM_LAYERS);
    layers[layer].objCount++;
}

/**
* Decrement the count of objects on a given layer.
*
* \param index IN the layer to change
*/

void DecrementLayerObjects(unsigned int layer)
{
    assert(layer <= NUM_LAYERS);
    layers[layer].objCount--;
}

/**
 *	Count the number of objects on each layer and store result in layers data structure.
 */

void LayerSetCounts(void)
{
    int inx;
    track_p trk;

    for (inx=0; inx<NUM_LAYERS; inx++) {
        layers[inx].objCount = 0;
    }

    for (trk=NULL; TrackIterate(&trk);) {
        inx = GetTrkLayer(trk);

        if (inx >= 0 && inx < NUM_LAYERS) {
            layers[inx].objCount++;
        }
    }
}

/**
 * Reset layer options to their default values. The default values are loaded
 * from the preferences file.
 */

void
DefaultLayerProperties(void)
{
    InitializeLayers(LayerPrefLoad, 0);
    UpdateLayerDlg();

    if (layoutLayerChanged) {
        MainProc(mainW, wResize_e, NULL);
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

    if (!IsLayerValid(layerCurrent)) {
        return;
    }

    if (layerCurrent == curLayer && layerFrozen) {
        NoticeMessage(MSG_LAYER_FREEZE, _("Ok"), NULL);
        layerFrozen = FALSE;
        ParamLoadControl(&layerPG, I_FRZ);
    }

    if (layerCurrent == curLayer && !layerVisible) {
        NoticeMessage(MSG_LAYER_HIDE, _("Ok"), NULL);
        layerVisible = TRUE;
        ParamLoadControl(&layerPG, I_VIS);
    }

    if (strcmp(layers[(int)layerCurrent].name, layerName) ||
            layerColor != layers[(int)layerCurrent].color ||
            layers[(int)layerCurrent].visible != (BOOL_T)layerVisible ||
            layers[(int)layerCurrent].frozen != (BOOL_T)layerFrozen ||
            layers[(int)layerCurrent].onMap != (BOOL_T)layerOnMap) {
        changed = TRUE;
        SetWindowTitle();
    }

    if (layerL) {
        strncpy(layers[(int)layerCurrent].name, layerName,
                sizeof layers[(int)layerCurrent].name);
        layerFormattedName = FormatLayerName(layerCurrent);
        wListSetValues(layerL, layerCurrent, layerFormattedName, NULL, NULL);
        free(layerFormattedName);
    }

    layerFormattedName = FormatLayerName(layerCurrent);
    wListSetValues(setLayerL, layerCurrent, layerFormattedName, NULL, NULL);
    free(layerFormattedName);

    if (layerCurrent < NUM_BUTTONS) {
        if (strlen(layers[(int)layerCurrent].name)>0) {
            wControlSetBalloonText((wControl_p)layer_btns[(int)layerCurrent],
                                   layers[(int)layerCurrent].name);
        } else {
            wControlSetBalloonText((wControl_p)layer_btns[(int)layerCurrent],
                                   _("Show/Hide Layer"));
        }
    }

    redraw = (layerColor != layers[(int)layerCurrent].color ||
              (BOOL_T)layerVisible != layers[(int)layerCurrent].visible);

    if ((!layerRedrawMap) && redraw) {
        RedrawLayer((unsigned int)layerCurrent, FALSE);
    }

    SetLayerColor(layerCurrent, layerColor);

    if (layerCurrent<NUM_BUTTONS &&
            layers[(int)layerCurrent].visible!=(BOOL_T)layerVisible) {
        wButtonSetBusy(layer_btns[(int)layerCurrent], layerVisible);
    }

    layers[(int)layerCurrent].visible = (BOOL_T)layerVisible;
    layers[(int)layerCurrent].frozen = (BOOL_T)layerFrozen;
    layers[(int)layerCurrent].onMap = (BOOL_T)layerOnMap;

    if (layerRedrawMap) {
        DoRedraw();
    } else if (redraw) {
        RedrawLayer((unsigned int)layerCurrent, TRUE);
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

    layerCurrent = (unsigned int)inx;
    strcpy(layerName, layers[inx].name);
    layerVisible = layers[inx].visible;
    layerFrozen = layers[inx].frozen;
    layerOnMap = layers[inx].onMap;
    layerColor = layers[inx].color;
    sprintf(message, "%ld", layers[inx].objCount);
    ParamLoadMessage(&layerPG, I_COUNT, message);
    ParamLoadControls(&layerPG);
}

void ResetLayers(void)
{
    int inx;

    for (inx=0; inx<NUM_LAYERS; inx++) {
        strcpy(layers[inx].name, inx==0?_("Main"):"");
        layers[inx].visible = TRUE;
        layers[inx].frozen = FALSE;
        layers[inx].onMap = TRUE;
        layers[inx].objCount = 0;
        SetLayerColor(inx, layerColorTab[inx%COUNT(layerColorTab)]);

        if (inx<NUM_BUTTONS) {
            wButtonSetLabel(layer_btns[inx], (char*)show_layer_bmps[inx]);
        }
    }

    wControlSetBalloonText((wControl_p)layer_btns[0], _("Main"));

    for (inx=1; inx<NUM_BUTTONS; inx++) {
        wControlSetBalloonText((wControl_p)layer_btns[inx], _("Show/Hide Layer"));
    }

    curLayer = 0;
    layerVisible = TRUE;
    layerFrozen = FALSE;
    layerOnMap = TRUE;
    layerColor = layers[0].color;
    strcpy(layerName, layers[0].name);
    LoadLayerLists();

    if (layerL) {
        ParamLoadControls(&layerPG);
        ParamLoadMessage(&layerPG, I_COUNT, "0");
    }
}


void SaveLayers(void)
{
    layers_save = malloc(NUM_LAYERS * sizeof(layers[0]));

    if (layers_save == NULL) {
        abort();
    }

    memcpy(layers_save, layers, NUM_LAYERS * sizeof layers[0]);
    ResetLayers();
}

void RestoreLayers(void)
{
    int inx;
    char * label;
    wDrawColor color;
    assert(layers_save != NULL);
    memcpy(layers, layers_save, NUM_LAYERS * sizeof layers[0]);
    free(layers_save);

    for (inx=0; inx<NUM_BUTTONS; inx++) {
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
        ParamLoadMessage(&layerPG, I_COUNT, "0");
    }

    LoadLayerLists();
}

/**
 * This function is called when the Done button on the layer dialog is pressed. It hides the layer dialog and
 * updates the layer information.
 *
 * \param IN ignored
 *
 */

static void LayerOk(void * junk)
{
    LayerSelect(layerCurrent);

    if (newLayerCount != layerCount) {
        layoutLayerChanged = TRUE;

        if (newLayerCount > NUM_BUTTONS) {
            newLayerCount = NUM_BUTTONS;
        }

        layerCount = newLayerCount;
    }

    if (layoutLayerChanged) {
        MainProc(mainW, wResize_e, NULL);
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
        LayerSelect((wIndex_t)*(long*)valueP);
        break;

    case I_NAME:
        LayerUpdate();
        break;

    case I_MAP:
        layerRedrawMap = TRUE;
        break;
    }
}


static void DoLayer(void * junk)
{
    if (layerW == NULL) {
        layerW = ParamCreateDialog(&layerPG, MakeWindowTitle(_("Layers")), _("Done"),
                                   LayerOk, NULL, TRUE, NULL, 0, LayerDlgUpdate);
    }

    /* set the globals to the values for the current layer */
    UpdateLayerDlg();
    layerRedrawMap = FALSE;
    wShow(layerW);
    layoutLayerChanged = FALSE;
}


BOOL_T ReadLayers(char * line)
{
    char * name;
    int inx, visible, frozen, color, onMap;
    unsigned long rgb;

    /* older files didn't support layers */

    if (paramVersion < 7) {
        return TRUE;
    }

    /* set the current layer */

    if (strncmp(line, "CURRENT", 7) == 0) {
        curLayer = atoi(line+7);

        if (!IsLayerValid(curLayer)) {
            curLayer = 0;
        }

        if (layerL) {
            wListSetIndex(layerL, curLayer);
        }

        if (setLayerL) {
            wListSetIndex(setLayerL, curLayer);
        }

        return TRUE;
    }

    /* get the properties for a layer from the file and update the layer accordingly */

    if (!GetArgs(line, "ddddu0000q", &inx, &visible, &frozen, &onMap, &rgb,
                 &name)) {
        return FALSE;
    }

    if (paramVersion < 9) {
        if ((int)rgb < sizeof oldColorMap/sizeof oldColorMap[0]) {
            rgb = wRGB(oldColorMap[(int)rgb][0], oldColorMap[(int)rgb][1],
                       oldColorMap[(int)rgb][2]);
        } else {
            rgb = 0;
        }
    }

    if (inx < 0 || inx >= NUM_LAYERS) {
        return FALSE;
    }

    color = wDrawFindColor(rgb);
    SetLayerColor(inx, color);
    strncpy(layers[inx].name, name, sizeof layers[inx].name);
    layers[inx].visible = visible;
    layers[inx].frozen = frozen;
    layers[inx].onMap = onMap;
    layers[inx].color = color;

    if (inx<NUM_BUTTONS) {
        if (strlen(name) > 0) {
            wControlSetBalloonText((wControl_p)layer_btns[(int)inx], layers[inx].name);
        }

        wButtonSetBusy(layer_btns[(int)inx], visible);
    }
    MyFree(name);

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

bool
IsLayerConfigured(unsigned int layerNumber)
{
    return (!layers[layerNumber].visible ||
            layers[layerNumber].frozen ||
            !layers[layerNumber].onMap ||
            layers[layerNumber].color !=
            layerColorTab[layerNumber % (COUNT(layerColorTab))] ||
            layers[layerNumber].name[0] ||
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

    for (inx = 0; inx < NUM_LAYERS; inx++) {
        if (IsLayerConfigured(inx)) {
            fprintf(f, "LAYERS %u %d %d %d %ld %d %d %d %d \"%s\"\n",
                    inx,
                    layers[inx].visible,
                    layers[inx].frozen,
                    layers[inx].onMap,
                    wDrawGetRGB(layers[inx].color),
                    0, 0, 0, 0,
                    PutTitle(layers[inx].name));
        }
    }

    fprintf(f, "LAYERS CURRENT %u\n", curLayer);
    return TRUE;
}


void InitLayers(void)
{
    unsigned int i;
    wPrefGetInteger(PREFSECT, "layer-button-count", &layerCount, layerCount);

    for (i = 0; i<COUNT(layerRawColorTab); i++) {
        layerColorTab[i] = wDrawFindColor(layerRawColorTab[i]);
    }

    /* create the bitmaps for the layer buttons */
    /* all bitmaps have to have the same dimensions */
    for (i = 0; i<NUM_BUTTONS; i++) {
        show_layer_bmps[i] = wIconCreateBitMap(l1_width, l1_height, show_layer_bits[i],
                                               layerColorTab[i%(COUNT(layerColorTab))]);
        layers[i].color = layerColorTab[i%(COUNT(layerColorTab))];
    }

    /* layer list for toolbar */
    setLayerL = wDropListCreate(mainW, 0, 0, "cmdLayerSet", NULL, 0, 10, 200, NULL,
                                SetCurrLayer, NULL);
    wControlSetBalloonText((wControl_p)setLayerL, GetBalloonHelpStr("cmdLayerSet"));
    AddToolbarControl((wControl_p)setLayerL, IC_MODETRAIN_TOO);

    for (i = 0; i<NUM_LAYERS; i++) {
        char *layerName;

        if (i<NUM_BUTTONS) {
            /* create the layer button */
            sprintf(message, "cmdLayerShow%u", i);
            layer_btns[i] = wButtonCreate(mainW, 0, 0, message,
                                          (char*)(show_layer_bmps[i]),
                                          BO_ICON, 0, (wButtonCallBack_p)FlipLayer, (void*)(intptr_t)i);
            /* add the help text */
            wControlSetBalloonText((wControl_p)layer_btns[i], _("Show/Hide Layer"));
            /* put on toolbar */
            AddToolbarControl((wControl_p)layer_btns[i], IC_MODETRAIN_TOO);
            /* set state of button */
            wButtonSetBusy(layer_btns[i], 1);
        }

        layerName = FormatLayerName(i);
        wListAddValue(setLayerL, layerName, NULL, (void*)i);
        free(layerName);
    }

    AddPlaybackProc("SETCURRLAYER", PlaybackCurrLayer, NULL);
    AddPlaybackProc("LAYERS", (playbackProc_p)ReadLayers, NULL);
}


addButtonCallBack_t InitLayersDialog(void)
{
    ParamRegister(&layerPG);
    return &DoLayer;
}
