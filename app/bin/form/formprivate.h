#ifndef HAVE_FORMPRIVATE_H
#define HAVE_FORMPRIVATE_H

#include <form.h>

extern int log_form;

extern char* prefSect;


void FormSaveDefaultValues(paramGroup_p group);

/* checkinputs.c */

wBool_t FormCheckInputs(paramGroup_p group, wControl_p b);

unsigned long FormIntegerGetValue(paramData_p data, const char* enteredValue);
FLOAT_T FormFloatGetValue(paramData_p data, const char* enteredValue);

void FormStringGetValue(paramData_p data, char* value);

/* formdistance.c */
FLOAT_T FormDecodeNumber(const char* enteredValue, BOOL_T* validP, int option, BOOL_T bDistance);
FLOAT_T FormDecodeDistance(const char *enteredValue, BOOL_T* validP);
FLOAT_T FormDecodeFloat(const char* enteredValue, BOOL_T* validP);
char* FormGetParseError();

/* actionbuttons.c */

void FormButtonCancel(void* groupVP);


#endif // HAVE_FORMPRIVATE_H
