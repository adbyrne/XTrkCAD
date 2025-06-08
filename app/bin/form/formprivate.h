#ifndef HAVE_FORMPRIVATE_H
#define HAVE_FORMPRIVATE_H

extern int log_form;

extern char* prefSect;


void FormSaveDefaultValues(paramGroup_p group);

/* checkinputs.c */

wBool_t FormCheckInputs(paramGroup_p group, wControl_p b);
wBool_t FormIntegerRangeCheck(paramData_p p, long valL);
wBool_t FormFloatRangeCheck(paramData_p p, FLOAT_T valF);
unsigned long FormIntegerGetValue(paramData_p data, const char* enteredValue);
FLOAT_T FormFloatGetValue(paramData_p data, const char* enteredValue);
wBool_t FormStringCheckValue(paramData_p data, char* value);
void FormStringGetValue(paramData_p data, char* value);

/* formdistance.c */

FLOAT_T FormDecodeDistance(const char *enteredValue, BOOL_T* validP);
char* FormGetParseError();

/* actionbuttons.c */

void FormButtonOk(void* groupVP);

void FormButtonCancel(void* groupVP);


#endif // HAVE_FORMPRIVATE_H
