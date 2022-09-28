/* Copyright (C) 2001-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cplat.h,v 1.17 2005/06/10 09:39:24 ghostgum Exp $ */
/* Platform specific includes and types */

/*
 * ZLIBNAME is name of zlib DLL/shared library
 * BZIP2NAME is name of bzip2 DLL/shared library
 * PATHSEP is a string containing the directory path separator.
 * INTPTR is an unsigned integer the same size as a pointer.
 * INTPTR_FORMAT is the printf format specifier used for INTPTR.
 * GGMODULE is a DLL/shared library handle.
 * GGMUTEX is mutual exclusion semaphore handle.
 * GGEVENT is an event semaphore handle.
 * GGTHREAD is a thread handle for the C run time library.
 */

#ifndef CPLAT_INCLUDED
#define CPLAT_INCLUDED

#if defined(_WIN32) || defined(_WIN64)
# ifndef _Windows
# define _Windows
# endif
#endif

#ifdef _WINDOWS
# ifndef _Windows
# define _Windows
# endif
#endif

#ifdef _Windows
# include <windows.h> /* must be included before stdio.h */
# include <direct.h>
# include <io.h>
# include "wgsver.h"
# define ZLIBNAME "zlib32.dll"
# define BZIP2NAME "libbz2.dll"
# define PATHSEP "\\"
# define EOLSTR "\r\n"
# ifdef _WIN64
#  define INTPTR __int64  /* an integer that same size as a pointer */
#  define INTPTR_FORMAT "%I64u"
# else
#  define INTPTR unsigned long
#  define INTPTR_FORMAT "%lu"
# endif
/* BYTE  is a predefined type in windows.h */
/* WORD  is a predefined type in windows.h */
/* DWORD is a predefined type in windows.h */
/* LONG  is a predefined type in windows.h */
/* TCHAR is a predefined type in windows.h */
/* LPTSTR is a predefined type in windows.h */
/* LPCTSTR is a predefined type in windows.h */
# define GGMODULE HMODULE
# define GGMUTEX HANDLE
# define GGEVENT HANDLE
# if _MSC_VER >= 1300
#  define GGTHREAD uintptr_t
# else
#  define GGTHREAD unsigned long
# endif
/* WINAPI already defined in windows.h */
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#endif

#ifdef OS2
#define INCL_DOS
# include <os2.h>
# include <io.h>
# define PATHSEP "\\"
# define EOLSTR "\r\n"
# define ZLIBNAME "zlib2.dll"  /* never used */
# define BZIP2NAME "bzip2.dll"  /* never used */
# define INTPTR unsigned long
# define INTPTR_FORMAT "%lu"
# define TCHAR char
# define LPTSTR TCHAR *
# define LPCTSTR const TCHAR *
# define GGMODULE HMODULE
# define GGMUTEX HMTX
# define GGEVENT HEVENT
# define GGTHREAD int
# define WINAPI _System
# define BI_RGB (0L)
# define BI_BITFIELDS (3L)
typedef unsigned short WORD; /* 16-bits */
typedef unsigned long DWORD; /* 32-bits (or longer)*/
typedef struct POINT_s {
    int x;
    int y;
} POINT;
#endif

#ifdef UNIX
# ifdef GTK
#  include <gtk/gtk.h>
# else
typedef struct _GdkRgbCmap GdkRgbCmap;
# endif
# include <unistd.h>
# define __USE_GNU	/* we might need recursive mutex */
# include <semaphore.h>
# include <pthread.h>
# define ZLIBNAME "libz.so"
# define BZIP2NAME "libbz2.so"
# define PATHSEP "/"
# define EOLSTR "\n"
# define INTPTR unsigned long
# define INTPTR_FORMAT "%lu"
# define TCHAR char
# define LPTSTR TCHAR *
# define LPCTSTR const TCHAR *
# define GGMODULE void *
# define GGMUTEX pthread_mutex_t
# define GGEVENT sem_t
# define GGTHREAD pthread_t
# define WINAPI 
# define stricmp strcasecmp
/* FIX: these shouldn't be here */
/* Windows IDs */
#define IDOK  1
#define IDCANCEL  2
#define IDYES 6
#define IDNO 7

#define MB_OK 		0x0
#define MB_OKCANCEL 	0x1
#define MB_YESNO 	0x4
#define MB_TYPEMASK	0xf
#define MB_ICONHAND	   0x10
#define MB_ICONQUESTION	   0x20
#define MB_ICONEXCLAMATION 0x30
#define MB_ICONASTERISK	   0x40

# define CW_USEDEFAULT (-32768)
# define BI_RGB (0L)
# define BI_BITFIELDS (3L)
typedef int BOOL;
typedef unsigned char BYTE; /* 8-bits */
typedef unsigned short WORD; /* 16-bits */
typedef unsigned long DWORD; /* 32-bits (or longer)*/
typedef long LONG; 	     /* signed 32-bits (or longer)*/
typedef struct POINT_s {
    int x;
    int y;
} POINT;
# ifndef FALSE
#  define FALSE (0)
# endif
# ifndef TRUE
#  define TRUE (!(FALSE))
# endif
# ifdef NO_SNPRINTF
int snprintf(char *buffer, size_t count, const char *fmt, ...);
int vsnprintf(char *buffer, size_t count, const char *fmt, va_list argptr);
# endif
#endif


/* Stuff for handling UNICODE */

#ifndef TEXT
# define TEXT(s) s	/* for Unicode strings */
#endif

/* Functions prefixed by "cs" take a character string
 * as an argument that may be Unicode or narrow
 * depending on the platform.
 */
#if defined(UNICODE) && defined(_Windows)
# define csncpy(s,t,n) wcsncpy(s,t,n)
# define csncat(s,t,n) wcsncat(s,t,n)
# define cscmp(s,t) wcscmp(s,t)
# define csicmp(s,t) wcsicmp(s,t)
# define csncmp(s,t,n) wcsncmp(s,t,n)
# define csrchr(s,c) wcsrchr(s,c)
# define cslen(s) wcslen(s)
# define csnprintf _snwprintf
# define csvnprintf _vsnwprintf
# define cstoi _wtoi
# define csfopen(s,m) _wfopen(s, m)
# define csunlink(s) _wunlink(s)
# define csgetcwd(s, n) _wgetcwd(s, n)
# define csmktemp(s) _wmktemp(s)
# define csgetenv(s) _wgetenv(s)
#else
# define csncpy(s,t,n) strncpy(s,t,n)
# define csncat(s,t,n) strncat(s,t,n)
# define cscmp(s,t) strcmp(s,t)
# define csicmp(s,t) stricmp(s,t)
# define csncmp(s,t,n) strncmp(s,t,n)
# define csrchr(s,c) strrchr(s,c)
# define cslen(s) strlen(s)
# define csnprintf snprintf
# define csvnprintf vsnprintf
# define cstoi atoi
# define csfopen(s,m) fopen(s, m)
# define csunlink(s) unlink(s)
# define csgetcwd(s, n) getcwd(s, n)
# define csmktemp(s) mktemp(s)
# define csgetenv(s) getenv(s)
#endif

#ifdef DO_NOT_USE_THESE
#define strcpy DO_NOT_USE_strcpy
#define strcat DO_NOT_USE_strcat
#define sprintf DO_NOT_USE_sprintf
#endif

#endif /* CPLAT_INCLUDED */

