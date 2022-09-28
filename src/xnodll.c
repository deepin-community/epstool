/* Copyright (C) 2004 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: xnodll.c,v 1.2 2004/11/29 08:09:33 ghostgum Exp $ */
/* Stubs for platforms without shared objects or DLLs */

#include <stdio.h>
#include <string.h>
#include "common.h"
#include "cdll.h"

/******************************************************************/

static int 
dll_msg(TCHAR *msg, LPCTSTR str, int msglen)
{
    int len = (int)cslen(str);
    if (len < msglen){
	csncat(msg, str, len);
	msglen -= len;
    }
    return msglen;
}



/* platform dependent */
/* Load DLL */
int
dll_open(GGMODULE *hmodule, LPCTSTR name, TCHAR *msg, int msglen)
{
    memset(msg, 0, msglen);
    msglen = dll_msg(msg, TEXT("Trying to load "), msglen);
    msglen = dll_msg(msg, name, msglen);
    msglen = dll_msg(msg, TEXT("\n"), msglen);
    msglen = dll_msg(msg, TEXT("Can't load libraries on this platform.\n"), msglen);
    *hmodule = NULL;
    return -1;
}

/* Unload DLL */
int
dll_close(GGMODULE *hmodule)
{
    return -1;
}

dll_proc
dll_sym(GGMODULE *hmodule, const char *name)
{
    return NULL;
}



/******************************************************************/
