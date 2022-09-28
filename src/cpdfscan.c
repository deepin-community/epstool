/* Copyright (C) 2002-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cpdfscan.c,v 1.7 2005/06/10 09:39:24 ghostgum Exp $ */
/* PDF scanner */

/* This is a rudimentary PDF scanner, intended to get
 * the page count, and for each page the Rotate, MediaBox 
 * and CropBox.
 */

#ifdef DEMO_PDFSCAN
# include <windows.h>
# include <stdio.h>
# include <stdarg.h>
# include <string.h>
# include <ctype.h>
# ifdef _MSC_VER
#  define vsnprintf _vsnprintf
# endif
# define csfopen fopen
# define cslen strlen
#else
# include "common.h"
# include <ctype.h>
#endif

#include "cpdfscan.h"


/* Limitations.
 * 
 * We currently load the entire xref table.  To minimise memory
 * would could instead keep a list of xref blocks, and do random
 * access within those.
 *
 * Memory management is very simple.  We just keep a linked
 * list of allocated blocks for composite objects.
 * We empty the stack, and free all PDF objects and composite
 * objects before returning to the caller.
 * We don't bother doing garbage collection.
 */


/* We keep a linked list of memory allocated for composite objects 
 * such as name, string, array or dict.
 */
typedef struct PDFMEM_s PDFMEM;
struct PDFMEM_s {
    void *ptr;
    int len;
    PDFMEM *next;
};
 
/* The token scanner and object references understand the following types */
typedef enum rtype_e {
    invalidtype=0,
    marktype=1,
    nulltype=2,
    booltype=3,		/* uses boolval */
    integertype=4,	/* uses intval */
    realtype=5,		/* uses realval */
    nametype=6,		/* uses nameval */
    stringtype=7,	/* uses strval */
    arraytype=8,	/* uses arrayval */
    dicttype=9,		/* uses dictval */
    optype=10,		/* uses opval */
    streamtype=11,	/* uses streamval */
    objtype=12,		/* uses objval */
    commenttype=13
} rtype;

const char *rtype_string[] = {
    "invalidtype", "marktype", "nulltype", "booltype", "integertype",
    "realtype", "nametype", "stringtype", "arraytype", "dicttype",
    "optype", "streamtype", "objtype", "commenttype"
};

/* A reference contains a simple object, or a pointer to 
 * a composite object.
 */
typedef struct ref_s ref;
struct ref_s {
    rtype type;
    int rsize;
    union value_u {
	/* simple */
        void *voidval;
	BOOL boolval;
	int intval;
	float realval;
	/* composite */
	char *nameval;
	char *strval;
	ref *arrayval;
	ref *dictval;
	char *opval;
	/* simple */
	unsigned long streamval;
	int objval;
    } value;
};

/* Cross reference table entry */
typedef struct PDFXREF_s {
    unsigned long offset;
    int generation;
    BOOL used;
} PDFXREF;

struct PDFSCAN_s {
    void *handle;
    int (*print_fn)(void *handle, const char *ptr, int len);
    TCHAR filename[1024];
    FILE *file;
    char *buf;
    int buflen;		/* length of allocated buf */
    int len;		/* #bytes currently in buf */
    int offset;		/* file offset to start of buf */
    int begin;		/* offset in buf to start of token */
    int end;		/* offset in buf to end of token */
    rtype token_type;	/* token type */
    BOOL instream;	/* In a stream, looking for endstream */
    unsigned long xref_offset;	/* offset to xref table */
    PDFXREF *xref;
    int xref_len;

    /* Object numbers obtained during pdf_scan_open() */
    int root;		/* root object reference */
    int info;		/* document info dicionary reference */
    int pages;		/* Pages dictionary reference */
    int page_count;	/* number of pages */

    /* Cached page media */
    int pagenum;
    int rotate;
    PDFBBOX mediabox;
    PDFBBOX cropbox;

    /* memory allocation */
    PDFMEM *memory_head;
    PDFMEM *memory_tail;

    /* operand stack */
    ref *ostack;
    int ostack_idx;	/* index to top of ostack */
    int ostack_len;	/* Initially 512 */
    int ostack_maxlen;	/* maximum depth of ostack */

    /* objects in memory */
    /* This contains pairs of integer & reference */
    ref *objs;
    int objs_count;	/* count of loaded objects */
    int objs_len;	/* length of objs */
    int objs_maxlen;	/* maximum number entries in objs */
};

typedef enum PDFSEEK_e {
    PDFSEEK_CUR,
    PDFSEEK_END,
    PDFSEEK_SET
} PDFSEEK;


/* Prototypes */
static int pdf_scan_next_token(PDFSCAN *ps);
static int pdf_scan_read_trailer(PDFSCAN *ps, unsigned long *prev);
static int pdf_scan_read_xref(PDFSCAN *ps, unsigned long xref_offset);

static void clear_stack(PDFSCAN *ps);
static void clear_objs(PDFSCAN *ps);
static void pdf_scan_freeall(PDFSCAN *ps);
static void pdf_scan_cleanup(PDFSCAN *ps);
static int pdf_scan_open_file(PDFSCAN *ps);


/*****************************************************************/
/* text message output */

static int
pdf_scan_write(PDFSCAN *ps, const char *str, int len)
{
    if (ps != NULL)
        fwrite(str, 1, len, stdout);
    else
	(*ps->print_fn)(ps->handle, str, len);
    return len;
}

static int
pdf_scan_msgf(PDFSCAN *ps, const char *fmt, ...)
{
va_list args;
int count;
char buf[2048];
    va_start(args,fmt);
    count = vsnprintf(buf, sizeof(buf), fmt, args);
    pdf_scan_write(ps, buf, count);
    va_end(args);
    return count;
}

/*****************************************************************/
/* memory allocation */

static void
pdf_scan_cleanup(PDFSCAN *ps)
{
    if (ps->file)
	fclose(ps->file);
    ps->file = NULL;
    clear_stack(ps);
    clear_objs(ps);
    pdf_scan_freeall(ps);
}

static void *pdf_scan_alloc(PDFSCAN *ps, const void *ptr, int len)
{
    void *data;
    PDFMEM *mem = (PDFMEM *)malloc(sizeof(PDFMEM));
    if (mem == NULL)
	return NULL;

    data = malloc(len);
    if (data == NULL) {
	free(mem);
	return NULL;
    }

    mem->ptr = data;
    mem->next = NULL;
    mem->len = len;
    memcpy(data, ptr, len);

    if (ps->memory_tail) {
	ps->memory_tail->next = mem;
	ps->memory_tail = mem;
    }
    else
	ps->memory_head = ps->memory_tail = mem;
    return data;
}

/* free all name/string/array/dict memory */
static void
pdf_scan_freeall(PDFSCAN *ps)
{
    PDFMEM *memnext;
    PDFMEM *mem = ps->memory_head;
    while (mem) {
	memnext = mem->next;
	free(mem->ptr);
	free(mem);
	mem = memnext;
    }
    ps->memory_head = ps->memory_tail = NULL;
}

/*****************************************************************/
/* Token checks */

static BOOL is_optoken(PDFSCAN *ps, const char *str)
{
    return (ps->token_type == optype) && 
	(ps->end-ps->begin == (int)strlen(str)) && 
	(memcmp(ps->buf+ps->begin, str, ps->end-ps->begin) == 0);
}

static int
type_check(PDFSCAN *ps, rtype type)
{
    if (ps->token_type == type)
	return 0;

    pdf_scan_msgf(ps, "Error at offset %ld.  Expecting %s and found %s\n",
	ps->offset + ps->begin, 
	rtype_string[(int)type],
	rtype_string[(int)ps->token_type]);
    pdf_scan_msgf(ps, "Token is \042");
    pdf_scan_write(ps, ps->buf+ps->begin, ps->end-ps->begin);
    pdf_scan_msgf(ps, "\042\n");
    return -1;
}

static int
op_check(PDFSCAN *ps, const char *str)
{
    int code = type_check(ps, optype);
    if (code)
	return code;

    if (!is_optoken(ps, str)) {
	pdf_scan_msgf(ps, 
	    "Error at offset %ld.  Expecting \042%s\042 and found \042",
	    ps->offset + ps->begin, str); 
	pdf_scan_write(ps, ps->buf+ps->begin, ps->end-ps->begin);
	pdf_scan_msgf(ps, "\042\n");
	code = -1;
    }
    return code;
}

/*****************************************************************/
/* stack */

const ref invalidref = {invalidtype, 0, {NULL}};
const ref markref = {marktype, 0, {NULL}};

/* Push item, return depth of stack */
/* >0 is success, <=0 is failure */
static int push_stack(PDFSCAN *ps, ref r)
{
    int idx;
    if (ps->ostack_idx + 1 >= ps->ostack_len) {
	/* increase stack size */
	ref *newstack;
	int newlen = ps->ostack_len + 256;
	if (newlen > ps->ostack_maxlen) {
	    pdf_scan_msgf(ps, "push_stack: stack overflow\n");
	    return 0;
	}
	newstack = (ref *)malloc(newlen * sizeof(ref));
	if (newstack == NULL) {
	    pdf_scan_msgf(ps, "push_stack: Out of memory\n");
	    return 0;
	}
	memcpy(newstack, ps->ostack, ps->ostack_len * sizeof(ref));
	free(ps->ostack);
	ps->ostack = newstack;
	ps->ostack_len = newlen;
    }
    idx = ++(ps->ostack_idx);
    ps->ostack[idx] = r;
    return idx;
}

static ref pop_stack(PDFSCAN *ps)
{
    if (ps->ostack_idx <= 0) {
	pdf_scan_msgf(ps, "pop_stack: stack underflow\n");
	return invalidref;
    }
    return ps->ostack[ps->ostack_idx--];
}

static void clear_stack(PDFSCAN *ps)
{
    ps->ostack_idx = 0;
}

static ref index_stack(PDFSCAN *ps, int n)
{
    if (n < 0) {
	pdf_scan_msgf(ps, "index_stack: index must not be negative\n");
	return invalidref;
    }
    if (ps->ostack_idx <= n) {
	pdf_scan_msgf(ps, "index_stack: stack isn't that deep\n");
	return invalidref;
    }
    return ps->ostack[ps->ostack_idx-n];
}

static ref top_stack(PDFSCAN *ps)
{
    if (ps->ostack_idx <= 0) {
	pdf_scan_msgf(ps, "top_stack: stack is empty\n");
	return invalidref;
    }
    return ps->ostack[ps->ostack_idx];
}

/*****************************************************************/
/* references */


static ref make_int(int value)
{
    ref r;
    r.type = integertype;
    r.rsize = 0;
    r.value.intval = value;
    return r;
}

static ref make_string(PDFSCAN *ps, const char *str, int len)
{
    ref r;
    r.type = stringtype;
    r.rsize = len;
    r.value.strval = pdf_scan_alloc(ps, str, len);
    if (r.value.strval == NULL)
	return invalidref;
    return r;
}

static ref make_name(PDFSCAN *ps, const char *str, int len)
{
    ref r;
    r.type = nametype;
    r.rsize = len;
    r.value.nameval = pdf_scan_alloc(ps, str, len);
    if (r.value.nameval == NULL)
	return invalidref;
    return r;
}

static BOOL nameref_equals(ref *r, const char *name)
{
    int len = (int)strlen(name);
    if (r->type != nametype)
	return FALSE;
    if (r->rsize != len)
	return FALSE;
    return (memcmp(r->value.nameval, name, len) == 0);
}

/* Get a reference from a dictionary */
/* Return the result, but don't push it */
static ref dict_get(PDFSCAN *ps, const char *name)
{
    int namelen = (int)strlen(name);
    ref dict = top_stack(ps);
    ref *r;
    int dictlen;
    int i;
    if (dict.type == invalidtype)
	return invalidref;
    dictlen = dict.rsize * 2;
    for (i = 0; i<dictlen; i+=2) {
	r = &dict.value.dictval[i];
	if ((r->rsize == namelen) && (r->type == nametype) &&
	    (memcmp(r->value.nameval, name, namelen) ==0))
	    return dict.value.dictval[i+1];
    }
    return invalidref;
}

/* convert the items on the stack to an array on the stack */
static ref array_to_mark(PDFSCAN *ps)
{
    ref r;
    ref *array;
    int n = ps->ostack_idx;
    int len;
    while ((n>0) && (ps->ostack[n].type != marktype))
	n--;
    if (n == 0) {
	pdf_scan_msgf(ps, "array_to_mark: no mark on stack\n");
	return invalidref;
    }
    len = ps->ostack_idx - n;
    r.type = arraytype;
    r.rsize = len;
    r.value.arrayval = NULL;
    if (len) {
        array = pdf_scan_alloc(ps, &ps->ostack[n+1], len * sizeof(ref));
	if (array)
	    r.value.arrayval = array;
	else
	    return invalidref;
    }
    ps->ostack_idx -= len + 1;
    push_stack(ps, r);
    return r;
}

/* convert the items on the stack to a dictionary on the stack */
static ref dict_to_mark(PDFSCAN *ps)
{
    ref r;
    ref *dict;
    int n = ps->ostack_idx;
    int len;
    while ((n>0) && (ps->ostack[n].type != marktype))
	n--;
    if (n == 0) {
	pdf_scan_msgf(ps, "dict_to_mark: no mark on stack\n");
	return invalidref;
    }
    len = ps->ostack_idx - n;
    if (len & 1) {
	pdf_scan_msgf(ps, "dict_to_mark: must have name/value pairs\n");
	return invalidref;
    }
    r.type = dicttype;
    r.rsize = len/2;
    r.value.arrayval = NULL;
    if (len) {
        dict = pdf_scan_alloc(ps, &ps->ostack[n+1], len * sizeof(ref));
	if (dict)
	    r.value.arrayval = dict;
	else
	    return invalidref;
    }
    ps->ostack_idx -= len + 1;
    push_stack(ps, r);
    return r;
}

/*****************************************************************/

/* Push reference from a token */
static ref push_token(PDFSCAN *ps)
{
    ref r;
    int len = ps->end - ps->begin;
    const char *p = ps->buf + ps->begin;
    r.type = ps->token_type;
    r.rsize = 0;
    r.value.voidval = NULL;
    switch(r.type) {
      case invalidtype:
	break;
      case marktype:
	break;
      case nulltype:
	break;
      case booltype:
	if ((len == 4) && (memcmp(p, "true", 4)==0))
	    r.value.boolval = TRUE;
	else if ((len == 5) && (memcmp(p, "true", 5)==0))
	    r.value.boolval = FALSE;
	else
	    r = invalidref;
	break;
      case integertype:
	{   char buf[64];
	    if (len > (int)sizeof(buf)-1)
		r = invalidref;
	    else {
		memcpy(buf, p, len);
		buf[len] = '\0';
		r.value.intval = atoi(buf);
	    }
	}
	break;
      case realtype:
	{   char buf[64];
	    if (len > (int)sizeof(buf)-1)
		r = invalidref;
	    else {
		memcpy(buf, p, len);
		buf[len] = '\0';
		r.value.realval = (float)atof(buf);
	    }
	}
	break;
      case nametype:
	r = make_name(ps, p+1, len-1);
	break;
      case stringtype:
	r = make_string(ps, p, len);
	break;
      case streamtype:
      case commenttype:
      case objtype:
      case optype:
      case arraytype:
      case dicttype:
	/* Can't push these from a token */
	/* These are made by operators like stream, R, ], >> */
	return invalidref;
      default:
	r.type = invalidtype;
	break;
    }
    push_stack(ps, r);
    return r;
}

/* Process known operators */
static int process_op(PDFSCAN *ps)
{
   ref r;
   if (ps->token_type != optype)
	return 1;	/* not an op */
   if (is_optoken(ps, "R")) {
	/* convert "n 0 R" to an indirect reference */
	ref r1 = index_stack(ps, 1);
	r = top_stack(ps);
	if ((r.type == integertype) && (r1.type == integertype)) {
	    r.type = objtype;
	    r.rsize = r.value.intval;
	    r.value.intval = r1.value.intval;
	    pop_stack(ps);
	    pop_stack(ps);
	    push_stack(ps, r);
	}
   }
   else if (is_optoken(ps, "]")) {
	array_to_mark(ps);
   }
   else if (is_optoken(ps, ">>")) {
	dict_to_mark(ps);
   }
   else if (is_optoken(ps, "null")) {
	r.type = nulltype;
	r.rsize = 0;
	r.value.voidval = NULL;
	push_stack(ps, r);
   }
   else if (is_optoken(ps, "obj")) {
	pdf_scan_msgf(ps, "ignoring obj token\n");
	/* ignore */
   }
   else if (is_optoken(ps, "endobj")) {
	pdf_scan_msgf(ps, "ignoring endobj token\n");
	/* ignore */
   }
   else if (is_optoken(ps, "stream")) {
	/* stream object contains offset to start of stream */
	r.type = streamtype;
	r.rsize = 0;
	r.value.streamval = ps->offset + ps->end;
	push_stack(ps, r);
	/* Now skip over stream */
        pdf_scan_next_token(ps);
   }
   else {
	pdf_scan_msgf(ps, "process_op: unrecognised operator \042");
	pdf_scan_write(ps, ps->buf+ps->begin, ps->end-ps->begin);
	pdf_scan_msgf(ps, "\042\n");
	return -1;
   }
   return 0;
}

/*****************************************************************/
/* Debugging and error messages */

#ifdef NOTUSED

/* Print a reference, returning number of characters written */
static int
print_ref(PDFSCAN *ps, ref *r)
{
    int n = 0;
    switch(r->type) {
      case invalidtype:
	n = pdf_scan_msgf(ps, "--invalid--");
	break;
      case marktype:
	n = pdf_scan_msgf(ps, "--mark--");
	break;
      case nulltype:
	n = pdf_scan_msgf(ps, "--null--");
	break;
      case booltype:
	n = pdf_scan_msgf(ps, "%s", r->value.boolval ? "true" : "false");
	break;
      case integertype:
	n = pdf_scan_msgf(ps, "%d", r->value.intval);
	break;
      case realtype:
	n = pdf_scan_msgf(ps, "%g", r->value.realval);
	break;
      case nametype:
	n = pdf_scan_write(ps, "/", 1);
	pdf_scan_write(ps, r->value.nameval, r->rsize);
	break;
      case stringtype:
	n = pdf_scan_write(ps, "(", 1);
	n += pdf_scan_write(ps, r->value.strval, r->rsize);
	n += pdf_scan_write(ps, ")", 1);
	break;
      case streamtype:
	n = pdf_scan_msgf(ps, "--stream:%d--", r->value.streamval);
	break;
      case commenttype:
	n = pdf_scan_msgf(ps, "--comment--");
	break;
      case objtype:
	n = pdf_scan_msgf(ps, "--obj:%d--", r->value.objval);
	break;
      case optype:
	n = pdf_scan_msgf(ps, "--op:");
	n += pdf_scan_write(ps, r->value.opval, r->rsize);
	n += pdf_scan_write(ps, "--", 2);
	break;
      case arraytype:
	n = pdf_scan_msgf(ps, "--array:%d--", r->rsize);
	break;
      case dicttype:
	n = pdf_scan_msgf(ps, "--dict:%d--", r->rsize);
	break;
      default:
	n = pdf_scan_msgf(ps, "--unknown--");
	break;
    }
    return n;
}

/* print a reference, expanding array and dict */ 
static int
print_ref_expand(PDFSCAN *ps, ref *r)
{
    int i;
    int n = 0;;
    if (r->type == arraytype) {
	n += pdf_scan_msgf(ps, "[ ");
	for (i=0; i<r->rsize; i++) {
	    n += print_ref(ps, &r->value.arrayval[i]);
	    n += pdf_scan_msgf(ps, " ");
	}
	n += pdf_scan_msgf(ps, "]");
    }
    else if (r->type == dicttype) {
	n += pdf_scan_msgf(ps, "<< ");
	for (i=0; i<r->rsize; i++) {
	    n += print_ref(ps, &r->value.dictval[i+i]);
	    n += pdf_scan_msgf(ps, " ");
	    n += print_ref(ps, &r->value.dictval[i+i+1]);
	    n += pdf_scan_msgf(ps, " ");
	}
	n += pdf_scan_msgf(ps, ">>");
    }
    else
	n += print_ref(ps, r);
    return n;
}

static void
print_stack(PDFSCAN *ps)
{
    int i, n=ps->ostack_idx;
    int col = 0;
    pdf_scan_msgf(ps, "Stack: ");
    for (i=1; i<=n; i++) {
	col += print_ref(ps, &ps->ostack[i]);
	if (col > 70) {
            pdf_scan_msgf(ps, "\n");
	    col = 0;
	}
	else
            col += pdf_scan_msgf(ps, " ");
    }
    pdf_scan_msgf(ps, "\n");
}

static void
print_stack_expand(PDFSCAN *ps)
{
    int i, n=ps->ostack_idx;
    pdf_scan_msgf(ps, "Stack:\n");
    for (i=1; i<=n; i++) {
        pdf_scan_msgf(ps, "%2d: ", i);
	print_ref_expand(ps, &ps->ostack[i]);
        pdf_scan_msgf(ps, "\n");
    }
}

static void pdf_scan_print_allocated(PDFSCAN *ps)
{
    int count = 0;
    int len = 0;
    PDFMEM *mem = ps->memory_head;
    while (mem) {
	len += sizeof(PDFMEM);
	len += mem->len;
	count++;
	mem = mem->next;
    }
    pdf_scan_msgf(ps, "Allocated memory %d bytes in %d objects\n", 
	len, count);
}

#endif

/*****************************************************************/
/* object reading and cache */

static int obj_add(PDFSCAN *ps, int objnum, ref objref)
{
    if (ps->objs_count + 2 >= ps->objs_len) {
	/* allocate more space */
	ref *newobjs;
	int newlen = ps->objs_len + 256;
	if (newlen > ps->objs_maxlen) {
	    pdf_scan_msgf(ps, "obj_add: too many objects to cache\n");
	    return 0;
	}
	newobjs = (ref *)malloc(newlen * sizeof(ref));
	if (newobjs == NULL) {
	    pdf_scan_msgf(ps, "obj_add: Out of memory\n");
	    return 0;
	}
	memcpy(newobjs, ps->objs, ps->objs_len * sizeof(ref));
	free(ps->objs);
	ps->objs = newobjs;
	ps->objs_len = newlen;
    }
    ps->objs[ps->objs_count++] = make_int(objnum);
    ps->objs[ps->objs_count++] = objref;
    return ps->objs_count;
}

static ref obj_find(PDFSCAN *ps, int objnum)
{
    int i;
    for (i=0; i<ps->objs_count; i+=2) {
	if (objnum == ps->objs[i].value.intval)
	    return ps->objs[i+1];
    }
    return invalidref;
}

static void clear_objs(PDFSCAN *ps)
{
    ps->objs_count = 0;
}

/*****************************************************************/
/* token parsing */

static int is_white(char ch)
{
    return (ch == '\0') || (ch == '\t') || (ch == '\n') ||
	(ch == '\f') || (ch == '\r') || (ch == ' ');
}

static int is_delimiter(char ch)
{
    return (ch == '(') || (ch == ')') || 
	(ch == '<') || (ch == '>') ||
	(ch == '[') || (ch == ']') ||
	(ch == '{') || (ch == '}') ||
	(ch == '/') || (ch == '%');
}


/* Scan next token from buffer, returning token type and offset to begin 
 * and end of token.
 * Return 0 if OK, 1 if no token or not enough data, -1 on error
 */
static int pdf_scan_token(const char *buf, int buflen, 
    rtype *ttype, int *tbegin, int *tend)
{
    int code = -1;
    int i = 0;
    rtype type;
    int begin, end;
    *ttype = type = invalidtype;
    *tbegin = begin = 0;
    *tend = end = 0;
    while ((i < buflen) && is_white(buf[i]))
	i++;
    if (i == buflen)
	return 1;

    begin = i;
    if (buf[i] == '%') {
	while (i < buflen) {
	    if ((buf[i] == '\n') || (buf[i] == '\r')) {
		type = commenttype;
		end = i;
		code = 0;
		break;
	    }
	    i++;
	}
        if (i >= buflen)
	    code = 1;

    }
    else if (buf[i] == '(') {
	/* string */
	int pcount = 0;
	type = stringtype;
	i++;
	while (i < buflen) {
	    if (buf[i] == '\\')
		i++;
	    else if (buf[i] == '(')
		pcount++;
	    else if (buf[i] == ')') {
		if (pcount <= 0) {
		    end = i+1;
		    code = 0;
		    break;
		}
		else
		    pcount--;
	    }
	    i++;
	}
	if (i >= buflen)
	    code = 1;
    }
    else if (buf[i] == '<') {
	i++;
	if (i >= buflen) {
	    code = 1;
	}
	else if (buf[i] == '<') {
	    /* marktype */
	    end = i+1;
	    type = marktype;
	    code = 0;
	}
	else {
	    /* hexadecimal string */
	    type = stringtype;
	    while (i < buflen) {
		if (buf[i] == '>') {
		    end = i+1;
		    code = 0;
		    break;
		}
		i++;
	    }
	    if (i >= buflen)
		code = 1;
	}
    }
    else if (buf[i] == '[') {
	code = 0;
	end = i+1;
	type = marktype;
    }
    else if (buf[i] == '/') {
	/* name */
	type = nametype;
	i++;
	while (i < buflen) {
	    if (is_white(buf[i]) || is_delimiter(buf[i])) {
		end = i;
		code = 0;
		break;
	    }
	    i++;
	}
	if (i >= buflen)
	    code = 1;
    }
    else if (is_delimiter(buf[i])) {
	/* skip over delimiter */
	if (buf[i] == '>') {
	    i++;
	    if (i < buflen) {
		if (buf[i] == '>') {
		    type = optype;
		    end = i+1;
		    code = 0;
		}
		else
		    code = -1;
	    }
	}
	else {
	    type = optype;
	    end = i+1;
	    code = 0;
	}
	if (i >= buflen)
	    code = 1;
    }
    else {
	/* First assume that it is an op */
	type = optype;
	while (i < buflen) {
	    if (is_white(buf[i]) || is_delimiter(buf[i])) {
		end = i;
		code = 0;
		break;
	    }
	    i++;
	}
	if (i >= buflen)
	    code = 1;

	/* try to convert it into a bool */
	if ((code == 0) && (type == optype)) {
	    if ((end - begin == 4) && 
		(memcmp(buf+begin, "true", 4) == 0)) {
		type = booltype;
	    }
	    else if ((end - begin == 5) && 
		(memcmp(buf+begin, "false", 5) == 0)) {
		type = booltype;
	    }
	}

	/* try to convert it into an integer */
	if ((code == 0) && (type == optype)) {
	    int j;
	    char ch;
	    BOOL isreal = FALSE;
	    BOOL isnum = TRUE;
	    for (j=begin; j<end; j++) {
		ch = buf[j];
		if (ch == '.')
		    isreal = TRUE;
		if (!((ch == '-') || (ch == '+') || (ch == '.') || 
		    isdigit((int)ch)))
		    isnum = FALSE;
	    }
	    if (isnum) {
		if (isreal)
		    type = realtype;
		else
		    type = integertype;
	    }
	}
    }

    *ttype = type;
    *tbegin = begin;
    *tend = end;
    return code;
}

/*****************************************************************/

static void pdf_scan_finish(PDFSCAN *ps)
{
    if (ps->file) {
	fclose(ps->file);
	ps->file = NULL;
    }
    if (ps->buf) {
	free(ps->buf);
	ps->buf = NULL;
    }
    ps->buflen = 0;
    if (ps->xref) {
	free(ps->xref);
	ps->xref = NULL;
    }
    ps->xref_len = 0;
    if (ps->ostack) {
	free(ps->ostack);
	ps->ostack = NULL;
    }
    ps->ostack_len = 0;
    ps->ostack_idx = 0;

    if (ps->objs) {
	free(ps->objs);
	ps->objs = NULL;
    }
    ps->objs_len = 0;
    ps->objs_count = 0;
    memset(ps, 0, sizeof(PDFSCAN));
}

static int pdf_scan_open_file(PDFSCAN *ps)
{
    ps->file = csfopen(ps->filename, TEXT("rb"));
    if (ps->file == NULL)
	return -1;
    return 0;
}

static int pdf_scan_init(PDFSCAN *ps, const TCHAR *name)
{
    int len = (int)(cslen(name)+1) * sizeof(TCHAR);
    if (len > (int)sizeof(ps->filename))
	return -1;
    memcpy(ps->filename, name, len);
    if (pdf_scan_open_file(ps) != 0) 
	return -1;
    ps->buflen = 256;
    ps->buf = (char *)malloc(ps->buflen);
    if (ps->buf == NULL) {
	pdf_scan_finish(ps);
	return -2;
    }
    ps->ostack_maxlen = 4096;
    ps->ostack_len = 256;
    ps->ostack_idx = 0;	/* empty */
    ps->ostack = (ref *)malloc(ps->ostack_len * sizeof(ref));
    if (ps->ostack == NULL) {
	pdf_scan_finish(ps);
	return -2;
    }
    /* make first item on stack invalid */
    ps->ostack[0].type = invalidtype;
    ps->ostack[0].rsize = 0;
    ps->ostack[0].value.voidval = NULL;

    /* object cache */
    ps->objs_maxlen = 1024;
    ps->objs_len = 256;
    ps->objs_count = 0;	/* empty */
    ps->objs = (ref *)malloc(ps->objs_len * sizeof(ref));
    if (ps->objs == NULL) {
	pdf_scan_finish(ps);
	return -2;
    }

    ps->pagenum = -1;	/* no cached media info yet */

    return 0;
}

static int pdf_scan_seek(PDFSCAN *ps, long offset, PDFSEEK whence)
{
    int code = -1;
    switch (whence) {
	case PDFSEEK_CUR:
	    offset = ps->offset + ps->end + offset;
	case PDFSEEK_SET:
	    ps->begin = ps->end = ps->len = 0;
	    code = fseek(ps->file, offset, SEEK_SET);
	    ps->offset = offset;
	    break;
	case PDFSEEK_END:
	    code = fseek(ps->file, 0, SEEK_END);
	    ps->begin = ps->end = ps->len = 0;
	    ps->offset = ftell(ps->file);
	    break;
    }
    return code;
}

/* Read next token from PDF file */
/* Return 0 if OK, or -1 if EOF, -2 if error */
/* Set *token_type to token type */
static int pdf_scan_next_token(PDFSCAN *ps)
{
    int code = 0;
    int count;
    rtype type=invalidtype;
    int begin=0, end=0;

    do {
	if ((code == 1) && ps->end) {
	    /* move characters to front of buffer */
	    if (ps->len - ps->end)
		memmove(ps->buf, ps->buf+ps->end, ps->len - ps->end);
	    ps->offset += ps->end;
	    ps->len = ps->len - ps->end;
	    ps->begin = 0;
	    ps->end = 0;
	}

	if ((code == 1) && (ps->len >= ps->buflen)) {
	    /* increase buffer size */
	    char *newbuf;
	    int newbuflen = 2 * ps->buflen;
	    newbuf = (char *)malloc(newbuflen);
	    if (newbuf) {
		memcpy(newbuf, ps->buf, ps->buflen);
		free(ps->buf);
		ps->buf = newbuf;
		ps->buflen = newbuflen;
	    }
	    else {
		pdf_scan_msgf(ps, "Out of memory in pdf_scan_next_token\n");
		pdf_scan_msgf(ps, "Tried to realloc %d to %d\n",
		    ps->buflen, newbuflen);
		code = -2;
		break;
	    }
	}

	if ((code == 1) || (ps->len == 0)) {
	    count = (int)fread(ps->buf+ps->len, 1, ps->buflen-ps->len, 
		ps->file);
	    if (count == 0) {
		pdf_scan_msgf(ps, "EOF in pdf_scan_next_token\n");
		code = -1;
		break;
	    }
	    ps->len += count;
	}

	while (ps->instream) {
	    /* We are in a stream.  Keep reading until we find
	     * the endstream.  This isn't robust. It can be fooled 
	     * by "endstream" occuring within a stream.
	     */
	    while ((ps->end < ps->len) && (ps->buf[ps->end] != 'e'))
		ps->end++;
	    /* look for endstream */
	    if (ps->end + 9 >= ps->len) {
		code = 1;	/* need more */
		break;
	    }
	    if (memcmp(ps->buf+ps->end, "endstream", 9) == 0)
		ps->instream = FALSE;
	    else
		ps->end++;
	}
	if (!ps->instream)
	    code = pdf_scan_token(ps->buf+ps->end, ps->len - ps->end, 
		&type, &begin, &end);
    } while (code == 1);


    if (code == 0) {
	/* got a token */
	ps->begin = ps->end + begin;
	ps->end = ps->end + end;
	ps->token_type = type;

	if ((type == optype) && (ps->end-ps->begin == 6) &&
		(memcmp(ps->buf+ps->begin, "stream", 6) == 0))
	    ps->instream = TRUE;
    }

    return code;
}

/*****************************************************************/
/* Reading %%EOF, xref, traler */

static int
previous_line(const char *str, int len)
{
    int i = len-1;
    /* first skip over EOL */
    while ((i > 0) && ((str[i]=='\r') || (str[i]=='\n')))
	i--;
    while ((i > 0) && !((str[i]=='\r') || (str[i]=='\n')))
	i--;
    if (!((str[i]=='\r') || (str[i]=='\n')))
	return -1; /* didn't find a line */
    return i+1;
}

static int
pdf_scan_find_xref(PDFSCAN *ps)
{
    char buf[4096];
    int i, j;
    int code = -1;
    int count;
    pdf_scan_seek(ps, 0, PDFSEEK_END);
    count = min((int)sizeof(buf), ps->offset);
    pdf_scan_seek(ps, -count, PDFSEEK_CUR);
    count = (int)fread(buf, 1, sizeof(buf), ps->file);
    pdf_scan_seek(ps, 0, PDFSEEK_SET);
    if (count == 0)
	return -1;
    i = count - 5;
    while (i > 0) {
	/* Find %%EOF */
	if (memcmp(buf+i, "%%EOF", 5) == 0) {
	    code = 0;
	    break;
	}
	i--;
    }
    if (i == 0) {
	pdf_scan_msgf(ps, "Failed to find %%EOF\n");
	code = -1;
    }
    if (code == 0) {
	/* Look for xref table offset */
	j = previous_line(buf, i);
	if (j >= 0)
	    ps->xref_offset = atol(buf+j);
	else 
	    code = -1;
	i = j;
	if (ps->xref_offset == 0)
	    code = -1;
	if (code != 0)
	    pdf_scan_msgf(ps, "Failed to find cross reference table\n");
    }

    if (code == 0) {
	/* Look for "startxref" */
	j = previous_line(buf, i);
	if (j >= 0) {
	    if (memcmp(buf+j, "startxref", 9) != 0)
		code = -1;
	}
	else {
	    code = -1;
	}
	if (code != 0)
	    pdf_scan_msgf(ps, "Failed to find startxref\n");
    }
    return code;
}

/* Read a cross reference table */
/* This is called for each cross reference table */
static int
pdf_scan_read_xref(PDFSCAN *ps, unsigned long xref_offset)
{
    int code;
    int i;
    int first = 0;
    int count = 0;
    unsigned long prev = 0;
    unsigned long offset = 0;
    int generation = 0;
    BOOL used = FALSE;
    pdf_scan_seek(ps, xref_offset, PDFSEEK_SET);
    code = pdf_scan_next_token(ps);
    if (code == 0)
	code = op_check(ps, "xref");
    while (code == 0) {
        code = pdf_scan_next_token(ps);
        if ((code == 0) && is_optoken(ps, "trailer"))
	    break;	/* finished this xref table */
	if (code == 0) {
	    first = atoi(ps->buf + ps->begin);
            code = pdf_scan_next_token(ps);
	}
	if (code == 0) {
	    count = atoi(ps->buf + ps->begin);
	}
	if (code == 0) {
	    /* make sure there is enough space in the table */
	    if (first + count > ps->xref_len) {
		int len = (first + count) * sizeof(PDFXREF);
		PDFXREF *newxref = (PDFXREF *)malloc(len);
		if (newxref) {
		    memset(newxref, 0, len);
		    memcpy(newxref, ps->xref, ps->xref_len * sizeof(PDFXREF));
		    free(ps->xref);
		    ps->xref = newxref;
		    ps->xref_len = first + count;
		}
		else {
		    pdf_scan_msgf(ps, "pdf_scan_read_xref: out of memory\n");
		    code = -2;
		    break;
		}
	    }
	}
	for (i=first; i<first+count; i++) {
            code = pdf_scan_next_token(ps);
	    if (code == 0) {
		offset = atol(ps->buf+ps->begin);
                code = pdf_scan_next_token(ps);
	    }
	    if (code == 0) {
		generation = atoi(ps->buf+ps->begin);
                code = pdf_scan_next_token(ps);
	    }
	    if (code == 0) {
		if (is_optoken(ps, "n"))
		    used = TRUE;
		else if (is_optoken(ps, "f"))
		    used = FALSE;
		else
		    code = -1;
	    }
	    /* We don't deal correctly with generation.
	     * We assume that the first xref table that marks an
	     * object as used is the definitive reference.
	     */
	    if (code == 0) {
		if (!(ps->xref[i].used)) {
		    ps->xref[i].offset = offset;
		    ps->xref[i].generation = generation;
		    ps->xref[i].used = used;
		}
	    }
	}
    }

    if (code == 0) {
	code = pdf_scan_read_trailer(ps, &prev);
	if ((code == 0) && prev && prev != ps->xref_offset) {
	    /* read older xref and trailer */
	    code = pdf_scan_read_xref(ps, prev);
	}
    }

    return code;
}

/* Read a trailer */
static int
pdf_scan_read_trailer(PDFSCAN *ps, unsigned long *prev)
{
    int code = 0;
    ref p;
    code = pdf_scan_next_token(ps);
    if ((code == 0) && (ps->token_type != marktype))
	code = -1;
    push_token(ps);
    while (code == 0) {
        code = pdf_scan_next_token(ps);
	if (code != 0)
	    break;
	if (is_optoken(ps, "startxref")) {
	    if (ps->root == 0) {
	        p = dict_get(ps, "Root");
	        if (p.type == objtype)
		    ps->root = p.value.objval;
		else {
		    pdf_scan_msgf(ps, 
			"trailer /Root requires indirect reference\n");
		    code = -1;
		}
	    }
	    p = dict_get(ps, "Prev");
	    if (p.type == integertype)
		*prev = p.value.intval;
	    else if (p.type != invalidtype) {
		code = -1;
		pdf_scan_msgf(ps, "trailer /Prev requires integer\n");
	    }
	    break;
	}
	if (process_op(ps) != 0)
	    push_token(ps);
    }
    if (code != 0)
	pdf_scan_msgf(ps, "Error reading trailer\n");
    return code;
}


static int pdf_scan_read_object_start(PDFSCAN *ps, int objnum)
{
    int code = 0;
    int value = 0;
    if (objnum == 0) {
	pdf_scan_msgf(ps, "Object 0 is always unused\n");
	return -1;
    }
    if (objnum >= ps->xref_len) {
	pdf_scan_msgf(ps, "Object reference %d doesn't exist.  There are only %d objects\n", objnum, ps->xref_len);
	return -1;
    }
    if (!ps->xref[objnum].used) {
	pdf_scan_msgf(ps, "Object %d is unused\n", objnum);
	return -1;
    }
    pdf_scan_seek(ps, ps->xref[objnum].offset, PDFSEEK_SET);

    code = pdf_scan_next_token(ps);		/* object number */
    if (code == 0)
	code = type_check(ps, integertype);
    if (code == 0) {
	value = atoi(ps->buf+ps->begin);	/* object number */
	code = pdf_scan_next_token(ps); 	/* generation */
    }
    if (code == 0)
	code = type_check(ps, integertype);
    if (code == 0)
	code = pdf_scan_next_token(ps);   	/* obj */
    if (code == 0)
	code = op_check(ps, "obj");

    if (value != objnum) {
	pdf_scan_msgf(ps, "Didn't find object %d\n", objnum);
	return -1;
    }
    return code;
}

/*****************************************************************/

/* Read an object, and leave it on the stack */
static int
pdf_scan_read_object(PDFSCAN *ps, int objnum)
{
    int code;
    ref objref = obj_find(ps, objnum);

    if (objref.type != invalidtype) {
	/* found in cache */
	push_stack(ps, objref);
	return 0;
    }

    code = pdf_scan_read_object_start(ps, objnum);
    if (code) {
	pdf_scan_msgf(ps, "Didn't find object %d\n", objnum);
	return -1;
    }

    code = pdf_scan_next_token(ps);
    if ((code == 0) && (ps->token_type != marktype))
	code = -1;
    push_token(ps);
    while (code == 0) {
        code = pdf_scan_next_token(ps);
	if (code != 0)
	    break;
	if (is_optoken(ps, "endobj")) {
	    obj_add(ps, objnum, top_stack(ps));
	    break;
	}
	if (process_op(ps) != 0)
	    push_token(ps);
    }
    return code;
}

/*****************************************************************/

/* find the object number for a page */
/* Return <= 0 if failure, or object number */
/* First page is 0 */
static int pdf_scan_find_page(PDFSCAN *ps, int pagenum)
{
    int code;
    ref kids;
    ref r;
    int pageobj = 0;
    int count_base = 0;
    int count;
    ref *pref;
    int i;
    int inext;

    if (pagenum >= ps->page_count) {
	pdf_scan_msgf(ps, "Not that many pages\n");
	return -1;
    }
    code = pdf_scan_read_object(ps, ps->pages);
    if (code) {
	pdf_scan_msgf(ps, "Didn't find Pages object\n");
	return -1;
    }
    /* iterate through Kids, looking for the one that includes this page */ 
    kids = dict_get(ps, "Kids");
    if (kids.type != arraytype) {
	pdf_scan_msgf(ps, "/Pages object %d must contain /Kids array\n",
	    ps->pages);
	return -1;
    }
    pop_stack(ps);	/* First Pages */
    for (i = 0; (i < kids.rsize) && (code == 0); i=inext) {
	inext = i+1;
	pref = &kids.value.arrayval[i];
	if (pref->type == objtype)
        code = pdf_scan_read_object(ps, pref->value.objval);
	if (code == 0) {
	    r = dict_get(ps, "Type"); 
	    if (nameref_equals(&r, "Page")) {
		if (count_base + i == pagenum) {
		    /* this is it */
		    pageobj = pref->value.objval;
		    pop_stack(ps);	/* the wanted page */
		    break;
		}
	    }
	    else if (nameref_equals(&r, "Pages")) {
	        r = dict_get(ps, "Count"); 
		if (r.type == integertype) {
		    count = r.value.intval;
		    if (pagenum < count_base + count) {
			/* It's under this child */
			inext = 0;
		        pop_stack(ps);	/* The old /Pages */
			code = pdf_scan_read_object(ps, pref->value.objval);
			if (code == 0) {
			    kids = dict_get(ps, "Kids");
			    if (kids.type != arraytype) {
				pdf_scan_msgf(ps, 
				"/Pages object %d must contain /Kids array\n",
				    pref->value.objval);
				code = -1;
			    }
			}
		    }
		    else {
			count_base += count;
		    }
		}
		else {
		    pdf_scan_msgf(ps, "/Pages /Count must be integer\n");
		    code = -1;
		}
	    }
	    else {
		pdf_scan_msgf(ps, 
		    "pdf_scan_find_page: object %d isn't Pages or Page\n", 
		    pref->value.objval);
		code = -1;
	    }
	    pop_stack(ps);
	}
    }

    if (pageobj <= 0) {
	pdf_scan_msgf(ps, "Failed to find page %d\n", pagenum+1);
	code = -1;
    }

    if (code)
	return -1;

    /* Don't clean up, since we will use the cached objects
     * when extracting the page media.
     */

    return pageobj;
}


static int
pdf_scan_read_page_count(PDFSCAN *ps)
{
    int code;
    ref p;
    code = pdf_scan_read_object(ps, ps->pages);
    if (code) {
	pdf_scan_msgf(ps, "Didn't find Pages object\n");
	return -1;
    }

    p = dict_get(ps, "Type");
    if (!nameref_equals(&p, "Pages")) {
	pdf_scan_msgf(ps, "Pages object didn't have /Type /Pages\n");
	return -1;
    }
    p = dict_get(ps, "Count");
    if (p.type != integertype) {
	pdf_scan_msgf(ps, "Pages object didn't integer /Count\n");
	return -1;
    }
    ps->page_count = p.value.intval;

    return code;
}

static int convert_float(ref r, float *f)
{
    if (r.type == realtype)
	*f = r.value.realval;
    else if (r.type == integertype)
	*f = (float)r.value.intval;
    else
       return -1;
    return 0;
}

static int
pdf_scan_read_bbox(PDFBBOX *box, ref array)
{
    int code = 0;
    if (array.type != arraytype)
	code = -1;
    if (array.rsize != 4)
	code = -1;
    if (code == 0)
        code = convert_float(array.value.arrayval[0], &box->llx);
    if (code == 0)
	code = convert_float(array.value.arrayval[1], &box->lly);
    if (code == 0)
	code = convert_float(array.value.arrayval[2], &box->urx);
    if (code == 0)
	code = convert_float(array.value.arrayval[3], &box->ury);
    return code;
}

/* Read catalog and leave on stack */
static int
pdf_scan_read_catalog(PDFSCAN *ps)
{
    int code;
    ref p;
    /* Read root object, making sure it is /Type /Catalog,
     * and that /Pages is an indirect reference
     */
    code = pdf_scan_read_object(ps, ps->root);
    if (code) {
	pdf_scan_msgf(ps, "Didn't find Root object\n");
	return -1;
    }

    p = dict_get(ps, "Type");
    if (!nameref_equals(&p, "Catalog")) {
	pdf_scan_msgf(ps, "Root object didn't have /Type /Catalog\n");
	return -1;
    }
    p = dict_get(ps, "Pages");
    if (p.type != objtype) {
	pdf_scan_msgf(ps, "Root object didn't indirect reference to /Pages\n");
	return -1;
    }
    ps->pages = p.value.intval;
    return 0;
}

/*****************************************************************/
/* public functions */


void
pdf_scan_close(PDFSCAN *ps)
{
    pdf_scan_cleanup(ps);
    pdf_scan_finish(ps);
    free(ps);
}


PDFSCAN *
pdf_scan_open(const TCHAR *filename, void *handle,
    int (*fn)(void *handle, const char *ptr, int len))
{
    int code;
    int rotate;
    PDFBBOX mediabox, cropbox;
    PDFSCAN *ps = (PDFSCAN *)malloc(sizeof(PDFSCAN));
    if (ps == NULL)
	return NULL;
    memset(ps, 0, sizeof(PDFSCAN));
    ps->handle = handle;
    ps->print_fn = fn;
    code = pdf_scan_init(ps, filename);
    if (code == -1)
	pdf_scan_msgf(ps, "Couldn't open PDF file\n");
    else if (code != 0)
	pdf_scan_msgf(ps, "Error initialising PDF scanner\n");

    if (code == 0)
        code = pdf_scan_find_xref(ps);
    if (code == 0)
	code = pdf_scan_read_xref(ps, ps->xref_offset);
    if (code == 0)
	code = pdf_scan_read_catalog(ps);
    if (code == 0)
	code = pdf_scan_read_page_count(ps);
    if (code == 0)
	code = pdf_scan_page_media(ps, 0, &rotate, &mediabox, &cropbox);

    pdf_scan_cleanup(ps);
    if (code != 0) {
	pdf_scan_close(ps);
	ps = NULL;
    }
    return ps;
}

int
pdf_scan_page_count(PDFSCAN *ps) 
{
    if (ps == NULL)
	return 0;
    return ps->page_count;
}

int
pdf_scan_page_media(PDFSCAN *ps, int pagenum, int *rotate,
    PDFBBOX *mediabox, PDFBBOX *cropbox)
{
    BOOL found_rotate = FALSE;
    BOOL found_mediabox = FALSE;
    BOOL found_cropbox = FALSE;
    BOOL has_parent = TRUE;
    ref p, objref;
    int objnum;

    if (ps == NULL)
	return -1;

    if (pagenum == ps->pagenum) {
	/* Used cached values */
	*rotate = ps->rotate;
	*mediabox = ps->mediabox;
	*cropbox = ps->cropbox;
	return 0;
    }

    if (ps->file == NULL) {
	if (pdf_scan_open_file(ps) != 0) 
	    return -1;
    }
    objnum = pdf_scan_find_page(ps, pagenum);
    if (objnum <= 0) {
	pdf_scan_cleanup(ps);
	return -1;
    }
    if (pdf_scan_read_object(ps, objnum) < 0) {
	pdf_scan_cleanup(ps);
	return -1;
    }

    while (has_parent) {
	if (!found_rotate) {
	    p = dict_get(ps, "Rotate");
	    if (p.type == integertype) {
		*rotate = p.value.intval;
		found_rotate = TRUE;
	    }
	}
	if (!found_mediabox) {
	    p = dict_get(ps, "MediaBox");
	    if (pdf_scan_read_bbox(mediabox, p) == 0)
		found_mediabox = TRUE;
	}
	if (!found_cropbox) {
	    p = dict_get(ps, "CropBox");
	    if (pdf_scan_read_bbox(cropbox, p) == 0)
		found_cropbox = TRUE;
	}
        if (found_rotate && found_mediabox && found_cropbox)
	    break;

	p = dict_get(ps, "Parent");
	if (p.type == objtype) {
	    objref = pop_stack(ps);
	    if (pdf_scan_read_object(ps, p.value.objval) < 0) {
		push_stack(ps, objref);
		has_parent = FALSE;
	    }
	}
	else
	    has_parent = FALSE;
    }
    pop_stack(ps);
    if (!found_cropbox) {
	*cropbox = *mediabox;
	found_cropbox = TRUE;
    }
    if (!found_rotate) {
	*rotate = 0;
	found_rotate = TRUE;
    }

    pdf_scan_cleanup(ps);
    
    if (found_rotate && found_mediabox && found_cropbox) {
	/* cache these values */
	ps->pagenum = pagenum;
	ps->rotate = *rotate;
	ps->mediabox = *mediabox;
	ps->cropbox = *cropbox;
        return 0;
    }
    
    return -1;
}

/*****************************************************************/

#ifdef DEMO_PDFSCAN

int test_print_fn(void *handle, const char *ptr, int len)
{
    fwrite(ptr, 1, len, stdout);
    return len;
}

int main(int argc, char *argv[])
{
    PDFSCAN *ps;
    int i, count;
    int code;
    PDFBBOX mediabox, cropbox;
    int rotate;

    if (argc < 2) {
	fprintf(stdout, "Usage: cpdfscan filename\n");
	return 1;
    }

    ps = pdf_scan_open(argv[1], NULL, test_print_fn);
    if (ps) {
	count = pdf_scan_page_count(ps);
	pdf_scan_msgf(ps, "Page count is %d\n", count);
	for (i=0; i<count; i++) {
	    code = pdf_scan_page_media(ps, i, &rotate, &mediabox, &cropbox);
	    if (code == 0) {
	        fprintf(stdout, "Page %d /Rotate %d ", i+1, rotate);
	        fprintf(stdout, "/MediaBox [%g %g %g %g] /CropBox [%g %g %g %g]\n", 
		    mediabox.llx, mediabox.lly, mediabox.urx, mediabox.ury,
		    cropbox.llx, cropbox.lly, cropbox.urx, cropbox.ury);
	    }
	    else
	        fprintf(stdout, "Page %d media unknown\n", i+1);
	}
	pdf_scan_close(ps);
    }
    return 0;
}

#endif
