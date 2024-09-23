

#ifndef HAVE_PARAMPRIVATE_H
#define HAVE_PARAMPRIVATE_H

extern int log_paraminput;
extern int log_paramLayout;
extern int paramCheckErrorCount;

extern dynArr_t paramGroups_da;
extern BOOL_T paramGroups_init;
#define paramGroups(N) DYNARR_N( paramGroup_p, paramGroups_da, N )

#endif // !HAVE_PARAMPRIVATE_H

