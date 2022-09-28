/* Copyright (C) 2001-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cimg.h,v 1.16 2005/06/10 09:39:24 ghostgum Exp $ */
/* Image information and format conversions header */

/***************************************/
/* image details */

struct IMAGE_s {
    unsigned int width;	    /* pixels */
    unsigned int height;    /* pixels */
    unsigned int raster;    /* bytes per row */
    unsigned int format;   /* pixel format */
    unsigned char *image;

#ifdef UNIX
    GdkRgbCmap *cmap;	/* colour map = palette */
#endif

#ifdef _Windows
    BITMAPINFOHEADER bmih;
    HPALETTE palette;
    /* Windows and OS/2 require a raster line which is a multiple 
     * of 4 bytes.  If raster is not what we expect, then we need 
     * to lie about about how many pixels are really on the line 
     * when doing bitmap transfers.  
     * align_width contains our calculation of how many pixels are 
     * needed to match raster.
     * If align_width != width, then bitmap transfer uses:
     *  source width      = img->align_width
     *  destination width = img->width
     */
    int align_width;
#endif
};
    
typedef enum SEPARATIONS_e {
    SEP_CYAN = 8,
    SEP_MAGENTA = 4,
    SEP_YELLOW = 2,
    SEP_BLACK = 1
} SEPARATIONS;


/* platform specific */

/* Initialise image, creating palette if necessary
 * Return 0 if OK, -ve if image format not supported by platform
 */
int image_platform_init(IMAGE *img);

/* Free any platform specific image members such as a palette */

int image_platform_finish(IMAGE *img);
/* If format is supported by platform, return it that format.
 * Otherwise, return the preferred format into which it should
 * be converted.
 */
unsigned int image_platform_format(unsigned int format);

/* platform independent */
int image_copy(IMAGE *newimg, IMAGE *oldimg, unsigned int format);
int image_copy_resize(IMAGE *newimg, IMAGE *oldimg, unsigned int format,
    float xddpi, float yddpi, float xrdpi, float yrdpi);
unsigned char colour_to_grey(unsigned char r, unsigned char g, 
    unsigned char b);
void image_colour(unsigned int format, int index, 
    unsigned char *r, unsigned char *g, unsigned char *b);
int image_depth(IMAGE *img);

int image_to_mono(IMAGE *img, unsigned char *dest, unsigned char *source);

int image_to_grey(IMAGE *img, unsigned char *dest, unsigned char *source);
void image_1grey_to_8grey(int width, unsigned char *dest, unsigned char *source);
void image_4grey_to_8grey(int width, unsigned char *dest, unsigned char *source);
void image_1native_to_8grey(int width, unsigned char *dest, unsigned char *source);
void image_4native_to_8grey(int width, unsigned char *dest, unsigned char *source);
void image_8native_to_8grey(int width, unsigned char *dest, unsigned char *source);
void image_24RGB_to_8grey(int width, unsigned char *dest, 
    unsigned char *source);
void image_24BGR_to_8grey(int width, unsigned char *dest, 
    unsigned char *source);

int image_to_24BGR(IMAGE *img, unsigned char *dest, unsigned char *source);
void image_4native_to_24BGR(int width, unsigned char *dest, unsigned char *source);
void image_32CMYK_to_24BGR(int width, unsigned char *dest, 
    unsigned char *source, int sep);
void image_16RGB565_to_24BGR(int width, unsigned char *dest, unsigned char *source);
void image_16RGB555_to_24BGR(int width, unsigned char *dest, unsigned char *source);
void image_16BGR565_to_24BGR(int width, unsigned char *dest, unsigned char *source);
void image_16BGR555_to_24BGR(int width, unsigned char *dest, unsigned char *source);

int image_to_24RGB(IMAGE *img, unsigned char *dest, unsigned char *source);
void image_1grey_to_24RGB(int width, unsigned char *dest, unsigned char *source);
void image_4grey_to_24RGB(int width, unsigned char *dest, unsigned char *source);
void image_8grey_to_24RGB(int width, unsigned char *dest, unsigned char *source);
void image_1native_to_24RGB(int width, unsigned char *dest, unsigned char *source);
void image_4native_to_24RGB(int width, unsigned char *dest, unsigned char *source);
void image_8native_to_24RGB(int width, unsigned char *dest, unsigned char *source);
void image_32CMYK_to_24RGB(int width, unsigned char *dest, unsigned char *source, int sep);
void image_16RGB565_to_24RGB(int width, unsigned char *dest, unsigned char *source);
void image_16RGB555_to_24RGB(int width, unsigned char *dest, unsigned char *source);
void image_16BGR565_to_24RGB(int width, unsigned char *dest, unsigned char *source);
void image_16BGR555_to_24RGB(int width, unsigned char *dest, unsigned char *source);

int image_merge_cmyk(IMAGE *img, IMAGE *layer, float cyan, float magenta,
   float yellow, float black);
int image_down_scale(IMAGE *newimg, IMAGE *oldimg);
/* use_85 parameter */
#define IMAGE_ENCODE_HEX 0
#define IMAGE_ENCODE_ASCII85 1
/* compress parameter */
#define IMAGE_COMPRESS_NONE 0
#define IMAGE_COMPRESS_RLE 1
#define IMAGE_COMPRESS_LZW 2
int image_to_eps(GFile *f, IMAGE *img, int llx, int lly, int urx, int ury,
    float fllx, float flly, float furx, float fury, int use_a85, int compress);
int image_to_epsfile(IMAGE *img, LPCTSTR filename, float xdpi, float ydpi);
int packbits(BYTE *comp, BYTE *raw, int length);
