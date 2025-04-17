#ifndef HAVE_FORM_H
#include "param.h"
#define HAVE_FORM_H
void FormInit(void);
void FormRegister(paramGroup_p pg);
void FormLoadDefaultValues(paramGroup_p pg);
wControl_p FormCreateDialog(
	paramGroup_p group,
	char* title,
	char* okLabel,
	paramActionOkProc okProc,
	char* cancelLabel,
	paramActionCancelProc cancelProc,
	BOOL_T needHelpButton,
	long winOption,
	paramChangeProc changeProc);

void FormCreateControls(paramGroup_p group, paramChangeProc changeProc);
void FormLoadControls(paramGroup_p pg);
void FormLoadSingleControl(paramGroup_p pg, int inx);

void FormStartRecord(FILE* fileHandle);
void FormMacroRecord(char* format, ...);


#endif // HAVE_FORM_H


