/* Copyright (C) 2001-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: calloc.c,v 1.10 2005/06/10 09:39:24 ghostgum Exp $ */
/* A debugging malloc */

/* This is prettly clunky, but it allows us to check if there
 * are any memory leaks and to check for double free.
 * If you enable (debug & DEBUG_MALLOC), then you must do
 * so before any memory is allocated, otherwise it will
 * crash when it frees memory on exit.
 */

#include "common.h"

#ifdef DEBUG_MALLOC
FILE *malloc_file;
#ifdef UNIX
#define MALLOC_FILE "gsview.txt"
#else
#define MALLOC_FILE "c:\\gsview.txt"
#endif

/*
#define DEBUG_PRINT
*/

#undef malloc
#undef calloc
#undef realloc
#undef free
size_t allocated_memory = 0;


#define SIGNATURE 0xa55a
typedef struct DMEM_s {
    size_t size;	/* size of this chunk */
    int signature;	/* to make sure we aren't looking at garbage */
    char notes[120];	/* description of block */
} DMEM;

void * debug_malloc(size_t size, const char *file, int line)
{
    if (debug & DEBUG_MEM) {
	char buf[MAXSTR];
	DMEM *mem = (DMEM *)malloc(size+sizeof(DMEM));
	void *p = NULL;
	if (mem) {
	    memset(mem, 0, size+sizeof(DMEM));
	    mem->size = size;
	    mem->signature = SIGNATURE;
	    allocated_memory += size;
	    snprintf(mem->notes, sizeof(mem->notes), "%s:%d %ld", 
		file, line, (long)size);
	    p = ((char *)mem) + sizeof(DMEM);
	}
	snprintf(buf, sizeof(buf), 
	    "%s, 0x%08lx, malloc(%ld), allocated = %ld\n", 
	    mem->notes, (long)p, (long)size, (long)allocated_memory);
	buf[sizeof(buf)-1]='\0';
#ifdef DEBUG_PRINT
	fputs(buf, stderr);
#endif
#ifdef DEBUG_MALLOC
	if (malloc_file == (FILE *)NULL)
	    malloc_file = fopen(MALLOC_FILE, "w");
	if (malloc_file != (FILE *)NULL) {
	    fputs(buf, malloc_file);
	    fflush(malloc_file);
	}
#endif
	return p;
    }

    return malloc(size);
}


void * debug_realloc(void *block, size_t size)
{
    if (debug & DEBUG_MEM) {
	char buf[MAXSTR];
	char *p = NULL;
	size_t oldsize = 0;
	if (block == NULL) {
	    strncpy(buf, "debug_realloc: null pointer\n", sizeof(buf));
	}
	else {
	    DMEM *old = (DMEM *)(((char *)block)-sizeof(DMEM));
	    DMEM *mem;
	    char *p;
	    oldsize = old->size;
	    allocated_memory -= oldsize;
	    mem = realloc(old, size + sizeof(DMEM));
	    if (mem) {
		memset(mem, 0, size+sizeof(DMEM));
		mem->size = size;
		mem->signature = SIGNATURE;
		allocated_memory += size;
		p = ((char *)mem) + sizeof(DMEM);
	    }
	    snprintf(buf, sizeof(buf),
		"realloc old %ld, new %ld, allocated = %ld\n",
		(long)oldsize, (long)size, (long)allocated_memory);
	    buf[sizeof(buf)-1]='\0';
	}

#ifdef DEBUG_PRINT
	fputs(buf, stderr);
#endif
#ifdef DEBUG_MALLOC
	if (malloc_file == (FILE *)NULL)
	    malloc_file = fopen(MALLOC_FILE, "w");
	if (malloc_file != (FILE *)NULL) {
	    fputs(buf, malloc_file);
	    fflush(malloc_file);
	}
#endif
	return p;
    }

    return realloc(block, size);
}

void debug_free(void *block)
{
    if (debug & DEBUG_MEM) {
	char buf[MAXSTR];
	DMEM *mem = (DMEM *)(((char *)block)-sizeof(DMEM));
	if (block == NULL) {
	    strncpy(buf, "debug_free: null pointer\n", sizeof(buf));
	}
	else {
	    size_t oldsize = mem->size;
	    allocated_memory -= oldsize;
	    if (mem->signature != SIGNATURE)
	        snprintf(buf, sizeof(buf), 
		    "free 0x%08lx NOT VALID\n", (long)block);
	    else
	        snprintf(buf, sizeof(buf), 
		    "%s, 0x%08lx, free %ld, allocated = %ld\n",
		    mem->notes, (long)block, 
		    (long)oldsize, (long)allocated_memory);
	    buf[sizeof(buf)-1]='\0';
	    memset(mem, 0, oldsize + sizeof(DMEM));
	}
#ifdef DEBUG_PRINT
	fputs(buf, stderr);
#endif
#ifdef DEBUG_MALLOC
	if (malloc_file == (FILE *)NULL)
	    malloc_file = fopen(MALLOC_FILE, "w");
	if (malloc_file != (FILE *)NULL) {
	    fputs(buf, malloc_file);
	    fflush(malloc_file);
	}
#endif
	if (block != NULL)
	    free((void *)mem);
	return;
    }

    free(block);
    return;
}

void debug_memory_report(void)
{
    if (debug & DEBUG_MEM) {
	char buf[256];
	snprintf(buf, sizeof(buf), "Allocated memory = %ld bytes\n", 
	    (long)allocated_memory);
	buf[sizeof(buf)-1]='\0';
#ifdef DEBUG_PRINT
	fputs(buf, stderr);
#endif
#ifdef DEBUG_MALLOC
	fputs(buf, malloc_file);
	fclose(malloc_file);
#endif
    }
}
#endif /* DEBUG_MALLOC */
