#ifndef HAVE_PARAMFILE_H
	#define HAVE_PARAMFILE_H
	#include <stdbool.h>
	#include <wlib.h>
	#include "common.h"

	extern DIST_T curBarScale;
	extern dynArr_t paramProc_da;
	extern dynArr_t paramFileInfo_da;

	void ParamCheckSumLine(char * line);
	wBool_t IsParamValid(int fileInx);
	bool IsParamFileDeleted(int fileInx);
	bool IsParamFileFavorite(int fileInx);
	void SetParamFileState(int index);
	int ReadParamFile(const char *fileName);
	void SetParamFileDeleted(int fileInx, bool deleted);
	void SetParamFileFavorite(int fileInx, bool favorite);
	char * GetParamFileName(int fileInx);
	char * GetParamFileContents(int fileInx);
	bool ReadParams(long key, const char * dirName, const char * fileName);

	#define CONTENTSCOMMAND "CONTENTS"
	char *GetParameterFileContent(char *file);
#endif // !HAVE_PARAMFILE_H
