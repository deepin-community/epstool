/* Copyright (C) 2001-2002 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cgssrv.h,v 1.6 2003/01/18 02:36:40 ghostgum Exp $ */
/* Ghostscript server header */

/* Public */

/* These error codes are returned where GS error codes
 * may be used.  GS uses 0 for success, -ve for error,
 * and <-100 for more serious errors.
 * We use <-200 to avoid confusion.
 */
typedef enum GSREQ_ERROR_e {
    GSREQ_ERROR_NOERROR = 0,		/* normal */
    GSREQ_ERROR_PAGERANGE = -201,	/* page out of range */
    GSREQ_ERROR_NOMOREPAGE = -202,	/* non-DSC and no more pages */
    GSREQ_ERROR_GS = -203,		/* Ghostscript returned an error */
    GSREQ_ERROR_FILE = -204,		/* Problem with reading file */
    GSREQ_ERROR_SETDEVICE = -205,	/* Probably due to lack of memory */
					/* so reset the page size. */
    GSREQ_ERROR_INTERNAL = -206,	/* Probably due to lack of memory */
    GSREQ_ERROR_UNKNOWN = -299
} GSREQ_ERROR;

typedef enum GSREQ_ACTION_e {
    GSREQ_ACTION_DISPLAY = 0,	
	/* Display a page. */ 
    GSREQ_ACTION_CONVERT = 1,	
	/* Convert to bitmap, PDF etc. */ 
    GSREQ_ACTION_ABORT = 2,	
	/* Abort GS, but don't unload GS DLL */
	/* Not sure if this will be used */
    GSREQ_ACTION_UNLOAD = 3,
	/* We about to exit, so unload GS DLL */
	/* Also used if we are changing GS version */
    GSREQ_ACTION_CLOSE_THREAD = 4
	/* We exiting GSview */
} GSREQ_ACTION;


typedef enum CONVERT_OUTPUT_e {
    CONVERT_FILE,	/* -sOutputFile="output" */
    CONVERT_PIPE,	/* -sOutputFile="%pipe%output" */
    CONVERT_HANDLE	/* -sOutputFile="%handle%output" */
} CONVERT_OUTPUT;

typedef struct GSREQ_CONVERT_s GSREQ_CONVERT;

struct GSREQ_CONVERT_s {
    CONVERT_OUTPUT method; /* determines meaning of output */
    TCHAR output[MAXSTR];
    char gsdevice[MAXSTR];
    PAGELIST pagelist;
    /* FIX: should we include page size here, or use PAGESPEC? */
};


struct GSREQ_s {
    GSREQ *next;
    GSREQ_ACTION action;
    PAGESPEC pagespec;	    /* valid if action is GSREQ_ACTION_DISPLAY */
    GSREQ_CONVERT convert;  /* valid if action is GSREQ_ACTION_CONVERT */
    CDSC *dsc;
    BOOL pdf;
    BOOL dcs2multi;	/* DCS 2.0 separation in separate file */
    BOOL temp_ps;	/* Temporary PostScript file created from bitmap.
			 * This file should be deleted when the request
			 * is removed from the queue.
			 */
    View *view;		/* The view that made this request. */
			/* We send notifications to this view. */
};


GSSRV * gssrv_new(GSview *app);
int gssrv_ref(GSSRV *s);
int gssrv_unref(GSSRV *s);
GSview * gssrv_app(GSSRV *s);
GSIMAGE * gssrv_img(GSSRV *s);
GGTHREAD * gssrv_thread_handle(GSSRV *s);
int gssrv_request(GSSRV *s, GSREQ *reqnew);
void gssrv_notify_view(GSSRV *s, int message, int param);
void gssrv_page_callback(GSSRV *s);
unsigned int gssrv_format(DISPLAY_FORMAT df);
void gssrv_run_thread(void *arg);
void plsrv_run_thread(void *arg);
const char *gssrv_error_message(int id);

/* platform specific */
int gssrv_poll(GSSRV *s);


/**********************************************/
/* Private */

#ifdef DEFINE_CGSSRV

struct GSSRV_s {
    void *handle;		/* Platform specific handle */

    int refcount;		/* Number of users of this object */
    GSview *app;		/* GSview app object */
    GSIMAGE *gsimg;			/* display device image */

    /* A linked list of requests. */
    /* The currently pending request is head. */
    /* The request list is NULL if there is nothing to do. */
    GSREQ *req;
    
    /* Before send a document or page to GS, we set request_removed to FALSE.
     * If we get a page callback, request_removed will be set to TRUE and the
     * request removed from the list.  If we don't get a callback (e.g beyond last 
     * page of non-DSC, or EPS without showpage), then request_removed will be
     * still be TRUE and we need to remove the request afterwards.
     */
    BOOL request_removed;    /* TRUE if we stopped at a page and removed the request */
    
    /* The last request we received.
     * This affects whether we just advance to the next page of the 
     * non-DSC document or whether we need to restart Ghostscript.
     * Once we start sending the document or page to GS this is also
     * the current request.
     */
    GSREQ lastreq;

    BOOL close_thread;		/* Set to when we wnat to exit */
				/* This overides all other requests */
    BOOL abort;			/* Ignore errors and exit GS */
    BOOL unload;		/* Unload GS, because we are exiting,
				 * or because we need to use a different
				 * GS, or because someone else needs to
			 	 * use GS DLL.
				 */

    BOOL waiting;		/* Used when singlethread to determine
				 * if we are polling or waiting.
				 * Ignored when multithread.
			         */
    GGEVENT event;		/* Semaphore for synchronising threads */
    GGTHREAD thread_handle;	/* GS thread */

    /* PDF page count is obtained when GS opens file */
    int pdf_page_first;
    int pdf_page_count;
    /* PDF media box, crop box and rotate are obtained when a page is opened */
    BBOX pdf_media_box;
    BBOX pdf_crop_box;
    ORIENT pdf_orient;

    /* GhostPCL read from stdin, so we need to keep the file open */
    GFile *gf;			/* stdin for GhostPCL */

    /* Buffer for storing stdout which we search for text tags
     * information about the PDF file.
     */
#define MAX_TAG_LEN 4096
    char pdf_tag_line[MAX_TAG_LEN];
    int pdf_page_number;
    PDFLINK *pdf_link_head;
};

int gssrv_remove(GSSRV *s, GSREQ *req);
void gssrv_request_translate(GSSRV *s);
int gssrv_set_lastreq(GSSRV *s, GSREQ *req);
BOOL gssrv_check_requests(GSSRV *s);
BOOL gssrv_check_abort(GSSRV *s);
int gssrv_event_wait(GSSRV *s);
int gssrv_run_string(GSSRV *s, const char *str);
int gssrv_run_formatted(GSSRV *s, const char *str, ...);


/* platform specific */
int gssrv_platform_init(GSSRV *s);
int gssrv_platform_finish(GSSRV *s);
int gssrv_event_wait(GSSRV *s);
int gssrv_event_post(GSSRV *s);

#endif /* DEFINE_GSSRV */
