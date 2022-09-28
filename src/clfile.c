/* Copyright (C) 2000-2004 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: clfile.c,v 1.5 2004/01/16 08:55:37 ghostgum Exp $ */

/* GFile is similar but not identical to MFC CFile, but is plain C. */
/* This implementation uses OS handles, and allows large files.
 * The large file implementation is for Linux and is not portable.
 * Use cfile.c if you need portability.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef __WIN32__
#include <unistd.h>
#include <linux/unistd.h>
#endif
#include "cfile.h"

int _llseek(unsigned int fd,  unsigned  long  offset_high,
   unsigned  long  offset_low,  loff_t  *result, unsigned int whence);
_syscall5(int,  _llseek,  uint,  fd, ulong, hi, ulong, lo,
    loff_t *, res, uint, wh);

/* These are the private bits */
struct GFile_s {
	int m_fd;
	time_t	m_filetime;		/* time/date of selected file */
	FILE_POS m_length;	/* length of selected file */
	int m_error;		/* non-zero if an error */
				/* error cleared by open, close or seek to 0 */
};

#ifndef ASSERT
#ifdef DEBUG
static void gfile_assert(const char *file, int line);
#define ASSERT(f) if (!(f)) gfile_assert(__FILE__, __LINE__)
#else
#define ASSERT(f)
#endif
#endif

#ifdef DEBUG
void gfile_assert(const char *file, int line)
{
   g_print("gfile_assert: file:%s line:%d\n", file, line);
}
#endif

int
gfile_error(GFile *gf)
{
    ASSERT(gf != NULL);
    ASSERT(gf->m_fd != 0);
    return gf->m_error;
}

FILE_POS gfile_get_length(GFile *gf)
{
    loff_t length = 0;
    loff_t result = 0;
    unsigned long offset_high;
    unsigned long offset_low;
    const int shift = sizeof(unsigned long) * 8;
    const unsigned long mask = (unsigned long)-1;
    /* Get the current position */
    _llseek(gf->m_fd, 0, 0,  &result, SEEK_CUR);
    if (sizeof(FILE_OFFSET) > sizeof(unsigned long)) {
        offset_high = (result >> shift) & mask;
	offset_low = result & mask;
    }
    else {
	offset_high = 0;
	offset_low = result;
    }
    /* Seek to the end to get the length */
    _llseek(gf->m_fd, 0, 0,  &length, SEEK_END);
    /* Seek back to where we were before */
    _llseek(gf->m_fd, offset_high, offset_low,  &result, SEEK_SET);
    return length;
}

int gfile_get_datetime(GFile *gf, unsigned long *pdt_low, 
    unsigned long *pdt_high)
{
    struct stat fstatus;
    ASSERT(gf != NULL);
    fstat(gf->m_fd, &fstatus);
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

GFile *gfile_open_handle(int hFile, unsigned int nOpenFlags)
{
    /* nOpenFlags ignored */
    GFile *gf = (GFile *)malloc(sizeof(GFile));
    if (gf == NULL)
	return NULL;
    memset(gf, 0, sizeof(GFile));
    gf->m_fd = hFile;
    gf->m_error = 0;
    return gf;
}

GFile *gfile_open(LPCTSTR lpszFileName, unsigned int nOpenFlags)
{
    GFile *gf;
    int fd;
    int flags = O_RDONLY;
    if ((nOpenFlags & 0xf) == gfile_modeWrite)
	flags = O_WRONLY;
    if ((nOpenFlags & 0xf000) == gfile_modeCreate)
	flags |= O_CREAT | O_TRUNC;
 
    if (lpszFileName[0] == '\0')
	fd = 1;	/* stdout */
    else
        fd = open(lpszFileName, flags,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (fd == -1)
	return NULL;

    gf = (GFile *)malloc(sizeof(GFile));
    if (gf == NULL) {
	close(fd);
	return NULL;
    }
    memset(gf, 0, sizeof(GFile));
    gf->m_fd = fd;
    gf->m_error = 0;
    return gf;
}

void gfile_close(GFile *gf)
{
    ASSERT(gf != NULL);
    ASSERT(gf->m_fd != -1);
    close(gf->m_fd);
    gf->m_fd = -1;
    gf->m_error = 0;
    free(gf);
}


unsigned int gfile_read(GFile *gf, void *lpBuf, unsigned int nCount)
{
    int count;
    ASSERT(gf != NULL);
    ASSERT(gf->m_fd != -1);
    count = read(gf->m_fd, lpBuf, nCount);
    if (count == -1)
	gf->m_error = 1;
    return (unsigned int)count;
}

unsigned int gfile_write(GFile *gf, const void *lpBuf, unsigned int nCount)
{
    int count;
    ASSERT(gf != NULL);
    ASSERT(gf->m_fd != -1);
    count = write(gf->m_fd, lpBuf, nCount);
    if (count == -1)
	gf->m_error = 1;
    return (unsigned int)count;
}

/* only works with reading */
int gfile_seek(GFile *gf, FILE_OFFSET lOff, unsigned int nFrom)
{
    int code;
    unsigned int origin;
    loff_t result = 0;
    unsigned long offset_high;
    unsigned long offset_low;
    const int shift = sizeof(unsigned long) * 8;
    const unsigned long mask = (unsigned long)-1;
    ASSERT(gf != NULL);
    ASSERT(gf->m_fd != -1);
    if (sizeof(FILE_OFFSET) > sizeof(unsigned long)) {
        offset_high = (lOff >> shift) & mask;
	offset_low = lOff & mask;
    }
    else {
	offset_high = 0;
	offset_low = lOff;
    }

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
    if ((origin == SEEK_SET) && (lOff == 0)) {
	gf->m_error = 0;
    }
    code = _llseek(gf->m_fd, offset_high, offset_low,  &result, origin);
    return code;
}

FILE_POS gfile_get_position(GFile *gf)
{
    int code;
    loff_t result;
    ASSERT(gf != NULL);
    ASSERT(gf->m_fd != 0);
    code = _llseek(gf->m_fd, 0, 0, &result, SEEK_CUR);
    return result;
}

int gfile_puts(GFile *gf, const char *str)
{
    return gfile_write(gf, str, strlen(str));
}
