#ifndef HAVE_TABSTRING_H
#define HAVE_TABSTRING_H

#include <common.h>

typedef struct tabString_s{
	char* ptr;
	unsigned long len;
} tabString_t;

typedef struct tabString_s* tabString_p;

#define T_MANUF			(0)
#define T_PROTO			(1)
#define T_DESC			(2)
#define T_PART			(3)
#define T_ROADNAME		(4)
#define T_REPMARK		(5)
#define T_NUMBER		(6)

void TabStringExtract(char* string, int count, tabString_t* tabs);

char* TabStringDup(const tabString_t* tab);

char* TabStringCpy(char* dst, const tabString_t* tab);

int TabStringCmp(char* src, tabString_t* tab);

long TabGetLong(tabString_t* tab);

FLOAT_T TabGetFloat(tabString_t* tab);

#endif
