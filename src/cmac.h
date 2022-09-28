/* Copyright (C) 2003-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cmac.h,v 1.3 2005/06/10 09:39:24 ghostgum Exp $ */
/* Macintosh AppleSingle, AppleDouble and MacBinary file formats */

typedef enum CMAC_TYPE_e {
    CMAC_TYPE_NONE=0,
    CMAC_TYPE_SINGLE=1,
    CMAC_TYPE_DOUBLE=2,
    CMAC_TYPE_MACBIN=3,
    CMAC_TYPE_RSRC=4
} CMAC_TYPE;

/* Mac finder and fork details */
typedef struct CMACFILE_s {
    CMAC_TYPE type;
    unsigned char file_type[4];	/* usually EPSF for us */
    unsigned char file_creator[4];
    DWORD finder_begin;
    DWORD finder_length;
    DWORD data_begin;
    DWORD data_length;
    DWORD resource_begin;
    DWORD resource_length;
    DWORD pict_begin;	/* PICT resource in EPSF file */
    DWORD pict_length;
} CMACFILE;


DWORD get_bigendian_dword(const unsigned char *buf);
WORD get_bigendian_word(const unsigned char *buf);
void put_bigendian_dword(unsigned char *dw, DWORD val);
void put_bigendian_word(unsigned char *w, WORD val);

/* Read header and identify if MacBinary, AppleSingle or AppleDouble */
CMACFILE *get_mactype(GFile *f);

/* Read resources and find location of EPSF PICT preview */
/* Return 0 on success, +ve on unsuitable file, -ve on error */
int get_pict(GFile *f, CMACFILE *mac, int debug);

/* Copy PICT from resources to another file */
/* Return 0 on succes, -ve on error */
int extract_mac_pict(GFile *f, CMACFILE *mac, LPCTSTR outname);

/* Extract EPSF from data fork to a file */
/* Returns 0 on success, negative on failure */
int extract_mac_epsf(GFile *f, CMACFILE *mac, LPCTSTR outname);

/* Write Macintosh binary files in one of several formats.
 * The data fork if present contains the EPSF.
 * The resource fork if present contains a preview image in PICT
 * format with id=256.
 * The finder informations give type "EPSF" and creator "MSWD".
 */

/* Write preview to AppleDouble format */
int write_appledouble(GFile *f, LPCTSTR pictname);

/* Write EPSF and preview to AppleSingle format */
int write_applesingle(GFile *f, LPCTSTR epsname, LPCTSTR pictname);

/* Write EPSF and preview to MacBinary I format */
/* name is the 1-63 character filename written in the MacBinary header */
int write_macbin(GFile *f, const char *name, LPCTSTR epsname, LPCTSTR pictname);

/* Write resources containing a single PICT with id=256 to file.
 * Return number of bytes written if OK, -ve if not OK.
 * If f==NULL, return number of bytes required for resources.
 */
int write_resource_pict(GFile *f, LPCTSTR pictname);

/* Returns -1 for error, 0 for Mac and 1 for non-Mac */
/* If Macintosh format and verbose, print some details to stdout */
int dump_macfile(LPCTSTR filename, int verbose);
