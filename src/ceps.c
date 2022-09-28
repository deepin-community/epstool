/* Copyright (C) 1993-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: ceps.c,v 1.43 2005/06/10 08:45:36 ghostgum Exp $ */

/* EPS preview manipulation */

#include "common.h"
#include <time.h>
#include "gdevdsp.h"
#include "dscparse.h"
#include "capp.h"
#include "cbmp.h"
#define DEFINE_CDOC
#include "cdoc.h"
#include "cmac.h"
#include "ceps.h"
#include "cimg.h"
#include "cps.h"


#define DOSEPS_HEADER_SIZE 30

/* Local prototypes */
static void write_doseps_header(CDSCDOSEPS *doseps, GFile *outfile);
static void shift_preview(unsigned char *preview, int bwidth, int offset);
static void validate_devbbox(IMAGE *img, CDSCBBOX *devbbox);
int write_interchange(GFile *f, IMAGE *img, CDSCBBOX devbbox);
static void write_bitmap_info(IMAGE *img, LPBITMAP2 pbmi, GFile *f);
static void make_bmp_info(LPBITMAP2 pbmi, IMAGE *img, float xdpi, float ydpi);
void copy_nobbox(GFile *outfile, GFile *infile, 
    FILE_POS begin, FILE_POS end);
void copy_bbox_header(GFile *outfile, 
    GFile *infile, FILE_POS begin, FILE_POS end,
    CDSCBBOX *bbox, CDSCFBBOX *hiresbbox);
static int without_eol(const char *str, int length);
static FILE_POS write_platefile_comments(Doc *doc, GFile *docfile, 
    GFile *epsfile, LPCTSTR epsname,
    int offset, FILE_POS file_offset, 
    BOOL dcs2_multi, BOOL write_all, BOOL missing_separations,
    const char *renamed[], BOOL some_renamed);
static FILE_POS write_singlefile_separations(Doc *doc, GFile *docfile, 
    GFile *epsfile, 
    BOOL dcs2_multi, BOOL write_all, BOOL missing_separations,
    BOOL some_renamed);

/* A placeable Windows Metafile header is needed
 * when the metafile is in a WMF file, but not
 * when it is in memory.
 */
typedef struct WINRECT_s {
	WORD	left;
	WORD	top;
	WORD	right;
	WORD	bottom;
} WINRECT;

typedef struct METAFILEHEADER_s {
    DWORD	key;
    WORD 	hmf;
    WINRECT 	bbox;
    WORD	inch;
    DWORD	reserved;
    WORD	checksum;
} METAFILEHEADER;


static void
write_doseps_header(CDSCDOSEPS *doseps, GFile *outfile)
{
    unsigned char doseps_id[] = {0xc5, 0xd0, 0xd3, 0xc6};
    gfile_write(outfile, doseps_id, 4);
    write_dword(doseps->ps_begin, outfile);
    write_dword(doseps->ps_length, outfile);
    write_dword(doseps->wmf_begin, outfile);
    write_dword(doseps->wmf_length, outfile);
    write_dword(doseps->tiff_begin, outfile);
    write_dword(doseps->tiff_length, outfile);
    write_word((WORD)(doseps->checksum), outfile);
}

/* shift preview by offset bits to the left */
/* width is in bytes */
/* fill exposed bits with 1's */
static void
shift_preview(unsigned char *preview, int bwidth, int offset)
{
int bitoffset;
int byteoffset;
int newwidth;
int shifter;
int i;
    if (offset == 0)
	return;
    byteoffset = offset / 8;
    newwidth = bwidth - byteoffset;
    /* first remove byte offset */
    memmove(preview, preview+byteoffset, newwidth);
    memset(preview+newwidth, 0xff, bwidth-newwidth);
    /* next remove bit offset */
    bitoffset = offset - byteoffset*8;
    if (bitoffset==0)
	return;
    bitoffset = 8 - bitoffset;
    for (i=0; i<newwidth; i++) {
       shifter = preview[i] << 8;
       if (i==newwidth-1)
	   shifter += 0xff;	/* can't access preview[bwidth] */
       else
	   shifter += preview[i+1];  
       preview[i] = (unsigned char)(shifter>>bitoffset);
    }
}


static void
validate_devbbox(IMAGE *img, CDSCBBOX *devbbox)
{
    /* make sure the pixel coordinates are valid */
    if ((devbbox->llx < 0) || (devbbox->llx >= (int)img->width))
	devbbox->llx = 0;
    if ((devbbox->urx < 0) || (devbbox->urx >= (int)img->width))
	devbbox->urx = img->width;
    if ((devbbox->lly < 0) || (devbbox->lly >= (int)img->height))
	devbbox->lly = 0;
    if ((devbbox->ury < 0) || (devbbox->ury >= (int)img->height))
	devbbox->ury = img->height;

    if ((devbbox->llx >= devbbox->urx) || (devbbox->lly >= devbbox->ury)) {
	devbbox->llx = devbbox->lly = 0;
	devbbox->urx = img->width;
	devbbox->ury = img->height;
    }
}

/* Write bitmap info.
 * This works even if LPBITMAP2 is not packed.
 */
static void
write_bitmap_info(IMAGE *img, LPBITMAP2 pbmi, GFile *f)
{
    int i;
    unsigned char r, g, b;
    unsigned char quad[4];
    int palcount = 0;

    /* write bitmap info */
    write_dword(BITMAP2_LENGTH, f);
    write_dword(pbmi->biWidth, f);
    write_dword(pbmi->biHeight, f);
    write_word(pbmi->biPlanes, f);
    write_word(pbmi->biBitCount, f);
    write_dword(pbmi->biCompression, f);
    write_dword(pbmi->biSizeImage, f);
    write_dword(pbmi->biXPelsPerMeter, f);
    write_dword(pbmi->biYPelsPerMeter, f);
    write_dword(pbmi->biClrUsed, f);
    write_dword(pbmi->biClrImportant, f);
  
    if (pbmi->biBitCount <= 8)
	palcount = 1 << pbmi->biBitCount;
    for (i=0; i<palcount; i++) {
	image_colour(img->format, i, &r, &g, &b);
	quad[0] = b;
	quad[1] = g;
	quad[2] = r;
	quad[3] = '\0';
	gfile_write(f, quad, 4);
    }
}

/* Make a BMP header from an IMAGE.
 * WARNING: The pbmi structure might not be packed, so it
 * should only be used by write_bitmap_info(), and not written
 * out directly.
 */
static void
make_bmp_info(LPBITMAP2 pbmi, IMAGE *img, float xdpi, float ydpi)
{
    int palcount = 0;
    int depth = image_depth(img);
    if (depth <= 8)
	palcount = 1 << depth;

    pbmi->biSize = sizeof(BITMAP2); /* WARNING - MAY NOT BE PACKED */
    pbmi->biWidth = img->width;
    pbmi->biHeight = img->width;
    pbmi->biPlanes = 1;
    pbmi->biBitCount = (WORD)image_depth(img);
    pbmi->biCompression = 0;
    pbmi->biSizeImage = 0;
    pbmi->biXPelsPerMeter = (long)(1000 * xdpi / 25.4);
    pbmi->biYPelsPerMeter = (long)(1000 * ydpi / 25.4);
    pbmi->biClrUsed = palcount;
    pbmi->biClrImportant = palcount;
}

/*********************************************************/

/* extract EPS or TIFF or WMF file from DOS EPS file */
int 
extract_doseps(Doc *doc, LPCTSTR outname, BOOL preview)
{
unsigned long pos;
unsigned long len;
unsigned int count;
char *buffer;
GFile* epsfile;
BOOL is_meta = TRUE;
GFile *outfile;
CDSC *dsc = doc->dsc;

    if ((dsc == (CDSC *)NULL) || (dsc->doseps == (CDSCDOSEPS *)NULL)) {
	app_csmsgf(doc->app, 
	    TEXT("Document \042%s\042 is not a DOS EPS file\n"),
	    doc->name);
	return -1;
    }

    epsfile = gfile_open(doc_name(doc), gfile_modeRead);
    pos = dsc->doseps->ps_begin;
    len = dsc->doseps->ps_length;
    if (preview) {
	pos = dsc->doseps->wmf_begin;
	len = dsc->doseps->wmf_length;
	if (pos == 0L) {
	    pos = dsc->doseps->tiff_begin;
	    len = dsc->doseps->tiff_length;
	    is_meta = FALSE;
	}
    }
    if (pos == 0L) {
	gfile_close(epsfile);
	app_csmsgf(doc->app, 
	    TEXT("Document \042%s\042 does not have a %s section\n"),
	    doc->name, preview ? TEXT("preview") : TEXT("PostScript"));
	return -1;
    }
    gfile_seek(epsfile, pos, gfile_begin); /* seek to section to extract */

    outfile = gfile_open(outname, gfile_modeWrite | gfile_modeCreate);
    if (outfile == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Failed to open output file \042%s\042\n"), outname);
	gfile_close(epsfile);
	return -1;
    }
    
    /* create buffer for file copy */
    buffer = (char *)malloc(COPY_BUF_SIZE);
    if (buffer == (char *)NULL) {
	app_csmsgf(doc->app, TEXT("Out of memory in extract_doseps\n"));
	gfile_close(epsfile);
	if (*outname!='\0')
	    gfile_close(outfile);
	return -1;
    }

    if (preview && is_meta) {
	/* check if metafile already contains header */
	DWORD key;
	char keybuf[4];
	DWORD wmf_key = 0x9ac6cdd7UL;
	gfile_read(epsfile, keybuf, 4);
	key = (unsigned int)(keybuf[0]) + 
	     ((unsigned int)(keybuf[1])<<8) + 
	     ((unsigned int)(keybuf[2])<<16) + 
	     ((unsigned int)(keybuf[3])<<24);
	gfile_seek(epsfile, pos, gfile_begin);	/* seek to section to extract */
	if ( key != wmf_key ) {
	    /* write placeable Windows Metafile header */
	    METAFILEHEADER mfh;
	    int i, temp;
	    unsigned short *pw;
	    mfh.key = wmf_key;
	    mfh.hmf = 0;
	    /* guess the location - this might be wrong */
	    mfh.bbox.left = 0;
	    mfh.bbox.top = 0;
	    if (dsc->bbox != (CDSCBBOX *)NULL) {
		temp = (dsc->bbox->urx - dsc->bbox->llx);
		/* double transfer to avoid GCC Solaris bug */
		mfh.bbox.right = (WORD)temp;	
		mfh.bbox.bottom = (WORD)(dsc->bbox->ury - dsc->bbox->lly);
		temp = (dsc->bbox->ury - dsc->bbox->lly);
		mfh.bbox.bottom = (WORD)temp;
	    }
	    else {
		/* bbox missing, assume A4 */
		mfh.bbox.right = 595;
		mfh.bbox.bottom = 842;
	    }
	    mfh.inch = 72;	/* PostScript points */
	    mfh.reserved = 0L;
	    mfh.checksum =  0;
	    pw = (WORD *)&mfh;
	    temp = 0;
	    for (i=0; i<10; i++) {
		temp ^= *pw++;
	    }
	    mfh.checksum = (WORD)temp;
	    write_dword(mfh.key, outfile);
	    write_word(mfh.hmf, outfile);
	    write_word(mfh.bbox.left,   outfile);
	    write_word(mfh.bbox.top,    outfile);
	    write_word(mfh.bbox.right,  outfile);
	    write_word(mfh.bbox.bottom, outfile);
	    write_word(mfh.inch, outfile);
	    write_dword(mfh.reserved, outfile);
	    write_word(mfh.checksum, outfile);
	}
    }

    while ( (count = (unsigned int)min(len,COPY_BUF_SIZE)) != 0 ) {
	count = (int)gfile_read(epsfile, buffer, count);
	gfile_write(outfile, buffer, count);
	if (count == 0)
	    len = 0;
	else
	    len -= count;
    }
    free(buffer);
    gfile_close(epsfile);
    gfile_close(outfile);

    return 0;
}

/* extract EPS or PICT file from Macintosh EPSF file */
int 
extract_macbin(Doc *doc, LPCTSTR outname, BOOL preview)
{
unsigned long pos;
unsigned long len;
unsigned int count;
char *buffer;
GFile* epsfile;
GFile *outfile;
CDSC *dsc = doc->dsc;

CMACFILE *mac;

    if ((dsc == (CDSC *)NULL) || (dsc->macbin == (CDSCMACBIN *)NULL)) {
	app_csmsgf(doc->app, 
	TEXT("Document \042%s\042 is not a Macintosh EPSF file with preview\n"),
	    doc->name);
	return -1;
    }

    epsfile = gfile_open(doc_name(doc), gfile_modeRead);
    if (epsfile == NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Failed to open \042%s\042\n"), doc_name(doc));
	return -1;
    }

    if (preview) {
	int code = 0;
	mac = get_mactype(epsfile);
	if (mac == NULL) {
	    app_csmsgf(doc->app, TEXT("Not a Mac file with resource fork\n"));
	    code = -1;
	}
	if (code == 0) {
	    code = get_pict(epsfile, mac, FALSE);
	    if (code)
	        app_csmsgf(doc->app, 
		    TEXT("Resource fork didn't contain PICT preview\n"));
	}
	if (code == 0) {
	    code = extract_mac_pict(epsfile, mac, outname);
	    if (code)
		app_csmsgf(doc->app, 
		    TEXT("Failed to find PICT preview or write file\n"));
	}
	gfile_close(epsfile);
	return code;
    }

    pos = dsc->macbin->data_begin;
    len = dsc->macbin->data_length;
    if (len == 0) {
	app_csmsgf(doc->app, 
	    TEXT("File has not data section for EPSF\n"));
	gfile_close(epsfile);
	return -1;
    }
    gfile_seek(epsfile, pos, gfile_begin); /* seek to section to extract */

    outfile = gfile_open(outname, gfile_modeWrite | gfile_modeCreate);
    if (outfile == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Failed to open output file \042%s\042\n"), outname);
	gfile_close(epsfile);
	return -1;
    }
    
    /* create buffer for file copy */
    buffer = (char *)malloc(COPY_BUF_SIZE);
    if (buffer == (char *)NULL) {
	app_csmsgf(doc->app, TEXT("Out of memory in extract_doseps\n"));
	gfile_close(epsfile);
	if (*outname!='\0')
	    gfile_close(outfile);
	return -1;
    }

    while ( (count = (unsigned int)min(len,COPY_BUF_SIZE)) != 0 ) {
	count = (int)gfile_read(epsfile, buffer, count);
	gfile_write(outfile, buffer, count);
	if (count == 0)
	    len = 0;
	else
	    len -= count;
    }
    free(buffer);
    gfile_close(epsfile);
    gfile_close(outfile);

    return 0;
}

/*********************************************************/

/* FIX: instead of devbbox, perhaps we should use CDSCFBBOX in points,
  or simply grab this from doc. */

/* make a PC EPS file with a TIFF Preview */
/* from a PS file and a bitmap */
int
make_eps_tiff(Doc *doc, IMAGE *img, CDSCBBOX devbbox, 
    CDSCBBOX *bbox, CDSCFBBOX *hires_bbox,
    float xdpi, float ydpi, BOOL tiff4, BOOL use_packbits, BOOL reverse,
    LPCTSTR epsname)
{
GFile *epsfile;
GFile *tiff_file;
TCHAR tiffname[MAXSTR];
CDSCDOSEPS doseps;
int code;
GFile *tpsfile;
TCHAR tpsname[MAXSTR];
char *buffer;
unsigned int count;
CDSC *dsc = doc->dsc;

    if (dsc == NULL)
	return -1;

    validate_devbbox(img, &devbbox);

    /* Create TIFF file */
    if ((tiff_file = app_temp_gfile(doc->app, tiffname, 
	sizeof(tiffname)/sizeof(TCHAR))) == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't open temporary TIFF file \042%s\042\n"),
	    tiffname);
	return -1;
    }
    code = image_to_tiff(tiff_file, img, 
	devbbox.llx, devbbox.lly, devbbox.urx, devbbox.ury, 
	xdpi, ydpi, tiff4, use_packbits);
    gfile_close(tiff_file);
    if (code) {
	app_csmsgf(doc->app, 
	    TEXT("Failed to write temporary TIFF file \042%s\042\n"),
	    tiffname);
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tiffname);
	return code;
    }

    /* Create temporary EPS file with updated headers */
    tpsfile = NULL;
    memset(tpsname, 0, sizeof(tpsname));
    if ((tpsfile = app_temp_gfile(doc->app, tpsname, 
	sizeof(tpsname)/sizeof(TCHAR))) == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't create temporary EPS file \042%s\042\n"),
	    tpsname);
	csunlink(tiffname);
	return -1;
    }
    gfile_close(tpsfile);

    code = copy_eps(doc, tpsname, bbox, hires_bbox, 
	DOSEPS_HEADER_SIZE, FALSE); 
    if (code) {
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tiffname);
	return -1;
    }


    if ( (tpsfile = gfile_open(tpsname, gfile_modeRead)) == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't open temporary EPS file \042%s\042\n"),
	    tpsname);
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tiffname);
	return -1;
    }

    /* Create DOS EPS output file */
    epsfile = gfile_open(epsname, gfile_modeWrite | gfile_modeCreate);
    if (epsfile == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't open output EPS file \042%s\042\n"),
	    epsname);
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tiffname);
	gfile_close(tpsfile);
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tpsname);
	return -1;
    }

    buffer = (char *)malloc(COPY_BUF_SIZE);
    if (buffer == (char *)NULL) {
	if (epsname[0]) {
	    gfile_close(epsfile);
	    if (!(debug & DEBUG_GENERAL))
		csunlink(epsname);
	}
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tiffname);
	gfile_close(tpsfile);
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tpsname);
	return -1;
    }

    if ((tiff_file = gfile_open(tiffname, gfile_modeRead)) == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't open temporary TIFF file \042%s\042\n"),
	    tiffname);
	free(buffer);
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tiffname);
	gfile_close(tpsfile);
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tpsname);
	if (epsname[0]) {
	    gfile_close(epsfile);
	    if (!(debug & DEBUG_GENERAL))
		csunlink(epsname);
	}
	return -1;
    }
    doseps.ps_length = (GSDWORD)gfile_get_length(tpsfile);
    doseps.wmf_begin = 0;
    doseps.wmf_length = 0;
    doseps.tiff_length = (GSDWORD)gfile_get_length(tiff_file);
    if (reverse) {
	doseps.tiff_begin = DOSEPS_HEADER_SIZE;
	doseps.ps_begin = doseps.tiff_begin + doseps.tiff_length;
    }
    else {
	doseps.ps_begin = DOSEPS_HEADER_SIZE;
	doseps.tiff_begin = doseps.ps_begin + doseps.ps_length;
    }
    doseps.checksum = 0xffff;
    write_doseps_header(&doseps, epsfile);

    gfile_seek(tpsfile, 0, gfile_begin);
    if (!reverse) {
        /* copy EPS file */
	while ((count = (int)gfile_read(tpsfile, buffer, COPY_BUF_SIZE)) != 0)
	    gfile_write(epsfile, buffer, count);
    }

    /* copy tiff file */
    gfile_seek(tiff_file, 0, gfile_begin);
    while ((count = (int)gfile_read(tiff_file, buffer, COPY_BUF_SIZE)) != 0)
	gfile_write(epsfile, buffer, count);

    if (reverse) {
        /* copy EPS file */
	while ((count = (int)gfile_read(tpsfile, buffer, COPY_BUF_SIZE)) != 0)
	    gfile_write(epsfile, buffer, count);
    }

    free(buffer);
    gfile_close(tiff_file);
    if (!(debug & DEBUG_GENERAL))
	csunlink(tiffname);
    gfile_close(tpsfile);
    if (!(debug & DEBUG_GENERAL))
	csunlink(tpsname);
    gfile_close(epsfile);
    return 0;
}

/*********************************************************/

static char hex[17] = "0123456789ABCDEF";

#define MAXHEXWIDTH 70

/* Write interchange preview to file f */
/* Does not copy the file itself */
int
write_interchange(GFile *f, IMAGE *img, CDSCBBOX devbbox)
{
    int i, j;
    unsigned char *preview;
    char buf[MAXSTR];
    const char *eol_str = EOLSTR;
    const char *endpreview_str = "%%EndPreview";
    BYTE *line;
    int preview_width, bwidth;
    int lines_per_scan;
    int topfirst = ((img->format & DISPLAY_FIRSTROW_MASK) == DISPLAY_TOPFIRST);
    char hexline[MAXHEXWIDTH+6];
    int hexcount = 0;
    unsigned int value;
    unsigned int depth = 8;
    
    validate_devbbox(img, &devbbox);

    switch (img->format & DISPLAY_COLORS_MASK) {
	case DISPLAY_COLORS_NATIVE:
	case DISPLAY_COLORS_GRAY:
	    if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_1)
		depth = 1;
    }

    switch (depth) {
	case 1:
	    /* byte width of interchange preview with 1 bit/pixel */
	    bwidth = (((devbbox.urx-devbbox.llx) + 7) & ~7) >> 3;
	    /* byte width of intermediate preview with 1 bit/pixel */
	    preview_width = ((img->width + 7) & ~7) >> 3;
	    break;
	case 8:
	    bwidth = devbbox.urx-devbbox.llx;
	    preview_width = img->width;
	    break;
	default:
	    return -1;
    }

    preview = (unsigned char *) malloc(preview_width);

    lines_per_scan = (bwidth + (MAXHEXWIDTH/2) - 1) / (MAXHEXWIDTH/2);
    buf[sizeof(buf)-1] = '\0';
    snprintf(buf, MAXSTR-1, "%%%%BeginPreview: %u %u %u %u%s",
	(devbbox.urx-devbbox.llx), (devbbox.ury-devbbox.lly), depth,
	(devbbox.ury-devbbox.lly)*lines_per_scan, eol_str);
    gfile_puts(f, buf);

    if (topfirst) 
	line = img->image + img->raster * (img->height - devbbox.ury);
    else
	line = img->image + img->raster * (devbbox.ury-1);

    /* process each line of bitmap */
    for (i = 0; i < (devbbox.ury-devbbox.lly); i++) {
	memset(preview,0xff,preview_width);
	if (depth == 1) {
	    image_to_mono(img, preview, line);
	    if (devbbox.llx)
		shift_preview(preview, preview_width, devbbox.llx);
	}
	else {
	    image_to_grey(img, preview, line);
	    if (devbbox.llx)
		memmove(preview, preview+devbbox.llx, preview_width);
	}
	hexcount = 0;
	hexline[hexcount++] = '%';
	hexline[hexcount++] = ' ';
	for (j=0; j<bwidth; j++) {
	    if (hexcount >= 2 + MAXHEXWIDTH) {
		if (strlen(eol_str) <= sizeof(hexline)-hexcount) {
		    memcpy(hexline+hexcount, eol_str, strlen(eol_str));
		    hexcount += (int)strlen(eol_str);
		}
		gfile_write(f, hexline, hexcount);
		hexcount = 0;
		hexline[hexcount++] = '%';
		hexline[hexcount++] = ' ';
	    }
	    value = preview[j];
	    if (depth == 8)
		value = 255 - value;
	    hexline[hexcount++] = hex[(value>>4)&15];
	    hexline[hexcount++] = hex[value&15];
	}
	if (hexcount && (strlen(eol_str) <= sizeof(hexline)-hexcount)) {
	    memcpy(hexline+hexcount, eol_str, strlen(eol_str));
	    hexcount += (int)strlen(eol_str);
	}
	if (hexcount)
	    gfile_write(f, hexline, hexcount);

	if (topfirst)
	    line += img->raster;
	else
	    line -= img->raster;

    }

    
    gfile_puts(f, endpreview_str);
    gfile_puts(f, eol_str);
    free(preview);

    return 0;
}

/*********************************************************/

typedef enum PREVIEW_TYPE_e {
    PREVIEW_UNKNOWN = 0,
    PREVIEW_TIFF = 1,
    PREVIEW_WMF = 2
} PREVIEW_TYPE;

/* Make a DOS EPS file from a PS file and a user supplied preview.
 * Preview may be WMF or TIFF.
 * Returns 0 on success.
 */
int
make_eps_user(Doc *doc, LPCTSTR preview_name, BOOL reverse, LPCTSTR epsname)
{
GFile *epsfile;
GFile *preview_file;
unsigned long preview_length;
unsigned char id[4];
PREVIEW_TYPE type = PREVIEW_UNKNOWN;
CDSCDOSEPS doseps;
char *buffer;
unsigned int count;
GFile *tpsfile;
TCHAR tpsname[MAXSTR];

    if ((preview_name == NULL) || preview_name[0] == '\0')
	return -1;
    if (doc->dsc == NULL)
	return -1;

    /* open preview, determine length and type */
    preview_file = gfile_open(preview_name, gfile_modeRead);
    if (preview_file == (GFile *)NULL) {
	app_csmsgf(doc->app, TEXT("Can't open preview file \042%s\042\n"),
	    preview_name);
	return -1;
    }

    /* Determine type of preview */
    gfile_read(preview_file, id, 4);
    preview_length = (GSDWORD)gfile_get_length(preview_file);
    gfile_seek(preview_file, 0, gfile_begin);

    if ((id[0] == 'I') && (id[1] == 'I'))
	type = PREVIEW_TIFF;
    if ((id[0] == 'M') && (id[1] == 'M'))
	type = PREVIEW_TIFF;
    if ((id[0] == 0x01) && (id[1] == 0x00) && 
	(id[2] == 0x09) && (id[3] == 0x00))
	type = PREVIEW_WMF;
    if ((id[0] == 0xd7) && (id[1] == 0xcd) && 
	(id[2] == 0xc6) && (id[3] == 0x9a)) {
	type = PREVIEW_WMF;
	preview_length -= 22;	/* skip over placeable metafile header */
        gfile_seek(preview_file, 22, gfile_begin);
    }

    if (type == PREVIEW_UNKNOWN) {
	app_csmsgf(doc->app, 
	 TEXT("Preview file \042%s\042 is not TIFF or Windows Metafile\n"),
	 preview_name);
	gfile_close(preview_file);
	return -1;
    }

    /* Create temporary EPS file containing all that is needed
     * including updated header and trailer.
     */
    tpsfile = NULL;
    memset(tpsname, 0, sizeof(tpsname));
    if ((tpsfile = app_temp_gfile(doc->app, tpsname, 
	sizeof(tpsname)/sizeof(TCHAR))) == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't create temporary EPS file \042%s\042\n"),
	    tpsname);
	gfile_close(preview_file);
	return -1;
    }
    gfile_close(tpsfile);

    if (copy_eps(doc, tpsname, doc->dsc->bbox, doc->dsc->hires_bbox, 
	DOSEPS_HEADER_SIZE, FALSE) < 0)
	return -1; 


    if ( (tpsfile = gfile_open(tpsname, gfile_modeRead)) == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't open temporary EPS file \042%s\042\n"),
	    tpsname);
	gfile_close(preview_file);
	return -1;
    }


    /* Create EPS output file */
    epsfile = gfile_open(epsname, gfile_modeWrite | gfile_modeCreate);

    if (epsfile == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't open EPS output file \042%s\042\n"),
	    epsname);
	gfile_close(preview_file);
	gfile_close(tpsfile);
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tpsname);
	return -1;
    }

    /* write DOS EPS binary header */
    doseps.ps_length = (GSDWORD)gfile_get_length(tpsfile);
    if (type == PREVIEW_WMF) {
	doseps.wmf_length = preview_length;
	doseps.tiff_begin = 0;
	doseps.tiff_length = 0;
	if (reverse) {
	    doseps.wmf_begin = DOSEPS_HEADER_SIZE;
	    doseps.ps_begin = doseps.wmf_begin + doseps.wmf_length;
	}
	else {
	    doseps.ps_begin = DOSEPS_HEADER_SIZE;
	    doseps.wmf_begin = doseps.ps_begin + doseps.ps_length;
	}
    }
    else {
	doseps.wmf_begin = 0;
	doseps.wmf_length = 0;
	doseps.tiff_length = preview_length;
	if (reverse) {
	    doseps.tiff_begin = DOSEPS_HEADER_SIZE;
	    doseps.ps_begin = doseps.tiff_begin + doseps.tiff_length;
	}
	else {
	    doseps.ps_begin = DOSEPS_HEADER_SIZE;
	    doseps.tiff_begin = doseps.ps_begin + doseps.ps_length;
	}
    }
    doseps.checksum = 0xffff;
    write_doseps_header(&doseps, epsfile);

    buffer = (char *)malloc(COPY_BUF_SIZE);
    if (buffer == (char *)NULL) {
	app_csmsgf(doc->app, TEXT("Out of memory in make_eps_user\n"));
	if (epsname[0]) {
	    gfile_close(epsfile);
	    if (!(debug & DEBUG_GENERAL))
		csunlink(epsname);
	}
	gfile_close(preview_file);
	gfile_close(tpsfile);
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tpsname);
	return -1;
    }

    gfile_seek(tpsfile, 0, gfile_begin);
    if (!reverse) {
	/* copy EPS file */
	while ((count = (int)gfile_read(tpsfile, buffer, COPY_BUF_SIZE)) != 0)
	    gfile_write(epsfile, buffer, count);
    }
    
    /* copy preview file */
    while ((count = (int)gfile_read(preview_file, buffer, COPY_BUF_SIZE)) != 0)
	gfile_write(epsfile, buffer, count);

    if (reverse) {
	/* copy EPS file */
	while ((count = (int)gfile_read(tpsfile, buffer, COPY_BUF_SIZE)) != 0)
	    gfile_write(epsfile, buffer, count);
    }

    free(buffer);
    gfile_close(tpsfile);
    if (!(debug & DEBUG_GENERAL))
	csunlink(tpsname);
    gfile_close(preview_file);
    gfile_close(epsfile);
    return 0; /* success */
}


/*********************************************************/

typedef struct tagMFH {
    WORD type;
    WORD headersize;
    WORD version;
    DWORD size;
    WORD nobj;
    DWORD maxrec;
    WORD noparam;
} MFH;

int write_metafile(GFile *f, IMAGE *img, CDSCBBOX devbbox, 
    float xdpi, float ydpi, MFH *mf);
static int metafile_init(IMAGE *img, CDSCBBOX *pdevbbox, MFH* mf);

/* A metafile object must not be larger than 64k */
/* Metafile bitmap object contains metafile header, */
/* bitmap header, palette and bitmap bits */
#define MAX_METAFILE_BITMAP 64000L	/* max size of bitmap bits */

static int
metafile_init(IMAGE *img, CDSCBBOX *pdevbbox, MFH* mf)
{
int wx, wy;
int ny, nylast;
int complete, partial;
int bytewidth;
int palcount;
unsigned long size;
int depth = image_depth(img);

    switch (depth) {
	case 1:
	case 4:
	case 8:
	case 24:
	    break;
	default:
	    /* unsupported format */
	    return -1;
    }

    validate_devbbox(img, pdevbbox);

    wx = pdevbbox->urx - pdevbbox->llx;
    wy = pdevbbox->ury - pdevbbox->lly;
    bytewidth = (( wx * depth + 31) & ~31) >> 3;
    ny = (int)(MAX_METAFILE_BITMAP / bytewidth);
    if (depth == 24)
	palcount = 0;
    else
	palcount = 1<<depth;

    complete = wy / ny;
    nylast = wy % ny;
    partial = nylast ? 1 : 0;
    
    mf->type = 1;		/* metafile in file */
    mf->headersize = 9;		/* 9 WORDs */
    mf->version = 0x300;	/* Windows 3.0 */
    mf->size = 			/* sizes in WORDs */
	9UL +			/* header */
	5 +			/* SetWindowOrg */
	5; 			/* SetWindowExt */
    /* complete StretchDIBits */
    mf->size += 14*complete;
    size = (40L + palcount*4L + (unsigned long)ny*(unsigned long)bytewidth)/2L;
    mf->size += size * (unsigned long)complete;
    /* partial StretchDIBits */
    mf->size += 14*partial;
    size = (40L + palcount*4L + (unsigned long)nylast*(unsigned long)bytewidth)/2L;
    mf->size += size * (unsigned long)partial;
    mf->size += 3;			/* end marker */

    mf->nobj = 0;
    size = complete ? 
       (40L + palcount*4L + (unsigned long)ny*(unsigned long)bytewidth)/2L
     : (40L + palcount*4L + (unsigned long)nylast*(unsigned long)bytewidth)/2L;
    mf->maxrec = 14L + size;
    mf->noparam = 0;
    return 0;
}



/* convert the display bitmap to a metafile picture */
int
write_metafile(GFile *f, IMAGE *img, CDSCBBOX devbbox, 
    float xdpi, float ydpi, MFH *mf)
{
    int i;
    int wx;
    int ny, sy, dy, wy;
    BYTE *line;
    BYTE *line2;
    LPBITMAP2 pbmi;
    int bsize;
    int bitoffset;
    int bytewidth, activewidth;
    int palcount;
    unsigned long size;
    int depth = image_depth(img);
    int topfirst = ((img->format & DISPLAY_FIRSTROW_MASK) == DISPLAY_TOPFIRST);

    dy = 0;
    wx = devbbox.urx - devbbox.llx;
    sy = devbbox.lly;
    wy = devbbox.ury - devbbox.lly;
    bitoffset = (devbbox.llx * depth);
    bytewidth = (( wx * depth + 31) & ~31) >> 3;
    activewidth = (( wx * depth + 7) & ~7) >> 3;
    ny = (int)(MAX_METAFILE_BITMAP / bytewidth);
    if (depth == 24)
	palcount = 0;
    else
	palcount = 1<<depth;

    /* create and initialize a BITMAPINFO for the <64k bitmap */
    bsize = sizeof(BITMAP2) + palcount * RGB4_LENGTH; 
    pbmi = (LPBITMAP2)malloc(bsize);
    if (pbmi == NULL) {
       return -1;
    }
    memset((char *)pbmi, 0, bsize);
    make_bmp_info(pbmi, img, xdpi, ydpi);
    pbmi->biClrUsed = 0;		/* write out full palette */
    pbmi->biClrImportant = 0;

    line2 = (BYTE *)malloc(img->raster);
    if (line2 == (BYTE *)NULL)
       return -1;
    pbmi->biWidth = wx;

    if (topfirst) 
	line = img->image + img->raster * (img->height - devbbox.lly - 1);
    else
	line = img->image + img->raster * devbbox.lly;

    /* write metafile header */
    write_word(mf->type, f);
    write_word(mf->headersize, f);
    write_word(mf->version, f);
    write_dword(mf->size, f);
    write_word(mf->nobj, f);
    write_dword(mf->maxrec, f);
    write_word(mf->noparam, f);

    /* write SetWindowOrg */
    write_dword(5, f);
    write_word(0x20b, f);
    write_word(0, f);
    write_word(0, f);

    /* write SetWindowExt */
    write_dword(5, f);
    write_word(0x20c, f);
    write_word((WORD)wy, f);
    write_word((WORD)wx, f);

    /* copy in chunks < 64k */
    for ( ; wy > ny; dy += ny, wy -= ny, sy += ny ) {
	pbmi->biHeight = ny;

	size = BITMAP2_LENGTH + (palcount * RGB4_LENGTH) 
		+ (unsigned long)bytewidth * (unsigned long)ny;
	/* write StretchDIBits header */
	write_dword(14 + size/2L, f);
	write_word(0x0f43, f);
	write_dword(0x00cc0020L, f);	/* SRC_COPY */
	write_word(0, f);			/* DIB_RGB_COLORS */
	write_word((WORD)ny, f);		/* Source cy */
	write_word((WORD)wx, f);		/* Source cx */
	write_word(0, f);			/* Source y */
	write_word(0, f);			/* Source x */
	write_word((WORD)ny, f);		/* Dest   cy */
	write_word((WORD)wx, f);		/* Dest   cx */
	write_word((WORD)(devbbox.ury - devbbox.lly - ny - dy), f); 
						/* Dest   y */
	write_word(0, f);			/* Dest   x */

	/* write bitmap header */
	write_bitmap_info(img, pbmi, f);

	/* write bitmap rows */
	for (i=0; i<ny; i++) {
	    if (depth == 24)
		image_to_24BGR(img, line2, line);
	    else
		memmove(line2, line, img->raster);
	    shift_preview(line2, img->raster, bitoffset);
	    if (activewidth < bytewidth)
		memset(line2+activewidth, 0xff, bytewidth-activewidth);
	    gfile_write(f, line2, bytewidth);
	    if (topfirst)
		line -= img->raster;
	    else
		line += img->raster;
	}
	
    }

    /* write StretchDIBits header */
    pbmi->biHeight = wy;
    size = BITMAP2_LENGTH + (palcount * RGB4_LENGTH) + 
	(unsigned long)bytewidth * (unsigned long)wy;
    write_dword(14 + size/2L, f);
    write_word(0x0f43, f);
    write_dword(0x00cc0020L, f);	/* SRC_COPY */
    write_word(0, f);		/* DIB_RGB_COLORS */
    write_word((WORD)wy, f);	/* Source cy */
    write_word((WORD)wx, f);	/* Source cx */
    write_word(0, f);		/* Source y */
    write_word(0, f);		/* Source x */
    write_word((WORD)wy, f);	/* Dest   cy */
    write_word((WORD)wx, f);	/* Dest   cx */
    write_word((WORD)(devbbox.ury - devbbox.lly - wy - dy), f);	/* Dest   y */
    write_word(0, f);		/* Dest   x */

    /* write bitmap header */
    write_bitmap_info(img, pbmi, f);

    /* copy last chunk */
    for (i=0; i<wy; i++) {
	if (depth == 24)
	    image_to_24BGR(img, line2, line);
	else
	    memmove(line2, line, img->raster);
	shift_preview(line2, img->raster, bitoffset);
	if (activewidth < bytewidth)
	    memset(line2+activewidth, 0xff, bytewidth-activewidth);
	gfile_write(f, line2, bytewidth);
	if (topfirst)
	    line -= img->raster;
	else
	    line += img->raster;
    }

    /* write end marker */
    write_dword(3, f);
    write_word(0, f);

    free((char *)pbmi);
    free(line2);

    return 0;
}


/*********************************************************/

/* Copy a DSC section, removing existing bounding boxes */
void
copy_nobbox(GFile *outfile, GFile *infile, 
    FILE_POS begin, FILE_POS end)
{
    const char bbox_str[] = "%%BoundingBox:";
    const char hiresbbox_str[] = "%%HiResBoundingBox:";
    char buf[DSC_LINE_LENGTH+1];
    int len;
    gfile_seek(infile, begin, gfile_begin);
    begin = gfile_get_position(infile);
    while (begin < end) {
	len = ps_fgets(buf, min(sizeof(buf)-1, end-begin), infile);
        begin = gfile_get_position(infile);
	if (len == 0) {
	    return;	/* EOF on input file */
	}
	else if (strncmp(buf, bbox_str, strlen(bbox_str)) == 0) {
	    /* skip it */
	}
	else if (strncmp(buf, hiresbbox_str, strlen(hiresbbox_str)) == 0) {
	    /* skip it */
	}
	else
	    gfile_write(outfile, buf, len);
    }
}

/* Copy a DSC header, removing existing bounding boxes 
 * and adding new ones.
 */
void
copy_bbox_header(GFile *outfile, 
    GFile *infile, FILE_POS begin, FILE_POS end,
    CDSCBBOX *bbox, CDSCFBBOX *hiresbbox)
{
    char buf[DSC_LINE_LENGTH+1];
    int len;

    memset(buf, 0, sizeof(buf)-1);
    gfile_seek(infile, begin, gfile_begin);
    len = ps_fgets(buf, min(sizeof(buf)-1, end-begin), infile);
    if (len)
	gfile_write(outfile, buf, len);	/* copy version line */
    /* Add bounding box lines */
    if (bbox) {
	snprintf(buf, sizeof(buf)-1, "%%%%BoundingBox: %d %d %d %d\n",
	    bbox->llx, bbox->lly, bbox->urx, bbox->ury);
	gfile_puts(outfile, buf);
    }
    if (hiresbbox) {
	snprintf(buf, sizeof(buf)-1, "%%%%HiResBoundingBox: %.3f %.3f %.3f %.3f\n",
	    hiresbbox->fllx, hiresbbox->flly, 
	    hiresbbox->furx, hiresbbox->fury);
	gfile_puts(outfile, buf);
    }

    begin = gfile_get_position(infile);
    copy_nobbox(outfile, infile, begin, end);
}

/* return the length of the line less the EOL characters */
static int
without_eol(const char *str, int length)
{
    int j;
    for (j=length-1; j>=0; j--) {
	if (!((str[j] == '\r') || (str[j] == '\n'))) {
	    j++;
	    break;
	}
    }
    if (j < 0)
	j = 0;
    return j;
}

static BOOL is_process_colour(const char *name)
{
    return ( 
	(dsc_stricmp(name, "Cyan")==0) ||
    	(dsc_stricmp(name, "Magenta")==0) ||
    	(dsc_stricmp(name, "Yellow")==0) ||
    	(dsc_stricmp(name, "Black")==0)
	);
}

static const char process_str[] = "%%DocumentProcessColors:";
static const char custom_str[] = "%%DocumentCustomColors:";
static const char cmyk_custom_str[] = "%%CMYKCustomColor:";
static const char rgb_custom_str[] = "%%RGBCustomColor:";
static const char eol_str[] = EOLSTR;

static const char *
separation_name(RENAME_SEPARATION *rs, const char *name)
{
    RENAME_SEPARATION *s;
    const char *newname = name;
    for (s=rs; s; s=s->next) {
	if (strcmp(s->oldname, name) == 0) {
	    newname = s->newname;
	    break;
	}
    }
    return newname;
}

/*
 * Set renamed to the names of the renamed separations.
 * renamed[] must be as long as dsc->page_count.
 * Return 0 on success, or -1 if a separation name appears twice.
 */
int
rename_separations(CDSC *dsc, RENAME_SEPARATION *rs, const char *renamed[])
{
    int duplicated = 0;
    int i, j;
    const char *sepname;
    renamed[0] = dsc->page[0].label;
    for (i=1; i<(int)dsc->page_count; i++) {
	sepname = separation_name(rs, dsc->page[i].label);
	/* Check if this is a duplicate separation, in which case
	 * we don't want to rename it twice.
	 */
	for (j=1; j<i; j++)
	    if (strcmp(dsc->page[j].label, dsc->page[i].label) == 0) {
		sepname = NULL;	/* don't do it */
		duplicated = 1;
	    }
	/* If the separation name already exists in the renamed
	 * list, don't rename it */ 
	if (sepname) {
	    for (j=1; j<i; j++) {
	        if (strcmp(renamed[j], sepname) == 0) {
		    sepname = NULL;	/* don't do it */
		    duplicated = 1;
		    break;
		}
	    }
	}
	if (sepname == NULL)
	    sepname = dsc->page[i].label;
	renamed[i] = sepname;
    }
    if (duplicated)
	return -1;	/* clash in separation names */
    return 0;
}

/* Find separation begin and end */
static int
dcs2_separation(CDSC *dsc, int pagenum, 
    const char **fname, FILE_POS *pbegin, FILE_POS *pend)
{
    GFile *f;
    TCHAR wfname[MAXSTR];
    *fname = dsc_find_platefile(dsc, pagenum);
    if (*fname) {
	narrow_to_cs(wfname, (int)sizeof(wfname), *fname, 
	    (int)strlen(*fname)+1);
	if ((f = gfile_open(wfname, gfile_modeRead)) != (GFile *)NULL) {
	    *pbegin = 0;
	    *pend = gfile_get_length(f);
	    gfile_close(f);
	}
	else {
	    /* Separation file didn't exist */
	    *pbegin = *pend = 0;
	    return 1;
	}
    }
    else {
	*pbegin = dsc->page[pagenum].begin;
	*pend = dsc->page[pagenum].end;
    }
    return 0;
}


static int 
fix_custom(Doc *doc, char *buf, int buflen, const char *renamed[])
{
    /* Create a new %%DocumentCustomColors:
     * containing only those separations that exist
     */
    CDSC *dsc = doc->dsc;
    int i;
    int missing;
    int n = 0;
    FILE_POS begin, end;
    const char *fname;
    int count = min((int)strlen(buf), buflen);
    if ((dsc == NULL) || !dsc->dcs2)
	return 0;
    for (i=1; i<(int)dsc->page_count; i++) {
	const char *sepname = renamed[i];
	missing = dcs2_separation(dsc, i, &fname, &begin, &end);
	if (!missing && !is_process_colour(sepname)) {
	    n++;
	    strncpy(buf+count, " (", buflen-count-1);
	    count = (int)strlen(buf);
	    strncpy(buf+count, sepname, buflen-count-1);
	    count = (int)strlen(buf);
	    strncpy(buf+count, ")", buflen-count-1);
	    count = (int)strlen(buf);
	}
    }
    return n;
}

static int
fix_process(Doc *doc, char *buf, int buflen, const char *renamed[])
{
    /* Create a new %%DocumentProcessColors:
     * containing only those separations that exist
     */
    CDSC *dsc = doc->dsc;
    int i;
    int n = 0;
    int missing;
    FILE_POS begin, end;
    const char *fname;
    int count = min((int)strlen(buf), buflen);
    if ((dsc == NULL) || !dsc->dcs2)
	return 0;
    for (i=1; i<(int)dsc->page_count; i++) {
	const char *sepname = renamed[i];
	missing = dcs2_separation(dsc, i, &fname, &begin, &end);
	if (!missing && is_process_colour(sepname)) {
	    n++;
	    strncpy(buf+count, " ", buflen-count-1);
	    count++;
	    strncpy(buf+count, sepname, buflen-count-1);
	    count = (int)strlen(buf);
	}
    }
    return n;
}

/* Write out %%CMYKCustomColor line and extensions.
 * count is the number of lines already written.
 */
static int 
write_cmyk_custom(GFile *gf, Doc *doc, const char *renamed[], int count)
{
    /* Create a new %%CMYKCustomColor:
     * containing only those colours that exist
     */
    CDSC *dsc = doc->dsc;
    CDSCCOLOUR *colour;
    int i;
    int missing;
    int n = count;
    FILE_POS begin, end;
    const char *fname;
    char buf[MAXSTR];
    if ((dsc == NULL) || !dsc->dcs2)
	return 0;
    buf[sizeof(buf)-1] = '\0';
    for (i=1; i<(int)dsc->page_count; i++) {
	const char *sepname = renamed[i];
	missing = dcs2_separation(dsc, i, &fname, &begin, &end);
	if (!missing && !is_process_colour(sepname)) {
	    /* find colour values */
	    for (colour=dsc->colours; colour; colour=colour->next) {
		if (strcmp(colour->name, dsc->page[i].label)==0)
		    break;
	    }
	    if ((colour != NULL) && 
		(colour->custom == CDSC_CUSTOM_COLOUR_CMYK)) {
		if (n == 0)
		    strncpy(buf, cmyk_custom_str, sizeof(buf)-1);
		else
		    strncpy(buf, "%%+", sizeof(buf)-1);
		snprintf(buf+strlen(buf), sizeof(buf)-1-strlen(buf), 
		    " %g %g %g %g (%s)",
		    colour->cyan, colour->magenta, colour->yellow, 
		    colour->black, sepname);
		gfile_puts(gf, buf);
		gfile_puts(gf, eol_str);
		n++;
	    }
	}
    }
    return n;
}

/* Write out %%RGBCustomColor line and extensions.
 * count is the number of lines already written.
 */
static int 
write_rgb_custom(GFile *gf, Doc *doc, const char *renamed[], int count)
{
    /* Create a new %%RGBCustomColor:
     * containing only those colours that exist
     */
    CDSC *dsc = doc->dsc;
    CDSCCOLOUR *colour;
    int i;
    int missing;
    int n = count;
    FILE_POS begin, end;
    const char *fname;
    char buf[MAXSTR];
    if ((dsc == NULL) || !dsc->dcs2)
	return 0;
    buf[sizeof(buf)-1] = '\0';
    for (i=1; i<(int)dsc->page_count; i++) {
	const char *sepname = renamed[i];
	missing = dcs2_separation(dsc, i, &fname, &begin, &end);
	if (!missing && !is_process_colour(sepname)) {
	    /* find colour values */
	    for (colour=dsc->colours; colour; colour=colour->next) {
		if (strcmp(colour->name, dsc->page[i].label)==0)
		    break;
	    }
	    if ((colour != NULL) && 
		(colour->custom == CDSC_CUSTOM_COLOUR_RGB)) {
		if (n == 0)
		    strncpy(buf, cmyk_custom_str, sizeof(buf)-1);
		else
		    strncpy(buf, "%%+", sizeof(buf)-1);
		snprintf(buf+strlen(buf), sizeof(buf)-1-strlen(buf), 
		    " %g %g %g (%s)",
		    colour->red, colour->green, colour->blue,
		    sepname);
		gfile_puts(gf, buf);
		gfile_puts(gf, eol_str);
		n++;
	    }
	}
    }
    return n;
}

/* Write out DCS2 platefile comments, and any multiple file separations */
static FILE_POS
write_platefile_comments(Doc *doc, GFile *docfile, 
    GFile *epsfile, LPCTSTR epsname,
    int offset, FILE_POS file_offset, 
    BOOL dcs2_multi, BOOL write_all, BOOL missing_separations,
    const char *renamed[], BOOL some_renamed)
{
    int i;
    CDSC *dsc = doc->dsc;
    FILE_POS begin, end;
    FILE_POS len;
    char platename[MAXSTR];
    char outbuf[MAXSTR];
    GFile *f;
    FILE_POS offset2 = 0;

    /* Now write the platefile comments */
    for (i=1; i<(int)dsc->page_count; i++) {
	/* First find length of separation */
	int missing;
	const char *fname = NULL;
	const char *sepname = renamed[i];
	missing = dcs2_separation(dsc, i, &fname, &begin, &end);
	len = end - begin;
	/* Now write out platefile */
	if (missing && (missing_separations || some_renamed)) {
	    if (debug & DEBUG_GENERAL)
		app_msgf(doc->app, 
		    "Skipping missing separation page %d \042%s\042\n",
		    i, dsc->page[i].label);
	}
	else if (dcs2_multi) {
	    int j;
	    int duplicate = 0;
	    memset(platename, 0, sizeof(platename));
	    cs_to_narrow(platename, sizeof(platename)-1, epsname, 
		(int)cslen(epsname)+1);
	    strncat(platename, ".", sizeof(platename) - strlen(platename));
	    strncat(platename, sepname, sizeof(platename) - strlen(platename));
	    for (j=1; j<i; j++) {
		/* Check if separation name is a duplicate */ 
		if (strcmp(sepname, renamed[j]) == 0)
		    duplicate = 1;
	    }
	    if (duplicate) /* append page number to make the filename unique */
		snprintf(platename+strlen(platename), 
		    sizeof(platename) - strlen(platename),
		    ".%d", i);
	    snprintf(outbuf, sizeof(outbuf)-1,  
		"%%%%PlateFile: (%s) EPS Local %s",
		sepname, platename);
	    gfile_puts(epsfile, outbuf);
	    gfile_puts(epsfile, eol_str);	/* change EOL */
	    if (write_all) {
		/* Write out multi file separations */
	        TCHAR wfname[MAXSTR];
		GFile *pf;
		narrow_to_cs(wfname, (int)sizeof(wfname), platename, 
		    (int)strlen(platename)+1);
		pf = gfile_open(wfname, gfile_modeWrite | gfile_modeCreate);
		if (pf != (GFile *)NULL) {
		    const char *fname = dsc_find_platefile(dsc, i);
		    if (fname) {
			narrow_to_cs(wfname, (int)sizeof(wfname), 
			    fname, (int)strlen(fname)+1);
			if ((f = gfile_open(wfname, gfile_modeRead)) 
			    != (GFile *)NULL) {
			    ps_copy(pf, f, begin, end);
			    gfile_close(f);
			}
		    }
		    else {
			ps_copy(pf, docfile, begin, end);
		    }
		    gfile_close(pf);
		}
		else {
		    return (FILE_POS)-1;	/* error */
		}
	    }
	}
	else {
	    char fmtbuf[MAXSTR];
	    snprintf(fmtbuf, sizeof(fmtbuf)-1,  
	        "%%%%%%%%PlateFile: (%%s) EPS #%%010%s %%010%s",
		DSC_OFFSET_FORMAT, DSC_OFFSET_FORMAT);
	    snprintf(outbuf, sizeof(outbuf)-1, fmtbuf,
		sepname, offset+file_offset+offset2, len);
	    gfile_puts(epsfile, outbuf);
	    gfile_puts(epsfile, eol_str);	/* change EOL */
	    offset2 += len;
	}
    }
    return offset2;
}

/* Write out DCS2 single file separations */
static FILE_POS
write_singlefile_separations(Doc *doc, GFile *docfile, 
    GFile *epsfile, 
    BOOL dcs2_multi, BOOL write_all, BOOL missing_separations,
    BOOL some_renamed)
{
    int i;
    CDSC *dsc = doc->dsc;
    FILE_POS begin, end;
    FILE_POS len;
    GFile *f;
    if (write_all && !dcs2_multi) {
	for (i=1; i<(int)dsc->page_count; i++) {
	    int missing;
	    const char *fname = NULL;
	    missing = dcs2_separation(dsc, i, &fname, &begin, &end);
	    len = end - begin;
	    if (missing && (missing_separations || some_renamed)) {
		if (debug & DEBUG_GENERAL)
		    app_msgf(doc->app, 
			"Skipping missing separation page %d \042%s\042\n",
			i, dsc->page[i].label);
	    }
	    else if (fname) {
	        TCHAR wfname[MAXSTR];
		narrow_to_cs(wfname, (int)sizeof(wfname), fname, 
		    (int)strlen(fname)+1);
		if ((f = gfile_open(wfname, gfile_modeRead)) != (GFile *)NULL) {
		    begin = 0;
		    end = gfile_get_length(f);
		    if (debug & DEBUG_GENERAL)
			app_msgf(doc->app, 
			    "Copying page %d from \042%s\042  %ld %ld\n",
			    i, fname, begin, end);
		    ps_copy(epsfile, f, begin, end);
		    gfile_close(f);
		}
	    }
	    else {
		begin = dsc->page[i].begin;
		end = dsc->page[i].end;
		if (debug & DEBUG_GENERAL)
		    app_msgf(doc->app, 
			"Copying page %d  %ld %ld\n",
			i, fname, begin, end);
		ps_copy(epsfile, docfile, begin, end);
	    }
	}
    }
    return 0;
}

/* Check that two DCS2 files can be combined */
static int 
check_dcs2_combine(Doc *doc1, Doc *doc2, 
    const char **renamed1, const char **renamed2, int tolerance)
{
    CDSC *dsc1;
    CDSC *dsc2;
    const char *sepname1;
    const char *sepname2;
    int i, j;

    if (doc1 == NULL)
	return -1;
    if (doc2 == NULL)
	return -1;
    dsc1 = doc1->dsc;
    dsc2 = doc2->dsc;
    if (!dsc1->dcs2)
	return -1;
    if (!dsc2->dcs2)
	return -1;
    if (dsc1->bbox == NULL)
	return -1;
    if (dsc2->bbox == NULL)
	return -1;
    /* Check that bounding boxes match */
    if ((dsc1->bbox->llx > dsc2->bbox->llx + tolerance) ||
        (dsc1->bbox->llx < dsc2->bbox->llx - tolerance) ||
        (dsc1->bbox->lly > dsc2->bbox->lly + tolerance) ||
        (dsc1->bbox->lly < dsc2->bbox->lly - tolerance) ||
        (dsc1->bbox->urx > dsc2->bbox->urx + tolerance) ||
        (dsc1->bbox->urx < dsc2->bbox->urx - tolerance) ||
        (dsc1->bbox->ury > dsc2->bbox->ury + tolerance) ||
        (dsc1->bbox->ury < dsc2->bbox->ury - tolerance)) {
	app_msgf(doc1->app, "Bounding Boxes don't match\n");
	return -1;
    }
    /* Check that separations don't conflict */
    for (i=1; i<(int)dsc1->page_count; i++) {
 	sepname1 = renamed1[i];
	for (j=1; i<(int)dsc2->page_count; i++) {
 	    sepname2 = renamed2[j];
	    if (strcmp(sepname1, sepname2) == 0) {
		app_msgf(doc1->app, 
		    "Separation \042%s\042 appears in both files\n",
		    sepname1);
		return -1;
	    }
	}
    }
    return 0;
}

/* Copy a DCS 2.0 file.
 * DSC 2.0 as single file looks like concatenated EPS files.
 * DSC parser treats these as separate pages and the
 * entire first EPS file is contained in the first page.
 * That is, there is no header, prolog or trailer.
 * Don't update the bounding box.
 * Do update the %%PlateFile comments.
 * If missing_separations is true, remove the names of missing
 * separations from the DSC comments.
 * The length of the composite page is returned in complen.
 * If composite is not NULL, use this file as the new composite page
 * with the existing header.
 * If rs is not NULL, rename some separations.
 * If doc2 is not NULL, add its separations.  It is an error if
 * separations are duplicated or the bounding box differs by more
 * than tolerance points.
 */
int
copy_dcs2(Doc *doc, GFile *docfile, 
    Doc *doc2, GFile *docfile2,
    GFile *epsfile, LPCTSTR epsname,
    int offset, BOOL dcs2_multi, BOOL write_all, BOOL missing_separations,
    FILE_POS *complen, GFile *composite, RENAME_SEPARATION *rs,
    int tolerance)
{
    const char platefile_str[] = "%%PlateFile:";
    const char cyanplate_str[] = "%%CyanPlate:";
    const char magentaplate_str[] = "%%MagentaPlate:";
    const char yellowplate_str[] = "%%YellowPlate:";
    const char blackplate_str[] = "%%BlackPlate:";
    const char endcomments_str[] = "%%EndComments";
    char buf[DSC_LINE_LENGTH+1];
    char outbuf[MAXSTR];
    CDSC *dsc = doc->dsc;
    FILE_POS len;
    FILE_POS file_offset = *complen;
    FILE_POS begin, end;
    FILE_POS header_position;
    BOOL found_endheader = FALSE;
    BOOL ignore_continuation = FALSE;
    int count;
    const char **renamed1 = NULL;
    const char **renamed2 = NULL;
    
    if (dsc->page_count == 0)
	return -1;

    /* Get renamed separations */
    renamed1 = (const char **)malloc(sizeof(const char *) * dsc->page_count);
    if (renamed1 == NULL)
	return -1;
    if (rename_separations(dsc, rs, renamed1) != 0) {
	free((void *)renamed1);
	return -1;
    }
    if (doc2) {
	renamed2 = (const char **)
	    malloc(sizeof(const char *) * doc2->dsc->page_count);
	if (renamed2 == NULL) {
	    free((void *)renamed1);
	    return -1;
	}
	if (rename_separations(doc2->dsc, rs, renamed2) != 0) {
	    free((void *)renamed1);
	    free((void *)renamed2);
	    return -1;
	}
    }

    if (doc2 && check_dcs2_combine(doc, doc2, renamed1, renamed2, tolerance)) {
	free((void *)renamed1);
	free((void *)renamed2);
	return -1;
    }

    gfile_seek(docfile, dsc->page[0].begin, gfile_begin);
    memset(buf, 0, sizeof(buf)-1);
    count = ps_fgets(buf, 
	min(sizeof(buf)-1, dsc->page[0].end - dsc->page[0].begin), docfile);
    header_position = gfile_get_position(docfile);
    if (count) { 
	/* copy version line */
	gfile_write(epsfile, buf, without_eol(buf, count));
	gfile_puts(epsfile, eol_str); 	/* change EOL */
    }
    while ((count = ps_fgets(buf, 
	min(sizeof(buf)-1, dsc->page[0].end - header_position), 
	docfile)) != 0) {
	header_position = gfile_get_position(docfile);
	/* check if end of header */
        if (count < 2)
	    found_endheader = TRUE;
	if (buf[0] != '%')
	    found_endheader = TRUE;
	if ((buf[0] == '%') && ((buf[1] == ' ') || 
	     (buf[1] == '\t') || (buf[1] == '\r') || (buf[1] == '\n')))
	    found_endheader = TRUE;
	if (strncmp(buf, endcomments_str, strlen(endcomments_str)) == 0)
	    found_endheader = TRUE;
	if (strncmp(buf, "%%Begin", 7) == 0)
	    found_endheader = TRUE;
	if (found_endheader)
	    break;	/* write out count characters from buf later */
	if ((buf[0] == '%') && (buf[1] == '%') && (buf[2] == '+') &&
	    ignore_continuation)
	    continue;
	else
	    ignore_continuation = FALSE;
	if ((strncmp(buf, platefile_str, strlen(platefile_str)) != 0) &&
	     (strncmp(buf, cyanplate_str, strlen(cyanplate_str)) != 0) &&
	     (strncmp(buf, magentaplate_str, strlen(magentaplate_str)) != 0) &&
	     (strncmp(buf, yellowplate_str, strlen(yellowplate_str)) != 0) &&
	     (strncmp(buf, blackplate_str, strlen(blackplate_str)) != 0)) {
	    /* Write all header lines except for DCS plate lines */
	    if ((rs != NULL) || missing_separations || doc2) {
		/* but don't write custom/process colour lines */
	        if (strncmp(buf, custom_str, strlen(custom_str)) == 0) {
		    count = 0;
		    ignore_continuation = TRUE;
		}
		else if (strncmp(buf, process_str, strlen(process_str)) == 0) {
		    count = 0;
		    ignore_continuation = TRUE;
		}
		else if (strncmp(buf, cmyk_custom_str, 
		    strlen(cmyk_custom_str)) == 0) {
		    count = 0;
		    ignore_continuation = TRUE;
		}
		else if (strncmp(buf, rgb_custom_str, 
		    strlen(rgb_custom_str)) == 0) {
		    count = 0;
		    ignore_continuation = TRUE;
		}
	    }
	    if (count == 0)
		continue;
	    gfile_write(epsfile, buf, without_eol(buf, count));
	    gfile_puts(epsfile, eol_str);	/* change EOL */
	}
    }

    /* Now write DocumentProcessColors, DocumentCustomColors,
     * CMYKCustomColor and RGBCustomColor
     */
    if ((rs!=NULL) || missing_separations || doc2) {
	int n;	/* number of entries written */
        strncpy(outbuf, process_str, sizeof(outbuf));
	n = fix_process(doc, outbuf, sizeof(outbuf), renamed1);
	if (doc2)
	    n += fix_process(doc2, outbuf, sizeof(outbuf), renamed2);
	if (n) {
	    gfile_write(epsfile, outbuf, 
		without_eol(outbuf, (int)strlen(outbuf)));
	    gfile_puts(epsfile, eol_str);	/* change EOL */
	}
        strncpy(outbuf, custom_str, sizeof(outbuf));
	n = fix_custom(doc, outbuf, sizeof(outbuf), renamed1);
	if (doc2)
	    n += fix_custom(doc2, outbuf, sizeof(outbuf), renamed2);
	if (n) {
	    gfile_write(epsfile, outbuf, 
		without_eol(outbuf, (int)strlen(outbuf)));
	    gfile_puts(epsfile, eol_str);	/* change EOL */
	}
	n = write_cmyk_custom(epsfile, doc, renamed1, 0);
	if (doc2)
	    write_cmyk_custom(epsfile, doc2, renamed2, n);
	n = write_rgb_custom(epsfile, doc, renamed1, 0);
	if (doc2)
	    write_rgb_custom(epsfile, doc2, renamed2, n);
    }


    /* Write out the platefile comments and any multiple file separations */
    len = write_platefile_comments(doc, docfile, epsfile, epsname, 
	offset, file_offset, 
	dcs2_multi, write_all, missing_separations, renamed1, (rs != NULL));
    if (len == (FILE_POS)-1) {
	free((void *)renamed1);
	if (renamed2)
	    free((void *)renamed2);
	return -1;
    }
    file_offset += len;
    if (doc2) {
	len = write_platefile_comments(doc2, docfile2, epsfile, epsname, 
	    offset, file_offset, 
	    dcs2_multi, write_all, missing_separations, renamed2, (rs != NULL));
	if (len == (FILE_POS)-1) {
	    free((void *)renamed1);
	    if (renamed2)
		free((void *)renamed2);
	    return -1;
	}
	file_offset += len;
    }

    /* Copy last line of header */
    if (found_endheader) {
	gfile_write(epsfile, buf, without_eol(buf, count));
	gfile_puts(epsfile, eol_str);	/* change EOL */
    }
    /* copy rest of composite */
    if (composite) {
	if (dsc->page_pages == 1) {
	    gfile_puts(epsfile, "%%Page: 1 1");
	    gfile_puts(epsfile, eol_str);	/* change EOL */
	}
	end = gfile_get_length(composite);
	gfile_seek(composite, 0, gfile_begin);
	snprintf(outbuf, sizeof(outbuf)-1, "%%BeginDocument: composite");
	gfile_puts(epsfile, "%%BeginDocument: composite");
	gfile_puts(epsfile, eol_str);	/* change EOL */
	ps_copy(epsfile, composite, 0, end);
	gfile_puts(epsfile, "%%EndDocument");
	gfile_puts(epsfile, eol_str);	/* change EOL */
	gfile_puts(epsfile, "%%Trailer");
	gfile_puts(epsfile, eol_str);	/* change EOL */
    }
    else {
	begin = header_position;
	end = dsc->page[0].end;
	ps_copy(epsfile, docfile, begin, end);
    }

    file_offset = gfile_get_position(epsfile);
    *complen = file_offset;

    /* Write out single file separations */
    write_singlefile_separations(doc, docfile, epsfile, 
        dcs2_multi, write_all, missing_separations, (rs != NULL));
    if (doc2)
        write_singlefile_separations(doc2, docfile2, epsfile, 
            dcs2_multi, write_all, missing_separations, (rs != NULL));
    free((void *)renamed1);
    if (renamed2)
	free((void *)renamed2);
    return 0;
}



/* Copy an EPS file.
 * %%BoundingBox and %%HiResBoundingBox will be brought to
 * the start of the header.
 * The new EPS file will have a prolog of "offset" bytes,
 * so update DCS 2.0 offsets accordingly.
 */
int
copy_eps(Doc *doc, LPCTSTR epsname, CDSCBBOX *bbox, CDSCFBBOX *hires_bbox, 
    int offset, BOOL dcs2_multi)
{
    GFile *docfile;
    GFile *f = NULL;
    CDSC *dsc = doc->dsc;
    int code = 0;

    docfile = gfile_open(doc_name(doc), gfile_modeRead);
    if (docfile == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't open document file \042%s\042\n"), doc_name(doc));
	return -1;
    }

    f = gfile_open(epsname, gfile_modeWrite | gfile_modeCreate);
    if (f == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't open EPS file \042%s\042\n"), epsname);
	gfile_close(docfile);
	return -1;
    }

    if (dsc->dcs2) {
	/* Write DCS 2.0, updating the %%PlateFile, but don't update
	 * %%BoundingBox.
	 */
	FILE_POS complen = 0;	/* length of composite page */
	/* Write once to calculate the offsets */
	code = copy_dcs2(doc, docfile, NULL, NULL, f, epsname, 
	    offset, dcs2_multi, FALSE, FALSE, &complen, NULL, NULL, 0);
	gfile_seek(docfile, 0, gfile_begin);
	gfile_close(f);
	f = NULL;
	if (code == 0) {
	    f = gfile_open(epsname, gfile_modeWrite | gfile_modeCreate);
	    if (f == (GFile *)NULL) {
		app_csmsgf(doc->app, 
		    TEXT("Can't open EPS file \042%s\042\n"), epsname);
		gfile_close(docfile);
		return -1;
	    }
	}
	if (code == 0) {
	    /* Then again with the correct offsets */
	    gfile_seek(f, 0, gfile_begin);
	    code = copy_dcs2(doc, docfile, NULL, NULL, f, epsname,
		offset, dcs2_multi, TRUE, FALSE, &complen, NULL, NULL, 0);
	}
    }
    else {
	/* Update the bounding box in the header and remove it from
	 * the trailer
	 */
	copy_bbox_header(f, docfile, 
	    dsc->begincomments, dsc->endcomments,
	    bbox, hires_bbox);
	ps_copy(f, docfile, dsc->begindefaults, dsc->enddefaults);
	ps_copy(f, docfile, dsc->beginprolog, dsc->endprolog);
	ps_copy(f, docfile, dsc->beginsetup, dsc->endsetup);
	if (dsc->page_count)
	    ps_copy(f, docfile, dsc->page[0].begin, dsc->page[0].end);
	copy_nobbox(f, docfile, dsc->begintrailer, dsc->endtrailer);
    }
    if (f)
	gfile_close(f);
    gfile_close(docfile);
    return code;
}



/*********************************************************/

/* make an EPSI file with an Interchange Preview */
/* from a PS file and a bitmap */
int
make_eps_interchange(Doc *doc, IMAGE *img, CDSCBBOX devbbox, 
    CDSCBBOX *bbox, CDSCFBBOX *hires_bbox, LPCTSTR epsname)
{
    GFile *epsfile;
    GFile *docfile;
    int code;
    CDSC *dsc = doc->dsc;

    if (dsc == NULL)
	return -1;

    if ((docfile = gfile_open(doc_name(doc), gfile_modeRead)) 
	== (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't open EPS file \042%s\042\n"),
	    doc_name(doc));
	return -1;
    }

    epsfile = gfile_open(epsname, gfile_modeWrite | gfile_modeCreate);

    if (epsfile == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't open output EPS file \042%s\042\n"),
	    epsname);
	gfile_close(docfile);
	return -1;
    }

    /* adjust %%BoundingBox: and %%HiResBoundingBox: comments */
    copy_bbox_header(epsfile, docfile, dsc->begincomments, dsc->endcomments, 
	bbox, hires_bbox);

    code = write_interchange(epsfile, img, devbbox);

    ps_copy(epsfile, docfile, dsc->begindefaults, dsc->enddefaults);
    ps_copy(epsfile, docfile, dsc->beginprolog, dsc->endprolog);
    ps_copy(epsfile, docfile, dsc->beginsetup, dsc->endsetup);
    if (dsc->page_count)
	ps_copy(epsfile, docfile, dsc->page[0].begin, dsc->page[0].end);
    copy_nobbox(epsfile, docfile, dsc->begintrailer, dsc->endtrailer);
    gfile_close(docfile);
    if (*epsname!='\0') {
	gfile_close(epsfile);
	if (code && (!(debug & DEBUG_GENERAL)))
	    csunlink(epsname);
    }
    return code;
}


/*********************************************************/


/* make a PC EPS file with a Windows Metafile Preview */
/* from a PS file and a bitmap */
int
make_eps_metafile(Doc *doc, IMAGE *img, CDSCBBOX devbbox, CDSCBBOX *bbox, 
    CDSCFBBOX *hires_bbox, float xdpi, float ydpi, BOOL reverse,
    LPCTSTR epsname)
{
    MFH mf;
    GFile *epsfile;
    GFile *tpsfile;
    TCHAR tpsname[MAXSTR];
    int code;
    int count;
    char *buffer;
    CDSCDOSEPS doseps;

    /* prepare metafile header and calculate length */
    code = metafile_init(img, &devbbox, &mf);

    /* Create temporary EPS file with updated headers */
    tpsfile = NULL;
    memset(tpsname, 0, sizeof(tpsname));
    if ((tpsfile = app_temp_gfile(doc->app, tpsname, 
	sizeof(tpsname)/sizeof(TCHAR))) == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't create temporary EPS file \042%s\042\n"),
	    tpsname);
	return -1;
    }
    gfile_close(tpsfile);

    code = copy_eps(doc, tpsname, bbox, hires_bbox, DOSEPS_HEADER_SIZE, FALSE); 
    if (code)
	return -1;

    if ( (tpsfile = gfile_open(tpsname, gfile_modeRead)) == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't open temporary EPS file \042%s\042\n"),
	    tpsname);
	return -1;
    }

    /* Create DOS EPS output file */
    epsfile = gfile_open(epsname, gfile_modeWrite | gfile_modeCreate);
    if (epsfile == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't open output EPS file \042%s\042\n"),
	    epsname);
	gfile_close(tpsfile);
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tpsname);
	return -1;
    }

    doseps.ps_length = (GSDWORD)gfile_get_length(tpsfile);
    doseps.wmf_length = mf.size * 2;
    doseps.tiff_begin = 0;
    doseps.tiff_length = 0;
    doseps.checksum = 0xffff;
    if (reverse) {
	doseps.wmf_begin = DOSEPS_HEADER_SIZE;
	doseps.ps_begin = doseps.wmf_begin + doseps.wmf_length;
    }
    else {
	doseps.ps_begin = DOSEPS_HEADER_SIZE;
	doseps.wmf_begin = doseps.ps_begin + doseps.ps_length;
    }
    write_doseps_header(&doseps, epsfile);

    buffer = (char *)malloc(COPY_BUF_SIZE);
    if (buffer == (char *)NULL) {
	if (epsname[0]) {
	    gfile_close(epsfile);
	    if (!(debug & DEBUG_GENERAL))
		csunlink(epsname);
	}
	gfile_close(tpsfile);
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tpsname);
	return -1;
    }

    gfile_seek(tpsfile, 0, gfile_begin);
    if (!reverse) {
	/* copy EPS file */
	while ((count = (int)gfile_read(tpsfile, buffer, COPY_BUF_SIZE)) != 0)
	    gfile_write(epsfile, buffer, count);
    }

    /* copy metafile */
    code = write_metafile(epsfile, img, devbbox, xdpi, ydpi, &mf);

    if (reverse) {
	/* copy EPS file */
	while ((count = (int)gfile_read(tpsfile, buffer, COPY_BUF_SIZE)) != 0)
	    gfile_write(epsfile, buffer, count);
    }

    free(buffer);
    gfile_close(tpsfile);
    if (!(debug & DEBUG_GENERAL))
	csunlink(tpsname);
    if (epsname[0])
       gfile_close(epsfile);
    return 0;
}

/*********************************************************/
/* Create a Macintosh file with PICT preview */
/* from a PS file and a bitmap */

int
make_eps_pict(Doc *doc, IMAGE *img, CDSCBBOX *bbox, 
    CDSCFBBOX *hires_bbox, float xdpi, float ydpi, CMAC_TYPE mac_type,
    LPCTSTR epsname)
{
    GFile *epsfile;
    GFile *tpsfile;
    TCHAR tpsname[MAXSTR];
    GFile *tpictfile;
    TCHAR tpictname[MAXSTR];
    char filename[MAXSTR];
    int code = 0;
    int len;
    CMAC_TYPE type = mac_type;
    const TCHAR *tp, *tq;

    len = (int)cslen(epsname);
    if (mac_type != CMAC_TYPE_NONE)
	type = mac_type;
    else if (epsname[0] == '.')
	type = CMAC_TYPE_DOUBLE;
    else if ((len > 3) &&
	(epsname[len-3] == '.') &&
	((epsname[len-2] == 'a') || (epsname[len-2] == 'A')) &&
	((epsname[len-1] == 's') || (epsname[len-1] == 'S')) )
	type = CMAC_TYPE_SINGLE;
    else if ((len > 5) &&
	((epsname[len-5] == '.') || (epsname[len-5] == '/')) &&
	((epsname[len-4] == 'r') || (epsname[len-4] == 'R')) &&
	((epsname[len-3] == 's') || (epsname[len-3] == 'S')) &&
	((epsname[len-2] == 'r') || (epsname[len-2] == 'R')) &&
	((epsname[len-1] == 'c') || (epsname[len-1] == 'C')) )
	type = CMAC_TYPE_RSRC;
    else
	type = CMAC_TYPE_MACBIN;

    /* Create temporary EPS file with updated headers */
    tpsfile = NULL;
    memset(tpsname, 0, sizeof(tpsname));
    if ((tpsfile = app_temp_gfile(doc->app, tpsname, 
	sizeof(tpsname)/sizeof(TCHAR))) == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't create temporary EPS file \042%s\042\n"),
	    tpsname);
	return -1;
    }
    gfile_close(tpsfile);

    code = copy_eps(doc, tpsname, bbox, hires_bbox, 0, FALSE); 
    if (code)
	return -1;

    if ( (tpsfile = gfile_open(tpsname, gfile_modeRead)) == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't open temporary EPS file \042%s\042\n"),
	    tpsname);
	return -1;
    }
    gfile_close(tpsfile);

    /* Create temporary PICT file */
    tpictfile = NULL;
    memset(tpictname, 0, sizeof(tpictname));
    if ((tpictfile = app_temp_gfile(doc->app, tpictname, 
	sizeof(tpictname)/sizeof(TCHAR))) == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't create temporary PICT file \042%s\042\n"),
	    tpictname);
	return -1;
    }
    gfile_close(tpictfile);
    code = image_to_pictfile(img, tpictname, xdpi, ydpi);

    /* Create Mac output file */
    epsfile = gfile_open(epsname, gfile_modeWrite | gfile_modeCreate);
    if (epsfile == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't open output EPS file \042%s\042\n"),
	    epsname);
	gfile_close(tpsfile);
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tpsname);
	return -1;
    }
    
    switch (type) {
	case CMAC_TYPE_SINGLE:
	    code = write_applesingle(epsfile, tpsname, tpictname);
	    break;
	case CMAC_TYPE_DOUBLE:
	    code = write_appledouble(epsfile, tpictname);
	    break;
	case CMAC_TYPE_MACBIN:
	    memset(filename, 0, sizeof(filename));
            /* Remove path information */
	    tp = tq = epsname;
	    while (*tp != '\0') {
		if ((*tp == '\\') || (*tp == '/') || (*tp == ':'))
		    tq = CHARNEXT(tp);
		tp = CHARNEXT(tp);
	    }
	    cs_to_narrow(filename, (int)sizeof(filename)-1, 
		    tq, (int)cslen(tq)+1);
	    len = (int)strlen(filename);
	    /* Remove trailing .bin */
    	    if ((len > 4) &&
		(filename[len-4] == '.') &&
		((filename[len-3] == 'b') || (filename[len-3] == 'B')) && 
		((filename[len-2] == 'i') || (filename[len-2] == 'I')) &&
		((filename[len-1] == 'n') || (filename[len-1] == 'N')) )
		filename[len-4] = '\0'; /* remove ".bin" inside MacBinary */
	    code = write_macbin(epsfile, filename, tpsname, tpictname);
	    break;
	case CMAC_TYPE_RSRC:
	    /* Just copy the resources */
    	    code = (write_resource_pict(epsfile, tpictname) < 0);
	    break;
	default:
	    code = -1;
    }

    if (!(debug & DEBUG_GENERAL))
	csunlink(tpsname);
    if (!(debug & DEBUG_GENERAL))
	csunlink(tpictname);
    if (epsname[0]) {
        gfile_close(epsfile);
	if ((code<0) && (!(debug & DEBUG_GENERAL)))
	    csunlink(epsname);
    }
    return code;
}

/*********************************************************/
/* Copy one page to a file. 
 * This will be used for feeding directly to Ghostscript,
 * so don't bother updating header comments.
 * Add code to force one and one only showpage.
 */
int
copy_page_temp(Doc *doc, GFile *f, int page)
{
    const char save_str[] = 
	"%!\nsave /GSview_save exch def\n/showpage {} def\n";
    const char restore_str[] = 
        "\nclear cleardictstack GSview_save restore\nshowpage\n";
    int code;
    gfile_puts(f, save_str);
    code  = copy_page_nosave(doc, f, page);
    gfile_puts(f, restore_str);
    return code;
}

int
copy_page_nosave(Doc *doc, GFile *f, int page)
{
    CDSC *dsc = doc->dsc;
    GFile *docfile;
    const char *fname;
    if (dsc == NULL)
	return -1;
   
    fname = dsc_find_platefile(dsc, page);
    if (fname) {
	/* A separation in a separate file */
	FILE_POS end;
	TCHAR wfname[MAXSTR];
	narrow_to_cs(wfname, (int)sizeof(wfname), fname, (int)strlen(fname)+1);
	if ((docfile = gfile_open(wfname, gfile_modeRead)) != (GFile *)NULL) {
	    end = gfile_get_length(docfile);
	    ps_copy(f, docfile, 0, end);
	    gfile_close(docfile);
	}
	/* else separation is missing, don't flag an error */
    }
    else {
	/* ordinary file */
	if ((docfile = gfile_open(doc_name(doc), gfile_modeRead)) 
	    == (GFile *)NULL) {
	    app_csmsgf(doc->app, 
		TEXT("Can't open document file \042%s\042\n"),
		doc_name(doc));
	    return -1;
	}
	ps_copy(f, docfile, dsc->begincomments, dsc->endcomments);
	ps_copy(f, docfile, dsc->begindefaults, dsc->enddefaults);
	ps_copy(f, docfile, dsc->beginprolog, dsc->endprolog);
	ps_copy(f, docfile, dsc->beginsetup, dsc->endsetup);
	if (dsc->page_count && (page >= 0) && (page < (int)dsc->page_count))
	    ps_copy(f, docfile, dsc->page[page].begin, dsc->page[page].end);
	ps_copy(f, docfile, dsc->begintrailer, dsc->endtrailer);
        gfile_close(docfile);
    }

    return 0;
}


/*********************************************************/
