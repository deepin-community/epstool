/* Copyright (C) 2000-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cfile.c,v 1.5 2005/06/10 09:39:24 ghostgum Exp $ */

/* GFile is similar but not identical to MFC CFile, but is plain C. */
/* This implementation uses C file streams */


#ifdef __WIN32__
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef __WIN32__
#include <unistd.h>
#endif
#include "cfile.h"

/* These are the private bits */
struct GFile_s {
	FILE *m_file;
	time_t	m_filetime;		/* time/date of selected file */
	FILE_POS m_length;	/* length of selected file */
};

#ifndef ASSERT
#ifdef DEBUG
static void gfile_assert(const char *file, int len);
#define ASSERT(f) if (!(f)) gfile_assert(__FILE__, __LINE__)
#else
#define ASSERT(f)
#endif
#endif


int
gfile_error(GFile *gf)
{
    ASSERT(gf != NULL);
    ASSERT(gf->m_file != 0);
    return ferror(gf->m_file);
}

FILE_POS gfile_get_length(GFile *gf)
{
    struct stat fstatus;
    fstat(fileno(gf->m_file), &fstatus);
    return fstatus.st_size;
}

int gfile_get_datetime(GFile *gf, unsigned long *pdt_low, 
    unsigned long *pdt_high)
{
    struct stat fstatus;
    ASSERT(gf != NULL);
    fstat(fileno(gf->m_file), &fstatus);
    *pdt_low = fstatus.st_mtime;
    *pdt_high = 0;
    return 1;
}

int gfile_changed(GFile *gf, FILE_POS length, 
    unsigned long dt_low, unsigned long dt_high)
{
    unsigned long this_dt_low, this_dt_high;
    FILE_POS this_length = gfile_get_length(gf);
    gfile_get_datetime(gf, &this_dt_low, &this_dt_high);
    return ( (this_length != length) ||
	(this_dt_low != dt_low) || (this_dt_high != dt_high));
}

GFile *gfile_open_handle(void *hFile, unsigned int nOpenFlags)
{
    GFile *gf;
    const char *access = "rb";
    if ((nOpenFlags & 0xf) == gfile_modeWrite)
	access = "wb";
    gf = (GFile *)malloc(sizeof(GFile));
    if (gf == NULL)
	return NULL;
    memset(gf, 0, sizeof(GFile));
    gf->m_file = fdopen((long)hFile, access);
    if (gf->m_file == NULL) {
	free(gf);
	gf = NULL;
    }
    return gf;
}

GFile *gfile_open(LPCTSTR lpszFileName, unsigned int nOpenFlags)
{
    GFile *gf;
    FILE *f;
    const char *access = "rb";
    if ((nOpenFlags & 0xf) == gfile_modeWrite)
	access = "wb";
 
    if (lpszFileName[0] == '\0')
	f = stdout;
    else
	f = fopen(lpszFileName, access);
    if (f == (FILE *)NULL)
	return NULL;

    gf = (GFile *)malloc(sizeof(GFile));
    if (gf == NULL) {
	fclose(f);
	return NULL;
    }
    memset(gf, 0, sizeof(GFile));
    gf->m_file = f;
    return gf;
}

void gfile_close(GFile *gf)
{
    ASSERT(gf != NULL);
    ASSERT(gf->m_file != 0);
    fclose(gf->m_file);
    gf->m_file = NULL;
    free(gf);
}


unsigned int gfile_read(GFile *gf, void *lpBuf, unsigned int nCount)
{
    ASSERT(gf != NULL);
    ASSERT(gf->m_file != 0);
    return fread(lpBuf, 1, nCount, gf->m_file);
}

unsigned int gfile_write(GFile *gf, const void *lpBuf, unsigned int nCount)
{
    ASSERT(gf != NULL);
    ASSERT(gf->m_file != 0);
    return fwrite(lpBuf, 1, nCount, gf->m_file);
}

/* only works with reading */
int gfile_seek(GFile *gf, FILE_OFFSET lOff, unsigned int nFrom)
{
    int origin;
    ASSERT(gf != NULL);
    ASSERT(gf->m_file != 0);

    switch(nFrom) {
	default:
	case gfile_begin:
	    origin = SEEK_SET;
	    break;
	case gfile_current:
	    origin = SEEK_CUR;
	    break;
	case gfile_end:
	    origin = SEEK_END;
	    break;
    }
    if ((origin == SEEK_SET) && (lOff == 0))
	rewind(gf->m_file);
    return fseek(gf->m_file, lOff, origin);
}

FILE_POS gfile_get_position(GFile *gf)
{
    ASSERT(gf != NULL);
    ASSERT(gf->m_file != 0);
    return ftell(gf->m_file);
}

int gfile_puts(GFile *gf, const char *str)
{
    return gfile_write(gf, str, strlen(str));
}
