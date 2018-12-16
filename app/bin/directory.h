#ifndef HAVE_DIRECTORY_H
	#define HAVE_DIRECTORY_H
	#include "common.h"
	BOOL_T SafeCreateDir(const char *dir);
	BOOL_T DeleteDirectory(const char *dir_path);
#endif