/* Copyright (C) 2004-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: clzw.c,v 1.2 2005/06/10 09:39:24 ghostgum Exp $ */
/* LZW compression, compatible with PostScript LZWDecode filter */

#include <stdlib.h>
#include <string.h>
#include "clzw.h"


/* 
 * Explanation of LZW by Mark Nelson in Dr. Dobb's Journal, October 1989.
 * http://www.dogma.net/markn/articles/lzw/lzw.htm
 * 
 * This is an implementation of 12-bit LZW, compatible with 
 * PostScript LZWDecode filter.
 */

#define LZW_HASH_SIZE 5021
#define LZW_MAX 4094
#define LZW_RESET 256
#define LZW_EOD 257
#define LZW_FIRST 258

typedef struct lzw_code_s {
    short code;
    short base_code;
    unsigned char ch;
} lzw_code_t;

struct lzw_state_s {
    short next_code;
    short lzwstr;
    lzw_code_t table[LZW_HASH_SIZE];
    int code_bit_length;  	/* length of current codes, 9, 10, 11 or 12 */
    short code_change;		/* code at which code_bit_length increases */
    int output_bits;      	/* bits that didn't fit in a whole byte */
				/* This must be a 32-bit or larger type */
    int output_bits_count;	/* number of bits that didn't fit */
    int bytes_in;		/* for checking compression efficiency */
    int bytes_out;
};

/* Reset the code table */
static void
lzw_reset(lzw_state_t *state)
{
    int i;
    lzw_code_t *table = state->table;
    state->next_code = LZW_FIRST;
    state->code_bit_length = 9;
    state->code_change = (short)((1<<state->code_bit_length)-1);
    state->bytes_in = 0;
    state->bytes_out = 0;
    for (i=0; i<LZW_HASH_SIZE; i++) {
	table[i].code = -1;
	table[i].base_code = -1;
	table[i].ch = 0;
    }
}

static void
lzw_init(lzw_state_t *state)
{
    memset(state, 0, sizeof(lzw_state_t));
    state->next_code = LZW_FIRST;
    state->code_bit_length = 9;
    state->output_bits = 0;
    state->output_bits_count = 0;
    state->lzwstr = -1;
    lzw_reset(state);
}

lzw_state_t *
lzw_new(void)
{
    lzw_state_t *state = (lzw_state_t *)malloc(sizeof(lzw_state_t));
    if (state != (lzw_state_t *)NULL)
	lzw_init(state);
    return state;
}

    
/* Returns hash index of code/ch if found, or index of an empty location */
static int
lzw_find_match(lzw_code_t *table, short code, unsigned char ch)
{
    int i = (ch << 4) ^ code;
    int hash_offset = (i == 0) ? 1 : LZW_HASH_SIZE - i;
    
    while (table) { /* while (true) */
	if (table[i].code == -1)
	    break;	/* not in table */
	else if ((table[i].base_code == code) && (table[i].ch == ch))
	    break;	/* found it */
	else {
	    /* search forwards from here */
	    i += hash_offset;
	    if (i >= LZW_HASH_SIZE)
		i -= LZW_HASH_SIZE;
	}
    }
    return i;
}

void
lzw_compress(lzw_state_t *state,
    const unsigned char *inbuf, int *inlen,
    unsigned char *outbuf, int *outlen)
{
    int icount = 0;
    int ilen = *inlen;
    int ocount = 0;
    int olen = *outlen;
    unsigned char ch;
    int hash_index;
    int bits = state->output_bits;
    int len = state->output_bits_count;
    int code_len = state->code_bit_length;
    short lzwstr = state->lzwstr;
    short next_code = state->next_code;
    lzw_code_t *table = state->table;
    int do_reset = 0;
    int bytes_in = state->bytes_in;
    int bytes_out = state->bytes_out;

    if (lzwstr == -1) {
	/* get first char */
	lzwstr = inbuf[icount++];
	/* PostScript LZWEncode always starts with LZW_RESET */
	bits = LZW_RESET;
	len = code_len;
    }
    /* Write out any bits we couldn't fit last time */
    while ((len >= 8) && (ocount < olen)) {
	outbuf[ocount++] = (unsigned char)(bits >> (len-8));
	len -= 8;
    }
    while ((icount < ilen) && (ocount < olen)) {
	ch = inbuf[icount++];
	hash_index = lzw_find_match(table, lzwstr, ch);
	if (table[hash_index].code != -1)
	    lzwstr = table[hash_index].code;
	else {
	    /* Output this code */
	    bits = (bits << code_len) + lzwstr;
	    len += code_len;
	    while ((len >= 8) && (ocount < olen)) {
		outbuf[ocount++] = (unsigned char)(bits >> (len-8));
		len -= 8;
	    }

	    if (next_code == state->code_change) {
    		state->code_bit_length = ++code_len;
		state->code_change = (short)((1 << code_len) - 1);
		/* Monitor compression efficiency */
		bytes_in = state->bytes_in + icount;
		bytes_out = state->bytes_out + ocount;
		if (bytes_out > bytes_in + bytes_in/16) {
		    /* Data is not compressing */
		    /* Reset the table to avoid ratio getting worse */
		    do_reset = 1;
		}
	    }

	    if (do_reset || (next_code >= LZW_MAX)) {
		/* Table is full or poor efficiency, so start again */
		bits = (bits << code_len) + LZW_RESET;
		len += code_len;
		while ((len >= 8) && (ocount < olen)) {
		    outbuf[ocount++] = (unsigned char)(bits >> (len-8));
		    len -= 8;
		}
		lzw_reset(state);
		lzwstr = ch;
    		next_code = state->next_code;
		code_len = state->code_bit_length;
		do_reset = 0;
	    }
	    else {
		/* Add new code to table */
		table[hash_index].code = next_code++;
		table[hash_index].base_code = lzwstr;
		table[hash_index].ch = ch;
		lzwstr = ch;
	    }
	}
    }
    if (*inlen == 0) {
	/* Flush and EOD */
	bits = (bits << 2*code_len) + (lzwstr << code_len) + LZW_EOD;
	len += 2*code_len;
	while ((len >= 8) && (ocount < olen)) {
	    outbuf[ocount++] = (unsigned char)(bits >> (len-8));
	    len -= 8;
	}
	if ((len > 0) && (ocount < olen))
	    outbuf[ocount++] = (unsigned char)(bits << (8-len));
    }

    /* Save state for next time */
    state->output_bits = bits;
    state->output_bits_count = len;
    state->code_bit_length = code_len;
    state->lzwstr = lzwstr;
    state->next_code = next_code;
    state->bytes_in += icount;
    state->bytes_out += ocount;
    *outlen = ocount;	/* bytes written to output buffer */
    *inlen = icount;	/* input bytes used */
}

void
lzw_free(lzw_state_t *state)
{
    free(state);
}


#ifdef STANDALONE
#include <stdio.h>
int main(int argc, char *argv[])
{
    char outbuf[4096];
    int outlen = sizeof(outbuf);
    int outcount;
    char inbuf[4096];
    int inlen;
    int incount;
    int inused;
    lzw_state_t *lzw;
    FILE *infile = NULL;
    FILE *outfile = NULL;
    int inread=0, outwritten=0;
    
    if (argc != 3)
	return 1;

    infile = fopen(argv[1], "rb");
    if (infile == (FILE*)NULL)
	return 1;
    outfile = fopen(argv[2], "wb");
    if (outfile == (FILE*)NULL)
	return 1;

    lzw = lzw_new();
    while ((incount = fread(inbuf, 1, sizeof(inbuf), infile)) != 0) {
	inread += incount;
	inused = 0;
	inlen = incount;
	while (inused < incount) {
	    outlen = sizeof(outbuf);
	    inlen = incount - inused;
	    lzw_compress(lzw, inbuf+inused, &inlen, outbuf, &outlen);
	    inused += inlen;
	    if (outlen)
		fwrite(outbuf, 1, outlen, outfile);
	    outwritten += outlen;
	}
    }
    inlen = 0;	/* EOD */
    outlen = sizeof(outbuf);
    lzw_compress(lzw, inbuf, &inlen, outbuf, &outlen);
    lzw_free(lzw);
    if (outlen)
	fwrite(outbuf, 1, outlen, outfile);
    outwritten += outlen;

    fprintf(stdout, "in=%d out=%d\n", inread, outwritten);

    fclose(infile);
    fclose(outfile);

    return 0;
}
#endif
