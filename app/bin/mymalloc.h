#ifndef MYMALLOC_H
#define MYMALLOC_H

/*
 * Safe Memory etc
 */
extern void SetMallocLog(void);
extern BOOL_T TestMallocs(void);
extern void* MyMalloc(size_t);
extern void* MyRealloc(void*, size_t);
extern void MyFree(void*);
extern void* memdup(void*, size_t);
extern char* MyStrdup(const char*);

#endif //MYMALLOC_H
