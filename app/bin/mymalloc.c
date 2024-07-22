/**
 * \file   mymalloc.c
 * \brief  Memory management 
 *
 * \author 
 */

 /*  XTrackCad - Model Railroad CAD
  *  Copyright (C) 2005, 2024 Dave Bullis
  *
  *  This program is free software; you can redistribute it and/or modify
  *  it under the terms of the GNU General Public License as published by
  *  the Free Software Foundation; either version 2 of the License, or
  *  (at your option) any later version.
  *
  *  This program is distributed in the hope that it will be useful,
  *  but WITHOUT ANY WARRANTY; without even the implied warranty of
  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  *  GNU General Public License for more details.
  *
  *  You should have received a copy of the GNU General Public License
  *  along with this program; if not, write to the Free Software
  *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
  */

#include <stdio.h>
#include <inttypes.h>
#include "common.h"

#define SLOG_FMT "0x%.12" PRIxPTR

  /****************************************************************************
   *
   * MEMORY ALLOCATION
   *
   */

static size_t totalMallocs = 0;
static size_t totalMalloced = 0;
static size_t totalRealloced = 0;
static size_t totalReallocs = 0;
static size_t totalFreeed = 0;
static size_t totalFrees = 0;

static void* StorageLog;

typedef struct slog_t {
	void* storage_p;
	size_t storage_size;
	BOOL_T freed;
} slog_t, * slog_p;

static int StorageLogCurrent = 0;


#define LOG_SIZE 1000000


static unsigned long guard0 = 0xDEADBEEF;
static unsigned long guard1 = 0xAF00BA8A;
static int log_malloc;

static void RecordMalloc(void* p, size_t size)
{


	if (!StorageLog) { StorageLog = malloc(sizeof(slog_t) * LOG_SIZE); }
	slog_p log_p = StorageLog;
	if (StorageLogCurrent < LOG_SIZE) {
		log_p[StorageLogCurrent].storage_p = p;
		log_p[StorageLogCurrent].storage_size = size;
		StorageLogCurrent++;
	}
	else {
		printf("Storage Log size exceeded, wrapped\n");
		log_p[0].storage_p = p;
		log_p[0].storage_size = size;
		StorageLogCurrent = 1;
	}
}

static void RecordMyFree(void* p)
{
	slog_p log_p = StorageLog;
	if (log_p) {
		for (int i = 0; i < StorageLogCurrent; i++) {
			if (!log_p[i].freed && log_p[i].storage_p == p) {
				log_p[i].freed = TRUE;
			}
		}
	}
}


EXPORT BOOL_T TestMallocs()
{
	size_t oldSize;
	size_t testedMallocs = 0;
	void* old;
	slog_p log_p = StorageLog;
	BOOL_T rc = TRUE;
	if (log_p) {
		for (int i = 0; i < StorageLogCurrent; i++) {
			if (log_p[i].freed) { continue; }
			old = log_p[i].storage_p;
			oldSize = log_p[i].storage_size;
			if (*(unsigned long*)((char*)old - sizeof(unsigned long)) != guard0) {
				LogPrintf("Guard 0 hosed, " SLOG_FMT " size: %llu \n", (uintptr_t)old, oldSize);
				rc = FALSE;
			}
			if (*(unsigned long*)((char*)old + oldSize) != guard1) {
				LogPrintf("Guard 1 hosed, " SLOG_FMT " size: %llu \n", (uintptr_t)old, oldSize);
				rc = FALSE;
			}
			testedMallocs++;
		}
	}
	LogPrintf("Tested: %llu Mallocs: %llu Total Malloced: %llu Freed: %llu Total Freed: %llu \n",
		testedMallocs, totalMallocs, totalMalloced, totalFrees, totalFreeed);
	return rc;
}

/**
 * Allocate memory
 *
 * Allocated memory has 'guard' values, before and after to detect overruns.
 * Aborts on allocation failure.
 *
 * \param size IN amount of memory to allocate
 *
 * \return Pointer to allocated memory - never NULL
 */

EXPORT void* MyMalloc(size_t size)
{
	void* p;
	totalMallocs++;
	totalMalloced += size;
	p = malloc((size_t)size + sizeof(size_t) + 2 * sizeof(unsigned long));
	if (p == NULL) {
		// We're hosed, get out of town
		lprintf("malloc(%ld) failed\n", size);
		abort();
	}

	LOG1(log_malloc,
		("  Malloc(%ld) = " SLOG_FMT " (" SLOG_FMT "-" SLOG_FMT ")\n",
			size, (size_t)((char*)p + sizeof(size_t) + sizeof(unsigned long)),
			(size_t)p,
			(size_t)((char*)p + size + sizeof(size_t) + 2 * sizeof(unsigned long))));

	*(size_t*)p = (size_t)size;
	p = (char*)p + sizeof(size_t);
	*(unsigned long*)p = guard0;
	p = (char*)p + sizeof(unsigned long);
	*(unsigned long*)((char*)p + size) = guard1;
	memset(p, 0, (size_t)size);
	if (extraButtons) {
		RecordMalloc(p, size);
	}
	return p;
}

/**
 * Reallocate memory
 *
 * Allocated memory has 'guard' values, before and after to detect overruns.
 * Aborts on allocation failure.
 *
 * \param old IN existing pointer to allocated memory
 * \param size IN amount of memory to allocate
 *
 * \return Pointer to reallocated memory - never NULL
 */

EXPORT void* MyRealloc(void* old, size_t size)
{
	size_t oldSize;
	void* new;
	if (old == NULL) {
		return MyMalloc(size);
	}
	totalReallocs++;
	totalRealloced += size;
	CHECKMSG((*(unsigned long*)((char*)old - sizeof(unsigned long)) == guard0),
		("Guard0 is hosed"));
	oldSize = *(size_t*)((char*)old - sizeof(unsigned long) - sizeof(size_t));
	CHECKMSG((*(unsigned long*)((char*)old + oldSize) == guard1),
		("Guard1 is hosed"));

	LOG1(log_malloc, ("  Realloc (" SLOG_FMT ",%ld) was %d\n", (size_t)old, size,
		oldSize))

		if ((long)oldSize == size) {
			return old;
		}
	new = MyMalloc(size);
	memcpy(new, old, min((size_t)size, oldSize));
	MyFree(old);
	return new;
}

EXPORT void MyFree(void* ptr)
{
	size_t oldSize;
	totalFrees++;
	if (ptr == NULL) {
		return;
	}
	CHECKMSG((*(unsigned long*)((char*)ptr - sizeof(unsigned long)) == guard0),
		("Guard0 is hosed"));
	oldSize = *(size_t*)((char*)ptr - sizeof(unsigned long)
		- sizeof(size_t));
	CHECKMSG((*(unsigned long*)((char*)ptr + oldSize) == guard1),
		("Guard1 is hosed"));

	LOG1(log_malloc,
		("  Free %ld at " SLOG_FMT " (" SLOG_FMT "-" SLOG_FMT ")\n",
			oldSize,
			(size_t)ptr,
			(size_t)((char*)ptr - sizeof * (size_t*)0 - sizeof * (long*)0),
			(size_t)((char*)ptr + oldSize + sizeof * (long*)0)));

	totalFreeed += oldSize;
	free((char*)ptr - sizeof * (long*)0 - sizeof * (size_t*)0);
	if (extraButtons) {
		RecordMyFree(ptr);
	}
}

EXPORT void* memdup(void* src, size_t size)
{
	void* p;
	p = MyMalloc(size);
	memcpy(p, src, size);
	return p;
}

EXPORT char* MyStrdup(const char* str)
{
	char* ret;
	ret = (char*)MyMalloc(strlen(str) + 1);
	strcpy(ret, str);
	return ret;
}

