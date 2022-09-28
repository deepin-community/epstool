/* Copyright (C) 2002-2003 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cbmp.h,v 1.5 2003/08/08 23:58:14 ghostgum Exp $ */
/* BMP structures */

typedef struct tagBITMAP2
{
    DWORD   biSize;
    LONG    biWidth;
    LONG    biHeight;
    WORD    biPlanes;
    WORD    biBitCount;
    DWORD   biCompression;
    DWORD   biSizeImage;
    LONG    biXPelsPerMeter;
    LONG    biYPelsPerMeter;
    DWORD   biClrUsed;
    DWORD   biClrImportant;
} BITMAP2;
typedef BITMAP2 * LPBITMAP2;
#define BITMAP2_LENGTH 40

typedef struct tagBITMAPFILE
{
    WORD    bfType;
    DWORD   bfSize;
    WORD    bfReserved1;
    WORD    bfReserved2;
    DWORD   bfOffBits;
} BITMAPFILE;
typedef BITMAPFILE * LPBITMAPFILE;
#define BITMAPFILE_LENGTH 14

typedef struct tagRGB4
{
    BYTE    rgbBlue;
    BYTE    rgbGreen;
    BYTE    rgbRed;
    BYTE    rgbReserved;
} RGB4;
typedef RGB4 * LPRGB4;

#define RGB4_BLUE	0
#define RGB4_GREEN	1
#define RGB4_RED	2
#define RGB4_EXTRA	3
#define RGB4_LENGTH	4

typedef enum PNM_FORMAT_s {
    PBMRAW=4,
    PGMRAW=5,
    PPMRAW=6,
    PNMANY=99	/* use any */
} PNM_FORMAT;

/* Prototypes */

IMAGE * bmpfile_to_image(LPCTSTR filename);
IMAGE * bmp_to_image(unsigned char *pbitmap, unsigned int length);
IMAGE * pnmfile_to_image(LPCTSTR filename);
void bitmap_image_free(IMAGE *img);
int image_to_bmpfile(IMAGE*img, LPCTSTR filename, float xdpi, float ydpi);
int image_to_pnmfile(IMAGE* img, LPCTSTR filename, PNM_FORMAT pnm_format);
int image_to_tifffile(IMAGE* img, LPCTSTR filename, float xdpi, float ydpi);
int image_to_pngfile(IMAGE* img, LPCTSTR filename);
int image_to_pictfile(IMAGE* img, LPCTSTR filename, float xdpi, float ydpi);
IMAGE * pngfile_to_image(LPCTSTR filename);
int image_to_tifffile(IMAGE* img, LPCTSTR filename, float xdpi, float ydpi);
int image_to_tiff(GFile *f, IMAGE *img, 
    int xoffset, int yoffset, int width, int height, 
    float xdpi, float ydpi, 
    BOOL tiff4, BOOL use_packbits);

void write_dword(DWORD val, GFile *f);
void write_word_as_dword(WORD val, GFile *f);
void write_word(WORD val, GFile *f);
DWORD get_dword(const unsigned char *buf);
WORD get_word(const unsigned char *buf);
