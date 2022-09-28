/* Copyright (C) 2000-2002 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: xdll.c,v 1.3 2003/05/06 13:17:40 ghostgum Exp $ */
/* Unix shared object loading using dlopen */

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
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
/* Load Ghostscript DLL */
int
dll_open(GGMODULE *hmodule, LPCTSTR name, TCHAR *msg, int msglen)
{
char *shortname;
char *s;
TCHAR buf[MAXSTR];


    memset(msg, 0, msglen);
    msglen = dll_msg(msg, TEXT("Trying to load "), msglen);
    msglen = dll_msg(msg, name, msglen);
    msglen = dll_msg(msg, TEXT("\n"), msglen);

    /* Try to load DLL first with given path */
    *hmodule = dlopen(name, RTLD_NOW);
    if (*hmodule == NULL) {
	/* failed */
	msglen = dll_msg(msg, TEXT(" Failed, "), msglen);
	msglen = dll_msg(msg, dlerror(), msglen);
	msglen = dll_msg(msg, TEXT("\n"), msglen);
	/* try once more, this time on system search path */
	memset(buf, 0, sizeof(buf));
	strncpy(buf, name, sizeof(buf)-1);
	s = shortname = buf;
	while (*s) {
	    if (*s == '/')
		shortname = s+1;
	    s = CHARNEXT(s);
 	}
	msglen = dll_msg(msg, TEXT("Trying to load "), msglen);
	msglen = dll_msg(msg, shortname, msglen);
	msglen = dll_msg(msg, TEXT("\n"), msglen);
	*hmodule = dlopen(shortname, RTLD_NOW);
	if (*hmodule == NULL) {
	    /* failed again */
	    msglen = dll_msg(msg, TEXT(" Failed, "), msglen);
	    msglen = dll_msg(msg, dlerror(), msglen);
	    msglen = dll_msg(msg, TEXT("\n"), msglen);
	}
    }
    return 0;
}

/* Unload Ghostscript DLL */
int
dll_close(GGMODULE *hmodule)
{
    dlclose(*hmodule);
    return 0;
}

dll_proc
dll_sym(GGMODULE *hmodule, const char *name)
{
    return (dll_proc)dlsym(*hmodule, name);
}



/******************************************************************/
