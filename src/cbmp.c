/* Copyright (C) 2001-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cbmp.c,v 1.17 2005/06/10 09:39:24 ghostgum Exp $ */
/* BMP and PBM to IMAGE conversion */
/* IMAGE to BMP, PBM, TIFF, PICT conversion */

/* These functions convert between a BMP or PBM file
 * and an IMAGE in the format as used by the Ghostscript
 * display device.
 */

#include "common.h"
#include "gdevdsp.h"
#include "cbmp.h"
#include "cimg.h"
#include "cps.h"	/* for ps_fgets */
#include <time.h>

#ifdef HAVE_LIBPNG
#include "png.h"
#endif

/* local prototypes */
static int read_pbm_bits(unsigned char *pbitmap, 
    unsigned int width, unsigned int height, GFile *f);
static int read_pgnm_bytes(unsigned char *pbitmap, 
    unsigned int length, GFile *f);
static int image_bmp2init(IMAGE *img, BITMAP2 *bmp2);
static void shift_bits(unsigned char *bits, int bwidth, int offset);
static void write_bigendian_dword(DWORD val, GFile *f);
static void write_bigendian_word(WORD val, GFile *f);

static const char szGSviewName[] = "GSview"; 

/* read/write word and dword in a portable way */

DWORD
get_dword(const unsigned char *buf)
{
    DWORD dw;
    dw = (DWORD)buf[0];
    dw += ((DWORD)buf[1])<<8;
    dw += ((DWORD)buf[2])<<16;
    dw += ((DWORD)buf[3])<<24;
    return dw;
}

WORD
get_word(const unsigned char *buf)
{
    WORD w;
    w = (WORD)buf[0];
    w |= (WORD)(buf[1]<<8);
    return w;
}

/* write DWORD as DWORD */
void
write_dword(DWORD val, GFile *f)
{
    unsigned char dw[4];
    dw[0] = (unsigned char)( val      & 0xff);
    dw[1] = (unsigned char)((val>>8)  & 0xff);
    dw[2] = (unsigned char)((val>>16) & 0xff);
    dw[3] = (unsigned char)((val>>24) & 0xff);
    gfile_write(f, &dw, 4);
}

/* write WORD as DWORD */
void
write_word_as_dword(WORD val, GFile *f)
{
    unsigned char dw[4];
    dw[0] = (unsigned char)( val      & 0xff);
    dw[1] = (unsigned char)((val>>8)  & 0xff);
    dw[2] = '\0';
    dw[3] = '\0';
    gfile_write(f, &dw, 4);
}

/* write WORD as WORD */
void
write_word(WORD val, GFile *f)
{
    unsigned char w[2];
    w[0] = (unsigned char)( val      & 0xff);
    w[1] = (unsigned char)((val>>8)  & 0xff);
    gfile_write(f, &w, 2);
}

/* write bigendian DWORD */
static void
write_bigendian_dword(DWORD val, GFile *f)
{
    unsigned char dw[4];
    dw[3] = (unsigned char)( val      & 0xff);
    dw[2] = (unsigned char)((val>>8)  & 0xff);
    dw[1] = (unsigned char)((val>>16) & 0xff);
    dw[0] = (unsigned char)((val>>24) & 0xff);
    gfile_write(f, &dw, 4);
}

/* write bigendian WORD */
static void
write_bigendian_word(WORD val, GFile *f)
{
    unsigned char w[2];
    w[1] = (unsigned char)( val      & 0xff);
    w[0] = (unsigned char)((val>>8)  & 0xff);
    gfile_write(f, &w, 2);
}

/* shift preview by offset bits to the left */
/* width is in bytes */
/* fill exposed bits with 1's */
static void
shift_bits(unsigned char *bits, int bwidth, int offset)
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
    memmove(bits, bits+byteoffset, newwidth);
    memset(bits+newwidth, 0xff, bwidth-newwidth);
    /* next remove bit offset */
    bitoffset = offset - byteoffset*8;
    if (bitoffset==0)
	return;
    bitoffset = 8 - bitoffset;
    for (i=0; i<newwidth; i++) {
       shifter = bits[i] << 8;
       if (i==newwidth-1)
	   shifter += 0xff;	/* can't access bits[bwidth] */
       else
	   shifter += bits[i+1];  
       bits[i] = (unsigned char)(shifter>>bitoffset);
    }
}


/* Load a Windows bitmap and return an image.
 * Because we are forcing this into a format that 
 * could be written by the Ghostscript "display"
 * device, we can't write palette images since
 * the palette in the display device is fixed.
 * We convert 4 and 8-bit palette bitmaps to
 * 24-bit colour.
 */
IMAGE *
bmpfile_to_image(LPCTSTR filename)
{
    GFile *f = gfile_open(filename, gfile_modeRead);
    IMAGE *img;
    unsigned char bmf_buf[BITMAPFILE_LENGTH];
    unsigned char *pbitmap;
    unsigned int length;
    unsigned int count;
    BITMAPFILE bmf;
    if (f == (GFile *)NULL)
	return NULL;
    gfile_read(f, bmf_buf, sizeof(bmf_buf));
    if ((bmf_buf[0] != 'B') || (bmf_buf[1] != 'M')) {
	/* Not a Windows bitmap */
	gfile_close(f);
	return NULL;
    }
    bmf.bfType = get_word(bmf_buf);
    bmf.bfSize = get_dword(bmf_buf+2);
    bmf.bfReserved1 = get_word(bmf_buf+6);
    bmf.bfReserved1 = get_word(bmf_buf+8);
    bmf.bfOffBits = get_dword(bmf_buf+10);
    length = bmf.bfSize - BITMAPFILE_LENGTH;

    pbitmap = (unsigned char *)malloc(length);
    if (pbitmap == NULL) {
	gfile_close(f);
	return NULL;
    }
    
    count = (int)gfile_read(f, pbitmap, length);
    gfile_close(f);

    img = bmp_to_image(pbitmap, length);
    free(pbitmap);

    return img;
}

static const unsigned char clr555[] = {
    0x1f, 0x00, 0x00, 0x00, /* blue */
    0xe0, 0x03, 0x00, 0x00, /* green */
    0x00, 0x7c, 0x00, 0x00 /* red */
};
static const unsigned char clr565[] = {
    0x1f, 0x00, 0x00, 0x00, /* blue */
    0xe0, 0x07, 0x00, 0x00, /* green */
    0x00, 0xf8, 0x00, 0x00 /* red */
};

IMAGE *
bmp_to_image(unsigned char *pbitmap, unsigned int length)
{
    BITMAP2 bmp2;
    RGB4 colour[256];
    int depth;
    int palcount;
    int pallength;
    int bytewidth;
    BOOL convert;
    int i;
    int x, y;
    IMAGE img;
    IMAGE *pimage;
    unsigned char *bits;
    unsigned char *dest;
    memset(&img, 0, sizeof(img));
    if (length < BITMAP2_LENGTH)
	return NULL;
    /* Read the BITMAPINFOHEADER in a portable way. */
    bmp2.biSize = get_dword(pbitmap);
    pbitmap += 4;
    if (bmp2.biSize < BITMAP2_LENGTH)
	return NULL;	/* we don't read OS/2 BMP format */
    bmp2.biWidth = get_dword(pbitmap);
    pbitmap += 4;
    bmp2.biHeight = get_dword(pbitmap);
    pbitmap += 4;
    bmp2.biPlanes = get_word(pbitmap);
    pbitmap += 2;
    bmp2.biBitCount = get_word(pbitmap);
    pbitmap += 2;
    bmp2.biCompression = get_dword(pbitmap);
    pbitmap += 4;
    bmp2.biSizeImage = get_dword(pbitmap);
    pbitmap += 4;
    bmp2.biXPelsPerMeter = get_dword(pbitmap);
    pbitmap += 4;
    bmp2.biYPelsPerMeter = get_dword(pbitmap);
    pbitmap += 4;
    bmp2.biClrUsed = get_dword(pbitmap);
    pbitmap += 4;
    bmp2.biClrImportant = get_dword(pbitmap);
    pbitmap += 4;
    pbitmap += bmp2.biSize - BITMAP2_LENGTH;

    /* Calculate the raster size, depth, palette length etc. */
    depth = bmp2.biPlanes * bmp2.biBitCount;
    bytewidth = ((bmp2.biWidth * depth + 31) & ~31) >> 3;
    palcount = 0;
    if (depth <= 8)
	palcount = (bmp2.biClrUsed != 0) ? 
		(int)bmp2.biClrUsed : (int)(1 << depth);
    pallength = 0;
    if ((depth == 16) || (depth == 32)) {
	if (bmp2.biCompression == BI_BITFIELDS)
	    pallength = 12;
    }
    else
	pallength = palcount * RGB4_LENGTH;
    for (i=0; i<palcount; i++) {
	colour[i].rgbBlue  = pbitmap[i*4+RGB4_BLUE];
	colour[i].rgbGreen = pbitmap[i*4+RGB4_GREEN];
	colour[i].rgbRed   = pbitmap[i*4+RGB4_RED];
    }

    if (length < bmp2.biSize + pallength + bmp2.biHeight * bytewidth)
	return NULL;

    /* Now find out which format to use */
    /* Default is 24-bit BGR */
    img.width = bmp2.biWidth;
    img.height = bmp2.biHeight;
    img.raster = img.width * 3;
    img.format = DISPLAY_COLORS_RGB | DISPLAY_ALPHA_NONE |
       DISPLAY_DEPTH_8 | DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST;
    convert = FALSE;

    /* We will save it as either 1-bit/pixel, 8-bit/pixel grey,
     * or 24-bit/pixel RGB.
     */
    if (depth == 1) {
	if ((colour[0].rgbBlue == 0) && 
	    (colour[0].rgbGreen == 0) && 
	    (colour[0].rgbRed == 0) &&
	    (colour[1].rgbBlue == 0xff) && 
	    (colour[1].rgbGreen == 0xff) && 
	    (colour[1].rgbRed == 0xff)) {
	    /* black and white */
	    img.format = DISPLAY_COLORS_GRAY | DISPLAY_ALPHA_NONE |
	       DISPLAY_DEPTH_1 | DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST;
	    img.raster = (img.width + 7) >> 3;
	}
	else if ((colour[0].rgbBlue == 0xff) && 
	    (colour[0].rgbGreen == 0xff) && 
	    (colour[0].rgbRed == 0xff) &&
	    (colour[1].rgbBlue == 0) && 
	    (colour[1].rgbGreen == 0) && 
	    (colour[1].rgbRed == 0)) {
	    /* black and white */
	    img.format = DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE |
	       DISPLAY_DEPTH_1 | DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST;
	    img.raster = (img.width + 7) >> 3;
	}
	else if ( (colour[0].rgbBlue == colour[0].rgbGreen) && 
	    (colour[0].rgbRed == colour[0].rgbGreen) &&
	    (colour[1].rgbBlue == colour[1].rgbGreen) && 
	    (colour[1].rgbRed == colour[1].rgbGreen)) {
	    /* convert to greyscale */
	    img.format = DISPLAY_COLORS_GRAY | DISPLAY_ALPHA_NONE |
	       DISPLAY_DEPTH_8 | DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST;
	    img.raster = img.width;
	    convert = TRUE;
	}
	else
	    /* convert to colour */
	    convert = TRUE;
    }
    else if ((depth == 4) || (depth == 8)) {
	BOOL grey = TRUE;
	for (i=0; i<palcount; i++) {
	    if ((colour[i].rgbBlue != colour[i].rgbGreen) ||  
	    (colour[i].rgbRed != colour[i].rgbGreen))
		grey = FALSE;
	}
	if (grey) {
	    img.format = DISPLAY_COLORS_GRAY | DISPLAY_ALPHA_NONE |
	       DISPLAY_DEPTH_8 | DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST;
	    img.raster = img.width;
	}
	convert = TRUE;
	
    }
    else if (depth == 16) {
	if (pallength == 0) {
	    img.format = DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE |
	       DISPLAY_DEPTH_16 | DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST |
	        DISPLAY_NATIVE_555;
	    img.raster = img.width * 2;
	}
	else if ((pallength == 12) &&
	    (memcmp(pbitmap, clr555, sizeof(clr555)) == 0)) {
	    img.format = DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE |
	       DISPLAY_DEPTH_16 | DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST |
	        DISPLAY_NATIVE_555;
	    img.raster = img.width * 2;
	}
	else if ((pallength == 12) &&
		(memcmp(pbitmap, clr565, sizeof(clr565)) == 0)) {
	    img.format = DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE |
	       DISPLAY_DEPTH_16 | DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST |
	        DISPLAY_NATIVE_565;
	    img.raster = img.width * 2;
	}
	else
	    return NULL;	/* unrecognised format */
    }
    else if (depth == 24) {
	/* already in correct format */
    }
    else if (depth == 32) {
	unsigned char clr888[] = {
	    0xff, 0x00, 0x00, 0x00, /* blue */
	    0x00, 0xff, 0x00, 0x00, /* green */
	    0x00, 0x00, 0xff, 0x00 /* red */
	};
	if ( (pallength == 0) || 
	    ((pallength == 12) &&
	     (memcmp(pbitmap, clr888, sizeof(clr888)) == 0)) ) {
	    img.format = DISPLAY_COLORS_RGB | DISPLAY_UNUSED_LAST |
	       DISPLAY_DEPTH_8 | DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST;
	    img.raster = img.width * 4;
	}
	else
	    return NULL;	/* unrecognised format */
    }
    else
	return NULL;	/* unrecognised format */

    pbitmap += pallength;

    img.raster = (img.raster + 3) & ~3;
    bits = (unsigned char *)malloc(img.raster * img.height);
    if (bits == NULL)
	return NULL;

    for (y=0; y<(int)img.height; y++) {
        dest = bits + y * img.raster;
	if (convert) {
	    int idx;
	    int shift = 7;
	    if (depth == 1) {
		for (x=0; x<bmp2.biWidth; x++) {
		    idx = pbitmap[x >> 3];
		    idx = (idx >> shift) & 1;
		    if ((img.format & DISPLAY_COLORS_MASK) == DISPLAY_COLORS_GRAY)
			*dest++ = colour[idx].rgbBlue;
		    else {
			/* colour */
			*dest++ = colour[idx].rgbBlue;
			*dest++ = colour[idx].rgbGreen;
			*dest++ = colour[idx].rgbRed;
		    }
		    shift--;
		    if (shift < 0)
			shift = 7;
		}
	    }
	    else if (depth == 4) {
		for (x=0; x<bmp2.biWidth; x++) {
		    idx = pbitmap[x/2];
		    if (x & 0)
			idx &= 0xf;
		    else
			idx = (idx >> 4) & 0xf;
		    if ((img.format & DISPLAY_COLORS_MASK) == DISPLAY_COLORS_GRAY)
			*dest++ = colour[idx].rgbBlue;
		    else {
			/* colour */
			*dest++ = colour[idx].rgbBlue;
			*dest++ = colour[idx].rgbGreen;
			*dest++ = colour[idx].rgbRed;
		    }
		}
	    }
	    else if (depth == 8) {
		for (x=0; x<bmp2.biWidth; x++) {
		    idx = pbitmap[x];
		    if ((img.format & DISPLAY_COLORS_MASK) == DISPLAY_COLORS_GRAY)
			*dest++ = colour[idx].rgbBlue;
		    else {
			/* colour */
			*dest++ = colour[idx].rgbBlue;
			*dest++ = colour[idx].rgbGreen;
			*dest++ = colour[idx].rgbRed;
		    }
		}
	    }
	    else {
		free(bits);
		return NULL;
	    }
	}
	else {
	    memcpy(dest, pbitmap, img.raster);
	}
	pbitmap += bytewidth;
    }

    pimage = (IMAGE *)malloc(sizeof(IMAGE));
    if (pimage == NULL) {
	free(bits);
	return NULL;
    }
    memcpy(pimage, &img, sizeof(IMAGE));
    pimage->image = bits;
    return pimage;
}


/* read width * height "bits" from file */
static int
read_pbm_bits(unsigned char *pbitmap, unsigned int width, unsigned int height,
    GFile *f)
{
    int count = 0;
    int ch;
    int mask = 0x80;
    int x, y;
    char buf[MAXSTR];
    int buf_count = 0;
    int buf_idx = 0;

    for (y=0; y<(int)height; y++) {
	mask = 0x80;
	for (x=0; x<(int)width; x++) {
	    if (mask == 0) {
		mask = 0x80;
		pbitmap++;
		count++;
	    }
	    ch = 0;
	    while ((ch != '0') && (ch != '1')) {
		if (buf_idx >= buf_count) {
		    buf_count = gfile_read(f, buf, min(sizeof(buf), width-x));
		    if (buf_count == 0)
			return -1;	/* premature EOF */
		    buf_idx = 0;
		}
		ch = buf[buf_idx++];
	    }
	    *pbitmap = (unsigned char)
		((ch == '1') ? (*pbitmap | mask) : (*pbitmap & (~mask)));
	    mask >>= 1;
	}
        pbitmap++;
	count++;
    }
    return count;
}

/* read length "bytes" from file */
static int
read_pgnm_bytes(unsigned char *pbitmap, unsigned int length, GFile *f)
{
    int count = 0;
    int ch = 0;
    int val;
    char buf[MAXSTR];
    int buf_count = 0;
    int buf_idx = 0;

    for (count=0; count < (int)length; count++) {
	val = 0;
	while (!((ch >= '0') && (ch <= '9'))) {
	    if (buf_idx >= buf_count) {
		buf_count = gfile_read(f, buf, min(sizeof(buf), length-count));
		if (buf_count == 0)
		    return -1;	/* premature EOF */
		buf_idx = 0;
	    }
	    ch = buf[buf_idx++];
	}
	while ((ch >= '0') && (ch <= '9')) {
	    val = (val*10) + ch - '0';
	    if (buf_idx >= buf_count) {
		buf_count = gfile_read(f, buf, min(sizeof(buf), length-count));
		if (buf_count == 0)
		    return -1;	/* premature EOF */
		buf_idx = 0;
	    }
	    ch = buf[buf_idx++];
	}
	*pbitmap++ = (unsigned char)val;
    }
    return count;
}

/* Load a PBMPLUS bitmap and return an image.
 * Supported formats are pbmraw, pgmraw and ppmraw as written
 * by Ghostscript.
 */
IMAGE *
pnmfile_to_image(LPCTSTR filename)
{
    GFile *f = gfile_open(filename, gfile_modeRead);
    IMAGE img;
    IMAGE *pimage;
    int code;
    char typeline[256];
    char sizeline[256];
    char maxvalline[256];
    char hdrline[256];
    char tupltype[256];
    int width = 0;
    int height = 0;
    int maxval = 255;
    int depth = 0;
    int pam = 0;
    int pbm = 0;
    int pgm = 0;
    int ppm = 0;
    int raw = 0;
    int cmyk = 0;
    unsigned int length;
    unsigned char *pbitmap;
    int endhdr;
    char *t1;
    char *t2;

    unsigned int count;

    if (f == (GFile *)NULL)
	return NULL;
    memset(&img, 0, sizeof(img));
    memset(typeline, 0, sizeof(typeline));
    memset(sizeline, 0, sizeof(sizeline));
    memset(maxvalline, 0, sizeof(maxvalline));
    code = ps_fgets(typeline, sizeof(typeline)-1, f) == 0;
    if (typeline[0] != 'P')
	code = 1;
    switch (typeline[1]) {
	case '1':
	    pbm = 1;
	    raw = 0;
	    break;
	case '2':
	    pgm = 1;
	    raw = 0;
	    break;
	case '3':
	    ppm = 1;
	    raw = 0;
	    break;
	case '4':
	    pbm = 1;
	    raw = 1;
	    break;
	case '5':
	    pgm = 1;
	    raw = 1;
	    break;
	case '6':
	    ppm = 1;
	    raw = 1;
	    break;
	case '7':
	    pam = 1;
	    raw = 1;
	    break;
	default:
	    code = 1;
    }

    if (pam) {
	/* Portable Arbitrary Map */
	endhdr = 0;
	while (!endhdr && !code) {
	    if (!code)
		code = ps_fgets(hdrline, sizeof(hdrline)-1, f) == 0;
	    while (!code && (hdrline[0] == '#'))
		/* skip comments */
		code = ps_fgets(hdrline, sizeof(hdrline)-1, f) == 0;
	    if (code)
		break;
	    t1 = hdrline;
	    while (*t1 && ((*t1==' ') || (*t1=='\t')))
		t1++;	/* skip whitespace */
	    t1 = strtok(t1, " \t\r\n");
	    if (t1 == NULL)
		break;
	    t2 = strtok(NULL, " \t\r\n");
	    if (strcmp(t1, "ENDHDR")==0) {
		endhdr = 1;
		continue;
	    }
	    else if (strcmp(t1, "WIDTH")==0) {
		if (t2)
		    code = sscanf(t2, "%u", &width) != 1;
	    }
	    else if (strcmp(t1, "HEIGHT")==0) {
		if (t2)
		    code = sscanf(t2, "%u", &height) != 1;
	    }
	    else if (strcmp(t1, "DEPTH")==0) {
		if (t2)
		    code = sscanf(t2, "%u", &depth) != 1;
	    }
	    else if (strcmp(t1, "MAXVAL")==0) {
		if (t2)
		    code = sscanf(t2, "%u", &maxval) != 1;
	    }
	    else if (strcmp(t1, "TUPLTYPE")==0) {
		if (t2)
		    strncpy(tupltype, t2, sizeof(tupltype)-1);
	    }
	}
	if (!endhdr)
	    code = 1;
	if ((width == 0) || (height == 0) || (depth == 0) || (maxval == 0))
	    code = 1;
	if ((strcmp(tupltype, "BLACKANDWHITE")==0) &&
	    (depth == 1) && (maxval == 1))
	    pbm = 1;
	if ((strcmp(tupltype, "GRAYSCALE")==0) &&
	    (depth == 1) && (maxval == 255))
	    pgm = 1;
	if ((strcmp(tupltype, "RGB")==0) && 
	    (depth == 3) && (maxval == 255))
	    ppm = 1;
	if ((strcmp(tupltype, "CMYK")==0) &&
	    (depth == 4) && (maxval == 255))
	    cmyk = 1;
    }
    else {
	if (!code)
	    code = ps_fgets(sizeline, sizeof(sizeline)-1, f) == 0;
	while (!code && (sizeline[0] == '#')) /* skip comments */
	    code = ps_fgets(sizeline, sizeof(sizeline)-1, f) == 0;

	if (!code)
	    code = sscanf(sizeline, "%u %u", &width, &height) != 2;
	if ((width == 0) || (height == 0))
	    code = 1;

	if (!code && (pgm || ppm)) {
	    code = ps_fgets(maxvalline, sizeof(maxvalline)-1, f) == 0;
	    while (!code && (maxvalline[0] == '#'))
		code = ps_fgets(maxvalline, sizeof(maxvalline)-1, f) == 0;
	    if (!code)
		code = sscanf(maxvalline, "%u", &maxval) != 1;
	}
	if (maxval != 255)
	    code = 1;
    }

    img.width = width;
    img.height = height;
    if (pbm) {
	img.format = DISPLAY_COLORS_NATIVE | DISPLAY_ALPHA_NONE |
	   DISPLAY_DEPTH_1 | DISPLAY_BIGENDIAN | DISPLAY_TOPFIRST;
	img.raster = (img.width + 7) >> 3;
    }
    else if (pgm) {
	img.format = DISPLAY_COLORS_GRAY | DISPLAY_ALPHA_NONE |
	   DISPLAY_DEPTH_8 | DISPLAY_BIGENDIAN | DISPLAY_TOPFIRST;
	img.raster = img.width;
    }
    else if (ppm) {
	img.format = DISPLAY_COLORS_RGB | DISPLAY_ALPHA_NONE |
	   DISPLAY_DEPTH_8 | DISPLAY_BIGENDIAN | DISPLAY_TOPFIRST;
	img.raster = img.width * 3;
    }
    else if (cmyk) {
	img.format = DISPLAY_COLORS_CMYK | DISPLAY_ALPHA_NONE |
	   DISPLAY_DEPTH_8 | DISPLAY_BIGENDIAN | DISPLAY_TOPFIRST;
	img.raster = img.width * 4;
    }
    else
	code = 1;

    length = img.raster * img.height;

    if (code) {
	gfile_close(f);
	return NULL;
    }

    pbitmap = (unsigned char *)malloc(length);
    if (pbitmap == NULL) {
	gfile_close(f);
	return NULL;
    }

    if (raw)
	count = (int)gfile_read(f, pbitmap, length);
    else if (pbm)
	count = read_pbm_bits(pbitmap, img.width, img.height, f);
    else
	count = read_pgnm_bytes(pbitmap, length, f);
    gfile_close(f);

    if (count != length) {
	free(pbitmap);
	return NULL;
    }

    pimage = (IMAGE *)malloc(sizeof(IMAGE));
    if (pimage == NULL) {
	free(pbitmap);
	return NULL;
    }
    memcpy(pimage, &img, sizeof(IMAGE));
    pimage->image = pbitmap;
    return pimage;
}

/* Free an image created by bmpfile_to_image or pnmfile_to_image */
void
bitmap_image_free(IMAGE *img)
{
    if (img && img->image) {
	free(img->image);
	memset(img, 0, sizeof(IMAGE));
	free(img);
    }
}

static int
image_bmp2init(IMAGE *img, BITMAP2 *bmp2)
{
    /* Create a BMP header from the IMAGE */
    /* If we need to convert the IMAGE before writing to a BMP file, 
     * the BMP header will not exactly match the image.
     */
    bmp2->biSize = BITMAP2_LENGTH;
    bmp2->biWidth = img->width;
    bmp2->biHeight = img->height;
    bmp2->biPlanes = 1;
    bmp2->biCompression = 0;
    switch (img->format & DISPLAY_COLORS_MASK) {
	case DISPLAY_COLORS_NATIVE:
	    switch (img->format & DISPLAY_DEPTH_MASK) {
		case DISPLAY_DEPTH_1:
		    bmp2->biBitCount = 1;
		    bmp2->biClrUsed = 2;
		    bmp2->biClrImportant = 2;
		    break;
		case DISPLAY_DEPTH_4:
		    /* Fixed color palette */
		    bmp2->biBitCount = 4;
		    bmp2->biClrUsed = 16;
		    bmp2->biClrImportant = 16;
		    break;
		case DISPLAY_DEPTH_8:
		    /* Fixed color palette */
		    bmp2->biBitCount = 8;
		    bmp2->biClrUsed = 96;
		    bmp2->biClrImportant = 96;
		    break;
		case DISPLAY_DEPTH_16:
		    /* RGB bitfields */
		    /* Bit fields */
		    if ((img->format & DISPLAY_ENDIAN_MASK)
			== DISPLAY_BIGENDIAN) {
			/* convert */
			bmp2->biBitCount = 24;
			bmp2->biClrUsed = 0;
			bmp2->biClrImportant = 0;
		    }
		    else {
			bmp2->biBitCount = 16;
			bmp2->biCompression = BI_BITFIELDS;
			bmp2->biClrUsed = 0;
			bmp2->biClrImportant = 0;
		    }
		    break;
		default:
		    return_error(-1);
	    }
	    break;
	case DISPLAY_COLORS_GRAY:
	    switch (img->format & DISPLAY_DEPTH_MASK) {
		case DISPLAY_DEPTH_1:
		    bmp2->biBitCount = 1;
		    bmp2->biClrUsed = 2;
		    bmp2->biClrImportant = 2;
		    break;
		case DISPLAY_DEPTH_4:
		    /* Fixed gray palette */
		    bmp2->biBitCount = 4;
		    bmp2->biClrUsed = 16;
		    bmp2->biClrImportant = 16;
		    break;
		case DISPLAY_DEPTH_8:
		    /* Fixed gray palette */
		    bmp2->biBitCount = 8;
		    bmp2->biClrUsed = 256;
		    bmp2->biClrImportant = 256;
		    break;
		default:
		    return_error(-1);
	    }
	    break;
	case DISPLAY_COLORS_RGB:
	    if ((img->format & DISPLAY_DEPTH_MASK) != DISPLAY_DEPTH_8)
		return_error(-1);
	    /* either native BGR, or we need to convert it */
	    bmp2->biBitCount = 24;
	    bmp2->biClrUsed = 0;
	    bmp2->biClrImportant = 0;
	    break;
	case DISPLAY_COLORS_CMYK:
	    /* convert */
	    bmp2->biBitCount = 24;
	    bmp2->biClrUsed = 0;
	    bmp2->biClrImportant = 0;
	    break;
    }

    bmp2->biSizeImage = 0;
    bmp2->biXPelsPerMeter = 0;
    bmp2->biYPelsPerMeter = 0;
    return 0;
}


/* Write an IMAGE as a Windows BMP file */
/* This is typically used to copy the display bitmap to a file */
int
image_to_bmpfile(IMAGE* img, LPCTSTR filename, float xdpi, float ydpi)
{
    BITMAP2 bmp2;
    BITMAPFILE bmf;
    int bytewidth;
    int depth;
    int palcount;
    int pallength;
    GFile *f;
    unsigned char r, g, b;
    unsigned char quad[4];
    unsigned char nulchar = '\0';
    int i;
    unsigned char *bits;
    unsigned char *row;
    int topfirst;

    if ((img == NULL) || (img->image == NULL))
	return -1;
    if (image_bmp2init(img, &bmp2) < 0)
	return -1;

    if ((xdpi > 0.0) && (ydpi > 0.0)) {
	bmp2.biXPelsPerMeter = (int)(xdpi * 1000.0 / 25.4 + 0.5);
	bmp2.biYPelsPerMeter = (int)(ydpi * 1000.0 / 25.4 + 0.5);
    }

    depth = bmp2.biPlanes * bmp2.biBitCount;
    bytewidth = ((bmp2.biWidth * depth + 31) & ~31) >> 3;
    switch (depth) {
	case 1:
	case 4:
	case 8:
	    palcount = 1 << depth;
	    pallength = palcount * RGB4_LENGTH;
	    break;
	case 16:
	    palcount = 0;
	    pallength = 12;
	default:
	    palcount = 0;
	    pallength = 0;
    }

    bmf.bfType = get_word((const unsigned char *)"BM");
    bmf.bfReserved1 = 0;
    bmf.bfReserved2 = 0;;
    bmf.bfOffBits = BITMAPFILE_LENGTH + BITMAP2_LENGTH + palcount;
    bmf.bfSize = bmf.bfOffBits + bytewidth * bmp2.biHeight;

    row = (unsigned char *)malloc(bytewidth);
    if (row == NULL)
	return -1;
    
    f = gfile_open(filename, gfile_modeWrite | gfile_modeCreate);
    if (f == (GFile *)NULL) {
	free(row);
	return -1;
    }

    /* Write BITMAPFILEHEADER */
    write_word(bmf.bfType, f);
    write_dword(bmf.bfSize, f);
    write_word(bmf.bfReserved1, f);
    write_word(bmf.bfReserved2, f);
    write_dword(bmf.bfOffBits, f);

    /* Write BITMAPINFOHEADER */
    write_dword(bmp2.biSize, f);
    write_dword(bmp2.biWidth, f);
    write_dword(bmp2.biHeight, f);
    write_word(bmp2.biPlanes, f);
    write_word(bmp2.biBitCount, f);
    write_dword(bmp2.biCompression, f);
    write_dword(bmp2.biSizeImage, f);
    write_dword(bmp2.biXPelsPerMeter, f);
    write_dword(bmp2.biYPelsPerMeter, f);
    write_dword(bmp2.biClrUsed, f);
    write_dword(bmp2.biClrImportant, f);

    /* Write palette or bitmasks */
    for (i=0; i<palcount; i++) {
	image_colour(img->format, i, &r, &g, &b);
	quad[0] = b;
	quad[1] = g;
	quad[2] = r;
	quad[3] = '\0';
	gfile_write(f, quad, 4);
    }
    if (bmp2.biCompression == BI_BITFIELDS) {
	if ((img->format & DISPLAY_555_MASK) == DISPLAY_NATIVE_555)
	    gfile_write(f, clr555, sizeof(clr555));
	else 
	    gfile_write(f, clr565, sizeof(clr565));
    }

    /* Write the bits */
    topfirst = ((img->format & DISPLAY_FIRSTROW_MASK) == DISPLAY_TOPFIRST);
    for (i=0; i<bmp2.biHeight; i++) {
	if (topfirst)
	    bits = img->image + img->raster * (img->height - i - 1);
	else
	    bits = img->image + img->raster * i;
	if (depth == 24) {
	    image_to_24BGR(img, row, bits);
	    gfile_write(f, row, bytewidth);
	}
	else {
	    if ((int)img->raster < bytewidth) {
		int j;
	        gfile_write(f, bits, img->raster);
		for (j=bytewidth-img->raster; j>0; j--)
		    gfile_write(f, &nulchar, 1);
	    }
	    else
	        gfile_write(f, bits, bytewidth);
	}
    }

    free(row);
    gfile_close(f);
    return 0;
}

/*********************************************************/

#define tiff_long(val, f) write_dword(val, f)
#define tiff_short(val, f) write_word_as_dword(val, f)
#define tiff_word(val, f) write_word(val, f)

#define TIFF_BYTE 1
#define TIFF_ASCII 2
#define TIFF_SHORT 3
#define TIFF_LONG 4
#define TIFF_RATIONAL 5

struct rational_s {
	DWORD numerator;
	DWORD denominator;
};
#define TIFF_RATIONAL_SIZE 8

struct ifd_entry_s {
	WORD tag;
	WORD type;
	DWORD length;
	DWORD value;
};
#define TIFF_IFD_SIZE 12

struct tiff_head_s {
	WORD order;
	WORD version;
	DWORD ifd_offset;
};
#define TIFF_HEAD_SIZE 8

/* Write tiff file from IMAGE.
 * Since this will be used by a DOS EPS file, we write an Intel TIFF file.
 * Include the pixels specified in devbbox, which is in pixel coordinates
 * not points.  If this is empty, the whole image will be used.
 * Resolution of bitmap is xdpi,ydpi.
 * If tiff4 is true, write a monochrome file compatible with TIFF 4,
 * otherwise make it compatible with TIFF 6.
 * If use_packbits is true and tiff4 is false, use packbits to
 * compress the bitmap.
 */
int image_to_tiff(GFile *f, IMAGE *img, 
    int xoffset, int yoffset, int width, int height, 
    float xdpi, float ydpi, 
    BOOL tiff4, BOOL use_packbits)
{
#define IFD_MAX_ENTRY 12
    WORD ifd_length;
    DWORD ifd_next;
    DWORD tiff_end, end;
    int i, j;
    unsigned char *preview;
    BYTE *line;
    const unsigned char nulchar = '\0';
    int temp_bwidth, bwidth;
    BOOL soft_extra = FALSE;
    int bitoffset;
    WORD *comp_length=NULL;	/* lengths of compressed lines */
    BYTE *comp_line=NULL;	/* compressed line buffer */
    int rowsperstrip;
    int stripsperimage;
    int strip, is;
    int strip_len;
    int lastrow;

    int depth;
    int preview_depth;
    int topfirst;

    if (img == NULL)
	return -1;

    topfirst = ((img->format & DISPLAY_FIRSTROW_MASK) == DISPLAY_TOPFIRST);

    depth = image_depth(img);
    if (depth == 1)
	preview_depth = 1;
    else if (depth == 4)
	preview_depth = 4;
    else if (depth == 8)
	preview_depth = 8;
    else 
	preview_depth = 24;
    if (tiff4)
	preview_depth = 1;

    /* byte width of source bitmap is img->raster */
    /* byte width of intermediate line, after conversion
     * to preview_depth, but before shifting to remove
     * unwanted pixels.
     */
    temp_bwidth = (img->width * preview_depth + 7) >> 3;
    /* byte width of preview */
    bwidth = (width * preview_depth + 7) >> 3;
    /* Number of bits to shift intermediate line to get preview line */
    bitoffset = xoffset * preview_depth;

    if (tiff4)
	rowsperstrip = 1; /* make TIFF 4 very simple */
    else {
	/* work out RowsPerStrip, to give < 8k compressed */
	/* or uncompressed data per strip */
	rowsperstrip = (8192 - 256) / bwidth;
	if (rowsperstrip == 0)
	    rowsperstrip = 1;	/* strips are larger than 8k */
    }
    stripsperimage = (height + rowsperstrip - 1) / rowsperstrip;
    if (stripsperimage == 1)
	rowsperstrip = height;

    preview = (unsigned char *) malloc(img->raster);
    if (preview == NULL)
	return -1;
    memset(preview,0xff,img->raster);

    /* compress bitmap, throwing away result, to find out compressed size */
    if (use_packbits) {
	comp_length = (WORD *)malloc(stripsperimage * sizeof(WORD));
	if (comp_length == NULL) {
	    free(preview);
	    return -1;
	}
	comp_line = (BYTE *)malloc(bwidth + bwidth/64 + 1);
	if (comp_line == NULL) {
	    free(preview);
	    free(comp_length);
	    return -1;
	}
	if (topfirst) 
	    line = img->image + img->raster * (img->height - yoffset - height);
	else
	    line = img->image + img->raster * (yoffset + height-1);

	/* process each strip */
	for (strip = 0; strip < stripsperimage; strip++) {
	    is = strip * rowsperstrip;
	    lastrow = min( rowsperstrip, height - is);
	    comp_length[strip] = 0;
	    strip_len = 0;
	    /* process each line within strip */
	    for (i = 0; i< lastrow; i++) {
		if (preview_depth == 1) {
		    memset(preview,0xff,img->raster);
		    image_to_mono(img, preview, line);
		    for (j=0; j<temp_bwidth; j++)
			preview[j] ^= 0xff;
		}
		else if (preview_depth == 24)
		    image_to_24RGB(img, preview, line);
		else if (depth == preview_depth)
		    memmove(preview,  line, img->raster);
		if (bitoffset)
		    shift_bits(preview, temp_bwidth, bitoffset);
		strip_len += packbits(comp_line, preview, bwidth);
		if (topfirst)
		    line += img->raster;
		else
		    line -= img->raster;
	    }
	    comp_length[strip] = (WORD)strip_len;
	}
    }
     

    /* write header */
    tiff_end = TIFF_HEAD_SIZE;
    tiff_word(0x4949, f);	/* Intel = little endian */
    tiff_word(42, f);
    tiff_long(tiff_end, f);

    /* write count of ifd entries */
    tiff_end += 2 /* sizeof(ifd_length) */;
    if (tiff4)
	ifd_length = 10;
    else {
	switch (preview_depth) {
	    case 24:
		/* extras are BitsPerPixel, SamplesPerPixel */
		ifd_length = 15;
		break;
	    case 8:
	    case 4:
		/* extras are BitsPerPixel, ColorMap */
		ifd_length = 15;
		break;
	    default:	/* bi-level */
		ifd_length = 13;
	}
    }
    tiff_word(ifd_length, f);

    tiff_end += ifd_length * TIFF_IFD_SIZE + 4 /* sizeof(ifd_next) */;
    ifd_next = 0;

    /* write each of the ifd entries */
    if (tiff4) {
	tiff_word(0xff, f);	    /* SubfileType */
	tiff_word(TIFF_SHORT, f);  /* value type */
	tiff_long(1, f);		    /* length */
	tiff_short(0, f);		    /* value */
    }
    else {
	tiff_word(0xfe, f);	/* NewSubfileType */
	tiff_word(TIFF_LONG, f);
	tiff_long(1, f);		    /* length */
	tiff_long(0, f);		    /* value */
    }

    tiff_word(0x100, f);	/* ImageWidth */
    if (tiff4) {
	tiff_word(TIFF_SHORT, f);
	tiff_long(1, f);
	tiff_short((short)width, f);
    }
    else {
	tiff_word(TIFF_LONG, f);
	tiff_long(1, f);
	tiff_long(width, f);
    }

    tiff_word(0x101, f);	/* ImageHeight */
    if (tiff4) {
	tiff_word(TIFF_SHORT, f);
	tiff_long(1, f);
	tiff_short((short)height, f);
    }
    else {
	tiff_word(TIFF_LONG, f);
	tiff_long(1, f);
	tiff_long(height, f);
    }

    if (!tiff4 && preview_depth>1) {
	tiff_word(0x102, f);	/* BitsPerSample */
	tiff_word(TIFF_SHORT, f);
	if (preview_depth == 24) {
	    tiff_long(3, f);
	    tiff_long(tiff_end, f);
	    tiff_end += 6;
	}
	else {
	    tiff_long(1, f);
	    tiff_short((WORD)preview_depth, f);
	}
    }

    tiff_word(0x103, f);	/* Compression */
    tiff_word(TIFF_SHORT, f);
    tiff_long(1, f);
    if (use_packbits)
	tiff_short(32773U, f);	/* packbits compression */
    else
	tiff_short(1, f);		/* no compression */

    tiff_word(0x106, f);	/* PhotometricInterpretation */
    tiff_word(TIFF_SHORT, f);
    tiff_long(1, f);
    if (tiff4 || preview_depth==1)
	tiff_short(1, f);		/* black is zero */
    else if (preview_depth==24)
	tiff_short(2, f);		/* RGB */
    else /* preview_depth == 4 or 8 */
	tiff_short(3, f);		/* Palette Color */

    tiff_word(0x111, f);	/* StripOffsets */
    tiff_word(TIFF_LONG, f);
    if (stripsperimage == 1) {
	/* This is messy and fragile */
	int len = 0;
	tiff_long(1, f);
	len += TIFF_RATIONAL_SIZE * 2;		/* resolutions */
	if (!tiff4) {
	    len += (((int)strlen(szGSviewName)+2)&~1) + 20;	/* software and date */
	    if (preview_depth == 4 || preview_depth == 8)
		len += 2 * 3*(1<<preview_depth);	/* palette */
	}
	tiff_long(tiff_end + len, f);
    }
    else {
	tiff_long(stripsperimage, f);
	tiff_long(tiff_end, f);
	tiff_end += (stripsperimage * 4 /* sizeof(DWORD) */);
    }

    if (!tiff4 && (preview_depth==24)) {
	tiff_word(0x115, f);	/* SamplesPerPixel */
	tiff_word(TIFF_SHORT, f);
	tiff_long(1, f);
	tiff_short(3, f);		/* 3 components */
    }

    tiff_word(0x116, f);	/* RowsPerStrip */
    tiff_word(TIFF_LONG, f);
    tiff_long(1, f);
    tiff_long(rowsperstrip, f);

    tiff_word(0x117, f);	/* StripByteCounts */
    tiff_word(TIFF_LONG, f);
    if (stripsperimage == 1) {
	tiff_long(1, f);
	if (use_packbits)
	    tiff_long(comp_length[0], f);
	else
	    tiff_long(bwidth * rowsperstrip, f);
    }
    else {
	tiff_long(stripsperimage, f);
	tiff_long(tiff_end, f);
	tiff_end += (stripsperimage * 4 /* sizeof(DWORD) */);
    }

    tiff_word(0x11a, f);	/* XResolution */
    tiff_word(TIFF_RATIONAL, f);
    tiff_long(1, f);
    tiff_long(tiff_end, f);
    tiff_end += TIFF_RATIONAL_SIZE;

    tiff_word(0x11b, f);	/* YResolution */
    tiff_word(TIFF_RATIONAL, f);
    tiff_long(1, f);
    tiff_long(tiff_end, f);
    tiff_end += TIFF_RATIONAL_SIZE;

    if (!tiff4) {
	tiff_word(0x128, f);	/* ResolutionUnit */
	tiff_word(TIFF_SHORT, f);
	tiff_long(1, f);
	tiff_short(2, f);		/* inches */

	tiff_word(0x131, f);	/* Software */
	tiff_word(TIFF_ASCII, f);
	i = (int)strlen(szGSviewName) + 1;
	tiff_long(i, f);
	tiff_long(tiff_end, f);
	tiff_end += i;
	if (tiff_end & 1) { /* pad to word boundary */
	    soft_extra = TRUE;
	    tiff_end++;
	}

	tiff_word(0x132, f);	/* DateTime */
	tiff_word(TIFF_ASCII, f);
	tiff_long(20, f);
	tiff_long(tiff_end, f);
	tiff_end += 20;

	if (preview_depth==4 || preview_depth==8) {
	    int palcount = 1<<preview_depth;
	    tiff_word(0x140, f);	/* ColorMap */
	    tiff_word(TIFF_SHORT, f);
	    tiff_long(3*palcount, f);  /* size of ColorMap */
	    tiff_long(tiff_end, f);
	    tiff_end += 2 * 3*palcount;
	}
    }


    /* write end of ifd tag */
    tiff_long(ifd_next, f);

    /* BitsPerSample for 24 bit colour */
    if (!tiff4 && (preview_depth==24)) {
	tiff_word(8, f);
	tiff_word(8, f);
	tiff_word(8, f);
    }

    /* strip offsets */
    end = tiff_end;
    if (stripsperimage > 1) {
	int stripwidth = bwidth * rowsperstrip;
	for (i=0; i<stripsperimage; i++) {
	    tiff_long(end, f);
	    if (use_packbits)
		end += comp_length[i];
	    else
		end += stripwidth;
	}
    }

    /* strip byte counts (after compression) */
    if (stripsperimage > 1) {
	for (i=0; i<stripsperimage; i++) {
	    if (use_packbits)
		tiff_long(comp_length[i], f);
	    else {
		is = i * rowsperstrip;
		lastrow = min( rowsperstrip, height - is);
		tiff_long(lastrow * bwidth, f);
	    }
	}
    }

    /* XResolution rational */
    tiff_long((int)xdpi, f);
    tiff_long(1, f);
    /* YResolution rational */
    tiff_long((int)ydpi, f);
    tiff_long(1, f);

    /* software and time strings */
    if (!tiff4) {
	time_t t;
	char now[20];
	struct tm* dt;
	gfile_write(f, szGSviewName, (int)strlen(szGSviewName)+1);
	if (soft_extra)
	    gfile_write(f, &nulchar, 1);
	t = time(NULL);
	dt = localtime(&t);
	snprintf(now, sizeof(now), "%04d:%02d:%02d %02d:%02d:%02d",
	    dt->tm_year+1900, dt->tm_mon+1, dt->tm_mday,
	    dt->tm_hour, dt->tm_min, dt->tm_sec);
	gfile_write(f, now, 20);
    }

    /* Palette */
    if (!tiff4 && ((preview_depth==4) || (preview_depth==8))) {
	int palcount = 1<<preview_depth;
	unsigned char r, g, b;
#define PALVAL(x) ((WORD)((x<< 8) | x))
	for (i=0; i<palcount; i++) {
	    image_colour(img->format, i, &r, &g, &b);
	    tiff_word(PALVAL(r), f);
	}
	for (i=0; i<palcount; i++) {
	    image_colour(img->format, i, &r, &g, &b);
	    tiff_word(PALVAL(g), f);
	}
	for (i=0; i<palcount; i++) {
	    image_colour(img->format, i, &r, &g, &b);
	    tiff_word(PALVAL(b), f);
	}
#undef PALVAL
    }


    if (topfirst) 
	line = img->image + img->raster * (img->height - yoffset - height);
    else
	line = img->image + img->raster * (yoffset+height-1);

    /* process each strip of bitmap */
    for (strip = 0; strip < stripsperimage; strip++) {
	int len;
	is = strip * rowsperstrip;
	lastrow = min( rowsperstrip, height - is);
	/* process each row of strip */
	for (i = 0; i < lastrow; i++) {
		if (preview_depth == 1) {
		    memset(preview,0,img->raster);
		    image_to_mono(img, preview, line);
		    for (j=0; j<temp_bwidth; j++)
			preview[j] ^= 0xff;
		}
		else if (preview_depth == 24)
		    image_to_24RGB(img, preview, line);
		else if (depth == preview_depth)
		    memmove(preview,  line, img->raster);
		if (bitoffset)
		    shift_bits(preview, temp_bwidth, bitoffset);
		if (use_packbits) {
		    len = (WORD)packbits(comp_line, preview, bwidth);
		    gfile_write(f, comp_line, len);
		}
		else
		    gfile_write(f, preview, bwidth);
		if (topfirst)
		    line += img->raster;
		else
		    line -= img->raster;
	}
    }

    if (use_packbits) {
	free(comp_length);
	free(comp_line);
    }
    free(preview);
    return 0;
}

/* Write an IMAGE as a TIFF file */
/* This is typically used to copy the display bitmap to a file */
int
image_to_tifffile(IMAGE* img, LPCTSTR filename, float xdpi, float ydpi)
{
    GFile *f;
    int code = 0;
    BOOL tiff4 = ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_1);

    if ((img == NULL) || (img->image == NULL))
	return -1;
    
    f = gfile_open(filename, gfile_modeWrite | gfile_modeCreate);
    if (f == (GFile *)NULL)
	return -1;

    code = image_to_tiff(f, img, 0, 0, img->width, img->height,
	xdpi, ydpi, tiff4, !tiff4);

    gfile_close(f);
    return 0;
}

/*********************************************************/

int
image_to_pnmfile(IMAGE* img, LPCTSTR filename, PNM_FORMAT pnm_format)
{
    PNM_FORMAT format = pnm_format;
    FILE *f;
    int bytewidth;
    unsigned char *row;
    unsigned char *bits;
    int topfirst;
    int i;
    if ((img == NULL) || (img->image == NULL))
	return -1;
    
    /* check if mono, grey or colour */
    if ((format != PBMRAW) && (format != PGMRAW) && (format != PPMRAW)) {
      switch (img->format & DISPLAY_COLORS_MASK) {
	case DISPLAY_COLORS_NATIVE:
	    switch (img->format & DISPLAY_DEPTH_MASK) {
		case DISPLAY_DEPTH_1:
		    format = PBMRAW;
		    break;
		case DISPLAY_DEPTH_4:
		case DISPLAY_DEPTH_8:
		case DISPLAY_DEPTH_16:
		    format = PPMRAW;
		    break;
		default:
		    return_error(-1);
	    }
	    break;
	case DISPLAY_COLORS_GRAY:
	    switch (img->format & DISPLAY_DEPTH_MASK) {
		case DISPLAY_DEPTH_1:
		    format = PBMRAW;
		    break;
		case DISPLAY_DEPTH_4:
		case DISPLAY_DEPTH_8:
		    /* Fixed gray palette */
		    format = PGMRAW;
		    break;
		default:
		    return_error(-1);
	    }
	    break;
	case DISPLAY_COLORS_RGB:
	    if ((img->format & DISPLAY_DEPTH_MASK) != DISPLAY_DEPTH_8)
		return_error(-1);
	    format = PPMRAW;
	    break;
	case DISPLAY_COLORS_CMYK:
	    /* convert */
	    format = PPMRAW;
	    break;
      }
    }
    if (format == PPMRAW)
	bytewidth = img->width * 3;
    else if (format == PGMRAW)
	bytewidth = img->width;
    else
	bytewidth = (img->width + 7) >> 3;
    row = (unsigned char *)malloc(bytewidth);
    if (row == NULL)
	return -1;
    topfirst = ((img->format & DISPLAY_FIRSTROW_MASK) == DISPLAY_TOPFIRST);

    f = csfopen(filename, TEXT("wb"));
    if (f == NULL) {
	free(row);
	return -1;
    }

    fprintf(f, "P%c\n", (int)(format+'0'));
    fprintf(f, "# Created by GSview\n");
    fprintf(f, "%d %d\n", img->width, img->height);
    if ((format == PGMRAW) || (format == PPMRAW))
        fprintf(f, "255\n");	/* max value */
    for (i=0; i<(int)img->height; i++) {
	if (topfirst)
	    bits = img->image + img->raster * i;
	else
	    bits = img->image + img->raster * (img->height - i - 1);
	if (format == PPMRAW)
	    image_to_24RGB(img, row, bits);
	else if (format == PGMRAW)
	    image_to_grey(img, row, bits);
	else
	    image_to_mono(img, row, bits);
        fwrite(row, 1, bytewidth, f);
    }
    
    free(row);
    fclose(f);
    return 0;
}

typedef enum PICTOp_e {
    PICT_NOP = 0x0000,
    PICT_Clip = 0x0001,
    PICT_VersionOp = 0x0011,
    PICT_DirectBitsRect = 0x009A, /* use this for 24/32 bits/pixel */
    PICT_OpEndPic = 0x00ff,
    PICT_Version = 0x02ff,
    PICT_HeaderOp = 0x0c00
} PICTOp;

/* Write PICT PixData
 * Return number of bytes generated.
 * Return -ve on error.
 * If file is NULL just return the number of bytes generated
 * without actually writing anything.
 */
static int 
write_pict_pixdata(IMAGE *img, GFile *f) 
{
    int i, j;
    int count;
    int topfirst;
    unsigned char b;
    unsigned char *bits;
    unsigned char *row, *sep, *packed;
    unsigned char *p;
    int wcount = 0;
    int rowwidth = img->width * 3;
    /* For the purposes of calculating the QuickDraw row width,
     * we must use 32-bits/pixel.
      */
    int qdrowwidth = img->width * 4;

    row = (unsigned char *)malloc(rowwidth);	/* 24-bit RGB */
    sep = (unsigned char *)malloc(qdrowwidth);	/* xRGB or RRGGBB */
    packed = (unsigned char *)malloc(rowwidth + rowwidth / 128 + 1);
    if ((row == NULL) || (sep == NULL) || (packed == NULL)) {
	if (row != NULL)
	    free(row);
	if (sep != NULL)
	    free(sep);
	if (packed != NULL)
	    free(packed);
	return -1;
    }

    /* With packType=4 we compress each component separately, red first */
    wcount = 0;
    topfirst = ((img->format & DISPLAY_FIRSTROW_MASK) == DISPLAY_TOPFIRST);
    for (i=0; i<(int)img->height; i++) {
	if (topfirst)
	    bits = img->image + img->raster * i;
	else
	    bits = img->image + img->raster * (img->height - i - 1);
	image_to_24RGB(img, row, bits);
	p = row;
	if (qdrowwidth < 8) {
	    /* Never compress short rows */
	    if (f) {
		for (j=0; j<(int)img->width; j++) {
		    /* Store as xRGB */
		    sep[4*j] = '\0';
		    sep[4*j+1] = *p++;
		    sep[4*j+2] = *p++;
		    sep[4*j+3] = *p++;
		}
		gfile_write(f, sep, qdrowwidth);
	    }
	    wcount += 8;
	}
	else {
	    for (j=0; j<(int)img->width; j++) {
		/* separate components */
		sep[j] = *p++;
		sep[j+img->width] = *p++;
		sep[j+img->width+img->width] = *p++;
	    }
	    count = packbits(packed, sep, rowwidth);
	    if (qdrowwidth > 250) {
		if (f)
		    write_bigendian_word((WORD)count, f);
		wcount += 2;
	    }
	    else {
		b = (unsigned char )count;
		if (f)
		    gfile_write(f, &b, 1);
		wcount += 1;
	    }
	    if (f)
		gfile_write(f, packed, count);
	    wcount += count;
	}
    }
    if (wcount & 1) {
	/* write out an even number of bytes for row data */
	b = 0;
	if (f)
	    gfile_write(f, &b, 1);
	wcount++;
    }

    return wcount;
}



/* Macintosh PICT */
/* Only writes 24-bit RGB */
int
image_to_pictfile(IMAGE* img, LPCTSTR filename, float xdpi, float ydpi)
{
    int i;
    int wcount;
    GFile *f = NULL;


    f = gfile_open(filename, gfile_modeWrite | gfile_modeCreate);
    if (f == (GFile *)NULL)
	return -1;

    /* Calculate compressed data length */
    wcount = write_pict_pixdata(img, NULL);

    /* PICT starts with 512 null bytes */
    for (i=0; i<128; i++)
        write_bigendian_dword(0, f);

    /* picture size - we will correct this later */
    /* 10 bytes */
    write_bigendian_word((WORD)(wcount+124), f);
    /* bounding box of rectangle */
    write_bigendian_word(0, f);			/* top */
    write_bigendian_word(0, f);			/* left */
    write_bigendian_word((WORD)img->height, f);	/* bottom */
    write_bigendian_word((WORD)img->width, f);	/* right */

    /* Imaging With QuickDraw: Appendix A - Picture Opcodes */
    /* 4 bytes */
    write_bigendian_word(PICT_VersionOp, f);
    write_bigendian_word(PICT_Version, f);

    /* Imaging With QuickDraw: Listing A-6 "A Sample Version 2 Picture" */
    /* 24 byte header + 2 byte NOP */
    write_bigendian_word(PICT_HeaderOp, f);
    write_bigendian_word((WORD)(-2), f); /* Version 2 extended */
    write_bigendian_word(0, f);			/* reserved */
    write_bigendian_dword((DWORD)(xdpi * 65536), f); /* best hRes */
    write_bigendian_dword((DWORD)(ydpi * 65536), f); /* best vRes */
    write_bigendian_word(0, f);			/* top */
    write_bigendian_word(0, f);			/* left */
    write_bigendian_word((WORD)img->height, f);	/* bottom */
    write_bigendian_word((WORD)img->width, f);	/* right */
    write_bigendian_word(0, f);			/* reserved */
    write_bigendian_word(PICT_NOP, f);
    /* 40 bytes to here */

    /* Clip */
    write_bigendian_word(PICT_Clip, f);
    write_bigendian_word(10, f);	/* size */
    write_bigendian_word(0, f);			/* top */
    write_bigendian_word(0, f);			/* left */
    write_bigendian_word((WORD)img->height, f);	/* bottom */
    write_bigendian_word((WORD)img->width, f);	/* right */

    /* Based on Imaging With QuickDraw  Listing A-2 for PackBitsRect */
    /* but modified to match DirectBitsRect output from Photoshop */
    write_bigendian_word(PICT_DirectBitsRect, f);
    write_bigendian_dword(0xff, f);	/* unknown */
    /* rowBytes: flags = 0x8000 for pixmap, row width */
    write_bigendian_word((WORD)(0x8000 | (img->width * 4)), f); /* rowBytes */
    write_bigendian_word(0, f);			/* bounds: top */
    write_bigendian_word(0, f);			/* bounds: left */
    write_bigendian_word((WORD)img->height, f);	/* bounds: bottom */
    write_bigendian_word((WORD)img->width, f);	/* bounds: right */
    write_bigendian_word(0, f); 	/* pmVersion */
    write_bigendian_word(4, f); 	/* packType: packbits by component */
    write_bigendian_dword(0, f); 	/* packSize: always 0 */
    write_bigendian_dword((DWORD)(xdpi * 65536), f); /* hRes: fixed */
    write_bigendian_dword((DWORD)(ydpi * 65536), f); /* vRes: fixed */
    write_bigendian_word(16, f); 	/* pixelType: RGBDirect */
    write_bigendian_word(32, f); 	/* pixelSize: 24-bit RGB */
    write_bigendian_word(3, f); 	/* cmpCount: 3 for RGB */
    write_bigendian_word(8, f); 	/* cmpSize: 8 bits/component */
    write_bigendian_dword(0, f); 	/* planeBytes */
    write_bigendian_dword(0, f); 	/* pmTable */
    write_bigendian_dword(0, f); 	/* pmReserved */

    /* srcRect */
    write_bigendian_word(0, f);			/* srcRect: top */
    write_bigendian_word(0, f);			/* srcRect: left */
    write_bigendian_word((WORD)img->height, f);	/* srcRect: bottom */
    write_bigendian_word((WORD)img->width, f);	/* srcRect: right */
    
    /* destRect */
    write_bigendian_word(0, f);			/* destRect: top */
    write_bigendian_word(0, f);			/* destRect: left */
    write_bigendian_word((WORD)img->height, f);	/* destRect: bottom */
    write_bigendian_word((WORD)img->width, f);	/* destRect: right */

    /* mode */
    write_bigendian_word(0x40, f);			/* transfer mode */

    /* PixData */
    write_pict_pixdata(img, f);

    write_bigendian_word(PICT_OpEndPic, f);

    gfile_close(f);
    return 0;
}

#ifdef HAVE_LIBPNG

static void
image_png_flush(png_structp png_ptr)
{
    png_FILE_p io_ptr;
    io_ptr = (png_FILE_p)CVT_PTR((png_ptr->io_ptr));
    if (io_ptr != NULL)
	fflush(io_ptr);
}

static void
image_png_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
   int count = fwrite(data, 1, length, (png_FILE_p)png_ptr->io_ptr);
   if (count != (int)length)
      png_error(png_ptr, "Write Error");
}

static void
image_png_read_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
   int count;
   count = fread(data, 1, length, (png_FILE_p)png_ptr->io_ptr);
   if (count != (int)length)
      png_error(png_ptr, "Read Error!");
}

int
image_to_pngfile(IMAGE* img, LPCTSTR filename)
{
    FILE *f;
    png_structp png_ptr;
    png_infop info_ptr;
    int colour_type;
    int bit_depth;
    int num_palette = 0;
    png_color palette[96];
    BOOL invert_mono = FALSE;
    BOOL topfirst;
    unsigned char *bits;
    unsigned char *row = NULL;
    int i;

    if ((img == NULL) || (img->image == NULL))
	return -1;

    switch (img->format & DISPLAY_COLORS_MASK) {
	case DISPLAY_COLORS_NATIVE:
	    switch (img->format & DISPLAY_DEPTH_MASK) {
		case DISPLAY_DEPTH_1:
		    colour_type = PNG_COLOR_TYPE_GRAY;
		    bit_depth = 1;
		    invert_mono = TRUE;
		    break;
		case DISPLAY_DEPTH_4:
		    colour_type = PNG_COLOR_TYPE_PALETTE;
		    bit_depth = 4;
		    num_palette = 16;
		    break;
		case DISPLAY_DEPTH_8:
		    colour_type = PNG_COLOR_TYPE_PALETTE;
		    num_palette = 96;
		    bit_depth = 8;
		    break;
		case DISPLAY_DEPTH_16:
		    /* convert */
		    colour_type = PNG_COLOR_TYPE_RGB;
		    bit_depth = 8;
		    break;
		default:
		    return_error(-1);
	    }
	    break;
	case DISPLAY_COLORS_GRAY:
	    switch (img->format & DISPLAY_DEPTH_MASK) {
		case DISPLAY_DEPTH_1:
		    colour_type = PNG_COLOR_TYPE_GRAY;
		    bit_depth = 1;
		    break;
		case DISPLAY_DEPTH_4:
		    colour_type = PNG_COLOR_TYPE_GRAY;
		    bit_depth = 4;
		    break;
		case DISPLAY_DEPTH_8:
		    colour_type = PNG_COLOR_TYPE_GRAY;
		    bit_depth = 8;
		    break;
		default:
		    return_error(-1);
	    }
	    break;
	case DISPLAY_COLORS_RGB:
	    if ((img->format & DISPLAY_DEPTH_MASK) != DISPLAY_DEPTH_8)
		return_error(-1);
	    colour_type = PNG_COLOR_TYPE_RGB;
	    bit_depth = 8;
	    break;
	case DISPLAY_COLORS_CMYK:
	    /* convert */
	    colour_type = PNG_COLOR_TYPE_RGB;
	    bit_depth = 8;
	    break;
	default:
	    return -1;
    }

    if (num_palette > sizeof(palette)/sizeof(palette[0]))
	return -1;
    if (colour_type == PNG_COLOR_TYPE_RGB) {
	row = (unsigned char *)malloc(img->width * 3);
	if (row == NULL)
	    return -1;
    }

    f = csfopen(filename, TEXT("wb"));
    if (f == NULL) {
	if (row)
	    free(row);
	return -1;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
	(png_voidp)NULL, NULL, NULL);
    if (png_ptr == NULL) {
	fclose(f);
	if (row)
	    free(row);
	return -1;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
	png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
	fclose(f);
	return -1;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(f);
	if (row)
	    free(row);
	return -1;
    }

    png_set_write_fn(png_ptr, (png_voidp)f, image_png_write_data,
	image_png_flush);

    png_set_IHDR(png_ptr, info_ptr, img->width, img->height,
	bit_depth, colour_type, PNG_INTERLACE_NONE,
	PNG_COMPRESSION_TYPE_DEFAULT,
	PNG_FILTER_TYPE_DEFAULT);

    if (colour_type == PNG_COLOR_TYPE_PALETTE) {
	for (i=0; i<num_palette; i++)
	    image_colour(img->format, i, 
		&palette[i].red, &palette[i].green, &palette[i].blue);
	png_set_PLTE(png_ptr, info_ptr, palette, num_palette);
    }
    if (invert_mono)
	png_set_invert_mono(png_ptr);

    png_write_info(png_ptr, info_ptr);

    topfirst = ((img->format & DISPLAY_FIRSTROW_MASK) == DISPLAY_TOPFIRST);
    for (i=0; i<(int)img->height; i++) {
	if (topfirst)
	    bits = img->image + img->raster * i;
	else
	    bits = img->image + img->raster * (img->height - i - 1);
	if (colour_type == PNG_COLOR_TYPE_RGB) {
	    image_to_24RGB(img, row, bits);
	    bits = row;
	}
	png_write_row(png_ptr, bits);
    }
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(f);
    if (row)
	free(row);
    return 0;
}

IMAGE *
pngfile_to_image(LPCTSTR filename)
{
    FILE *f;
    png_structp png_ptr;
    png_infop info_ptr;
    png_infop end_info;
    unsigned char png_buf[8];
    png_uint_32 width;
    png_uint_32 height;
    int bit_depth;
    int color_type;
    int interlace_type;
    int compression_method;
    int filter_method;
    unsigned char **rows = NULL;
    unsigned char *bits = NULL;
    int nbytes;
    int raster;
    int i;
    IMAGE *img = NULL;

    f = csfopen(filename, TEXT("rb"));
    if (f == (FILE *)NULL)
	return NULL;
    fread(png_buf, 1, sizeof(png_buf), f);
    if (png_sig_cmp(png_buf, 0, sizeof(png_buf))!=0) {
	/* Not a PNG file */
	fclose(f);
	return NULL;
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
	(png_voidp)NULL, NULL, NULL);
    if (png_ptr == NULL) {
	fclose(f);
	return NULL;
    }
    png_set_read_fn(png_ptr, (png_voidp)f, image_png_read_data);

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
	png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
	fclose(f);
	return NULL;
    }

    end_info = png_create_info_struct(png_ptr);
    if (end_info == NULL) {
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
	fclose(f);
	return NULL;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	fclose(f);
	if (rows)
	    free(rows);
	if (bits)
	    free(bits);
	if (img)
	    free(img);
	return NULL;
    }

    png_set_sig_bytes(png_ptr, sizeof(png_buf));

    png_read_info(png_ptr, info_ptr);

    png_get_IHDR(png_ptr, info_ptr, &width, &height, 
	&bit_depth, &color_type, &interlace_type,
	&compression_method, &filter_method);


    if (color_type == PNG_COLOR_TYPE_PALETTE)
	png_set_palette_to_rgb(png_ptr);

    if (color_type == PNG_COLOR_TYPE_GRAY)
	nbytes = 1;
    else
	nbytes = 3;

    if ((color_type == PNG_COLOR_TYPE_GRAY) &&
	(bit_depth < 8))
	png_set_gray_1_2_4_to_8(png_ptr);

    if (bit_depth == 16)
	png_set_strip_16(png_ptr);

    if (color_type & PNG_COLOR_MASK_ALPHA)
	png_set_strip_alpha(png_ptr);


    /* Allocate memory, and set row pointers */
    raster = (width * nbytes + 3) & ~3;
    rows = (unsigned char **)malloc(height * sizeof(unsigned char *));
    if (rows == NULL)
	longjmp(png_jmpbuf(png_ptr),-1);
    bits = (unsigned char *)malloc(raster * height);
    if (bits == NULL)
	longjmp(png_jmpbuf(png_ptr),-1);
    for (i=0; i<(int)height; i++)
	rows[i] = bits + i * raster;

    png_read_rows(png_ptr, rows, NULL, height);

    png_read_end(png_ptr, end_info);

    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    fclose(f);
    if (rows)
	free(rows);

    img = (IMAGE *)malloc(sizeof(IMAGE));
    if (img == NULL)
	free(bits);
    else {
	img->width = width;
	img->height = height;
	img->raster = raster;
	img->format = (nbytes == 1) 
	    ? (DISPLAY_COLORS_GRAY | DISPLAY_ALPHA_NONE |
	       DISPLAY_DEPTH_8 | DISPLAY_BIGENDIAN | DISPLAY_TOPFIRST)
	    : (DISPLAY_COLORS_RGB | DISPLAY_ALPHA_NONE |
	       DISPLAY_DEPTH_8 | DISPLAY_BIGENDIAN | DISPLAY_TOPFIRST);
	img->image = bits;
    }

    return img;
}

#else /* HAVE_LIBPNG */
int
image_to_pngfile(IMAGE* img, LPCTSTR filename)
{
    /* not supported */
    return -1;
}

IMAGE *
pngfile_to_image(LPCTSTR filename)
{
    /* not supported */
    return NULL;
}
#endif /* HAVE_LIBPNG */


/*********************************************************/

