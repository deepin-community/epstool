/* Copyright (C) 2003-2005 Ghostgum Software Pty Ltd.  All rights reserved.

  This software is provided AS-IS with no warranty, either express or
  implied.

  This software is distributed under licence and may not be copied,
  modified or distributed except as expressly authorised under the terms
  of the licence contained in the file LICENCE in this distribution.
*/

/* $Id: cmac.c,v 1.11 2005/06/10 09:39:24 ghostgum Exp $ */
/* Macintosh AppleSingle, AppleDouble and MacBinary file formats */
/* Macintosh does not use a flat file system.
 * Each file can have a data fork and a resource fork.
 * EPSF files have the PostScript in the data fork, 
 * optionally have a PICT preview in the resource fork.
 * In addition, finder info gives the file type using FOURCC codes
 * such as "EPSF" or "PICT".
 * When files are copied to foreign file systems, the resource
 * fork may be left behind.  Alternatives to retain the resource
 * fork are to package the finder data, data fork and resource fork 
 * in a single MacBinary or AppleSingle file, 
 * or to put the data fork in a flat file and the finder info 
 * and resource fork in an AppleDouble file.
 */

#include "common.h"
#include <time.h>
#include "cmac.h"

static int extract_mac_data(GFile *f, LPCTSTR outname, 
   unsigned long begin, unsigned long length, unsigned long header);

DWORD
get_bigendian_dword(const unsigned char *buf)
{
    DWORD dw;
    dw = ((DWORD)buf[0])<<24;
    dw += ((DWORD)buf[1])<<16;
    dw += ((DWORD)buf[2])<<8;
    dw += (DWORD)buf[3];
    return dw;
}

WORD
get_bigendian_word(const unsigned char *buf)
{
    WORD w;
    w = (WORD)(buf[0]<<8);
    w |= (WORD)buf[1];
    return w;
}

/* write DWORD as DWORD */
void
put_bigendian_dword(unsigned char *dw, DWORD val)
{
    dw[0] = (unsigned char)((val>>24) & 0xff);
    dw[1] = (unsigned char)((val>>16) & 0xff);
    dw[2] = (unsigned char)((val>>8)  & 0xff);
    dw[3] = (unsigned char)( val      & 0xff);
}


/* write WORD as WORD */
void
put_bigendian_word(unsigned char *w, WORD val)
{
    w[0] = (unsigned char)((val>>8)  & 0xff);
    w[1] = (unsigned char)( val      & 0xff);
}

const unsigned char apple_single_magic[4] = {0x00, 0x05, 0x16, 0x00};
const unsigned char apple_double_magic[4] = {0x00, 0x05, 0x16, 0x07};

CMACFILE *get_mactype(GFile *f)
{
#define ASD_HEADER_LENGTH 26
#define MACBIN_HEADER_LENGTH 128
    unsigned char data[MACBIN_HEADER_LENGTH];
    CMAC_TYPE type = CMAC_TYPE_NONE;
    CMACFILE *mac;
    int i;
    int asd_entries = 0;
    DWORD version;
    DWORD EntryID;
    DWORD Offset;
    DWORD Length;
    FILE_POS file_length;
    int count;

    if (f == NULL)
	return NULL;
    file_length = gfile_get_length(f);
    count = gfile_read(f, data, ASD_HEADER_LENGTH);
    if (count >= ASD_HEADER_LENGTH) {
	if (memcmp(data, apple_single_magic, 4) == 0)
	    type = CMAC_TYPE_SINGLE;
	else if (memcmp(data, apple_double_magic, 4) == 0)
	    type = CMAC_TYPE_DOUBLE;
	if ((type == CMAC_TYPE_SINGLE || type == CMAC_TYPE_DOUBLE)) {
	    version = get_bigendian_dword(data+4);
	    if ((version != 0x00010000) && (version != 0x00020000))
		type = CMAC_TYPE_NONE;
	    asd_entries = get_bigendian_word(data+24);
	}
	else if (type == CMAC_TYPE_NONE) {
	    count += gfile_read(f, data+ASD_HEADER_LENGTH, 
		MACBIN_HEADER_LENGTH-ASD_HEADER_LENGTH);
	    if (count >= MACBIN_HEADER_LENGTH && 
		(data[0]==0x0) && 
		(data[1] >= 1) && (data[1] <= 63) && 
		(data[74]==0x0) && (data[82]==0x0) &&
		(data[65]>=' ') && (data[65]<='z') &&
		(data[66]>=' ') && (data[66]<='z') &&
		(data[67]>=' ') && (data[67]<='z') &&
		(data[68]>=' ') && (data[68]<='z') &&
		(data[69]>=' ') && (data[69]<='z') &&
		(data[70]>=' ') && (data[70]<='z') &&
		(data[71]>=' ') && (data[71]<='z') &&
		(data[72]>=' ') && (data[72]<='z')) {
		type = CMAC_TYPE_MACBIN;
	    }
	    else {
		/* check for bare resource fork */
		DWORD data_begin = get_bigendian_dword(data);
		DWORD map_begin = get_bigendian_dword(data+4);
		DWORD data_length = get_bigendian_dword(data+8);
		DWORD map_length = get_bigendian_dword(data+12);
		if ((data_begin == 0x100) && 
		    (data_begin + data_length == map_begin) &&
		    (map_begin + map_length == file_length))
		    type = CMAC_TYPE_RSRC;
	    }
	}
    }

    if (type == CMAC_TYPE_NONE)
	return NULL;

    mac = (CMACFILE *)malloc(sizeof(CMACFILE));
    if (mac == NULL)
	return NULL;
    memset(mac, 0, sizeof(CMACFILE));
    mac->type = type;

    /* Read Mac Binary stuff */
    if (type == CMAC_TYPE_MACBIN) {
	memcpy(mac->file_type, data+65, 4);
	memcpy(mac->file_creator, data+69, 4);
	mac->data_begin = 128;
	mac->data_length = get_bigendian_dword(data+83);
        mac->resource_begin = 
	    (mac->data_begin + mac->data_length + 127 ) & ~127;
	mac->resource_length = get_bigendian_dword(data+87);
    }
    else if (type == CMAC_TYPE_RSRC) {
	memcpy(mac->file_type, "    ", 4);
	memcpy(mac->file_creator, "    ", 4);
	mac->resource_begin = 0;
	mac->resource_length = file_length;
    }
    else {
	/* AppleSingle or AppleDouble */
	for (i=0; i<asd_entries; i++) {
	    count = gfile_read(f, data, 12);
	    EntryID = get_bigendian_dword(data);
	    Offset = get_bigendian_dword(data+4);
	    Length = get_bigendian_dword(data+8);
	    switch (EntryID) {
		case 1:	/* Data fork */
		    mac->data_begin = Offset;
		    mac->data_length = Length;
		    break;
		case 2:	/* Resource fork */
		    mac->resource_begin = Offset;
		    mac->resource_length = Length;
		    break;
		case 9:	/* Finder info */
		    mac->finder_begin = Offset;
		    mac->finder_length = Length;
		    break;
	    }
	}
	if (mac->finder_begin != 0) {
	    gfile_seek(f, mac->finder_begin, SEEK_SET);
	    count = gfile_read(f, data, min(sizeof(data), mac->finder_length));
	    if (count >= 8) {
		memcpy(mac->file_type, data, 4);
		memcpy(mac->file_creator, data+4, 4);
	    }
	}
    }

    return mac;
}


typedef struct CMAC_RESOURCE_HEADER_s {
    DWORD data_begin;
    DWORD map_begin;
    DWORD data_length;
    DWORD map_length;
} CMAC_RESOURCE_HEADER;

typedef struct CMAC_RESOURCE_MAP_s {
    unsigned char reshdr[16];
    DWORD reshdl;
    WORD filerefno;
    WORD attributes;
    WORD offset_type_list;
    WORD offset_name_list;
    WORD type_count;
} CMAC_RESOURCE_MAP;

typedef struct CMAC_RESOURCE_TYPE_LIST_s {
    unsigned char type[4];
    WORD count;
    WORD offset_ref_list;
} CMAC_RESOURCE_TYPE_LIST;

typedef struct CMAC_RESOURCE_REF_LIST_s {
    WORD id;
    WORD offset_name;
    unsigned char attributes;
    DWORD offset_data;	/* 3 bytes only */
    DWORD handle;
} CMAC_RESOURCE_REF_LIST;


/* Find the location of PICT in the resource fork, if present */
int
get_pict(GFile *f, CMACFILE *mac, int debug)
{
    CMAC_RESOURCE_HEADER reshdr;
    CMAC_RESOURCE_MAP resmap;
    CMAC_RESOURCE_TYPE_LIST typelist;
    CMAC_RESOURCE_REF_LIST reflist;
    DWORD res_offset;
    DWORD map_offset;
    DWORD type_offset;
    DWORD ref_offset;
    DWORD preview_offset = 0;
    DWORD preview_length = 0;
    unsigned char data[16];
    char name[257];
    int name_length;
    int count;
    int i, j;
    if (mac == NULL)
	return 1;
    if (mac->type == CMAC_TYPE_NONE)
	return 1;
    if (((mac->type != CMAC_TYPE_RSRC) && (mac->resource_begin == 0)) 
	|| (mac->resource_length == 0))
	return 1;

    memset(&resmap, 0, sizeof(resmap));
    memset(&typelist, 0, sizeof(typelist));
    memset(&reflist, 0, sizeof(reflist));

    res_offset = mac->resource_begin;
    gfile_seek(f, res_offset, SEEK_SET);
    count = gfile_read(f, data, 16);
    if (count != 16)
	return -1;
    reshdr.data_begin = get_bigendian_dword(data);
    reshdr.map_begin = get_bigendian_dword(data+4);
    reshdr.data_length = get_bigendian_dword(data+8);
    reshdr.map_length = get_bigendian_dword(data+12);
    if (debug) {
	fprintf(stdout, "resource data: %ld %ld\n", 
		reshdr.data_begin, reshdr.data_length);
	fprintf(stdout, "resource map: %ld %ld\n", 
		reshdr.map_begin, reshdr.map_length);
    }

    map_offset = res_offset+reshdr.map_begin;
    gfile_seek(f, map_offset, SEEK_SET);
    count = gfile_read(f, &resmap.reshdr, 16);
    if (count != 16)
	return -1;
    count = gfile_read(f, data, 14);
    if (count != 14)
	return -1;
    resmap.reshdl = get_bigendian_dword(data);
    resmap.filerefno = get_bigendian_word(data+4);
    resmap.attributes = get_bigendian_word(data+6);
    resmap.offset_type_list = get_bigendian_word(data+8);
    resmap.offset_name_list = get_bigendian_word(data+10);
    resmap.type_count = get_bigendian_word(data+12);

    if (debug) {
	fprintf(stdout, " resource handle %ld\n", resmap.reshdl);
	fprintf(stdout, " file reference number %d\n", 
	    resmap.filerefno);
	fprintf(stdout, " attributes 0x%x\n", resmap.attributes);
	fprintf(stdout, " offset type list %d\n", resmap.offset_type_list);
	fprintf(stdout, " offset name list %d\n", resmap.offset_name_list);
	fprintf(stdout, " type count %d\n", resmap.type_count);
    }

    /* Documentation says that the type list starts at 
     * map_offset + resmap.offset_type_list, but we have
     * found that it is actually 2 bytes further on.
     * Perhaps the type count is supposed be part of the type_list
     */
    type_offset = map_offset+resmap.offset_type_list;
    for (i=0; i<=resmap.type_count; i++) {
        gfile_seek(f, type_offset + 2 + i * 8, SEEK_SET); /* +2 KLUDGE */
        count = gfile_read(f, &typelist.type, 4);
	if (count != 4)
	    return -1;
        count = gfile_read(f, data, 4);
	if (count != 4)
	    return -1;
	typelist.count = get_bigendian_word(data);
	typelist.offset_ref_list = get_bigendian_word(data+2);
	if (debug)
	    fprintf(stdout, "type %d %c%c%c%c count=%d offset=%d\n", i,
		typelist.type[0], typelist.type[1], 
		typelist.type[2], typelist.type[3],
		typelist.count, typelist.offset_ref_list);
	ref_offset = type_offset + typelist.offset_ref_list;
	for (j=0; j<=typelist.count; j++) {
            gfile_seek(f, ref_offset + j * 12, SEEK_SET);
	    count = gfile_read(f, data, 12);
	    if (count != 12)
		return -1;
	    reflist.id = get_bigendian_word(data);
	    reflist.offset_name = get_bigendian_word(data+2);
	    reflist.attributes = data[4];
	    reflist.offset_data = ((DWORD)data[5])<<16;
	    reflist.offset_data += ((DWORD)data[6])<<8;
	    reflist.offset_data += ((DWORD)data[7]);
	    reflist.handle = get_bigendian_dword(data+8);
	    if (debug) {
		fprintf(stdout, "  reflist %d id=%d name=%d attributes=0x%x data=%ld 0x%lx\n", 
		    j, reflist.id, reflist.offset_name, reflist.attributes,
		    reflist.offset_data, reflist.offset_data);
		gfile_seek(f, res_offset + reshdr.data_begin +
		    reflist.offset_data, SEEK_SET);
		count = gfile_read(f, data, 4);
		if (count != 4)
		    return -1;
		fprintf(stdout, "  length=%ld 0x%lx\n",
		get_bigendian_dword(data), get_bigendian_dword(data));
	    }


	    if ((resmap.offset_name_list < reshdr.map_length) &&
		(resmap.offset_name_list != 0xffff) &&
		(reflist.offset_name != 0xffff)) {
		gfile_seek(f, map_offset + resmap.offset_name_list + 
		    reflist.offset_name, SEEK_SET);
		count = gfile_read(f, data, 1);
		if (count != 1)
		    return -1;
		name_length = data[0];
		if (name_length <= 256) {
		    count = gfile_read(f, name, name_length);
		    if (count != name_length)
			return -1;
		    name[name_length] = '\0';
		    if (debug)
			fprintf(stdout, "    name=%s\n", name);
		}
	    }
	    if ((memcmp(typelist.type, "PICT", 4) == 0) &&
		(reflist.id == 256)) {
		/* This is the PICT preview for an EPS files */
		preview_offset = 
		    res_offset + reshdr.data_begin + reflist.offset_data;
	    }
	}
    }

    if (preview_offset != 0) {
	gfile_seek(f, preview_offset, SEEK_SET);
	gfile_read(f, data, 4);
	preview_length = get_bigendian_dword(data);
	if (preview_length != 0) {
	    mac->pict_begin = preview_offset + 4;
	    mac->pict_length = preview_length;
	}
    }
    return 0;
}


/* Extract EPSF from data resource fork to a file */
/* Returns 0 on success, negative on failure */
static int 
extract_mac_data(GFile *f, LPCTSTR outname, 
   unsigned long begin, unsigned long length, unsigned long header)
{
unsigned long len;
unsigned int count;
char *buffer;
GFile *outfile;
int code = 0;
    if ((begin == 0) || (length == 0))
	return -1;

    if (*outname == '\0')
	return -1;

    /* create buffer for file copy */
    buffer = (char *)malloc(COPY_BUF_SIZE);
    if (buffer == (char *)NULL)
	return -1;

    outfile = gfile_open(outname, gfile_modeWrite | gfile_modeCreate);
    if (outfile == (GFile *)NULL) {
	free(buffer);
	return -1;
    }

    /* PICT files when stored separately start with 512 nulls */
    memset(buffer, 0, COPY_BUF_SIZE);
    if (header && header < COPY_BUF_SIZE)
        gfile_write(outfile, buffer, header);

    gfile_seek(f, begin, gfile_begin); /* seek to EPSF or PICT */
    len = length; 
    while ( (count = (unsigned int)min(len,COPY_BUF_SIZE)) != 0 ) {
	count = (int)gfile_read(f, buffer, count);
	gfile_write(outfile, buffer, count);
	if (count == 0) {
	    len = 0;
	    code = -1;
	}
	else
	    len -= count;
    }
    free(buffer);
    gfile_close(outfile);

    return code;
}

/* Extract PICT from resource fork to a file */
/* Returns 0 on success, negative on failure */
int 
extract_mac_pict(GFile *f, CMACFILE *mac, LPCTSTR outname)
{
    if ((f == NULL) || (mac == NULL))
	return -1;
    /* PICT files when stored separately start with 512 nulls */
    return extract_mac_data(f, outname, 
	mac->pict_begin, mac->pict_length, 512);
}

/* Extract EPSF from data fork to a file */
/* Returns 0 on success, negative on failure */
int 
extract_mac_epsf(GFile *f, CMACFILE *mac, LPCTSTR outname)
{
    if ((f == NULL) || (mac == NULL))
	return -1;
    return extract_mac_data(f, outname, 
	mac->data_begin, mac->data_length, 0);
}

/* Write resources containing a single PICT with id=256 to file.
 * Return number of bytes written if OK, -ve if not OK.
 * If f==NULL, return number of bytes required for resources.
 */
int
write_resource_pict(GFile *f, LPCTSTR pictname)
{
    GFile *pictfile;
    unsigned long pict_length;
    unsigned char data[256];
    unsigned long data_offset = 256;
    unsigned long map_length = 58;
    unsigned long pict_offset = 0;	/* at start of resource data */
    unsigned resource_length;
    unsigned long len;
    unsigned int count;
    int code = 0;

    pictfile = gfile_open(pictname, gfile_modeRead);
    if (pictfile == NULL)
	return -1;
    pict_length = gfile_get_length(pictfile) - 512;
    if ((long)pict_length < 0) {
	gfile_close(pictfile);
	return -1;
    }
    resource_length = data_offset + 4 + pict_length + map_length;
    if (f == NULL) {
	gfile_close(pictfile);
	return resource_length;
    }

    /* resource header */
    memset(data, 0, sizeof(data));
    put_bigendian_dword(data, data_offset);	/* data offset */
    put_bigendian_dword(data+4, data_offset + 4 + pict_length); /* map offset */
    put_bigendian_dword(data+8, pict_length + 4); /* data length */
    put_bigendian_dword(data+12, map_length);	/* map length */
    gfile_write(f, data, data_offset);

    /* pict file */
    put_bigendian_dword(data, pict_length);
    gfile_write(f, data, 4);
    len = pict_length; 
    gfile_seek(pictfile, 512, SEEK_SET);
    while ( (count = (unsigned int)min(len,sizeof(data))) != 0 ) {
	count = (int)gfile_read(pictfile, data, count);
	gfile_write(f, data, count);
	if (count == 0) {
	    len = 0;
	    code = -1;
	}
	else
	    len -= count;
    }
    gfile_close(pictfile);
    if (code < 0)
	return code;

    /* resource map */
    memset(data, 0, sizeof(data));
    put_bigendian_dword(data+16, 0);	/* resource handle */
    put_bigendian_word(data+20, 0);	/* file reference number */
    put_bigendian_word(data+22, 0);	/* attributes */
    put_bigendian_word(data+24, 28);	/* offset to type list */
    put_bigendian_word(data+26, 50);	/* offset to name list */
    gfile_write(f, data, 28);

    /* type list */
    memset(data, 0, sizeof(data));
    put_bigendian_word(data, 0);	/* number of types */
    memcpy(data+2, "PICT", 4);
    put_bigendian_word(data+6, 0);	/* type count */
    put_bigendian_word(data+8, 10);	/* offset to ref list */
    gfile_write(f, data, 10);

    /* reference list */
    memset(data, 0, sizeof(data));
    put_bigendian_word(data, 256);	/* resource id for EPSF preview */
    put_bigendian_word(data+2, 0);	/* offset to name */
    data[4] = '\0';			/* attributes */
    data[5] = (unsigned char)((pict_offset>>16) & 0xff);
    data[6] = (unsigned char)((pict_offset>>8)  & 0xff);
    data[7] = (unsigned char)( pict_offset      & 0xff);
    put_bigendian_dword(data+8, 0);	/* handle */
    gfile_write(f, data, 12);

    /* name list */
    memset(data, 0, sizeof(data));
    data[0] = 7;
    memcpy(data+1, "Preview", 7);
    gfile_write(f, data, 8);

    return resource_length;
}

/* Write an AppleDouble file containing a single PICT with id=256 to file. */
int
write_appledouble(GFile *f, LPCTSTR pictname)
{
    unsigned char data[256];
    unsigned long resource_length;

    /* get length of resources */
    resource_length = write_resource_pict(NULL, pictname);

    memset(data, 0, sizeof(data));
    memcpy(data, apple_double_magic, 4);	/* magic signature */
    put_bigendian_dword(data+4, 0x00020000);	/* version */
    /* 16 bytes filler */
    put_bigendian_word(data+24, 2);	/* 2 entries, finder and resource */
    /* finder descriptor */
    put_bigendian_dword(data+26, 9);	/* finder id */
    put_bigendian_dword(data+30, 50);	/* offset */
    put_bigendian_dword(data+34, 32);	/* length */
    /* resource descriptor */
    put_bigendian_dword(data+38, 2);	/* resource fork id */
    put_bigendian_dword(data+42, 82);	/* offset */
    put_bigendian_dword(data+46, resource_length);	/* length */
    /* finder info */
    memcpy(data+50, "EPSF", 4);		/* file type */
    memcpy(data+54, "MSWD", 4);		/* file creator */
    data[58] = 1;			/* ??? need to check finder info */
    gfile_write(f, data, 82);

    /* Now copy the resource fork */
    if (write_resource_pict(f, pictname) <= 0)
	return -1;
    return 0;
}

/* Write an AppleSingle file with a data fork containing EPSF
 * and a resource fork containing a preview as a single PICT 
 * with id=256.
 */
int
write_applesingle(GFile *f, LPCTSTR epsname, LPCTSTR pictname)
{
    unsigned long resource_length;
    unsigned long data_length;
    unsigned char data[256];
    unsigned char *buffer;
    GFile *epsfile;
    unsigned long len;
    unsigned int count;
    int code = 0;

    buffer = (unsigned char *)malloc(COPY_BUF_SIZE);
    if (buffer == (unsigned char *)NULL)
	return -1;

    /* get length of data and resources */
    epsfile = gfile_open(epsname, gfile_modeRead);
    if (epsname == NULL) {
	free(buffer);
	return -1;
    }
    data_length = gfile_get_length(epsfile);
    resource_length = write_resource_pict(NULL, pictname);

    memset(data, 0, sizeof(data));
    memcpy(data, apple_single_magic, 4);	/* magic signature */
    put_bigendian_dword(data+4, 0x00020000);	/* version */
    /* 16 bytes filler */
    put_bigendian_word(data+24, 3); /* 3 entries, finder, data and resource */
    /* finder descriptor */
    put_bigendian_dword(data+26, 9);	/* finder id */
    put_bigendian_dword(data+30, 62);	/* offset */
    put_bigendian_dword(data+34, 32);	/* length */
    /* data descriptor */
    put_bigendian_dword(data+38, 1);	/* data fork id */
    put_bigendian_dword(data+42, 94);	/* offset */
    put_bigendian_dword(data+46, data_length);	/* length */
    /* resource descriptor */
    put_bigendian_dword(data+50, 2);	/* resource fork id */
    put_bigendian_dword(data+54, 94+data_length);	/* offset */
    put_bigendian_dword(data+58, resource_length);	/* length */
    /* finder info */
    memcpy(data+62, "EPSF", 4);		/* file type */
    memcpy(data+66, "MSWD", 4);		/* file creator */
    data[70] = 1;			/* ??? need to check finder info */
    gfile_write(f, data, 94);

    /* Copy data fork */
    len = data_length;
    while ( (count = (unsigned int)min(len,COPY_BUF_SIZE)) != 0 ) {
	count = (int)gfile_read(epsfile, buffer, count);
	gfile_write(f, buffer, count);
	if (count == 0) {
	    len = 0;
	    code = -1;
	}
	else
	    len -= count;
    }
    gfile_close(epsfile);
    free(buffer);
    if (code < 0)
	return code;

    /* Now copy the resource fork */
    if (write_resource_pict(f, pictname) <= 0)
	return -1;
    return 0;
}


/* Write a MacBinary file with a data fork containing EPSF
 * and a resource fork containing a preview as a single PICT 
 * with id=256.
 */
int
write_macbin(GFile *f, const char *name, LPCTSTR epsname, LPCTSTR pictname)
{
    unsigned char *buffer;
    unsigned char data[128];
    const char *macname;
    int macname_length;
    unsigned long data_length;
    unsigned long resource_length;
    unsigned long len;
    unsigned int count;
    int code = 0;
    GFile *epsfile;
    time_t now = time(NULL);
    now += 2082844800LU;	/* convert from Unix to Mac time */
    macname = name;
    if (name[0] == '\0')	/* we are writing to stdout */
	macname = "Unknown";

    buffer = (unsigned char *)malloc(COPY_BUF_SIZE);
    if (buffer == (unsigned char *)NULL)
	return -1;

    /* get length of data and resources */
    epsfile = gfile_open(epsname, gfile_modeRead);
    if (epsname == NULL) {
	free(buffer);
	return -1;
    }
    data_length = gfile_get_length(epsfile);
    resource_length = write_resource_pict(NULL, pictname);

    /* MacBinary I header */
    memset(data, 0, sizeof(data));
    data[0] = 0;			/* version */
    macname_length = min(63, (int)strlen(macname));
    data[1] = (unsigned char)macname_length;
    memcpy(data+2, macname, macname_length);
    memcpy(data+65, "EPSF", 4);		/* file type */
    memcpy(data+69, "MSWD", 4);		/* file creator */
    data[73] = 1;			/* finder flags */
    put_bigendian_dword(data+83, data_length);
    put_bigendian_dword(data+87, resource_length);
    put_bigendian_dword(data+91, (DWORD)now);	/* creation date */
    put_bigendian_dword(data+95, (DWORD)now);	/* last modified date */
    gfile_write(f, data, 128);
    
    /* copy data fork */
    len = data_length;
    while ( (count = (unsigned int)min(len,COPY_BUF_SIZE)) != 0 ) {
	count = (int)gfile_read(epsfile, buffer, count);
	gfile_write(f, buffer, count);
	if (count == 0) {
	    len = 0;
	    code = -1;
	}
	else
	    len -= count;
    }
    gfile_close(epsfile);
    free(buffer);
    if (code < 0)
	return code;

    /* Pad to 128 byte boundary */
    memset(data, 0, sizeof(data));
    count = data_length & 127;
    if (count)
	gfile_write(f, data, 128-count);

    /* Now copy the resource fork */
    if (write_resource_pict(f, pictname) <= 0)
	return -1;

    /* Pad to 128 byte boundary */
    count = resource_length & 127;
    if (count)
	gfile_write(f, data, 128-count);

    return 0;
}


/* Returns -1 for error, 0 for Mac and 1 for non-Mac */
/* If Macintosh format and verbose, print some details to stdout */
int dump_macfile(LPCTSTR filename, int verbose)
{
    CMACFILE *mac;
    const char *p;
    GFile *f = gfile_open(filename, gfile_modeRead);
    if (f == NULL)
	return -1;
    mac = get_mactype(f);
    if (mac)
        get_pict(f, mac, (verbose > 1));
    
    gfile_close(f);

    if (mac == NULL)
	return 1;
    if (verbose) {
	switch (mac->type) {
	    case CMAC_TYPE_SINGLE:
		p = "AppleSingle";
		break;
	    case CMAC_TYPE_DOUBLE:
		p = "AppleDouble";
		break;
	    case CMAC_TYPE_MACBIN:
		p = "MacBinary";
		break;
	    case CMAC_TYPE_RSRC:
		p = "Resource";
		break;
	    default:
		p = "Unknown";
	}
	fprintf(stdout, "Macintosh Binary Format: %s\n", p);
	fprintf(stdout, " File Type: %c%c%c%c\n", 
	    mac->file_type[0], mac->file_type[1], 
	    mac->file_type[2], mac->file_type[3]);
	fprintf(stdout, " File Creator: %c%c%c%c\n", 
	    mac->file_creator[0], mac->file_creator[1],
	    mac->file_creator[2], mac->file_creator[3]);
	fprintf(stdout, " Finder Info: %ld %ld\n", 
	    mac->finder_begin, mac->finder_length);
	fprintf(stdout, " Data Fork: %ld %ld\n", 
	    mac->data_begin, mac->data_length);
	fprintf(stdout, " Resource Fork: %ld %ld\n", 
	    mac->resource_begin, mac->resource_length);
	fprintf(stdout, " PICT: %ld %ld, 0x%lx 0x%lx\n", 
	    mac->pict_begin, mac->pict_length,
	    mac->pict_begin, mac->pict_length);
    }
    free(mac);
    return 0;
}

#ifdef STANDALONE
/* To compile standalone for Windows,
 * cl -D_Windows -D__WIN32__ -DSTANDALONE -Isrc -Isrcwin
 *    src/cmac.c obj/wfile.obj obj/calloc.obj
 */


int debug;


void usage(void)
{
    fprintf(stdout, "Usage:\n\
    File info: cmac -i input.eps\n\
    Extract data fork: cmac -d input output\n\
    Extract PICT: cmac -x input.eps output.pict\n\
    Write AppleSingle: cmac -1 input.eps input.pict output.eps\n\
    Write AppleDouble: cmac -2 input.pict ._output.eps\n\
    Write MacBinary: cmac -b input.eps input.pict output.eps\n\
");
}

int main(int argc, char *argv[])
{
    GFile *f;
    CMACFILE *mac;
    int code = 0;
    if (argc < 2) {
	usage();
	return 1;
    }
 
    if (argv[1][0] != '-') {
	usage();
	return 1;
    }
    switch (argv[1][1]) {
	case 'i':
	    if (argc != 3) {
		usage();
		return 1;
	    }
	    if (dump_macfile(argv[2], 2) == 1)
		fprintf(stdout, "Not a Macintosh Resource, MacBinary, AppleSingle or AppleDouble file";
	    break;
	case 'd':
	    /* Extract data fork */
	    if (argc != 4) {
		usage();
		return 1;
	    }
	    f = gfile_open(argv[2], gfile_modeRead);
	    if (f == NULL) {
		fprintf(stdout, "Failed to open file \042%s\042\n", argv[1]);
		return -1;
	    }
	    mac = get_mactype(f);
	    if (mac == NULL) {
		fprintf(stdout, "Not a Mac file with resource fork\n");
		code = 1;
	    }
	    if (code == 0) {
		code = extract_mac_data(f, argv[3], 
		    mac->data_begin, mac->data_length, 0);
		if (code)
		    fprintf(stdout, "Failed to extract data fork.\n");
		else
		    fprintf(stdout, "Success\n");
	    }
	    gfile_close(f);
	    break;
	case 'x':
	    /* Extract PICT preview */
	    if (argc != 4) {
		usage();
		return 1;
	    }
	    f = gfile_open(argv[2], gfile_modeRead);
	    if (f == NULL) {
		fprintf(stdout, "Failed to open file \042%s\042\n", argv[1]);
		return -1;
	    }
	    mac = get_mactype(f);
	    if (mac == NULL) {
		fprintf(stdout, "Not a Mac file with resource fork\n");
		code = 1;
	    }
	    if (get_pict(f, mac, FALSE) == 0) {
		if (extract_mac_pict(f, mac, argv[3]) != 0)
		    fprintf(stdout, "Failed to find PICT id=256 or write file\n");
		else
		    fprintf(stdout, "Success\n");
		    
	    }
	    else {
		    fprintf(stdout, "Resource fork didn't contain PICT preview\n");
	    }
	    gfile_close(f);
	    break;
	case 'm':
	    /* Create MacBinary */
	    if (argc != 5) {
		usage();
		return 1;
	    }
	    f = gfile_open(argv[4], gfile_modeWrite | gfile_modeCreate);
	    if (f == NULL) {
		fprintf(stdout, "Failed to create file \042%s\042\n", argv[3]);
		return -1;
	    }
	    code = write_macbin(f, argv[2], argv[2], argv[3]);
	    if (code != 0)
		fprintf(stdout, "Failed to write MacBinary\n");
	    else
		fprintf(stdout, "Success at writing MacBinary\n");
	    gfile_close(f);
	    break;
	case '1':
	    /* Create AppleSingle */
	    if (argc != 5) {
		usage();
		return 1;
	    }
	    f = gfile_open(argv[4], gfile_modeWrite | gfile_modeCreate);
	    if (f == NULL) {
		fprintf(stdout, "Failed to create file \042%s\042\n", argv[3]);
		return -1;
	    }
	    code = write_applesingle(f, argv[2], argv[3]);
	    if (code != 0)
		fprintf(stdout, "Failed to write AppleSingle\n");
	    else
		fprintf(stdout, "Success at writing AppleSingle\n");
	    gfile_close(f);
	    break;
	case '2':
	    /* Create AppleDouble */
	    if (argc != 4) {
		usage();
		return 1;
	    }
	    f = gfile_open(argv[3], gfile_modeWrite | gfile_modeCreate);
	    if (f == NULL) {
		fprintf(stdout, "Failed to create file \042%s\042\n", argv[3]);
		return -1;
	    }
	    code = write_appledouble(f, argv[2]);
	    if (code != 0)
		fprintf(stdout, "Failed to write AppleDouble\n");
	    else
		fprintf(stdout, "Success at writing AppleDouble\n");
	    gfile_close(f);
	    break;
	default:
	    usage();
	    return 1;
    }

    return code;
}
#endif
