#ifndef HAVE_DIALOGS_H
#define HAVE_DIALOGS_H
void DialogsInit(void);
void DialogsRegister(paramGroup_p pg);
void DialogsSetDefaultValues(paramGroup_p pg);
wControl_p DialogsCreateDialog(
	paramGroup_p group,
	char* title,
	char* okLabel,
	paramActionOkProc okProc,
	char* cancelLabel,
	paramActionCancelProc cancelProc,
	BOOL_T needHelpButton,
	long winOption,
	paramChangeProc changeProc);
#endif // HAVE_DIALOGS_H


