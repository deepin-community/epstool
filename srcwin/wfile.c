/* Copyright (C) 2000-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/


/* $Id: wfile.c,v 1.1.2.9 2005/06/10 09:39:24 ghostgum Exp $ */

/* GFile is similar but not identical to MFC CFile. */
/* This implementation uses Windows APIs */

#define STRICT
#include <windows.h>
#include <stdio.h>

#include "cfile.h"

/* These are the private bits */
struct GFile_s {
#ifdef MEMORYFILE
	/* Read from a memory buffer */
	const char *m_pBase;
	long m_nOffset;
	long m_nLen;
#else
#ifdef _Windows
	/* Windows API */
	void *m_hFile;
	int m_error;
	int m_openflags;
#else
	FILE *m_file;
#endif /* !_Windows */
#endif /* !MEMORYFILE */
};


#ifndef ASSERT
#ifdef DEBUG
static void gfile_assert(const char *file, int len);
#define ASSERT(f) if (!(f)) gfile_assert(__FILE__, __LINE__)
static void gfile_assert(const char *file, int line) 
{
  printf("Assert failed in file %s at line %d\n", file, line);
}
#else
#define ASSERT(f)
#endif
#endif

int
gfile_error(GFile *gf)
{
    ASSERT(gf != NULL);
    ASSERT(gf->m_hFile != INVALID_HANDLE_VALUE);
    return gf->m_error;
}

FILE_POS gfile_get_length(GFile *gf)
{
    BY_HANDLE_FILE_INFORMATION fi;
    ASSERT(gf != NULL);
    GetFileInformationByHandle((HANDLE)gf->m_hFile, &fi);
/* FIX */
    return (FILE_POS)
	(((unsigned __int64 )fi.nFileSizeHigh << 32) + fi.nFileSizeLow);
/*
    return fi.nFileSizeLow;
*/
}

BOOL gfile_get_datetime(GFile *gf, unsigned long *pdt_low, 
    unsigned long *pdt_high)
{
    FILETIME datetime;
    BOOL flag;
    ASSERT(gf != NULL);
    flag = GetFileTime((HANDLE)gf->m_hFile, NULL, NULL, &datetime);
    *pdt_low = datetime.dwLowDateTime;
    *pdt_high = datetime.dwHighDateTime;
    return flag;
}

BOOL gfile_changed(GFile *gf, FILE_POS length, unsigned long dt_low,
    unsigned long dt_high)
{
    unsigned long this_dt_low, this_dt_high;
    FILE_POS this_length = gfile_get_length(gf);
    gfile_get_datetime(gf, &this_dt_low, &this_dt_high);
    return ( (this_length != length) ||
	(this_dt_low != dt_low) || (this_dt_high != dt_high));
}


GFile *gfile_open_handle(void *hFile, unsigned int nOpenFlags)
{
    GFile *gf = (GFile *)malloc(sizeof(GFile));
    if (gf == NULL) {
	CloseHandle((HANDLE)hFile);
	return NULL;
    }
    memset(gf, 0, sizeof(GFile));
    gf->m_hFile = hFile;
    gf->m_error = 0;
    gf->m_openflags = nOpenFlags;
    return gf;
}

GFile *gfile_open(LPCTSTR lpszFileName, unsigned int nOpenFlags)
{
    GFile *gf;
    DWORD dwAccess = GENERIC_READ;
    DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
    DWORD dwCreate = OPEN_EXISTING;
    HANDLE hFile;
    ASSERT(nOpenFlags == GENERIC_READ);
    if ((nOpenFlags & 0xf) == gfile_modeRead)
	dwAccess = GENERIC_READ;
    if ((nOpenFlags & 0xf) == gfile_modeWrite)
	dwAccess = GENERIC_WRITE;
    if ((nOpenFlags & 0xf0) == gfile_shareDenyWrite)
	dwShareMode = FILE_SHARE_READ;
    if ((nOpenFlags & 0xf0) == gfile_shareExclusive)
	dwShareMode = 0;
    if ((nOpenFlags & 0xf000) == gfile_modeCreate)
	dwCreate = CREATE_ALWAYS;

    if (lpszFileName[0] == '\0')
	hFile = GetStdHandle(STD_OUTPUT_HANDLE);
    else
	hFile = CreateFile(lpszFileName, dwAccess,
	    dwShareMode, NULL, dwCreate, FILE_ATTRIBUTE_NORMAL,
	    NULL);
    if (hFile == INVALID_HANDLE_VALUE)
	return NULL;
    gf = (GFile *)malloc(sizeof(GFile));
    if (gf == NULL) {
	CloseHandle(hFile);
	return NULL;
    }
    memset(gf, 0, sizeof(GFile));
    gf->m_hFile = hFile;
    gf->m_error = 0;
    gf->m_openflags = nOpenFlags;
    return gf;
}

void gfile_close(GFile *gf)
{
    ASSERT(gf != NULL);
    ASSERT(gf->m_hFile != 0);
    CloseHandle((HANDLE)gf->m_hFile);
    gf->m_hFile = 0;
    gf->m_error = 0;
    free(gf);
}


UINT gfile_read(GFile *gf, void *lpBuf, UINT nCount)
{
    DWORD nBytesRead;
    ASSERT(gf != NULL);
    ASSERT(gf->m_hFile != 0);
    if (ReadFile((HANDLE)gf->m_hFile, lpBuf, nCount, &nBytesRead, NULL))
	return nBytesRead;
    else
	gf->m_error = 1;	/* file error */
    return 0;
}

UINT gfile_write(GFile *gf, void *lpBuf, UINT nCount)
{
    DWORD nBytesWritten;
    ASSERT(gf != NULL);
    ASSERT(gf->m_hFile != 0);
    if (WriteFile((HANDLE)gf->m_hFile, lpBuf, nCount, &nBytesWritten, NULL))
	return nBytesWritten;
    else
	gf->m_error = 1;	/* file error */
    return 0;
}

int gfile_seek(GFile *gf, FILE_OFFSET lOff, unsigned int nFrom)
{
    DWORD dwMoveMethod;
    LONG lHiOff = (LONG)((unsigned __int64)lOff >> 32);
    ASSERT(gf != NULL);
    ASSERT(gf->m_hFile != 0);
    switch(nFrom) {
	default:
	case gfile_begin:
	    dwMoveMethod = FILE_BEGIN;
	    break;
	case gfile_current:
	    dwMoveMethod = FILE_CURRENT;
	    break;
	case gfile_end:
	    dwMoveMethod = FILE_END;
	    break;
    }
    /* return value on error is 0xffffffff */
    return (SetFilePointer((HANDLE)gf->m_hFile, 
	(LONG)lOff, &lHiOff, dwMoveMethod) == 0xffffffff);
}

FILE_POS gfile_get_position(GFile *gf)
{
    LONG lHiOff = 0;
    LONG lLoOff;
    ASSERT(gf != NULL);
    ASSERT(gf->m_hFile != 0);
    lLoOff = SetFilePointer((HANDLE)gf->m_hFile, 0, &lHiOff, FILE_CURRENT); 
    return (FILE_POS)(((unsigned __int64)lHiOff << 32) + lLoOff);
}

int gfile_puts(GFile *gf, const char *str)
{
    return gfile_write(gf, str, (int)strlen(str));
}
