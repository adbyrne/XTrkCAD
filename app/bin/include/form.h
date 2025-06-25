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

void FormCreateControls(paramGroup_p group);
void FormLoadControls(paramGroup_p pg);
void FormLoadSingleControl(paramGroup_p pg, int inx);
void FormLoadMessage(paramGroup_p pg, int inx, char* message);

void FormStartRecord(FILE* fileHandle);
void FormMacroRecord(char* format, ...);
void FormGroupRecord(paramGroup_p pg);

wBool_t FormIntegerRangeCheck(paramData_p p, long valL);
wBool_t FormFloatRangeCheck(paramData_p p, FLOAT_T valF);
wBool_t FormStringCheckValue(paramData_p data, char* value);

void FormDialogOkActive(paramGroup_p pg, int active);
void FormCancel_Undo(wWin_p winP);
void FormCancel_Current(paramGroup_p group);
void FormCancel_Reset(paramGroup_p group);
void FormCancel_Restore(paramGroup_p group);

//EXPORT void* FormCancel_Null = NULL;

#define FormCancel_Custom( PROC ) PROC

void FormControlActive(paramGroup_p pg, int inx, BOOL_T active);
void FormMenuPush(void* dp);

#endif // HAVE_FORM_H


