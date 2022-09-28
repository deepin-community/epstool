/* Copyright (C) 1995-2015 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: epstool.c,v 1.69 2005/06/10 08:45:36 ghostgum Exp $ */

#include "common.h"
#include "dscparse.h"
#include "errors.h"
#include "iapi.h"
#include "gdevdsp.h"
#define DEFINE_COPT
#include "copt.h"
#define DEFINE_CAPP
#include "capp.h"
#include "cbmp.h"
#define DEFINE_CDOC
#include "cdoc.h"
#include "cdll.h"
#include "cgssrv.h"
#include "cmac.h"
#include "ceps.h"
#include "cimg.h"
#include "cpagec.h"
#include "cres.h"
#ifdef __WIN32__
#include "wgsver.h"
#endif
#if defined(UNIX) || defined(OS2)
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#endif

const char *epstool_name = "epstool";
const char *epstool_version = "3.09";  /* should be EPSTOOL_VERSION */
const char *epstool_date = "2015-03-13"; /* should be EPSTOOL_DATE */
const char *copyright = "Copyright 1995-2015 Ghostgum Software Pty Ltd";

const char *cmd_help = "\
Commands (one only):\n\
  --add-tiff4-preview        or  -t4\n\
  --add-tiff6u-preview       or  -t6u\n\
  --add-tiff6p-preview       or  -t6p\n\
  --add-tiff-preview         or  -tg\n\
  --add-interchange-preview  or  -i\n\
  --add-metafile-preview     or  -w\n\
  --add-pict-preview\n\
  --add-user-preview filename\n\
  --dcs2-multi\n\
  --dcs2-single\n\
  --dcs2-report\n\
  --extract-postscript       or  -p\n\
  --extract-preview          or  -v\n\
  --bitmap\n\
  --copy\n\
  --dump\n\
  --help                     or  -h\n\
  --test-eps\n\
  --version\n\
";

const char *opt_help = "\
Options:\n\
  --bbox                     or  -b\n\
  --combine-separations filename\n\
  --combine-tolerance pts\n\
  --custom-colours filename\n\
  --debug                    or  -d\n\
  --device name\n\
  --doseps-reverse\n\
  --dpi resolution\n\
  --dpi-render resolution\n\
  --ignore-information\n\
  --ignore-warnings\n\
  --ignore-errors\n\
  --gs command\n\
  --gs-args arguments\n\
  --mac-binary\n\
  --mac-double\n\
  --mac-rsrc\n\
  --mac-single\n\
  --missing-separations\n\
  --output filename\n\
  --page-number\n\
  --quiet\n\
  --rename-separation old_name new_name\n\
  --replace-composite\n\
";


typedef enum {
    CMD_UNKNOWN,
    CMD_TIFF4,
    CMD_TIFF6U,
    CMD_TIFF6P,
    CMD_TIFF,
    CMD_INTERCHANGE,
    CMD_WMF,
    CMD_PICT,
    CMD_USER,
    CMD_DCS2_MULTI,
    CMD_DCS2_SINGLE,
    CMD_DCS2_REPORT,
    CMD_PREVIEW,
    CMD_POSTSCRIPT,
    CMD_BITMAP,
    CMD_COPY,
    CMD_DUMP,
    CMD_HELP,
    CMD_TEST,
    CMD_VERSION
} CMD;

typedef enum{
    CUSTOM_CMYK,
    CUSTOM_RGB
} CUSTOM_COLOUR_TYPE;
typedef struct CUSTOM_COLOUR_s CUSTOM_COLOUR;
struct CUSTOM_COLOUR_s {
    char name[MAXSTR];
    CUSTOM_COLOUR_TYPE type;
    /* if type=CUSTOM_CMYK */
    float cyan;
    float magenta;
    float yellow;
    float black;
    /* if type=CUSTOM_RGB */
    float red;
    float green;
    float blue;
    /* Next colour */
    CUSTOM_COLOUR *next;
};
typedef struct OPT_s {
    CMD cmd;
    TCHAR device[64];	/* --device name for --bitmap or --add-tiff-preview */
    BOOL composite;		/* --replace-composite */
    BOOL bbox;			/* --bbox */
    int dscwarn;		/* --ignore-warnings etc. */
    TCHAR gs[MAXSTR];		/* --gs command */
    TCHAR gsargs[MAXSTR*4];	/* --gs-args arguments */
    TCHAR input[MAXSTR];	/* filename */
    TCHAR output[MAXSTR];	/* --output filename or (second) filename */
    TCHAR user_preview[MAXSTR];	/* --add-user-preview filename */
    BOOL quiet;			/* --quiet */
    BOOL debug;			/* --debug */
    BOOL doseps_reverse;	/* --doseps-reverse */
    float dpi;			/* --dpi resolution */
    float dpi_render;		/* --dpi-render resolution */
    BOOL help;			/* --help */
    TCHAR custom_colours[MAXSTR]; /* --custom-colours filename */
    CUSTOM_COLOUR *colours;
    BOOL missing_separations;	/* --missing-separations */
    TCHAR combine[MAXSTR];	/* --combine-separations filename */
    int tolerance;		/* --combine-tolerance pts */
    RENAME_SEPARATION *rename_sep; /* --rename-separation */
    CMAC_TYPE mac_type;		/* --mac-binary, --mac-double, --mac-single */
				/* or --mac-rsrc */
    int page;			/* --page-number for --bitmap */
    int image_compress; 	/* IMAGE_COMPRESS_NONE, RLE, LZW */
    int image_encode;		/* IMAGE_ENCODE_HEX, ASCII85 */
} OPT;


#define MSGOUT stdout
#ifdef UNIX
const char gsexe[] = "gs";
#define COLOUR_DEVICE "ppmraw"
#define GREY_DEVICE "pgmraw"
#define MONO_DEVICE "pbmraw"
#endif

#ifdef OS2
TCHAR gsexe[MAXSTR] = TEXT("gsos2.exe");
#define COLOUR_DEVICE TEXT("bmp16m")
#define GREY_DEVICE TEXT("bmpgray")
#define MONO_DEVICE TEXT("bmpmono")
#endif

#ifdef _WIN32
TCHAR gsexe[MAXSTR] = TEXT("gswin32c.exe");
#define COLOUR_DEVICE TEXT("bmp16m")
#define GREY_DEVICE TEXT("bmpgray")
#define MONO_DEVICE TEXT("bmpmono")
#endif


static void print_help(void);
static void print_version(void);
static int parse_args(OPT *opt, int argc, TCHAR *argv[]);
static Doc * epstool_open_document(GSview *app, OPT *opt, TCHAR *name);
static int epstool_add_preview(Doc *doc, OPT *opt);
static int epstool_dcs2_copy(Doc *doc, Doc *doc2, OPT *opt);
static int epstool_dcs2_report(Doc *doc);
static int epstool_dcs2_composite(Doc *doc, OPT *opt, GFile *compfile);
static int epstool_dcs2_check_files(Doc *doc, OPT *opt);
static int epstool_extract(Doc *doc, OPT *opt);
static int epstool_bitmap(Doc *doc, OPT *opt);
static int epstool_copy(Doc *doc, OPT *opt);
static int epstool_copy_bitmap(Doc *doc, OPT *opt);
static int epstool_test(Doc *doc, OPT *opt);
static void epstool_dump_fn(void *caller_data, const char *str);

static IMAGE *make_preview_image(Doc *doc, OPT *opt, int page, LPCTSTR device,
    CDSCBBOX *bbox, CDSCFBBOX *hires_bbox, int calc_bbox);
static int make_preview_file(Doc *doc, OPT *opt, int page, 
    LPCTSTR preview, LPCTSTR device,
    float dpi, CDSCBBOX *bbox, CDSCFBBOX *hires_bbox, int calc_bbox);
static int calculate_bbox(Doc *doc, OPT *opt, LPCTSTR psname, 
    CDSCBBOX *bbox, CDSCFBBOX *hires_bbox);
static int calc_device_size(float dpi, CDSCBBOX *bbox, CDSCFBBOX *hires_bbox,
    int *width, int *height, float *xoffset, float *yoffset);
static int exec_program(LPTSTR command,
    int hstdin, int hstdout, int hstderr,
    LPCTSTR stdin_name, LPCTSTR stdout_name, LPCTSTR stderr_name);
static int custom_colours_read(OPT *opt);
static CUSTOM_COLOUR *custom_colours_find(OPT *opt, const char *name);
static void custom_colours_free(OPT *opt);
static int colour_to_cmyk(CDSC *dsc, const char *name, 
   float *cyan, float *magenta, float *yellow, float *black);


static void
print_version(void)
{
    fprintf(MSGOUT, "%s %s %s\n", epstool_name, epstool_version, epstool_date);
}

static void 
print_help(void)
{
    print_version();
    fprintf(MSGOUT, "%s\n", copyright);
    fprintf(MSGOUT, "Usage: epstool command [options] inputfile outputfile\n");
    fprintf(MSGOUT, "%s", cmd_help);
    fprintf(MSGOUT, "%s", opt_help);
}

/* return 0 on success, or argument index if there is a duplicated
 * command, a missing parameter, or an unknown argument.
 */
static int
parse_args(OPT *opt, int argc, TCHAR *argv[])
{
    int arg;
    const TCHAR *p;
    memset(opt, 0, sizeof(OPT));
    opt->cmd = CMD_UNKNOWN;
    opt->dpi = 72.0;
    opt->dpi_render = 0.0;
    opt->mac_type = CMAC_TYPE_NONE;
    opt->dscwarn = CDSC_ERROR_NONE;
    opt->image_encode = IMAGE_ENCODE_ASCII85;
    opt->image_compress = IMAGE_COMPRESS_LZW;
    csncpy(opt->gs, gsexe, sizeof(opt->gs)/sizeof(TCHAR)-1);
    for (arg=1; arg<argc; arg++) {
	p = argv[arg];
	if ((cscmp(p, TEXT("--add-tiff4-preview")) == 0) ||
	    (cscmp(p, TEXT("-t4")) == 0)) {
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_TIFF4;
	}
	else if ((cscmp(p, TEXT("--add-tiff6u-preview")) == 0) ||
	    (cscmp(p, TEXT("-t6u")) == 0)) {
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_TIFF6U;
	}
	else if ((cscmp(p, TEXT("--add-tiff6p-preview")) == 0) ||
	    (cscmp(p, TEXT("-t6p")) == 0)) {
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_TIFF6P;
	}
	else if ((cscmp(p, TEXT("--add-tiff-preview")) == 0) ||
	    (cscmp(p, TEXT("-tg")) == 0)) {
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_TIFF;
	}
	else if ((cscmp(p, TEXT("--add-interchange-preview")) == 0) ||
	    (cscmp(p, TEXT("-i")) == 0)) {
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_INTERCHANGE;
	}
	else if ((cscmp(p, TEXT("--add-metafile-preview")) == 0) ||
	    (cscmp(p, TEXT("-w")) == 0)) {
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_WMF;
	}
	else if ((cscmp(p, TEXT("--add-pict-preview")) == 0) ||
	    (cscmp(p, TEXT("-w")) == 0)) {
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_PICT;
	}
	else if (cscmp(p, TEXT("--add-user-preview")) == 0) {
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_USER;
	    arg++;
	    if (arg == argc)
		return arg;
	    csncpy(opt->user_preview, argv[arg], 
		sizeof(opt->user_preview)/sizeof(TCHAR)-1);
	}
	else if (cscmp(p, TEXT("--dcs2-multi")) == 0) {
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_DCS2_MULTI;
	}
	else if (cscmp(p, TEXT("--dcs2-single")) == 0) {
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_DCS2_SINGLE;
	}
	else if (cscmp(p, TEXT("--dcs2-report")) == 0) {
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_DCS2_REPORT;
	}
	else if (cscmp(p, TEXT("--dump")) == 0) {
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_DUMP;
	}
	else if (cscmp(p, TEXT("--test-eps")) == 0) {
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_TEST;
	}
	else if (cscmp(p, TEXT("--replace-composite")) == 0) {
	    opt->composite = TRUE;
	}
	else if (cscmp(p, TEXT("--missing-separations")) == 0) {
	    opt->missing_separations = TRUE;
	}
	else if (cscmp(p, TEXT("--combine-separations")) == 0) {
	    arg++;
	    if (arg == argc)
		return arg;
	    csncpy(opt->combine, argv[arg], sizeof(opt->combine)/sizeof(TCHAR)-1);
	}
	else if (cscmp(p, TEXT("--combine-tolerance")) == 0) {
	    char buf[MAXSTR];
	    arg++;
	    if (arg == argc)
		return arg;
	    cs_to_narrow(buf, (int)sizeof(buf)-1, argv[arg], (int)cslen(argv[arg])+1);
	    opt->tolerance = atoi(buf);
	}
	else if (cscmp(p, TEXT("--rename-separation")) == 0) {
	    RENAME_SEPARATION *rs = opt->rename_sep;
	    arg++;
	    if (arg+1 == argc)
		return arg;
	    if (rs) {
		while (rs && (rs->next != NULL))
		    rs = rs->next;
		rs->next = 
		    (RENAME_SEPARATION *)malloc(sizeof(RENAME_SEPARATION));
		rs = rs->next;
	    }
	    else {
		opt->rename_sep = rs = 
		    (RENAME_SEPARATION *)malloc(sizeof(RENAME_SEPARATION));
	    }
	    if (rs) {
		char oldname[MAXSTR];
		char newname[MAXSTR];
		char *p;
		char *q;
		int len;
		memset(rs, 0, sizeof(RENAME_SEPARATION));
		rs->next = NULL;
		memset(oldname, 0, sizeof(oldname));
	        cs_to_narrow(oldname, (int)sizeof(oldname)-1, 
		    argv[arg], (int)cslen(argv[arg])+1);
		len = (int)strlen(oldname)+1;
		p = (char *)malloc(len);
		if (p) {
		    memcpy(p, oldname, len);
		    rs->oldname = p;
		}
		arg++;
		memset(newname, 0, sizeof(newname));
	        cs_to_narrow(newname, (int)sizeof(newname)-1, 
		    argv[arg], (int)cslen(argv[arg])+1);
		len = (int)strlen(newname)+1;
		q = (char *)malloc(len);
		if (q) {
		    memcpy(q, newname, len);
		    rs->newname = q;
		}
		if ((p == NULL) || (q == NULL)) {
		    fprintf(stderr, "Out of memory\n");
		    return arg;
		}
	    }
	    else {
		fprintf(stderr, "Out of memory\n");
		return arg;
	    }
	}
	else if (cscmp(p, TEXT("--custom-colours")) == 0) {
	    arg++;
	    if (arg == argc)
		return arg;
	    csncpy(opt->custom_colours, argv[arg], 
		sizeof(opt->custom_colours)/sizeof(TCHAR)-1);
	}
	else if ((cscmp(p, TEXT("--extract-preview")) == 0) ||
	    (cscmp(p, TEXT("-v")) == 0)) {
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_PREVIEW;
	}
	else if ((cscmp(p, TEXT("--extract-postscript")) == 0) ||
	    (cscmp(p, TEXT("-p")) == 0)) {
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_POSTSCRIPT;
	}
	else if (cscmp(p, TEXT("--bitmap")) == 0) {
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_BITMAP;
	}
	else if (cscmp(p, TEXT("--copy")) == 0) {
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_COPY;
	}
	else if (cscmp(p, TEXT("--device")) == 0) {
	    arg++;
	    if (arg == argc)
		return arg;
	    csncpy(opt->device, argv[arg], sizeof(opt->device)/sizeof(TCHAR)-1);
	}
	else if (cscmp(p, TEXT("--page-number")) == 0) {
	    char buf[MAXSTR];
	    arg++;
	    if (arg == argc)
		return arg;
	    cs_to_narrow(buf, (int)sizeof(buf)-1, argv[arg], 
		(int)cslen(argv[arg])+1);
	    opt->page = atoi(buf) - 1;
	    if (opt->page < 0)
		opt->page = 0;
	}
	else if ((cscmp(p, TEXT("--bbox")) == 0) ||
	    (cscmp(p, TEXT("-b")) == 0)) {
	    opt->bbox = TRUE;
	}
	else if (cscmp(p, TEXT("--ignore-information")) == 0) {
	    opt->dscwarn = CDSC_ERROR_INFORM;
	}
	else if (cscmp(p, TEXT("--ignore-warnings")) == 0) {
	    opt->dscwarn = CDSC_ERROR_WARN;
	}
	else if (cscmp(p, TEXT("--ignore-errors")) == 0) {
	    opt->dscwarn = CDSC_ERROR_ERROR;
	}
	else if (cscmp(p, TEXT("--gs")) == 0) {
	    arg++;
	    if (arg == argc)
		return arg;
	    csncpy(opt->gs, argv[arg], sizeof(opt->gs)/sizeof(TCHAR)-1);
	}
	else if (cscmp(p, TEXT("--gs-args")) == 0) {
	    arg++;
	    if (arg == argc)
		return arg;
	    csncpy(opt->gsargs, argv[arg], sizeof(opt->gsargs)/sizeof(TCHAR)-1);
	}
	else if (cscmp(p, TEXT("--mac-binary")) == 0) {
	    opt->mac_type = CMAC_TYPE_MACBIN;
	}
	else if (cscmp(p, TEXT("--mac-double")) == 0) {
	    opt->mac_type = CMAC_TYPE_DOUBLE;
	}
	else if (cscmp(p, TEXT("--mac-rsrc")) == 0) {
	    opt->mac_type = CMAC_TYPE_RSRC;
	}
	else if (cscmp(p, TEXT("--mac-single")) == 0) {
	    opt->mac_type = CMAC_TYPE_SINGLE;
	}
	else if (cscmp(p, TEXT("--output")) == 0) {
	    arg++;
	    if (arg == argc)
		return arg;
	    csncpy(opt->output, argv[arg], 
		sizeof(opt->output)/sizeof(TCHAR)-1);
	}
	else if (cscmp(p, TEXT("--quiet")) == 0) {
	    opt->quiet = TRUE;
	}
	else if ((cscmp(p, TEXT("--debug")) == 0) ||
	    (cscmp(p, TEXT("-d")) == 0)) {
	    opt->debug = TRUE;
	}
	else if ((cscmp(p, TEXT("--doseps-reverse")) == 0) ||
	    (cscmp(p, TEXT("-d")) == 0)) {
	    opt->doseps_reverse = TRUE;
	}
	else if (cscmp(p, TEXT("--dpi")) == 0) {
	    char buf[MAXSTR];
	    arg++;
	    if (arg == argc)
		return arg;
	    cs_to_narrow(buf, (int)sizeof(buf)-1, argv[arg], 
		(int)cslen(argv[arg])+1);
	    opt->dpi = (float)atof(buf);
	}	
	else if (cscmp(p, TEXT("--dpi-render")) == 0) {
	    char buf[MAXSTR];
	    arg++;
	    if (arg == argc)
		return arg;
	    cs_to_narrow(buf, (int)sizeof(buf)-1, argv[arg], 
		(int)cslen(argv[arg])+1);
	    opt->dpi_render = (float)atof(buf);
	}	
	else if (cscmp(p, TEXT("--image-encode")) == 0) {
	    /* Not in user documentation - for debug purposes only */
	    char buf[MAXSTR];
	    arg++;
	    if (arg == argc)
		return arg;
	    cs_to_narrow(buf, (int)sizeof(buf)-1, argv[arg], 
		(int)cslen(argv[arg])+1);
	    opt->image_encode = atoi(buf);
	    if ((opt->image_encode < IMAGE_ENCODE_HEX) ||
	        (opt->image_encode > IMAGE_ENCODE_ASCII85))
	    opt->image_encode = IMAGE_ENCODE_ASCII85;
	}	
	else if (cscmp(p, TEXT("--image-compress")) == 0) {
	    /* Not in user documentation - for debug purposes only */
	    char buf[MAXSTR];
	    arg++;
	    if (arg == argc)
		return arg;
	    cs_to_narrow(buf, (int)sizeof(buf)-1, argv[arg], 
		(int)cslen(argv[arg])+1);
	    opt->image_compress = atoi(buf);
	    if ((opt->image_compress < IMAGE_COMPRESS_NONE) ||
	        (opt->image_compress > IMAGE_COMPRESS_LZW))
	    opt->image_compress = IMAGE_COMPRESS_LZW;
	}
	else if ((cscmp(p, TEXT("--help")) == 0) || (cscmp(p, TEXT("-h"))==0)) {
	    opt->help = TRUE;
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_HELP;
	}
	else if (cscmp(p, TEXT("--version")) == 0) {
	    if (opt->cmd != CMD_UNKNOWN)
		return arg;
	    opt->cmd = CMD_VERSION;
	}
	else if (*p != '-') {
	    if (opt->input[0] == 0)
		csncpy(opt->input, argv[arg], 
		    sizeof(opt->input)/sizeof(TCHAR)-1);
	    else if (opt->output[0] == 0)
		csncpy(opt->output, argv[arg], 
		    sizeof(opt->output)/sizeof(TCHAR)-1);
	    else
		return arg;
	}
	else {
	    return arg;
	}
    }
    return 0;
}


static Doc *
epstool_open_document(GSview *app, OPT *opt, TCHAR *name)
{
    int code;
    Doc *doc;
    int require_eps = 1;
    /* Create a document */
    doc = doc_new(app);
    if (doc == NULL) {
	debug |= DEBUG_LOG;
	app_csmsgf(app, TEXT("Failed to create new Doc\n"));
	app_unref(app);
	return NULL;
    }
    /* Attach it to application */
    doc_add(doc, app);
    doc_dsc_warn(doc, opt->dscwarn);
    doc_verbose(doc, opt->debug);

    code = doc_open(doc, name);
    if (code < 0) {
	debug |= DEBUG_LOG;
	app_csmsgf(app, TEXT("Error opening file \042%s\042.\n"), name);
	code = -1;
    }
    else if (code > 0) {
	debug |= DEBUG_LOG;
	app_csmsgf(app, TEXT(
	"Input file \042%s\042 didn't have DSC comments.\n"), name);
	code = -1;
    }

    /* Some features don't require an EPS input */
    if (opt->cmd == CMD_DCS2_REPORT)
	require_eps = 0;
    if (opt->cmd == CMD_TEST)
	require_eps = 0;
    if (opt->cmd == CMD_DUMP)
	require_eps = 0;
    if ((opt->cmd == CMD_COPY) && (doc->doctype == DOC_BITMAP))
	require_eps = 0;

    if ((code == 0) && require_eps &&
	   ((doc->dsc == NULL) || (!doc->dsc->epsf)) ) {
	debug |= DEBUG_LOG;
	app_csmsgf(app, 
	    TEXT("Input file \042%s\042 is not EPSF.\n"), name);
	code = -1;
    }
    if ((code == 0) && require_eps && (doc->dsc != NULL)) {
	if ((doc->dsc->worst_error > CDSC_ERROR_INFORM) &&
	    (doc->dsc->worst_error > opt->dscwarn)) {
	    app_csmsgf(app, 
		TEXT("EPS had unacceptable warnings or errors.\n"));
	    code = -1;
	}
    }
    if (code) {
	doc_close(doc);
	doc_remove(doc);
	doc_unref(doc);
	doc = NULL;
    }
    return doc;
}


#ifdef UNICODE
int wmain(int argc, TCHAR *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    GSview *app;
    Doc *doc = NULL;
    Doc *doc2 = NULL;
    int code = 0;
    int i, arg;
    OPT opt;

#ifdef __WIN32__
    { 	/* Find latest version of Ghostscript */
	char buf[MAXSTR];
	if (find_gs(buf, sizeof(buf)-1, 550, FALSE)) {
	    narrow_to_cs(gsexe, (int)(sizeof(gsexe)/sizeof(TCHAR)-1),
		buf, (int)strlen(buf)+1);
	}
    }
#endif

    memset(&opt, 0, sizeof(opt));
    arg = parse_args(&opt, argc, argv);
    debug = 0;
    if (opt.debug) {
	debug = DEBUG_LOG | DEBUG_MEM | DEBUG_GENERAL;
	opt.quiet = 0;
    }
    if (!opt.quiet)
	debug |= DEBUG_LOG;	/* enable output */
    if (opt.help) {
	print_help();
	return 1;
    }
    if (opt.cmd == CMD_VERSION) {
	print_version();
	return 1;
    }
    if (opt.dpi_render < opt.dpi)
	opt.dpi_render = opt.dpi;

    app = app_new(NULL, FALSE);
    if (app == NULL) {
        fprintf(MSGOUT, "Can't create epstool app\n");
	return 1;
    }

    if (arg != 0) {
	debug |= DEBUG_LOG;
	app_csmsgf(app, TEXT("epstool: Error in command line arguments:\n  "));
	for (i=0; i<arg; i++)
	    app_csmsgf(app, TEXT("%s "), argv[i]);
	app_csmsgf(app, TEXT("\n    The next argument is unrecognised, missing, or conflicts:\n  "));
	for (; i<argc; i++)
	    app_csmsgf(app, TEXT("%s "), argv[i]);
	app_csmsgf(app, TEXT("\n"));
	app_unref(app);
	return 1;
    }
    
    if ((opt.cmd == CMD_TIFF) && (opt.device[0] == '\0')) {
	debug |= DEBUG_LOG;
	app_csmsgf(app, TEXT("--add-tiff-preview requires a device to be specified with --device\n"));
	code = -1;
    }
    if (opt.cmd == CMD_UNKNOWN) {
	debug |= DEBUG_LOG;
	app_csmsgf(app, TEXT("No command specified.\n"));
	code = -1;
    }
    if (opt.input[0] == '\0') {
	debug |= DEBUG_LOG;
	app_csmsgf(app, TEXT("Input file not specified.\n"));
	code = -1;
    }
    if (opt.custom_colours[0] != '\0') {
	FILE *f = csfopen(opt.custom_colours, TEXT("rb"));
	if (f == (FILE*)NULL) {
	    debug |= DEBUG_LOG;
	    app_csmsgf(app, TEXT("Failed to open \042%s\042.\n"), 
		opt.custom_colours);
	    code = -1;
	}
	else
	    fclose(f);
    }
    if (code == 0) {
	FILE *f = csfopen(opt.input, TEXT("rb"));
	if (f == (FILE*)NULL) {
	    debug |= DEBUG_LOG;
	    app_csmsgf(app, TEXT("Failed to open \042%s\042.\n"), opt.input);
	    code = -1;
	}
	else
	    fclose(f);
    }
    if ((opt.output[0] == '\0') && 
        !((opt.cmd == CMD_DCS2_REPORT) || 
	  (opt.cmd == CMD_TEST) ||
	  (opt.cmd == CMD_DUMP)) ) {
	debug |= DEBUG_LOG;
	app_csmsgf(app, TEXT("Output file not specified.\n"));
	code = -1;
    }
    if ((opt.output[0] == '-') && (opt.output[1] == '\0'))
        opt.output[0] = '\0';  /* use stdout */
    if (code != 0) {
	debug |= DEBUG_LOG;
	app_csmsgf(app, TEXT("Run \042epstool --help\042 for more details.\n"));
	app_unref(app);
	return 1;
    }

    if (opt.cmd == CMD_DUMP)
        dump_macfile(opt.input, 1);

    doc = epstool_open_document(app, &opt, opt.input);
    if (doc == NULL)
	code = -1;

    if (opt.combine[0]) {
	/* open second DCS2 input file */
	doc2 = epstool_open_document(app, &opt, opt.combine);
	if (doc2 == NULL)
	    code = -1;
    }

    if ((code == 0) && opt.bbox) {
	switch (opt.cmd) {
	    case CMD_TIFF4:
	    case CMD_TIFF6U:
	    case CMD_TIFF6P:
	    case CMD_INTERCHANGE:
	    case CMD_WMF:
	    case CMD_COPY:
		if (doc->dsc->dcs2) {
		    debug |= DEBUG_LOG;
		    app_csmsgf(app, TEXT("Ignoring --bbox for DCS 2.0.\n"));
		    opt.bbox = 0;
		}
		/* all others are these OK */
		break;
	    default:
		debug |= DEBUG_LOG;
		app_csmsgf(app, TEXT(
		  "Can't use --bbox with this command.  Ignoring --bbox.\n"));
		opt.bbox = 0;
	}
    }

    if (code == 0) {
      switch (opt.cmd) {
	case CMD_TIFF4:
	case CMD_TIFF6U:
	case CMD_TIFF6P:
	case CMD_TIFF:
	case CMD_INTERCHANGE:
	case CMD_WMF:
	case CMD_PICT:
    	case CMD_USER:
	    code = epstool_add_preview(doc, &opt);
	    break;
	case CMD_DCS2_SINGLE:
	case CMD_DCS2_MULTI:
	    if (doc->dsc->dcs2)
		code = epstool_dcs2_check_files(doc, &opt);
	    if (code == 0)
		code = epstool_dcs2_copy(doc, doc2, &opt); 
	    break;
	case CMD_DCS2_REPORT:
	    code = epstool_dcs2_report(doc); 
	    break;
	case CMD_PREVIEW:
	case CMD_POSTSCRIPT:
	    code = epstool_extract(doc, &opt);
	    break;
	case CMD_BITMAP:
	    code = epstool_bitmap(doc, &opt);
	    break;
	case CMD_COPY:
	    code = epstool_copy(doc, &opt);
	    break;
	case CMD_TEST:
	    code = epstool_test(doc, &opt);
	    break;
	case CMD_DUMP:
	    if (doc && doc->dsc)
		dsc_display(doc->dsc, epstool_dump_fn);
	    break;
 	default:
	case CMD_UNKNOWN:
	case CMD_HELP:
	    /* should reach here */
	    code = -1;
	    break;
      }
    }


    if (doc) {
	doc_close(doc);
	doc_remove(doc);	/* detach doc from app */
	doc_unref(doc);
    }

    app_unref(app);

#ifdef DEBUG_MALLOC
    debug_memory_report();
#endif

    debug &= ~DEBUG_MEM;
    while (opt.rename_sep) {
	RENAME_SEPARATION *rs = opt.rename_sep;
	if (rs->oldname)
	    free((void *)rs->oldname);
	if (rs->newname)
	    free((void *)rs->newname);
	opt.rename_sep = rs->next;
	free(rs);
    }

    if (!opt.quiet)
        fprintf(MSGOUT, "%s\n", code == 0 ? "OK" : "Failed");
    if (code != 0)
	return 1;
    return code;
}

/****************************************************************/

static int
epstool_add_preview(Doc *doc, OPT *opt)
{
    int code = 0;
    CDSCBBOX devbbox = {0,0,595,842};
    CDSCBBOX bbox = {0, 0, 0, 0};
    CDSCFBBOX hires_bbox = {0.0, 0.0, 0.0, 0.0};
    CDSCFBBOX *phires_bbox = NULL;
    const TCHAR *device = COLOUR_DEVICE;
    FILE *f;
    TCHAR preview[MAXSTR];
    IMAGE *img = NULL;
    if (opt->device[0] != '\0')
	device = opt->device;
    else if (opt->cmd == CMD_INTERCHANGE)
	device = MONO_DEVICE;
    if (opt->cmd == CMD_TIFF4)
	device = MONO_DEVICE;

    if (doc->dsc->bbox) {
	bbox = *doc->dsc->bbox;
    }
    else
	opt->bbox = 1;

    if (doc->dsc->hires_bbox) {
	hires_bbox = *doc->dsc->hires_bbox;
	phires_bbox = &hires_bbox;
    }

    if (opt->cmd == CMD_USER) {
	/* Attempt to load BMP or PPM */
	/* If these fail, assume it is TIFF or WMF */
	img = bmpfile_to_image(opt->user_preview);
	if (img == NULL)
	    img = pnmfile_to_image(opt->user_preview);
    }
    else if (opt->cmd == CMD_TIFF) {
	/* We'll use ghostscript to make the image. */
    }
    else {
	img = make_preview_image(doc, opt, 0, device, 
	    &bbox, &hires_bbox, opt->bbox);
	if (img == NULL) {
	    app_csmsgf(doc->app, TEXT("Couldn't make preview image\n"));
	    return -1;
	}
	if ((hires_bbox.fllx < hires_bbox.furx) &&
	    (hires_bbox.flly < hires_bbox.fury))
	    phires_bbox = &hires_bbox;
	
    }
    if (img) {
	devbbox.llx = devbbox.lly = 0;
	devbbox.urx = img->width;
	devbbox.ury = img->height;
    }

    /* add preview to EPS file */

    switch (opt->cmd) {
	case CMD_TIFF4:
	    code = make_eps_tiff(doc, img, devbbox, &bbox, phires_bbox, 
		opt->dpi, opt->dpi, TRUE, FALSE, opt->doseps_reverse,
		opt->output);
	    break;
	case CMD_TIFF6U:
	    code = make_eps_tiff(doc, img, devbbox, &bbox, phires_bbox, 
		opt->dpi, opt->dpi, FALSE, FALSE, opt->doseps_reverse,
		opt->output);
	    break;
	case CMD_TIFF6P:
	    code = make_eps_tiff(doc, img, devbbox, &bbox, phires_bbox, 
		opt->dpi, opt->dpi, FALSE, TRUE, opt->doseps_reverse,
		opt->output);
	    break;
	case CMD_TIFF:
	    /* create preview file */
	    preview[0] = '\0';
	    if ((f = app_temp_file(doc->app, preview, 
		sizeof(preview)/sizeof(TCHAR))) == (FILE *)NULL) {
		app_csmsgf(doc->app, 
		    TEXT("Can't create temporary tiff file \042%s\042\n"),
		    preview);
		code = -1;
	    }
	    else {
		fclose(f);
		if (code == 0)
		    code = make_preview_file(doc, opt, 0, preview, 
			device, opt->dpi, &bbox, &hires_bbox, opt->bbox);
		if (code == 0)
		    code = make_eps_user(doc, preview, opt->doseps_reverse,
			opt->output);
		if (!(debug & DEBUG_GENERAL))
		    csunlink(preview);
	    }
	    break;
	case CMD_INTERCHANGE:
	    code = make_eps_interchange(doc, img, devbbox, 
		&bbox, phires_bbox, opt->output);
	    break;
	case CMD_WMF:
	    code = make_eps_metafile(doc, img, devbbox, &bbox, phires_bbox, 
		opt->dpi, opt->dpi, opt->doseps_reverse, opt->output);
	    break;
	case CMD_PICT:
	    code = make_eps_pict(doc, img, &bbox, phires_bbox, 
		opt->dpi, opt->dpi, opt->mac_type, opt->output);
	    break;
    	case CMD_USER:
	    if (img)
		code = make_eps_tiff(doc, img, devbbox, &bbox, phires_bbox,
		    opt->dpi, opt->dpi, FALSE, TRUE, opt->doseps_reverse, 
		    opt->output);
	    else
	        code = make_eps_user(doc, opt->user_preview, 
		    opt->doseps_reverse, opt->output);
	    break;
	default:
	    return 0;
    }

    if (img)
	bitmap_image_free(img);
    return code;
}

/****************************************************************/

static int
epstool_extract(Doc *doc, OPT *opt)
{
    int code;
    if ((doc->dsc != (CDSC *)NULL) && 
	(doc->dsc->macbin != (CDSCMACBIN *)NULL))
	code = extract_macbin(doc, opt->output, opt->cmd == CMD_PREVIEW);
    else
        code = extract_doseps(doc, opt->output, opt->cmd == CMD_PREVIEW);
    return code;
}

/****************************************************************/

static int 
epstool_bitmap(Doc *doc, OPT *opt)
{
    int code = 0;
    CDSCBBOX bbox;
    CDSCFBBOX hires_bbox;
    LPCTSTR device = COLOUR_DEVICE;
    int page = opt->page;
    if (opt->device[0] != '\0')
	device = opt->device;

    if (doc->dsc->bbox)
	bbox = *doc->dsc->bbox;
    else {
	bbox.llx = bbox.lly = bbox.urx = bbox.ury = 0;
	opt->bbox = 1;
    }

    if (doc->dsc->hires_bbox)
	hires_bbox = *doc->dsc->hires_bbox;
    else {
	hires_bbox.fllx = (float)bbox.llx;
	hires_bbox.flly = (float)bbox.lly;
	hires_bbox.furx = (float)bbox.urx;
	hires_bbox.fury = (float)bbox.ury;
    }

    if (doc->dsc->page_count == 0)
	page = 0;
    else if ((opt->page < 0) || (opt->page >= (int)doc->dsc->page_count))
	return -1;	/* invalid page */

    code = make_preview_file(doc, opt, page, opt->output,
	device, opt->dpi, &bbox, &hires_bbox, opt->bbox);

    return code;
}

/****************************************************************/

static int 
epstool_copy(Doc *doc, OPT *opt)
{
    int code = 0;
 
    /*
     * "epstool --copy" can also convert a BMP, PBM or PNG to EPS
     * for testing the DCS 2.0 composite image writing code.
     * This isn't in the user documentation.
     */
    if (doc->doctype == DOC_BITMAP)
	return epstool_copy_bitmap(doc, opt);

    /* Copy an EPS to another EPS */
    if (opt->bbox) {
	CDSCBBOX bbox;
	CDSCFBBOX hires_bbox;
	GFile *f;
	TCHAR tpsname[MAXSTR];

	memset(tpsname, 0, sizeof(tpsname));
	memset(&bbox, 0, sizeof(bbox));
	memset(&hires_bbox, 0, sizeof(hires_bbox));
	if (doc->dsc->bbox)
	    memcpy(&bbox, &doc->dsc->bbox, sizeof(bbox)); 
	if (doc->dsc->hires_bbox)
	    memcpy(&hires_bbox, &doc->dsc->hires_bbox, sizeof(hires_bbox)); 

	/* Copy page to temporary file */
	if ((f = app_temp_gfile(doc->app, tpsname, 
	    sizeof(tpsname)/sizeof(TCHAR))) == (GFile *)NULL) {
	    app_csmsgf(doc->app, 
		TEXT("Can't create temporary ps file \042%s\042\n"),
		tpsname);
	    return -1;
	}
	code = copy_page_temp(doc, f, 0);
	gfile_close(f);
	if (code != 0) {
	    if (!(debug & DEBUG_GENERAL))
		csunlink(tpsname);
	    return -1;
	}
	code = calculate_bbox(doc, opt, tpsname, &bbox, &hires_bbox);

	if (code == 0)
	    code = copy_eps(doc, opt->output, &bbox, &hires_bbox, 0, FALSE); 

	/* delete temporary ps file */
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tpsname);
    }
    else {
	code = copy_eps(doc, opt->output, doc->dsc->bbox, 
		doc->dsc->hires_bbox, 0, FALSE); 
    }

    return code;
}

/* Save a BMP, PBM or PNG as an EPS file */
static int 
epstool_copy_bitmap(Doc *doc, OPT *opt)
{
    int code = 0;
    IMAGE *img;
    img = bmpfile_to_image(doc->name);
    if (img == NULL)
	img = pnmfile_to_image(doc->name);
    if (img == NULL)
	img = pngfile_to_image(doc->name);
    if (img == NULL) 
	code = -1;

    if (code == 0) {
	GFile *f = NULL;
	double width = (img->width * 72.0 / opt->dpi);
	double height = (img->height * 72.0 / opt->dpi);
	
	if ((img == NULL) || (img->image == NULL))
	    code = -1;
	
	if (code == 0) {
	    f = gfile_open(opt->output, gfile_modeWrite | gfile_modeCreate);
	    if (f == NULL) {
		code = -1;
		app_msgf(doc->app, 
		    "Failed to open \042%s\042 for writing\n", opt->output);
	    }
	}

	if (code == 0)
	    code = image_to_eps(f, img, 0, 0, 
	        (int)(width + 0.999), (int)(height + 0.999), 
		0.0, 0.0, (float)width, (float)height,
		opt->image_encode, opt->image_compress);
	if (f)
	    gfile_close(f);
     }
     return code;
}

/****************************************************************/

/* If multi is TRUE, write DCS 2.0 multiple file, otherwise
 * write the single file version.
 */
static int 
epstool_dcs2_copy(Doc *doc, Doc *doc2, OPT *opt)
{
    GFile *infile = NULL;
    GFile *doc2file = NULL;
    GFile *outfile = NULL;
    DSC_OFFSET complen = 0;
    int code = 0;
    GFile *compfile = NULL;
    TCHAR compname[MAXSTR];
    TCHAR temp_outname[MAXSTR];

    if (doc->dsc->dcs2 == NULL) {
	app_csmsgf(doc->app, TEXT("Input file is not DCS 2.0\n"));
	return -1;
    }
    memset(compname, 0, sizeof(compname));
    if (opt->composite) {
	/* Replace composite (first page) with a raster generated
	 * from the separations.
	 */
	if ((compfile = app_temp_gfile(doc->app, compname, 
	    sizeof(compname)/sizeof(TCHAR))) == (GFile *)NULL) {
	    app_csmsgf(doc->app, 
		TEXT("Can't create temporary composite EPS file \042%s\042\n"),
		compname);
	    return -1;
	}
	custom_colours_read(opt);
	code = epstool_dcs2_composite(doc, opt, compfile);
	custom_colours_free(opt);
	gfile_close(compfile);
	if (code == 0) {
	    compfile = gfile_open(compname, gfile_modeRead);
	    if (compfile == (GFile *)NULL)
		code = -1;
	}
	if (code) {
	    if (!(debug & DEBUG_GENERAL) && compname[0])
		csunlink(compname);
	    return -1;
	}
    }

    /* Let's convert from single file to multi file */
    if (code == 0)
	code = (infile = gfile_open(doc_name(doc), gfile_modeRead)) 
	    == (GFile *)NULL;
    if ((code == 0) && doc2)
	code = (doc2file = gfile_open(doc_name(doc2), gfile_modeRead)) 
	    == (GFile *)NULL;
    if (opt->cmd == CMD_DCS2_SINGLE) {
	if (code == 0)  /* write first pass to temporary file, not opt->output */
	    code = (outfile = app_temp_gfile(doc->app, temp_outname, 
		sizeof(temp_outname)/sizeof(TCHAR))) == (GFile *)NULL;
	if (code == 0)
	    code = copy_dcs2(doc, infile, doc2, doc2file,
		outfile, temp_outname,
		0 /* offset */, 
		FALSE /* multi */,
		FALSE /* write all */, 
		opt->missing_separations,
		&complen, compfile, opt->rename_sep, opt->tolerance);
	gfile_seek(infile, 0, gfile_begin);
	if (compfile)
	    gfile_seek(compfile, 0, gfile_begin);
	gfile_close(outfile);
	outfile = NULL;
        if (!(debug & DEBUG_GENERAL) && temp_outname[0])
	    csunlink(temp_outname);
    }
    if (code == 0) {
	code = (outfile = gfile_open(opt->output, 
	    gfile_modeWrite | gfile_modeCreate)) == (GFile *)NULL;
	if (code)
	    app_msgf(doc->app, 
	        "Failed to open \042%s\042 for writing\n", opt->output);
    }
    if (code == 0)
	code = copy_dcs2(doc, infile, doc2, doc2file,
	    outfile, opt->output,
	    0 /* offset */, 
	    opt->cmd == CMD_DCS2_MULTI,
	    TRUE /* write all */, 
	    opt->missing_separations,
	    &complen, compfile, opt->rename_sep, opt->tolerance);
    if (infile != (GFile *)NULL)
	gfile_close(infile);
    if (doc2file != (GFile *)NULL)
	gfile_close(doc2file);
    if (outfile != (GFile *)NULL)
	gfile_close(outfile);
    if (compfile != (GFile *)NULL)
	gfile_close(compfile);
    if (!(debug & DEBUG_GENERAL) && compname[0])
	csunlink(compname);

    return code;
}

static int 
epstool_dcs2_report(Doc *doc)
{
    int i;
    int code = 0;
    CDSC *dsc = doc->dsc;
    const char *type = "Unknown";
    const char *preview = "Unknown";

    if (dsc && dsc->dcs2) {
	DSC_OFFSET length = 0;
	GFile *f;
	/* Check for missing composite */
	if ((dsc->page_count == 0) ||
	     (dsc->page[0].begin == dsc->page[0].end)) {
	    app_msgf(doc->app, TEXT("WARNING: Missing composite, so a separation offset is probably wrong.\n"));
	    code = 2;
	}
	/* Check for separations that extend beyond EOF */
	if ((f = gfile_open(doc_name(doc), gfile_modeRead)) != (GFile *)NULL) {
	    length = gfile_get_length(f);
	    gfile_close(f);
	}
	for (i=0; i<(int)dsc->page_count; i++) {
	    if ((dsc->page[i].begin > length) ||
	        (dsc->page[i].end > length)) {
		app_msgf(doc->app, 
		    TEXT("WARNING: separation %s extends beyond EOF\n"),
		    dsc->page[i].label);
		code = 2;
	    }
	}
    }

    /* Document type */
    if (dsc == NULL) {
	if (doc->doctype == DOC_PDF)
	    type = "PDF";
	else
	    type = "Unknown";
    }
    else if (dsc->dcs2) {
	if (dsc->dcs1)
	    type = "DCS1.0";
	else
	    type = "DCS2.0";
    }
    else if (dsc->epsf)
	type = "EPSF";
    else if (dsc->pdf)
	type = "PDF";
    else
	type = "DSC";
    fprintf(stdout, "Type\t%s\n", type);
    if (dsc == NULL)
	return 1;

    /* Preview type */
    switch (dsc->preview) {
	default:
	case CDSC_NOPREVIEW:
	    preview = "None";
	    break;
	case CDSC_EPSI:
	    preview = "Interchange";
	    break;
	case CDSC_TIFF:
	    preview = "TIFF";
	    break;
	case CDSC_WMF:
	    preview = "WMF";
	    break;
	case CDSC_PICT:
	    preview = "PICT";
	    break;
    }
    fprintf(stdout, "Preview\t%s\n", preview);

    if (dsc->bbox)
        fprintf(stdout, "BoundingBox\t%d\t%d\t%d\t%d\n", 
	    dsc->bbox->llx, dsc->bbox->lly,
	    dsc->bbox->urx, dsc->bbox->ury);
    else
        fprintf(stdout, "BoundingBox\n");
    if (dsc->hires_bbox)
        fprintf(stdout, "HiBoundingBox\t%g\t%g\t%g\t%g\n", 
	    dsc->hires_bbox->fllx, dsc->hires_bbox->flly,
	    dsc->hires_bbox->furx, dsc->hires_bbox->fury);
    else
        fprintf(stdout, "HiResBoundingBox\n");


    if (!dsc->dcs2)
	return 1;

    /* Pages */
    for (i=0; i<(int)dsc->page_count; i++) {
	int found = -1;
	const char *name = dsc->page[i].label;
	float cyan, magenta, yellow, black;
	cyan = magenta = yellow = black = 0.0;
	if (name == NULL)
	    name = "Unknown";
	if (strcmp(name, "Composite") != 0)
	    found = colour_to_cmyk(dsc, name, &cyan, &magenta, &yellow, &black);
	if (found == 0)
	    fprintf(stdout, "%s\t%lu\t%g\t%g\t%g\t%g\n", name, 
		(unsigned long)(dsc->page[i].end - dsc->page[i].begin),
		cyan, magenta, yellow, black);
	else
	    fprintf(stdout, "%s\t%lu\n", name, 
		(unsigned long)(dsc->page[i].end - dsc->page[i].begin));
    }
    return code;
}

/* Check that all separations actually exist */
static int 
epstool_dcs2_check_files(Doc *doc, OPT *opt)
{
    int code = 0;
    int i;
    CDSC *dsc = doc->dsc;
    GFile *f;
    const char *fname;
    const char **renamed1 = NULL;
    for (i=1; (i<(int)dsc->page_count); i++) {
	/* First find length of separation */
	fname = dsc_find_platefile(dsc, i);
	if (fname) {
	    TCHAR wfname[MAXSTR];
	    narrow_to_cs(wfname, (int)(sizeof(wfname)/sizeof(TCHAR)-1),
		fname, (int)strlen(fname)+1);
	    if ((f = gfile_open(wfname, gfile_modeRead)) != (GFile *)NULL) {
		gfile_close(f);
	    }
	    else {
		if (!opt->missing_separations)
		    code = -1;
		if (!opt->quiet)
		    app_msgf(doc->app, 
		    "Separation \042%s\042 is missing file \042%s\042\n",
			dsc->page[i].label, fname);
	    }
	}
	else {
	    if (dsc->page[i].end <= dsc->page[i].begin) {
		if (!opt->quiet)
	            app_msgf(doc->app, 
			"Separation \042%s\042 is empty\n",
			dsc->page[i].label);
		if (!opt->missing_separations)
		    code = -1;
	    }
	}
    }
    /* Test if duplicate separations would occur */
    renamed1 = (const char **)malloc(sizeof(const char *) * dsc->page_count);
    if (renamed1 != NULL) {
	if (rename_separations(dsc, opt->rename_sep, renamed1) != 0) {
	    code = -1;
	    if (!opt->quiet)
	      app_msgf(doc->app, "Duplicate separations are not permitted.\n");
	}
	free((void *)renamed1);
    }

    return code;
}

/****************************************************************/
/* Read --custom-colours file etc. */

static int 
custom_colours_read(OPT *opt)
{
    FILE *f;
    char line[MAXSTR];
    CUSTOM_COLOUR colour;
    CUSTOM_COLOUR *pcolour;
    CUSTOM_COLOUR *tail = opt->colours;
    int i;
    char *s;
    int code = 0;
    if (opt->custom_colours[0]) {
	/* read CMYK colours from file */
	f =  csfopen(opt->custom_colours, TEXT("r"));
	if (f == NULL)
	    return -1;
	while (tail && tail->next)
	    tail = tail->next;
	while (fgets(line, sizeof(line), f) != NULL) {
	    memset(&colour, 0, sizeof(colour));
	    i = 0;
	    s = line;
	    if (strncmp(line, "%%CMYKCustomColor:", 18) == 0) {
		s += 18;
		colour.type = CUSTOM_CMYK;
		while (*s && (*s == ' '))
		    s++;
		/* Get the colour values */
		s = strtok(s, " \t\r\n");
		if (s != NULL) {
		    colour.cyan = (float)atof(s);
		    s = strtok(NULL, " \t\r\n");
		}
		if (s != NULL) {
		    colour.magenta = (float)atof(s);
		    s = strtok(NULL, " \t\r\n");
		}
		if (s != NULL) {
		    colour.yellow = (float)atof(s);
		    s = strtok(NULL, " \t\r\n");
		}
		if (s != NULL) {
		    colour.black = (float)atof(s);
		    s = strtok(NULL, "\t\r\n");
		}
	    }
	    else if (strncmp(line, "%%RGBCustomColor:", 17) == 0) {
		s += 17;
		colour.type = CUSTOM_RGB;
		while (*s && (*s == ' '))
		    s++;
		/* Get the colour values */
		s = strtok(s, " \t\r\n");
		if (s != NULL) {
		    colour.red = (float)atof(s);
		    s = strtok(NULL, " \t\r\n");
		}
		if (s != NULL) {
		    colour.green = (float)atof(s);
		    s = strtok(NULL, " \t\r\n");
		}
		if (s != NULL) {
		    colour.blue = (float)atof(s);
		    s = strtok(NULL, "\t\r\n");
		}
	    }
	    else {
		s = NULL;
	    }

	    if (s != NULL) {
		/* Get the colour name */
		while (*s && (*s == ' '))
		    s++;
		if (*s == '(') {
		    s++;
		    while (*s && (*s != ')')) {
			if (i < (int)sizeof(colour.name)-1)
			    colour.name[i++] = *s;
			s++;
		    }
		    if (*s == ')')
			s++;
		}
		else {
		    while (*s && (*s != ' ') && (*s != '\t') &&
			(*s != '\r') && (*s != '\n')) {
			if (i < (int)sizeof(colour.name)-1)
			    colour.name[i++] = *s;
			s++;
		    }
		}
		colour.name[i] = '\0';
	    }
	    if (debug & DEBUG_GENERAL) {
		if (s == NULL)
		    fprintf(stdout, "Unrecognised line: %s\n", line);
		else if (colour.type == CUSTOM_CMYK)
		    fprintf(stdout, "CMYK Colour: %g %g %g %g (%s)\n",
			colour.cyan, colour.magenta, colour.yellow, 
		        colour.black, colour.name);
		else if (colour.type == CUSTOM_RGB)
		    fprintf(stdout, "RGB Colour: %g %g %g (%s)\n",
			colour.red, colour.green, colour.blue, 
		        colour.name);
		else 
		    fprintf(stdout, "Unrecognised colour\n");
	    }
	    if (s == NULL) {
		if (code == 0)
		    code = 1;
	    }
	    else {
		pcolour = (CUSTOM_COLOUR *)malloc(sizeof(CUSTOM_COLOUR));
		if (pcolour == NULL)
		    code = -1;
		else {
		    /* append to list */
		    memcpy(pcolour, &colour, sizeof(colour));
		    if (tail) {
			tail->next = pcolour;
			tail = pcolour;
		    }
		    else
			opt->colours = tail = pcolour;
		}
	    }
	}
	fclose(f);
    }
    return code;
}

static void 
custom_colours_free(OPT *opt)
{
    CUSTOM_COLOUR *pcolour;
    while (opt->colours) {
	pcolour = opt->colours;
	opt->colours = opt->colours->next;
	free(pcolour);
    }
}

static CUSTOM_COLOUR *
custom_colours_find(OPT *opt, const char *name)
{
    CUSTOM_COLOUR *pcolour = opt->colours;
    while (pcolour) {
	if (strcmp(name, pcolour->name) == 0)
	    break;
	pcolour = pcolour->next;
    }
    return pcolour;
}

/****************************************************************/
static float
round_float(float f, int n)
{
    return  (float)( ((int)(f * n + 0.5)) / (float)n );
}

/* Calculate the bounding box using the ghostscript bbox device */
static int
calculate_bbox(Doc *doc, OPT *opt, LPCTSTR psname, CDSCBBOX *bbox, CDSCFBBOX *hires_bbox)
{
    FILE *bboxfile;
    TCHAR bboxname[MAXSTR];
    TCHAR command[MAXSTR*8];
    char line[MAXSTR];
    int got_bbox = 0;
    int got_hires_bbox = 0;
    int code = 0;
    int llx, lly, urx, ury;
    float fllx, flly, furx, fury;
    const int pagesize = 9400; /* Must be < 9419 on 7.07, < 150976 on 8.x */
    const int offset = 3000;
    if ((bboxfile = app_temp_file(doc->app, bboxname, 
	sizeof(bboxname)/sizeof(TCHAR))) == (FILE *)NULL) {
	app_csmsgf(doc->app, TEXT("Can't create temporary bbox file \042%s\042\n"),
	    bboxname);
	return -1;
    }
    fclose(bboxfile);

    csnprintf(command, sizeof(command)/sizeof(TCHAR), TEXT(
	"\042%s\042 %s -dNOPAUSE -dBATCH -sDEVICE=bbox %s \
 -c \042<</PageSize [%d %d] /PageOffset [%d %d]>> setpagedevice\042 -f \042%s\042"), 
	opt->gs, opt->quiet ? TEXT("-dQUIET") : TEXT(""), opt->gsargs,
	pagesize, pagesize, offset, offset, psname);
    if (!opt->quiet)
	app_csmsgf(doc->app, TEXT("%s\n"), command);
    code = exec_program(command, -1, fileno(stdout), -1, NULL, NULL, bboxname);
    if (code != 0)
	app_csmsgf(doc->app, 
	    TEXT("Ghostscript failed to obtain bounding box\n"));

    /* Now scan for bounding box info */
    if (code == 0) {
	if ((bboxfile = csfopen(bboxname, TEXT("rb"))) == (FILE *)NULL) {
	    app_csmsgf(doc->app, 
		TEXT("Can't open temporary bbox file \042%s\042\n"),
		bboxname);
	    code = -1;
	}
    }

    if (code == 0) {
	while (fgets(line, sizeof(line), bboxfile) != NULL) {
	    if (strncmp(line, "%%BoundingBox: ", 15) == 0) {
		if (!opt->quiet)
		    app_msgf(doc->app, "%s", line);
		if (sscanf(line+15, "%d %d %d %d", &llx, &lly, &urx, &ury) 
		    == 4) {
		    bbox->llx = llx-offset;
		    bbox->lly = lly-offset;
		    bbox->urx = urx-offset;
		    bbox->ury = ury-offset;
		    got_bbox = 1;
		}
	    }
	    else if (strncmp(line, "%%HiResBoundingBox: ", 20) == 0) {
		if (!opt->quiet)
		    app_msgf(doc->app, "%s", line);
		if (sscanf(line+20, "%f %f %f %f", &fllx, &flly, &furx, &fury) 
		    == 4) {
		    hires_bbox->fllx = round_float(fllx-offset, 1000);
		    hires_bbox->flly = round_float(flly-offset, 1000);
		    hires_bbox->furx = round_float(furx-offset, 1000);
		    hires_bbox->fury = round_float(fury-offset, 1000);
		    got_hires_bbox = 1;
		}
	    }
	    if (got_bbox && got_hires_bbox)
		break;
	}
    }

    fclose(bboxfile);
    if (!(debug & DEBUG_GENERAL))
	csunlink(bboxname);

    if ((code == 0) && (!got_bbox)) {
	app_csmsgf(doc->app, TEXT("Didn't get bounding box\n"));
	code = -1;
    }
    if ((code == 0) && (!got_hires_bbox)) {
	app_csmsgf(doc->app, TEXT("Didn't get hires bounding box\n"));
	code = -1;
    }
    return code;
}

static int
calc_device_size(float dpi, CDSCBBOX *bbox, CDSCFBBOX *hires_bbox,
    int *width, int *height, float *xoffset, float *yoffset)
{
    int code = 0;
    int hires_bbox_valid = 1;
    if ((bbox->llx >= bbox->urx) ||
	(bbox->lly >= bbox->ury)) {
	code = -1;
    }

    if ((hires_bbox->fllx > hires_bbox->furx) ||
	(hires_bbox->flly > hires_bbox->fury)) {
	hires_bbox_valid = 0;
	code = -1;
    }
    if ((hires_bbox->fllx == hires_bbox->furx) ||
	(hires_bbox->flly == hires_bbox->fury)) {
	hires_bbox_valid = 0;
	/* ignore hires_bbox */
    }

    /* Make the preview image */
    if (hires_bbox_valid) {
	*width = (int)((hires_bbox->furx - hires_bbox->fllx)*dpi/72.0+0.5);
	*height = (int)((hires_bbox->fury - hires_bbox->flly)*dpi/72.0+0.5);
	*xoffset = -hires_bbox->fllx;
	*yoffset = -hires_bbox->flly;
    }
    else {
	*width = (int)((bbox->urx - bbox->llx)*dpi/72.0+0.5);
	*height = (int)((bbox->ury - bbox->lly)*dpi/72.0+0.5);
	*xoffset = (float)(-bbox->llx);
	*yoffset = (float)(-bbox->lly);
    }
    return 0;
}


static int
make_preview_file(Doc *doc, OPT *opt, int page, 
    LPCTSTR preview, LPCTSTR device,
    float dpi, CDSCBBOX *bbox, CDSCFBBOX *hires_bbox, int calc_bbox)
{
    GFile *f;
    int code = 0;
    TCHAR tpsname[MAXSTR];
    TCHAR command[MAXSTR*8];
    int width, height;
    float xoffset, yoffset;

    /* Copy page to temporary file */
    if ((f = app_temp_gfile(doc->app, tpsname, 
	sizeof(tpsname)/sizeof(TCHAR))) == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't create temporary ps file \042%s\042\n"),
	    tpsname);
	return -1;
    }
    code = copy_page_temp(doc, f, page);
    gfile_close(f);
    if (code != 0) {
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tpsname);
	return -1;
    }

    /* Get bbox */
    if ((code == 0) && calc_bbox)
	code = calculate_bbox(doc, opt, tpsname, bbox, hires_bbox);
    width = height = 0;
    xoffset = yoffset = 0.0;
    if (code == 0)
        code = calc_device_size(dpi, bbox, hires_bbox, &width, &height,
	    &xoffset, &yoffset);
    if (code) {
	app_csmsgf(doc->app, TEXT("BoundingBox is invalid\n"));
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tpsname);
	return -1;
    }
	
    /* Make the preview image */
    csnprintf(command, sizeof(command)/sizeof(TCHAR),
	TEXT("\042%s\042 %s -dNOPAUSE -dBATCH -sDEVICE=%s -sOutputFile=\042%s\042 -r%g -g%dx%d %s -c %f %f translate -f \042%s\042"), 
	opt->gs, opt->quiet ? TEXT("-dQUIET") : TEXT(""), 
	device, (preview[0]=='\0' ? TEXT("-") : preview), dpi, width, height, 
	opt->gsargs, xoffset, yoffset, tpsname);
    if (!opt->quiet)
	app_csmsgf(doc->app, TEXT("%s\n"), command);
    code = exec_program(command, -1, fileno(stdout), fileno(stderr),
	NULL, NULL, NULL);
    if (code != 0)
	app_csmsgf(doc->app, 
	    TEXT("Ghostscript failed to create preview image\n"));

    /* delete temporary ps file */
    if (!(debug & DEBUG_GENERAL))
	csunlink(tpsname);

    return code;
}


static IMAGE *
make_preview_image(Doc *doc, OPT *opt, int page, LPCTSTR device,
    CDSCBBOX *bbox, CDSCFBBOX *hires_bbox, int calc_bbox)
{
    IMAGE *img = NULL;
    IMAGE *newimg = NULL;
    TCHAR preview[MAXSTR];
    GFile *f;
    int code = 0;

    /* Create a temporary file for ghostscript bitmap output */
    if ((f = app_temp_gfile(doc->app, preview, 
	sizeof(preview)/sizeof(TCHAR))) == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't create temporary bitmap file \042%s\042\n"),
	    preview);
	code = -1;
    }
    else
	gfile_close(f);

    if (code == 0)
	code = make_preview_file(doc, opt, page, 
	    preview, device, opt->dpi_render, bbox, hires_bbox, calc_bbox);

    if (code == 0) {
	/* Load image */
	img = bmpfile_to_image(preview);
	if (img == NULL)
	    img = pnmfile_to_image(preview);
    }

    if (img && (opt->dpi_render != opt->dpi)) {
	/* downscale it */
	newimg = (IMAGE *)malloc(sizeof(IMAGE));
	if (newimg != NULL) {
	    int ncomp;
	    int width, height;
	    float xoffset, yoffset;
	    memset(newimg, 0, sizeof(newimg));
	    calc_device_size(opt->dpi, bbox, hires_bbox, &width, &height,
		 &xoffset, &yoffset);
	    newimg->width = width;
	    newimg->height = height;
	    newimg->format = img->format;
	    if ((newimg->format & DISPLAY_COLORS_MASK) 
		== DISPLAY_COLORS_CMYK)
		ncomp = 4;
	    else if ((newimg->format & DISPLAY_COLORS_MASK) 
		== DISPLAY_COLORS_RGB)
		ncomp = 3;
	    else
		ncomp = 1;
	    newimg->raster = newimg->width * ncomp;	/* bytes per row */
	    newimg->image = malloc(newimg->raster * newimg->height);
	    if (newimg->image == NULL) {
		free(newimg);
		newimg = NULL;
	    }
	}
	if (newimg != NULL) {
	    memset(newimg->image, 0, newimg->raster * newimg->height);
	    if (image_down_scale(newimg, img) != 0) {
		bitmap_image_free(newimg);
		newimg = NULL;
	    }
	}
	if (newimg != NULL) {
	    bitmap_image_free(img);
	    img = newimg;
	}
    }
	
    if (!(debug & DEBUG_GENERAL))
	csunlink(preview);
    return img;
}


/****************************************************************/
int
epstool_dcs2_composite(Doc *doc, OPT *opt, GFile *compfile)
{
    CDSC *dsc = doc->dsc;
    IMAGE img;
    IMAGE *layer = NULL;
    int width, height;
    float xoffset, yoffset;
    float cyan, magenta, yellow, black;
    int code = 0;
    int i;
    CDSCBBOX bbox = {0, 0, 0, 0};
    CDSCFBBOX hires_bbox = {0.0, 0.0, 0.0, 0.0};
    int hires_bbox_valid = 0;

    /* Require original to be DCS 2.0 */ 
    if (doc->dsc == NULL)
	return -1;
    if (doc->dsc->dcs2 == NULL)
	return -1;

    if (dsc->bbox) {
	bbox = *dsc->bbox;
    }
    else
	return -1;
    if (dsc->hires_bbox) {
	hires_bbox = *dsc->hires_bbox;
	hires_bbox_valid = 1;
    }

    /* Make a CMYK image for the composite */
    code = calc_device_size(opt->dpi, &bbox, &hires_bbox, &width, &height,
	    &xoffset, &yoffset);
    if (code) {
	app_csmsgf(doc->app, TEXT("BoundingBox is invalid\n"));
	return -1;
    }
    memset(&img, 0, sizeof(img));
    img.width = width;
    img.height = height;
    img.raster = img.width * 4;
    img.format = DISPLAY_COLORS_CMYK | DISPLAY_ALPHA_NONE |
       DISPLAY_DEPTH_8 | DISPLAY_BIGENDIAN | DISPLAY_TOPFIRST;
    img.image = (unsigned char *)malloc(img.raster * img.height);
    if (img.image == NULL)
	return -1;
    memset(img.image, 0, img.raster * img.height);
 
    /* For each plate, make an image, then merge it into
     * the composite
     */
    for (i=1; i<(int)dsc->page_count; i++) {
	/* find colour */
	int found;
	CUSTOM_COLOUR *ccolour = NULL;
	const char *name = dsc->page[i].label;
	if (name == NULL) {
	    app_msgf(doc->app, "Page %d doesn't have a label\n", i);
	    code = -1;
	    break;
	}
	found = colour_to_cmyk(dsc, name, &cyan, &magenta, &yellow, &black);
	if ((ccolour = custom_colours_find(opt, name)) != NULL) {
	    /* Use local colour overrides from --custom-colours */
	    found = 0;
	    if (ccolour->type == CUSTOM_RGB) {
		cyan = (float)(1.0 - ccolour->red);
		magenta = (float)(1.0 - ccolour->green);
		yellow = (float)(1.0 - ccolour->blue);
		black = min(cyan, min(magenta, yellow));
		if (black > 0.0) {
		    cyan -= black;
		    magenta -= black;
		    yellow -= black;
		}
	    }
	    else {
		cyan = ccolour->cyan;
		magenta = ccolour->magenta;
		yellow = ccolour->yellow;
		black = ccolour->black;
	    }
	}
	if (found < 0) {
	    app_msgf(doc->app, "Unrecognised colour (%s)\n", name);
	    code = -1;
	    break;
	}

	if ((cyan == 0.0) && (magenta == 0.0) && (yellow == 0.0) && 
	    (black == 0.0)) {
	    if (!opt->quiet)
		app_msgf(doc->app, "Skipping blank separation %s\n", name);
	    continue;
	}
	if (!opt->quiet)
	    app_msgf(doc->app, "Creating image from separation %s\n", name);
	/* Now get the separation image */
        layer = make_preview_image(doc, opt, i, GREY_DEVICE,
    		&bbox, &hires_bbox, FALSE);
	if (layer == NULL) {
	    app_msgf(doc->app, "Failed to make image for separation (%s)\n",
		name);
	    code = -1;
	    break;
	}
	else {
	    if (!opt->quiet)
		app_msgf(doc->app, "Merging separation %g %g %g %g  %s\n", 
		    cyan, magenta, yellow, black, name);
	    code = image_merge_cmyk(&img, layer, cyan, magenta, yellow, black);
	    bitmap_image_free(layer);
	    if (code < 0) {
	        app_msgf(doc->app, "Failed to merge separation (%s)\n", name);
		code = -1;
		break;
	    }
	}
	
    } 


    if (code == 0) {
	if (!opt->quiet)
	    app_msgf(doc->app, "Writing composite as EPS\n");
	code = image_to_eps(compfile, &img, 
		bbox.llx, bbox.lly, bbox.urx, bbox.ury, 
		hires_bbox.fllx, hires_bbox.flly, 
		hires_bbox.furx, hires_bbox.fury, 
		opt->image_encode, 
		opt->image_compress);
    }

    free(img.image);
    return code;
}

/* Return 0 if CMYK set from the DSC comments, -1 if not set */
static int
colour_to_cmyk(CDSC *dsc, const char *name, 
   float *cyan, float *magenta, float *yellow, float *black)
{
    int code = 0;
    CDSCCOLOUR *colour = dsc->colours;
    if (name == NULL)
	return -1;
    while (colour) {
	if (colour->name && (dsc_stricmp(colour->name, name)==0))
	    break;
	colour = colour->next;
    }

    if (colour && (colour->custom == CDSC_CUSTOM_COLOUR_CMYK)) {
	*cyan = colour->cyan;
	*magenta = colour->magenta;
	*yellow = colour->yellow;
	*black = colour->black;
    }
    else if (colour && (colour->custom == CDSC_CUSTOM_COLOUR_RGB)) {
	*cyan = (float)(1.0 - colour->red);
	*magenta = (float)(1.0 - colour->green);
	*yellow = (float)(1.0 - colour->blue);
	*black = min(*cyan, min(*magenta, *yellow));
	if (*black > 0.0) {
	    *cyan -= *black;
	    *magenta -= *black;
	    *yellow -= *black;
	}
    }
    else {
	if (dsc_stricmp(name, "Cyan") == 0) {
	    *cyan = 1.0;
	    *magenta = *yellow = *black = 0.0;
	}
	else if (dsc_stricmp(name, "Magenta") == 0) {
	    *magenta = 1.0;
	    *cyan = *yellow = *black = 0.0;
	}
	else if (dsc_stricmp(name, "Yellow") == 0) {
	    *yellow = 1.0;
	    *cyan = *magenta = *black = 0.0;
	}
	else if (dsc_stricmp(name, "Black") == 0) {
	    *black = 1.0;
	    *cyan = *yellow = *magenta = 0.0;
	}
	else {
	    code = -1;
	}
    }
    return code;
}

const char *epswarn_prolog[] = {
"%!\n",
"% This code is wrapped around an EPS file to partly test if it complies\n",
"% with the EPS specfication.\n",
"\n",
"/eps_warn_file (%stdout) (w) file def\n",
"\n",
"globaldict begin /eps_warn_ok true def end\n",
"\n",
"/eps_write_only { % name string -- name\n",
"  eps_warn_file exch writestring\n",
"  eps_warn_file ( /) writestring\n",
"  dup eps_warn_file exch 32 string cvs writestring\n",
"  eps_warn_file (\\n) writestring \n",
"  eps_warn_file flushfile\n",
"} bind def\n",
"\n",
"/eps_write { % name string -- name\n",
"  eps_write_only\n",
"  //globaldict /eps_warn_ok false put\n",
"} bind def\n",
"\n",
"/eps_warn { % name --\n",
"  (EPSWARN FAIL: EPS file must not use) \n",
"  eps_write\n",
"  //systemdict exch get exec\n",
"} bind def\n",
"\n",
"% Prohibited operators in systemdict\n",
"/banddevice {/banddevice eps_warn} def\n",
"/clear {/clear eps_warn} def\n",
"/cleardictstack {/cleardictstack eps_warn} def\n",
"/copypage {/copypage eps_warn} def\n",
"/erasepage {/erasepage eps_warn} def\n",
"/exitserver {/exitserver eps_warn} def % this won't work - exitserver is in serverdict\n",
"/serverdict {/serverdict eps_warn} def % so use this to provide warnings instead\n",
"/statusdict {/statusdict eps_warn} def\n",
"/framedevice {/framedevice eps_warn} def\n",
"/grestoreall {/grestoreall eps_warn} def\n",
"/initclip {/initclip eps_warn} def\n",
"/initgraphics {/initgraphics eps_warn} def\n",
"/initmatrix {/initmatrix eps_warn} def\n",
"/renderbands {/renderbands eps_warn} def\n",
"/setglobal {/setglobal eps_warn} def\n",
"/setpagedevice {/setpagedevice eps_warn} def\n",
"/setpageparams {/setpageparams eps_warn} def\n",
"/setshared {/setshared eps_warn} def\n",
"/startjob {/startjob eps_warn} def\n",
"% If quit is executed, then epswarn_check will never be run.\n",
"/quit {\n",
"  % systemdict /quit doesn't work when it has been redefined in userdict\n",
"  /quit (EPSWARN FAIL: EPS file must not use) eps_write\n",
"  //globaldict /eps_warn_ok false put\n",
"  //systemdict begin quit \n",
"} def\n",
"\n",
"% These page sizes are defined in userdict, not systemdict\n",
"/eps_pagesize_warn {\n",
"  (EPSWARN FAIL: EPS file must not set page size:)\n",
"  eps_write pop\n",
"} def\n",
"/11x17 {/11x17 eps_pagesize_warn} def\n",
"/a3 {/a3 eps_pagesize_warn} def\n",
"/a4 {/a4 eps_pagesize_warn} def\n",
"/a4small {/a4small eps_pagesize_warn} def\n",
"/a5 {/a5 eps_pagesize_warn} def\n",
"/ledger {/ledger eps_pagesize_warn} def\n",
"/legal {/legal eps_pagesize_warn} def\n",
"/letter {/letter eps_pagesize_warn} def\n",
"/lettersmall {/lettersmall eps_pagesize_warn} def\n",
"/note {/note eps_pagesize_warn} def\n",
"\n",
"% These operators can only be used if the parameter\n",
"% is saved and restored afterwards, or if setting\n",
"% them takes into account their previous value.\n",
"% For example 'matrix setmatrix' is not permitted,\n",
"% but 'matrix current matrix setmatrix' is allowed.\n",
"/eps_warntwo {\n",
"  (EPSWARN WARNING: EPS file should be careful using) \n",
"  eps_write_only\n",
"  //systemdict exch get exec\n",
"} def\n",
"/nulldevice {/nulldevice eps_warntwo} def % can't test this\n",
"/setgstate {/setgstate eps_warntwo} def % can't test this\n",
"/sethalftone {/sethalftone eps_warntwo} def\n",
"/setmatrix {/setmatrix eps_warntwo} def\n",
"/setscreen {/setscreen eps_warntwo} def\n",
"/settransfer {/settransfer eps_warntwo} def\n",
"/setcolortransfer {/setcolortransfer eps_warntwo} def\n",
"\n",
"% Take snapshot of some items\n",
"count /eps_count exch def\n",
"countdictstack /eps_countdictstack exch def\n",
"currentpagedevice /PageCount get /eps_pagecount exch def\n",
"/eps_showpage_count 0 def\n",
"\n",
"\n",
"/epswarn_check_write { % string --\n",
"  eps_warn_file exch writestring\n",
"  eps_warn_file flushfile\n",
"  //globaldict /eps_warn_ok false put\n",
"} def\n",
"\n",
"/epswarn_check {\n",
"  % count\n",
"  count eps_count ne {\n",
"    (EPSWARN FAIL: EPS file altered operand stack count\\n) epswarn_check_write \n",
"  } if\n",
"  //systemdict /clear get exec\n",
"  % countdictstack\n",
"  countdictstack eps_countdictstack ne {\n",
"   (EPSWARN FAIL: EPS file altered dictionary stack count\\n) \n",
"   epswarn_check_write\n",
"  } if\n",
"  countdictstack eps_countdictstack sub {end} repeat\n",
"  % real page count\n",
"  currentpagedevice /PageCount get eps_pagecount ne {\n",
"    (EPSWARN FAIL: EPS file forcibly output a page\\n) epswarn_check_write\n",
"  } if\n",
"  % showpage count\n",
"  eps_showpage_count 1 gt {\n",
"    (EPSWARN FAIL: EPS file used showpage more than once\\n) epswarn_check_write\n",
"  } if\n",
"} def\n",
"\n",
"% EPS files are normally encapsulated inside a save/restore\n",
"save /epswarn_save exch def\n",
"\n",
"% redefine showpage, and count how many times it is called\n",
"/showpage { userdict dup /eps_showpage_count get 1 add \n",
"  /eps_showpage_count exch put \n",
"} def\n",
"\n",
"% Now for something to test this\n",
"\n",
NULL};

const char epswarn_epilog[] = "\
%!\n\
% Check that all is well\n\
epswarn_check\n\
%epswarn_save restore\n\
//globaldict /eps_warn_ok get not \n\
 {eps_warn_file (\nEPSWARN FAIL\\n) writestring} \n\
 {eps_warn_file (\nEPSWARN PASS\\n) writestring} \n\
 ifelse\n\
systemdict begin % so quit works...\n\
";


/* Test if an EPS file really is EPS compliant */
static int
epstool_test(Doc *doc, OPT *opt)
{
    GFile *f;
    int code = 0;
    TCHAR tpsname[MAXSTR];
    TCHAR command[MAXSTR*8];
    unsigned int len;
    FILE *testfile = NULL;
    TCHAR testname[MAXSTR];
    CDSCBBOX bbox;
    CDSCFBBOX hires_bbox;
    BOOL found_error = FALSE;
    BOOL found_warning = FALSE;
    BOOL found_pass = FALSE;
    BOOL bbox_valid = FALSE;
    char line[256];
    int i;
    BOOL dsc_error = FALSE;
    BOOL dsc_warning = FALSE;

    if (doc->dsc) {
	dsc_error = (doc->dsc->worst_error ==  CDSC_ERROR_ERROR);
	dsc_warning = (doc->dsc->worst_error ==  CDSC_ERROR_WARN);
    }

    /* Copy prolog, page and epilog to a temporary file */
    if ((f = app_temp_gfile(doc->app, tpsname, 
	sizeof(tpsname)/sizeof(TCHAR))) == (GFile *)NULL) {
	app_csmsgf(doc->app, 
	    TEXT("Can't create temporary ps file \042%s\042\n"),
	    tpsname);
	return -1;
    }
    for (i=0; epswarn_prolog[i]; i++) {
        len = (int)strlen(epswarn_prolog[i]);
        if (!code && (len != gfile_write(f, epswarn_prolog[i], len)))
	    code = -1;
    }
    code = copy_page_nosave(doc, f, 0);
    len = (int)strlen(epswarn_epilog);
    if (len != gfile_write(f, epswarn_epilog, len))
	code = -1;
    gfile_close(f);
    if (code != 0) {
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tpsname);
	return -1;
    }

    if ((testfile = app_temp_file(doc->app, testname, 
	sizeof(testname)/sizeof(TCHAR))) == (FILE *)NULL) {
	app_csmsgf(doc->app, TEXT("Can't create temporary file \042%s\042\n"),
	    testname);
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tpsname);
	return -1;
    }
    fclose(testfile);
    testfile = NULL;

    /* Interpret the file to test it */
    csnprintf(command, sizeof(command)/sizeof(TCHAR),
	TEXT("\042%s\042 %s -dNOEPS -dNOPAUSE -dBATCH -dNODISPLAY %s \042%s\042"), 
	opt->gs, opt->quiet ? TEXT("-dQUIET") : TEXT(""), 
	opt->gsargs, tpsname);
    if (!opt->quiet)
	app_csmsgf(doc->app, TEXT("%s\n"), command);
    code = exec_program(command, -1, -1 , fileno(stderr),
	NULL, testname, NULL);
    if (code != 0)
	app_csmsgf(doc->app, 
	    TEXT("Ghostscript failed to interpret file\n"));
    /* delete temporary ps file */
    if (!(debug & DEBUG_GENERAL))
	csunlink(tpsname);

    /* Now check testfile for reports of problems */
    if (code == 0) {
	if ((testfile = csfopen(testname, TEXT("rb"))) == (FILE *)NULL) {
	    app_csmsgf(doc->app, 
		TEXT("Can't open temporary file \042%s\042\n"),
		testname);
	    code = -1;
	}
    }

    if (code == 0) {
	while (fgets(line, sizeof(line), testfile) != NULL) {
	    if (!opt->quiet)
		app_msgf(doc->app, "%s", line);
	    if (strncmp(line, "EPSWARN FAIL: ", 14) == 0)
		found_error = TRUE;
	    else if (strncmp(line, "EPSWARN WARNING: ", 17) == 0)
		found_warning = TRUE;
	    else if (strncmp(line, "EPSWARN PASS", 12) == 0)
		found_pass = TRUE;
	}
    }
    if (testfile)
        fclose(testfile);
    if (!(debug & DEBUG_GENERAL))
	csunlink(testname);
    if (!opt->quiet)
	app_csmsg(doc->app, TEXT("\n"));

    if ((code == 0) && (doc->dsc) && (doc->dsc->bbox)) {
	/* Copy page to temporary file */
	if ((f = app_temp_gfile(doc->app, tpsname, 
	    sizeof(tpsname)/sizeof(TCHAR))) == (GFile *)NULL) {
	    app_csmsgf(doc->app, 
		TEXT("Can't create temporary ps file \042%s\042\n"),
		tpsname);
	    return -1;
	}
	code = copy_page_temp(doc, f, 0);
	gfile_close(f);
	if (code != 0) {
	    if (!(debug & DEBUG_GENERAL))
		csunlink(tpsname);
	    return -1;
	}
	code = calculate_bbox(doc, opt, tpsname, &bbox, &hires_bbox);
        if (!opt->quiet)
	    app_csmsg(doc->app, TEXT("\n"));
	if (code == 0) {
	    if ( (bbox.llx >= doc->dsc->bbox->llx-1) &&
	         (bbox.lly >= doc->dsc->bbox->lly-1) &&
	         (bbox.urx <= doc->dsc->bbox->urx+1) &&
	         (bbox.ury <= doc->dsc->bbox->ury+1)) {
		bbox_valid = TRUE;
	    }
	    if (!opt->quiet) {
	        app_csmsgf(doc->app, 
	            TEXT("File has   %%%%BoundingBox: %d %d %d %d\n"),
		    doc->dsc->bbox->llx, doc->dsc->bbox->lly,
		    doc->dsc->bbox->urx, doc->dsc->bbox->ury);
	        app_csmsgf(doc->app, 
	            TEXT("Correct is %%%%BoundingBox: %d %d %d %d\n"),
		    bbox.llx, bbox.lly, bbox.urx, bbox.ury);
		if (!bbox_valid)
		    app_csmsgf(doc->app, 
			TEXT("File bounding box needs to be larger\n"));
	    }
	}
	else  {
	    if (!opt->quiet)
	       app_csmsgf(doc->app, 
	           TEXT("Failed to calculate bounding box\n"));
	}

	/* delete temporary ps file */
	if (!(debug & DEBUG_GENERAL))
	    csunlink(tpsname);
    }
    else {
	if (!opt->quiet)
	   app_csmsgf(doc->app, 
	       TEXT("Missing %%%%BoundingBox\n"));
    }

    if (found_error || !found_pass || !bbox_valid || 
	dsc_error || dsc_warning ||
	!doc->dsc || !doc->dsc->epsf)
	code = -1;

    if (!opt->quiet) {
	if (found_warning)
	   app_csmsgf(doc->app, TEXT("File used operators that sometimes cause problems in an EPS file.\n"));
        if (found_error)
	   app_csmsgf(doc->app, TEXT("File used PostScript operators that are prohibited in an EPS file.\n"));
	if (found_pass)
	   app_csmsgf(doc->app, TEXT("PostScript appears well behaved.\n"));
        if (doc->dsc && doc->dsc->epsf)
	    app_csmsgf(doc->app, TEXT("File claims to be EPS.\n"));
	else
	    app_csmsgf(doc->app, TEXT("File is not EPS.\n"));
    }

    if (code != 0)
        fprintf(MSGOUT, "FAIL: File does not comply with EPS specification.\n");
    else
	fprintf(MSGOUT, "PASS: File appears to be well behaved EPS.\n");

    return code;
}

static void epstool_dump_fn(void *caller_data, const char *str)
{
    fputs(str, stdout);
}


/****************************************************************/
/* Functions from GSview app that we need. */
/* Some of these should be moved from capp.c to a separate file. */

#ifdef _MSC_VER
# pragma warning(disable:4100) /* ignore "Unreferenced formal parameter" */
#endif

int
app_platform_init(GSview *a)
{
    return 0;
}

int
app_platform_finish(GSview *a)
{
    return 0;
}

int
app_lock(GSview *a)
{
    return 0;
}

int
app_unlock(GSview *a)
{
    return 0;
}

void
app_log(const char *str, int len)
{
    fwrite(str, 1, len, MSGOUT);
}

int
cs_to_narrow(char *nstr, int nlen, LPCTSTR wstr, int wlen)
{
#ifdef UNICODE
    return WideCharToMultiByte(CP_ACP, 0, wstr, wlen, nstr, nlen, NULL, NULL);
#else
    return char_to_narrow(nstr, nlen, wstr, wlen);
#endif
}

int 
narrow_to_cs(TCHAR *wstr, int wlen, const char *nstr, int nlen)
{
#ifdef UNICODE
    return MultiByteToWideChar(CP_ACP, 0, nstr, nlen, wstr, wlen);
#else
    return narrow_to_char(wstr, wlen, nstr, nlen);
#endif
}

int get_dsc_response(GSview *app, LPCTSTR str)
{
    app_csmsgf(app, TEXT("\n%s\n"), str);
    return CDSC_RESPONSE_OK;
}

int 
load_string(GSview *a, int id, TCHAR *buf, int len)
{
    char msg[MAXSTR];
    const char *s = NULL;
    int reslen;
    int dscmsg = (id - CDSC_RESOURCE_BASE) / 2;
    if (buf && len)
	buf[0] = '\0';
    /* CDSC_MESSAGE_INCORRECT_USAGE should be dsc->max_error */
    if (a /* ignore unused parameter GSview *a */
	&& (dscmsg >= 0) && (dscmsg <= CDSC_MESSAGE_INCORRECT_USAGE)) {
	if (((id - CDSC_RESOURCE_BASE) & 1) == 0)
	    s = dsc_message[dscmsg];
	else
	    s = "";
    }
    else 
    switch (id) {
	case IDS_DSC_LINEFMT:
	    s = "%sAt line %d:";
	    break;
	case IDS_DSC_INFO:
	    s = "DSC Information\n";
	    break;
	case IDS_DSC_WARN:
	    s = "DSC Warning\n";
	    break;
	case IDS_DSC_ERROR:
	    s = "DSC Error\n";
	    break;
    }

    if (s == NULL) {
	snprintf(msg, sizeof(msg), "String %d\n", id);
	s = msg;
    }

    reslen = narrow_to_cs(NULL, 0, s, (int)strlen(s)+1);
    if (reslen > len)
	return reslen;
    return narrow_to_cs(buf, len, s, (int)strlen(s)+1);
}

int app_msg_box(GSview *a, LPCTSTR str, int icon)
{
    app_csmsgf(a, TEXT("%s\n"), str);
    return 0;
}

int 
gssrv_request(GSSRV *s, GSREQ *reqnew)
{
    return -1;
}

int 
pagecache_unref_all(GSview *a) 
{
    return 0;
}


void
doc_savestat(Doc *doc)
{
}

int 
image_platform_init(IMAGE *img)
{
    return 0;
}

unsigned int 
image_platform_format(unsigned int format)
{
    return format;
}

#ifdef _MSC_VER
# pragma warning(default:4100)
#endif

/****************************************************************/
/* platform specific code for running another program */

/*
 * If hstdin not -1, duplicate handle and give to program,
 * else if stdin_name not NULL, open filename and give to program.
 * Same for hstdout/stdout_name and hstderr/stderr_name.
 */
#ifdef __WIN32__
int
exec_program(LPTSTR command,
    int hstdin, int hstdout, int hstderr,
    LPCTSTR stdin_name, LPCTSTR stdout_name, LPCTSTR stderr_name)
{
    int code = 0;
    HANDLE hChildStdinRd = INVALID_HANDLE_VALUE;
    HANDLE hChildStdoutWr = INVALID_HANDLE_VALUE;
    HANDLE hChildStderrWr = INVALID_HANDLE_VALUE;
    HANDLE hStdin = INVALID_HANDLE_VALUE;
    HANDLE hStderr = INVALID_HANDLE_VALUE;
    HANDLE hStdout = INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES saAttr;
    STARTUPINFO siStartInfo;
    PROCESS_INFORMATION piProcInfo;
    DWORD exitcode = (DWORD)-1;

    /* Set the bInheritHandle flag so pipe handles are inherited. */
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    /* if handles are provided, use them */
    if ((hstdin != -1)) {
	INTPTR handle;
	handle = _get_osfhandle(hstdin);
	if (handle == -1)
	    code = -1;
	if (code == 0) {
	    hStdin = (HANDLE)handle;
	    if (!DuplicateHandle(GetCurrentProcess(), hStdin,
		GetCurrentProcess(), &hChildStdinRd, 0,
		TRUE,       /* inherited */
		DUPLICATE_SAME_ACCESS))
	        code = -1;
	}
    }
    if ((code==0) && (hstdout != -1)) {
	INTPTR handle;
	handle = _get_osfhandle(hstdout);
	if (handle == -1)
	    code = -1;
	if (code == 0) {
	    hStdout = (HANDLE)handle;
	    if (!DuplicateHandle(GetCurrentProcess(), hStdout,
		GetCurrentProcess(), &hChildStdoutWr, 0,
		TRUE,       /* inherited */
		DUPLICATE_SAME_ACCESS))
	        code = -1;
	}
    }
    if ((code==0) && (hstderr != -1)) {
	INTPTR handle;
	handle = _get_osfhandle(hstderr);
	if (handle == -1)
	    code = -1;
	if (code == 0) {
	    hStderr = (HANDLE)handle;
	    if (!DuplicateHandle(GetCurrentProcess(), hStderr,
		GetCurrentProcess(), &hChildStderrWr, 0,
		TRUE,       /* inherited */
		DUPLICATE_SAME_ACCESS))
	        code = -1;
	}
    }

    /* If files are requested, create them */
    if ((code==0) && stdin_name && (hChildStdinRd == INVALID_HANDLE_VALUE)) {
	hChildStdinRd = CreateFile(stdin_name, GENERIC_READ, 
	    0  /* no file sharing */,
	    &saAttr /* allow handle to be inherited */,
	    OPEN_EXISTING, 0, NULL);
	if (hChildStdinRd == INVALID_HANDLE_VALUE)
	    code = -1;
    }
    if ((code==0) && stdout_name && (hChildStdoutWr == INVALID_HANDLE_VALUE)) {
	hChildStdoutWr = CreateFile(stdout_name, GENERIC_WRITE, 
	    0  /* no file sharing */,
	    &saAttr /* allow handle to be inherited */,
	    OPEN_ALWAYS, 0, NULL);
	if (hChildStdoutWr == INVALID_HANDLE_VALUE)
	    code = -1;
    }
    if ((code==0) && stderr_name && (hChildStderrWr == INVALID_HANDLE_VALUE)) {
	hChildStderrWr = CreateFile(stderr_name, GENERIC_WRITE, 
	    0  /* no file sharing */,
	    &saAttr /* allow handle to be inherited */,
	    OPEN_ALWAYS, 0, NULL);
	if (hChildStderrWr == INVALID_HANDLE_VALUE)
	    code = -1;
    }

    /* Set up members of STARTUPINFO structure. */
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.lpReserved = NULL;
    siStartInfo.lpDesktop = NULL;
    siStartInfo.lpTitle = NULL;  /* use executable name as title */
    /* next two lines ignored */
    siStartInfo.dwX = siStartInfo.dwY = (DWORD)CW_USEDEFAULT;
    siStartInfo.dwXSize = siStartInfo.dwYSize = (DWORD)CW_USEDEFAULT;
    siStartInfo.dwXCountChars = 80;
    siStartInfo.dwYCountChars = 25;
    siStartInfo.dwFillAttribute = 0;			/* ignored */
    siStartInfo.dwFlags = STARTF_USESTDHANDLES;
    siStartInfo.wShowWindow = SW_SHOWNORMAL;		/* ignored */
    siStartInfo.cbReserved2 = 0;
    siStartInfo.lpReserved2 = NULL;
    siStartInfo.hStdInput = hChildStdinRd;
    siStartInfo.hStdOutput = hChildStdoutWr;
    siStartInfo.hStdError = hChildStderrWr;
    memset(&piProcInfo, 0, sizeof(piProcInfo));

    if ((code == 0) && !CreateProcess(NULL,
        command,
        NULL,          /* process security attributes        */
        NULL,          /* primary thread security attributes */
        TRUE,          /* handles are inherited              */
	0,             /* creation flags                     */
	NULL,          /* environment                        */
        NULL,          /* use parent's current directory     */
        &siStartInfo,  /* STARTUPINFO pointer                */
        &piProcInfo)) { /* receives PROCESS_INFORMATION  */
	code = -1;
    }

    /* close our copy of the handles */
    if (hChildStdinRd != INVALID_HANDLE_VALUE)
	CloseHandle(hChildStdinRd);
    if (hChildStdoutWr != INVALID_HANDLE_VALUE)
	CloseHandle(hChildStdoutWr);
    if (hChildStderrWr != INVALID_HANDLE_VALUE)
	CloseHandle(hChildStderrWr);

    if (code == 0) {
	/* wait for process to finish */
	WaitForSingleObject(piProcInfo.hProcess, 300000);
	GetExitCodeProcess(piProcInfo.hProcess, &exitcode);
	CloseHandle(piProcInfo.hProcess);
	CloseHandle(piProcInfo.hThread);
    }

    if (code)
	return code;
    return (int)exitcode;
}
#endif

#if defined(UNIX)
int
exec_program(LPTSTR command,
    int hstdin, int hstdout, int hstderr,
    LPCTSTR stdin_name, LPCTSTR stdout_name, LPCTSTR stderr_name)
{
    int code = 0;
    int hChildStdinRd = -1;
    int hChildStdoutWr = -1;
    int hChildStderrWr = -1;
    int handle;
    pid_t pid;
    int exitcode;
#define MAXARG 64
    char *argv[MAXARG+1];
    int argc = 0;
    char *args, *d, *e, *p;

    /* Parse command line handling quotes. */
    memset(argv, 0, sizeof(argv));
    argc = 0;
    args = (char *)malloc(strlen(command)+1);
    if (args == (char *)NULL)
	return -1;
    p = command;
    d = args;
    while (*p) {
	/* for each argument */
	if (argc >= MAXARG - 1) {
	    fprintf(stderr, "Too many arguments\n");
	    free(args);
	    return -1;
	}

        e = d;
        while ((*p) && (*p != ' ')) {
	    if (*p == '\042') {
		/* Remove quotes, skipping over embedded spaces. */
		/* Doesn't handle embedded quotes. */
		p++;
		while ((*p) && (*p != '\042'))
		    *d++ =*p++;
	    }
	    else 
		*d++ = *p;
	    if (*p)
		p++;
        }
	*d++ = '\0';
	argv[argc++] = e;

	while ((*p) && (*p == ' '))
	    p++;	/* Skip over trailing spaces */
    }
    argv[argc] = NULL;

    pid = fork();
    if (pid == (pid_t)-1) {
	/* fork failed */
	fprintf(stderr, "Failed to fork, error=%d\n", errno);
	return -1;
    }
    else if (pid == 0) {
	/* child */
	/* if handles are provided, use them */
	if ((code == 0) && (hstdin != -1)) {
	    hChildStdinRd = dup2(hstdin, 0);
	    if (hChildStdinRd == -1)
		code = -1;
	}
	if ((code==0) && (hstdout != -1)) {
	    hChildStdoutWr = dup2(hstdout, 1);
	    if (hChildStdoutWr == -1)
		code = -1;
	}
	if ((code==0) && (hstderr != -1)) {
	    hChildStderrWr = dup2(hstderr, 2);
	    if (hChildStderrWr == -1)
		code = -1;
	}
	if ((code==0) && stdin_name && (hChildStdinRd == -1)) {
	    handle = open(stdin_name, O_RDONLY);
	    hChildStdinRd = dup2(handle, 0);
	    if (handle != -1)
		close(handle);
	    if (hChildStdinRd == -1)
		code = -1;
	}
	if ((code==0) && stdout_name && (hChildStdoutWr == -1)) {
	    handle = open(stdout_name, O_WRONLY | O_CREAT, 0666);
	    hChildStdoutWr = dup2(handle, 1);
	    if (handle != -1)
		close(handle);
	    if (hChildStdoutWr == -1)
		code = -1;
	}
	if ((code==0) && stderr_name && (hChildStderrWr == -1)) {
	    handle = open(stderr_name, O_WRONLY | O_CREAT, 0666);
	    hChildStderrWr = dup2(handle, 2);
	    if (handle != -1)
		close(handle);
	    if (hChildStderrWr == -1)
		code = -1;
	}

	if (code) {
	    fprintf(stderr, "Failed to open stdin/out/err, error=%d\n", errno);
	    if (hChildStdinRd)
		close(hChildStdinRd);
	    if (hChildStdoutWr)
		close(hChildStdoutWr);
	    if (hChildStderrWr)
		close(hChildStderrWr);
	    exit(1);
	}

	/* Now execute it */
	if (execvp(argv[0], argv) == -1) {
	    fprintf(stderr, "Failed to execute ghostscript, error=%d\n", errno);
	    exit(1);
	}
    }

    /* parent - wait for child to finish */
    free(args);
    wait(&exitcode);
    return exitcode;
}
#endif

/*
 * If hstdin not -1, duplicate handle and give to program,
 * else if stdin_name not NULL, open filename and give to program.
 * Same for hstdout/stdout_name and hstderr/stderr_name.
 */
#ifdef OS2
int
exec_program(LPTSTR command,
    int hstdin, int hstdout, int hstderr,
    LPCTSTR stdin_name, LPCTSTR stdout_name, LPCTSTR stderr_name)
{
    HFILE hStdin = 0;
    HFILE hStdout = 1;
    HFILE hStderr = 2;
    HFILE hOldStdin;
    HFILE hOldStdout;
    HFILE hOldStderr;
    HFILE hNewStdin = -1;
    HFILE hNewStdout = -1;
    HFILE hNewStderr = -1;
    APIRET rc = 0;
    ULONG action;
    CHAR szFailName[CCHMAXPATH];
    RESULTCODES resc;
    PSZ cmd;
    PSZ args;
    int cmdlen;

    /* Copy command into format needed by DosExecPgm */
    cmdlen = strlen(command)+1;
    cmd = (char *)malloc(cmdlen+1);
    if (cmd) {
	char *p;
	memset(cmd, 0, cmdlen+1);
	strncpy(cmd, command, cmdlen);
	p = args = cmd;
	if (*p == '"') {
	   p++;
	   args = p;
	   while (*p && (*p != '"'))
	     p++;
 	}
	else  {
	   while (*p && (*p != 0))
	     p++;
	}
	*p = '\0';
    }
    else
	return -1;

    /* save existing stdio handles */
    hOldStdin = 0xffffffff; /* allocate new handle */
    if (rc == 0)
	rc = DosDupHandle(hStdin, &hOldStdin);
    hOldStdout = 0xffffffff; /* allocate new handle */
    if (rc == 0)
        rc = DosDupHandle(hStdout, &hOldStdout);
    hOldStderr = 0xffffffff; /* allocate new handle */
    if (rc == 0)
        rc = DosDupHandle(hStderr, &hOldStderr);
    if (rc != 0) {
	free(cmd);
	return -1;
    }

    /* if handles or files are provided, use them */
    if ((hstdin != -1)) {
	if (rc == 0)
	    DosDupHandle((HFILE)hstdin, &hStdin);
    }
    else if ((rc == 0) && stdin_name) {
	rc = DosOpen((PCSZ)stdin_name,	/* filename */
		&hNewStdin,	/* pointer to handle */
		&action,	/* pointer to result */
		0,		/* initial length */
		FILE_NORMAL,	/* normal file */
		OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
		OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE,
		0);
	if (rc == 0)
	    rc= DosDupHandle(hNewStdin, &hStdin);
    }

    if ((hstdout != -1)) {
	if (rc == 0)
	    DosDupHandle((HFILE)hstdout, &hStdout);
    }
    else if ((rc == 0) && stdout_name) {
	rc = DosOpen((PCSZ)stdout_name,	/* filename */
		&hNewStdout,	/* pointer to handle */
		&action,	/* pointer to result */
		0,		/* initial length */
		FILE_NORMAL,	/* normal file */
		OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS,
		OPEN_ACCESS_WRITEONLY | OPEN_SHARE_DENYREADWRITE,
		0);
	if (rc == 0)
	    rc= DosDupHandle(hNewStdout, &hStdout);
    }
    if ((hstderr != -1)) {
	if (rc == 0)
	    DosDupHandle((HFILE)hstderr, &hStderr);
    }
    else if ((rc == 0) && stderr_name) {
	rc = DosOpen((PCSZ)stderr_name,	/* filename */
		&hNewStderr,	/* pointer to handle */
		&action,	/* pointer to result */
		0,		/* initial length */
		FILE_NORMAL,	/* normal file */
		OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS,
		OPEN_ACCESS_WRITEONLY | OPEN_SHARE_DENYWRITE,
		0);
	if (rc == 0)
	    rc= DosDupHandle(hNewStderr, &hStderr);
    }
    if (rc != 0) {
	free(cmd);
	return -1;
    }

    rc = DosExecPgm(szFailName, sizeof(szFailName),
	EXEC_SYNC, args, (PSZ)NULL, &resc, args);

    /* Put stdio back to original handles */
    DosDupHandle(hOldStdin, &hStdin);
    DosDupHandle(hOldStdout, &hStdout);
    DosDupHandle(hOldStderr, &hStderr);
    DosClose(hOldStdin);
    DosClose(hOldStdout);
    DosClose(hOldStderr);
    if (hNewStdin != -1)
        DosClose(hNewStdin);
    if (hNewStdout != -1)
        DosClose(hNewStdout);
    if (hNewStderr != -1)
        DosClose(hNewStderr);

    if (rc)
	return -1;
    return (int)resc.codeResult;
}
#endif
