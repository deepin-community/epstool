/* Copyright (C) 2000-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cfile.h,v 1.4 2005/06/10 09:39:24 ghostgum Exp $ */

/* GFile is similar but to MFC CFile but is implemented as C, not C++.
 * It may also support long files if FILE_POS is > 32-bits.
 * FILE_OFFSET is a signed integer used for positioning a file.
 * FILE_POS is an unsigned integer used for positioning a file.
 * 
 * GFile should really be buffered, but it isn't a disaster
 * if it is unbuffered.  Reading of PostScript files for parsing
 * or DSC comments uses 4kbyte blocks, while normal reading uses 1kbyte.
 * Handling of bitmap files will generally read/write headers in 
 * 2 and 4 blocks, but read/write raster data in text line or 
 * scan line blocks.
 */


#if defined(STDIO) || defined(MEMORYFILE) || !defined(_Windows) || defined(OS2)
# ifndef LPCTSTR
#  define LPCTSTR const char *
# endif
# ifndef GENERIC_READ
#  define GENERIC_READ (0x80000000L)
# endif
# ifndef FILE_SHARE_READ
#  define FILE_SHARE_READ 0x00000001
# endif
#endif

#ifndef FILE_OFFSET
# define FILE_OFFSET long
#endif
#ifndef FILE_POS
# define FILE_POS unsigned FILE_OFFSET
#endif

typedef struct GFile_s GFile;

/* for gfile_open nOpenFlags */
enum OpenFlags {gfile_modeRead = 0x0000, gfile_modeWrite = 0x0001,
    gfile_shareExclusive=0x0010, gfile_shareDenyWrite=0x0020, 
    gfile_modeCreate=0x1000};

/* for gfile_seek nFrom */
enum {gfile_begin, gfile_current, gfile_end};

/* Open a file from a Windows or OS handle */
/* We use "void *" instead of "int" because Windows handles
 * are the same size as pointers, while int is smaller for Win64.
 */
GFile *gfile_open_handle(void *hFile, unsigned int nOpenFlags);

/* Open a file */
GFile *gfile_open(LPCTSTR lpszFileName, unsigned int nOpenFlags);

/* Close a file */
void gfile_close(GFile *gf);

/* Read from a file */
unsigned int gfile_read(GFile *gf, void *lpBuf, unsigned int nCount);

/* Write to a file */
unsigned int gfile_write(GFile *gf, const void *lpBuf, unsigned int nCount);

/* Seek to a position in the file. */
/* Reset any file errors. */
int gfile_seek(GFile *gf, FILE_OFFSET lOff, unsigned int nFrom);

/* Get the current file position */
FILE_POS gfile_get_position(GFile *gf);

/* Get the file length */
FILE_POS gfile_get_length(GFile *gf);

/* Get the file date and time.  The actual data format 
 * in pdt_low and pdt_high is undefined, and is only used
 * for comparison by gfile_changed().
 * Return 0 if success, -ve for failure.
 */
int gfile_get_datetime(GFile *gf, unsigned long *pdt_low, 
    unsigned long *pdt_high);

/* Check if the length or datetime has changed. Use gfile_get_length
 * and gfile_get_datetime.
 * Return non-zero if changed, zero if unchanged.
 */
int gfile_changed(GFile *gf, FILE_POS length, 
    unsigned long dt_low, unsigned long dt_high);

/* Return non-zero if a file error occurred */
int gfile_error(GFile *gf);

/* For memory mapped file, set the base address and length of the 
 * memory block.
 */
void gfile_set_memory(GFile *gf, const char *base, FILE_POS len);


/***********************************************************/
/* These are implementation independent */

int gfile_puts(GFile *gf, const char *str);


