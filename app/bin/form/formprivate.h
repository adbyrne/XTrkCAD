#ifndef HAVE_FORMPRIVATE_H
#define HAVE_FORMPRIVATE_H

extern int log_form;

void CreateControls(paramGroup_p group);
void FormSaveDefaultValues(paramGroup_p group);
wBool_t FormCheckInputs(paramGroup_p group, wControl_p b);
wBool_t FormIntegerRangeCheck(paramData_p p, long valL);
wBool_t FormFloatRangeCheck(paramData_p p, FLOAT_T valF);
unsigned long FormIntegerGetValue(paramData_p data, const char* enteredValue);
FLOAT_T FormFloatGetValue(paramData_p data, const char* enteredValue);

FLOAT_T FormDecodeDistance(const char *enteredValue, BOOL_T* validP);
char* FormGetParseError();

#endif // HAVE_FORMPRIVATE_H
