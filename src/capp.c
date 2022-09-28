/* Copyright (C) 2001-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: capp.c,v 1.20 2005/06/10 09:39:24 ghostgum Exp $ */
/* Application */

#include "common.h"
#include "dscparse.h"
#define DEFINE_COPT
#include "copt.h"
#define DEFINE_CAPP
#include "capp.h"
#include "cargs.h"
#include "cdll.h"
#include "cgssrv.h"
#include "cimg.h"
#include "cpagec.h"
#include "cprofile.h"
#include "cres.h"

/* GLOBAL WARNING */
int debug = DEBUG_GENERAL;
const char szAppName[] = "GSview";
/* GLOBAL WARNING */

static void rotate_last_files(GSview *a, int count);

GSDLL **
app_dll(GSview *a)
{
    /* Because Ghostscript only supports one instance, we store
     * the DLL in the app.
     */
    return &a->gsdll;
}

PLDLL **
app_pldll(GSview *a)
{
    /* Because GhostPCL only supports one instance, we store
     * the DLL in the app.
     */
    return &a->pldll;
}

Doc **
app_docs(GSview *a)
{
    return &a->doclist;
}

OPTION *
app_option(GSview *a)
{
    return &a->option;
}

PAGECACHE **
app_pagecache(GSview *a)
{
    return &a->pagecache;
}

BOOL
app_multithread(GSview *a)
{
    return a->multithread;
}

int 
app_gssrv_request(GSview *a, GSREQ *reqnew)
{
    if (a->gssrv)
        return gssrv_request(a->gssrv, reqnew);
    return -1;
}

int 
app_plsrv_request(GSview *a, GSREQ *reqnew)
{
    if (a->plsrv)
        return gssrv_request(a->plsrv, reqnew);
    return -1;
}

/* we call this when we know that the app mutex is already locked by us */
int
app_msg_len_nolock(GSview *a, const char *str, int len)
{
    char *p;
    const char *s;
    int i, lfcount;

    /* if debugging, write to a log file */
    if (debug & DEBUG_LOG)
	app_log(str, len);

    /* we need to add \r after each \n, so count the \n's */
    lfcount = 0;
    s = str;
    for (i=0; i<len; i++) {
	if (*s == '\n')
	    lfcount++;
	s++;
    }
    if (len + lfcount >= TWSCROLL)
	return 0;	/* too large */
    if (len + lfcount + a->twend >= TWLENGTH-1) {
	/* scroll buffer */
	a->twend -= TWSCROLL;
	memmove(a->twbuf, a->twbuf+TWSCROLL, a->twend);
    }
    p = a->twbuf+a->twend;
    for (i=0; i<len; i++) {
	if (*str == '\n') {
	    *p++ = '\r';
	}
	if (*str == '\0') {
	    *p++ = ' ';	/* ignore null characters */
	    str++;
	}
	else
	    *p++ = *str++;
    }
    a->twend += (len + lfcount);
    *(a->twbuf+a->twend) = '\0';
    return len;
}

/* Add string for Ghostscript message window */
int
app_msg_len(GSview *a, const char *str, int len)
{
    int n;
    app_lock(a);
    n = app_msg_len_nolock(a, str, len);
    app_unlock(a);
    return n;
}

int
app_msg(GSview *a, const char *str)
{
    return app_msg_len(a, str, (int)strlen(str));
}

int
app_msgf(GSview *a, const char *fmt, ...)
{
va_list args;
int count;
char buf[2048];
    va_start(args,fmt);
    count = vsnprintf(buf,sizeof(buf),fmt,args);
    buf[sizeof(buf)-1]='\0';
    if (count >= (int)sizeof(buf)-1) {
	debug |= DEBUG_LOG;
	app_msg(a, "PANIC: internal buffer overflow.  Stack is corrupted\n");
    }
    app_msg(a, buf);
    va_end(args);
    return count;
}

int
app_csmsg_len_nolock(GSview *a, LPCTSTR wstr, int wlen)
{
    char buf[MAXSTR];
    char *nstr = NULL;
    int nlen = cs_to_narrow(NULL, 0, wstr, wlen);
    if (nlen < (int)sizeof(buf))
	nstr = buf;
    else
	nstr = (char *)malloc(nlen); 
    if (nstr) {
	cs_to_narrow(nstr, nlen, wstr, wlen);
	app_msg_len_nolock(a, nstr, nlen);
	if (nstr != buf)
	    free(nstr);
    }
    return wlen;
}

int
app_csmsg_len(GSview *a, LPCTSTR wstr, int wlen)
{
    int n;
    app_lock(a);
    n = app_csmsg_len_nolock(a, wstr, wlen);
    app_unlock(a);
    return n;
}


int
app_csmsg(GSview *a, LPCTSTR wstr)
{
    return app_csmsg_len(a, wstr, (int)cslen(wstr));
}

int
app_csmsgf(GSview *a, LPCTSTR fmt, ...)
{
va_list args;
int count;
TCHAR buf[2048];
    va_start(args,fmt);
    count = csvnprintf(buf,sizeof(buf)/sizeof(TCHAR),fmt,args);
    if (count >= (int)sizeof(buf)/(int)sizeof(TCHAR)-1) {
	debug |= DEBUG_LOG;
	app_msg(a, "PANIC: internal buffer overflow.  Stack is corrupted\n");
    }
    app_csmsg(a, buf);
    va_end(args);
    return count;
}

#ifdef NO_SNPRINTF
/* Dangerous code here - no length checking */
int
snprintf(char *buffer, size_t count, const char *fmt, ...)
{
va_list args;
int len;
    va_start(args,fmt);
    len = vsprintf(buffer,fmt,args);
    buffer[count-1]='\0';
    if (len >= (int)count) {
	debug |= DEBUG_LOG;
	printf("PANIC: internal buffer overflow.  Stack is corrupted\n");
    }
    va_end(args);
    return len;
}

int
vsnprintf(char *buffer, size_t count, const char *fmt, va_list argptr)
{
    int len = vsprintf(buffer, fmt, argptr);
    if (len >= (int)count) {
	debug |= DEBUG_LOG;
	printf("PANIC: internal buffer overflow.  Stack is corrupted\n");
    }
    return len;
}
#endif

static void
rotate_last_files(GSview *a, int count)
{
int i;
TCHAR buf[MAXSTR];
    if (count >= MAX_LAST_FILES)
	return;
    csncpy(buf, a->last_files[count], MAXSTR-1);
    for (i=count; i>0; i--)
	csncpy(a->last_files[i], a->last_files[i-1], MAXSTR-1);
    csncpy(a->last_files[0], buf, MAXSTR-1);
}

void
app_update_last_files(GSview *a, LPCTSTR filename)
{
int i;
    for (i=0; i<a->last_files_count; i++) {
	if (cscmp(filename, a->last_files[i]) == 0)
	    break;
    }
    if (i < a->last_files_count) {
	/* already in list */
	rotate_last_files(a, i);
	return;
    }
    if (a->last_files_count < MAX_LAST_FILES)
        a->last_files_count++;
    rotate_last_files(a, a->last_files_count-1);
    csncpy(a->last_files[0], filename, MAXSTR-1);
}

LPCTSTR
app_last_files(GSview *a, int i)
{
    if (i < a->last_files_count)
	return a->last_files[i];
    return NULL;
}

/* Read last files from INI file */
int
app_read_last_files(GSview *a, LPCTSTR filename)
{
    int i;
    char buf[MAXSTR];
    char lastfile[MAXSTR];
    const char *section = INISECTION;
    PROFILE *prf;
    prf = profile_open(filename);
    if (prf == NULL)
	return_error(-1);

    a->last_files_count = 0;
    memset(lastfile, 0, sizeof(lastfile));
    for (i=0; i<MAX_LAST_FILES; i++) {
	snprintf(buf, sizeof(buf), "LastFile%d", i);
	buf[sizeof(buf)-1]='\0';
	profile_read_string(prf, section, buf, "", 
	    lastfile, sizeof(lastfile));
	narrow_to_cs(a->last_files[i], sizeof(a->last_files[i])/sizeof(TCHAR)-1,
	    lastfile, (int)strlen(lastfile)+1);
	if (a->last_files[i][0])
	    a->last_files_count++;
    }
    profile_close(prf);
    return 0;
}

int
app_write_last_files(GSview *a, LPCTSTR filename)
{
    int i;
    char buf[MAXSTR];
    char lastfile[MAXSTR];
    const char *section = INISECTION;
    PROFILE *prf;
    prf = profile_open(filename);
    if (prf == NULL)
	return_error(-1);

    a->last_files_count = 0;
    memset(lastfile, 0, sizeof(lastfile));
    for (i=0; i<MAX_LAST_FILES; i++) {
	snprintf(buf, sizeof(buf), "LastFile%d", i);
        buf[sizeof(buf)-1]='\0';
	cs_to_narrow(lastfile, sizeof(lastfile)-1,
	    a->last_files[i], (int)cslen(a->last_files[i])+1);
	profile_write_string(prf, section, buf, lastfile);
    }
    profile_close(prf);
    return 0;
}

/* Platform independent processing of arguments, before window creation */
void
app_use_args(GSview *app, GSVIEW_ARGS *args)
{
    OPTION *opt = app_option(app);
    if (args->geometry) {
	if (args->geometry == 4) {
	    opt->img_origin.x = args->geometry_xoffset;
	    opt->img_origin.y = args->geometry_yoffset;
	}
	if (args->geometry >= 2) {
	    opt->img_size.x = args->geometry_width;
	    opt->img_size.y = args->geometry_height;
	}
    }
}


static int
app_init(GSview *a)
{
    app_platform_init(a);
    return 0;
}

static int
app_finish(GSview *a)
{
    pagecache_unref_all(a);
    zlib_free(a);
    bzip2_free(a);
    app_platform_finish(a);
    return 0;
}


GSview *
app_new(void *handle, BOOL multithread)
{
    GSview *a = (GSview *)malloc(sizeof(GSview));
    if (a) {
	memset(a, 0, sizeof(GSview));
	a->handle = handle;
	a->multithread = multithread;
	app_init(a);
	app_ref(a);	/* caller has reference to app */
    }
    return a;
}

/* Increment reference count of App */
/* A Document will increment refcount when it attaches to App */
/* Assumes we own the lock */
int
app_ref(GSview *a)
{
    int refcount = ++(a->refcount);
    if (debug & DEBUG_DEV) {
	char buf[MAXSTR];
	snprintf(buf, sizeof(buf), "app refcount=%d\n", refcount);
	app_msg_len_nolock(a, buf, (int)strlen(buf));
    }
    return refcount;
}

/* Release reference to App. */
/* When reference count reaches zero, App is freed. */
/* Assumes we own the lock */
int
app_unref(GSview *a)
{
    int refcount;
    if (a->refcount > 0)
	a->refcount--;
    refcount = a->refcount;
    if (debug & DEBUG_DEV) {
	char buf[MAXSTR];
	snprintf(buf, sizeof(buf), "app refcount=%d\n", refcount);
	buf[sizeof(buf)-1]='\0';
	app_msg_len_nolock(a, buf, (int)strlen(buf));
    }
    if (refcount == 0) {
	app_finish(a);
	memset(a, 0, sizeof(GSview));
	free(a);
    }

    return refcount;
}



int 
zlib_load(GSview *a)
{
    int code = 0;
    TCHAR buf[1024];
    ZLIB *zlib = &a->zlib;
    if (zlib->loaded)
	return 0;

    memset(buf, 0, sizeof(buf));
    code = dll_open(&zlib->hmodule, TEXT(ZLIBNAME), 
	buf, sizeof(buf)/sizeof(TCHAR)-1);
    if (code != 0) {
	app_csmsg(a, buf);
    }
    else {
	if (code == 0)
	    zlib->gzopen = (PFN_gzopen)dll_sym(&zlib->hmodule, "gzopen");
	if (zlib->gzopen == NULL) {
	    app_msg(a, "Can't find gzopen\n");
	    code = -1;
	}
	if (code == 0)
	    zlib->gzread = (PFN_gzread)dll_sym(&zlib->hmodule, "gzread");
	if (zlib->gzread == NULL) {
	    app_msg(a, "Can't find gzread\n");
	    code = -1;
	}
	if (code == 0)
	    zlib->gzclose = (PFN_gzclose)dll_sym(&zlib->hmodule, "gzclose");
	if (zlib->gzclose == NULL) {
	    app_msg(a, "Can't find gzclose\n");
	    code = -1;
	}
	if (code == 0)
	    zlib->loaded = TRUE;
	else {
	    dll_close(&zlib->hmodule);
	    memset(zlib, 0, sizeof(ZLIB));
	    zlib->loaded = FALSE;
	}
    }
    return code;
}

void
zlib_free(GSview *a)
{
    if (a->zlib.loaded == FALSE)
	return;
    dll_close(&a->zlib.hmodule);
    a->zlib.hmodule = (GGMODULE)0;
    a->zlib.gzopen = NULL;
    a->zlib.gzread = NULL;
    a->zlib.gzclose = NULL;
    a->zlib.loaded = FALSE;
}

int
zlib_uncompress(GSview *app, GFile *outfile, const char *filename)
{
    gzFile infile;
    char *buffer;
    int count;

    if (zlib_load(app))
	return_error(-1);

    /* create buffer for file copy */
    buffer = (char *)malloc(COPY_BUF_SIZE);
    if (buffer == (char *)NULL)
	return_error(-1);

    if ((infile = app->zlib.gzopen(filename, "rb")) == (gzFile)NULL) {
	free(buffer);
	return_error(-1);
    }
	
    while ( (count = app->zlib.gzread(infile, buffer, COPY_BUF_SIZE)) > 0 ) {
	gfile_write(outfile, buffer, count);
    }
    free(buffer);
    app->zlib.gzclose(infile);
    if (count < 0)
	return_error(-1);
    return 0;
}

int 
bzip2_load(GSview *a)
{
    int code = 0;
    TCHAR buf[1024];
    BZIP2 *bzip2 = &a->bzip2;
    if (bzip2->loaded)
	return 0;

    memset(buf, 0, sizeof(buf));
    code = dll_open(&bzip2->hmodule, TEXT(BZIP2NAME), 
	buf, sizeof(buf)/sizeof(TCHAR)-1);
    if (code != 0) {
	app_csmsg(a, buf);
    }
    else {
	if (code == 0)
	    bzip2->bzopen = (PFN_bzopen)dll_sym(&bzip2->hmodule, "BZ2_bzopen");
	if (bzip2->bzopen == NULL) {
	    app_msg(a, "Can't find bzopen\n");
	    code = -1;
	}
	if (code == 0)
	    bzip2->bzread = (PFN_bzread)dll_sym(&bzip2->hmodule, "BZ2_bzread");
	if (bzip2->bzread == NULL) {
	    app_msg(a, "Can't find bzread\n");
	    code = -1;
	}
	if (code == 0)
	    bzip2->bzclose = (PFN_bzclose)dll_sym(&bzip2->hmodule, "BZ2_bzclose");
	if (bzip2->bzclose == NULL) {
	    app_msg(a, "Can't find bzclose\n");
	    code = -1;
	}
	if (code == 0)
	    bzip2->loaded = TRUE;
	else {
	    dll_close(&bzip2->hmodule);
	    memset(bzip2, 0, sizeof(ZLIB));
	    bzip2->loaded = FALSE;
	}
    }
    return code;
}

void 
bzip2_free(GSview *a)
{
    if (a->bzip2.loaded == FALSE)
	return;
    dll_close(&a->bzip2.hmodule);
    a->bzip2.hmodule = (GGMODULE)0;
    a->bzip2.bzopen = NULL;
    a->bzip2.bzread = NULL;
    a->bzip2.bzclose = NULL;
    a->bzip2.loaded = FALSE;
}

int
bzip2_uncompress(GSview *app, GFile *outfile, const char *filename)
{
    bzFile infile;
    char *buffer;
    int count;

    if (bzip2_load(app))
	return_error(-1);

    /* create buffer for file copy */
    buffer = (char *)malloc(COPY_BUF_SIZE);
    if (buffer == (char *)NULL)
	return_error(-1);

    if ((infile = app->bzip2.bzopen(filename, "rb")) == (gzFile)NULL) {
	free(buffer);
	return_error(-1);
    }
	
    while ( (count = app->bzip2.bzread(infile, buffer, COPY_BUF_SIZE)) > 0 ) {
	gfile_write(outfile, buffer, count);
    }
    free(buffer);
    app->bzip2.bzclose(infile);
    if (count < 0)
	return_error(-1);
    return 0;
}

GFile *
app_temp_gfile(GSview *app, TCHAR *fname, int len)
{
    TCHAR *temp;    
#if defined(UNIX) || defined(OS2)
    long fd;
#endif
    memset(fname, 0, len*sizeof(TCHAR));
    if ( (temp = csgetenv(TEXT("TEMP"))) == NULL )
#ifdef UNIX
	csncpy(fname, "/tmp", len-1);
#else
	csgetcwd(fname, len-1);
#endif
    else
	csncpy(fname, temp, len-1);
    /* Prevent X's in path from being converted by mktemp. */
#if defined(_Windows) || defined(OS2)
    temp = fname;
    while (*temp) {
	if (*temp == '/')
	    *temp = '\\';
	if (*temp < 'a')
	    *temp = (char)tolower(*temp);
	temp = CHARNEXT(temp);
    }
#endif
    if ( cslen(fname) && (fname[cslen(fname)-1] != PATHSEP[0]) )
	csncat(fname, TEXT(PATHSEP), len-1-cslen(fname));

    csncat(fname, TEXT("gsview"), len-1-cslen(fname));
    csncat(fname, TEXT("XXXXXX"), len-1-cslen(fname));
#if defined(UNIX) || defined(OS2)
    fd = mkstemp(fname);
    if (debug & DEBUG_GENERAL)
	app_csmsgf(app, TEXT("Creating temporary file \042%s\042\n"), fname); 
    return gfile_open_handle((void *)fd, gfile_modeWrite | gfile_modeCreate);
#else
    csmktemp(fname);
    if (debug & DEBUG_GENERAL)
	app_csmsgf(app, TEXT("Creating temporary file \042%s\042\n"), fname); 
    return gfile_open(fname, gfile_modeWrite | gfile_modeCreate);
#endif
}

FILE *
app_temp_file(GSview *app, TCHAR *fname, int len)
{
    TCHAR *temp;    
#if defined(UNIX) || defined(OS2)
    long fd;
#endif
    memset(fname, 0, len*sizeof(TCHAR));
    if ( (temp = csgetenv(TEXT("TEMP"))) == NULL )
#ifdef UNIX
	csncpy(fname, "/tmp", len-1);
#else
	csgetcwd(fname, len-1);
#endif
    else
	csncpy(fname, temp, len-1);
    /* Prevent X's in path from being converted by mktemp. */
#if defined(_Windows) || defined(OS2)
    temp = fname;
    while (*temp) {
	if (*temp == '/')
	    *temp = '\\';
	if (*temp < 'a')
	    *temp = (char)tolower(*temp);
	temp = CHARNEXT(temp);
    }
#endif
    if ( cslen(fname) && (fname[cslen(fname)-1] != PATHSEP[0]) )
	csncat(fname, TEXT(PATHSEP), len-1-cslen(fname));

    csncat(fname, TEXT("gsview"), len-1-cslen(fname));
    csncat(fname, TEXT("XXXXXX"), len-1-cslen(fname));
#if defined(UNIX) || defined(OS2)
    fd = mkstemp(fname);
    if (debug & DEBUG_GENERAL)
	app_csmsgf(app, TEXT("Creating temporary file \042%s\042\n"), fname); 
    return fdopen(fd, "wb");
#else
    csmktemp(fname);
    if (debug & DEBUG_GENERAL)
	app_csmsgf(app, TEXT("Creating temporary file \042%s\042\n"), fname); 
    return csfopen(fname, TEXT("wb"));
#endif
}


float 
app_get_points(GSview *a, const TCHAR *str)
{
    float val;
    TCHAR ptbuf[16];
    TCHAR inchbuf[16];
    TCHAR mmbuf[16];
    TCHAR unitbuf[MAXSTR];
    TCHAR *p = unitbuf;
    TCHAR *q;
    char fbuf[64];
    int i;
    memset(unitbuf, 0, sizeof(unitbuf));
    csncpy(unitbuf, str, sizeof(unitbuf)/sizeof(TCHAR)-1);
    ptbuf[0] = inchbuf[0] = mmbuf[0] = '\0';
    load_string(a, IDS_UNITNAME + UNIT_PT, 
	ptbuf, sizeof(ptbuf));
    load_string(a, IDS_UNITNAME + UNIT_IN, 
	inchbuf, sizeof(inchbuf));
    load_string(a, IDS_UNITNAME + UNIT_MM, 
	mmbuf, sizeof(mmbuf));
    while (*p && (*p == ' '))
	p = CHARNEXT(p);
    cs_to_narrow(fbuf, (int)sizeof(fbuf)-1, p, (int)cslen(p)+1);
    fbuf[sizeof(fbuf)-1] = '\0';
    val = (float)atof(fbuf);
    while (*p && ( ((*p >= '0') && (*p <= '9')) || (*p == '.')))
	p = CHARNEXT(p);
    while (*p && (*p == ' '))
	p = CHARNEXT(p);
    q = p;
    while (*q && (*q >= 'A'))
	q = CHARNEXT(q);
    if (*q)
	q = CHARNEXT(q);
    i = (int)(q - p);
    if ((csncmp(p, ptbuf, max(i, (int)cslen(ptbuf))) == 0) ||
	     (csncmp(p, TEXT("pt"), 2) == 0)) {
	/* do nothing */
    }
    else if ((csncmp(p, inchbuf, max(i, (int)cslen(inchbuf))) == 0) || 
	     (csncmp(p, TEXT("in"), 2) == 0))
	val *= 72.0;
    else if ((csncmp(p, mmbuf, max(i, (int)cslen(mmbuf))) == 0) || 
	     (csncmp(p, TEXT("mm"), 2) == 0))
	val *= (float)(72.0 / 25.4);
    else if (csncmp(p, TEXT("cm"), 2) == 0)
	val *= (float)(72.0 / 2.54);
    else if (csncmp(p, TEXT("m"), 1) == 0)
	val *= (float)(72.0 / 0.0254);
    else if (csncmp(p, TEXT("ft"), 2) == 0)
	val *= (float)(72.0 * 12.0);
    return val;
}

void 
app_put_points(GSview *a, UNIT unit, TCHAR *buf, int len, float n)
{
    TCHAR ubuf[16];
    float factor = 1.0;
    if (len < 1)
	return;
    buf[0] = '\0';
    ubuf[0] = '\0';
    load_string(a, IDS_UNITNAME + unit, ubuf, sizeof(ubuf));
    if (len < 32 + (int)cslen(ubuf))
	return;
    switch (unit) {
	case UNIT_MM:
	    factor = (float)(25.4 / 72.0);
	    break;
	case UNIT_IN:
	    factor = (float)(1.0 / 72.0);
	    break;
	case UNIT_PT:
	default:
	   factor = 1.0;
    }
    csnprintf(buf, len, TEXT("%g %s"), n*factor, ubuf);
    buf[len-1] = '0';
}
