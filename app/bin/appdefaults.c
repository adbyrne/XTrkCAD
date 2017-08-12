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
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <Windows.h>

#include "common.h"
#include "wlib.h"

enum defaultTypes {
	INTEGERCONSTANT,
	FLOATCONSTANT,
	STRINGCONSTANT,
	INTEGERFUNC,
	FLOATFUNC,
	STRINGFUNC
};


static int GetLocalMeasureSystem(struct appDefault *ptrDefault);
static int GetLocalDistanceFormat(struct appDefault *ptrDefault);
static char *GetLocalPopularScale(struct appDefault *ptrDefault);
static double GetLocalRoomSize(struct appDefault *ptrDefault);

struct appDefault {
	char *defaultKey;						/**< the key used to access the value */
	bool wasUsed;							/**< value has already been used on this run */
	enum defaultTypes valueType;			/**< type of default, constant or pointer to a function */
	union
	{
		int		intValue;
		double  floatValue;
		char    *stringValue;
		int		(*intFunc)(struct appDefault *);
		double  (*floatFunc)(struct appDefault *);			/**< if pointer, the function returns the default */
		char   *(*stringFunc)(struct appDefault *);
	} defaultValue;
};

struct appDefault xtcDefaults[] = {
	{ "DialogItem.cmdopt-preselect", 0, INTEGERCONSTANT,{ .intValue = 1 } },			/**< default command is select */
	{ "DialogItem.pref-dstfmt", 0, INTEGERFUNC,{ .intFunc = GetLocalDistanceFormat } },	/**< number format for distances */
	{ "DialogItem.pref-units", 0, INTEGERFUNC,{ .intFunc = GetLocalMeasureSystem } },	/**< default unit depends on region */
	{ "draw.roomsizeX", 0, FLOATFUNC, {.floatFunc = GetLocalRoomSize }},				/**< layout width */
	{ "draw.roomsizeY", 0, FLOATFUNC,{ .floatFunc = GetLocalRoomSize } },				/**< layout depth */
	{ "misc.scale", 0, STRINGFUNC, { .stringFunc = GetLocalPopularScale}},				/**< the (probably) most popular scale for a region */
};

#define DEFAULTCOUNT (sizeof(xtcDefaults)/sizeof(xtcDefaults[0]))

static char regionCode[3];					/**< will be initialized to the locale's region code */

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
	if (r >= l)
	{
		int mid = l + (r - l) / 2;
		int res = strcmp(key, arr[mid].defaultKey );

		// If the element is present at the middle itself
		if (!res)  return mid;

		// If the array size is 1 
		if (r == 0) return -1;

		// If element is smaller than mid, then it can only be present
		// in left subarray
		if (res < 0) return binarySearch(arr, l, mid - 1, key);

		// Else the element can only be present in right subarray
		return binarySearch(arr, mid + 1, r, key);
	}

	// We reach here when element is not present in array
	return -1;
}

struct appDefault *
FindDefault(struct appDefault *defaultValues, const char *section, const char *name)
{
	char *searchString = malloc(strlen(section) + strlen(name) + 2); //includes separator and terminating \0
	int res;
	sprintf(searchString, "%s.%s", section, name);

	res = binarySearch(defaultValues, 0, DEFAULTCOUNT-1, searchString);
	free(searchString);

	if (res != -1 && defaultValues[res].wasUsed == FALSE ) {
		defaultValues[res].wasUsed = TRUE;
		return(defaultValues + res);
	} else {
		return (NULL);
	}
}

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
		char *ptr;
		pLang = getenv("LANG");
		if(pLang) {
			ptr = strchr(pLang, '-');
			if (ptr) {
				strncpy(regionCode, ptr, 2);
			}
	}
#endif
}

/**
 * For the US the classical 4x8 sheet is used as default size. in the metric world 1,25x2,0m is used.
 */

static double
GetLocalRoomSize(struct appDefault *ptrDefault)
{
	if (!regionCode[0]) {
		InitializeRegionCode();
	}

	if (!strcmp(ptrDefault->defaultKey, "draw.roomsizeX")) {
		return(strcmp(regionCode, "US") ? 125.0/2.54 : 48);
	}
	if (!strcmp(ptrDefault->defaultKey, "draw.roomsizeY")) {
		return(strcmp(regionCode, "US") ? 200.0 / 2.54 : 96);
	}
	return(0.0);		// should never get here
}

/**
 * The most popular scale is supposed to be HO except for UK where OO is assumed.
 */

static char *
GetLocalPopularScale(struct appDefault *ptrDefault)
{
	if (!regionCode[0]) {
		InitializeRegionCode();
	}
	return(strcmp(regionCode, "UK") ? "HO" : "OO");
}

/**
 *	The measurement system is english for the US and metric elsewhere
 */
static int 
GetLocalMeasureSystem(struct appDefault *ptrDefault)
{
	if (!regionCode[0]) {
		InitializeRegionCode();
	}
	return(strcmp(regionCode, "US") ? 1 : 0);
}

/**
*	The distance format is 999.9 cm for metric and ?? for english
*/
static int
GetLocalDistanceFormat(struct appDefault *ptrDefault)
{
	if (!regionCode[0]) {
		InitializeRegionCode();
	}
	return(strcmp(regionCode, "US") ? 8 : 0);
}

wBool_t
wPrefGetIntegerExt(const char *section, const char *name, long *result, long defaultValue)
{
	struct appDefault *thisDefault;

	thisDefault = FindDefault(xtcDefaults, section, name);
	if (thisDefault) {
		if (thisDefault->valueType == INTEGERCONSTANT) {
			defaultValue = thisDefault->defaultValue.intValue;
		} else {
			defaultValue = (thisDefault->defaultValue.intFunc)(thisDefault);
		}
	}
	return (wPrefGetIntegerBasic(section, name, result, defaultValue));
}


wBool_t
wPrefGetFloatExt(const char *section, const char *name, double *result, double defaultValue)
{
	struct appDefault *thisDefault;

	thisDefault = FindDefault(xtcDefaults, section, name);
	if (thisDefault) {
		if (thisDefault->valueType == FLOATCONSTANT) {
			defaultValue = thisDefault->defaultValue.floatValue;
		}
		else {
			defaultValue = (thisDefault->defaultValue.floatFunc)(thisDefault);
		}
	}
	return (wPrefGetFloatBasic(section, name, result, defaultValue));
}


char *
wPrefGetStringExt(const char *section, const char *name)
{
	struct appDefault *thisDefault;
	char *prefString;
	char *defaultValue;

	thisDefault = FindDefault(xtcDefaults, section, name);
	if (thisDefault) {
		if (thisDefault->valueType == STRINGCONSTANT) {
			defaultValue = thisDefault->defaultValue.stringValue;
		}
		else {
			defaultValue = (thisDefault->defaultValue.stringFunc)(thisDefault);
		}
		prefString = wPrefGetStringBasic(section, name);
		return (prefString ? prefString : defaultValue);
	}
	else {
		return (wPrefGetStringBasic(section, name));
	}
}