/** \file directory.c
 * Directory Management
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
#include <string.h>

#ifdef WINDOWS
    #include "include/dirent.h"
    #include <direct.h>
    #define unlink(a) _unlink((a))
    #define rmdir(a) _rmdir((a))
#else
    #include <dirent.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

#include <wlib.h>
#include "directory.h"
#include "dynstring.h"
#include "i18n.h"
#include "messages.h"
#include "misc.h"

/*****************************************************************************
 * Safe Create Dir
 * \param IN dir The directory path to create
 *
 * \return TRUE if ok
 *
 */

BOOL_T SafeCreateDir(const char *dir)
{
    int err;

#ifdef WINDOWS
    err = _mkdir(dir);
#else
    err = mkdir(dir, 0755);
#endif
    if (err < 0) {
        if (errno != EEXIST) {
            NoticeMessage(MSG_DIR_CREATE_FAIL,
                          _("Continue"), NULL, dir, strerror(errno));
            perror(dir);
            return FALSE;
        }
    }
    return TRUE;
}

/************************************************
 * DeleteDirectory empties and removes a directory recursively
 *
 * \param IN dir_path The Directory to empty and remove
 *
 * \return TRUE if ok
 *
 */
BOOL_T DeleteDirectory(const char *dir_path)
{
    size_t path_len;
    char *full_path = NULL;
    DIR *dir;
    struct stat stat_path, stat_entry;
    struct dirent *entry;
    DynString path;

    // stat for the path
    int resp = stat(dir_path, &stat_path);

    if (resp != 0 && errno == ENOENT) {
        return TRUE;    //Does not Exist
    }

    // if path is not dir - exit
    if (!(S_ISDIR(stat_path.st_mode))) {
        NoticeMessage(MSG_NOT_DIR_FAIL,
                      _("Continue"), NULL, dir_path);
        return FALSE;
    }

    // if not possible to read the directory for this user
    if ((dir = opendir(dir_path)) == NULL) {
        NoticeMessage(MSG_DIR_OPEN_FAIL,
                      _("Continue"), NULL, dir_path);
        return FALSE;
    }

    // the length of the path
    path_len = strlen(dir_path) + 1;
    DynStringMalloc(&path, path_len + 16);	//guessing the total path length

    // iteration through entries in the directory
    while ((entry = readdir(dir)) != NULL) {

        // skip entries "." and ".."
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
            continue;
        }

        // determinate a full path of an entry
        DynStringReset(&path);
        DynStringCatCStrs(&path, dir_path, FILE_SEP_CHAR, entry->d_name, NULL);
        full_path = DynStringToCStr(&path);
        // stat for the entry
        stat(full_path, &stat_entry);

        // recursively remove a nested directory
        if (S_ISDIR(stat_entry.st_mode) != 0) {
            DeleteDirectory(full_path);
            continue;
        }

        // remove a file object
        if (unlink(full_path)) {
            NoticeMessage(MSG_UNLINK_FAIL, _("Continue"), NULL, full_path);
            DynStringFree(&path);
            closedir(dir);
            return FALSE;
        } else {
#if DEBUG
            printf("Removed a file: %s \n", full_path);
#endif
        }
    }

    closedir(dir);
    DynStringFree(&path);

    // remove the devastated directory and close the object of it
    if (rmdir(dir_path)) {
        NoticeMessage(MSG_RMDIR_FAIL, _("Continue"), NULL, dir_path);
        return FALSE;
    } else {
#if DEBUG
        printf("Removed a directory: %s \n", dir_path);
#endif
    }
    return TRUE;
}
