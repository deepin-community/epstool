/* Copyright (C) 2002 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cmbcs.c,v 1.6 2002/08/01 08:27:52 ghostgum Exp $ */
/* Multiple Byte Character Set */

/* 
 * GSview uses Unicode on Windows.
 * On Linux it may use a multiple byte character set,
 * such as UTF-8, EUC, Shift-JIS.
 * This file provides support for stepping over multiple byte
 * characters.  This is needed when searching for a particular
 * characters such as a tab, space, slash or backslash.
 * We assume that the null character will not occur within 
 * a MBCS string, and use the C strlen(str)+1 to get the byte
 * count for allocating memory.
 *
 * For Japanese text on Unix, EUC is most commonly used, SJIS is often
 * used, UTF-8 and UCS-2 are rarely used.
 * For Japanese filenames on Unix, SJIS is most commonly used 
 * (for compatibility with Windows), EUC and UTF-8 are sometimes used,
 * UCS-2 is rarely used.
 * GSview only searches for characters in TCHAR strings we may be able 
 * to handle filenames in a different encoding by doing the translation
 * in cs_to_narrow().
 * 
 * FIX: explain TCHAR, cs, MBCS.
 */

#include "common.h"

#ifndef UNICODE

CODEPAGE global_codepage = CODEPAGE_SBCS;	/* GLOBAL */

/* Return number of bytes from current character to start of
 * next character.
 */
int char_next(const char *str)
{
    int i;
    const unsigned char *t = (const unsigned char *)str;
    switch (global_codepage) {
	default:
	case CODEPAGE_SBCS:
	    i = 1;
	    break;
	case CODEPAGE_UTF8:
	    if (t[0] == 0)
		i = 0;
	    else if ((t[0] > 0) && (t[0] <= 0x7f))
		i = 1;
	    else {
		/* multiple byte UTF-8 */
		/* scan until we find a byte in a suitable range */
		i = 0;
		while (t[i] && (t[i] >= 0x80) && (t[i] <= 0xbf))
		    i++;
	    }
	    break;
	case CODEPAGE_EUC:
	    if (t[0] == 0x8f) {
		/* 3 bytes */
		if (t[1] == '\0')
		    i = 1;
		else if (t[2] == '\0')
		    i = 2;
		else 
		    i = 3;
	    }
	    else if (t[0] & 0x80) {
		/* 2 bytes */
		if (str[1] == '\0')
		    i = 1;
		else 
		    i = 2;
	    }
	    else
		i = 1;
	case CODEPAGE_SJIS:
	    if (t[0] == 0) {
		i = 0;
	    }
	    else if ((t[0] > 0) && (t[0] <= 0x7f)) {
		i = 1;
	    }
	    else if ((t[0] >= 0x80) && (t[0] <= 0xbf)) {
		if (t[1] == '\0')
		    i = 1;
		else
		    i = 2;
	    }
	    else if ((t[0] >= 0xa0) && (t[0] <= 0xdf)) {
		i = 1;
	    }
	    else if ((t[0] >= 0xe0) && (t[0] <= 0xef)) {
		if (t[1] == '\0')
		    i = 1;
		else
		    i = 2;
	    }
	    else
		i = 1;
    }
    return i;
}

/* This implementation is for systems that don't support wide characters */
/* Convert a cs (wide or narrow) string to a narrow string.
 * If the output narrow string needs to be null terminated,
 * the input string length needs to include the null.
 * Returns the number of characters written to the narrow string.
 * If nlen is 0, the function returns the needed buffer size for nstr.
 * If the function fails, it returns 0.
 */
int
char_to_narrow(char *nstr, int nlen, LPCTSTR wstr, int wlen)
{
    /* no translation */
    if (nlen == 0)
	return wlen;
    if (nlen < wlen)
	return 0;
    memcpy(nstr, wstr, wlen);
    return wlen;
}


/* opposite of char_to_narrow */
int 
narrow_to_char(TCHAR *wstr, int wlen, const char *nstr, int nlen)
{
    /* no translation */
    if (wlen == 0)
	return nlen;
    if (wlen < nlen)
	return 0;
    memcpy(wstr, nstr, nlen);
    return nlen;
}

#endif

/* Convert ISO-Latin1 str to UTF-8 ustr.
 * Return byte length of UTF-8 string.
 * If ustr is NULL or insufficient space don't copy.
 * This is needed for the gtk+ user interface.
 */
int 
latin1_to_utf8(char *ustr, int ulen, const char *str, int slen)
{
    int i, j;
    const char *p = str;
    int len = slen;
    for (i=0; i<slen; i++)
	if (p[i] & 0x80)
	    len++;
    if ((ustr != NULL) && (ulen <= len)) {
	p = str;
        for (i=0, j=0; i<slen; i++) {
	    if (*p & 0x80) {
		ustr[j++] = (char)(0xc0 | ((*p & 0xc0) >> 6));
		ustr[j++] = (char)(0x80 | (*p & 0x3f));
	    }
	    else
		ustr[j++] = *p;
	    p++;
	}
    }
    return len;
}

