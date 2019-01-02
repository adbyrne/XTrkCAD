#ifndef HAVE_PARAMFILE_H
	#define HAVE_PARAMFILE_H
	#include <stdbool.h>

	#include "common.h"

	enum paramFileState { PARAMFILE_UNLOADED, PARAMFILE_COMPATIBLE, PARAMFILE_FIT, PARAMFILE_MAXSTATE };

	extern DIST_T curBarScale;
	extern dynArr_t paramProc_da;
	extern dynArr_t paramFileInfo_da;

	void ParamCheckSumLine(char * line);
	wBool_t IsParamValid(int fileInx);
	bool IsParamFileDeleted(int fileInx);
	void SetParamFileDeleted(int fileInx, bool deleted);
	char * GetParamFileName(int fileInx);
	char * GetParamFileContents(int fileInx);
	bool ReadParams(long key, const char * dirName, const char * fileName);
#endif // !HAVE_PARAMFILE_H
