/** \file fileio.c
 * Loading and saving files. Handles trackplans, XTrackCAD exports and cut/paste
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#ifndef WINDOWS
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#endif
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#ifdef WINDOWS
#include <io.h>
#include <windows.h>
	//#if _MSC_VER >=1400
	//	#define strdup _strdup
	//#endif
#else
#endif
#include <sys/stat.h>
#include <stdarg.h>
#include <locale.h>

#include <stdint.h>

// POSIX dependencies
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <assert.h>

#include <zip.h>
#include <cJSON.h>

#include "common.h"
#include "compound.h"
#include "cselect.h"
#include "cundo.h"
#include "custom.h"
#include "draw.h"
#include "fileio.h"
#include "fcntl.h"
#include "i18n.h"
#include "layout.h"
#include "messages.h"
#include "misc.h"
#include "param.h"
#include "paths.h"
#include "track.h"
#include "utility.h"
#include "version.h"


/*#define TIME_READTRACKFILE*/

EXPORT const char * workingDir;
EXPORT const char * libDir;

static char * customPath = NULL;
static char * customPathBak = NULL;

EXPORT char * clipBoardN;

static int log_paramFile;

char zip_unpack_dir_name[] = "zip_in";
char zip_pack_dir_name[] = "zip_out";

#ifdef WINDOWS
#define rename( F1, F2 ) Copyfile( F1, F2 )

static int Copyfile( char * fn1, char * fn2 )
{
	FILE *f1, *f2;
	size_t size;
	f1 = fopen( fn1, "r" );
	if ( f1 == NULL )
		return 0;
	f2 = fopen( fn2, "w" );
	if ( f2 == NULL ) {
		fclose( f1 );
		return -1;
	}
	while ( (size=fread( message, 1, sizeof message, f1 )) > 0 )
		fwrite( message, size, 1, f2 );
	fclose( f1 );
	fclose( f2 );
	return 0;
}
#endif

/**
 * Save the old locale and set to new.
 *
 * \param newlocale IN the new locale to set
 * \return    pointer to the old locale
 */

char *
SaveLocale( char *newLocale )
{
	char *oldLocale;
	char *saveLocale = NULL;

	/* get old locale setting */
	oldLocale = setlocale(LC_ALL, NULL);

	/* allocate memory to save */
	if (oldLocale)
		saveLocale = strdup( oldLocale );

	setlocale(LC_ALL, newLocale );

	return( saveLocale );
}

/**
 * Restore a previously saved locale.
 *
 * \param locale IN return value from earlier call to SaveLocale
 */

void
RestoreLocale( char * locale )
{
	if( locale ) {
		setlocale( LC_ALL, locale );
		free( locale );
	}
}


/****************************************************************************
 *
 * PARAM FILE INPUT
 *
 */

EXPORT FILE * paramFile = NULL;
char *paramFileName;
EXPORT wIndex_t paramLineNum = 0;
EXPORT char paramLine[STR_LONG_SIZE];
EXPORT char * curContents;
EXPORT char * curSubContents;
static long paramCheckSum;

#define PARAM_DEMO (-1)

typedef struct {
		char * name;
		readParam_t proc;
		} paramProc_t;
static dynArr_t paramProc_da;
#define paramProc(N) DYNARR_N( paramProc_t, paramProc_da, N )


EXPORT void Stripcr( char * line )
{
	char * cp;
	cp = line + strlen(line);
	if (cp == line)
		return;
	cp--;
	if (*cp == '\n')
		*cp-- = '\0';
	if (cp >= line && *cp == '\r')
		*cp = '\0';
}

EXPORT void ParamCheckSumLine( char * line )
{
	long mult=1;
	while ( *line )
		paramCheckSum += (((long)(*line++))&0xFF)*(mult++);
}

EXPORT char * GetNextLine( void )
{
	if (!paramFile) {
		paramLine[0] = '\0';
		return NULL;
	}
	if (fgets( paramLine, sizeof paramLine, paramFile ) == NULL) {
		AbortProg( "Permature EOF on %s", paramFileName );
	}
	Stripcr( paramLine );
	ParamCheckSumLine( paramLine );
	paramLineNum++;
	return paramLine;
}


/**
 * Show an error message if problems occur during loading of a param or layout file.
 * The user has the choice to cancel the operation or to continue. If operation is
 * canceled the open file is closed.
 *
 * \param IN msg error message
 * \param IN showLine set to true if current line should be included in error message
 * \param IN ... variable number additional error information
 * \return TRUE to continue, FALSE to abort operation
 *
 */

EXPORT int InputError(
		char * msg,
		BOOL_T showLine,
		... )
{
	va_list ap;
	char * mp = message;
	int ret;

	mp += sprintf( message, "INPUT ERROR: %s:%d\n",
		paramFileName, paramLineNum );
	va_start( ap, showLine );
	mp += vsprintf( mp, msg, ap );
	va_end( ap );
	if (showLine) {
		*mp++ = '\n';
		strcpy( mp, paramLine );
	}
	strcat( mp, _("\nDo you want to continue?") );
	if (!(ret = wNoticeEx( NT_ERROR, message, _("Continue"), _("Stop") ))) {
		if ( paramFile )
			fclose(paramFile);
		paramFile = NULL;
	}
	return ret;
}


EXPORT void SyntaxError(
		char * event,
		wIndex_t actual,
		wIndex_t expected )
{
	InputError( "%s scan returned %d (expected %d)",
		TRUE, event, actual, expected );
}

/**
 * Parse a line in XTrackCAD's file format
 *
 * \param line IN line to parse
 * \param format IN ???
 *
 * \return FALSE in case of parsing error, TRUE on success
 */

EXPORT BOOL_T GetArgs(
		char * line,
		char * format,
		... )
{
	char * cp, * cq;
	int argNo;
	long * pl;
	unsigned long *pul;
	int * pi;
	FLOAT_T *pf;
	coOrd p, *pp;
	char * ps;
	char ** qp;
	va_list ap;
	char *oldLocale = NULL;

	oldLocale = SaveLocale("C");

	cp = line;
	va_start( ap, format );
	for (argNo=1;*format;argNo++,format++) {
		while (isspace((unsigned char)*cp)) cp++;
		if (!*cp && strchr( "XZYzc", *format ) == NULL ) {
			RestoreLocale(oldLocale);
			InputError( "Arg %d: EOL unexpected", TRUE, argNo );
			return FALSE;
		}
		switch (*format) {
		case '0':
			(void)strtol( cp, &cq, 10 );
			if (cp == cq) {
				RestoreLocale(oldLocale);
				InputError( "Arg %d: expected integer", TRUE, argNo );
				return FALSE;
			}
			cp = cq;
			break;
		case 'X':
			pi = va_arg( ap, int * );
			*pi = 0;
			break;
		case 'Z':
			pl = va_arg( ap, long * );
			*pl = 0;
			break;
		case 'Y':
			pf = va_arg( ap, FLOAT_T * );
			*pf = 0;
			break;
		case 'L':
			pi = va_arg( ap, int * );
			*pi = (int)strtol( cp, &cq, 10 );
			if (cp == cq) {
				RestoreLocale(oldLocale);
				InputError( "Arg %d: expected integer", TRUE, argNo );
				return FALSE;
			}
			cp = cq;
			break;
		case 'd':
			pi = va_arg( ap, int * );
			*pi = (int)strtol( cp, &cq, 10 );
			if (cp == cq) {
				RestoreLocale(oldLocale);
				InputError( "Arg %d: expected integer", TRUE, argNo );
				return FALSE;
			}
			cp = cq;
			break;
		case 'w':
			pf = va_arg( ap, FLOAT_T * );
			*pf = (FLOAT_T)strtol( cp, &cq, 10 );
			if (cp == cq) {
				RestoreLocale(oldLocale);
				InputError( "Arg %d: expected integer", TRUE, argNo );
				return FALSE;
			}
			if (*cq == '.')
				*pf = strtod( cp, &cq );
			else
				*pf /= mainD.dpi;
			cp = cq;
			break;
		case 'u':
			pul = va_arg( ap, unsigned long * );
			*pul = strtoul( cp, &cq, 10 );
			if (cp == cq) {
				RestoreLocale(oldLocale);
				InputError( "Arg %d: expected integer", TRUE, argNo );
				return FALSE;
			}
			cp = cq;
			break;
		case 'l':
			pl = va_arg( ap, long * );
			*pl = strtol( cp, &cq, 10 );
			if (cp == cq) {
				RestoreLocale(oldLocale);
				InputError( "Arg %d: expected integer", TRUE, argNo );
				return FALSE;
			}
			cp = cq;
			break;
		case 'f':
			pf = va_arg( ap, FLOAT_T * );
			*pf = strtod( cp, &cq );
			if (cp == cq) {
				RestoreLocale(oldLocale);
				InputError( "Arg %d: expected float", TRUE, argNo );
				return FALSE;
			}
			cp = cq;
			break;
		case 'z':
			pf = va_arg( ap, FLOAT_T * );
#ifdef LATER
			if ( paramVersion >= 9 ) {
				*pf = strtod( cp, &cq );
				if (cp == cq) {
					RestoreLocale(oldLocale);
					InputError( "Arg %d: expected float", TRUE, argNo );
					return FALSE;
				}
				cp = cq;
			} else {
				*pf = 0.0;
			}
#endif
			*pf = 0.0;
			break;
		case 'p':
			pp = va_arg( ap, coOrd * );
			p.x = strtod( cp, &cq );
			if (cp == cq) {
				RestoreLocale(oldLocale);
				InputError( "Arg %d: expected float", TRUE, argNo );
				return FALSE;
			}
			cp = cq;
			p.y = strtod( cp, &cq );
			if (cp == cq) {
				RestoreLocale(oldLocale);
				InputError( "Arg %d: expected float", TRUE, argNo );
				return FALSE;
			}
			cp = cq;
			*pp = p;
			break;
		case 's':
			ps = va_arg( ap, char * );
			while (isspace((unsigned char)*cp)) cp++;
			while (*cp && !isspace((unsigned char)*cp)) *ps++ = *cp++;
			*ps++ = '\0';
			break;
		case 'q':
			qp = va_arg( ap, char * * );
			if (*cp != '\"')
				/* Stupid windows */
				cq = strchr( cp, '\"' );
			else
				cq = cp;
			if (cq!=NULL) {
				cp = cq;
				ps = &message[0];
				cp++;
				while (*cp) {
					if ( (ps-message)>=sizeof message)
						AbortProg( "Quoted title argument too long" );
					if (*cp == '\"') {
						if (*++cp == '\"') {
							*ps++ = '\"';
						} else {
							*ps = '\0';
							cp++;
							break;
						}
					} else {
						*ps++ = *cp;
					}
					cp++;
				}
				*ps = '\0';
			} else {
				message[0] = '\0';
			}
			*qp = (char*)MyStrdup(message);
			break;
		case 'c':
			qp = va_arg( ap, char * * );
			while (isspace((unsigned char)*cp)) cp++;
			if (*cp)
				*qp = cp;
			else
				*qp = NULL;
			break;
		default:
			AbortProg( "getArgs: bad format char" );
		}
	}
	va_end( ap );
	RestoreLocale(oldLocale);
	return TRUE;
}

EXPORT wBool_t ParseRoomSize(
		char * s,
		coOrd * roomSizeRet )
{
	coOrd size;
	char *cp;

	size.x = strtod( s, &cp );
	if (cp != s) {
		s = cp;
		while (isspace((unsigned char)*s)) s++;
		if (*s == 'x' || *s == 'X') {
			size.y = strtod( ++s, &cp );
			if (cp != s) {
#ifdef LATER
				if (units == UNITS_METRIC) {
					size.x /= 2.54;
					size.y /= 2.54;
				}
#endif
				*roomSizeRet = size;
				return TRUE;
			}
		}
	}
	return FALSE;
}


EXPORT void AddParam(
		char * name,
		readParam_t proc )
{
	DYNARR_APPEND( paramProc_t, paramProc_da, 10 );
	paramProc(paramProc_da.cnt-1).name = name;
	paramProc(paramProc_da.cnt-1).proc = proc;
}


EXPORT BOOL_T ReadParams(
		long key,
		const char * dirName,
		const char * fileName )
{
	FILE * oldFile;
	char *cp;
	wIndex_t oldLineNum;
	wIndex_t pc;
	long oldCheckSum;
	long checkSum=0;
	BOOL_T checkSummed;
	long paramVersion = -1;
	char *oldLocale = NULL;

	if (dirName) {
		MakeFullpath(&paramFileName, dirName, fileName, NULL);
	} else {
		MakeFullpath(&paramFileName, fileName, NULL);
	}
	paramLineNum = 0;
	curBarScale = -1;
	curContents = strdup( fileName );
	curSubContents = curContents;

LOG1( log_paramFile, ("ReadParam( %s )\n", fileName ) )

	oldLocale = SaveLocale("C");

	paramFile = fopen( paramFileName, "r" );
	if (paramFile == NULL) {
		/* Reset the locale settings */
		RestoreLocale( oldLocale );

		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Parameter"), paramFileName, strerror(errno) );

		return FALSE;
	}
	paramCheckSum = key;
	paramLineNum = 0;
	checkSummed = FALSE;
	while ( paramFile && ( fgets(paramLine, 256, paramFile) ) != NULL ) {
		paramLineNum++;
		Stripcr( paramLine );
		if (strncmp( paramLine, "CHECKSUM ", 9 ) == 0) {
			checkSum = atol( paramLine+9 );
			checkSummed = TRUE;
			goto nextLine;
		}
		ParamCheckSumLine( paramLine );
		if (paramLine[0] == '#') {
			/* comment */
		} else if (paramLine[0] == 0) {
			/* empty paramLine */
		} else if (strncmp( paramLine, "INCLUDE ", 8 ) == 0) {
			cp = &paramLine[8];
			while (*cp && isspace((unsigned char)*cp)) cp++;
			if (!*cp) {
				InputError( "INCLUDE - no file name", TRUE );

				/* Close file and reset the locale settings */
				if (paramFile) fclose(paramFile);
				RestoreLocale( oldLocale );

				return FALSE;
			}
			oldFile = paramFile;
			oldLineNum = paramLineNum;
			oldCheckSum = paramCheckSum;
			ReadParams( key, dirName, cp );
			paramFile = oldFile;
			paramLineNum = oldLineNum;
			paramCheckSum = oldCheckSum;
			if (dirName) {
				MakeFullpath(&paramFileName, dirName, fileName, NULL);
			} else {
				MakeFullpath(&paramFileName, fileName);
			}
		} else if (strncmp( paramLine, "CONTENTS ", 9) == 0 ) {
			curContents = MyStrdup( paramLine+9 );
			curSubContents = curContents;
		} else if (strncmp( paramLine, "SUBCONTENTS ", 12) == 0 ) {
			curSubContents = MyStrdup( paramLine+12 );
		} else if (strncmp( paramLine, "PARAM ", 6) == 0 ) {
			paramVersion = atol( paramLine+6 );
		} else {
			for (pc = 0; pc < paramProc_da.cnt; pc++ ) {
				if (strncmp( paramLine, paramProc(pc).name,
							 strlen(paramProc(pc).name)) == 0 ) {
					paramProc(pc).proc( paramLine );
					goto nextLine;
				}
			}
			InputError( "Unknown param line", TRUE );
		}
 nextLine:;
	}
	if ( key ) {
		if ( !checkSummed || checkSum != paramCheckSum ) {
			/* Close file and reset the locale settings */
			if (paramFile) fclose(paramFile);
			RestoreLocale( oldLocale );

			NoticeMessage( MSG_PROG_CORRUPTED, _("Ok"), NULL, paramFileName );

			return FALSE;
		}
	}
	if (paramFile)fclose( paramFile );
	free(paramFileName);
	paramFileName = NULL;
	RestoreLocale( oldLocale );

	return TRUE;
}


static void ReadCustom( void )
{
	FILE * f;
	MakeFullpath(&customPath, workingDir, sCustomF, NULL);
	customPathBak = MyStrdup( customPath );
	customPathBak[ strlen(customPathBak)-1 ] = '1';
	f = fopen( customPath, "r" );
	if ( f != NULL ) {
		fclose( f );
		curParamFileIndex = PARAM_CUSTOM;
		ReadParams( 0, workingDir, sCustomF );
	}
}


/*
 * Open the file and then set the locale to "C". Old locale will be copied to
 * oldLocale. After the required file I/O is done, the caller must call
 * CloseCustom() with the same locale value that was returned in oldLocale by
 * this function.
 */
EXPORT FILE * OpenCustom( char *mode )
{
	FILE * ret = NULL;

	if (inPlayback)
		return NULL;
	if ( *mode == 'w' )
		rename( customPath, customPathBak );
	if (customPath) {
		ret = fopen( customPath, mode );
		if (ret == NULL) {
			NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Custom"), customPath, strerror(errno) );
		}
	}

	return ret;
}


EXPORT char * PutTitle( char * cp )
{
	static char title[STR_SIZE];
	char * tp = title;
	while (*cp && (tp-title)<=(sizeof title)-3) {
		if (*cp == '\"') {
			*tp++ = '\"';
			*tp++ = '\"';
		} else {
			*tp++ = *cp;
		}
		cp++;
	}
	if ( *cp )
		NoticeMessage( _("putTitle: title too long: %s"), _("Ok"), NULL, title );
	*tp = '\0';
	return title;
}

/**
 * Set the title of the main window. After loading a file or changing a design
 * this function is called to set the filename and the changed mark in the title
 * bar.
 */

void SetWindowTitle( void )
{
	char *filename;

	if ( changed > 2 || inPlayback )
		return;

	filename = GetLayoutFilename();
	sprintf( message, "%s%s - %s(%s)",
		(filename && filename[0])?filename: _("Unnamed Trackplan"),
		changed>0?"*":"", sProdName, sVersion );
	wWinSetTitle( mainW, message );
}

/*****************************************************************************
 * Directory Management
 */

static BOOL_T safe_create_dir(const char *dir)
{
	if (mkdir(dir, 0755) < 0) {
		if (errno != EEXIST) {
			perror(dir);
			return FALSE;
		}
	}
	return TRUE;
}

static BOOL_T delete_directory(const char *dir_path)
{
	size_t path_len;
	char *full_path = NULL;
	DIR *dir;
	struct stat stat_path, stat_entry;
	struct dirent *entry;

	// stat for the path
	int resp = stat(dir_path, &stat_path);

	if (resp == ENOENT) return TRUE; //Does not Exist

	// if path is not dir - exit
	if (S_ISDIR(stat_path.st_mode) == 0) {
		fprintf(stderr, "%s: %s \n", "Is not directory", dir_path);
		return FALSE;
	}

	// if not possible to read the directory for this user
	if ((dir = opendir(dir_path)) == NULL) {
		fprintf(stderr, "%s: %s \n", "Can`t open directory", dir_path);
		return FALSE;
	}

	// the length of the path
	path_len = strlen(dir_path);

	// iteration through entries in the directory
	while ((entry = readdir(dir)) != NULL) {

		// skip entries "." and ".."
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
			continue;

		// determinate a full path of an entry
		full_path = calloc(path_len + strlen(entry->d_name) + 1, sizeof(char));
		strcpy(full_path, dir_path);
		strcat(full_path, "/");
		strcat(full_path, entry->d_name);

		// stat for the entry
		stat(full_path, &stat_entry);

		// recursively remove a nested directory
		if (S_ISDIR(stat_entry.st_mode) != 0) {
			delete_directory(full_path);
			continue;
		}

		// remove a file object
		if (unlink(full_path) == 0)
			printf("Removed a file: %s \n", full_path);
		else {
			printf("Can`t remove a file: %s \n", full_path);
			closedir(dir);
			free(full_path);
			return FALSE;
		}
	}

	// remove the devastated directory and close the object of it
	if (rmdir(dir_path) == 0)
		printf("Removed a directory: %s \n", dir_path);
	else {
		printf("Can`t remove a directory: %s \n", dir_path);
		closedir(dir);
		if (full_path) free(full_path);
		return FALSE;
	}

	closedir(dir);
	if (full_path) free(full_path);
	return TRUE;
}

/*****************************************************************************
 * Archive processing
 */

static BOOL_T add_directory_to_archive (
				struct zip * za,
				const char * dir_path,
				const char * prefix) {

	char *full_path;
	char *arch_path;
	DIR *dir;
	const char * buf;
	struct stat stat_path, stat_entry;
	struct dirent *entry;

	zip_source_t * zt;

	// stat for the path
	stat(dir_path, &stat_path);

	// if path does not exists or is not dir - exit with status -1
	if (S_ISDIR(stat_path.st_mode) == 0) {
		fprintf(stderr, "xtrkcad: Is not a directory %s \n",  dir_path);
		return FALSE;
	}

	// if not possible to read the directory for this user
	if ((dir = opendir(dir_path)) == NULL) {
		fprintf(stderr, "xtrkcad: Can`t open directory %s \n", dir_path);
		return FALSE;
	}

	// iteration through entries in the directory
	while ((entry = readdir(dir)) != NULL) {
		// skip entries "." and ".."
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
			continue;

		// determinate a full path of an entry
		MakeFullpath(&full_path, dir_path, entry->d_name, NULL);

		// stat for the entry
		stat(full_path, &stat_entry);

		if (prefix[0])
			MakeFullpath(&arch_path, prefix, entry->d_name, NULL);
		else
			MakeFullpath(&arch_path, entry->d_name, NULL);

		// recursively add a nested directory
		if (S_ISDIR(stat_entry.st_mode) != 0) {
			if (zip_dir_add(za,arch_path,0) < 0) {
				zip_error_t  *ziperr = zip_get_error(za);
				buf = zip_error_strerror(ziperr);
				fprintf(stderr, "xtrkcad: Can't write directory %s - %s \n", arch_path, buf);
			}
			if (add_directory_to_archive(za,full_path,arch_path) != TRUE) {
				free(full_path);
				free(arch_path);
				return FALSE;
			}
			free(arch_path);
			continue;
		}
		zt = zip_source_file(za, full_path, 0, -1);
		if (zip_file_add(za,arch_path,zt,0)==-1) {
			zip_error_t  *ziperr = zip_get_error(za);
			buf = zip_error_strerror(ziperr);
			fprintf(stderr, "xtrkcad: Can't write file %s into %s - %s \n", full_path, arch_path, buf);
			free(full_path);
			free(arch_path);
			return FALSE;
		}
		free(arch_path);
		free(full_path);
	}

	closedir(dir);
	return TRUE;
}

static BOOL_T create_archive (
				const char * dir_path,
				const char * fileName) {

	struct zip *za;
	int err;
	char buf[100];
	char a[STR_LONG_SIZE];

	unlink(fileName); 						//Delete Old


	char * archive = strdup(fileName);  	// Because of const char

	char * archive_name = FindFilename(archive);

	char * archive_path;

	MakeFullpath(&archive_path,workingDir, archive_name, NULL);

	//free(archive);

	if ((za = zip_open(archive_path, ZIP_CREATE, &err)) == NULL) {
			zip_error_to_str(buf, sizeof(buf), err, errno);
			fprintf(stderr, "xtrkcad: can't create zip archive %s %s \n",
				archive_path, buf);
			free(archive_path);
			return FALSE;
	}

	add_directory_to_archive(za,dir_path,"");

	if (zip_close(za) == -1) {
		    zip_error_to_str(buf, sizeof(buf), err, errno);
			fprintf(stderr, "xtrkcad: can't close zip archive %s - %s\n", archive_path, buf);
			free(archive_path);
			return FALSE;
	}

	unlink(fileName); 							//Delete Old
	if (rename(archive_path,fileName) == -1) {	//Move zip into place
		fprintf(stderr, "xtrkcad: can't move zip archive into place %s - %s \n",
						fileName, strerror(errno));
		free(archive_path);
		return FALSE;
	}
	free(archive_path);
	return TRUE;

}

static BOOL_T unpack_archive_for(
				const char * pathName,  /*Full name of archive*/
				const char * fileName,  /*Just the name and extension */
				const char * tempDir,   /*Directory to unpack into */
				BOOL_T file_only) {
	char *dirName;
	char a[STR_LONG_SIZE];
	struct zip *za;
	struct zip_file *zf;
	struct zip_stat sb;
	char buf[100];
	int err;
	int i, len;
	int fd;
	long long sum;


	if ((za = zip_open(pathName, 0, &err)) == NULL) {
		zip_error_to_str(buf, sizeof(buf), err, errno);
		fprintf(stderr, "xtrkcad: can't open xtrkcad zip archive `%s': %s \n",
			pathName, buf);
		return FALSE;
	}

	for (i = 0; i < zip_get_num_entries(za, 0); i++) {
		if (zip_stat_index(za, i, 0, &sb) == 0) {
			printf("==================\n");
			len = strlen(sb.name);
			printf("Name: [%s], ", sb.name);
			printf("Size: [%llu], ", sb.size);
			printf("mtime: [%u]\n", (unsigned int)sb.mtime);
			if (sb.name[len - 1] == '/' && !file_only) {
				MakeFullpath(&dirName, tempDir, &sb.name[0], NULL);
				if (safe_create_dir(dirName)!=TRUE) {
					return FALSE;
				}
			} else {
				zf = zip_fopen_index(za, i, 0);
				if (!zf) {
					fprintf(stderr, "xtrkcad zip archive open index error \n");
					return FALSE;
				}

				if (file_only) {
					if (strncmp(sb.name, fileName, strlen(fileName))!=0) {
						continue; /* Ignore any other files than the one we asked for */
					}
				}
				MakeFullpath(&dirName, tempDir, &sb.name[0], NULL);
				fd = open(dirName, O_RDWR | O_TRUNC | O_CREAT , 0644);
				if (fd < 0) {
					fprintf(stderr, "xtrkcad zip archive file open failed %s %s \n", dirName, &sb.name[0]);
					return FALSE;
				}

				sum = 0;
				while (sum != sb.size) {
					len = zip_fread(zf, buf, 100);
					if (len < 0) {
						fprintf(stderr, "xtrkcad zip archive read failed \n");
						return FALSE;
					}
					write(fd, buf, len);
					sum += len;
				}
				close(fd);
				zip_fclose(zf);
			}
		} else {
			printf("File[%s] Line[%d]\n", __FILE__, __LINE__);
		}
	}

	if (zip_close(za) == -1) {
		fprintf(stderr, "xtrkcad: zip archive can't close archive `%s \n", pathName);
		return FALSE;
	}
	return TRUE;
}


/*****************************************************************************
 *
 * LOAD / SAVE TRACKS
 *
 */

static struct wFilSel_t * loadFile_fs;
static struct wFilSel_t * saveFile_fs;

static wWin_p checkPointingW;
static paramData_t checkPointingPLs[] = {
   {    PD_MESSAGE, N_("Check Pointing") } };
static paramGroup_t checkPointingPG = { "checkpoint", 0, checkPointingPLs, sizeof checkPointingPLs/sizeof checkPointingPLs[0] };

static char * checkPtFileName1;
static char * checkPtFileName2;

/** Read the layout design.
 *
 * \param IN pathName filename including directory
 * \param IN fileName pointer to filename part in pathName
 * \param IN full
 * \param IN noSetCurDir if FALSE current diurectory is changed to file location
 * \param IN complain  if FALSE error messages are supressed
 *
 * \return FALSE in case of load error
 */

static BOOL_T ReadTrackFile(
		const char * pathName,
		const char * fileName,
		BOOL_T full,
		BOOL_T noSetCurDir,
		BOOL_T complain )
{
	int count;
	coOrd roomSize;
	long scale;
	char * cp;
	char *oldLocale = NULL;
	int ret = TRUE;

	oldLocale = SaveLocale( "C" );

	paramFile = fopen( pathName, "r" );
	if (paramFile == NULL) {
		/* Reset the locale settings */
		RestoreLocale( oldLocale );

		if ( complain )
			NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, sProdName, pathName, strerror(errno) );

		return FALSE;
	}

	paramLineNum = 0;
	paramFileName = strdup( fileName );

	InfoMessage("0");
	count = 0;
	while ( paramFile && ( fgets(paramLine, sizeof paramLine, paramFile) ) != NULL ) {
		count++;
		if (count%10 == 0) {
			InfoMessage( "%d", count );
			wFlush();
		}
		paramLineNum++;
		if (strlen(paramLine) == (sizeof paramLine) -1 &&
			paramLine[(sizeof paramLine)-1] != '\n') {
			if( !(ret = InputError( "Line too long", TRUE )))
				break;
		}
		Stripcr( paramLine );
		if (paramLine[0] == '#' ||
			paramLine[0] == '\n' ||
			paramLine[0] == '\0' ) {
			/* comment */
			continue;
		}

		if (ReadTrack( paramLine )) {

		} else if (strncmp( paramLine, "END", 3 ) == 0) {
			break;
		} else if (strncmp( paramLine, "VERSION ", 8 ) == 0) {
			paramVersion = strtol( paramLine+8, &cp, 10 );
			if (cp)
				while (*cp && isspace((unsigned char)*cp)) cp++;
			if ( paramVersion > iParamVersion ) {
				if (cp && *cp) {
					NoticeMessage( MSG_UPGRADE_VERSION1, _("Ok"), NULL, paramVersion, iParamVersion, sProdName, cp );
				} else {
					NoticeMessage( MSG_UPGRADE_VERSION2, _("Ok"), NULL, paramVersion, iParamVersion, sProdName );
				}
				break;
			}
			if ( paramVersion < iMinParamVersion ) {
				NoticeMessage( MSG_BAD_FILE_VERSION, _("Ok"), NULL, paramVersion, iMinParamVersion, sProdName );
				break;
			}
		} else if (!full) {
			if( !(ret = InputError( "unknown command", TRUE )))
				break;
		} else if (strncmp( paramLine, "TITLE1 ", 7 ) == 0) {
			SetLayoutTitle(paramLine + 7);
		} else if (strncmp( paramLine, "TITLE2 ", 7 ) == 0) {
			SetLayoutSubtitle(paramLine + 7);
		} else if (strncmp( paramLine, "ROOMSIZE", 8 ) == 0) {
			if ( ParseRoomSize( paramLine+8, &roomSize ) ) {
				SetRoomSize( roomSize );
				/*wFloatSetValue( roomSizeXPD.control, PutDim(roomSize.x) );*/
				/*wFloatSetValue( roomSizeYPD.control, PutDim(roomSize.y) );*/
			} else {
				if( !(ret = InputError( "ROOMSIZE: bad value", TRUE )))
					break;
			}
		} else if (strncmp( paramLine, "SCALE ", 6 ) == 0) {
			if ( !DoSetScale( paramLine+5 ) ) {
				if( !(ret = InputError( "SCALE: bad value", TRUE )))
					break;
			}
		} else if (strncmp( paramLine, "MAPSCALE ", 9 ) == 0) {
			scale = atol( paramLine+9 );
			if (scale > 1) {
				mapD.scale = mapScale = scale;
			}
		} else if (strncmp( paramLine, "LAYERS ", 7 ) == 0) {
			ReadLayers( paramLine+7 );
		} else {
			if( !(ret = InputError( "unknown command", TRUE )))
				break;
		}
	}

	if (paramFile)
		fclose(paramFile);

	if( ret ) {
		if (!noSetCurDir)
			SetCurrentPath( LAYOUTPATHKEY, fileName );

		if (full) {
//			SetCurrentPath(LAYOUTPATHKEY, pathName);
			SetLayoutFullPath(pathName);
			//strcpy(curPathName, pathName);
			//curFileName = &curPathName[fileName-pathName];
			SetWindowTitle();
		}
	}

	RestoreLocale( oldLocale );

	paramFile = NULL;

	free(paramFileName);
	paramFileName = NULL;
	InfoMessage( "%d", count );
	return ret;
}

/***************************************************
 * JSON routines
 */

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
 *\param nameOfLayout - the layout this is a manifest for
 *\param background - the full filepath to the background image (or NULL) -> TODO this will become an array with a count
 *\param DependencyDir - the relative path in the archive to the directory in which the included object(s) will be stored
 *
 *\returns a String containing the JSON object
 */

char* CreateManifest(char* nameOfLayout, char* background,
						char* DependencyDir) {
	cJSON* manifest = cJSON_CreateObject();
	if (manifest != NULL) {
		cJSON* a_object = cJSON_CreateObject();
		cJSON_AddItemToObject(manifest, "layout", a_object);
		cJSON_AddStringToObject(a_object, "name", nameOfLayout);
		cJSON* dependencies = cJSON_AddArrayToObject(manifest, "dependencies");
		cJSON* b_object = cJSON_CreateObject();
		if (background && background[0]) {
			cJSON_AddStringToObject(b_object, "name", "background");
			cJSON_AddStringToObject(b_object, "copy-path", background);
			cJSON_AddStringToObject(b_object, "filename", FindFilename(background));
			cJSON_AddStringToObject(b_object, "arch-path", DependencyDir);
			cJSON_AddNumberToObject(b_object, "size", GetLayoutBackGroundSize());
			cJSON_AddNumberToObject(b_object, "pos-x", GetLayoutBackGroundPos().x);
			cJSON_AddNumberToObject(b_object, "pos-y", GetLayoutBackGroundPos().y);
			cJSON_AddNumberToObject(b_object, "screen", GetLayoutBackGroundScreen());
			cJSON_AddNumberToObject(b_object, "angle", GetLayoutBackGroundAngle());
			cJSON_AddItemToArray(dependencies, b_object);
		}
	}
	free(background);
	char* json_Manifest = cJSON_Print(manifest);
	cJSON_Delete(manifest);
	return json_Manifest;
}

/**************************************************************************
 * Pull in a Manifest File and extract values from it
 * \param mamifest - the full path to the mainifest.json file
 * \param zip_directory - the path to the directory to place extracted objects
 *
 * \returns - the layout filename
 */

EXPORT char* ParseManifest(char* manifest, char* zip_directory) {
	char* background_file[1] = {NULL};
	char* layoutname = NULL;
	cJSON* json_manifest = cJSON_Parse(manifest);
	cJSON* layout = cJSON_GetObjectItemCaseSensitive(json_manifest, "layout");
	cJSON* name = cJSON_GetObjectItemCaseSensitive(layout, "name");
	layoutname = cJSON_GetStringValue(name);
	fprintf(stderr, "Layout name %s \n",layoutname);

	cJSON* dependencies = cJSON_GetObjectItemCaseSensitive(json_manifest,
			"dependencies");
	cJSON* dependency;
	cJSON_ArrayForEach(dependency, dependencies) {
		cJSON* name = cJSON_GetObjectItemCaseSensitive(dependency, "name");
		if (strcmp(cJSON_GetStringValue(name), "background") == 0) {
			cJSON* filename = cJSON_GetObjectItemCaseSensitive(dependency, "filename");
			cJSON* archpath = cJSON_GetObjectItemCaseSensitive(dependency, "arch-path");
			MakeFullpath(&background_file[0], zip_directory, cJSON_GetStringValue(archpath), cJSON_GetStringValue(filename), NULL);
			fprintf(stderr, "Link to background image %s \n", background_file[0]);
			LoadImageFile(1,&background_file[0], NULL);
			cJSON* size = cJSON_GetObjectItemCaseSensitive(dependency, "size");
			SetLayoutBackGroundSize(size->valuedouble);
			cJSON* posx = cJSON_GetObjectItemCaseSensitive(dependency, "pos-x");
			cJSON* posy = cJSON_GetObjectItemCaseSensitive(dependency, "pos-y");
			coOrd pos;
			pos.x = posx->valuedouble;
			pos.y = posy->valuedouble;
			SetLayoutBackGroundPos(pos);
			cJSON* screen = cJSON_GetObjectItemCaseSensitive(dependency, "screen");
			SetLayoutBackGroundScreen(screen->valuedouble);
			cJSON* angle = cJSON_GetObjectItemCaseSensitive(dependency, "angle");
			SetLayoutBackGroundAngle(angle->valuedouble);
		}
	}
	cJSON_Delete(json_manifest);
	if (background_file[0]) free(background_file[0]);
	return layoutname;
}

int LoadTracks(
		int cnt,
		char **fileName,
		void * data)
{
#ifdef TIME_READTRACKFILE
	long time0, time1;
#endif
	char *nameOfFile = NULL;

	char *extOfFile;

	assert( fileName != NULL );
	assert( cnt == 1 ); 

	SetCurrentPath(LAYOUTPATHKEY, fileName[0]);
	paramVersion = -1;
	wSetCursor( mainD.d, wCursorWait );
	Reset();
	ClearTracks();
	ResetLayers();
	checkPtMark = changed = 0;
	LayoutBackGroundInit();
	UndoSuspend();
	useCurrentLayer = FALSE;
#ifdef TIME_READTRACKFILE
	time0 = wGetTimer();
#endif
	nameOfFile = FindFilename( fileName[ 0 ] );

 /*
  * Support zipped filetype .zxtc
  */
	extOfFile = FindFileExtension( nameOfFile);

	BOOL_T zipped = FALSE;

	char * full_path = fileName[0];

	if (extOfFile && (strcmp(extOfFile,"zxtc")==0)) {

		char * zip_input;

		MakeFullpath(&zip_input, workingDir, zip_unpack_dir_name, NULL);

		//If .zxtc unpack file into temporary input dir (cleared and re-created)

		delete_directory(zip_input);
		safe_create_dir(zip_input);


		if (unpack_archive_for(fileName[0], nameOfFile, zip_input, FALSE)!=TRUE) {
			NoticeMessage( MSG_UNPACK_FAIL, _("Continue"), NULL, fileName[0], nameOfFile, zip_input);
		} else {

			char * manifest_file;

			MakeFullpath(&manifest_file, zip_input, "manifest.json", NULL);

			char * manifest = 0;
			long length;
			FILE * f = fopen (manifest_file, "rb");

			if (f)
			{
			  fseek (f, 0, SEEK_END);
			  length = ftell (f);
			  fseek (f, 0, SEEK_SET);
			  manifest = malloc (length);
			  if (manifest)
			  {
				fread (manifest, 1, length, f);
			  }
			  fclose (f);
			} else {
				fprintf(stderr, "Can't open Manifest %s \n",manifest_file);
			}

			char * arch_file = NULL;


			//Set filename to point to included .xtc file
			//Use the name inside manifest (this helps if a user renames the zip)
			if (manifest)
			{
				arch_file = ParseManifest(manifest, zip_input);
			}

			// If no manifest value use same name as the archive
			if (arch_file && arch_file[0])
				MakeFullpath(&full_path, zip_input, nameOfFile, NULL);
			else {
				MakeFullpath(&full_path, zip_input, arch_file, NULL);

			}
			nameOfFile = FindFilename(full_path);
			extOfFile = FindFileExtension(nameOfFile);
			for (int i=0;i<4;i++) {   //remove z
				extOfFile[i] = extOfFile[i+1];
			}
		}

		zipped = TRUE;

		free(zip_input);

	}

	if (ReadTrackFile( full_path, FindFilename( fileName[0]), TRUE, FALSE, TRUE )) {

		if (zipped) {  //Put back to .zxtc extension - change back title and path
			nameOfFile = FindFilename( fileName[0]);
			extOfFile = FindFileExtension( fileName[0]);
			SetCurrentPath( LAYOUTPATHKEY, fileName[0] );
			SetLayoutFullPath(fileName[0]);
			SetWindowTitle();
		}

		wMenuListAdd( fileList_ml, 0, nameOfFile, MyStrdup(fileName[0]) );
		ResolveIndex();
#ifdef TIME_READTRACKFILE
		time1 = wGetTimer();
		printf( "time= %ld ms \n", time1-time0 );
#endif
		RecomputeElevations();
		AttachTrains();
		DoChangeNotification( CHANGE_ALL );
		DoUpdateTitles();
		LoadLayerLists();
		LayerSetCounts();
	}
	UndoResume();
	Reset();
	wSetCursor( mainD.d, defaultCursor );
	return TRUE;
}

/**
 * Load the layout specified by data. Filename may contain a full
 * path.
 * \param index IN ignored
 * \param label IN ignored
 * \param data IN path and filename 
 */

EXPORT void DoFileList(
		int index,
		char * label,
		void * data )
{
	char *pathName = (char*)data;

	LoadTracks( 1, &pathName, NULL );
}


static BOOL_T DoSaveTracks(
		const char * fileName )
{
	FILE * f;
	time_t clock;
	BOOL_T rc = TRUE;
	char *oldLocale = NULL;

	oldLocale = SaveLocale( "C" );

	f = fopen( fileName, "w" );
	if (f==NULL) {
		RestoreLocale( oldLocale );

		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Track"), fileName, strerror(errno) );

		return FALSE;
	}
	wSetCursor( mainD.d, wCursorWait );
	time(&clock);
	rc &= fprintf(f,"#%s Version: %s, Date: %s\n", sProdName, sVersion, ctime(&clock) )>0;
	rc &= fprintf(f, "VERSION %d %s\n", iParamVersion, PARAMVERSIONVERSION )>0;
	Stripcr( GetLayoutTitle() );
	Stripcr( GetLayoutSubtitle() );
	rc &= fprintf(f, "TITLE1 %s\n", GetLayoutTitle())>0;
	rc &= fprintf(f, "TITLE2 %s\n", GetLayoutSubtitle())>0;
	rc &= fprintf(f, "MAPSCALE %ld\n", (long)mapD.scale )>0;
	rc &= fprintf(f, "ROOMSIZE %0.6f x %0.6f\n", mapD.size.x, mapD.size.y )>0;
	rc &= fprintf(f, "SCALE %s\n", curScaleName )>0;
	rc &= WriteLayers( f );
	rc &= WriteMainNote( f );
	rc &= WriteTracks( f );
	rc &= fprintf(f, "END\n")>0;
	if ( !rc )
		NoticeMessage( MSG_WRITE_FAILURE, _("Ok"), NULL, strerror(errno), fileName );
	fclose(f);

	RestoreLocale( oldLocale );

	checkPtMark = changed;
	wSetCursor( mainD.d, defaultCursor );
	return rc;
}

/************************************************
 * Copy Dependency - copy file into another directory
 *
 * \param name
 * \param traget_dir
 *
 * \returns TRUE/FALSE for success
 *
 */
static BOOL_T CopyDependency(char * name, char * target_dir) {
		char * backname = FindFilename(name);

		BOOL_T copied = TRUE;

		FILE * source = fopen(name, "r");
		if (source != NULL) {

		   char * target_file;
		   MakeFullpath(&target_file, target_dir, backname, NULL);
		   FILE * target = fopen(target_file, "w");
		   if (target != NULL) {

			    int ch;

			   while ((ch = fgetc(source)) != EOF)
				  fputc(ch, target);

			   fprintf(stderr, "xtrkcad: Included file %s into %s \n",
			   		   				name, target_file);
		   } else {
			   NoticeMessage( MSG_COPY_FAIL, _("Continue"), NULL, name, target_file);
			   copied = FALSE;
		   }
		   fclose(source);
		   fclose(target);
		} else {
			NoticeMessage( MSG_COPY_OPEN_FAIL, _("Continue"), NULL, name);
			copied = FALSE;
		}
		return copied;
}


static doSaveCallBack_p doAfterSave;



static int SaveTracks(
		int cnt,
		char **fileName,
		void * data )
{

	assert( fileName != NULL );
	assert( cnt == 1 );

	char *nameOfFile = FindFilename(fileName[0]);

	SetCurrentPath(LAYOUTPATHKEY, fileName[0]);

	//Support Archive .zxtc files

	char * extOfFile = FindFileExtension( fileName[0]);


	if (extOfFile && (strcmp(extOfFile,"zxtc")==0)) {

		char * ArchiveName;

		//Set filename to point to be the same as the included .xtc file.
		//This is also in the manifest - in case a user renames the archive file.

		char * zip_output;

		MakeFullpath(&zip_output, workingDir, zip_pack_dir_name, NULL);

		delete_directory(zip_output);
		safe_create_dir(zip_output);

		MakeFullpath(&ArchiveName, zip_output, nameOfFile, NULL);

		extOfFile = FindFileExtension(ArchiveName);

		// Get rid of the 'z'
		for (int i=0; i<4; i++)
			extOfFile[i] = extOfFile[i+1];

		char * DependencyDir;

		//The included files are placed (for now) into an includes directory - TODO an array of includes with directories by type
		MakeFullpath(&DependencyDir, zip_output, "includes", NULL);

		safe_create_dir(DependencyDir);

		char * background = GetLayoutBackGroundFullPath();

		if (background && background[0])
			CopyDependency(background,DependencyDir);

		//The details are stored into the manifest - TODO use arrays for files, locations
		char* json_Manifest = CreateManifest(nameOfFile, background, "includes");
		char * manifest_file;

		MakeFullpath(&manifest_file, zip_output, "manifest.json", NULL);

		FILE *fp = fopen(manifest_file, "ab+");
		if (fp != NULL)
		{
		   fputs(json_Manifest, fp);
		   fclose(fp);
		} else {
			NoticeMessage( MSG_MANIFEST_FAIL, _("Continue"), NULL, manifest_file );
		}

		free(manifest_file);
		free(json_Manifest);

		DoSaveTracks( ArchiveName );

		if (create_archive(	zip_output,	fileName[0]) != TRUE) {
			NoticeMessage( MSG_ARCHIVE_FAIL, _("Continue"), NULL, fileName[0], zip_output );
		}
		free(zip_output);
		free(ArchiveName);

	} else

		DoSaveTracks( fileName[ 0 ] );

	nameOfFile = FindFilename( fileName[ 0 ] );
	wMenuListAdd( fileList_ml, 0, nameOfFile, MyStrdup(fileName[ 0 ]) );
	checkPtMark = changed = 0;

	SetLayoutFullPath(fileName[0]);

	if (doAfterSave)
		doAfterSave();
	doAfterSave = NULL;
	return TRUE;
}


EXPORT void DoSave( doSaveCallBack_p after )
{
	doAfterSave = after;
	if (*(GetLayoutFilename()) == '\0') {
		if (saveFile_fs == NULL)
			saveFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("Save Tracks"),
				sSourceFilePattern, SaveTracks, NULL );
		wFilSelect( saveFile_fs, GetCurrentPath(LAYOUTPATHKEY));
	} else {
		char *temp = GetLayoutFullPath(); 
		SaveTracks( 1, &temp, NULL );
	}
	SetWindowTitle();
	SaveState();
}

EXPORT void DoSaveAs( doSaveCallBack_p after )
{
	doAfterSave = after;
	if (saveFile_fs == NULL)
		saveFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("Save Tracks As"),
			sSourceFilePattern, SaveTracks, NULL );
	wFilSelect( saveFile_fs, GetCurrentPath(LAYOUTPATHKEY));
	SetWindowTitle();
	SaveState();
}

EXPORT void DoLoad( void )
{
	loadFile_fs = wFilSelCreate( mainW, FS_LOAD, 0, _("Open Tracks"),
		sSourceFilePattern, LoadTracks, NULL );
	wFilSelect( loadFile_fs, GetCurrentPath(LAYOUTPATHKEY));
	SaveState();
}


EXPORT void DoCheckPoint( void )
{
	int rc;

	if (checkPointingW == NULL) {
		ParamRegister( &checkPointingPG );
		checkPointingW = ParamCreateDialog( &checkPointingPG, MakeWindowTitle(_("Check Pointing")), NULL, NULL, NULL, FALSE, NULL, F_TOP|F_CENTER, NULL );
	}
	rename( checkPtFileName1, checkPtFileName2 );
	wShow( checkPointingW );
	rc = DoSaveTracks( checkPtFileName1 );

	/* could the check point file be written ok? */
	if( rc ) {
		/* yes, delete the backup copy of the checkpoint file */
		remove( checkPtFileName2 );
	} else {
		/* no, rename the backup copy back to the checkpoint file name */
		rename( checkPtFileName2, checkPtFileName1 );
	}
	wHide( checkPointingW );
}

/**
 * Remove all temporary files before exiting.When the program terminates
 * normally through the exit choice, files that are created temporarily are removed:
 * xtrkcad.ckp
 *
 * \param none
 * \return none
 *
 */

EXPORT void CleanupFiles( void )
{
	if( checkPtFileName1 )
		remove( checkPtFileName1 );
}

/**
 * Check for existance of checkpoint file. Existance of a checkpoint file means that XTrkCAD was not properly
 * terminated.
 *
 * \param none
 * \return TRUE if exists, FALSE otherwise
 *
 */

EXPORT int ExistsCheckpoint( void )
{
	struct stat fileStat;

	MakeFullpath(&checkPtFileName1, workingDir, sCheckPointF, NULL);
	MakeFullpath(&checkPtFileName2, workingDir, sCheckPoint1F, NULL);

	if( !stat( checkPtFileName1, &fileStat ) ) {
		return TRUE;
	} else {
		return FALSE;
	}
}

/**
 * Load checkpoint file
 *
 * \return TRUE if exists, FALSE otherwise
 *
 */

EXPORT int LoadCheckpoint( void )
{
	char *search;

	paramVersion = -1;
	wSetCursor( mainD.d, wCursorWait );

	MakeFullpath(&search, workingDir, sCheckPointF, NULL);
	UndoSuspend();

	if (ReadTrackFile( search, search + strlen(search) - strlen( sCheckPointF ), TRUE, TRUE, TRUE )) {
		ResolveIndex();

		RecomputeElevations();
		AttachTrains();
		DoChangeNotification( CHANGE_ALL );
		DoUpdateTitles();
	}

	Reset();
	UndoResume();

	wSetCursor( mainD.d, defaultCursor );

	SetLayoutFullPath("");
	SetWindowTitle();
	changed = TRUE;
	free( search );
	return TRUE;
}

/*****************************************************************************
 *
 * IMPORT / EXPORT
 *
 */

static struct wFilSel_t * exportFile_fs;
static struct wFilSel_t * importFile_fs;


static int ImportTracks(
		int cnt,
		char **fileName,
		void * data )
{
	char *nameOfFile;
	long paramVersionOld = paramVersion;

	assert( fileName != NULL );
	assert( cnt == 1 );

	nameOfFile = FindFilename(fileName[ 0 ]);
	paramVersion = -1;
	wSetCursor( mainD.d, wCursorWait );
	Reset();
	SetAllTrackSelect( FALSE );
	ImportStart();
	UndoStart( _("Import Tracks"), "importTracks" );
	useCurrentLayer = TRUE;
	ReadTrackFile( fileName[ 0 ], nameOfFile, FALSE, FALSE, TRUE );
	ImportEnd();
	/*DoRedraw();*/
	EnableCommands();
	wSetCursor( mainD.d, defaultCursor );
	paramVersion = paramVersionOld;
	importMove = TRUE;
	DoCommandB( (void*)(intptr_t)selectCmdInx );
	SelectRecount();
	return TRUE;
}


EXPORT void DoImport( void )
{
	if (importFile_fs == NULL)
		importFile_fs = wFilSelCreate( mainW, FS_LOAD, 0, _("Import Tracks"),
			sImportFilePattern, ImportTracks, NULL );

	wFilSelect( importFile_fs, GetCurrentPath(LAYOUTPATHKEY));
}


/**
 * Export the selected track pieces
 *
 * \param cnt IN Count of filenames, should always be 1
 * \param fileName IN array of fileNames with cnt names
 * \param data IN unused
 * \return FALSE on error, TRUE on success
 */

static int DoExportTracks(
		int cnt,
		char **fileName,
		void * data )
{
	FILE * f;
	time_t clock;
	char *oldLocale = NULL;

	assert( fileName != NULL );
	assert( cnt == 1 );

	SetCurrentPath( IMPORTPATHKEY, fileName[ 0 ] );
	f = fopen( fileName[ 0 ], "w" );
	if (f==NULL) {
		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Export"), fileName[0], strerror(errno) );
		return FALSE;
	}

	oldLocale = SaveLocale("C");

	wSetCursor( mainD.d, wCursorWait );
	time(&clock);
	fprintf(f,"#%s Version: %s, Date: %s\n", sProdName, sVersion, ctime(&clock) );
	fprintf(f, "VERSION %d %s\n", iParamVersion, PARAMVERSIONVERSION );
	ExportTracks( f );
	fprintf(f, "END\n");
	fclose(f);

	RestoreLocale( oldLocale );

	Reset();
	wSetCursor( mainD.d, defaultCursor );
	UpdateAllElevations();
	return TRUE;
}


EXPORT void DoExport( void )
{
	if (selectedTrackCount <= 0) {
		ErrorMessage( MSG_NO_SELECTED_TRK );
		return;
	}
	if (exportFile_fs == NULL)
		exportFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("Export Tracks"),
				sImportFilePattern, DoExportTracks, NULL );

	wFilSelect( exportFile_fs, GetCurrentPath(LAYOUTPATHKEY));
}



EXPORT BOOL_T EditCopy( void )
{
	FILE * f;
	time_t clock;
	char *oldLocale = NULL;

	if (selectedTrackCount <= 0) {
		ErrorMessage( MSG_NO_SELECTED_TRK );
		return FALSE;
	}
	f = fopen( clipBoardN, "w" );
	if (f == NULL) {
		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Clipboard"), clipBoardN, strerror(errno) );
		return FALSE;
	}

	oldLocale = SaveLocale("C");

	time(&clock);
	fprintf(f,"#%s Version: %s, Date: %s\n", sProdName, sVersion, ctime(&clock) );
	fprintf(f, "VERSION %d %s\n", iParamVersion, PARAMVERSIONVERSION );
	ExportTracks(f);
	fprintf(f, "END\n");
	RestoreLocale(oldLocale);
	fclose(f);
	return TRUE;
}


EXPORT BOOL_T EditCut( void )
{
	if (!EditCopy())
		return FALSE;
	SelectDelete();
	return TRUE;
}

/**
 * Paste clipboard content. XTrackCAD uses a disk file as clipboard replacement. This file is read and the
 * content is inserted.
 *
 * \return    TRUE if success, FALSE on error (file not found)
 */

EXPORT BOOL_T EditPaste( void )
{
	BOOL_T rc = TRUE;
	char *oldLocale = NULL;

	oldLocale = SaveLocale("C");

	wSetCursor( mainD.d, wCursorWait );
	Reset();
	SetAllTrackSelect( FALSE );
	ImportStart();
	UndoStart( _("Paste"), "paste" );
	useCurrentLayer = TRUE;
	if ( !ReadTrackFile( clipBoardN, sClipboardF, FALSE, TRUE, FALSE ) ) {
		NoticeMessage( MSG_CANT_PASTE, _("Continue"), NULL );
		rc = FALSE;
	}
	ImportEnd();
	/*DoRedraw();*/
	EnableCommands();
	wSetCursor( mainD.d, defaultCursor );
	importMove = TRUE;
	DoCommandB( (void*)(intptr_t)selectCmdInx );
	SelectRecount();
	UpdateAllElevations();
	RestoreLocale(oldLocale);
	return rc;
}

/*****************************************************************************
 *
 * INITIALIZATION
 *
 */

EXPORT void FileInit( void )
{
	if ( (libDir = wGetAppLibDir()) == NULL ) {
		abort();
	}
	if ( (workingDir = wGetAppWorkDir()) == NULL )
		AbortProg( "wGetAppWorkDir()" );
}

EXPORT BOOL_T ParamFileInit( void )
{
	curParamFileIndex = PARAM_DEMO;
	log_paramFile = LogFindIndex( "paramFile" );
	if ( ReadParams( lParamKey, libDir, sParamQF ) == FALSE )
		return FALSE;

	curParamFileIndex = PARAM_CUSTOM;
	if (lParamKey == 0) {
		ReadParamFiles();
		ReadCustom();
	}

	SetLayoutFullPath("");
	MakeFullpath(&clipBoardN, workingDir, sClipboardF, NULL);
	return TRUE;

}
