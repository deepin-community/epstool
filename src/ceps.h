/* Copyright (C) 1993-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: ceps.h,v 1.13 2005/06/10 09:39:24 ghostgum Exp $ */

/* EPS preview manipulation */

int extract_doseps(Doc *doc, LPCTSTR outname, BOOL preview);
int extract_macbin(Doc *doc, LPCTSTR outname, BOOL preview);
int make_eps_tiff(Doc *doc, IMAGE *img, CDSCBBOX devbbox, 
    CDSCBBOX *bbox, CDSCFBBOX *hires_bbox,
    float xdpi, float ydpi, BOOL tiff4, BOOL use_packbits, BOOL reverse,
    LPCTSTR epsname);
int make_eps_user(Doc *doc, LPCTSTR preview_name, BOOL reverse, 
    LPCTSTR epsname);
int make_eps_interchange(Doc *doc, IMAGE *img, CDSCBBOX devbbox, 
    CDSCBBOX *bbox, CDSCFBBOX *hires_bbox,
    LPCTSTR epsname);
int make_eps_metafile(Doc *doc, IMAGE *img, CDSCBBOX devbbox, 
    CDSCBBOX *bbox, CDSCFBBOX *hires_bbox,
    float xdpi, float ydpi, BOOL reverse, LPCTSTR epsname);
int make_eps_pict(Doc *doc, IMAGE *img,
    CDSCBBOX *bbox, CDSCFBBOX *hires_bbox,
    float xdpi, float ydpi, CMAC_TYPE type, LPCTSTR epsname);
int copy_page_temp(Doc *doc, GFile *f, int page);
int copy_page_nosave(Doc *doc, GFile *f, int page);
int copy_eps(Doc *doc, LPCTSTR epsname, CDSCBBOX *bbox, CDSCFBBOX *hires_bbox,
    int offset, BOOL dcs2_multi);

typedef struct RENAME_SEPARATION_s RENAME_SEPARATION;
struct RENAME_SEPARATION_s {
    char *oldname;
    char *newname;
    RENAME_SEPARATION *next;
};
int rename_separations(CDSC *dsc, RENAME_SEPARATION *rs, const char *renamed[]);
int copy_dcs2(Doc *doc, GFile *docfile, 
    Doc *doc2, GFile *docfile2, 
    GFile *epsfile, LPCTSTR epsname,
    int offset, BOOL dcs2_multi, BOOL write_all, BOOL missing_separations,
    FILE_POS *complen, GFile *composite, RENAME_SEPARATION *rs,
    int tolerance);
