/* Copyright (C) 2001-2002 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cpagec.h,v 1.1 2002/04/17 11:39:07 ghostgum Exp $ */
/* Page cache header */

/* Public */
void pt_to_pixel(PAGESPEC *ps, double *x, double *y);
void pixel_to_pt(PAGESPEC *ps, double *x, double *y);

PAGECACHE *pagecache_new(GSview *app, GSREQ *req, IMAGE *img, PDFLINK *pdflinks);
PAGECACHE * pagecache_find(GSview *app, PAGESPEC *ps);
int pagecache_unref(GSview *app, PAGECACHE *page);
int pagecache_unref_all(GSview *app) ;


/****************************************************/
/* Private */
#ifdef DEFINE_CPAGEC

/* page cache */
struct PAGECACHE_s {
    PAGECACHE *next;	    /* linked list */
    int refcount;	    /* number of views using this image */
    PAGESPEC pagespec;
    /* changes to img must be protected by app_lock */
    IMAGE img;
    PDFLINK *pdflink;
};


#endif /* DEFINE_CPAGEC */
