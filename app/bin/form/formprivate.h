#ifndef HAVE_FORMPRIVATE_H
#define HAVE_FORMPRIVATE_H

#include <form.h>

extern int log_form;

extern char* prefSect;


void FormSaveDefaultValues(paramGroup_p group);

/* checkinputs.c */

unsigned long FormIntegerGetValue(paramData_p data, const char* enteredValue);
FLOAT_T FormFloatGetValue(paramData_p data, const char* enteredValue);

void FormStringGetValue(paramData_p data, char* value);

/* loadcontrols.c */

char *FormFloatFormat(paramData_p entry, FLOAT_T value);

/* formdistance.c */
FLOAT_T FormDecodeNumber(const char* enteredValue, BOOL_T* validP, int option,
                         BOOL_T bDistance);
FLOAT_T FormDecodeDistance(const char *enteredValue, BOOL_T* validP);
FLOAT_T FormDecodeFloat(const char* enteredValue, BOOL_T* validP);
char* FormGetParseError();

#endif // HAVE_FORMPRIVATE_H
