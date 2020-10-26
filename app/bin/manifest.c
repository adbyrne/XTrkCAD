/** \file manifest.c
 * JSON routines
 */
 /*  XTrkCad - Model Railroad CAD
  *  Copyright (C) 2018 Adam Richards and Martin Fischer
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
  
#include <string.h>

#include "cJSON.h"
#include "fileio.h"
#include "layout.h"
#include "misc2.h"
#include "paths.h"
#include "include/utf8convert.h"

extern int log_zip;

 /**********************************************************
  * Build JSON Manifest -  manifest.json
  * There are only two objects in the root -
  * - The layout object defines the correct filename for the layout
  * - The dependencies object is an arraylist of included elements
  *
  * Each element has a name, a filename and an arch-path (where in the archive it is located)
  * It may have other values - a common one the copy-path is where it was copied from the originators machine (info only)
  *
  * There is one reserved name - "background" which is for the image file that is used as a layout background
  *
  *\param IN nameOfLayout - the layout this is a manifest for
  *\param IN background - the full filepath to the background image (or NULL) -> TODO this will become an array with a count
  *\param IN DependencyDir - the relative path in the archive to the directory in which the included object(s) will be stored
  *
  *\returns a String containing the JSON object
  */

char* CreateManifest(char* nameOfLayout, char* background,
	char* dependencyDir)
{
	cJSON* manifest = cJSON_CreateObject();
	if (manifest != NULL) {
		char *copyOfFileName = MyStrdup(nameOfLayout);
		cJSON* a_object = cJSON_CreateObject();
		cJSON_AddItemToObject(manifest, "layout", a_object);
#ifdef WINDOWS
		copyOfFileName = Convert2UTF8(copyOfFileName);
#endif // WINDOWS
		cJSON_AddStringToObject(a_object, "name", copyOfFileName);
		MyFree(copyOfFileName);

		cJSON* dependencies = cJSON_AddArrayToObject(manifest, "dependencies");
		cJSON* b_object = cJSON_CreateObject();
		if (background && background[0]) {
			char *backg;
			cJSON_AddStringToObject(b_object, "name", "background");

			backg = MyStrdup(FindFilename(background));
#ifdef WINDOWS
			backg = Convert2UTF8(backg);
#endif
			cJSON_AddStringToObject(b_object, "filename", backg);
			MyFree(backg);
			backg = MyStrdup(background);
#ifdef WINDOWS
			backg = Convert2UTF8(backg);
			ConvertPathForward(backg);
#endif // WINDOWS			
			cJSON_AddStringToObject(b_object, "copy-path", backg);
			cJSON_AddStringToObject(b_object, "arch-path", dependencyDir);
			MyFree(backg);
			cJSON_AddNumberToObject(b_object, "size", GetLayoutBackGroundSize());
			cJSON_AddNumberToObject(b_object, "pos-x", GetLayoutBackGroundPos().x);
			cJSON_AddNumberToObject(b_object, "pos-y", GetLayoutBackGroundPos().y);
			cJSON_AddNumberToObject(b_object, "screen", GetLayoutBackGroundScreen());
			cJSON_AddNumberToObject(b_object, "angle", GetLayoutBackGroundAngle());
			cJSON_AddItemToArray(dependencies, b_object);
		}
	}
	char* json_Manifest = cJSON_Print(manifest);
	cJSON_Delete(manifest);
	return json_Manifest;
}

/**************************************************************************
 * Pull in a Manifest File and extract values from it
 * \param IN manifest - the full path to the mainifest.json file
 * \param IN zip_directory - the path to the directory for extracted objects
 *
 * \returns - the layout filename
 */

char* ParseManifest(char* manifest, char* zip_directory)
{
	char* background_file[1] = { NULL };
	char* layoutname;

	char *oldLocale = SaveLocale("C");
	cJSON* json_manifest = cJSON_Parse(manifest);
	RestoreLocale(oldLocale);

	cJSON* layout = cJSON_GetObjectItemCaseSensitive(json_manifest, "layout");
	cJSON* name = cJSON_GetObjectItemCaseSensitive(layout, "name");
	layoutname = cJSON_GetStringValue(name);
#ifdef WINDOWS
	ConvertUTF8ToSystem(layoutname);
#endif // WINDOWS

	LOG(log_zip, 1, ("Zip-Manifest %s \n", layoutname))
#if DEBUG
		fprintf(stderr, "Layout name %s \n", layoutname);
#endif

	cJSON* dependencies = cJSON_GetObjectItemCaseSensitive(json_manifest,
		"dependencies");
	cJSON* dependency;
	cJSON_ArrayForEach(dependency, dependencies) {
		cJSON* name = cJSON_GetObjectItemCaseSensitive(dependency, "name");
		if (strcmp(cJSON_GetStringValue(name), "background") == 0) {
			char *file;
			char *path;
			cJSON* filename = cJSON_GetObjectItemCaseSensitive(dependency, "filename");
			cJSON* archpath = cJSON_GetObjectItemCaseSensitive(dependency, "arch-path");
			file = MyStrdup(cJSON_GetStringValue(filename));
			path = MyStrdup(cJSON_GetStringValue(archpath));
#ifdef WINDOWS
			ConvertUTF8ToSystem(file);
			ConvertUTF8ToSystem(path);
#endif
			MakeFullpath(&background_file[0], zip_directory, path,
				file, NULL);
			MyFree(file);
			MyFree(path);
#if DEBUG
			printf("Link to background image %s \n", background_file[0]);
#endif
			LoadImageFile(1, &background_file[0], NULL);
			cJSON* size = cJSON_GetObjectItemCaseSensitive(dependency, "size");
			SetLayoutBackGroundSize(size->valuedouble);
			cJSON* posx = cJSON_GetObjectItemCaseSensitive(dependency, "pos-x");
			cJSON* posy = cJSON_GetObjectItemCaseSensitive(dependency, "pos-y");
			coOrd pos;
			pos.x = posx->valuedouble;
			pos.y = posy->valuedouble;
			SetLayoutBackGroundPos(pos);
			cJSON* screen = cJSON_GetObjectItemCaseSensitive(dependency, "screen");
			SetLayoutBackGroundScreen((int)screen->valuedouble);
			cJSON* angle = cJSON_GetObjectItemCaseSensitive(dependency, "angle");
			SetLayoutBackGroundAngle(angle->valuedouble);
			LayoutBackGroundSave();      //Force out Values	to override saved
		}
	}
	char *str = NULL;
	if (background_file[0]) {
		free(background_file[0]);
	}
	if (layoutname) {
		str = strdup(layoutname);
	}
	cJSON_Delete(json_manifest);
	return str;
}

