/* Copyright (C) 2001-2003 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: capp.h,v 1.7 2003/01/18 02:36:40 ghostgum Exp $ */
/* Application header */

/* Public */

int app_platform_init(GSview *a);
int app_platform_finish(GSview *a);
int app_gssrv_request(GSview *a, GSREQ *reqnew);
int app_plsrv_request(GSview *a, GSREQ *reqnew);
GSview *app_new(void *handle, BOOL multithread);
int app_free(GSview *a);
int app_ref(GSview *a);
int app_unref(GSview *a);
GSDLL **app_dll(GSview *a);
PLDLL **app_pldll(GSview *a);
Doc **app_docs(GSview *a);
OPTION *app_option(GSview *a);
PAGECACHE **app_pagecache(GSview *a);
BOOL app_multithread(GSview *a);
int app_lock(GSview *a);
int app_unlock(GSview *a);
#define MAX_LAST_FILES 4
void app_update_last_files(GSview *a, LPCTSTR filename);
LPCTSTR app_last_files(GSview *a, int i);
int app_read_last_files(GSview *a, LPCTSTR filename);
int app_write_last_files(GSview *a, LPCTSTR filename);

int zlib_load(GSview *a);
void zlib_free(GSview *a);
int zlib_uncompress(GSview *app, GFile *outfile, const char *filename);
#ifdef __cplusplus
extern "C" {
#endif
    /* for zlib gunzip decompression */
    typedef void *gzFile ;
    typedef gzFile (WINAPI *PFN_gzopen)(const char *path, const char *mode);
    typedef int (WINAPI *PFN_gzread)(gzFile file, void *buf, unsigned len);
    typedef int (WINAPI *PFN_gzclose)(gzFile file);
#ifdef __cplusplus
}
#endif

int bzip2_load(GSview *a);
void bzip2_free(GSview *a);
int bzip2_uncompress(GSview *app, GFile *outfile, const char *filename);
#ifdef __cplusplus
extern "C" {
#endif
    /* for bzip2 decompression */
    typedef void *bzFile ;
    typedef bzFile (WINAPI *PFN_bzopen)(const char *path, const char *mode);
    typedef int (WINAPI *PFN_bzread)(bzFile file, void *buf, unsigned len);
    typedef int (WINAPI *PFN_bzclose)(bzFile file);
#ifdef __cplusplus
}
#endif

/* Write text messages to log window */
void app_log(const char *str, int len);
int app_msgf(GSview *a, const char *fmt, ...);
int app_msg(GSview *a, const char *str);
int app_msg_len(GSview *a, const char *str, int len);
int app_msg_len_nolock(GSview *a, const char *str, int len);
int app_csmsgf(GSview *a, LPCTSTR fmt, ...);
int app_csmsg(GSview *a, LPCTSTR wstr);
int app_csmsg_len(GSview *a, LPCTSTR wstr, int wlen);
int app_csmsg_len_nolock(GSview *a, LPCTSTR wstr, int wlen);

int load_string(GSview *app, int i, TCHAR *buf, int len);
const char *get_string(GSview *a, int id);	/* Not Windows */
int get_dsc_response(GSview *app, LPCTSTR str);
GFile *app_temp_gfile(GSview *app, TCHAR *fname, int len);
FILE *app_temp_file(GSview *app, TCHAR *fname, int len);
int app_msg_box(GSview *a, LPCTSTR str, int icon);


extern const char szAppName[];		/* GLOBAL */


/****************************************************/
/* Private */
#ifdef DEFINE_CAPP

typedef struct STRING_s STRING;

typedef struct ZLIB_s ZLIB;
struct ZLIB_s {
    BOOL loaded;
    GGMODULE hmodule;
    PFN_gzopen gzopen;
    PFN_gzread gzread;
    PFN_gzclose gzclose;
};

typedef struct BZIP2_s BZIP2;
struct BZIP2_s {
    BOOL loaded;
    GGMODULE hmodule;
    PFN_bzopen bzopen;
    PFN_bzread bzread;
    PFN_bzclose bzclose;
};

struct GSview_s {
    void *handle;	/* Platform specific handle */
			/* e.g. pointer to MFC theApp */

    int refcount;	/* number of references to Application */
    /* MRU files */
    TCHAR last_files[MAX_LAST_FILES][MAXSTR]; /* last 4 files used */
    int last_files_count;		    /* number of files known */

    /* List of documents */
    Doc *doclist;

    /* Ghostscript DLL */
    GSDLL *gsdll;

    /* GhostPCL DLL */
    PLDLL *pldll;

    /* The server thread which executes ghostscript */
    /* and handles requests for rendering and printing. */
    GSSRV *gssrv;

    /* The server thread which executes GhostPCL */
    /* and handles requests for rendering and printing. */
    GSSRV *plsrv;

    /* The list of rendered pages that we have cached */
    PAGECACHE *pagecache;

    /* Options */
    OPTION option;
    TCHAR option_name[MAXSTR];	/* filename for options */

    /* Compression libraries */
    ZLIB zlib;
    BZIP2 bzip2;

    /* TRUE if application is shutting down */
    BOOL quitnow;

    /* When running single threaded, we need to return to main
     * loop to complete some commands.
     */
    BOOL in_main;	/* TRUE if in main loop */
    BOOL go_main;	/* TRUE if we should return to main loop */

    /* We can run as either single thread, or multithread with
     * Ghostscript on the second thread.
     */
    BOOL multithread;

    /* When running multithread, we use a mutex to protect
     * access to resources.
     */
    GGMUTEX hmutex;
    int lock_count;	/* for debugging lock */

    /* Text Window for Ghostscript Messages */
    /* This is maintained in narrow characters, even if the user
     * interface is using wide characters.  This is because we
     * want to have a simple file for sending bug reports,
     * and because Ghostscript is taking all input and filenames
     * as narrow characters. If a wide character filename is
     * not translated to narrow characters correctly, this
     * should show up in the message log.
     */
#ifdef _Windows
#define TWLENGTH 61440
#endif
#ifdef OS2
#define TWLENGTH 16384
#endif
#ifndef TWLENGTH
#define TWLENGTH 61440
#endif
#define TWSCROLL 1024
    char twbuf[TWLENGTH];
    int twend;

    STRING *strings;	/* translation strings */
};

int free_strings(STRING *s);
int init_strings(GSview *a, const char *filename);

float app_get_points(GSview *a, const TCHAR *str);
void app_put_points(GSview *a, UNIT unit, TCHAR *buf, int len, float n);

#endif /* DEFINE_CAPP */

