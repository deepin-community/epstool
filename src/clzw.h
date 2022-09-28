/* Copyright (C) 2004 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: clzw.h,v 1.1 2004/07/08 09:14:34 ghostgum Exp $ */
/* LZW compression, compatible with PostScript LZWDecode filter */

/* Structure for holding LZW compressor state */
typedef struct lzw_state_s lzw_state_t;

/* allocate and initialise an LZW compressor */ 
lzw_state_t *lzw_new(void);

/*
 * Compress a buffer with LZW.
 * Inputs:
 *   inbuf is input buffer
 *   *inlen is count of bytes in the input buffer
 *   outbuf is output buffer
 *   *outlen is the length of the output buffer
 * Outputs:
 *   *inlen is count of used input bytes
 *   *outlen is the count of output bytes produced
 * If the output *inlen is not equal to the input *inlen,
 * then output buffer is full and lzw_compress should
 * be called again with the remaining input bytes
 * and another output buffer.
 * To signal EOD, call with *inlen = 0.
 */
void lzw_compress(lzw_state_t *state,
    const unsigned char *inbuf, int *inlen,
    unsigned char *outbuf, int *outlen);

/* 
 * Free the LZW structure
 * You must first have signalled EOD to lzw_compress, 
 * otherwise you will lose data.
 */
void lzw_free(lzw_state_t *state);

