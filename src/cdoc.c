/* Copyright (C) 2001-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cdoc.c,v 1.19 2005/06/10 08:45:36 ghostgum Exp $ */
/* Common routines for Doc object */

#include "common.h"
#include "dscparse.h"
#include "capp.h"
#define DEFINE_CDOC
#include "cdoc.h"
#include "cres.h"

typedef enum PclType_e {
    PCLTYPE_UNKNOWN,
    PCLTYPE_PCL,
    PCLTYPE_PXL,
    PCLTYPE_POSTSCRIPT
} PclType;


/* private */
static int doc_scan(Doc *doc);	/* look for DSC comments */
static int doc_init(Doc *doc);
static int doc_gunzip(Doc *doc);
static int doc_bunzip2(Doc *doc);
static int doc_msg_len(void *handle, const char *str, int len);
static PclType doc_pl_parse(const char *str, int len, unsigned int *resolution);
static int doc_pl_readline(const char *str, int len);
static PclType doc_pjl_parse(const char *str, int len, unsigned int *resolution);

/* prototypes */
void doc_message(void *caller_data, const char *str);
int show_dsc_error(void *caller_data, CDSC *dsc, 
	unsigned int explanation, const char *line, unsigned int line_len);


/* Create a new document object */
Doc *
doc_new(GSview *a)
{
    Doc *d = (Doc *)malloc(sizeof(Doc));
    if (d == NULL) {
	app_msg(a, "Out of memory\n");
	return_error(NULL);
    }
    app_lock(a);
    doc_init(d);
    d->app = a;
    app_ref(a);		/* doc has reference to app */
    doc_ref(d);		/* caller has reference to doc */
    app_unlock(a);
    return d;
}


/* Add document to application list of documents */
int 
doc_add(Doc *d, GSview *a)
{
    Doc *dn;
    app_lock(a);
    dn = *app_docs(a);
    while (dn && dn->next)
	dn = dn->next;
    if (dn == NULL)
	*app_docs(a) = d;
    else
	dn->next = d;
    doc_ref(d);		/* app has reference to doc */
    app_unlock(a);
    return 0;
}

/* Remove document from application list */
int 
doc_remove(Doc *d)
{
    GSview *a = d->app;
    int code = 0;
    Doc *dn;
    app_lock(a);
    dn = *app_docs(a);
    if (dn == NULL) {
	/* not in list */
	char str[] = "App has no Doc\n";
	/* Doc has already locked app */
	app_msg_len_nolock(a, str, (int)strlen(str));
	code = -1;
    }
    else if (dn == d) {
	*app_docs(a) = d->next;
	doc_unref(d);	/* app loses reference to doc */
    }
    else {
	while (dn && dn->next != d)
	    dn = dn->next;
	if (dn && dn->next == d) {
	    dn->next = d->next;
	    doc_unref(d);	/* app loses reference to doc */
	}
	else {
	    char str[] = "Doc not found\n";
	    /* Doc has already locked app */
	    app_msg_len_nolock(a, str, (int)strlen(str));
	    code = -1;
	}
    }
    app_unlock(a);
    return code;
}


/* Increment reference count of Doc */
/* A View will increment refcount when it attaches to Doc */
/* Assumes we own the lock */
int
doc_ref(Doc *d)
{
    int refcount = ++(d->refcount);
    if (debug & DEBUG_DEV) {
	char buf[MAXSTR];
	snprintf(buf, sizeof(buf), "doc refcount=%d\n", refcount);
	buf[sizeof(buf)-1]='\0';
	app_msg_len_nolock(d->app, buf, (int)strlen(buf));
    }
    return refcount;
}

/* Release reference to Doc. */
/* When reference count reaches zero, Doc is freed. */
/* Assumes we own the lock */
int
doc_unref(Doc *d)
{
    int refcount;
    GSview *a = d->app;
    if (d->refcount > 0)
	d->refcount--;
    refcount = d->refcount;
    if (debug & DEBUG_DEV) {
	char buf[MAXSTR];
	snprintf(buf, sizeof(buf), "doc refcount=%d\n", refcount);
	buf[sizeof(buf)-1]='\0';
	app_msg_len_nolock(d->app, buf, (int)strlen(buf));
    }
    if (d->refcount == 0) {
        doc_close(d);
	d->app = NULL;
	app_unref(a);	/* doc loses reference to app */
	memset(d, 0, sizeof(Doc));
	free(d);
    }
    return refcount;
}

GSview *
doc_app(Doc *doc)
{
    return doc->app;
}


static int
doc_init(Doc *doc)
{
    memset(doc, 0, sizeof(Doc));
    doc->handle = NULL;
    doc->app = NULL;
    doc->next = NULL;
    doc->viewlist = NULL;
    doc->doctype = DOC_UNKNOWN;
    doc->page_count = -1;
    doc->ignore_dsc = FALSE;
    doc->dsc_warn = CDSC_ERROR_WARN;
    doc->verbose = TRUE;
    return 0;
}

int
doc_close(Doc *doc)
{
#ifdef NOTUSED
    /* index of extracted text */
    free_text_index(doc);
    if (doc->text_name[0])
	wunlink(doc->text_name);
    memset(doc->text_name, 0, sizeof(doc->text_name));
#endif

    /* temporary uncompressed file */
    if (doc->tname[0])
	csunlink(doc->tname);

    memset(doc->name, 0, sizeof(doc->name));
    memset(doc->tname, 0, sizeof(doc->tname));
    doc->doctype = DOC_UNKNOWN;
    doc->gzip = FALSE;
    doc->bzip2 = FALSE;
    doc->page_count = 0;		
    doc->length1 = 0;
    doc->length2 = 0;
    doc->time1 = 0;
    doc->time2 = 0;

    if (doc->dsc)
	dsc_unref(doc->dsc);
    doc->dsc = NULL;
    
#ifdef NOTUSED
    doc->ignore_special = FALSE;
#endif
    doc->ctrld = FALSE;
    doc->pjl = FALSE;

    if (doc->pdfscan)
	pdf_scan_close(doc->pdfscan);
    doc->pdfscan = NULL;
    return 0;
}

/* Open a new document.
 * +ve = non DSC
 * 0 = DSC
 * -ve = error
 */
int
doc_open(Doc *doc, LPCTSTR filename)
{
    int code;
    doc_close(doc);
    csncpy(doc->name, filename, sizeof(doc->name)/sizeof(TCHAR)-1);
    code = doc_scan(doc);
    if (code < 0)
	doc_close(doc);
    return code;
}

DocType
doc_type(Doc *doc)
{
    return doc->doctype;
}

BOOL
doc_is_open(Doc *doc)
{
    return (doc->name[0] != '\0');
}

void 
doc_ignore_dsc(Doc *doc, BOOL flag)
{
    doc->ignore_dsc = flag;
}

void 
doc_dsc_warn(Doc *doc, int level)
{
    doc->dsc_warn = level;
}

void 
doc_verbose(Doc *doc, BOOL verbose)
{
    doc->verbose = verbose;
}

void
doc_message(void *caller_data, const char *str)
{
    app_msg(((Doc *)caller_data)->app, str);
}


/* Debug for DSC comments */
void
doc_dump(Doc *doc)
{
    app_csmsgf(doc->app, TEXT("DSC dump for %.200s\n"), doc->name);
    dsc_display(doc->dsc, doc_message);
    app_csmsgf(doc->app, TEXT("End of DSC dump\n"));
}

int 
show_dsc_error(void *caller_data, CDSC *dsc, 
	unsigned int explanation, const char *line, unsigned int line_len)
{
    Doc *doc = (Doc *)caller_data;
    int response = CDSC_RESPONSE_CANCEL;
    int severity;
    char buf[MAXSTR];
    int len;
    TCHAR title[MAXSTR];
    TCHAR linefmt[MAXSTR];
    int i;
    TCHAR *p;

    if (explanation > dsc->max_error)
	return CDSC_RESPONSE_OK;

    severity = dsc->severity[explanation];

    /* If debug function provided, copy messages there */
    /* These are always in English */
    if (dsc->debug_print_fn) {
	switch (severity) {
	    case CDSC_ERROR_INFORM:
		dsc_debug_print(dsc, "\nDSC Information");
		break;
	    case CDSC_ERROR_WARN:
		dsc_debug_print(dsc, "\nDSC Warning");
		break;
	    case CDSC_ERROR_ERROR:
		dsc_debug_print(dsc, "\nDSC Error");
	        break;
	}
	dsc_debug_print(dsc, "\n");
	if (explanation <= dsc->max_error) {
	    if (line && line_len) {
		int length = min(line_len, sizeof(buf)-1);
		snprintf(buf, sizeof(buf), "At line %d:\n", dsc->line_count);
		buf[sizeof(buf)-1]='\0';
		dsc_debug_print(dsc, buf);
		strncpy(buf, line, length);
		buf[length]='\0';
		dsc_debug_print(dsc, "  ");
		dsc_debug_print(dsc, buf);
	    }
	    dsc_debug_print(dsc, dsc_message[explanation]);
	}
    }

    /* Here you could prompt user for OK, Cancel, Ignore ALL DSC */
    if (severity <= doc->dsc_warn)
	return response;

    switch (severity) {
	case CDSC_ERROR_INFORM:
	    i = IDS_DSC_INFO;
	    break;
	case CDSC_ERROR_WARN:
	    i = IDS_DSC_WARN;
	    break;
	case CDSC_ERROR_ERROR:
	    i = IDS_DSC_ERROR;
	    break;
	default:
	    i = -1;
    }
    if (i != -1)
        load_string(doc->app, i, title, sizeof(title)/sizeof(TCHAR));
    else
	title[0] = '\0';

    /* build up string */
#define MLEN 4096
    p = (TCHAR *)malloc(MLEN*sizeof(TCHAR));
    memset(p, 0, MLEN*sizeof(TCHAR));
    if (p == (TCHAR *)NULL)
	return response;

    if (line) {
	int len;
	int wlen;
        load_string(doc->app, IDS_DSC_LINEFMT, linefmt, sizeof(linefmt)/sizeof(TCHAR));
        csnprintf(p, MLEN, linefmt, title, dsc->line_count);
	csncat(p, TEXT("\n   "), MLEN-1-cslen(p));
	len = (int)cslen(p);
	if (line_len > 256)
	    line_len = 256;	/* this is the maximum DSC line length */
	wlen = narrow_to_cs(NULL, 0, line, line_len);
	narrow_to_cs(p+len, MLEN-1-len, line, line_len);
        p[len+wlen] = '\0';
    }
    else {
	csncpy(p, title, MLEN-1);
	csncat(p, TEXT("\n"), MLEN-1-cslen(p));
    }
    len = (int)cslen(p);
    load_string(doc->app, CDSC_RESOURCE_BASE+(explanation*2), 
	p+len, MLEN-len);
    len = (int)cslen(p);
    load_string(doc->app, CDSC_RESOURCE_BASE+(explanation*2)+1, 
	p+len, MLEN-len);
    
    response = get_dsc_response(doc->app, p);

    free(p);

    if (dsc->debug_print_fn) {
	switch (response) {
	    case CDSC_RESPONSE_OK:
		dsc_debug_print(dsc, "Response = OK\n");
		break;
	    case CDSC_RESPONSE_CANCEL:
		dsc_debug_print(dsc, "Response = Cancel\n");
		break;
	    case CDSC_RESPONSE_IGNORE_ALL:
		dsc_debug_print(dsc, "Response = Ignore All DSC\n");
		break;
	}
    }

    return response;
}

LPCTSTR
doc_name(Doc *doc)
{
    /* If original file was compressed, give name of uncompressed file */
    if ((doc->tname[0]!='\0') && ((doc->gzip) || doc->bzip2))
	return doc->tname;
    /* otherwise return original file name */
    return doc->name;
}


/* gunzip to temporary file */
static int
doc_gunzip(Doc *doc)
{
    GFile *outfile;
    int code;
    char name[MAXSTR+MAXSTR];
    if ((outfile = app_temp_gfile(doc->app, doc->tname, 
	sizeof(doc->tname)/sizeof(TCHAR)-1)) 
	== (GFile *)NULL) {
	TCHAR buf[MAXSTR];
	load_string(doc->app, IDS_NOTEMP, buf, sizeof(buf)/sizeof(TCHAR));
	app_csmsg(doc->app, buf);
	return_error(-1);
    }

    cs_to_narrow(name, sizeof(name)-1, doc->name, (int)cslen(doc->name)+1);
    app_msg(doc->app, "Uncompressing ");
    app_msg(doc->app, name);
    app_msg(doc->app, " to ");
    app_csmsg(doc->app, doc->tname);
    app_msg(doc->app, "\n");

    code = zlib_uncompress(doc->app, outfile, name);
    gfile_close(outfile);
    if (code != 0) {
	csunlink(doc->tname);
	doc->tname[0] = '\0';
	return_error(-1);
    }

    return 0;
}


/* Uncompress bzip2 to temporary file */
static int
doc_bunzip2(Doc *doc)
{
    GFile *outfile;
    int code;
    char name[MAXSTR+MAXSTR];
    if ((outfile = app_temp_gfile(doc->app, doc->tname, 
	sizeof(doc->tname)/sizeof(TCHAR)-1)) 
	== (GFile *)NULL) {
	TCHAR buf[MAXSTR];
	load_string(doc->app, IDS_NOTEMP, buf, sizeof(buf)/sizeof(TCHAR));
	app_csmsg(doc->app, buf);
	return_error(-1);
    }

    cs_to_narrow(name, sizeof(name)-1, doc->name, (int)cslen(doc->name)+1);
    app_msg(doc->app, "Uncompressing ");
    app_msg(doc->app, name);
    app_msg(doc->app, " to ");
    app_csmsg(doc->app, doc->tname);
    app_msg(doc->app, "\n");

    code = bzip2_uncompress(doc->app, outfile, name);
    gfile_close(outfile);
    if (code != 0) {
	csunlink(doc->tname);
	doc->tname[0] = '\0';
	return_error(-1);
    }

    return 0;
}

/* Try to recognise PCL files.
 * PJL starts with
 *  ESC %-12345X
 * PCL commonly starts with 
 *  ESC %, ESC E, ESC *, ESC &
 * PCLXL commonly starts with
 *  ) HP-PCL XL
 * If the resolution is not specified, set it to 0.
 */
static PclType doc_pl_parse(const char *str, int len, unsigned int *resolution)
{
    PclType type;
    *resolution = 0;
    if ((len >= 9) && (memcmp(str, "\033%-12345X", 9) == 0))
	type = doc_pjl_parse(str, len, resolution);
    else if ((len >= 11) && (memcmp(str, ") HP-PCL XL", 11) == 0))
	type = PCLTYPE_PXL;
    else if ((len >= 2) && (str[0] == '\033') &&
	((str[1]=='%') || (str[1]=='E') || (str[1]=='*') || (str[1] == '&')))
	type = PCLTYPE_PCL;
    else 
	type = PCLTYPE_UNKNOWN;
    return type;
}
 
/* Get length of line in buffer.
 */
static int doc_pl_readline(const char *str, int len)
{
    int i = 0;
    /* skip until EOL */
    while ((i<len) && (str[i] != '\r') && (str[i] != '\n'))
	i++;
    /* skip over EOL */
    while ((i<len) && ((str[i] == '\r') || (str[i] == '\n')))
	i++;
    return i;
}

/* PJL starts with \033%-12345X
 * and finishes with a line that does not start with @.
 * We are interested in the following lines:
 *  @PJL SET RESOLUTION=600
 *  @PJL ENTER LANGUAGE=PCL
 *  @PJL ENTER LANGUAGE=PCLXL
 *  @PJL ENTER LANGUAGE=POSTSCRIPT
 * If the language type is not specified but PJL is used,
 * assume PCL.
 */
static PclType doc_pjl_parse(const char *str, int len, unsigned int *resolution)
{
    PclType type = PCLTYPE_PCL;
    const char *p;
    int idx = 0;
    int count;
    int i;
    char buf[16];
    int bufcnt;
    if ((len < 9) || (memcmp(str, "\033%-12345X", 9) != 0))
	return PCLTYPE_UNKNOWN;
    idx = 9;
    while ((count = doc_pl_readline(str+idx, len-idx)) != 0) {
	p = str+idx;
	if ((count >= 9) && (memcmp(p, "\033%-12345X", 9) == 0)) {
	    count -= 9;
	    p += 9;
	    idx += 9;
	    if (count == 0)
		break;
	}
	if (*p != '@')
	    break; /* Not PJL */
	if ((count >= 19) && 
	    (memcmp(p, "@PJL SET RESOLUTION", 19) == 0)) {
	    i = 19;
	    while ((i<count) && (p[i] == ' '))
		i++;
	    if ((i<count) && (p[i] == '=')) {
		i++;
		while ((i<count) && (p[i] == ' '))
		    i++;
		/* now read number */
		bufcnt = min(count-i, (int)sizeof(buf)-1);
		memcpy(buf, p+i, bufcnt);
		buf[bufcnt] = '\0';
		*resolution = atoi(buf);
	    }
	}
	else if ((count >= 19) && 
	    (memcmp(p, "@PJL ENTER LANGUAGE", 19) == 0)) {
	    i = 19;
	    while ((i<count) && (p[i] == ' '))
		i++;
	    if ((i<count) && (p[i] == '=')) {
		i++;
		while ((i<count) && (p[i] == ' '))
		    i++;
		/* now read language */
		if ((count-i >= 10) &&
	            (memcmp(p+i, "POSTSCRIPT", 10) == 0))
		    type = PCLTYPE_POSTSCRIPT;
		else if ((count-i >= 5) &&
	            (memcmp(p+i, "PCLXL", 5) == 0))
		    type = PCLTYPE_PXL;
		else if ((count-i >= 3) &&
	            (memcmp(p+i, "PCL", 3) == 0))
		    type = PCLTYPE_PCL;
	    }
	}
	idx += count;
    }
    return type;
}


/* scan file for PostScript Document Structuring Conventions */
/* return -ve if error */
/* return +ve if not DSC */
/* On return, doc->dsc is non-zero if DSC */
int
doc_scan(Doc *doc)
{
char line[4096];
FILE_POS file_length;
GFile *f;
CDSC *dsc;
PclType pcltype = PCLTYPE_UNKNOWN;

    if ( (f = gfile_open(doc->name, gfile_modeRead)) == (GFile *)NULL ) {
	app_msg(doc->app, "File \042");
	app_csmsg(doc->app, doc_name(doc));
	app_msg(doc->app, "\042 does not exist\n");
	return_error(-1);
    }

    if (doc->dsc)
	dsc_unref(doc->dsc);
    doc->dsc = NULL;
    
    /* get first line to look for magic numbers */
    memset(line, 0, sizeof(line));
    gfile_read(f, line, sizeof(line)-1);
    gfile_seek(f, 0, gfile_begin);

    /* check for gzip */
    doc->gzip = FALSE;
    doc->bzip2 = FALSE;
    if ( (line[0]=='\037') && (line[1]=='\213') ) { /* 1F 8B */
	doc->gzip = TRUE;
	gfile_close(f);
	if (doc_gunzip(doc) != 0) {
	    app_msg(doc->app, "Failed to gunzip file\n");
	    return_error(-1);
	}
	if ((f = gfile_open(doc_name(doc), gfile_modeRead)) == (GFile *)NULL) {
	    app_msg(doc->app, "File '");
	    app_csmsg(doc->app, doc_name(doc));
	    app_msg(doc->app, "' does not exist\n");
	    return_error(-1);
	}
	gfile_read(f, line, sizeof(line)-1);
	gfile_seek(f, 0, gfile_begin);
    }

    /* check for bzip2 */
    if ( (line[0]=='B') && (line[1]=='Z') && (line[2]=='h')) { /* "BZh */
	doc->bzip2 = TRUE;
	gfile_close(f);
	if (doc_bunzip2(doc) != 0) {
	    app_msg(doc->app, "Failed to uncompress bzip2 file\n");
	    return_error(-1);
	}
	if ((f = gfile_open(doc_name(doc), gfile_modeRead)) == (GFile *)NULL) {
	    app_msg(doc->app, "File '");
	    app_csmsg(doc->app, doc_name(doc));
	    app_msg(doc->app, "' does not exist\n");
	    return_error(-1);
	}
	gfile_read(f, line, sizeof(line)-1);
	gfile_seek(f, 0, gfile_begin);
    }

    file_length = gfile_get_length(f);

    /* save file date and length */
    doc_savestat(doc);

    doc->doctype = DOC_UNKNOWN;
    doc->dpi = 0;

    /* check for PDF */
    if ( strncmp("%PDF-", line, 5) == 0 ) {
	gfile_close(f);
	doc->doctype = DOC_PDF;
	doc->page_count = 0;
	if (doc->pdfscan)
	    pdf_scan_close(doc->pdfscan);
	doc->pdfscan = NULL;
	doc->pdfscan = pdf_scan_open(doc_name(doc), doc->app, doc_msg_len);
	doc->page_count = pdf_scan_page_count(doc->pdfscan);
        if (debug & DEBUG_GENERAL)
	    app_msgf(doc->app, "PDF page count %d\n", doc->page_count);
	return 0;
    }

    /* check for PCL */
#ifdef NOTUSED
    if ((line[0]=='\033') && (line[1]=='E') && (line[2]=='\033')) {
	TCHAR buf[MAXSTR];
	doc->doctype = DOC_PCL;
	load_string(doc->app, IDS_PROBABLY_PCL, buf, sizeof(buf)/sizeof(TCHAR));
	app_msgf(doc->app, "%s\n", buf);
	if (app_msg_box(doc->app, buf, MB_ICONEXCLAMATION | MB_YESNO) 
	    != IDYES) {
	    gfile_close(f);
	    return_error(-1);
	}
    }
#else
    pcltype = doc_pl_parse(line, (int)strlen(line), &doc->dpi);
    if ((pcltype == PCLTYPE_PCL) || (pcltype == PCLTYPE_PXL)) {
	doc->doctype = DOC_PCL;
        if (debug & DEBUG_GENERAL)
	    app_msgf(doc->app, "Document is PCL or PXL\n");
	gfile_close(f);
	return 0;
    }
#endif

    /* check for Windows BMP or NETPBM */
    if ( ((line[0]=='B') && (line[1]=='M')) ||
	 ((line[0]=='P') && (line[1]>='1') && (line[1]<='6')) ||
	 (((unsigned char)line[0]==0x89) && (line[1]=='P') && 
	  (line[2]=='N') && (line[3]=='G'))
       ) {
	doc->doctype = DOC_BITMAP;
	doc->page_count = 1;
	gfile_close(f);
	return 0;
    }

    /* Otherwise, assume it is PostScript */
    doc->doctype = DOC_PS;

    /* check for documents that start with Ctrl-D */
    doc->ctrld = (line[0] == '\004');
    /* check for HP LaserJet prologue */
    doc->pjl = FALSE;
    if (strncmp("\033%-12345X", line, 9) == 0)
	doc->pjl = TRUE;
    if (doc->ignore_dsc)
	doc->dsc = (CDSC *)NULL;
    else  {
	int code = 0;
	int count;
	char *d;
	doc->dsc = NULL;
	if ( (d = (char *) malloc(COPY_BUF_SIZE)) == NULL)
	    return_error(-1);

	doc->dsc = dsc_new(doc);
	if (doc->verbose)
	    dsc_set_debug_function(doc->dsc, doc_message);
	dsc_set_error_function(doc->dsc, show_dsc_error);
	dsc_set_length(doc->dsc, file_length);
	while ((count = (int)gfile_read(f, d, COPY_BUF_SIZE))!=0) {
	    code = dsc_scan_data(doc->dsc, d, count);
	    if ((code == CDSC_ERROR) || (code == CDSC_NOTDSC)) {
		/* not DSC or an error */
		break;
	    }
	}
	if ((code == CDSC_ERROR) || (code == CDSC_NOTDSC)) {
	    dsc_unref(doc->dsc);
	    doc->dsc = NULL;
	}
	else {
	    dsc_fixup(doc->dsc);
	}
        free(d);
    }
    gfile_close(f);

    /* check for DSC comments */
    dsc = doc->dsc;
    if (dsc == (CDSC *)NULL)
	return 1;	/* OK, but not DSC */

    if (dsc->doseps) {
	BOOL bad_header = FALSE;
	/* check for errors in header */
	if (dsc->doseps->ps_begin > file_length)
	    bad_header = TRUE;
	if (dsc->doseps->ps_begin + dsc->doseps->ps_length > file_length)
	    bad_header = TRUE;
	if (dsc->doseps->wmf_begin > file_length)
	    bad_header = TRUE;
	if (dsc->doseps->wmf_begin + dsc->doseps->wmf_length > file_length)
	    bad_header = TRUE;
	if (dsc->doseps->tiff_begin > file_length)
	    bad_header = TRUE;
	if (dsc->doseps->tiff_begin + dsc->doseps->tiff_length > file_length)
	    bad_header = TRUE;
	if (bad_header) {
	    TCHAR buf[MAXSTR];
	    load_string(doc->app, IDS_BAD_DOSEPS_HEADER, 
		buf, sizeof(buf)/sizeof(TCHAR));
	    app_csmsgf(doc->app, TEXT("%s\n"), buf);
	    app_msg_box(doc->app, buf, 0);
	    /* Ignore the bad information */
	    dsc_unref(doc->dsc);
	    doc->dsc = NULL;
	    return_error(-1);
	}
    }

    if (debug & DEBUG_GENERAL)
	doc_dump(doc);

    return 0;
}


/* reverse zero based page number if needed */
int
doc_map_page(Doc *doc, int page)
{
    if (doc->dsc != (CDSC *)NULL)
        if (doc->dsc->page_order == CDSC_DESCEND) 
	    return (doc->dsc->page_count - 1) - page;
    return page;
}

int 
doc_page_limit(Doc *doc, int page)
{
    if (doc == NULL)
	return 0;
    if (doc->doctype == DOC_PS) {
	if (doc->dsc) {
	    if (page >= (int)doc->dsc->page_count)
		page = doc->dsc->page_count - 1;
	    if (page < 0)
		page = 0;
	}
	else if (doc->page_count) {
	    if (page >= (int)doc->page_count)
		page = doc->page_count - 1;
	}
    }
    else if (doc->doctype == DOC_PDF) {
	if (page >= (int)doc->page_count)
	    page = doc->page_count - 1;
	if (page < 0)
	    page = 0;
    }
    else if (doc->doctype == DOC_BITMAP) 
	page = 0;
    return page;
}


View **
doc_views(Doc *doc)
{
    return &doc->viewlist;
}

/* append to a string in a buffer of length len */
static TCHAR *
csappend(LPTSTR dest, LPCTSTR src, int len)
{
    return csncat(dest, src, len - (int)cslen(dest)/sizeof(TCHAR) - 1);
}

/* Information about a document, suitable for showing in a dialog */
void
doc_info(Doc *doc, DocInfo *info) 
{
    TCHAR buf[MAXSTR];
    CDSC *dsc = doc->dsc;
    int typelen;
    memset(info, 0, sizeof(DocInfo));
    if (cslen(doc->name))
	csncpy(info->name, doc->name, sizeof(info->name)/sizeof(TCHAR));
    else
	load_string(doc->app, IDS_NOFILE, info->name, 
	    sizeof(info->name)/sizeof(TCHAR));
    
    typelen = sizeof(info->type)/sizeof(TCHAR);
    if (doc->gzip)
	csappend(info->type, TEXT("gzip "), typelen);
    if (doc->bzip2)
	csappend(info->type, TEXT("bzip2 "), typelen);
    if (dsc) {
	load_string(doc->app, IDS_CTRLD, buf, sizeof(buf)/sizeof(TCHAR));
	if (doc->ctrld)
	    csappend(info->type, buf, typelen);
	load_string(doc->app, IDS_PJL, buf, sizeof(buf)/sizeof(TCHAR));
	if (doc->pjl)
	    csappend(info->type, buf, typelen);
	load_string(doc->app, IDS_DCS2, buf, sizeof(buf)/sizeof(TCHAR));
	if (dsc->dcs2)
	    csappend(info->type, buf, typelen);
	if (dsc->epsf) {
	    switch(dsc->preview) {
		default:
		case CDSC_NOPREVIEW:
		    buf[0] = '\0';
		    break;
		case CDSC_EPSI:
		    load_string(doc->app, IDS_EPSI, 
			buf, sizeof(buf)/sizeof(TCHAR));
		    break;
		case CDSC_TIFF:
		    load_string(doc->app, IDS_EPST, 
			buf, sizeof(buf)/sizeof(TCHAR));
		    break;
		case CDSC_WMF:
		    load_string(doc->app, IDS_EPSW, 
			buf, sizeof(buf)/sizeof(TCHAR));
		    break;
	    }
	    csappend(info->type, buf, typelen);
	}
	else {
	    load_string(doc->app, IDS_DSC, buf, sizeof(buf)/sizeof(TCHAR));
	    csappend(info->type, buf, typelen);
	}
	if (dsc->dsc_title)
	    narrow_to_cs(info->title, sizeof(info->title)/sizeof(TCHAR)-1,
		dsc->dsc_title, (int)strlen(dsc->dsc_title)+1);
	if (dsc->dsc_date)
	    narrow_to_cs(info->date, sizeof(info->date)/sizeof(TCHAR)-1,
		dsc->dsc_date, (int)strlen(dsc->dsc_date)+1);
	if (dsc->bbox) {
	    csnprintf(buf, sizeof(buf)/sizeof(TCHAR), 
		TEXT("%d %d %d %d"), dsc->bbox->llx, dsc->bbox->lly,
		dsc->bbox->urx, dsc->bbox->ury);
	    buf[sizeof(buf)-1]='\0';
	    csncpy(info->bbox, buf, sizeof(info->bbox)/sizeof(TCHAR));
	}
	if (dsc->hires_bbox) {
	    csnprintf(buf, sizeof(buf)/sizeof(TCHAR), TEXT("%g %g %g %g"), 
		dsc->hires_bbox->fllx, dsc->hires_bbox->flly,
		dsc->hires_bbox->furx, dsc->hires_bbox->fury);
	    buf[sizeof(buf)-1]='\0';
	    csncpy(info->hiresbbox, buf, 
		sizeof(info->hiresbbox)/sizeof(TCHAR));
	}
	switch (dsc->page_orientation) {
	    case CDSC_PORTRAIT:
		load_string(doc->app, IDS_PORTRAIT, info->orientation, 
		    sizeof(info->orientation)/sizeof(TCHAR));
		break;
	    case CDSC_LANDSCAPE:
		load_string(doc->app, IDS_LANDSCAPE, info->orientation, 
		    sizeof(info->orientation)/sizeof(TCHAR));
		break;
	}
	switch (dsc->page_order) {
	    case CDSC_ASCEND:
		load_string(doc->app, IDS_ASCEND, info->pageorder, 
		    sizeof(info->pageorder)/sizeof(TCHAR));
		break;
	    case CDSC_DESCEND:
		load_string(doc->app, IDS_LANDSCAPE, info->pageorder, 
		    sizeof(info->pageorder)/sizeof(TCHAR));
		break;
	    case CDSC_SPECIAL:
		load_string(doc->app, IDS_SPECIAL, info->pageorder, 
		    sizeof(info->pageorder)/sizeof(TCHAR));
		break;
	}
	if (dsc->page_media && dsc->page_media->name) {
	    int wlen = narrow_to_cs(buf, (int)sizeof(buf)/sizeof(TCHAR), 
		dsc->page_media->name, (int)strlen(dsc->page_media->name)+1);
	    csnprintf(buf+wlen, sizeof(buf)/sizeof(TCHAR)-wlen, TEXT(" %g %g"),
		dsc->page_media->width, dsc->page_media->height);
	    buf[sizeof(buf)-1]='\0';
	}
	else {
	    buf[0] = '\0';
	}
	csncpy(info->pagemedia, buf, sizeof(info->pages)/sizeof(TCHAR));
	csnprintf(buf, sizeof(buf)/sizeof(TCHAR), TEXT("%d"), dsc->page_count);
	buf[sizeof(buf)-1]='\0';
	csncpy(info->pages, buf, sizeof(info->pages)/sizeof(TCHAR));
    }
    else if (doc->doctype == DOC_PDF) {
	load_string(doc->app, IDS_PDF, buf, sizeof(buf)/sizeof(TCHAR));
	csappend(info->type, buf, typelen);
	csnprintf(buf, sizeof(buf)/sizeof(TCHAR), 
	    TEXT("%d"), doc->page_count);
	buf[sizeof(buf)-1]='\0';
	csncpy(info->pages, buf, sizeof(info->pages)/sizeof(TCHAR));
    }
}

/* Generate string for page list box, as either
 *   ordinal
 *   ordinal    "label"
 */
void
doc_ordlabel(Doc *doc, char *buf, int buflen, int page_number)
{
    if (doc->doctype == DOC_PDF) {
        snprintf(buf, buflen, "%d", page_number+1);
    }
    else if (doc->dsc) {
	const char *label;
	int ordinal;
	label = doc->dsc->page[doc_map_page(doc, page_number)].label;
	ordinal = doc->dsc->page[doc_map_page(doc, page_number)].ordinal;
	snprintf(buf, buflen, "%d", ordinal);
	if (strcmp(buf, label) != 0) {
	    /* label and ordinal don't match, so use composite */
	    snprintf(buf, buflen, "%d  \042%s\042", ordinal, label);
	}
    }
}

static int 
doc_msg_len(void *handle, const char *str, int len)
{
    return app_msg_len((GSview *)handle, str, len);
}

/* Copy document unmodified */
int
doc_copyfile(Doc *doc, LPCTSTR filename)
{
    GFile *infile, *outfile;
    int code = 0;
    char *buf;
    int count = 0;

    buf = (char *)malloc(COPY_BUF_SIZE);
    if (buf == (char *)NULL)
	return -1;

    if ((infile = gfile_open(doc_name(doc), gfile_modeRead)) == (GFile *)NULL){
	app_msg(doc->app, "File \042");
	app_csmsg(doc->app, doc_name(doc));
	app_msg(doc->app, "\042 does not exist\n");
	free(buf);
	return_error(-1);
    }
    if ((outfile = gfile_open(filename, gfile_modeWrite | gfile_modeCreate)) 
	== (GFile *)NULL) {
	app_msg(doc->app, "File \042");
	app_csmsg(doc->app, filename);
	app_msg(doc->app, "\042 can not be opened for writing\n");
	free(buf);
	gfile_close(infile);
	return_error(-1);
    }

    while (((count = (int)gfile_read(infile, buf, COPY_BUF_SIZE)) > 0))
        if ((int)gfile_write(outfile, buf, count) != count)
	    break;
    free(buf);

    if (gfile_error(infile)!=0)
	code = -1;
    if (gfile_error(outfile)!=0)
	code = -1;

    gfile_close(outfile);
    gfile_close(infile);
    if (code && !(debug & DEBUG_GENERAL))
	csunlink(filename);
    return code;
}
