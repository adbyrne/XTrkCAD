/** \file archive.c
 *   ARCHIVE PROCESSING
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

#include <errno.h>
#include <fcntl.h>

#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <zip.h>

#ifdef WINDOWS
    #include "../wlib/mswlib/dirent.h"
    #include <direct.h>
    #include <io.h>
    #include <process.h>
    #define unlink(a) _unlink((a))
    #define rmdir(a) _rmdir((a))
    #define open(name, flag, mode) _open((name), (flag), (mode))
    #define write(file, buffer, count) _write((file),(buffer), (count))
    #define close(file) _close((file))
    #define getpid() _getpid()
#else
    #include <dirent.h>
    #include <unistd.h>
#endif

#include <wlib.h>
#include "archive.h"
#include "directory.h"
#include "dynstring.h"
#include "i18n.h"
#include "messages.h"
#include "misc.h"
#include "misc2.h"
#include "paths.h"

int log_zip = 0;

/**
 * Create the full path for temporary directories used in zip archive operations
 *
 * \param archive operation
 * \return pointer to full path, must be free'd by caller
 */

char *
GetZipDirectoryName(enum ArchiveOps op)
{
    char *opDesc;
    char *directory;
    DynString zipDirectory;

	DynStringMalloc(&zipDirectory, 0);

    switch (op) {
    case ARCHIVE_READ:
        opDesc = "in";
        break;
    case ARCHIVE_WRITE:
        opDesc = "out";
        break;
    default:
        opDesc = "err";
        break;
    }

    DynStringPrintf(&zipDirectory,
                    "%s" FILE_SEP_CHAR "zip_%s.%d",
                    workingDir,
                    opDesc,
                    getpid());

    directory = strdup(DynStringToCStr(&zipDirectory));
    DynStringFree(&zipDirectory);
    return (directory);
}

/*****************************************************************************
 * Add directory to archive
 *
 * \param IN zip 		The open zip archive handle
 * \param IN dir_path 	The path to add
 * \param IN prefix 	The prefix in the archive
 *
 * \returns 	TRUE if OK
 */

BOOL_T AddDirectoryToArchive(
    struct zip * za,
    const char * dir_path,
    const char * prefix)
{

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
        NoticeMessage(MSG_NOT_DIR_FAIL,
                      _("Continue"), NULL, dir_path);
        return FALSE;
    }

    // if not possible to read the directory for this user
    if ((dir = opendir(dir_path)) == NULL) {
        NoticeMessage(MSG_OPEN_DIR_FAIL,
                      _("Continue"), NULL, dir_path);
        return FALSE;
    }

    // iteration through entries in the directory
    while ((entry = readdir(dir)) != NULL) {
        // skip entries "." and ".."
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
            continue;
        }

        // determinate a full path of an entry
        MakeFullpath(&full_path, dir_path, entry->d_name, NULL);

        // stat for the entry
        stat(full_path, &stat_entry);

        if (prefix && prefix[0]) {
            MakeFullpath(&arch_path, prefix, entry->d_name, NULL);
        } else {
            MakeFullpath(&arch_path, entry->d_name, NULL);
        }

        // recursively add a nested directory
        if (S_ISDIR(stat_entry.st_mode) != 0) {
            if (zip_dir_add(za, arch_path, 0) < 0) {
                zip_error_t  *ziperr = zip_get_error(za);
                buf = zip_error_strerror(ziperr);
                NoticeMessage(MSG_ZIP_DIR_ADD_FAIL,
                              _("Continue"), NULL, arch_path, buf);
#if DEBUG
                printf("Added Directory %s \n", arch_path);
#endif
            }

            if (AddDirectoryToArchive(za, full_path, arch_path) != TRUE) {
                free(full_path);
                free(arch_path);
                return FALSE;
            }
            free(arch_path);
            continue;
        } else {
            zt = zip_source_file(za, full_path, 0, -1);
            if (zip_file_add(za, arch_path, zt, 0) == -1) {
                zip_error_t  *ziperr = zip_get_error(za);
                buf = zip_error_strerror(ziperr);
                NoticeMessage(MSG_ZIP_FILE_ADD_FAIL, _("Continue"), NULL, full_path, arch_path,
                              buf);
                free(full_path);
                free(arch_path);
                return FALSE;
            }
#if DEBUG
            printf("Added File %s", full_path);
#endif
        }
        free(arch_path);
        free(full_path);
    }

    closedir(dir);
    return TRUE;
}

/***********************************************************************
 * Create Archive
 *
 * \param IN dir_path The place to create the archive
 * \param IN fileName The name of the archive
 *
 * \return TRUE if ok
 */

BOOL_T CreateArchive(
    const char * dir_path,
    const char * fileName)
{
    struct zip *za;
    int err;
    char buf[100];

    char * archive = strdup(fileName);  	// Because of const char
    char * archive_name = FindFilename(archive);
    char * archive_path;

    MakeFullpath(&archive_path, workingDir, archive_name, NULL);

    free(archive);

    if ((za = zip_open(archive_path, ZIP_CREATE, &err)) == NULL) {
        zip_error_to_str(buf, sizeof(buf), err, errno);
        NoticeMessage(MSG_ZIP_CREATE_FAIL, _("Continue"), NULL, archive_path, buf);
        free(archive_path);
        return FALSE;
    }
#if DEBUG
    printf("====================== \n");
    printf("Started Archive %s", archive_path);
#endif

    AddDirectoryToArchive(za, dir_path, "");

    if (zip_close(za) == -1) {
        zip_error_to_str(buf, sizeof(buf), err, errno);
        NoticeMessage(MSG_ZIP_CLOSE_FAIL, _("Continue"), NULL, archive_path, buf);
        free(archive_path);
        return FALSE;
    }

    unlink(fileName); 							//Delete Old
    if (rename(archive_path, fileName) == -1) {	//Move zip into place
        NoticeMessage(MSG_ZIP_RENAME_FAIL, _("Continue"), NULL, archive_path, fileName,
                      strerror(errno));
        free(archive_path);
        return FALSE;
    }
    free(archive_path);

#if DEBUG
    printf("Moved Archive to %s", fileName);
    printf("====================== \n");
#endif
    return TRUE;
}

/**************************************************************************
 * Unpack_Archive_for
 *
 * \param IN pathName the name of the archive
 * \param IN fileName just the filename and extension of the layout
 * \param IN tempDir The directory to use to unpack into
 *
 * \returns TRUE if all worked
 */
BOOL_T UnpackArchiveFor(
    const char * pathName,  /*Full name of archive*/
    const char * fileName,  /*Layout name and extension */
    const char * tempDir,   /*Directory to unpack into */
    BOOL_T file_only)
{
    char *dirName;
    struct zip *za;
    struct zip_file *zf;
    struct zip_stat sb;
    char buf[100];
    int err;
    int i;
    int64_t len;
    FILE  *fd;
    long long sum;


    if ((za = zip_open(pathName, 0, &err)) == NULL) {
        zip_error_to_str(buf, sizeof(buf), err, errno);
        NoticeMessage(MSG_ZIP_OPEN_FAIL, _("Continue"), NULL, pathName, buf);
        fprintf(stderr, "xtrkcad: can't open xtrkcad zip archive `%s': %s \n",
                pathName, buf);
        return FALSE;
    }

    for (i = 0; i < zip_get_num_entries(za, 0); i++) {
        if (zip_stat_index(za, i, 0, &sb) == 0) {
            len = strlen(sb.name);

#if DEBUG
            printf("==================\n");
            printf("Name: [%s], ", sb.name);
            printf("Size: [%llu], ", sb.size);
            printf("mtime: [%u]\n", (unsigned int)sb.mtime);
            printf("mtime: [%u]\n", (unsigned int)sb.mtime);
#endif

            LOG(log_zip, 1, ("================= \n"))
            LOG(log_zip, 1, ("Zip-Name [%s] \n", sb.name))
            LOG(log_zip, 1, ("Zip-Size [%llu] \n", sb.size))
            LOG(log_zip, 1, ("Zip-mtime [%u] \n", (unsigned int)sb.mtime))

            if (sb.name[len - 1] == '/' && !file_only) {
                MakeFullpath(&dirName, tempDir, &sb.name[0], NULL);
                if (SafeCreateDir(dirName) != TRUE) {
                    free(dirName);
                    return FALSE;
                }
                free(dirName);
            } else {
                zf = zip_fopen_index(za, i, 0);
                if (!zf) {
                    NoticeMessage(MSG_ZIP_INDEX_FAIL, _("Continue"), NULL);
                    fprintf(stderr, "xtrkcad zip archive open index error \n");
                    return FALSE;
                }

                if (file_only) {
                    if (strncmp(sb.name, fileName, strlen(fileName)) != 0) {
                        continue; /* Ignore any other files than the one we asked for */
                    }
                }
                MakeFullpath(&dirName, tempDir, &sb.name[0], NULL);
                fd = fopen(dirName, "wb");
                if (!fd) {
                    NoticeMessage(MSG_ZIP_FILE_OPEN_FAIL, _("Continue"), NULL, dirName,
                                  strerror(errno));
                    free(dirName);
                    return FALSE;
                }

                sum = 0;
                while (sum != sb.size) {
                    len = zip_fread(zf, buf, 100);
                    if (len < 0) {
                        NoticeMessage(MSG_ZIP_READ_FAIL, _("Continue"), NULL, dirName, &sb.name[0]);
                        free(dirName);
						fclose(fd);
                        return FALSE;
                    }
                    fwrite(buf, 1, (unsigned int)len, fd);
                    sum += len;
                }
                fclose(fd);
                free(dirName);
                zip_fclose(zf);
            }
        } else {
            LOG(log_zip, 1, ("Zip-Unknown File[%s] Line[%d] \n", __FILE__, __LINE__))
#if DEBUG
            printf("File[%s] Line[%d]\n", __FILE__, __LINE__);
#endif
        }
    }

    if (zip_close(za) == -1) {
        NoticeMessage(MSG_ZIP_CLOSE_FAIL, _("Continue"), NULL, dirName, &sb.name[0]);
        return FALSE;
    }
    return TRUE;
}

