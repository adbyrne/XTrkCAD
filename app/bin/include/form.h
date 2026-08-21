#ifndef HAVE_FORM_H
#include "param.h"
#define HAVE_FORM_H
void FormInit(void);
void FormRegister(paramGroup_p pg);
void FormLoadDefaultValues(paramGroup_p pg);
void FormSaveDefaultValues(paramGroup_p pg);
void FormUpdatePrefs(void);

paramGroup_cp DialogGroupFind( const char * sName );
paramGroup_cp * DialogGroupIter( paramGroup_cp * );

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
void FormMenuPush(void* dp);

void FormStartRecord(FILE* fileHandle);
void FormMacroRecord(char* format, ...);
void FormGroupRecord(paramGroup_cp pg);
void FormErrorState( paramData_p p, wBool_t valid, const char *reason );

wBool_t FormIntegerRangeCheck(paramData_p p, long valL);
wBool_t FormFloatRangeCheck(paramData_p p, FLOAT_T valF);
wBool_t FormStringCheckValue(paramData_p data, char* value);
wBool_t FormCheckInputs(paramGroup_p group, wControl_p b);
long FormUpdate(paramGroup_p pg);

void FormDialogOkActive(paramGroup_p pg, int active);
void FormButtonOk( paramGroup_p group);
void FormCancel_Undo(paramGroup_p group );
void FormCancel_Current(paramGroup_p group);
void FormCancel_Reset(paramGroup_p group);
void FormCancel_Restore(paramGroup_p group);
void FormCancel_Null(paramGroup_cp group);

#define FormCancel_Custom( PROC ) PROC

void FormControlActive(paramGroup_p pg, int inx, BOOL_T active);
void FormControlShow(paramGroup_p pg, int inx, BOOL_T bShow);
EXPORT void FormHilite( wWin_p win, wControl_p control, BOOL_T hilite );
void FormGroupReveal(paramGroup_p pg, const char *id, BOOL_T reveal);
typedef void (*paramExpanderToggleProc)(paramGroup_p pg, const char *id,
                                        BOOL_T revealed, void *context);

void FormGroupExpanderShow(paramGroup_p pg, const char *id, BOOL_T reveal);
void FormGroupExpanderSetSummary(paramGroup_p pg, const char *id,
                                 const char *summary);
void FormGroupSetShadow(paramGroup_p pg, const char *id, BOOL_T shown);
void FormResetInvalid( wControl_p win );

void FormMenuPush(void* dp);

void FormFetchData(paramGroup_p pg);

#endif // HAVE_FORM_H


