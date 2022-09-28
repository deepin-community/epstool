/* Copyright (C) 2001-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: common.h,v 1.15 2005/06/10 09:39:24 ghostgum Exp $ */
/* Common header */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "cplat.h"
#include "cfile.h"

#define __PROTOTYPES__

#define MAXSTR 256
#define COPY_BUF_SIZE 4096

#define GS_REVISION_MIN 704
#define GS_REVISION     704
#define GS_REVISION_MAX 899

#define return_error(val) return val

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

/* Opaque types */

#ifndef CDSC_TYPEDEF
#define CDSC_TYPEDEF
typedef struct CDSC_s CDSC;
#endif

#ifndef Doc_TYPEDEF
#define Doc_TYPEDEF
typedef struct Doc_s Doc;
#endif

#ifndef GSDLL_TYPEDEF
#define GSDLL_TYPEDEF
typedef struct GSDLL_s GSDLL;
#endif

#ifndef PLDLL_TYPEDEF
#define PLDLL_TYPEDEF
typedef struct PLDLL_s PLDLL;
#endif

#ifndef GSIMAGE_TYPEDEF
#define GSIMAGE_TYPEDEF
typedef struct GSIMAGE_s GSIMAGE;
#endif

#ifndef GSREQ_TYPEDEF
#define GSREQ_TYPEDEF
typedef struct GSREQ_s GSREQ;
#endif

#ifndef GSSRV_TYPEDEF
#define GSSRV_TYPEDEF
typedef struct GSSRV_s GSSRV;
#endif

#ifndef GSview_TYPEDEF
#define GSview_TYPEDEF
typedef struct GSview_s GSview;
#endif

#ifndef HISTORY_TYPEDEF
#define HISTORY_TYPEDEF
typedef struct HISTORY_s HISTORY;
#endif

#ifndef IMAGE_TYPEDEF
#define IMAGE_TYPEDEF
typedef struct IMAGE_s IMAGE;
#endif

#ifndef MEDIA_TYPEDEF
#define MEDIA_TYPEDEF
typedef struct MEDIA_s MEDIA;
#endif

#ifndef OPTION_TYPEDEF
#define OPTION_TYPEDEF
typedef struct OPTION_s OPTION;
#endif

#ifndef PAGECACHE_TYPEDEF
#define PAGECACHE_TYPEDEF
typedef struct PAGECACHE_s PAGECACHE;
#endif

#ifndef PDFLINK_TYPEDEF
#define PDFLINK_TYPEDEF
typedef struct PDFLINK_s PDFLINK;
#endif

#ifndef View_TYPEDEF
#define View_TYPEDEF
typedef struct View_s View;
#endif


/* Public functions */

/***************************************/
/* Unicode, Multiple Byte Character Set and narrow strings. */

#ifdef UNICODE
#define CHARNEXT(x) ((x)+1)
#else
#define CHARNEXT(x) (global_codepage == CODEPAGE_SBCS ? ((x)+1) : ((x)+char_next(x)))

typedef enum {
    CODEPAGE_SBCS = 0,	/* single byte character set */
    CODEPAGE_UTF8 = 1,
    CODEPAGE_EUC = 2,
    CODEPAGE_SJIS = 3
} CODEPAGE;

extern CODEPAGE global_codepage; 	/* GLOBAL */
int char_next(const char *str);
#endif

/* convert between cs (wide or multibyte) and narrow strings */
int cs_to_narrow(char *nstr, int nlen, LPCTSTR wstr, int wlen);
int narrow_to_cs(TCHAR *wstr, int wlen, const char *nstr, int nlen);

/* Implementation that just copies the string */
int char_to_narrow(char *nstr, int nlen, LPCTSTR wstr, int wlen);
int narrow_to_char(TCHAR *wstr, int wlen, const char *nstr, int nlen);

/* Convert ISO-Latin1 to UTF-8 */
/* Used where unknown text is passed to gtk+ GUI */
int latin1_to_utf8(char *ustr, int ulen, const char *str, int slen);

/***************************************/
/* Debugging flags */

#define DEBUG_GENERAL	0x01
#define DEBUG_GDI	0x02	/* GDI printing */
#define DEBUG_MEM	0x04	/* memory allocation */
#define DEBUG_LOG	0x08	/* write gs_addmess() to file c:\gsview.txt */
#define DEBUG_GSINPUT	0x10	/* log all input written to GS */
#define DEBUG_DEV	0x80	/* For debugging */

extern int debug;			/* GLOBAL */

/***************************************/
/* Debugging malloc */
/*
#define DEBUG_MALLOC
*/
#ifdef DEBUG_MALLOC
# define malloc(x) debug_malloc(x,__FILE__,__LINE__)
# define calloc debug_calloc
# define realloc debug_realloc
# define free debug_free
void * debug_malloc(size_t size, const char *file, int line);
void * debug_realloc(void *block, size_t size);
void debug_free(void *block);
void debug_memory_report(void);
#endif


/***************************************/

/* should put this in cpagelst.h */

typedef struct PAGELIST_s {
    int current;	/* index of current selection */
    BOOL multiple;	/* true if multiple selection allowed */
    int page_count;	/* number of entries in select */
    BOOL *select;	/* array of selection flags */
    BOOL reverse;	/* reverse pages when extracting or printing */
} PAGELIST;

void page_list_free(PAGELIST *page_list);
void page_list_copy(PAGELIST *newlist, PAGELIST *oldlist);
void page_list_init(PAGELIST *page_list, int pages);


/***************************************/

typedef enum ORIENT_e {
    ORIENT_DEFAULT = -1,	/* use PDF/DSC default */
    /* following values match setpagedevice /Orientation */
    ORIENT_PORTRAIT = 0,
    ORIENT_LANDSCAPE = 3,
    ORIENT_UPSIDEDOWN = 2,
    ORIENT_SEASCAPE = 1
} ORIENT;


typedef enum DISPLAY_FORMAT_e {
    DISPLAY_FORMAT_AUTO = 0,
    DISPLAY_FORMAT_GREY_1 = 1,
    DISPLAY_FORMAT_GREY_8 = 2,
    DISPLAY_FORMAT_COLOUR_4 = 3,
    DISPLAY_FORMAT_COLOUR_8 = 4,
    DISPLAY_FORMAT_COLOUR_24 = 5,
    DISPLAY_FORMAT_CMYK_32 = 6,
    DISPLAY_FORMAT_COUNT = 7
} DISPLAY_FORMAT;

/***************************************/
/* PAGESPEC is used in rendering requests, and to describe
 * the results of a render.
 */
typedef struct PAGESPEC_s PAGESPEC;

typedef enum PAGESIZE_METHOD_e {
    PAGESIZE_GIVEN = 0,		/* use llx,lly,urx,ury */
    PAGESIZE_CROPBOX = 1,	/* use PDF crop box */
    PAGESIZE_MEDIABOX = 2,	/* use PDF media box */
    PAGESIZE_VARIABLE = 3	/* default is llx,lly,urx,ury, but allow */
				/* user PostScript to change page size */
} PAGESIZE_METHOD;

struct PAGESPEC_s {
    TCHAR filename[MAXSTR];
    int page;			/* page number */
    PAGESIZE_METHOD pagesize;
    /* If pagesize is not PAGESIZE_GIVEN, then the following dimensions
     * do not need to be specified on request, but will be filled
     * in just before rendering.  This is because we don't know th
     * PDF page size until we start rendering.
     */
    float llx;	/* Points */
    float lly;	/* Points */
    float urx;	/* Points */
    float ury;	/* Points */

    float xddpi;	/* display dpi */
    float yddpi; 	/* display dpi */
    float xrdpi;	/* render dpi */
    float yrdpi; 	/* render dpi */
    /* On request, set both req_orient and orient to the desired orientation
     * After rendering, orient will be set to the actual orientation,
     * while orient_request may be left as ORIENT_DEFAULT.
     * This is needed because we don't know the actual PDF orientation 
     * until we start rendering.
     */
    ORIENT orient_request;
    ORIENT orient;
    unsigned int format;	/* display device format */
    int alpha_text;		/* 1, 2 or 4 */
    int alpha_graphics;		/* 1, 2 or 4 */
};


