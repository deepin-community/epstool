/* Copyright (C) 1993-2001, Ghostgum Software Pty Ltd.  All rights reserved.
  
  This file is part of GSview.
  
  This program is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the GSview Free Public Licence 
  (the "Licence") for full details.
  
  Every copy of GSview must include a copy of the Licence, normally in a 
  plain ASCII text file named LICENCE.  The Licence grants you the right 
  to copy, modify and redistribute GSview, but only under certain conditions 
  described in the Licence.  Among other things, the Licence requires that 
  the copyright notice and this notice be preserved on all copies.
*/

/* $Id: cprofile.c,v 1.3 2002/05/29 11:39:43 ghostgum Exp $ */
/* Common profile (INI file) routines */

/* Windows provides good Profile functions.
 * OS/2 provides binary profile functions which don't allow 
 * easy editing of the INI file.
 * Unix doesn't have generic profile functions.
 * 
 * To keep things consistent, these profile routines write
 * text INI files compatible with Windows.
 * Hand editing is easy.
 *
 * profile_open() reads the entire profile into memory.
 * For efficiency, try to avoid repeated opening and closing the profile.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cplat.h"
#define DEFINE_CPROFILE
#include "cprofile.h"

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

/* free keys in the section, but not the section itself */
static void
profile_free_section(prfsection *section)
{
prfentry *pe, *ne;
	pe = section->entry;
	while (pe) { /* free this entry */
	    if (pe->name)
		free(pe->name);
	    if (pe->value)
		free(pe->value);
	    ne = pe->next;
	    free(pe);
	    pe = ne;
	}
	if (section->name)
	    free(section->name);
}

static PROFILE *
profile_cleanup(PROFILE *prf)
{
prfsection *ps, *ns;
	if (prf == (PROFILE *)NULL)
	    return NULL;
	if (prf->file)
	    fclose(prf->file);
	ps = prf->section;
	while (ps) {  /* free this section */
	    profile_free_section(ps);
	    ns = ps->next;
	    free(ps);
	    ps = ns;
	}
	if (prf->name)
	    free(prf->name);
	free(prf);
	return NULL;
}

PROFILE *
profile_open(LPCTSTR filename)
{
char line[256];
PROFILE *prf;
prfsection *ps, *ns;
prfentry *pe, *ne;
char *p;
int len;

	if ( (prf = (PROFILE *)malloc(sizeof(PROFILE))) == (PROFILE *)NULL )
	    return (PROFILE *)NULL;
	prf->changed = FALSE;
	prf->section = NULL;
        len = (int)cslen(filename)+1;
	if ( (prf->name = (TCHAR *)malloc(len*sizeof(TCHAR))) == (TCHAR *)NULL )
	    return profile_cleanup(prf);
	csncpy(prf->name, filename, len);
	if ( (prf->file = csfopen(filename, TEXT("r"))) == (FILE *)NULL )
	    return prf;	/* file doesn't exist - we may be creating it */
	ps = ns = NULL;
	pe = ne = NULL;
	while (fgets(line, sizeof(line), prf->file)) {
	    if (line[0] == '[') { /* new section */
	        if ( (ns = (prfsection *)malloc(sizeof(prfsection))) 
	            == (prfsection *)NULL )
	            return profile_cleanup(prf);
	        ns->name = NULL;
	        ns->entry = NULL;
	        ns->next = NULL;
	        if (ps)
	            ps->next = ns;
	        else
	            prf->section = ns;
		ps = ns;
		pe = NULL;
	        if ( (p = strchr(line+1, ']')) != (char *)NULL )
	            *p = '\0';
		len = (int)strlen(line);
	        if ( (ns->name = (char *)malloc(len)) == (char *)NULL )
	            return profile_cleanup(prf);
	        strncpy(ns->name, line+1, len);
	    }
	    else {	/* new entry */
	    	if (ns == (prfsection *)NULL)
	    	    continue;
	        if ( (p = strchr(line, '\n')) != (char *)NULL )
	            *p = '\0';
	        if (line[0] == '\0')
		    continue;
	        if ( (ne = (prfentry *)malloc(sizeof(prfentry))) 
	            == (prfentry *)NULL )
	            return profile_cleanup(prf);
	        ne->name = NULL;
	        ne->value = NULL;
	        ne->next = NULL;
	        if (pe)
	            pe->next = ne;
	        else
	            ns->entry = ne;
		pe = ne;
	        if (line[0] == ';') { /* comment line */
		    len = (int)strlen(line)+1;
	            if ( (ne->value = (char *)malloc(len)) == (char *)NULL )
	                return profile_cleanup(prf);
	            strncpy(ne->value, line, len);
	        }
	        else {	/* a real entry */
	            strtok(line, "=");
		    len = (int)strlen(line)+1;
	            if ( (ne->name = (char *)malloc(len)) == (char *)NULL )
	                return profile_cleanup(prf);
	            strncpy(ne->name, line, len);
		    p = line + strlen(line) + 1;
/*
	            if ( (p = strtok(NULL, "=")) == (char *)NULL )
	            	continue;
*/
		    len = (int)strlen(p)+1;
	            if ( (ne->value = (char *)malloc(len)) == (char *)NULL )
	                return profile_cleanup(prf);
	            strncpy(ne->value, p, len);
	        }
	    }
	}
	fclose(prf->file);
	prf->file = NULL;
	return prf;
}

int 
profile_read_string(PROFILE *prf, const char *section, const char *entry, 
    const char *def, char *buffer, int len)
{
prfsection *ps;
prfentry *pe;
int count;
int slen;
char *p;
	if (prf == (PROFILE *)NULL)
	    return 0;
	if (buffer == (char *)NULL)
	    return 0;
	ps = prf->section;
	if (section == NULL) {
	    /* return all section names */
	    count = 0;
	    p = buffer;
	    *p = '\0';
	    while (ps) {
		slen = (int)strlen(ps->name)+ 1;
		if ( ps->name && ((slen + 2 + count) < len) ) {
		    strncpy(p, ps->name, slen);
		    count += (int)strlen(ps->name) + 1;
		    p = buffer + count;
		}
	        ps = ps->next;
	    }
	    *p = '\0';
	    return count;
	}
	while (ps) {
	    if (strcmp(ps->name, section) == 0)
	        break;
	    ps = ps->next;
	}
	if (ps == (prfsection *)NULL) {
	    strncpy(buffer, def, len); 
	    return min((int)strlen(buffer), len);
        }
	/* found section */
	pe = ps->entry;
	if (entry == NULL) {
	    /* return all entry names */
	    count = 0;
	    p = buffer;
	    *p = '\0';
	    while (pe) {
		slen = (int)strlen(pe->name)+1;
		if ( pe->name && ((int)(count + slen + 2) < len) ) {
		    strncpy(p, pe->name, slen);
		    count += (int)strlen(pe->name) + 1;
		    p = buffer + count;
		}
	        pe = pe->next;
	    }
	    *p = '\0';
	    return count;
	}
	while (pe) {
	    if (pe->name && (strcmp(pe->name, entry) == 0))
	    	break;
	    pe = pe->next;
	}
	if ( (pe == (prfentry *)NULL) ||
	     (pe->value == (char *)NULL) ) {
	    strncpy(buffer, def, len); 
	    return min((int)strlen(buffer), len);
	}
	/* return value */
        strncpy(buffer, pe->value, len);	/* got it! */
	return min((int)strlen(buffer), len);
}

BOOL
profile_write_string(PROFILE *prf, const char *section, const char *entry, 
    const char *value)
{
prfsection *ps, *ns;
prfentry *pe, *ne;
int slen;
	if (prf == (PROFILE *)NULL)
	    return FALSE;
	ns = prf->section;
	ps = NULL;
	while (ns) {
	    if (strcmp(ns->name, section) == 0)
	        break;
	    ps = ns;
	    ns = ns->next;
	}
	if (entry == (char *)NULL) {
	    /* delete section */
	    if (ns == (prfsection *)NULL)
		return TRUE;
	    profile_free_section(ns);
	    if (ps)
		ps->next = ns->next;
	    else
		prf->section = ns->next;
	    free(ns);
	    prf->changed = TRUE;
	    return TRUE;
	}
	if (ns == (prfsection *)NULL) {
	    /* add section */
	    if ( (ns = (prfsection *)malloc(sizeof(prfsection))) 
	        == (prfsection *)NULL )
	        return FALSE;
	    ns->name = NULL;
	    ns->entry = NULL;
	    ns->next = NULL;
	    if (ps)
	        ps->next = ns;
	    else
	        prf->section = ns;
	    ps = ns;
	    pe = NULL;
	    slen = (int)strlen(section)+1;
	    if ( (ns->name = (char *)malloc(slen)) == (char *)NULL )
	        return FALSE;
	    strncpy(ns->name, section, slen);
        }
	ne = ns->entry;
	pe = NULL;
	while (ne) {
	    if (ne->name && (strcmp(ne->name, entry) == 0))
	    	break;
	    pe = ne;
	    ne = ne->next;
	}
	if (ne == (prfentry *)NULL) {
	    /* add new entry */
	    if ( (ne = (prfentry *)malloc(sizeof(prfentry))) 
	        == (prfentry *)NULL )
	        return FALSE;
	    ne->name = NULL;
	    ne->value = NULL;
	    ne->next = NULL;
	    if (pe)
	        pe->next = ne;
	    else
	        ns->entry = ne;
	    pe = ne;
	    slen = (int)strlen(entry)+1;
	    if ( (ne->name = (char *)malloc(slen)) == (char *)NULL )
	        return FALSE;
	    strncpy(ne->name, entry, slen);
	}
	if (ne->value != (char *)NULL)
	    free(ne->value);	/* release old value */
	if (value) { /* change value */
	    slen = (int)strlen(value)+1;
	    if ( (ne->value = (char *)malloc(slen)) == (char *)NULL )
		return FALSE;
	    strncpy(ne->value, value, slen);
	}
	else { /* delete entry */
	    free(ne->name);
	    if (pe)
	        pe->next = ne->next;
	    else
		ns->entry = ne->next;
	    free(ne);
	}
	prf->changed = TRUE;
	return TRUE;
}

BOOL
profile_close(PROFILE *prf)
{
prfsection *ps;
prfentry *pe;
	if (prf == (PROFILE *)NULL)
		return FALSE;
	if (prf->changed) {
	    if ( (prf->file = csfopen(prf->name, TEXT("w"))) == (FILE *)NULL ) {
		profile_cleanup(prf);
	        return FALSE;
	    }
	    ps = prf->section;
	    while (ps) {
	        if (ps->name)
		    fprintf(prf->file, "[%s]\n", ps->name);
	        pe = ps->entry;
	        while (pe) {
		    if (pe->name) {
		    	if (pe->value)
		    	    fprintf(prf->file, "%s=%s\n", pe->name, pe->value);
		    	else
		    	    fprintf(prf->file, "%s=\n", pe->name);
		    }
		    else {
		    	if (pe->value)
		    	    fprintf(prf->file, "%s\n", pe->value);
		    }
		    pe = pe->next;
	        }
		fprintf(prf->file, "\n");
	        ps = ps->next;
	    }
	}
	profile_cleanup(prf);
	return TRUE;
}

