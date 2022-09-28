/* Copyright (C) 2001-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: wdll.c,v 1.1.2.6 2005/06/10 09:39:24 ghostgum Exp $ */

#include <windows.h>
#include <stdio.h>
#include "cplat.h"
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


#ifndef ERROR_DLL_NOT_FOUND
#define ERROR_DLL_NOT_FOUND 1157L
#endif

/* display error message for LoadLibrary */
static int
load_library_error(LPCTSTR dllname, TCHAR *msg, int msglen)
{
LPCTSTR text_reason;
TCHAR buf[MAX_PATH+128];
int reason;
LPVOID lpMessageBuffer;
    reason = GetLastError() & 0xffff;
    switch (reason) {
	case ERROR_FILE_NOT_FOUND:	/* 2 */
	    text_reason = TEXT("File not found");
	    break;
	case ERROR_PATH_NOT_FOUND:	/* 3 */
	    text_reason = TEXT("Path not found");
	    break;
	case ERROR_NOT_ENOUGH_MEMORY:	/* 8 */
	    text_reason = TEXT("Not enough memory");
	    break;
	case ERROR_BAD_FORMAT:		/* 11 */
	    text_reason = TEXT("Bad EXE or DLL format");
	    break;
	case ERROR_OUTOFMEMORY:		/* 14 */
	    text_reason = TEXT("Out of memory");
	    break;
	case ERROR_DLL_NOT_FOUND:	/* 1157 */
	    text_reason = TEXT("DLL not found");
	    break;
	default:
	    text_reason = (TCHAR *)NULL;
    }
    if (text_reason)
        csnprintf(buf, sizeof(buf)/sizeof(TCHAR), 
	    TEXT("Failed to load %s, error %d = %s\n"), 
	    dllname, reason, text_reason);
    else
	csnprintf(buf, sizeof(buf)/sizeof(TCHAR),
	    TEXT("Failed to load %s, error %d\n"), dllname, reason);
    msglen = dll_msg(msg, buf, msglen);

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
	FORMAT_MESSAGE_FROM_SYSTEM,
	NULL, reason,
	MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* user default language */
	(LPTSTR) &lpMessageBuffer, 0, NULL);
    if (lpMessageBuffer) {
        msglen = dll_msg(msg, (LPTSTR)lpMessageBuffer, msglen);
        msglen = dll_msg(msg, TEXT("\r\n"), msglen);
	LocalFree(LocalHandle(lpMessageBuffer));
    }
    return msglen;
}

/* Load DLL.  Write log of actions in msg */
int
dll_open(GGMODULE *hmodule, LPCTSTR name, TCHAR *msg, int msglen)
{
LPCTSTR shortname;
TCHAR fullname[MAX_PATH];
TCHAR *p;
HINSTANCE hInstance = GetModuleHandle(NULL);
    memset(msg, 0, msglen);
    msglen = dll_msg(msg, TEXT("Trying to load "), msglen);
    msglen = dll_msg(msg, name, msglen);
    msglen = dll_msg(msg, TEXT("\n"), msglen);

    /* Try to load DLL first with given path */
    *hmodule = LoadLibrary(name);
    if (*hmodule == (GGMODULE)NULL) {
	/* failed */
	msglen = load_library_error(name, msg, msglen);
	/* try again, with path of EXE */
	if ((shortname = csrchr(name, '\\')) == (TCHAR *)NULL)
	    shortname = name;
	else
	    shortname++;

	GetModuleFileName(hInstance, fullname, sizeof(fullname));
	if ((p = csrchr(fullname,'\\')) != (TCHAR *)NULL)
	    p++;
	else
	    p = fullname;
	*p = '\0';
	csncat(fullname, shortname, sizeof(fullname)-cslen(fullname)-1);

	msglen = dll_msg(msg, TEXT("Trying to load "), msglen);
	msglen = dll_msg(msg, fullname, msglen);
	msglen = dll_msg(msg, TEXT("\n"), msglen);

	*hmodule = LoadLibrary(fullname);
	if (*hmodule == (GGMODULE)NULL) {
	    /* failed again */
	    msglen = load_library_error(fullname, msg, msglen);
	    /* try once more, this time on system search path */
	    msglen = dll_msg(msg, TEXT("Trying to load "), msglen);
	    msglen = dll_msg(msg, shortname, msglen);
	    msglen = dll_msg(msg, TEXT("\n"), msglen);
	    *hmodule = LoadLibrary(shortname);
	    if (*hmodule == (GGMODULE)NULL) {
		/* failed again */
		msglen = load_library_error(shortname, msg, msglen);
	    }
	}
    }

    if (*hmodule == (GGMODULE)NULL) {
	*hmodule = (GGMODULE)NULL;
	return -1;
    }

    return 0;
}


int
dll_close(GGMODULE *hmodule)
{
    FreeLibrary(*hmodule);
    return 0;
}


dll_proc
dll_sym(GGMODULE *hmodule, const char *name)
{
    return (dll_proc)GetProcAddress(*hmodule, name);
}

/******************************************************************/
