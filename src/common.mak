# Copyright (C) 2002-2005 Ghostgum Software Pty Ltd.  All rights reserved.
#
#  This software is provided AS-IS with no warranty, either express or
#  implied.
#
#  This software is distributed under licence and may not be copied,
#  modified or distributed except as expressly authorised under the terms
#  of the licence contained in the file LICENCE in this distribution.
#

# $Id: common.mak,v 1.34 2005/06/10 09:39:24 ghostgum Exp $
# Common makefile for GSview

# Used by all clients 
OBJCOM1=$(OD)calloc$(OBJ) $(OD)capp$(OBJ) \
 $(OD)cbmp$(OBJ) $(OD)cdoc$(OBJ) $(OD)ceps$(OBJ) \
 $(OD)cimg$(OBJ) $(OD)clzw$(OBJ) \
 $(OD)cmac$(OBJ) $(OD)cmbcs$(OBJ) $(OD)cpdfscan$(OBJ) \
 $(OD)cprofile$(OBJ) $(OD)cps$(OBJ) \
 $(OD)dscparse$(OBJ) $(OD)dscutil$(OBJ)

# Used by simple viewer
OBJCOM2=$(OD)cdisplay$(OBJ) $(OD)cgsdll$(OBJ) \
 $(OD)cgssrv$(OBJ) $(OD)chist$(OBJ) \
 $(OD)copt$(OBJ) $(OD)cpagec$(OBJ) $(OD)cpdf$(OBJ) \
 $(OD)cpldll$(OBJ) $(OD)cplsrv$(OBJ) \
 $(OD)cview$(OBJ) 

# Used by complex viewer
OBJCOM3=$(OD)cargs$(OBJ) $(OD)ccoord$(OBJ) $(OD)cvcmd$(OBJ)

OBJS=$(OBJCOM1) $(OBJCOM2) $(OBJCOM3) $(OBJPLAT)

EPSTOOL_VERSION=3.09
EPSTOOL_DATE=2015-03-15
EPSOBJS=$(EPSOBJPLAT) \
 $(OD)epstool$(OBJ) \
 $(OBJCOM1)

EPSTESTOBJS=$(EPSOBJPLAT) \
 $(OD)epstest$(OBJ) \
 $(OD)cbmp$(OBJ) \
 $(OD)cimg$(OBJ) $(OD)clzw$(OBJ)

cplat_h=$(SRC)cplat.h
cfile_h=$(SRC)cfile.h
common_h=$(SRC)common.h $(cplat_h) $(cfile_h)
capp_h=$(SRC)capp.h
cargs_h=$(SRC)cargs.h
cbmp_h=$(SRC)cbmp.h
cdisplay_h=$(SRC)cdisplay.h
cdll_h=$(SRC)cdll.h
cdoc_h=$(SRC)cdoc.h
ceps_h=$(SRC)ceps.h
cmac_h=$(SRC)cmac.h
cgsdll_h=$(SRC)cgsdll.h
cgssrv_h=$(SRC)cgssrv.h
chist_h=$(SRC)chist.h
cimg_h=$(SRC)cimg.h
clzw_h=$(SRC)clzw.h
cmsg_h=$(SRC)cmsg.h
copt_h=$(SRC)copt.h
cpagec_h=$(SRC)cpagec.h
cpdf_h=$(SRC)cpdf.h
cpdfscan_h=$(SRC)cpdfscan.h
cpldll_h=$(SRC)cpldll.h
cprofile_h=$(SRC)cprofile.h
cps_h=$(SRC)cps.h
cres_h=$(SRC)cres.h
cvcmd_h=$(SRC)cvcmd.h
cver_h=$(SRC)cver.h
cview_h=$(SRC)cview.h
dscparse_h=$(SRC)dscparse.h
errors_h=$(SRC)errors.h
gdevdsp_h=$(SRC)gdevdsp.h
iapi_h=$(SRC)iapi.h
plapis_h=$(SRC)plapis.h


all: $(BEGIN) $(TARGET)

$(OD)calloc$(OBJ): $(SRC)calloc.c $(common_h)
	$(COMP) $(FOO)calloc$(OBJ) $(CO) $(SRC)calloc.c

$(OD)capp$(OBJ): $(SRC)capp.c $(common_h) $(dscparse_h) $(copt_h) \
 $(capp_h) $(cdll_h) $(cgssrv_h) $(cimg_h) $(cpagec_h) $(cprofile_h) \
 $(cres_h)
	$(COMP) $(FOO)capp$(OBJ) $(CO) $(SRC)capp.c

$(OD)cargs$(OBJ): $(SRC)cargs.c $(common_h) $(dscparse_h) $(capp_h) \
 $(cargs_h) $(copt_h) $(cview_h)
	$(COMP) $(FOO)cargs$(OBJ) $(CO) $(SRC)cargs.c

$(OD)cbmp$(OBJ): $(SRC)cbmp.c $(common_h) $(gdevdsp_h) $(cimg_h)
	$(COMP) $(LIBPNGINC) $(FOO)cbmp$(OBJ) $(CO) $(SRC)cbmp.c

$(OD)ccoord$(OBJ): $(SRC)ccoord.c $(common_h) $(gdevdsp_h) $(cpagec_h)
	$(COMP) $(FOO)ccoord$(OBJ) $(CO) $(SRC)ccoord.c

$(OD)cdisplay$(OBJ): $(SRC)cdisplay.c $(common_h) $(gdevdsp_h) $(errors_h) \
 $(capp_h) $(cimg_h) $(cdisplay_h) $(cgssrv_h) $(cmsg_h)
	$(COMP) $(FOO)cdisplay$(OBJ) $(CO) $(SRC)cdisplay.c

$(OD)cdoc$(OBJ): $(SRC)cdoc.c $(common_h) $(dscparse_h) $(capp_h) \
 $(cdoc_h) $(cpdfscan_h) $(cres_h)
	$(COMP) $(FOO)cdoc$(OBJ) $(CO) $(SRC)cdoc.c

$(OD)ceps$(OBJ): $(SRC)ceps.c $(common_h) $(dscparse_h) \
 $(gdevdsp_h) $(capp_h) $(cbmp_h) $(cdoc_h) $(ceps_h) $(cimg_h) \
 $(cmac_h) $(cpdfscan_h) $(cps_h)
	$(COMP) $(FOO)ceps$(OBJ) $(CO) $(SRC)ceps.c

$(OD)cfile$(OBJ): $(SRC)cfile.c $(SRC)cfile.h
	$(COMP) $(FOO)cfile$(OBJ) $(CO) $(SRC)cfile.c

$(OD)clfile$(OBJ): $(SRC)clfile.c $(SRC)cfile.h
	$(COMP) $(FOO)clfile$(OBJ) $(CO) $(SRC)clfile.c

$(OD)cgsdll$(OBJ): $(SRC)cgsdll.c $(common_h) $(errors_h) $(iapi_h) \
 $(capp_h) $(cdll_h) $(cgsdll_h)
	$(COMP) $(FOO)cgsdll$(OBJ) $(CO) $(SRC)cgsdll.c

$(OD)cgssrv$(OBJ): $(SRC)cgssrv.c $(common_h) $(dscparse_h) \
 $(errors_h) $(iapi_h) \
 $(capp_h) $(cdll_h) $(cimg_h) $(copt_h) $(cdisplay_h) $(cdoc_h) \
 $(cgsdll_h) $(cmsg_h) $(cpdf_h) $(cpdfscan_h) $(cview_h) $(cgssrv_h)
	$(COMP) $(FOO)cgssrv$(OBJ) $(CO) $(SRC)cgssrv.c

$(OD)chist$(OBJ): $(SRC)chist.c $(common_h) $(chist_h)
	$(COMP) $(FOO)chist$(OBJ) $(CO) $(SRC)chist.c

$(OD)cimg$(OBJ): $(SRC)cimg.c $(common_h) $(gdevdsp_h) $(cimg_h) $(clzw_h)
	$(COMP) $(FOO)cimg$(OBJ) $(CO) $(SRC)cimg.c

$(OD)clzw$(OBJ): $(SRC)clzw.c $(clzw_h)
	$(COMP) $(FOO)clzw$(OBJ) $(CO) $(SRC)clzw.c

$(OD)cmac$(OBJ): $(SRC)cmac.c $(cmac_h) $(common_h)
	$(COMP) $(FOO)cmac$(OBJ) $(CO) $(SRC)cmac.c

$(OD)cmbcs$(OBJ): $(SRC)cmbcs.c $(common_h)
	$(COMP) $(FOO)cmbcs$(OBJ) $(CO) $(SRC)cmbcs.c

$(OD)copt$(OBJ): $(SRC)copt.c $(common_h) $(dscparse_h) $(copt_h) $(cprofile_h)
	$(COMP) $(FOO)copt$(OBJ) $(CO) $(SRC)copt.c

$(OD)cpagec$(OBJ): $(SRC)cpagec.c $(common_h) $(dscparse_h) \
 $(capp_h) $(cgssrv_h) $(cimg_h) $(cpagec_h) $(cpdf_h) $(cview_h)
	$(COMP) $(FOO)cpagec$(OBJ) $(CO) $(SRC)cpagec.c

$(OD)cpdf$(OBJ): $(SRC)cpdf.c $(common_h) $(dscparse_h) \
 $(capp_h) $(cpdf_h) $(cgssrv_h) $(cview_h)
	$(COMP) $(FOO)cpdf$(OBJ) $(CO) $(SRC)cpdf.c

$(OD)cpdfscan$(OBJ): $(SRC)cpdfscan.c $(common_h) $(dscparse_h) \
 $(capp_h) $(cpdfscan_h)
	$(COMP) $(FOO)cpdfscan$(OBJ) $(CO) $(SRC)cpdfscan.c

$(OD)cpldll$(OBJ): $(SRC)cpldll.c $(common_h) $(errors_h) $(iapi_h) \
 $(capp_h) $(cdll_h) $(cpldll_h)
	$(COMP) $(FOO)cpldll$(OBJ) $(CO) $(SRC)cpldll.c

$(OD)cplsrv$(OBJ): $(SRC)cplsrv.c $(common_h) $(dscparse_h) \
 $(errors_h) $(iapi_h) \
 $(capp_h) $(cdll_h) $(cimg_h) $(copt_h) $(cdisplay_h) $(cdoc_h) \
 $(cgsdll_h) $(cmsg_h) $(cpdf_h) $(cpdfscan_h) $(cview_h) $(cgssrv_h)
	$(COMP) $(FOO)cplsrv$(OBJ) $(CO) $(SRC)cplsrv.c

$(OD)cprofile$(OBJ): $(SRC)cprofile.c $(cplat_h) $(cprofile_h)
	$(COMP) $(FOO)cprofile$(OBJ) $(CO) $(SRC)cprofile.c

$(OD)cps$(OBJ): $(SRC)cps.c $(common_h) $(dscparse_h) \
 $(capp_h) $(cdoc_h)
	$(COMP) $(FOO)cps$(OBJ) $(CO) $(SRC)cps.c

$(OD)cvcmd$(OBJ): $(SRC)cvcmd.c $(common_h) $(dscparse_h) \
 $(copt_h) $(capp_h) $(cdoc_h) $(cgssrv_h) $(chist_h) \
 $(cmsg_h) $(cpdfscan_h) $(cps_h) $(cres_h) $(cvcmd_h) $(cview_h)
	$(COMP) $(FOO)cvcmd$(OBJ) $(CO) $(SRC)cvcmd.c

$(OD)cview$(OBJ): $(SRC)cview.c $(common_h) $(dscparse_h) \
 $(capp_h) $(cdoc_h) $(cgssrv_h) $(chist_h) $(cimg_h) \
 $(cmsg_h) $(copt_h) $(cpagec_h) $(cpdf_h) $(cpdfscan_h) $(cres_h) \
 $(cvcmd_h) $(cview_h)
	$(COMP) $(FOO)cview$(OBJ) $(CO) $(SRC)cview.c

$(OD)dscparse$(OBJ): $(SRC)dscparse.c $(dscparse_h)
	$(COMP) $(FOO)dscparse$(OBJ) $(CO) $(SRC)dscparse.c

$(OD)dscutil$(OBJ): $(SRC)dscutil.c $(dscparse_h)
	$(COMP) $(FOO)dscutil$(OBJ) $(CO) $(SRC)dscutil.c

$(BD)epstool$(EXE): $(OD)lib.rsp $(EPSOBJS)
	$(CLINK) $(FE)$(BD)epstool$(EXE) $(EPSOBJS) $(EPSLIB)

$(OD)epstool$(OBJ): $(SRC)epstool.c $(SRC)common.mak \
 $(common_h) $(copt_h) $(capp_h) $(cbmp_h) $(cdoc_h) $(cdll_h) \
 $(ceps_h) $(cimg_h) $(cmac_h) $(cps_h) $(cres_h) \
 $(dscparse_h) $(errors_h) $(iapi_h) $(gdevdsp_h)
	$(COMP) $(FOO)epstool$(OBJ) $(CO) -DEPSTOOL_VERSION="$(EPSTOOL_VERSION)" -DEPSTOOL_DATE="$(EPSTOOL_DATE)" $(SRC)epstool.c

$(BD)epstest$(EXE): $(OD)lib.rsp $(EPSTESTOBJS)
	$(CLINK) $(FE)$(BD)epstest$(EXE) $(EPSTESTOBJS) $(EPSLIB)

$(OD)epstest$(OBJ): $(SRC)epstest.c $(SRC)common.mak \
 $(common_h) $(copt_h) $(capp_h) $(cbmp_h) $(cdoc_h) $(cdll_h) \
 $(ceps_h) $(cimg_h) $(cmac_h) $(cps_h) $(cres_h) \
 $(dscparse_h) $(errors_h) $(iapi_h) $(gdevdsp_h)
	$(COMP) $(FOO)epstest$(OBJ) $(CO) $(SRC)epstest.c


$(BD)dscparse$(EXE): (OD)dscparse$(OBJ) $(SRC)dscutil.c
	$(CC) $(CFLAGS) $(GSCFLAGS) -DSTANDALONE $(FOO)dscutils$(OBJ) $(CO) $(SRC)dscutil.c
	$(CLINK) $(CFLAGS) $(GSCFLAGS) $(FEO)dscparse$(EXE) $(OD)dscparse$(OBJ) $(OD)dscutils$(OBJ)

# X11/gtk+ specific

$(OD)xdll$(OBJ): $(SRC)xdll.c $(common_h) $(cdll_h)
	$(COMP) $(FOO)xdll$(OBJ) $(CO) $(SRC)xdll.c

$(OD)xnodll$(OBJ): $(SRC)xnodll.c $(common_h) $(cdll_h)
	$(COMP) $(FOO)xnodll$(OBJ) $(CO) $(SRC)xnodll.c


# Build distribution files for epstool
EPSDIST=epstool-$(EPSTOOL_VERSION)
EPSZIP=epstool-$(EPSTOOL_VERSION).zip
EPSTAR=epstool-$(EPSTOOL_VERSION).tar
EPSTARGZ=$(EPSTAR).gz

epssrc:
	-$(RMDIR) $(EPSDIST)
	-mkdir $(EPSDIST)
	-mkdir $(EPSDIST)$(DD)$(SRCDIR)
	-mkdir $(EPSDIST)$(DD)$(SRCWINDIR)
	-mkdir $(EPSDIST)$(DD)doc
	$(CP) $(SRC)calloc.* $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)capp.* $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)cargs.h $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)cbmp.* $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)cdll.h $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)cdoc.* $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)ceps.* $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)cfile.* $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)clfile.* $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)clzw.* $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)cgssrv.h $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)cimg.* $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)cmac.* $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)cmbcs.* $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)common.h $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)copt.h $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)cpagec.h $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)cpdfscan.* $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)cplat.h $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)cprofile.* $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)cps.* $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)cres.h $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)dscparse.* $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)dscutil.c $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)epstool.c $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)epstool.mak $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)errors.h $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)iapi.h $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)gdevdsp.h $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)xdll.c $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)xnodll.c $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)common.mak $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)os2.mak $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)unix.mak $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)unixcom.mak $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRC)epstool.spec $(EPSDIST)$(DD)$(SRCDIR)
	$(CP) $(SRCWIN)wdll.c $(EPSDIST)$(DD)$(SRCWIN)
	$(CP) $(SRCWIN)wfile.c $(EPSDIST)$(DD)$(SRCWIN)
	$(CP) $(SRCWIN)wgsver.c $(EPSDIST)$(DD)$(SRCWIN)
	$(CP) $(SRCWIN)wgsver.h $(EPSDIST)$(DD)$(SRCWIN)
	$(CP) $(SRCWIN)windows.mak $(EPSDIST)$(DD)$(SRCWIN)
	$(CP) $(SRCWIN)epstool.mak $(EPSDIST)$(DD)$(SRCWIN)
	$(CP) doc$(DD)epstool.htm $(EPSDIST)$(DD)doc
	$(CP) doc$(DD)epstool.1 $(EPSDIST)$(DD)doc
	$(CP) doc$(DD)gsview.css $(EPSDIST)$(DD)doc
	$(CP) doc$(DD)cygwin.README $(EPSDIST)$(DD)doc
	$(CP) doc$(DD)cygwin.hint $(EPSDIST)$(DD)doc
	$(CP) $(SRC)epstool.mak $(EPSDIST)$(DD)makefile
	echo "Documentation for epstool is in doc/epstool.htm" > epstool.txt
	$(CP) epstool.txt $(EPSDIST)$(DD)epstool.txt
	$(CP) LICENCE $(EPSDIST)


$(EPSZIP): epssrc $(BD)epstool$(EXE)
	$(CP) $(SRCWIN)epstool.mak $(EPSDIST)$(DD)makefile
	-$(RM) $(EPSZIP)
	-mkdir $(EPSDIST)$(DD)bin
	$(CP) $(BD)epstool$(EXE) $(EPSDIST)$(DD)bin$(DD)epstool$(EXE)
	zip -r -X -9 $(EPSZIP) $(EPSDIST)

epszip: $(EPSZIP)

$(EPSTAR): epssrc
	-$(RM) $(EPSTAR)
	tar -cvf $(EPSTAR) $(EPSDIST)

$(EPSTARGZ): $(EPSTAR)
	-$(RM) $(EPSTARGZ)
	gzip $(EPSTAR)

epstar: $(EPSTARGZ)

