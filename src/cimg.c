/* Copyright (C) 2001-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cimg.c,v 1.21 2005/06/10 09:39:24 ghostgum Exp $ */
/* Common image format and conversion functions */

#include "common.h"
#include "gdevdsp.h"
#include "cimg.h"
#include "clzw.h"

static int a85write(char *line, unsigned char *buf, int count);

/* Return a palette entry for given format and index */
void
image_colour(unsigned int format, int index, 
    unsigned char *r, unsigned char *g, unsigned char *b)
{
    switch (format & DISPLAY_COLORS_MASK) {
	case DISPLAY_COLORS_NATIVE:
	    switch (format & DISPLAY_DEPTH_MASK) {
		case DISPLAY_DEPTH_1:
		    *r = *g = *b = (unsigned char)(index ? 0 : 255);
		    break;
		case DISPLAY_DEPTH_4:
		    {
		    int one = index & 8 ? 255 : 128;
		    *r = (unsigned char)(index & 4 ? one : 0);
		    *g = (unsigned char)(index & 2 ? one : 0);
		    *b = (unsigned char)(index & 1 ? one : 0);
		    }
		    break;
		case DISPLAY_DEPTH_8:
		    /* palette of 96 colours */
		    /* 0->63 = 00RRGGBB, 64->95 = 010YYYYY */
		    if (index < 64) {
			int one = 255 / 3;
			*r = (unsigned char)(((index & 0x30) >> 4) * one);
			*g = (unsigned char)(((index & 0x0c) >> 2) * one);
			*b = (unsigned char)( (index & 0x03) * one);
		    }
		    else {
			int val = index & 0x1f;
			*r = *g = *b = (unsigned char)((val << 3) + (val >> 2));
		    }
		    break;
	    }
	    break;
	case DISPLAY_COLORS_GRAY:
	    switch (format & DISPLAY_DEPTH_MASK) {
		case DISPLAY_DEPTH_1:
		    *r = *g = *b = (unsigned char)(index ? 255 : 0);
		    break;
		case DISPLAY_DEPTH_4:
		    *r = *g = *b = (unsigned char)((index<<4) + index);
		    break;
		case DISPLAY_DEPTH_8:
		    *r = *g = *b = (unsigned char)index;
		    break;
	    }
	    break;
    }
}

unsigned char 
colour_to_grey(unsigned char r, unsigned char g, unsigned char b)
{
    return (unsigned char)
	((r * 77) / 255 + (g * 150) / 255 + (b * 28) / 255);
}


/********************************************************/
/* 24BGR */

/* convert one line of 4-bit native to 24BGR */
void
image_4native_to_24BGR(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    int val;
    for (i=0; i<width; i++) {
	val = *source;
	if (i & 1)
	    source++;
	else
	    val >>= 4;
	val &= 0x0f;
	image_colour(DISPLAY_COLORS_NATIVE | DISPLAY_DEPTH_4,
		val, dest+2, dest+1, dest);
	dest += 3;
    }
}


/* convert one line of 16BGR555 to 24BGR */
/* byte0=GGGBBBBB byte1=0RRRRRGG */
void
image_16BGR555_to_24BGR(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    WORD w;
    unsigned char value;
    for (i=0; i<width; i++) {
	w = (WORD)(source[0] + (source[1] << 8));
	value =   (unsigned char)(w & 0x1f);		/* blue */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	value =   (unsigned char)((w >> 5) & 0x1f);	/* green */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	value =   (unsigned char)((w >> 10) & 0x1f);	/* red */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	source += 2;
    }
}

/* convert one line of 16BGR565 to 24BGR */
/* byte0=GGGBBBBB byte1=RRRRRGGG */
void
image_16BGR565_to_24BGR(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    WORD w;
    unsigned char value;
    for (i=0; i<width; i++) {
	w = (WORD)(source[0] + (source[1] << 8));
	value =   (unsigned char)(w & 0x1f);		/* blue */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	value =   (unsigned char)((w >> 5) & 0x3f);	/* green */
	*dest++ = (unsigned char)((value << 2) + (value >> 4));
	value =   (unsigned char)((w >> 11) & 0x1f);	/* red */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	source += 2;
    }
}

/* convert one line of 16RGB555 to 24BGR */
/* byte0=0RRRRRGG byte1=GGGBBBBB */
void
image_16RGB555_to_24BGR(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    WORD w;
    unsigned char value;
    for (i=0; i<width; i++) {
	w = (WORD)((source[0] << 8) + source[1]);
	value =   (unsigned char)(w & 0x1f);		/* blue */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	value =   (unsigned char)((w >> 5) & 0x1f);	/* green */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	value =   (unsigned char)((w >> 10) & 0x1f);	/* red */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	source += 2;
    }
}

/* convert one line of 16RGB565 to 24BGR */
/* byte0=RRRRRGGG byte1=GGGBBBBB */
void
image_16RGB565_to_24BGR(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    WORD w;
    unsigned char value;
    for (i=0; i<width; i++) {
	w = (WORD)((source[0] << 8) + source[1]);
	value =   (unsigned char)(w & 0x1f);		/* blue */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	value =   (unsigned char)((w >> 5) & 0x3f);	/* green */
	*dest++ = (unsigned char)((value << 2) + (value >> 4));
	value =   (unsigned char)((w >> 11) & 0x1f);	/* red */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	source += 2;
    }
}


/* convert one line of 32CMYK to 24BGR */
void
image_32CMYK_to_24BGR(int width, unsigned char *dest, unsigned char *source,
    int sep)
{
    int i;
    int cyan, magenta, yellow, black;
    for (i=0; i<width; i++) {
	cyan = source[0];
	magenta = source[1];
	yellow = source[2];
	black = source[3];
	if (!(sep & SEP_CYAN))
	    cyan = 0;
	if (!(sep & SEP_MAGENTA))
	    magenta = 0;
	if (!(sep & SEP_YELLOW))
	    yellow = 0;
	if (!(sep & SEP_BLACK))
	    black = 0;
	*dest++ = 
	    (unsigned char)((255 - yellow)  * (255 - black)/255); /* blue */
	*dest++ = 
	    (unsigned char)((255 - magenta) * (255 - black)/255); /* green */
	*dest++ = 
	     (unsigned char)((255 - cyan)    * (255 - black)/255); /* red */
	source += 4;
    }
}

int
image_to_24BGR(IMAGE *img, unsigned char *dest, unsigned char *source)
{
    unsigned char *d = dest;
    unsigned char *s = source;
    int width = img->width; 
    unsigned int alpha = img->format & DISPLAY_ALPHA_MASK;
    BOOL bigendian = (img->format & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN;
    int code = -1;
    int i;

    switch (img->format & DISPLAY_COLORS_MASK) {
	case DISPLAY_COLORS_NATIVE:
	    if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_1) {
		image_1native_to_24RGB(width, dest, source); /* monochrome */
		code = 0;
	    }
	    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_4) {
		image_4native_to_24BGR(width, dest, source);
		code = 0;
	    }
	    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
	        for (i=0; i<width; i++) {
		    image_colour(img->format, *s++, d+2, d+1, d);
		    d+= 3;
		}
		code = 0;
	    }
	    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_16) {
		if (bigendian) {
		    if ((img->format & DISPLAY_555_MASK)
			== DISPLAY_NATIVE_555)
			image_16RGB555_to_24BGR(img->width, 
			    dest, source);
		    else
			image_16RGB565_to_24BGR(img->width, 
			    dest, source);
		}
		else {
		    if ((img->format & DISPLAY_555_MASK)
			== DISPLAY_NATIVE_555) {
			image_16BGR555_to_24BGR(img->width, 
			    dest, source);
		    }
		    else
			image_16BGR565_to_24BGR(img->width, 
			    dest, source);
		}
		code = 0;
	    }
	    break;
	case DISPLAY_COLORS_GRAY:
	    if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_1) {
		image_1grey_to_24RGB(width, dest, source);
		code = 0;
	    }
	    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_4) {
		image_4grey_to_24RGB(width, dest, source);
		code = 0;
	    }
	    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
		image_8grey_to_24RGB(width, dest, source);
		code = 0;
	    }
	    break;
	case DISPLAY_COLORS_RGB:
	    if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
		for (i=0; i<width; i++) {
		    if ((alpha == DISPLAY_ALPHA_FIRST) || 
			(alpha == DISPLAY_UNUSED_FIRST))
			s++;
		    if (bigendian) {
			*d++ = s[2];
			*d++ = s[1];
			*d++ = s[0];
			s+=3;
		    }
		    else {
			*d++ = *s++;
			*d++ = *s++;
			*d++ = *s++;
		    }
		    if ((alpha == DISPLAY_ALPHA_LAST) || 
			(alpha == DISPLAY_UNUSED_LAST))
			s++;
		}
		code = 0;
	    }
	    break;
	case DISPLAY_COLORS_CMYK:
	    if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
		image_32CMYK_to_24BGR(width, dest, source, 
		    SEP_CYAN | SEP_MAGENTA | SEP_YELLOW | SEP_BLACK);
		code = 0;
	    }
	    break;
    }
    return code;
}

/********************************************************/
/* 24RGB */

/* convert one line of 16BGR555 to 24RGB */
/* byte0=GGGBBBBB byte1=0RRRRRGG */
void
image_16BGR555_to_24RGB(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    WORD w;
    unsigned char value;
    for (i=0; i<width; i++) {
	w = (WORD)(source[0] + (source[1] << 8));
	value =   (unsigned char)((w >> 10) & 0x1f);	/* red */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	value =   (unsigned char)((w >> 5) & 0x1f);	/* green */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	value =   (unsigned char)(w & 0x1f);		/* blue */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	source += 2;
    }
}

/* convert one line of 16BGR565 to 24RGB */
/* byte0=GGGBBBBB byte1=RRRRRGGG */
void
image_16BGR565_to_24RGB(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    WORD w;
    unsigned char value;
    for (i=0; i<width; i++) {
	w = (WORD)(source[0] + (source[1] << 8));
	value =   (unsigned char)((w >> 11) & 0x1f);	/* red */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	value =   (unsigned char)((w >> 5) & 0x3f);	/* green */
	*dest++ = (unsigned char)((value << 2) + (value >> 4));
	value =   (unsigned char)(w & 0x1f);		/* blue */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	source += 2;
    }
}

/* convert one line of 16RGB555 to 24RGB */
/* byte0=0RRRRRGG byte1=GGGBBBBB */
void
image_16RGB555_to_24RGB(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    WORD w;
    unsigned char value;
    for (i=0; i<width; i++) {
	w = (WORD)((source[0] << 8) + source[1]);
	value =   (unsigned char)((w >> 10) & 0x1f);	/* red */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	value =   (unsigned char)((w >> 5) & 0x1f);	/* green */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	value =   (unsigned char)(w & 0x1f);		/* blue */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	source += 2;
    }
}

/* convert one line of 16RGB565 to 24RGB */
/* byte0=RRRRRGGG byte1=GGGBBBBB */
void
image_16RGB565_to_24RGB(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    WORD w;
    unsigned char value;
    for (i=0; i<width; i++) {
	w = (WORD)((source[0] << 8) + source[1]);
	value =   (unsigned char)((w >> 11) & 0x1f);	/* red */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	value =   (unsigned char)((w >> 5) & 0x3f);	/* green */
	*dest++ = (unsigned char)((value << 2) + (value >> 4));
	value =   (unsigned char)(w & 0x1f);		/* blue */
	*dest++ = (unsigned char)((value << 3) + (value >> 2));
	source += 2;
    }
}


/* convert one line of 32CMYK to 24RGB */
void
image_32CMYK_to_24RGB(int width, unsigned char *dest, unsigned char *source,
    int sep)
{
    int i;
    int cyan, magenta, yellow, black;
    for (i=0; i<width; i++) {
	cyan = source[0];
	magenta = source[1];
	yellow = source[2];
	black = source[3];
	if (!(sep & SEP_CYAN))
	    cyan = 0;
	if (!(sep & SEP_MAGENTA))
	    magenta = 0;
	if (!(sep & SEP_YELLOW))
	    yellow = 0;
	if (!(sep & SEP_BLACK))
	    black = 0;
	*dest++ = (unsigned char)
	    ((255 - cyan)    * (255 - black)/255); /* red */
	*dest++ = (unsigned char)
	    ((255 - magenta) * (255 - black)/255); /* green */
	*dest++ = (unsigned char)
	    ((255 - yellow)  * (255 - black)/255); /* blue */
	source += 4;
    }
}

/* convert one line of 1-bit gray to 24RGB */
void
image_1grey_to_24RGB(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    for (i=0; i<width; i++) {
	if (*source & (1 << (7 - (i & 7))))
	    dest[0] = dest[1] = dest[2] = 255;
	else 
	    dest[0] = dest[1] = dest[2] = 0;
	dest += 3;
	if ((i & 7) == 7)
	    source++;
    }
}

/* convert one line of 4-bit grey to 24RGB */
void
image_4grey_to_24RGB(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    int val;
    for (i=0; i<width; i++) {
	if (i & 1) {
	    val = (*source) & 0xf;
	    source++;
	}
	else
	    val = (*source >> 4) & 0xf;
	val = val + (val << 4);
	dest[0] = dest[1] = dest[2] = (unsigned char)val;
	dest += 3;
    }
}

/* convert one line of 8-bit grey to 24RGB */
void
image_8grey_to_24RGB(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    for (i=0; i<width; i++) {
	dest[0] = dest[1] = dest[2] = *source++;
	dest += 3;
    }
}

/* convert one line of 1-bit native to 24RGB */
void
image_1native_to_24RGB(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    for (i=0; i<width; i++) {
	if (*source & (1 << (7 - (i & 7))))
	    dest[0] = dest[1] = dest[2] = 0;
	else 
	    dest[0] = dest[1] = dest[2] = 255;
	dest += 3;
	if ((i & 7) == 7)
	    source++;
    }
}

/* convert one line of 4-bit native to 24RGB */
void
image_4native_to_24RGB(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    int val;
    for (i=0; i<width; i++) {
	val = *source;
	if (i & 1)
	    source++;
	else
	    val >>= 4;
	val &= 0x0f;
	image_colour(DISPLAY_COLORS_NATIVE | DISPLAY_DEPTH_4,
		val, dest, dest+1, dest+2);
	dest += 3;
    }
}

int
image_to_24RGB(IMAGE *img, unsigned char *dest, unsigned char *source)
{
    unsigned char *d = dest;
    unsigned char *s = source;
    int width = img->width; 
    unsigned int alpha = img->format & DISPLAY_ALPHA_MASK;
    BOOL bigendian = (img->format & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN;
    int i;
    int code = -1;

    switch (img->format & DISPLAY_COLORS_MASK) {
	case DISPLAY_COLORS_NATIVE:
	    if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_1) {
		image_1native_to_24RGB(img->width, dest, source);
		code = 0;
	    }
	    if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_4) {
		image_4native_to_24RGB(img->width, dest, source);
		code = 0;
	    }
	    if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
	        for (i=0; i<width; i++) {
		    image_colour(img->format, *s++, d, d+1, d+2);
		    d+= 3;
		}
		code = 0;
	    }
	    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_16) {
		if (bigendian) {
		    if ((img->format & DISPLAY_555_MASK)
			== DISPLAY_NATIVE_555)
			image_16RGB555_to_24RGB(img->width, 
			    dest, source);
		    else
			image_16RGB565_to_24RGB(img->width, 
			    dest, source);
		}
		else {
		    if ((img->format & DISPLAY_555_MASK)
			== DISPLAY_NATIVE_555) {
			image_16BGR555_to_24RGB(img->width, 
			    dest, source);
		    }
		    else
			image_16BGR565_to_24RGB(img->width, 
			    dest, source);
		}
		code = 0;
	    }
	    break;
	case DISPLAY_COLORS_GRAY:
	    if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_1) {
		image_1grey_to_24RGB(width, dest, source);
		code = 0;
	    }
	    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_4) {
		image_4grey_to_24RGB(width, dest, source);
		code = 0;
	    }
	    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
		image_8grey_to_24RGB(width, dest, source);
		code = 0;
	    }
	    break;
	case DISPLAY_COLORS_RGB:
	    if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
		for (i=0; i<width; i++) {
		    if ((alpha == DISPLAY_ALPHA_FIRST) || 
			(alpha == DISPLAY_UNUSED_FIRST))
			s++;
		    if (bigendian) {
			*d++ = *s++;
			*d++ = *s++;
			*d++ = *s++;
		    }
		    else {
			*d++ = s[2];
			*d++ = s[1];
			*d++ = s[0];
			s+=3;
		    }
		    if ((alpha == DISPLAY_ALPHA_LAST) || 
			(alpha == DISPLAY_UNUSED_LAST))
			s++;
		}
		code = 0;
	    }
	    break;
	case DISPLAY_COLORS_CMYK:
	    if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
		image_32CMYK_to_24RGB(width, dest, source, 
		    SEP_CYAN | SEP_MAGENTA | SEP_YELLOW | SEP_BLACK);
		code = 0;
	    }
	    break;
    }
    return code;
}

/********************************************************/
/* Grey */

void
image_1grey_to_8grey(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    for (i=0; i<width; i++) {
	if (*source & (1 << (7 - (i & 7))))
	    *dest++ = 255;
	else 
	    *dest++ = 0;
	if ((i & 7) == 7)
	    source++;
    }
}

void
image_4grey_to_8grey(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    int value;
    for (i=0; i<width; i++) {
	if (i & 1)
	    value = (*source >> 4) & 0x0f;
	else {
	    value = (*source) & 0x0f;
	    source++;
	}
	*dest++ = (unsigned char)(value + (value << 4));
    }
}

void
image_1native_to_8grey(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    for (i=0; i<width; i++) {
	if (*source & (1 << (7 - (i & 7))))
	    *dest++ = 0;
	else 
	    *dest++ = 255;
	if ((i & 7) == 7)
	    source++;
    }
}

void
image_4native_to_8grey(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    int value;
    unsigned char r, g, b;
    for (i=0; i<width; i++) {
	if (i & 1)
	    value = (*source >> 4) & 0x0f;
	else {
	    value = (*source) & 0x0f;
	    source++;
	}
	image_colour(DISPLAY_COLORS_NATIVE | DISPLAY_DEPTH_4,
	    value, &r, &g, &b);
	*dest++ = colour_to_grey(r,g,b);
    }
}

void
image_8native_to_8grey(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    int value;
    unsigned char r, g, b;
    for (i=0; i<width; i++) {
	value = *source++;
	image_colour(DISPLAY_COLORS_NATIVE | DISPLAY_DEPTH_8,
	    value, &r, &g, &b);
	*dest++ = colour_to_grey(r,g,b);
    }
}


void
image_24RGB_to_8grey(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    for (i=0; i<width; i++) {
	*dest++ = colour_to_grey(source[0], source[1], source[2]);
	source+=3;
    }
}

void
image_24BGR_to_8grey(int width, unsigned char *dest, unsigned char *source)
{
    int i;
    for (i=0; i<width; i++) {
	*dest++ = colour_to_grey(source[2], source[1], source[0]);
	source+=3;
    }
}

static int
image_to_8grey(IMAGE *img, unsigned char *dest, unsigned char *source)
{
    /* This is the fallback method which converts to 24RGB first */
    unsigned char *row = (unsigned char *)malloc(img->width*3);
    int code = -1;
    if (row) {
	code = image_to_24RGB(img, row, source);
	image_24RGB_to_8grey(img->width, dest, row);
	free(row);
    }
    return code;
}

/* Convert any format to 8-bit grey.
 * Returns -ve if unsupported format.
 */
int
image_to_grey(IMAGE *img, unsigned char *dest, unsigned char *source)
{
    int width = img->width;
    int code = -1;
    unsigned int alpha = img->format & DISPLAY_ALPHA_MASK;
    switch (img->format & DISPLAY_COLORS_MASK) {
	case DISPLAY_COLORS_NATIVE:
	    if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_1) {
		image_1native_to_8grey(width, dest, source);
		code = 0;
	    }
	    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_4) {
		image_4native_to_8grey(width, dest, source);
		code = 0;
	    }
	    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
		image_8native_to_8grey(width, dest, source);
		code = 0;
	    }
	    else
		code = image_to_8grey(img, dest, source);
	    break;
	case DISPLAY_COLORS_GRAY:
	    if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_1) {
		image_1grey_to_8grey(width, dest, source);
	        code = 0;
	    }
	    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_4) {
		image_4grey_to_8grey(width, dest, source);
	        code = 0;
	    }
	    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
		memcpy(dest, source, width);
		code = 0;
	    }
	    break;
	case DISPLAY_COLORS_RGB:
	    if ((img->format & DISPLAY_DEPTH_MASK) != DISPLAY_DEPTH_8)
		code = -1;
	    else if (alpha == DISPLAY_ALPHA_NONE) {
		if ((img->format & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN)
		    image_24RGB_to_8grey(width, dest, source);
		else
		    image_24BGR_to_8grey(width, dest, source);
		code = 0;
	    }
	    else
		code = image_to_8grey(img, dest, source);
	    break;
	case DISPLAY_COLORS_CMYK:
	    if ((img->format & DISPLAY_DEPTH_MASK) != DISPLAY_DEPTH_8)
		code = -1;
	    else
		code = image_to_8grey(img, dest, source);
	    break;
    }

    return code;
}

/********************************************************/


/* Convert a line of image to monochrome with 0=white, 1=black.
 * Returns -ve if unsupported format.
 */
int
image_to_mono(IMAGE *img, unsigned char *dest, unsigned char *source)
{
    int width = img->width;
    unsigned int alpha = img->format & DISPLAY_ALPHA_MASK;
    BOOL bigendian = (img->format & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN;
    int bwidth = ((width + 7) & ~7) >> 3; /* byte width with 1 bit/pixel */
    unsigned char omask = 0x80;
    int oroll = 7;
    int black;
    unsigned char *s = source;
    unsigned char mask0 = 0;
    unsigned char mask1 = 0;
    unsigned char mask2 = 0;
    unsigned char mask3 = 0;
    int bytes_per_pixel = 0;
    BOOL additive = TRUE;
    int j;

    switch (img->format & DISPLAY_COLORS_MASK) {
	case DISPLAY_COLORS_NATIVE:
	    if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_1) {
		for (j = 0; j < bwidth ; j++)
		    dest[j] = source[j];
		return 0;
	    }
	    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_4) {
		for (j = 0; j < width; j++) {
		    if (j & 1) {
			black = (*s & 0x0f) != 0x0f;
			s++;
		    }
		    else
			black = (*s & 0xf0) != 0xf0;
		    if (black) 
			dest[j/8] |= omask;
		    else
			dest[j/8] &= (unsigned char)(~omask);
		    oroll--;
		    omask >>= 1;
		    if (oroll < 0) {
			omask = 0x80;
			oroll = 7;
		    }
		}
		return 0;
	    }
	    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
		for (j = 0; j < width; j++) {
		    black = (*s != 0x3f) && (*s != 0x5f);
		    s++;
		    if (black) 
			dest[j/8] |= omask;
		    else
			dest[j/8] &= (unsigned char)(~omask);
		    oroll--;
		    omask >>= 1;
		    if (oroll < 0) {
			omask = 0x80;
			oroll = 7;
		    }
		}
		return 0;
	    }
	    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_16) {
		bytes_per_pixel = 2;
		if (bigendian) {
		    if ((img->format & DISPLAY_555_MASK)
			== DISPLAY_NATIVE_555) {
			mask0 = 0x7f;
			mask1 = 0xff;
		    }
		    else {
			mask0 = 0xff;
			mask1 = 0xff;
		    }
		}
		else {
		    if ((img->format & DISPLAY_555_MASK)
			== DISPLAY_NATIVE_555) {
			mask0 = 0xff;
			mask1 = 0x7f;
		    }
		    else {
			mask0 = 0xff;
			mask1 = 0xff;
		    }
		}
	    }
	    break;
	case DISPLAY_COLORS_GRAY:
	    if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_1) {
		for (j = 0; j < bwidth ; j++)
		    dest[j] = (unsigned char)~source[j];
		return 0;
	    }
	    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_4) {
		for (j = 0; j < width; j++) {
		    if (j & 1) {
			black = (*s & 0x0f) != 0x0f;
		        s++;
		    }
		    else
			black = (*s & 0xf0) != 0xf0;
		    if (black) 
			dest[j/8] |= omask;
		    else
			dest[j/8] &= (unsigned char)(~omask);
		    oroll--;
		    omask >>= 1;
		    if (oroll < 0) {
			omask = 0x80;
			oroll = 7;
		    }
		}
		return 0;
	    }
	    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
		bytes_per_pixel = 1;
		mask0 = 0xff;
	    }
	    else
		return -1;
	    break;
	case DISPLAY_COLORS_RGB:
	    if ((img->format & DISPLAY_DEPTH_MASK) != DISPLAY_DEPTH_8)
		return -1;

	    if ((alpha == DISPLAY_ALPHA_FIRST) || 
		(alpha == DISPLAY_UNUSED_FIRST)) {
		bytes_per_pixel = 4;
		mask0 = 0;
		mask1 = 0xff;
		mask2 = 0xff;
		mask3 = 0xff;
	    }
	    else {
		bytes_per_pixel = 3;
		mask0 = 0xff;
		mask1 = 0xff;
		mask2 = 0xff;
		mask3 = 0;
	    }
	    if ((alpha == DISPLAY_ALPHA_LAST) || 
		(alpha == DISPLAY_UNUSED_LAST))
		bytes_per_pixel++;
	    break;
	case DISPLAY_COLORS_CMYK:
	    if ((img->format & DISPLAY_DEPTH_MASK) != DISPLAY_DEPTH_8)
		return -1;

	    additive = FALSE;
	    bytes_per_pixel = 4;
	    mask0 = 0xff;
	    mask1 = 0xff;
	    mask2 = 0xff;
	    mask3 = 0xff;
	    break;
    }

    /* one or more bytes per pixel */

    memset(dest, 0xff, bwidth);
    omask = 0x80;
    oroll = 7;

    for (j = 0; j < width; j++) {
	if (additive) {
	    /* RGB */
	    black =  (s[0] & mask0) != mask0;
	    if (bytes_per_pixel > 1)
		black |= (s[1] & mask1) != mask1;
	    if (bytes_per_pixel > 2)
		black |= (s[2] & mask2) != mask2;
	    if (bytes_per_pixel > 3)
		black |= (s[3] & mask3) != mask3;
	}
	else {
	    /* CMYK */
	    black =  (s[0] & mask0) != 0;
	    black |= (s[1] & mask1) != 0;
	    black |= (s[2] & mask2) != 0;
	    black |= (s[3] & mask3) != 0;
	}
	s += bytes_per_pixel;
	if (black) 
	    dest[j/8] |= omask;
	else
	    dest[j/8] &= (unsigned char)(~omask);
	oroll--;
	omask >>= 1;
	if (oroll < 0) {
	    omask = 0x80;
	    oroll = 7;
	}
    }
    return 0;
}


/* Return number of bits per pixel.
 * If format unknown, return -ve.
 */
int
image_depth(IMAGE *img)
{
    unsigned int alpha = img->format & DISPLAY_ALPHA_MASK;
    int bitcount = -1;
    switch (img->format & DISPLAY_COLORS_MASK) {
	case DISPLAY_COLORS_NATIVE:
	case DISPLAY_COLORS_GRAY:
	    switch (img->format & DISPLAY_DEPTH_MASK) {
		case DISPLAY_DEPTH_1:
		    bitcount = 1;
		    break;
		case DISPLAY_DEPTH_4:
		    bitcount = 4;
		    break;
		case DISPLAY_DEPTH_8:
		    bitcount = 8;
		    break;
		case DISPLAY_DEPTH_16:
		    bitcount = 16;
		    break;
	    }
	    break;
	case DISPLAY_COLORS_RGB:
	    if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
		if ((alpha == DISPLAY_ALPHA_FIRST) || 
		    (alpha == DISPLAY_UNUSED_FIRST))
		    bitcount = 32;
		else
		    bitcount = 24;
		if ((alpha == DISPLAY_ALPHA_LAST) || 
		    (alpha == DISPLAY_UNUSED_LAST))
		    bitcount += 8;
	    }
	    break;
	case DISPLAY_COLORS_CMYK:
	    if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8)
		bitcount = 32;
	    break;
    }

    return bitcount;
}

/********************************************************/

/* Copy an image, converting format if needed */
int image_copy(IMAGE *newimg, IMAGE *oldimg, unsigned int format)
{
    int code = 0;
    int depth;
    int colour_format = (format & DISPLAY_COLORS_MASK);
    BOOL mono = FALSE;
    BOOL grey = FALSE;
    BOOL rgb24 = FALSE;
    BOOL bgr24 = FALSE;
    memset(newimg, 0, sizeof(IMAGE));
    newimg->width = oldimg->width;
    newimg->height = oldimg->height;
    newimg->raster = oldimg->raster;	/* bytes per row */
    newimg->format = format;
    newimg->image = NULL;
    if (newimg->format == oldimg->format) {
	/* copy unmodified */
	int image_size = newimg->height * newimg->raster;
        newimg->image = (unsigned char *)malloc(image_size);
	if (newimg->image == NULL)
	    code = -1;
	else {
	    memcpy(newimg->image, oldimg->image, image_size);
	    if (image_platform_init(newimg) < 0) {
		free(newimg->image);
		newimg->image = NULL;
		code = -1;
	    }
	}
	return code;
    }

    /* Need to convert the format */
    /* We support 1-bit monochrome, 8-bit grey, 24-bit RGB and 24-bit BGR */
    depth = image_depth(newimg);
    if ((depth == 1) && (colour_format == DISPLAY_COLORS_NATIVE))
	mono = TRUE;
    else if ((depth == 8) && (colour_format == DISPLAY_COLORS_GRAY))
	grey = TRUE;
    else if ((depth == 24) && (colour_format == DISPLAY_COLORS_RGB)) {
        if ((format & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN)
	    rgb24 = TRUE;
	else
	    bgr24 = TRUE;
    }
    else
	code = -1;

    if (code == 0) {
	newimg->raster = (((depth * newimg->width + 7) >> 3) + 3) & ~3;
	newimg->image = (unsigned char *)
	    malloc(newimg->raster * newimg->height);
    }

    if ((newimg->image == NULL))
	code = -1;
    else if (code == 0) {
	/* convert each row */
	unsigned char *source, *dest;
        BOOL invert = (newimg->format & DISPLAY_FIRSTROW_MASK) !=
		      (oldimg->format & DISPLAY_FIRSTROW_MASK);
	int i;
	for (i=0; i<(int)newimg->height; i++) {
	    source = oldimg->image + oldimg->raster * i;
	    if (invert)
	        dest = newimg->image + newimg->raster * (newimg->height-1-i);
	    else
	        dest = newimg->image + newimg->raster * i;
	    if (mono)
		code = image_to_mono(oldimg, dest, source);
	    else if (grey)
		code = image_to_grey(oldimg, dest, source);
	    else if (rgb24)
		code = image_to_24RGB(oldimg, dest, source);
	    else if (bgr24)
		code = image_to_24BGR(oldimg, dest, source);
	    if (code)
		break;
	} 
    }

    if (code == 0)
        code = image_platform_init(newimg);
    if (code) {
	if (newimg->image) {
	    free(newimg->image);
	    newimg->image = NULL;
	}
    }
    return code;
}

/********************************************************/

/* Merge a separation stored as greyscale into a CMYK composite */
int 
image_merge_cmyk(IMAGE *img, IMAGE *layer, float cyan, float magenta,
   float yellow, float black)
{
    int x;
    int y;
    unsigned char *img_row;
    unsigned char *layer_row;
    unsigned char *p;
    int img_topfirst;
    int layer_topfirst;
    unsigned int val;
    if ((img == NULL) || (img->image == NULL))
	return -1;
    if ((layer == NULL) || (layer->image == NULL))
	return -1;
    if ((img->format & DISPLAY_COLORS_MASK) != DISPLAY_COLORS_CMYK)
	return -1;
    if ((img->format & DISPLAY_DEPTH_MASK) != DISPLAY_DEPTH_8)
	return -1;
    if ((img->format & DISPLAY_ENDIAN_MASK) != DISPLAY_BIGENDIAN)
	return -1;
    if ((layer->format & DISPLAY_COLORS_MASK) != DISPLAY_COLORS_GRAY)
	return -1;
    if ((layer->format & DISPLAY_DEPTH_MASK) != DISPLAY_DEPTH_8)
	return -1;
    if (img->width != layer->width)
	return -1;
    if (img->height != layer->height)
	return -1;

    img_topfirst = ((img->format & DISPLAY_FIRSTROW_MASK) == DISPLAY_TOPFIRST);
    layer_topfirst = ((layer->format & DISPLAY_FIRSTROW_MASK) 
	== DISPLAY_TOPFIRST);

    for (y=0; y<(int)img->height; y++) {
        img_row = img->image + img->raster * 
	    (img_topfirst ? y : ((int)img->height - y - 1));
        layer_row = layer->image + layer->raster * 
	    (layer_topfirst ? y : ((int)layer->height - y - 1));
	for (x=0; x<(int)img->width; x++) {
	    val = layer_row[x];
	    p = &img_row[x*4];
	    p[0] = (unsigned char)min(255, p[0] + (255-val) * cyan);
	    p[1] = (unsigned char)min(255, p[1] + (255-val) * magenta);
	    p[2] = (unsigned char)min(255, p[2] + (255-val) * yellow);
	    p[3] = (unsigned char)min(255, p[3] + (255-val) * black);
	}
    }
    return 0;
}

/********************************************************/

typedef struct scale_pixels_s {
   unsigned int end;	/* index of last pixel (full or partial) */
   unsigned int frac;	/* fraction * 16 of last pixel */
} scale_pixels_t;

/* Down scale an image.
 * This is intended to scale a hires resolution monochrome image 
 * to a lower resolution greyscale image.
 * Input can be:
 *   1bit/pixel native or grey
 *   8bit/pixel grey,
 *   24 or 32bit/pixel RGB
 *   32bit/pixel CMYK.
 * If the input format is grey, the output format can be one of
 *   8bit/pixel grey,
 *   24 or 32bit/pixel RGB
 *   32bit/pixel CMYK.
 * If the input format is RGB or CMYK, the output format must match.
 * Row order must be the same for both images.
 */
int image_down_scale(IMAGE *newimg, IMAGE *oldimg)
{
    unsigned int i;
    unsigned int xi, xo, yi, yo;
    unsigned int end, last;
    scale_pixels_t *spx = NULL;
    scale_pixels_t *spy = NULL;
    unsigned int *sum1 = NULL;	/* horizontal merge of one source row */
    unsigned int *sumn = NULL;	/* merge of several rows */
    unsigned int frac;
    unsigned int val;
    unsigned int maxval;
    unsigned char *row_in;
    unsigned char *row_out;
    unsigned int mask;
    unsigned int byteval = 0;
    unsigned int width_in = oldimg->width;
    unsigned int height_in = oldimg->height;
    unsigned int width_out = newimg->width;
    unsigned int height_out = newimg->height;
    BOOL mono_wb = FALSE;
    BOOL mono_bw = FALSE;
    BOOL cmyk_in = FALSE;
    BOOL cmyk_out = FALSE;
    unsigned int ncomp_in = 0;
    unsigned int ncomp_out = 0;
    unsigned int ncomp_out_first = 0;
    unsigned int ncomp_out_last = 0;

    /* Check if input image format is supported */
    switch (oldimg->format & DISPLAY_COLORS_MASK) {
	case DISPLAY_COLORS_NATIVE:
	    switch (oldimg->format & DISPLAY_DEPTH_MASK) {
		case DISPLAY_DEPTH_1:
		    ncomp_in = 1;
		    mono_wb = TRUE;
		    break;
	     }
	     break;
	case DISPLAY_COLORS_GRAY:
	    switch (oldimg->format & DISPLAY_DEPTH_MASK) {
		case DISPLAY_DEPTH_1:
		    ncomp_in = 1;
		    mono_bw = TRUE;
		    break;
		case DISPLAY_DEPTH_8:
		    ncomp_in = 1;
		    break;
	    }
	    break;
	case DISPLAY_COLORS_RGB:
	    if ((oldimg->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
		int alpha = (oldimg->format & DISPLAY_ALPHA_MASK);
		ncomp_in = 3;
		if ((alpha == DISPLAY_ALPHA_FIRST) || 
		    (alpha == DISPLAY_UNUSED_FIRST))
		    ncomp_in++;
		if ((alpha == DISPLAY_ALPHA_LAST) || 
		    (alpha == DISPLAY_UNUSED_LAST))
		    ncomp_in++;
	    }
	    break;
	case DISPLAY_COLORS_CMYK:
	    if ((oldimg->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
		ncomp_in = 4;
		cmyk_in = TRUE;
	    }
    }

    if (ncomp_in == 0)
	return -1;

    /* Check if output image format is supported */
    switch (newimg->format & DISPLAY_COLORS_MASK) {
	case DISPLAY_COLORS_GRAY:
	    switch (newimg->format & DISPLAY_DEPTH_MASK) {
		case DISPLAY_DEPTH_8:
		    ncomp_out = 1;
		    ncomp_out_last = ncomp_out;
		    break;
	    }
	    break;
	case DISPLAY_COLORS_RGB:
	    if ((newimg->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
		int alpha = (newimg->format & DISPLAY_ALPHA_MASK);
		ncomp_out = 3;
		ncomp_out_first = 0;
	        ncomp_out_last = ncomp_out;
		if (alpha == DISPLAY_ALPHA_FIRST) {
		    ncomp_out++;
		    ncomp_out_last++;
		}
		else if (alpha == DISPLAY_UNUSED_FIRST) {
		    ncomp_out++;
		    ncomp_out_first++;
		    ncomp_out_last++;
		}
		if (alpha == DISPLAY_ALPHA_LAST) {
		    ncomp_out++;
		    ncomp_out_last++;
		}
		else if (alpha == DISPLAY_UNUSED_LAST) {
		    ncomp_out++;
		}
	    }
	    break;
	case DISPLAY_COLORS_CMYK:
	    if ((newimg->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
		ncomp_out = 4;
		cmyk_out = TRUE;
	    }
    }

    if (ncomp_out == 0)
	return -1;
    if (ncomp_out < ncomp_in)
	return -1;
    if ((ncomp_out != ncomp_in) && (ncomp_in != 1))
	return -1;
    if (cmyk_out != cmyk_in)
	return -1;

    if ((newimg->format && DISPLAY_FIRSTROW_MASK) != 
        (oldimg->format && DISPLAY_FIRSTROW_MASK))
	return -1;

    if (width_out > width_in)
	width_out = width_in;
    if (height_out > height_in)
	height_out = height_in;

    maxval = (int)(16 * width_in / width_out) * 
	     (int)(16 * height_in / height_out); 

    spy = (scale_pixels_t *)malloc(height_out * sizeof(scale_pixels_t));
    spx = (scale_pixels_t *)malloc(width_out * sizeof(scale_pixels_t));
    sum1 = (unsigned int *)malloc(width_out * ncomp_in * sizeof(unsigned int));
    sumn = (unsigned int *)malloc(width_out * ncomp_in * sizeof(unsigned int));
    if ((spy == NULL) || (spx == NULL) || (sum1 == NULL) || (sumn == NULL)) {
	if (spy != NULL)
	    free(spy);
	if (spx != NULL)
	    free(spx);
	if (sum1 != NULL)
	    free(sum1);
	if (sumn == NULL)
	    free(sumn);
	return -1;
    }

    /* precalculate the integer pixel offsets and fractions */
    for (xo=0; xo<width_out; xo++) {
	last = (xo+1) * 16 * width_in / width_out;
	end = (last) & (~0xf);
	spx[xo].end= end>>4;
	spx[xo].frac = last - end;
	if (spx[xo].frac == 0) {
	    spx[xo].end--;
	    spx[xo].frac = 16;
	}
    }
    for (yo=0; yo<height_out; yo++) {
	last = (yo+1) *  16 * height_in / height_out;
	end = (last) & (~0xf);
	spy[yo].end= end>>4;
	spy[yo].frac = last - end;
	if (spy[yo].frac == 0) {
	    spy[yo].end--;
	    spy[yo].frac = 16;
	}
    }
    yo = 0;
    memset(sumn, 0, width_out * ncomp_in * sizeof(unsigned int));
    for (yi=0; yi<height_in; yi++) {
	xo = 0;
	for (i=0; i<ncomp_in; i++)
	    sum1[xo*ncomp_in+i] = 0;
	row_in = oldimg->image + yi * oldimg->raster;
	if (mono_wb) {
	    /* 1bit/pixel, 0 is white, 1 is black  */
	    mask = 0;
	    end = spx[xo].end;
	    for (xi=0; xi<width_in; xi++) {
		if ((mask >>= 1) == 0) {
		    mask = 0x80;
		    byteval = row_in[xi>>3];
		}
		val = (byteval & mask) ? 0 : 255;
		if (xi >= end) {
		    /* last (possibly partial) pixel of group */
		    sum1[xo] += val * (frac = spx[xo].frac);
		    if (++xo < width_out)
			sum1[xo] = val * (16 - frac);
		    end = spx[xo].end;
		}
		else
		    sum1[xo] += val << 4;
	    }
	}
	else if (mono_bw) {
	    /* 0 is black, 1 is white */
	    mask = 0;
	    end = spx[xo].end;
	    for (xi=0; xi<width_in; xi++) {
		if ((mask >>= 1) == 0) {
		    mask = 0x80;
		    byteval = row_in[xi>>3];
		}
		val = (byteval & mask) ? 255 : 0;
		if (xi >= end) {
		    /* last (possibly partial) pixel of group */
		    sum1[xo] += val * (frac = spx[xo].frac);
		    if (++xo < width_out)
			sum1[xo] = val * (16 - frac);
		    end = spx[xo].end;
		}
		else
		    sum1[xo] += val << 4;
	    }
	}
	else if (ncomp_in == 1) {
	    /* 8bits/component grey */
	    for (xi=0; xi<width_in; xi++) {
		val = row_in[xi];
		if (xi >= spx[xo].end) {
		    /* last (possibly partial) pixel of group */
		    sum1[xo] += val * (frac = spx[xo].frac);
		    if (++xo < width_out)
			sum1[xo] = val * (16 - frac);
		}
		else
		    sum1[xo] += val << 4;
	    }
	}
	else if (ncomp_in >= 3) {
	    /* 8bits/component RGB, BGR, xRGB, CMYK etc. */
	    for (xi=0; xi<width_in; xi++) {
		for (i=0; i<ncomp_in; i++) {
		    val = row_in[xi*ncomp_in+i];
		    if (xi >= spx[xo].end) {
			/* last (possibly partial) pixel of group */
			frac = spx[xo].frac;
			sum1[xo*ncomp_in+i] += val * frac;
			if (xo+1 < width_out)
			    sum1[(xo+1)*ncomp_in+i] = val * (16 - frac);
		    }
		    else
			sum1[xo*ncomp_in+i] += val << 4;
		}
		if (xi >= spx[xo].end)
		    xo++;
	    }
	}
	if (yi >= spy[yo].end) {
	    frac = spy[yo].frac;
	    /* add last partial row to sumn */
	    for (xo=0; xo<width_out*ncomp_in; xo++)
		sumn[xo] += sum1[xo] * frac;
	    /* write out merged row */
	    row_out = newimg->image + yo * newimg->raster;
	    for (xo=0; xo < width_out*ncomp_in; xo++) {
		val = sumn[xo] / maxval;
		if (val > 255)
		    val = 255;
		if (ncomp_in == ncomp_out) {
		    row_out[xo] = (unsigned char)val;
		}
		else {
		    /* we are converting grey to colour */
		    if (cmyk_out) {
			row_out[xo*ncomp_out+0] = 
			row_out[xo*ncomp_out+1] = 
			row_out[xo*ncomp_out+2] = (unsigned char)0;
			row_out[xo*ncomp_out+3] = (unsigned char)val;
			
		    }
		    else {
			/* RGB */
			for (i=0; i<ncomp_out_first; i++)
			    row_out[xo*ncomp_out+i] = (unsigned char)0;
			for (; i<ncomp_out_last; i++)
			    row_out[xo*ncomp_out+i] = (unsigned char)val;
			for (; i<ncomp_out; i++)
			    row_out[xo*ncomp_out+i] = (unsigned char)0;
		    }
		}
	    }
	    /* Put first partial row in sumn */
	    yo++;
	    frac = 16 - frac;
	    if (yo < height_out) {
		for (xo=0; xo<width_out*ncomp_in; xo++)
		    sumn[xo] = sum1[xo] * frac;
	    }
	}
	else {
	    /* add whole row to sumn */
	    for (xo=0; xo<width_out*ncomp_in; xo++)
		sumn[xo] += sum1[xo]*16;
	}
    }

    free(spy);
    free(spx);
    free(sum1);
    free(sumn);

    return 0;
}


/* Copy an image, resizing it if needed.
 * Currently only supports resizing down.
 */
int
image_copy_resize(IMAGE *newimg, IMAGE *oldimg, unsigned int format,
    float xddpi, float yddpi, float xrdpi, float yrdpi)
{
    if ((xddpi != xrdpi) && (yddpi != yrdpi) &&
        (xddpi <= xrdpi) && (yddpi <= yrdpi)) {
	int temp_format = 
	    DISPLAY_COLORS_RGB | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_8 | 
	    DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST;	/* 24-bit BGR */
	int grey_format = 
	    DISPLAY_COLORS_GRAY | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_8 | 
	    DISPLAY_LITTLEENDIAN | DISPLAY_BOTTOMFIRST;	/* 8-bit grey */
	/* Down scale image */
	memset(newimg, 0, sizeof(IMAGE));
	newimg->width = (unsigned int)(oldimg->width * xddpi / xrdpi + 0.5);
	newimg->height = (unsigned int)(oldimg->height * yddpi / yrdpi + 0.5);
	if ((((format & DISPLAY_COLORS_MASK) == DISPLAY_COLORS_NATIVE) ||
	     ((format & DISPLAY_COLORS_MASK) == DISPLAY_COLORS_GRAY)) && 
	    (((format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_1)))
	    /* need to convert up to 8bit/pixel */
	    newimg->format = image_platform_format(
		(format & ~DISPLAY_COLORS_MASK & ~DISPLAY_DEPTH_MASK)
		| DISPLAY_COLORS_GRAY | DISPLAY_DEPTH_8);
	else
	    newimg->format = format;
	newimg->raster = 
	    (((image_depth(newimg) * newimg->width + 7) >> 3) + 3) & ~3;
	/* Try converting as is */
	newimg->image = malloc(newimg->raster * newimg->height);
	if (newimg->image == NULL)
	    return -1;
	if (image_down_scale(newimg, oldimg) != 0) {
	    /* Conversion failed, so input format probably not OK.
	     * Convert to RGB and try again.
	     * Ignore requested format.
	     */
	    IMAGE tempimg;
	    free(newimg->image);
	    newimg->image = NULL;
	    memset(&tempimg, 0, sizeof(tempimg));
	    tempimg.width = oldimg->width;
	    tempimg.height = oldimg->height;
	    if (((((oldimg->format & DISPLAY_COLORS_MASK) 
		  == DISPLAY_COLORS_NATIVE) ||
	         ((oldimg->format & DISPLAY_COLORS_MASK) 
		  == DISPLAY_COLORS_GRAY)) && 
	        ((oldimg->format & DISPLAY_DEPTH_MASK) 
		  == DISPLAY_DEPTH_1))
		|| 
	         (((oldimg->format & DISPLAY_COLORS_MASK) 
		  == DISPLAY_COLORS_GRAY) && 
	        ((oldimg->format & DISPLAY_DEPTH_MASK) 
		  == DISPLAY_DEPTH_8)))
		tempimg.format = image_platform_format(grey_format);
	    else
		tempimg.format = image_platform_format(temp_format);
	    tempimg.raster = 
	        (((image_depth(&tempimg) * tempimg.width + 7) >> 3) + 3) & ~3;
	    tempimg.image = malloc(tempimg.raster * tempimg.height);
	    if (tempimg.image == NULL) {
		return -1;
	    }
	    newimg->format = tempimg.format;
	    newimg->raster = 
		(((image_depth(newimg) * newimg->width + 7) >> 3) + 3) & ~3;
	    newimg->image = malloc(newimg->raster * newimg->height);
	    if (newimg->image == NULL) {
		free(tempimg.image);
		return -1;
	    }
	    image_copy(&tempimg, oldimg, tempimg.format);
	    if (image_down_scale(newimg, &tempimg) != 0) {
		/* nothing worked */
		free(tempimg.image);
		free(newimg->image);
		newimg->image = NULL;
		/* fall through to copy unmodified */
	    }
	    else {
		free(tempimg.image);
		image_platform_init(newimg);
		return 0;	/* succesfully resized */
	    }
	}
	else {
	    image_platform_init(newimg);
	    return 0;	/* succesfully resized */
	}
    }

    /* Not resized, or failed resize, so just copy image */
    return image_copy(newimg, oldimg, format);
}


/********************************************************/


/* ASCII85Encode to a character line.
 * Returns the number of characters produced, which can be
 * no more than count+1
 */
static int 
a85write(char *line, unsigned char *buf, int count)
{
    int i;
    unsigned long value;
    unsigned char abuf[5];
    if (count == 0)
	return 0;
    if (count < 4) {
	for (i = count; i < 4; i++)
	    buf[i] = 0;
    }
    if ((count == 4) && 
	(buf[0] == 0) && (buf[1] == 0) && (buf[2] == 0) && (buf[3] == 0)) {
	line[0] = 'z';
	return 1;
    }
    value = ((unsigned int)(buf[0])<<24) + 
	((unsigned int)(buf[1])<<16) + 
	((unsigned int)(buf[2])<<8) + 
	(unsigned int)(buf[3]);
    abuf[0] = (unsigned char)(value / (85L*85*85*85));
    value -= abuf[0] * (85L*85*85*85);
    abuf[1] = (unsigned char)(value / (85L*85*85));
    value -= abuf[1] * (85L*85*85);
    abuf[2] = (unsigned char)(value / (85L*85));
    value -= abuf[2] * (85L*85);
    abuf[3] = (unsigned char)(value / (85L));
    value -= abuf[3] * (85L);
    abuf[4] = (unsigned char)(value);
    for (i=0; i<count+1; i++)
	line[i] = (char)(abuf[i]+'!');
    return count+1;
}

/* Simple byte RLE, known as PackBits on the Macintosh and
 * RunLengthEncode in PostScript.
 * The output buffer is comp.
 * The input buffer is raw, containing length bytes.
 * The output buffer must be at least length*129/128 bytes long.
 */
int
packbits(BYTE *comp, BYTE *raw, int length)
{
    BYTE *cp;
    int literal = 0;	/* number of literal bytes */
    int prevlit = 0;	/* length of previous literal */
    int repeat = 0;		/* number of repeat bytes - 1*/
    int start = 0;		/* start of block to be coded */
    BYTE previous = raw[0];
    int i, j;

    cp = comp;
    for (i=0; i<length; i++) {
	if (literal == 128) {
	    /* code now */
	    *cp++ = (BYTE)(literal-1);
	    for (j=0; j<literal; j++)
		*cp++ = (BYTE)raw[start+j];
	    prevlit = 0; 		/* because we can't add to it */
	    literal = 0;
	    /* repeat = 0; */	/* implicit */
	    start = i;
	    previous = raw[i];
	}
	if (repeat == 128) {
	    /* write out repeat block */
	    *cp++ = (BYTE)-127;	/* repeat count 128 */
	    *cp++ = previous;
	    repeat = 0;
	    literal = 0;
	    start = i;
	    prevlit = 0;
	    previous = raw[i];
	}
	if (raw[i] == previous) {
	    if (literal == 1) {
		/* replace by repeat */
		repeat = 1;
		literal = 0;
	    }
	    else if (literal) {
		/* write out existing literal */
		literal--;	/* remove repeat byte from literal */
		*cp++ = (BYTE)(literal-1);
		for (j=0; j<literal; j++)
		    *cp++ = raw[start+j];
		if (literal < 126)
		    prevlit = literal; 
		else
		    prevlit = 0;	/* we won't be able to add to it */
		/* repeat = 0; */	/* implicit */
		start = i-1;
		repeat = 1;
		literal = 0;
	    }
	    repeat++;
	}
	else {
	    /* doesn't match previous byte */
	    if (repeat) {
		/* write out repeat block, or merge with literal */
		if (repeat == 1) {
		    /* repeats must be 2 bytes or more, so code as literal */
		    literal = repeat;
		    repeat = 0;
		} else if (repeat == 2) {	/* 2 byte repeat */
		    /* code 2 byte repeat as repeat */
		    /* except when preceeded by literal */
		    if ( (prevlit) && (prevlit < 126) ) {
			/* previous literal and room to combine */
			start -= prevlit;  /* continue previous literal */
			cp -= prevlit+1;
			literal = prevlit + 2;
			prevlit = 0;
			repeat = 0;
		    }
		    else {
			/* code as repeat */
			*cp++ = (BYTE)(-repeat+1);
			*cp++ = previous;
			start = i;
			prevlit = 0;
			/* literal = 0; */	/* implicit */
			repeat = 0;
		    }
		    /* literals will be coded later */
		}
		else {
		    /* repeat of 3 or more bytes */
		    *cp++ = (BYTE)(-repeat+1);
		    *cp++ = previous;
		    start = i;
		    repeat = 0;
		    prevlit = 0;
		}
	    }
	    literal++;
	}
        previous = raw[i];
    }
    if (repeat == 1) {
	/* can't code repeat 1, use literal instead */
	literal = 1;
	repeat = 0;
    }
    if (literal) {
	/* code left over literal */
	*cp++ = (BYTE)(literal-1);
	for (j=0; j<literal; j++)
	    *cp++ = raw[start+j];
    }
    if (repeat) {
	/* code left over repeat */
	*cp++ = (BYTE)(-repeat+1);
	*cp++ = previous;
    }
    return (int)(cp - comp);	/* number of code bytes */
}


/* Write an image as an EPS file.
 * Currently we support 8bits/component RGB or CMYK without conversion,
 * 1, 4 or 8 bits/pixel grey without conversion,
 * 1 bit native without conversion,
 * and a number of other formats by conversion to 24RGB.
 * Output can be ASCII85 or ASCIIHex encoded, and can be
 * compressed with RunLengthEncode.
 */
int 
image_to_eps(GFile *f, IMAGE *img, int llx, int lly, int urx, int ury,
    float fllx, float flly, float furx, float fury, int use_a85, int compress)
{
    int x, y;
    int topfirst;
    int bigendian;
    int hires_bbox_valid = 1;
    unsigned char *row;
    int i;
    int ncomp;			/* number of components per source pixel */
    int count = 0;
    const char hex[] = "0123456789abcdef";
    unsigned char *packin;
    unsigned char *packout;
    int packin_count;
    int packout_count;
    int packout_len;
    int separate = 0;		/* Write components separately */
    int depth;
    int compwidth;		/* width of one row of one component in bytes */
    BOOL convert = FALSE;	/* convert to RGB24 */
    BOOL invert = FALSE;	/* black=0 for FALSE, black=1 for TRUE */
    unsigned char *convert_row = NULL;
    char buf[MAXSTR];
    lzw_state_t *lzw = NULL;

    if ((fllx >= furx) || (flly >= fury))
	hires_bbox_valid = 0;

    topfirst = ((img->format & DISPLAY_FIRSTROW_MASK) == DISPLAY_TOPFIRST);
    bigendian = ((img->format & DISPLAY_ENDIAN_MASK) == DISPLAY_BIGENDIAN);

    if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_1) {
	depth = 1;
	compwidth = (img->width + 7) >> 3;
    }
    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_4) {
	depth = 4;
	compwidth = (img->width + 1) >> 1;
    }
    else if ((img->format & DISPLAY_DEPTH_MASK) == DISPLAY_DEPTH_8) {
	depth = 8;
	compwidth = img->width;
    }
    else
	return -1;


    if ((img->format & DISPLAY_COLORS_MASK) == DISPLAY_COLORS_CMYK)
 	ncomp = 4;
    else if ((img->format & DISPLAY_COLORS_MASK) == DISPLAY_COLORS_RGB) {
 	ncomp = 3;
	if ((img->format & DISPLAY_ALPHA_MASK) != DISPLAY_ALPHA_NONE) {
	    convert = TRUE;
	    compwidth = img->width;
	    bigendian = TRUE;	/* Convert to RGB, not BGR */
	}
    }
    else if ((img->format & DISPLAY_COLORS_MASK) == DISPLAY_COLORS_GRAY)
	ncomp = 1;
    else if ((img->format & DISPLAY_COLORS_MASK) == DISPLAY_COLORS_NATIVE) {
	if (depth == 1) {
	    ncomp = 1;
	    invert = TRUE;
	}
	else {
	    ncomp = 3;
	    compwidth = img->width;
	    convert = TRUE;
	    bigendian = TRUE;	/* Convert to RGB, not BGR */
	}
    }
    else
	return -1;

    packin = (unsigned char *)malloc(compwidth * ncomp);
    if (packin == NULL)
	return -1;
    packout_len = compwidth * ncomp * 5 / 4 + 4;
    packout = (unsigned char *)malloc(packout_len);
    if (packout == NULL) {
	free(packin);
	return -1;
    }
    if (convert) {
	convert_row = (unsigned char *)malloc(compwidth * ncomp);
	if (convert_row == NULL) {
	    free(packin);
	    free(packout);
	    return -1;
	}
    }
    if (compress == IMAGE_COMPRESS_LZW) {
	lzw = lzw_new();
	if (lzw == (lzw_state_t *)NULL) {
	    free(packin);
	    free(packout);
 	    if (convert_row)
	        free(convert_row);
	    return -1;
	}
    }

    gfile_puts(f, "%!PS-Adobe-3.0 EPSF-3.0\n");
    snprintf(buf, sizeof(buf)-1, 
	"%%%%BoundingBox: %d %d %d %d\n", llx, lly, urx, ury);
    gfile_puts(f, buf);
    if (hires_bbox_valid) {
	snprintf(buf, sizeof(buf)-1,  "%%%%HiResBoundingBox: %g %g %g %g\n",
	    fllx, flly, furx, fury);
        gfile_puts(f, buf);
    }
    gfile_puts(f, "%%Pages: 1\n");
    gfile_puts(f, "%%EndComments\n");
    gfile_puts(f, "%%Page: 1 1\n");
    gfile_puts(f, "gsave\n");
    if (hires_bbox_valid) {
        snprintf(buf, sizeof(buf)-1,  "%g %g translate\n", fllx, flly);
        gfile_puts(f, buf);
	snprintf(buf, sizeof(buf)-1,  "%g %g scale\n", furx-fllx, fury-flly);
        gfile_puts(f, buf);
    }
    else {
        snprintf(buf, sizeof(buf)-1,  "%d %d translate\n", llx, lly);
        gfile_puts(f, buf);
	snprintf(buf, sizeof(buf)-1,  "%d %d scale\n", urx - llx, ury - lly);
        gfile_puts(f, buf);
    }
    if (ncomp == 1)
        gfile_puts(f, "/DeviceGray setcolorspace\n");
    else if (ncomp == 3)
        gfile_puts(f, "/DeviceRGB setcolorspace\n");
    else if (ncomp == 4)
        gfile_puts(f, "/DeviceCMYK setcolorspace\n");
    if ((ncomp > 1) && (compress != IMAGE_COMPRESS_NONE)) {
	/* Include as RRRRGGGGBBBB not RGBRGBRGBRGB
	 * (or CCCCMMMMYYYYKKKK not CMYKCMYKCMYKCMYK)
	 * Since this compresses with RLE better.
	 */
	separate = 1;
    }
    if (separate) {
	/* Include as RRGGBB not RGBRGB (or CCMMYYKK not CMYKCMYK) */
	/* Since this compresses with RLE better */
	if (use_a85)
	    gfile_puts(f, "/infile currentfile /ASCII85Decode filter");
	else
	    gfile_puts(f, "/infile currentfile /ASCIIHexDecode filter");
	if (compress == IMAGE_COMPRESS_LZW)
	    gfile_puts(f, " /LZWDecode filter");
	else if (compress == IMAGE_COMPRESS_RLE)
	    gfile_puts(f, " /RunLengthDecode filter");
	gfile_puts(f, " def\n");
	for (i=0; i<ncomp; i++) {
	    snprintf(buf, sizeof(buf)-1, 
		"/str%d %d string def\n", i, img->width);
            gfile_puts(f, buf);
	}
    }
    gfile_puts(f, "<<\n /ImageType 1\n");
    snprintf(buf, sizeof(buf)-1, " /Width %d\n", img->width);
    gfile_puts(f, buf);
    snprintf(buf, sizeof(buf)-1, " /Height %d\n", img->height);
    gfile_puts(f, buf);
    snprintf(buf, sizeof(buf)-1, " /BitsPerComponent %d\n", depth);
    gfile_puts(f, buf);
    gfile_puts(f, " /Decode [ ");
    for (i=0; i<ncomp; i++) {
	if (invert)
	    gfile_puts(f, "1 0 ");
	else
            gfile_puts(f, "0 1 ");
    }
    gfile_puts(f, "]\n");
    snprintf(buf, sizeof(buf)-1, " /ImageMatrix [%d 0 0 %d 0 %d]\n",
	img->width, -(int)img->height, img->height);
    gfile_puts(f, buf);
    if (separate) {
	gfile_puts(f, " /MultipleDataSources true\n");
	gfile_puts(f, " /DataSource [\n");
	for (i=0; i<ncomp; i++) {
	    snprintf(buf, sizeof(buf)-1, 
		"  {infile str%d readstring pop}\n", i);
            gfile_puts(f, buf);
	}
	gfile_puts(f, " ]\n");
    }
    else {
	if (use_a85)
	    gfile_puts(f, " /DataSource currentfile /ASCII85Decode filter\n");
	else
	    gfile_puts(f, " /DataSource currentfile /ASCIIHexDecode filter\n");
	if (compress == IMAGE_COMPRESS_LZW)
	    gfile_puts(f, " /LZWDecode filter\n");
	else if (compress == IMAGE_COMPRESS_RLE)
	    gfile_puts(f, " /RunLengthDecode filter\n");
    }
    gfile_puts(f, ">>\nimage\n");
    count = 0;
    packout_count = 0;
    for (y=0; y<(int)img->height; y++) {
        row = img->image + img->raster * 
	    (topfirst != 0 ? y : ((int)img->height - y - 1));
	if (convert) {
	    image_to_24RGB(img, convert_row, row);
	    row = convert_row;
	}
	packin_count = 0;
	if (separate) {
	    if (bigendian) {
		for (i=0; i<ncomp; i++)
		    for (x=0; x<compwidth; x++)
			packin[packin_count++] = row[x*ncomp+i];
	    }
	    else {
		for (i=ncomp-1; i>=0; i--)
		    for (x=0; x<compwidth; x++)
			packin[packin_count++] = row[x*ncomp+i];
	    }
	}
	else {
	    for (x=0; x<compwidth; x++) {
		if (bigendian) {
		    for (i=0; i<ncomp; i++)
			packin[packin_count++] = row[x*ncomp+i];
		}
		else {
		    for (i=ncomp-1; i>=0; i--)
			packin[packin_count++] = row[x*ncomp+i];
		}
	    }
	}
	if (compress == IMAGE_COMPRESS_LZW) {
	    int inlen = packin_count;
	    int outlen = packout_len - packout_count;
	    lzw_compress(lzw, packin, &inlen, packout+packout_count, &outlen);
	    packout_count += outlen;
	    if (y == (int)img->height-1) {
		/* This is the last row */
		/* Flush and EOD */
		inlen = 0;	/* EOD */
		outlen = packout_len - packout_count;
		lzw_compress(lzw, packin, &inlen, packout+packout_count, 
		    &outlen);
		packout_count += outlen;
	    }
	}
	else if (compress == IMAGE_COMPRESS_RLE) {
	    packout_count += 
		packbits(packout+packout_count, packin, packin_count);
	}
	else {
	    memcpy(packout+packout_count, packin, packin_count);
	    packout_count += packin_count;
	}
	for (i=0; i<packout_count; i++) {
	    if (use_a85) {
		if (packout_count-i < 4)
		    break;
		else
		    count += a85write(buf+count, packout+i, 4);
		i += 3;
	    }
	    else {
		buf[count++] = hex[(packout[i]>>4) & 0xf];
		buf[count++] = hex[packout[i] & 0xf];
	    }
	    if (count >= 70) {
		buf[count++] = '\n';
		gfile_write(f, buf, count);
		count = 0;
	    }
	    
	}
	if (packout_count-i > 0)
	    memmove(packout, packout+i, packout_count-i);
	packout_count -= i;
    }
    if (use_a85) {
	if (packout_count)
	    count += a85write(buf+count, packout, packout_count);
	buf[count++] = '~';
	buf[count++] = '>';
	buf[count++] = '\n';
    }
    else {
	buf[count++] = '>';
	buf[count++] = '\n';
    }
    gfile_write(f, buf, count);
    gfile_puts(f, "grestore\n");
    gfile_puts(f, "showpage\n");
    gfile_puts(f, "%%Trailer\n");
    gfile_puts(f, "%%EOF\n");
    if (lzw)
	lzw_free(lzw);
    if (convert)
	free(convert_row);
    free(packin);
    free(packout);
    return 0;
}

int 
image_to_epsfile(IMAGE *img, LPCTSTR filename, float xdpi, float ydpi)
{
    GFile *f;
    int code = 0;
    int width = (int)(img->width * 72.0 / xdpi);
    int height = (int)(img->height * 72.0 / ydpi);
    

    if ((img == NULL) || (img->image == NULL))
	return -1;
    
    f = gfile_open(filename, gfile_modeWrite | gfile_modeCreate);
    if (f == NULL)
	return -1;

    code = image_to_eps(f, img, 0, 0, width, height, 
	0.0, 0.0, (float)width, (float)height,
	TRUE, IMAGE_COMPRESS_LZW);

    gfile_close(f);
    return code;
}


/********************************************************/
