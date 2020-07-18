#include <stddef.h>

#ifndef HAVE_STRINGXTC_H
	#define HAVE_STRINGXTC_H
	size_t strscpy(char *dest, const char *src, size_t count);
	char *XtcStrlwr(char *str);
	int	XtcStricmp(const char *a, const char *b);
#endif
