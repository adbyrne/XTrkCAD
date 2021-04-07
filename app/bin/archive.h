#ifndef HAVE_ARCHIVE_H
#define HAVE_ARCHIVE_H
#include <zip.h>
#include "common.h"

enum ArchiveOps { ARCHIVE_READ, ARCHIVE_WRITE }; // has to be contiguous, see CleanupFiles()!

extern int log_zip;
extern const char *workingDir;

char *GetZipDirectoryName(enum ArchiveOps op);
BOOL_T AddDirectoryToArchive(struct zip * za, const char * dir_path, const char * prefix);
BOOL_T CreateArchive(const char * dir_path, const char * fileName);
BOOL_T UnpackArchiveFor(const char * pathName, const char * fileName, const char * tempDir, BOOL_T file_only);
#endif
