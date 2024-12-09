#ifndef HAVE_FORMPRIVATE_H
#define HAVE_FORMPRIVATE_H

extern int log_form;

void CreateControls(paramGroup_p group);
void FormSaveDefaultValues(paramGroup_p group);
wBool_t FormCheckInputs(paramGroup_p group, wControl_p b);
wBool_t FormIntegerRangeCheck(paramData_p p, long valL);
wBool_t FormFloatRangeCheck(paramData_p p, FLOAT_T valF);

#endif // HAVE_FORMPRIVATE_H
