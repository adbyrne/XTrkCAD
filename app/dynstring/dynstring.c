/** \file dynstring.c
* Library for dynamic string functions
*/

#include <malloc.h>
#include <stdarg.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>

#include "dynstring.h"

/**
* Get the current length of the string
*
* \param s IN the dynamic string
* \return    the length of the string in bytes
*/

size_t DynStringSize(DynString *s)
{
    if (isnas(s)) {
        return 0;
    }

    return s->size;
}

/* An initialized empty struct string */
#define STRINIT() (DynStringMalloc(16))

/**
* Allocate memory for a string of the desired length. To optimize memory usage
* a minimum length of 16 bytes is allocated. The allocated string has to be freed
* using the DynStringFree() function.
*
* \param s IN pointer to DynString structure 
* \param size IN number of bytes to allocate
* \return pointer to the DynString
*/

DynString *DynStringMalloc(DynString *s, size_t size)
{
    if (size < 16) {
        size = 16;
    }

    s->s = malloc(size);
    s->size = 0;
    s->b_size = (size_t)(size | STR_FREEABLE);
    return (s);
}

/**
* Try to compact string memory by reallocating a buffer large enough for the current
* string content.
*
* \param s IN the dynamic string
*/

void DynStringRealloc(DynString *s)
{
    char *buf;

    /* Not a string? */
    if (isnas(s)) {
        return;
    }

    /* Can't realloc? */
    if (!(s->b_size & STR_FREEABLE)) {
        return;
    }

    /* Don't invoke undefined behaviour with realloc(x, 0) */
    if (!s->size) {
        free(s->s);
        s->s = malloc(16);
    } else {
        /* Try to compact */
        buf = realloc(s->s, s->size);

        if (buf) {
            s->s = buf;
        }
    }
}

/**
* Resize the string for a minimum number of bytes. In order to optimize memory usage the
* actually allocated block of memory can be larger than the requested size.
* In case of an error, the string is set to NaS
*
* \param s IN  OUT the string
* \param size IN the requested new size
*/

void DynStringResize(DynString *s, size_t size)
{
    char *buf;
    size_t bsize;

    /* Are we not a string? */
    if (isnas(s)) {
        return;
    }

    /* Not resizable */
    if (!(s->b_size & STR_FREEABLE)) {
        DynString s2;

        /* Don't do anything if we want to shrink */
        if (size <= s->size) {
            return;
        }

        /* Need to alloc a new string */
        DynStringMalloc(&s2, size);
        /* Copy into new string */
        memcpy(s2.s, s->s, s->size);
        /* Point to new string */
        s->s = s2.s;
        s->b_size = s2.b_size;
        return;
    }

    /* Too big */
    if (size & STR_FREEABLE) {
        DynString nas = NaS;
        free(s->s);
        *s = nas;
        return;
    }

    bsize = (size_t)(s->b_size - STR_FREEABLE);

    /* Keep at least 16 bytes */
    if (size < 16) {
        size = 16;
    }

    /* Nothing to do? */
    if ((4 * size > 3 * bsize) && (size <= bsize)) {
        return;
    }

    /* Try to double size instead of using a small increment */
    if ((size > bsize) && (size < bsize * 2)) {
        size = bsize * 2;
    }

    /* Keep at least 16 bytes */
    if (size < 16) {
        size = 16;
    }

    buf = realloc(s->s, size);

    if (!buf) {
        DynString nas = NaS;
        /* Failed, go to NaS state */
        free(s->s);
        *s = nas;
    } else {
        s->s = buf;
        s->b_size = (size_t)(size | STR_FREEABLE);
    }
}

/**
* Free the previously allocated string.
*
* \param s IN OUT the dynamic string
*/

void DynStringFree(DynString *s)
{
    DynString nas = NaS;

    if (s->b_size & STR_FREEABLE) {
        free(s->s);
    }

    *s = nas;
}

/**
* Create a newly allocated copy of the passed dynamic string.
*
* \param s IN the dynamic string
* \return the newly allocated dynamic string
*/

/* Create a new string as a copy of an old one */
DynString *DynStringDupStr(DynString *s2, DynString *s)
{
    DynString nas = NaS;

    /* Not a string? */
    if (isnas(s)) {
        return NULL;
    }

    DynStringMalloc(s2, s->size);
    s2->size = s->size;
    memcpy(s2->s, s->s, s->size);
    return s2;
}

/**
* Copy the memory from the source string into the dest string.
*
* \param dest IN the destination dynamic string
* \param src IN the source dynamic string
*/

void DynStringCpyStr(DynString *dest, DynString *src)
{
    /* Are we no a string */
    if (isnas(src)) {
        return;
    }

    DynStringResize(dest, src->size);

    if (isnas(dest)) {
        return;
    }

    dest->size = src->size;
    memcpy(dest->s, src->s, src->size);
}

/**
* Return the content of the dynamic string as a \0 terminated C string. This memory may not be freed by the
* caller.
*
* \param s IN the dynamic string
* \return the C string
*/

char *DynStringToCStr(DynString *s)
{
    size_t bsize;

    /* Are we not a string? */
    if (isnas(s)) {
        return NULL;
    }

    /* Get real buffer size */
    bsize = s->b_size & ~STR_FREEABLE;

    if (s->size == bsize) {
        /* Increase buffer size */
        DynStringResize(s, bsize + 1);

        /* Are we no longer a string? */
        if (isnas(s)) {
            return NULL;
        }
    }

    /* Tack a zero on the end */
    s->s[s->size] = 0;
    /* Don't update the size */
    /* Can use this buffer as long as you don't append anything else */
    return s->s;
}

/**
* Concatenate a number of bytes from the source string to the dynamic string.
*
* \param s IN the dynamic string
* \param len IN the number of bytes to append to the dynamic string
* \param str IN the source string
*/

void DynStringNCatCStr(DynString *s, size_t len, const char *str)
{
    size_t bsize;

    /* Are we not a string? */
    if (isnas(s)) {
        return;
    }

    /* Nothing to do? */
    if (!str || !len) {
        return;
    }

    /* Get real buffer size */
    bsize = s->b_size & ~STR_FREEABLE;

    if (s->size + len >= bsize) {
        DynStringResize(s, s->size + len);

        /* Are we no longer a string? */
        if (isnas(s)) {
            return;
        }
    }

    memcpy(&s->s[s->size], str, len);
    s->size += len;
}

/**
* Concatenate a zero-terminated source string to the dynamic string.
*
* \param s IN the dynamic string
* \param str IN the source string
*/

void DynStringCatCStr(DynString *s, const char *str)
{
    if (str) {
        DynStringNCatCStr(s, strlen(str), str);
    }
}

/**
* Concatenate a dynamic string to another dynamic string.
*
* \param s IN the destination dynamic string
* \param s2 IN the source dynamic string
*/

void DynStringCatStr(DynString *s, const DynString *s2)
{
    DynStringNCatCStr(s, s2->size, s2->s);
}

/**
* Concatenate a variable number zero terminated strings to the dynamic string. The
* list of source strings has to be terminated by a NULL pointer.
*
* \param s IN the dynamic string
* \param ... IN variable number of C strings
*/

void DynStringCatCStrs(DynString *s, ...)
{
    const char *str;
    va_list v;

    /* Are we not a string? */
    if (isnas(s)) {
        return;
    }

    va_start(v, s);

    for (str = va_arg(v, const char *); str; str = va_arg(v, const char *)) {
        DynStringNCatCStr(s, strlen(str), str);
    }

    va_end(v);
}

/**
* Concatenate a variable number of dynamic string to another dynamic string.
* The list of source strings has to be terminated by a NULL pointer.
*
* \param s IN the destination dynamic string
* \param s2 IN the source dynamic strings
*/

void DynStringCatStrs(DynString *s1, ...)
{
    const DynString *s2;
    va_list v;

    /* Are we not a string? */
    if (isnas(s1)) {
        return;
    }

    va_start(v, s1);

    for (s2 = va_arg(v, const DynString *); s2; s2 = va_arg(v, const DynString *)) {
        DynStringNCatCStr(s1, s2->size, s2->s);
    }

    va_end(v);
}

/**
* Return a formatted dynamic string. Formatting is performed similar to the printf style
* of functions.
*
* \param s IN the dynamic string
* \param fmt IN format specification
* \param ... IN values
*/

void DynStringPrintf(DynString *s, const char *fmt, ...)
{
    va_list v;
    size_t len;
    DynString nas = NaS;

    ///* Are we not a string? */
    //if (isnas(s)) {
    //    *s = STRINIT();
    //}

    /* Nothing to do? */
    if (!fmt) {
        return;
    }

    va_start(v, fmt);
    len = vsnprintf(NULL, 0, fmt, v) + 1;
    va_end(v);
    DynStringResize(s, len);

    /* Are we no longer a string? */
    if (isnas(s)) {
        return;
    }

    va_start(v, fmt);
    vsnprintf(s->s, len, fmt, v);
    va_end(v);
    s->size = len - 1;
}
