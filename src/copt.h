/* Copyright (C) 2001-2002 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: copt.h,v 1.10 2003/01/29 06:55:17 ghostgum Exp $ */
/* Options header */

/* Public */
int opt_init(OPTION *opt);
int opt_read(OPTION *opt, LPCTSTR ininame);
int opt_write(OPTION *opt, LPCTSTR ininame);
int opt_media(OPTION *opt, int n, const char **name, 
    float *width, float *height);

/****************************************************/
/* Private */
#ifdef DEFINE_COPT

#define DEFAULT_MEDIA "a4"
/*
#define DEFAULT_MEDIA "letter"
*/
#define DEFAULT_RESOLUTION 72.0
#define DEFAULT_ZOOMRES 300.0

#define INISECTION "GSview"

typedef enum UNIT_e {
    UNIT_PT = 0,
    UNIT_MM = 1,
    UNIT_IN = 2
} UNIT;

typedef enum SAFER_e {
    SAFER_UNSAFE = 0,
    SAFER_SAFE = 1,
    SAFER_PARANOID = 2
} SAFER;

struct MEDIA_s {
    char name[MAXSTR];	/* name of media  */
    float width;		/* pts */
    float height;		/* pts */

    ORIENT orientation;
    BOOL swap_landscape; /* landscape <-> seascape */

    BOOL rotate;	/* width and height swapped */

    float xdpi;		/* dpi */
    float ydpi;		/* dpi */
    DISPLAY_FORMAT depth;
    int alpha_text;	/* bpp */
    int alpha_graphics;	/* bpp */
};

#define MAX_USERMEDIA 16
typedef struct USERMEDIA_s USERMEDIA;
struct USERMEDIA_s {
    char name[MAXSTR];	/* name of media  */
    float width;		/* pts */
    float height;		/* pts */
};

/* options that are saved in INI file */
struct OPTION_s {
	int	language;
	int	gsversion;
	BOOL	savesettings;
	POINT	img_origin;
	POINT	img_size;

	/* Ghostscript */
	char	gsdll[MAXSTR];
	char	gsinclude[MAXSTR];
	char	gsother[MAXSTR];

	/* GhostPCL */
	char	pldll[MAXSTR];

	/* Security */
	SAFER	safer;

	/* Unit */
	UNIT	unit;
	BOOL	unitfine;

	/* Pagesize */
	float	custom_width;
	float	custom_height;
	USERMEDIA usermedia[MAX_USERMEDIA];
	BOOL	variable_pagesize;
	MEDIA	media;

	/* DSC */
	int	dsc_warn;	/* level of DSC error warnings */
	BOOL    ignore_dsc;
	BOOL	epsf_clip;

	/* PDF */
	BOOL	pdf_crop;

	/* Display */
	float	zoom_xdpi;
	float	zoom_ydpi;
	float	pcl_dpi;

#ifdef NOTUSED
	BOOL	configured;
	BOOL	img_max;
	int	pstotext;
	BOOL	button_show;
#endif

/* FIX: are these used yet? */
	BOOL	epsf_warn;

	/* Do NOT save the following in the INI file */
/* FIX: put this in the user dictionary during gssrv_run_header for PDF */
	char pdf_password[MAXSTR];	/* -sPDFPassword= */


#ifdef NOTUSED
	BOOL	epsf_warn;
	BOOL	redisplay;
	BOOL	show_bbox;
	BOOL	save_dir;
        /* for printing to GS device */
	char	printer_device[64];	/* Ghostscript device for printing */
	char	printer_resolution[64];
	BOOL	print_fixed_media;
	/* for converting with GS device */
	char	convert_device[64];
	char	convert_resolution[64];	/* Ghostscript device for converting */
	BOOL	convert_fixed_media;
	/* for printing to GDI device */
	int	print_gdi_depth;	/* IDC_MONO, IDC_GREY, IDC_COLOUR */
	BOOL	print_gdi_fixed_media;
        /* general printing */
#define PRINT_GDI 0
#define PRINT_GS 1
#define PRINT_PS 2
#define PRINT_CONVERT 3
	int	print_method;		/* GDI, GS, PS */
	BOOL	print_reverse;		/* pages to be in reverse order */
	BOOL	print_to_file;
	char	printer_queue[MAXSTR];	/* for Win32 */
	int	pdf2ps;
	BOOL	auto_bbox;
	MATRIX	ctm;
	MEASURE measure;
#endif

};
#endif /* DEFINE_COPT */
