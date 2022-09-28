/* Copyright (C) 1993-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cdoc.h,v 1.9 2005/01/14 08:15:52 ghostgum Exp $ */
/* Document header */

/* Public */
#ifndef Doc_TYPEDEF
#define Doc_TYPEDEF
typedef struct Doc_s Doc;
#endif

/* Information about a document, suitable for showing in a dialog */
typedef struct DocInfo_s DocInfo;
struct DocInfo_s {
    TCHAR name[MAXSTR];
    TCHAR type[MAXSTR];
    TCHAR title[MAXSTR];
    TCHAR date[MAXSTR];
    TCHAR bbox[MAXSTR];
    TCHAR hiresbbox[MAXSTR];
    TCHAR orientation[MAXSTR];
    TCHAR pageorder[MAXSTR];
    TCHAR pagemedia[MAXSTR];
    TCHAR pages[MAXSTR];
};

typedef enum DocType_e {
    DOC_UNKNOWN,
    DOC_PS,
    DOC_PDF,
    DOC_PCL,
    DOC_BITMAP
} DocType;

Doc * doc_new(GSview *a);
int doc_add(Doc *d, GSview *a);
int doc_remove(Doc *d);
int doc_ref(Doc *d);
int doc_unref(Doc *d);
GSview * doc_app(Doc *d);
View **doc_views(Doc *doc);
int doc_open(Doc *doc, LPCTSTR filename);
int doc_close(Doc *doc);
DocType doc_type(Doc *doc);
BOOL doc_is_open(Doc *doc);
void doc_ignore_dsc(Doc *doc, BOOL flag);
void doc_dsc_warn(Doc *doc, int level);
void doc_verbose(Doc *doc, BOOL verbose);
void doc_dump(Doc *doc);
int doc_map_page(Doc *doc, int page);
int doc_page_limit(Doc *doc, int page);
LPCTSTR doc_name(Doc *doc);
void doc_info(Doc *doc, DocInfo *info);
void doc_ordlabel(Doc *doc, char *buf, int buflen, int page_number);
int doc_copyfile(Doc *doc, LPCTSTR filename);

/* platform specific */
void doc_savestat(Doc *doc);	/* save file length and date/time */
BOOL doc_changed(Doc *doc);


/****************************************************/
/* Private */
#ifdef DEFINE_CDOC

#include "cpdfscan.h"

typedef struct tagTEXTINDEX {
    int word;	/* offset to word */
    int line;	/* line number on page */
    CDSCBBOX bbox;
} TEXTINDEX;

/* If you change this, remember to update doc_begin and doc_end */
struct Doc_s {
    void *handle;		/* Platform specific handle */
				/* e.g. pointer to MFC CDocument */

    GSview *app;		/* GSview app object */
    Doc *next;			/* next document in list */
    int refcount;		/* number of views of this document */

    /* List of views */
    View *viewlist;

    TCHAR text_name[MAXSTR];	/* name of file containing extracted text */
#ifdef NOTUSED
    TEXTINDEX *text_index;
    unsigned int text_index_count;  /* number of words in index */
    char *text_words;		    /* storage for words */
#endif

    TCHAR name[MAXSTR];		/* name of selected document file */
    TCHAR tname[MAXSTR];	/* name of temporary file (uncompressed) */
    DocType doctype;		/* DOC_PS, DOC_PDF, etc. */
    BOOL gzip;			/* file compressed with gzip */
    BOOL bzip2;			/* file compressed with bzip2 */
    int page_count;		
    DSC_OFFSET length1;		/* length of selected file (uncompressed) */
    DSC_OFFSET length2;		/* length of selected file (uncompressed) */
    unsigned long time1;	/* date/time of open file */
    unsigned long time2;	/* date/time of open file */
    unsigned int dpi;		/* Resolution if available from PJL */

    /* If doctype == DOC_PS, the following are used */
    CDSC *dsc;			/* DSC structure.  NULL if not DSC */
#ifdef NOTUSED    
    BOOL ignore_special;	/* %%PageOrder: Special to be ignored */
#endif
    BOOL ignore_dsc;		/* DSC comments to be ignored */
    int dsc_warn;		/* Warning level for DSC comments */
    BOOL verbose;		/* Enable DSC debug messages */
    BOOL ctrld;			/* file starts with ^D */
    BOOL pjl;			/* file starts with HP LaserJet PJL */

    /* if doctype == DOC_PDF, the following is used */
    PDFSCAN *pdfscan;
};

#endif /* DEFINE_CDOC */
