#ifndef HAVE_DYNSTRING_H
#define HAVE_DYNSTRING_H

typedef struct DynString DynString;
struct DynString
{
	char *s;
	size_t size;		// length of the string
	size_t b_size;		//  length of the buffer containing the string
};

#define NaS {NULL, 0, 0}
#define isnas(S) (!(S)->s)
#define STR_FREEABLE (1ULL << 63)

size_t DynstrSize(DynString * s);

DynString * DynstrMalloc(size_t size);

 void DynstrRealloc(DynString * s);

 void DynstrResize(DynString * s, size_t size);

 void DynstrFree(DynString * s);

 DynString * DynstrDupStr(DynString * s);

 void DynstrCpyStr(DynString * dest, DynString * src);

 char * DynstrToCStr(DynString * s);

 void DynstrNCatCStr(DynString * s, size_t len, const char * str);

 void DynstrCatCStr(DynString * s, const char * str);

 void DynstrCatStr(DynString * s, const DynString * s2);

 void DynstrCatCStrs(DynString * s, ...);

 void DynstrCatStrs(DynString * s1, ...);

 void DynstrPrintf(DynString * s, const char * fmt, ...);

#endif // !HAVE_DYNSTRING_H