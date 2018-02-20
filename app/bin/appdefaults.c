/** \file appdefaults.c
* Provide defaults, mostly for first run of the program.
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
*  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifdef WINDOWS
#include <Windows.h>
#include <malloc.h>
#endif

#include "common.h"
#include "custom.h"
#include "fileio.h"
#include "paths.h"
#include "wlib.h"

enum defaultTypes {
    INTEGERCONSTANT,
    FLOATCONSTANT,
    STRINGCONSTANT,
    INTEGERFUNCTION,
    FLOATFUNCTION,
    STRINGFUNCTION
};

struct appDefault {
    char *defaultKey;						/**< the key used to access the value */
    bool wasUsed;							/**< value has already been used on this run */
    enum defaultTypes
    valueType;			/**< type of default, constant or pointer to a function */
    union {
        int 	intValue;
        double  floatValue;
        char    *stringValue;
        int		(*intFunction)(struct appDefault *, void *);
        double	(*floatFunction)(struct appDefault *, void *);		
        char	*(*stringFunction)(struct appDefault *, void *);
    } defaultValue;
    void *additionalData;
};

static int GetLocalMeasureSystem(struct appDefault *ptrDefault,
                                 void *additionalData);
static int GetLocalDistanceFormat(struct appDefault *ptrDefault,
                                  void *additionalData);
static char *GetLocalPopularScale(struct appDefault *ptrDefault,
                                  void *additionalData);
static double GetLocalRoomSize(struct appDefault *ptrDefault,
                               void *additionalData);
static char *GetParamFullPath(struct appDefault *ptrDefault,
                              void *additionalData);
static char *GetParamPrototype(struct appDefault *ptrDefault,
								void *additionalData);

/**
 * List of application default settings. As this is searched by binary search, the list has to be kept sorted
 * alphabetically for the key, the first element
 * Also the search is case sensitive on this field.
 */

struct appDefault xtcDefaults[] = {
    { "DialogItem.cmdopt-preselect", 0, INTEGERCONSTANT,{ .intValue = 1 } },			/**< default command is select */
	{ "DialogItem.grid-horzenable", 0, INTEGERCONSTANT, { .intValue = 0 }},
	{ "DialogItem.grid-vertenable", 0, INTEGERCONSTANT,{ .intValue = 0 } },
	{ "DialogItem.pref-dstfmt", 0, INTEGERFUNCTION,{ .intFunction = GetLocalDistanceFormat } },	/**< number format for distances */
    { "DialogItem.pref-units", 0, INTEGERFUNCTION,{ .intFunction = GetLocalMeasureSystem } },	/**< default unit depends on region */
	{ "DialogItem.rgbcolor-exception", 0, INTEGERCONSTANT, { .intValue = 15923462 }},		/**< rich yellow as exception color */
	{ "Parameter File Map.British stock", 0, STRINGFUNCTION,{ .stringFunction = GetParamFullPath }, "br.xtp" },
	{ "Parameter File Map.European stock", 0, STRINGFUNCTION,{ .stringFunction = GetParamFullPath }, "eu.xtp" },
	{ "Parameter File Map.NMRA RP12-25 Feb 2015 O scale Turnouts", 0, STRINGFUNCTION,{ .stringFunction = GetParamFullPath }, "nmra-o.xtp" },
    { "Parameter File Map.NMRA RP12-27 Feb 2015 S Scale Turnouts", 0, STRINGFUNCTION,{ .stringFunction = GetParamFullPath }, "nmra-s.xtp"  },
    { "Parameter File Map.NMRA RP12-31 Feb 2015 HO Scale Turnouts", 0, STRINGFUNCTION,{ .stringFunction = GetParamFullPath }, "nmra-ho.xtp" },
    { "Parameter File Map.NMRA RP12-33 Feb 2015 TT Scale Turnouts", 0, STRINGFUNCTION,{ .stringFunction = GetParamFullPath }, "nmra-tt.xtp" },
    { "Parameter File Map.NMRA RP12-35 Feb 2015 N Scale Turnouts", 0, STRINGFUNCTION,{ .stringFunction = GetParamFullPath }, "nmra-n.xtp" },
    { "Parameter File Map.NMRA RP12-37 Feb 2015 Z scale Turnouts", 0, STRINGFUNCTION,{ .stringFunction = GetParamFullPath }, "nmra-z.xtp" },
	{ "Parameter File Map.North American Prototypes", 0, STRINGFUNCTION,{ .stringFunction = GetParamFullPath }, "protoam.xtp" },
    { "Parameter File Map.Trees", 0, STRINGFUNCTION,{ .stringFunction = GetParamFullPath } , "trees.xtp" },
	{ "Parameter File Names.File1", 0, STRINGFUNCTION,{ .stringFunction = GetParamPrototype }},
	{ "Parameter File Names.File2", 0, STRINGCONSTANT,{ .stringValue = "Trees" } },
	{ "Parameter File Names.File3", 0, STRINGCONSTANT,{ .stringValue = "NMRA RP12-37 Feb 2015 Z scale Turnouts" } },
    { "Parameter File Names.File4", 0, STRINGCONSTANT,{ .stringValue = "NMRA RP12-35 Feb 2015 N Scale Turnouts" } },
    { "Parameter File Names.File5", 0, STRINGCONSTANT,{ .stringValue = "NMRA RP12-33 Feb 2015 TT Scale Turnouts" } },
    { "Parameter File Names.File6", 0, STRINGCONSTANT,{ .stringValue = "NMRA RP12-31 Feb 2015 HO Scale Turnouts" } },
    { "Parameter File Names.File7", 0, STRINGCONSTANT,{ .stringValue = "NMRA RP12-27 Feb 2015 S Scale Turnouts" } },
    { "Parameter File Names.File8", 0, STRINGCONSTANT,{ .stringValue = "NMRA RP12-25 Feb 2015 O scale Turnouts" } },
    { "draw.roomsizeX", 0, FLOATFUNCTION, {.floatFunction = GetLocalRoomSize }},				/**< layout width */
    { "draw.roomsizeY", 0, FLOATFUNCTION,{ .floatFunction = GetLocalRoomSize } },				/**< layout depth */
    { "misc.scale", 0, STRINGFUNCTION, { .stringFunction = GetLocalPopularScale}},				/**< the (probably) most popular scale for a region */
};

#define DEFAULTCOUNT (sizeof(xtcDefaults)/sizeof(xtcDefaults[0]))


static long bFirstRun;						/**< TRUE if appl is run the first time */
static char regionCode[3];					/**< will be initialized to the locale's region code */

static wBool_t(*GetIntegerPref)(const char *, const char *, long *, long) = wPrefGetIntegerExt;	/**< pointer to active integer pref getter */
static wBool_t(*GetFloatPref)(const char *, const char *, double *, double) = wPrefGetFloatExt; /**< pointer to active float pref getter */
static char *(*GetStringPref)(const char *, const char *) = wPrefGetStringExt;					/**< pointer to active string pref getter */

/**
 * A recursive binary search function. It returns location of x in
 * given array arr[l..r] is present, otherwise -1
 * Taken from http://www.geeksforgeeks.org/binary-search/ and modified
 *
 * \param arr IN array to search
 * \param l IN starting index
 * \param r IN highest index in array
 * \param key IN key to search
 * \return index if found, -1 otherwise
 */

static int binarySearch(struct appDefault arr[], int l, int r, char *key)
{
    if (r >= l) {
        int mid = l + (r - l) / 2;
        int res = strcmp(key, arr[mid].defaultKey);

        // If the element is present at the middle itself
        if (!res) {
            return mid;
        }

        // If the array size is 1
        if (r == 0) {
            return -1;
        }

        // If element is smaller than mid, then it can only be present
        // in left subarray
        if (res < 0) {
            return binarySearch(arr, l, mid - 1, key);
        }

        // Else the element can only be present in right subarray
        return binarySearch(arr, mid + 1, r, key);
    }

    // We reach here when element is not present in array
    return -1;
}

/**
 * Lookup default  for a value
 *
 * \param defaultValues IN array of all default values
 * \param section IN default's section
 * \param name IN default's name
 * \return pointer to default if found, NULL otherwise
 */
struct appDefault *
FindDefault(struct appDefault *defaultValues, const char *section,
            const char *name)
{
    char *searchString = malloc(strlen(section) + strlen(name) +
                                2); //includes separator and terminating \0
    int res;
    sprintf(searchString, "%s.%s", section, name);

    res = binarySearch(defaultValues, 0, DEFAULTCOUNT-1, searchString);
    free(searchString);

    if (res != -1 && defaultValues[res].wasUsed == FALSE) {
        defaultValues[res].wasUsed = TRUE;
        return (defaultValues + res);
    } else {
        return (NULL);
    }
}
/**
 * Get the application's default region code. On Windows, the system's API is used.
 * On *ix the environment variable LANG is supposed to contain a value in the
 * format ll_RR* where rr is the two character region code.
 */
static void
InitializeRegionCode(void)
{
    strcpy(regionCode, "US");

#ifdef WINDOWS
    {
        LCID lcid;
        char iso3166[10];

        lcid = GetThreadLocale();
        GetLocaleInfo(lcid, LOCALE_SISO3166CTRYNAME, iso3166, sizeof(iso3166));
        strncpy(regionCode, iso3166, 2);
    }
#else
    {
        char *pLang;
        pLang = getenv("LANG");

        if (pLang) {
            char *ptr;
            ptr = strpbrk(pLang, "_-");

            if (ptr) {
                strncpy(regionCode, ptr + 1, 2);
            }
        }
    }
#endif
}

/**
 * For the US the classical 4x8 sheet is used as default size. in the metric world 1,25x2,0m is used.
 */

static double
GetLocalRoomSize(struct appDefault *ptrDefault, void *data)
{
    if (!strcmp(ptrDefault->defaultKey, "draw.roomsizeY")) {
        return (strcmp(regionCode, "US") ? 125.0/2.54 : 48);
    }

    if (!strcmp(ptrDefault->defaultKey, "draw.roomsizeX")) {
        return (strcmp(regionCode, "US") ? 200.0 / 2.54 : 96);
    }

    return (0.0);		// should never get here
}

/**
 * The most popular scale is supposed to be HO except for UK where OO is assumed.
 */

static char *
GetLocalPopularScale(struct appDefault *ptrDefault, void *data)
{
    return (strcmp(regionCode, "GB") ? "HO" : "OO");
}

/**
 *	The measurement system is english for the US and metric elsewhere
 */
static int
GetLocalMeasureSystem(struct appDefault *ptrDefault, void *data)
{
    return (strcmp(regionCode, "US") ? 1 : 0);
}

/**
*	The distance format is 999.9 cm for metric and ?? for english
*/
static int
GetLocalDistanceFormat(struct appDefault *ptrDefault, void *data)
{
    return (strcmp(regionCode, "US") ? 8 : 5);
}

/**
* Prototype definitions currently only exist for US and British. So US 
* is assumed to be the default. 
*/

static char*
GetParamPrototype(struct appDefault *ptrDefault, void *additionalData)
{
	return (strcmp(regionCode, "GB") ? "North American Prototypes" : "British stock");
}

/**
 * The full path to the applications parameter directory
 */
static char *
GetParamFullPath(struct appDefault *ptrDefault, void *additionalData)
{
    char *str;
    MakeFullpath(&str, libDir, PARAM_SUBDIR, (char*)additionalData, (void *)0);
    return str;
}


/**
 * The following are three jump points for the correct implementation. Changing the funtion pointer
 * allows to switch from the extended default version to the basic implementation.
 */

wBool_t 
wPrefGetInteger(const char *section, const char *name, long *result, long defaultValue)
{
	return GetIntegerPref(section, name, result, defaultValue);
}

wBool_t
wPrefGetFloat(const char *section, const char *name, double *result, double defaultValue)
{
	return GetFloatPref(section, name, result, defaultValue);
}

char * 
wPrefGetString(const char *section, const char *name)
{
	return GetStringPref(section, name);
}

/**
 * Get an integer value from the configuration file. The is a wrapper for the real
 * file access and adds a region specific default value.
 *
 * \param section IN section in config file
 * \param name IN name in config file
 * \param result OUT pointer to result
 * \param defaultValue IN the default value to use if config is not found
 * \return returncode of wPrefGetIntegerBasic()
 */
wBool_t
wPrefGetIntegerExt(const char *section, const char *name, long *result,
                   long defaultValue)
{
    struct appDefault *thisDefault;

    thisDefault = FindDefault(xtcDefaults, section, name);

    if (thisDefault) {
        if (thisDefault->valueType == INTEGERCONSTANT) {
            defaultValue = thisDefault->defaultValue.intValue;
        } else {
            defaultValue = (thisDefault->defaultValue.intFunction)(thisDefault,
                           thisDefault->additionalData);
        }
    }

    return (wPrefGetIntegerBasic(section, name, result, defaultValue));
}

/**
 * Get a float value from the configuration file. The is a wrapper for the real
 * file access and adds a region specific default value.
 *
 * \param section IN section in config file
 * \param name IN name in config file
 * \param result OUT pointer to result
 * \param defaultValue IN the default value to use if config is not found
 * \return returncode of wPrefGetFloatBasic()
 */

wBool_t
wPrefGetFloatExt(const char *section, const char *name, double *result,
                 double defaultValue)
{
    struct appDefault *thisDefault;

    thisDefault = FindDefault(xtcDefaults, section, name);

    if (thisDefault) {
        if (thisDefault->valueType == FLOATCONSTANT) {
            defaultValue = thisDefault->defaultValue.floatValue;
        } else {
            defaultValue = (thisDefault->defaultValue.floatFunction)(thisDefault,
                           thisDefault->additionalData);
        }
    }

    return (wPrefGetFloatBasic(section, name, result, defaultValue));
}

/**
 * Get a string from the configuration file. The is a wrapper for the real
 * file access and adds a region specific default value.
 *
 * \param section IN section in config file
 * \param name IN name in config file
 * \return returncode of wPrefGetStringBasic()
 */
char *
wPrefGetStringExt(const char *section, const char *name)
{
    struct appDefault *thisDefault;

    thisDefault = FindDefault(xtcDefaults, section, name);

    if (thisDefault) {
        char *prefString;
        char *defaultValue;

        if (thisDefault->valueType == STRINGCONSTANT) {
            defaultValue = thisDefault->defaultValue.stringValue;
        } else {
            defaultValue = (thisDefault->defaultValue.stringFunction)(thisDefault,
                           thisDefault->additionalData);
        }

        prefString = (char *)wPrefGetStringBasic(section, name);
        return (prefString ? prefString : defaultValue);
    } else {
        return ((char *)wPrefGetStringBasic(section, name));
    }
}

/**
 * Initialize the application default system. The flag firstrun is used to find
 * out whether the application was run before. This is accomplished by trying
 * to read it from the configuration file. As it is only written after this 
 * test, it can never be found on the first run of the application ie. when the
 * configuration file does not exist yet.
 */

void 
InitAppDefaults(void)
{
	wPrefGetIntegerBasic( "misc", "firstrun", &bFirstRun, TRUE);
	if (bFirstRun) {
		wPrefSetInteger("misc", "firstrun", FALSE);
		InitializeRegionCode();
	} else {
		GetIntegerPref = wPrefGetIntegerBasic;
		GetFloatPref = wPrefGetFloatBasic;
		GetStringPref = wPrefGetStringBasic;
	}
}
