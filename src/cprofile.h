/* Copyright (C) 2001-2002 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cprofile.h,v 1.2 2002/05/27 09:43:38 ghostgum Exp $ */
/* Windows style INI files */

/* Public */
#ifndef PROFILE_TYPEDEF
#define PROFILE_TYPEDEF
typedef struct PROFILE_s PROFILE;
#endif

PROFILE * profile_open(LPCTSTR filename);
int profile_read_string(PROFILE *prf, const char *section, const char *entry, 
    const char *def, char *buffer, int len);
BOOL profile_write_string(PROFILE *prf, const char *section, const char *entry, 
    const char *value);
BOOL profile_close(PROFILE *prf);

/****************************************************/
/* Private */
#ifdef DEFINE_CPROFILE

typedef struct prfentry_s prfentry;
typedef struct prfsection_s prfsection;

struct prfentry_s {
	char *name;
	char *value;
	prfentry *next;
};

struct prfsection_s {
	char *name;
	prfentry *entry;
	prfsection *next;
};

#ifdef NOTUSED
typedef struct prop_item_s prop_item;
struct prop_item_s {
	char name[MAXSTR];
	char value[MAXSTR];
};
#endif

struct PROFILE_s {
	TCHAR *name;
	FILE *file;
	BOOL changed;
	prfsection *section;
};
#endif

