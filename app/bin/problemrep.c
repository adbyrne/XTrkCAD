/** \file problemrep.c
 * Collect data for a problem report and remove private info
*/

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2024 Martin Fischer
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef WINDOWS
#include <Windows.h>
#include <FreeImage.h>
#define CONFIG_DELIMITER '='
#else
#define strnicmp strncasecmp
#define stricmp	strcasecmp
#define CONFIG_DELIMITER ':'
#endif // WINDOWS

#include <wlib.h>

#include "include/problemrep.h"
#include "archive.h"
#include "directory.h"
#include "dynstring.h"
#include "fileio.h"
#include "layout.h"
#include "misc.h"
#include "include/paramfilelist.h"
#include "paths.h"
#include "version.h"

static void SaveSystemInfo(char* dir);
static void ZipProblemData(const char* src);
static void ProblemDataCollect();

static dynArr_t configFiles_da;
#define configFile(N) DYNARR_N(char *,configFiles_da,N)

/**
 * Create a temporary directory as intermediate space for collected files.
 * The returned pointer has to be MyFree()'d by the caller.
 *
 * \return pointer to directory name, the complete path
 */

static char *
CreateTempDirectory()
{
	char* dir;
#ifdef WINDOWS
	struct _stat fileStatus;
#else
	struct stat fileStatus;
#endif

	dir = wGetTempPath();

	if (!stat(dir, (struct stat *const) &fileStatus)) {
		if (fileStatus.st_mode & S_IFDIR) {
			return(dir);
		}
	} else {
		mkdir(dir, 0700);
	}

	return MyStrdup(dir);
}

/**
 * Save version information about the operating environment to a file.
 *
 * \param dir	destination directory
 */

void
SaveSystemInfo(char* dir)
{
	FILE* fh;
	char* fileName=NULL;

	MakeFullpath(&fileName, dir, "versions.txt", NULL);

	if (fileName) {
		fh = fopen(fileName, "w");

		fprintf(fh, "XTrackCAD: %s\n", VERSION);
		fprintf(fh, "OS: %s\n", wGetOSVersion());
#ifdef WINDOWS
		fprintf(fh, "FreeImage: %s\n", FreeImage_GetVersion());
#else
		// get gtk version
		fprintf(fh, "GTK Version: %s", wGetPlatformVersion() );
#endif // WINDOWS

		fclose(fh);
	}
	free(fileName);
}

/**
 * Replace the directory name in a configuration file line. Assumption is that
 * the name of the directory starts after the '=' (Windows) or ':' (UNIX)
 *
 * \para  result	pointer to DynString for result, DynString is cleared if
 *					directory was not found
 * \param in		line from configuration file
 * \param dir		directory name to look for
 * \param replace	replacement string
 * \return			true if found and replaced, false otherwise
 */

static bool
ReplaceDirectoryName(DynString *result, char* in, const char* dir,
                     char* replace)
{
	bool rc = false;

	DynStringClear(result);

#ifdef WINDOWS
	rc = strnicmp(in, dir, strlen(dir));
#else
	rc = strncmp(in, dir, strlen(dir));
#endif // WINDOWS

	if(!rc) {
		DynStringCatCStrs(result, replace, in + strlen(dir), NULL);
	}
	return(!rc);
}

/**
 * Find the user id in a string and replace it with a placeholer.
 *
 * \param result	resulting string DynFree() after use
 * \param in		string to search in
 * \param replace	replacement string
 * \return			true if found and replaced
 */

static bool
ReplaceUserID(DynString* result, char* in, char* replace)
{
	char* user = strstr(in, wGetUserID());
	bool rc = false;
	DynStringClear(result);

	if (user) {
		DynStringNCatCStr(result, user - in, in);
		DynStringCatCStrs(result, replace, user + strlen(user), NULL);
		rc = true;
	}

	return(rc);
}

/**
 * Replace directory names in a configuration value with the appropriate
 * symbolic name for that directory.
 * The name of the config value is ignored at the moment, but could be used
 * for refined logic
 *
 * \param result	pointer to DynString for modified value
 * \param name		name of configuration value
 * \param value		the value
 * \return
 */
static bool
FilterConfigLine(DynString *result, char* name, char *value)
{
	bool clean;

	DynStringClear(result);

	clean = ReplaceDirectoryName(result, value, wGetAppLibDir(),
	                             "<<applibdir>>");

	// NOTE: the order of these calls is important, possible subdirs of a
	// homedir must be checked first
	if (!clean) {
		clean = ReplaceDirectoryName(result, value, wGetAppWorkDir(),
		                             "<<workdir>>");
		if (!clean) {
			clean = ReplaceDirectoryName(result, value, wGetUserHomeRootDir(),
			                             "<<home>>");
		}
	}

	// replace any remaining references to the current userid
	if (!clean) {
		clean = ReplaceUserID(result, value, "<<user>>");
	}

	if (!clean) {
		DynStringCatCStr(result, value);
	}

	return(clean);
}

/**
 * Read a configuration file, replace private information and write result
 * to a file.
 *
 * \param	srcfile	configuration file to read
 * \param	destdir	destination directory
 * \return	true
 */

#define DELIMITER_COUNT (4)		// count of delimiter chars in config line

static bool
PickupConfigFile(char *srcfile, char* destdir)
{
	FILE* fhRead;
	FILE* fhWrite;
	DynString configLine;
	DynString destFile;

	DynStringMalloc(&destFile, FILENAME_MAX);
	DynStringMalloc(&configLine, FILENAME_MAX);

	DynStringCatCStrs(&destFile, destdir, FILE_SEP_CHAR,
	                  FindFilename(srcfile), NULL);

	ProblemrepUpdateW(_("Get settings file %s\n"), srcfile);

	fhRead = fopen(srcfile, "r");
	fhWrite = fopen(DynStringToCStr(&destFile), "w");

	if (fhRead && fhWrite) {
		char* lineptr = NULL;
		size_t linelen = 0;

		while (!feof(fhRead)) {
			char* section;
			char* name;
			char* value;
			size_t totalLength;

			if(getline(&lineptr, &linelen, fhRead)==-1) {
				continue;
			}

			wPrefTokenize(lineptr, &section, &name, &value);
			if (name && value) {
				FilterConfigLine(&configLine, name, value);

				// calculate maximum possible length of resulting line
				totalLength = (section ? strlen(section) : 0) +
				              (name ? strlen(name) : 0) +
				              (value ? strlen(value) : 0) + DELIMITER_COUNT;

				// increase buffer size by 256  byte if too small
				if (totalLength > linelen) {
					size_t newLen = ((totalLength + 256) & (~0xff));
					lineptr = realloc(lineptr, newLen);
					if (!lineptr) {
						AbortProg("!lineptr", __FILE__, __LINE__,
						          "Can't realloc memory");
					}
					linelen = newLen;
				}
				value = DynStringToCStr(&configLine);
			}
			wPrefFormatLine(section, name, value, lineptr);
			fprintf(fhWrite, "%s\n", lineptr);
		}
		free(lineptr);
		fclose(fhRead);
		fclose(fhWrite);
	}
	DynStringFree(&configLine);
	DynStringFree(&destFile);

	return(true);
}

/**
 * Replace the leading part of a directory name with a place holder.
 *
 * \param result	modified directory name
 * \param dir		original name
 * \param search	path to be replaced is present
 * \param replace	replacement
 * \return			true if found and replaced
 */

static bool
FilterDirectoryName(DynString* result, char* dir, const char* search,
                    char* replace)
{
	bool rc = false;
	if (!strnicmp(dir, search, strlen(search))) {
		DynStringReset(result);
		DynStringCatCStrs(result, replace, dir + strlen(search), NULL);
		rc = true;
	}

	return(rc);
}

/**
 * Filter path information from a NOTE subcommand. In case the home directory
 * is referenced it is changed using a place holder.
 * In case a NOTE references a file (sub format 2) the home directory
 * changed using a place holder
 *
 * \param out	open file handle
 * \param work	subfunction of NOTE without the leading command
 */

static void
FilterLayoutNote(FILE* out, char* work)
{
	DynString result;
	bool isDocument = false;
	char* token;

	DynStringMalloc(&result, FILENAME_MAX);

	fprintf(out, "NOTE ");

	for (int i = 0; i < 8; i++) {
		token = strtok(NULL, " \t");
		fprintf(out, "%s ", token);
	}

	if (token && !strcmp(token, "2")) {
		isDocument = true;
	}
	// filename is next
	token = strtok(NULL, " \t\"");

	char * filename = ConvertFromEscapedText(token);

	if (isDocument && FilterDirectoryName(&result, filename,
	                                      wGetUserHomeDir(), "<<home>>")) {
		MyFree(filename);
		filename = ConvertToEscapedText(DynStringToCStr(&result));
		fprintf(out, "\"%s\"", filename);
	} else {
		fprintf(out, "\"%s\"", token);
	}

	MyFree(filename);
	filename = NULL;

	token = strtok(NULL, "\n");
	fprintf(out, "%s\n", token);

	DynStringFree(&result);
}

/**
 * Filter path information from a SET subcommand. In case the home directory
 * is referenced it is changed using a place holder
 *
 * \param out	open file handle
 * \param work	subcommand of LAYERS without the leading command
 */

static void
FilterLayers(FILE *out, char* work)
{
	DynString result;

	DynStringMalloc(&result, FILENAME_MAX );

	char* token = strtok(NULL, " \t");
	if (token ) {
		if (!stricmp(token, "SET")) {
			char* filename;
			bool clean;

			fprintf(out, "%s ", token);
			token = strtok(NULL, " \t");
			fprintf(out, "%s ",token);
			token = strtok(NULL, "\"");
			filename = ConvertFromEscapedText(token);

			DYNARR_APPEND(char*, configFiles_da, 1);
			configFile(configFiles_da.cnt - 1) = MyStrdup(filename);

			clean = FilterDirectoryName(&result, filename, wGetUserHomeDir(),
			                            "<<home>>");
			if (clean) {
				MyFree(filename);
				filename = ConvertToEscapedText(DynStringToCStr(&result));
				fprintf(out, "\"%s\"\n", filename);
			} else {
				fprintf(out, "\"%s\"\n", token);
			}
			MyFree(filename);

		} else {
			const char* remainder = token + strlen(token) + 1;
			fprintf(out, "%s %s", token, remainder );
		}
	}
	DynStringFree(&result);
}

/**
 * Filter privacy information from a single line in xtc format. The result is
 * saved to a file
 *
 * \param out	open file handle
 * \param in	valid line from xtc file
 * \return		true
 */

static bool
FilterLayoutLine(FILE *out, const char* in)
{
	char* workCopy = MyStrdup(in);
	char* token;

	token = strtok(workCopy, " \t\n");
	if (token) {
		bool done = false;
		if (!stricmp(token, "LAYERS")) {
			// handle LAYERS
			fprintf(out, "%s ", token);
			FilterLayers(out, workCopy);
			done = true;
		}
		if (!stricmp(token, "NOTE")) {
			// handle NOTE document
			FilterLayoutNote(out, workCopy);
			done = true;
		}
		if (!done) {
			fputs(in, out);
		}
	}

	MyFree(workCopy);
	return(true);
}

/**
 * Get the currently open layout file, filter privacy information and place
 * it in an temporary directory.
 *
 * \param	dir	destination directory
 * \return	true
 */

static bool
PickupLayoutFile(char* dir)
{
	FILE* fhRead;
	FILE* fhWrite;
	DynString layoutLine;
	DynString destFile;

	DynStringMalloc(&destFile, FILENAME_MAX);
	DynStringMalloc(&layoutLine, STR_SIZE);

	DynStringCatCStrs(&destFile, dir, FILE_SEP_CHAR,
	                  GetLayoutFilename(), NULL);

	ProblemrepUpdateW(_("Add layout file %s\n"), GetLayoutFullPath());
	fhRead = fopen(GetLayoutFullPath(), "r");
	fhWrite = fopen(DynStringToCStr(&destFile), "w");

	if (fhRead && fhWrite) {
		char* lineptr = NULL;
		size_t linelen = 0;
		while (!feof(fhRead)) {
			getline(&lineptr, &linelen, fhRead);
			if (!feof(fhRead)) {
				FilterLayoutLine(fhWrite, lineptr);
			}
		}
		free(lineptr);
		fclose(fhRead);
		fclose(fhWrite);
	}
	DynStringFree(&layoutLine);
	DynStringFree(&destFile);
	return(true);
}

/**
 * Get the custom parameter file.
 *
 * \param	dest	temporary directory
 * \return	true on success
 */

static bool
PickupCustomFile(char* dest)
{
	char* inFile;
	char* outFile;
	bool rc;

	MakeFullpath(&inFile, wGetAppWorkDir(), "xtrkcad.cus", NULL);
	MakeFullpath(&outFile, dest, "xtrkcad.cus", NULL);
	ProblemrepUpdateW(_("Add custom parameter definitions\n"));

	rc = Copyfile(inFile, outFile);

	free(inFile);
	free(outFile);

	return(rc==0);
}

/**
 * Create a zip file from the collected information. The zip file is created
 * in the same directory as the layout design. A unique name is generated from
 * the current date and time.
 *
 * \param src	directory with collected information
 */

static void
ZipProblemData(const char* src)
{
	char* dest = MyStrdup(GetLayoutFullPath());
	char* out;
	char *filename = strrchr(dest, PATH_SEPARATOR[0]) + 1;
	struct tm* currentTime;
	time_t clock;
	char timestamp[80];

	time(&clock);
	currentTime = gmtime(&clock);
	strftime(timestamp, 80, "pd-%y%m%dT%H%M%S.zip", currentTime);

	*filename = '\0';

	MakeFullpath(&out, dest, timestamp, NULL);
	ProblemrepUpdateW(_("Create zip archive %s\n"), out);

	CreateArchive(src, out);
	free(out);
}

void
ProblemDataCollect()
{
	char* tempDirectory;
	char* subdirectory = NULL;
	bool ret;
	char *filename = GetLayoutFullPath();

	if(*filename == '\0') {
		ProblemrepUpdateW(_("No layout design loaded! Operation is cancelled.\n"));
		return;
	}

	if (!ProblemSaveLayout()) {
		return;
	}

	tempDirectory = CreateTempDirectory();
	SaveSystemInfo(tempDirectory);

	DYNARR_APPEND(char *, configFiles_da, 1);
	configFile(configFiles_da.cnt - 1) = MyStrdup(wGetProfileFilename());

	ret = PickupLayoutFile(tempDirectory);

	MakeFullpath(&subdirectory, tempDirectory, "workdir", NULL);
	mkdir(subdirectory, 0700);
	if (ret) {
		ret = PickupCustomFile(subdirectory);
	}

	for (int i = 0; i < configFiles_da.cnt; i++) {
		char *file = configFile(i);
		PickupConfigFile(file, subdirectory);
		MyFree(file);
	}
	free(subdirectory);
	subdirectory = NULL;

	MakeFullpath(&subdirectory, tempDirectory, "params", NULL);
	mkdir(subdirectory, 0700);

	for (int i = 0; i < GetParamFileCount(); i++) {
		char* file = GetParamFileName(i);

		if (strncmp(file, wGetAppLibDir(), strlen(wGetAppLibDir()))) {
			char* destfile;
			ProblemrepUpdateW(_("Get local parameter file %s\n"), file);

			MakeFullpath(&destfile, subdirectory, FindFilename(file), NULL);
			Copyfile(file, destfile);
			free(destfile);
		}
	}
	free(subdirectory);
	subdirectory = NULL;

	if (ret) {
		ZipProblemData(tempDirectory);

		DeleteDirectory(tempDirectory);
	}

	DYNARR_FREE(char*, configFiles_da);
}

void
DoProblemCollect(void* unused)
{
	ProblemrepCreateW(NULL);

	ProblemDataCollect();
}
