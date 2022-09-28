/* Copyright (C) 1993-2002 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cargs.h,v 1.1 2002/04/17 11:39:07 ghostgum Exp $ */
/* Argument parsing header */

#ifndef GSVIEW_ARGS_TYPEDEF
#define GSVIEW_ARGS_TYPEDEF
typedef struct GSVIEW_ARGS_s GSVIEW_ARGS;
#endif

struct GSVIEW_ARGS_s {
    int debug;			/* /d */
    int multithread;		/* /t */
    int help;			/* -help */
    int version;		/* -version */
    TCHAR filename[MAXSTR];	/* filename */
    TCHAR queue[MAXSTR];		/* /pQUEUE  or /sQUEUE */
    char device[64];		/* /fDEVICE */
    int print;			/* /p */
    int convert;		/* /f */
    int spool;			/* /s */
#ifdef NOTUSED
    int existing;		/* /e */
    int exit_existing; 		/* /x */
#endif
    char media[64];		/* /ma4 */
    int orient;	/* ORIENT_AUTO etc */	/* /oportrait */
    float xdpi;			/* -rXDPIxYDPI */
    float ydpi;
    int geometry;		/* -geometry WIDTHxHEIGHT+XOFFSET+YOFFSET */
    int geometry_width;
    int geometry_height;
    int geometry_xoffset;
    int geometry_yoffset;
};

int parse_argv(GSVIEW_ARGS *args, int argc, TCHAR *argv[]);
void app_use_args(GSview *app, GSVIEW_ARGS *args);
void view_use_args(View *v, GSVIEW_ARGS *args);

