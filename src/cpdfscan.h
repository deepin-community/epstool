/* Copyright (C) 2002 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cpdfscan.h,v 1.2 2002/07/26 12:09:06 ghostgum Exp $ */
/* PDF scanner header */

/* This is a rudimentary PDF scanner, intended to get
 * the page count, and for each page the Rotate, MediaBox 
 * and CropBox.
 */

/* Opaque type for PDF scanner */
typedef struct PDFSCAN_s PDFSCAN;

/* For MediaBox and CropBox */
typedef struct PDFBBOX_s PDFBBOX;
struct PDFBBOX_s {
    float llx;
    float lly;
    float urx;
    float ury;
};

/* Open a PDF file and read trailer, cross reference tables,
 * and number of pages.  Returns NULL if it fails.
 * If print_fn is NULL, it will print error messages to stdout.
 * PDF file is kept open until pdf_scan_close.
 */
PDFSCAN * pdf_scan_open(const TCHAR *filename, void *handle,
    int(*print_fn)(void *handle, const char *ptr, int len));

/* Return number of pages in PDF file */
int pdf_scan_page_count(PDFSCAN *ps) ;

/* Read Rotate, MediaBox and CropBox for the specified page */
/* Return 0 if OK, non-zero on error */
int pdf_scan_page_media(PDFSCAN *ps, int pagenum, int *rotate,
    PDFBBOX *mediabox, PDFBBOX *cropbox);

/* Close PDF file and ps. */
void pdf_scan_close(PDFSCAN *ps);

