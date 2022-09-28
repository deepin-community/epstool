/* Copyright (C) 1993-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cps.c,v 1.7 2005/06/10 09:39:24 ghostgum Exp $ */
/* Copying PostScript files */

#include "common.h"
#include "dscparse.h"
#include "capp.h"
#define DEFINE_CDOC
#include "cdoc.h"

int ps_extract(Doc *doc, LPCTSTR filename, PAGELIST *pagelist, int copies);
int ps_copy(GFile *outfile, GFile *infile, FILE_OFFSET begin, FILE_OFFSET end);
int ps_fgets(char *s, int n, GFile *f);

static int ps_writestring(GFile *f, const char *str)
{
    return gfile_write(f, str, (int)strlen(str));
}

/* Like fgets, but allows any combination of EOL characters
 * and returns the count of bytes, not the string pointer.
 * This reads one byte at a time, so is pretty slow.
 * Use it for copying a header or trailer, but not for
 * copying the whole document.
 */
int ps_fgets(char *s, int n, GFile *f)
{
    char ch = '\0';
    int not_eof = 0;
    char *p = s;
    int count = 0;	/* bytes written to buffer */
    /* copy until first EOL character */
    while ( (count < n) &&  ((not_eof = (int)gfile_read(f, &ch, 1)) != 0)
	&& (ch != '\r') && (ch != '\n') ) {
	*p++ = (char)ch;
	count++;
    }

    if ((count < n) && not_eof) {
        /* check for extra EOL characters */
	if (ch == '\r') {
	    *p++ = (char)ch;
	    count++;
	    /* check if MS-DOS \r\n is being used */
	    if ((count < n) && ((not_eof = (int)gfile_read(f, &ch, 1)) != 0)) {
		if (ch == '\n') {
		    /* Yes, MS-DOS */
		    *p++ = (char)ch;
		    count++;
		}
		else {
		    /* No, Macintosh */
		    gfile_seek(f, -1, gfile_current);
		}
	    }
	}
	else {
	    /* must have been '\n' */
	    *p++ = (char)ch;
	    count++;
	}
    }
    if (count < n)
	*p = '\0';

    if ( (!not_eof) && (p == s) )
	return 0;
    return count;
}



static void 
ps_num_copies(GFile *f, int copies)
{
   char buf[MAXSTR];
   if (copies >= 1) {
      snprintf(buf, sizeof(buf)-1,  
	"[{\n%%%%BeginFeature: *NumCopies %d\n", copies);
      ps_writestring(f, buf);
      snprintf(buf, sizeof(buf)-1,  
	"<< /NumCopies %d >> setpagedevice\n", copies);
      ps_writestring(f, buf);
      ps_writestring(f, "%%%%EndFeature\n} stopped cleartomark\n");
   }
}

/* Copy DSC until a particular comment is found */
/* Do not copy the comment */
/* Stop if file offset exceeds end */
/* return TRUE if comment found, FALSE if not found */
static BOOL 
ps_copy_find(GFile *outfile, GFile *infile, FILE_OFFSET end,
        char *s, int n, const char *comment)
{
    int count;
    int remain;
    while (((remain = (FILE_POS)end - gfile_get_position(infile)) > 0) &&
        ((count = ps_fgets(s, min(n-1, remain), infile))!=0)) {
        s[n-1] = '\0';
        if (strncmp(s, comment, strlen(comment)) == 0)
            return TRUE;
        gfile_write(outfile, s, count);
    }
    return FALSE;
}

int 
ps_copy(GFile *outfile, GFile *infile, FILE_OFFSET begin, FILE_OFFSET end)
{
    char *buf;
    int count = 0;
    buf = (char *)malloc(COPY_BUF_SIZE);
    if (buf == (char *)NULL)
	return -1;
    if (begin >= 0)
        gfile_seek(infile, begin, gfile_begin);	/* seek to section to extract */
    begin = gfile_get_position(infile);
    while (begin < end) {
        count = (int)(min(end-begin, COPY_BUF_SIZE));
	if ((count = (int)gfile_read(infile, buf, count)) > 0) {
	    gfile_write(outfile, buf, count);
	    begin += count;
	}
	else
	    begin = end;	/* EOF or error */
    }
    free(buf);
    return count;
}

static int
ps_copy_setup(GFile *outfile, GFile *infile, 
    FILE_OFFSET begin, FILE_OFFSET end, int copies)
{
    char line[DSC_LINE_LENGTH+1];
    BOOL found;
    int code = 0;
    if (copies > 1) {
        if (begin != end) {
            gfile_seek(infile, begin, gfile_begin);
            /* copy up to, but not including %%EndSetup */
            found = ps_copy_find(outfile, infile, end,
                line, sizeof(line)-1, "%%EndSetup");
            /* insert code for multiple copies */
            ps_num_copies(outfile, copies);
            /* copy rest of setup section, including %%EndSetup */
            if (found)
	        ps_writestring(outfile, line);
            code = ps_copy(outfile, infile, -1, end);
        }
        else {
            /* setup section was missing - add our own. */
	    ps_writestring(outfile, "%%BeginSetup\n");
            ps_num_copies(outfile, copies);
	    ps_writestring(outfile, "%%EndSetup\n");
        }
    }
    else
        code = ps_copy(outfile, infile, begin, end);
    return code;
}


/* Extract pages from a PostScript file */
/* The pages are in v->pagelist */
static int
ps_extract_pages(Doc *doc, GFile *infile, GFile *outfile, 
    PAGELIST *pagelist, int copies)
{
    int i;
    int pages = 0;
    int neworder;
    BOOL reverse;
    DSC_OFFSET position;
    char line[DSC_LINE_LENGTH+1];
    char buf[MAXSTR];
    BOOL end_header;
    BOOL line_written;
    BOOL pages_written = FALSE;
    BOOL pageorder_written = FALSE;
    CDSC *dsc = doc->dsc;
    int page;
    int count;

    if (dsc == NULL)
	return_error(-1);
    if ((pagelist->page_count == 0) || (pagelist->select == NULL))
	return_error(-1);	/* can't do */
    if (pagelist->page_count != (int)dsc->page_count)
	return_error(-1);
    for (i=0; i<pagelist->page_count; i++) {
	if (pagelist->select[i]) 
	    pages++;
    }

    /* neworder = page order of the extracted document */
    /* reverse = reverse the current page order */
    neworder = dsc->page_order;
    reverse = FALSE;
    if (neworder == CDSC_ORDER_UNKNOWN)	/* No page order so assume ASCEND */
	neworder = CDSC_ASCEND;
    /* Don't touch SPECIAL pageorder */
    if (pagelist->reverse) {
        /* New page order to be DESCEND */
	if (neworder == CDSC_ASCEND)
	    neworder = CDSC_DESCEND;
	else if (neworder == CDSC_DESCEND) {
	    /* neworder = DESCEND;*/	/* unchanged */
	    reverse = FALSE;	/* already reversed, don't do it again */
	}
    }
    else {
	if (neworder == CDSC_DESCEND) {
	    neworder = CDSC_ASCEND;
	    reverse = TRUE;	/* reverse it to become ascending */
	}
    }

    /* copy header, fixing up %%Pages: and %%PageOrder:
     * Write a DSC 3.0 %%Pages: or %%PageOrder: in header,
     * even if document was DSC 2.x.
     */
    position = gfile_seek(infile, dsc->begincomments, gfile_begin);
    while ( position < dsc->endcomments ) {
	if (ps_fgets(line, min(sizeof(line), dsc->endcomments - position), 
	    infile) == 0)
	    return FALSE;
	position = gfile_get_position(infile);
	end_header = (strncmp(line, "%%EndComments", 13) == 0);
	if ((line[0] != '%') && (line[0] != ' ') && (line[0] != '+')
	 && (line[0] != '\t') && (line[0] != '\r') && (line[0] != '\n'))
	    end_header = TRUE;
	line_written = FALSE;
	if (end_header || strncmp(line, "%%Pages:", 8) == 0) {
	    if (!pages_written) {
		snprintf(buf, sizeof(buf)-1, "%%%%Pages: %d\r\n", pages);
	        ps_writestring(outfile, buf);
		pages_written = TRUE;
	    }
	    line_written = !end_header;
	}
	if (end_header || strncmp(line, "%%PageOrder:", 12) == 0) {
	    if (!pageorder_written) {
		if (neworder == CDSC_ASCEND)
		    ps_writestring(outfile, "%%PageOrder: Ascend\r\n");
		else if (neworder == CDSC_DESCEND)
		    ps_writestring(outfile, "%%PageOrder: Descend\r\n");
		else 
		    ps_writestring(outfile, "%%PageOrder: Special\r\n");
		pageorder_written = TRUE;
	    }
	    line_written = !end_header;
	}
	if (!line_written)
	    ps_writestring(outfile, line);	
    }

    if (dsc->begincomments != dsc->endcomments) {
	if (!pages_written) {
	    snprintf(buf, sizeof(buf)-1, "%%%%Pages: %d\r\n", pages);
	    ps_writestring(outfile, buf);	
	    /* pages_written = TRUE; */
	}
	if (!pageorder_written) {
	    if (neworder == CDSC_ASCEND)
		ps_writestring(outfile, "%%PageOrder: Ascend\r\n");
	    else if (neworder == CDSC_DESCEND)
		ps_writestring(outfile, "%%PageOrder: Descend\r\n");
	    else 
		ps_writestring(outfile, "%%PageOrder: Special\r\n");
	    /* pageorder_written = TRUE; */
	}
    }

    ps_copy(outfile, infile, dsc->beginpreview, dsc->endpreview);
    ps_copy(outfile, infile, dsc->begindefaults, dsc->enddefaults);
    ps_copy(outfile, infile, dsc->beginprolog, dsc->endprolog);
    ps_copy_setup(outfile, infile, dsc->beginsetup, dsc->endsetup, copies);

    /* Copy each page */
    page = 1;
    i = reverse ? dsc->page_count - 1 : 0;
    while ( reverse ? (i >= 0)  : (i < (int)dsc->page_count) ) {
	if (pagelist->select[doc_map_page(doc,i)])  {
	    gfile_seek(infile, dsc->page[i].begin, gfile_begin);
	    position = gfile_get_position(infile);
	    /* Read original %%Page: line */
	    if ((count = ps_fgets(line, sizeof(line)-1, infile)) == 0)
	        return -1;
	    /* Write new %%Page: line with new ordinal */
	    if (dsc->page[i].label)
		snprintf(buf, sizeof(buf)-1,  "%%%%Page: %s %d\r\n", 
		    dsc->page[i].label, page);
	    else
		snprintf(buf, sizeof(buf)-1,  
		    "%%%%Page: %d %d\r\n", page, page);
	    ps_writestring(outfile, buf);
	    page++;
	    if (strncmp(line, "%%Page:", 7) != 0)
		gfile_write(outfile, line, count);
	    ps_copy(outfile, infile, -1, dsc->page[i].end);
	}
        i += reverse ? -1 : 1;
    }

    /* copy trailer, removing %%Pages: and %%PageOrder: */
    gfile_seek(infile, dsc->begintrailer, gfile_begin);
    position = gfile_get_position(infile);
    while ( position < dsc->endtrailer ) {
	if (ps_fgets(line, min(sizeof(line), dsc->endtrailer - position), 
	    infile) == 0)
	    return -1;
	position = gfile_get_position(infile);
	if (strncmp(line, "%%Pages:", 8) == 0) {
	    continue;	/* has already been written in header */
	}
	else if (strncmp(line, "%%PageOrder:", 12) == 0) {
	    continue;	/* has already been written in header */
	}
	else {
	    ps_writestring(outfile, line);	
	}
    }
    return 0;
}



/* Extract pages from a PostScript file.
 * If copies > 1, insert PostScript code to request printing 
 * copies of each page.
 * Used for saving a page that is being displayed,
 * or in preparation for printing.
 * Returns 0 if OK, -ve if error.
 */
int
ps_extract(Doc *doc, LPCTSTR filename, PAGELIST *pagelist, int copies)
{
    GFile *infile, *outfile;
    CDSC *dsc;
    int code;

    if (doc_type(doc) == DOC_PDF)
	return -1;
    if ((infile = gfile_open(doc_name(doc), gfile_modeRead)) == (GFile *)NULL) {
	app_msg(doc->app, "File \042");
	app_csmsg(doc->app, doc_name(doc));
	app_msg(doc->app, "\042 does not exist\n");
	return_error(-1);
    }
    if ( (outfile = gfile_open(filename, gfile_modeWrite | gfile_modeCreate)) 
	== (GFile *)NULL ) {
	app_msg(doc->app, "File \042");
	app_csmsg(doc->app, filename);
	app_msg(doc->app, "\042 can not be opened for writing\n");
	gfile_close(infile);
	return_error(-1);
    }

    if (pagelist == NULL) {
	/* Save file without changes */
	code = ps_copy(outfile, infile, 0, gfile_get_length(infile));
    }
    else if (doc->dsc && doc->dsc->page_count &&
	pagelist->page_count && (pagelist->select != NULL)) {
	/* DSC with pages */
	code = ps_extract_pages(doc, infile, outfile, pagelist, copies);
    }
    else if (doc->dsc) {
	/* DSC without pages */
	dsc = doc->dsc;
	code = ps_copy(outfile, infile, dsc->begincomments, dsc->endcomments);
	if (!code)
	  code = ps_copy(outfile, infile, dsc->begindefaults, dsc->enddefaults);
	if (!code)
	  code = ps_copy(outfile, infile, dsc->beginprolog, dsc->endprolog);
	if (!code)
	  code = ps_copy_setup(outfile, infile, dsc->beginsetup, dsc->endsetup,
		copies);
	if (!code)
	  code = ps_copy(outfile, infile, dsc->begintrailer, dsc->endtrailer);
    }
    else {
	/* Not DSC */
	if (copies > 1)
	    ps_num_copies(outfile, copies);
	code = ps_copy(outfile, infile, 0, gfile_get_length(infile));
    }

    gfile_close(outfile);
    gfile_close(infile);
    if (code && !(debug & DEBUG_GENERAL))
	csunlink(filename);
    return code;
}


