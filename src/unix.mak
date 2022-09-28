# Copyright (C) 2002-2005 Ghostgum Software Pty Ltd.  All rights reserved.
#
#  This software is provided AS-IS with no warranty, either express or
#  implied.
#
#  This software is distributed under licence and may not be copied,
#  modified or distributed except as expressly authorised under the terms
#  of the licence contained in the file LICENCE in this distribution.
#

# $Id: unix.mak,v 1.24 2005/06/10 09:39:24 ghostgum Exp $
# Unix makefile for GSview

BINDIR=./bin
OBJDIR=./obj
SRCDIR=./src
SRCWINDIR=./srcwin

XINCLUDE=
PFLAGS=-DMULTITHREAD
PLINK=-lpthread -lrt

GTKCFLAGS=-DGTK `pkg-config --cflags gtk+-2.0` -DGTK_DISABLE_DEPRECATED -DDEBUG_MALLOC
GTKLIBS=`pkg-config --libs gtk+-2.0`

LIBPNGINC=-Ilibpng -Izlib
LIBPNGCFLAGS=-DHAVE_LIBPNG
LIBPNGLIBS=-lpng

#LONGFILEDEF=-DLARGEFILES -DFILE_OFFSET="long long" -DDSC_OFFSET="unsigned long long" -DDSC_OFFSET_FORMAT=\"llu\" -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
#LONGFILEMOD=clfile
LONGFILEDEF=
LONGFILEMOD=cfile

include $(SRCDIR)/unixcom.mak

OBJPLAT=$(OD)xapp$(OBJ) $(OD)xdll$(OBJ) $(OD)xdlg$(OBJ) \
 $(OD)xdoc$(OBJ) \
 $(OD)xgsimg$(OBJ) $(OD)xgssrv$(OBJ) $(OD)ximg$(OBJ) \
 $(OD)xmain$(OBJ) $(OD)xmenu$(OBJ) $(OD)xopt$(OBJ) \
 $(OD)xview$(OBJ) $(OD)$(LONGFILEMOD)$(OBJ)

xview_h=$(SRC)xview.h

XPM=$(SRCDIR)/xpm/$(NUL)
XPMBUTTONS=$(XPM)xback.xpm $(XPM)xfwd.xpm $(XPM)xgoto.xpm $(XPM)xhelp.xpm \
 $(XPM)xinfo.xpm $(XPM)xmagm.xpm $(XPM)xmagp.xpm $(XPM)xnext.xpm \
 $(XPM)xnexts.xpm $(XPM)xopen.xpm $(XPM)xprev.xpm $(XPM)xprevs.xpm \
 $(XPM)xprint.xpm

BEGIN=$(OD)lib.rsp
TARGET=$(BD)gsview$(EXE)

include $(SRCDIR)/common.mak

$(TARGET): $(OBJS)
	$(LINK) $(FE)$(TARGET) $(OBJS) $(LFLAGS)

$(OD)lib.rsp: makefile
	-mkdir $(BINDIR)
	-mkdir $(OBJDIR)
	echo "dummy" > $(OD)lib.rsp

# X11/gtk+ specific

$(OD)xapp$(OBJ): $(SRC)xapp.c $(common_h) $(dscparse_h) \
 $(copt_h) $(capp_h) $(cres_h) $(cver_h) $(cview_h) \
 en/clang.h en/clang.rc $(SRC)xlang.rc
	$(COMP) -Ien -I. $(FOO)xapp$(OBJ) $(CO) $(SRC)xapp.c

$(OD)xdlg$(OBJ): $(SRC)xdlg.c $(common_h) $(dscparse_h) \
 $(capp_h) $(cres_h) $(cview_h)
	$(COMP) $(FOO)xdlg$(OBJ) $(CO) $(SRC)xdlg.c

$(OD)xdoc$(OBJ): $(SRC)xdoc.c $(common_h) $(dscparse_h) $(capp_h) $(cdoc_h)
	$(COMP) $(FOO)xdoc$(OBJ) $(CO) $(SRC)xdoc.c

$(OD)xgsimg$(OBJ): $(SRC)xgsimg.c $(common_h) $(gdevdsp_h) $(errors_h) \
 $(capp_h) $(cimg_h) $(cdisplay_h)
	$(COMP) $(FOO)xgsimg$(OBJ) $(CO) $(SRC)xgsimg.c

$(OD)xgssrv$(OBJ): $(SRC)xgssrv.c $(common_h) $(gdevdsp_h) $(dscparse_h) \
 $(capp_h) $(cimg_h) $(cdisplay_h) $(copt_h) $(cpdf_h) $(cgssrv_h)
	$(COMP) $(FOO)xgssrv$(OBJ) $(CO) $(SRC)xgssrv.c

$(OD)ximg$(OBJ): $(SRC)ximg.c $(common_h) $(gdevdsp_h) $(errors_h) $(cimg_h)
	$(COMP) $(FOO)ximg$(OBJ) $(CO) $(SRC)ximg.c

$(OD)xmain$(OBJ): $(SRC)xmain.c $(common_h) $(errors_h) $(iapi_h) \
 $(dscparse_h) $(copt_h) $(capp_h) $(cargs_h) $(cdoc_h) \
 $(cdll_h) $(cgsdll_h) $(cmsg_h) $(cview_h) $(cvcmd_h)
	$(COMP) $(FOO)xmain$(OBJ) $(CO) $(SRC)xmain.c

$(OD)xmenu$(OBJ): $(SRC)xmenu.c $(common_h) $(copt_h) $(cres_h) \
 $(cvcmd_h) $(cview_h) $(xview_h) $(XPMBUTTONS)
	$(COMP) $(FOO)xmenu$(OBJ) $(CO) $(SRC)xmenu.c

$(OD)xopt$(OBJ): $(SRC)xopt.c $(common_h) $(dscparse_h) \
 $(capp_h) $(cres_h) $(cview_h)
	$(COMP) $(FOO)xopt$(OBJ) $(CO) $(SRC)xopt.c

$(OD)xview$(OBJ): $(SRC)xview.c $(common_h) \
 $(errors_h) $(iapi_h) $(gdevdsp_h) \
 $(dscparse_h) $(copt_h) $(capp_h) $(cimg_h) $(cdisplay_h) $(cmsg_h) \
 $(dpagec_h) $(cgssrv_h) $(cvcmd_h) $(cview_h) $(xview_h)
	$(COMP) $(FOO)xview$(OBJ) $(CO) $(SRC)xview.c

# Languages
$(BD)en.txt: $(SRC)xlang.c $(cver_h) $(cres_h) $(SRC)xlang.rc \
 en/clang.h en/clang.rc
	$(COMP) -Ien $(FEO)xlangen$(EXE) $(SRC)xlang.c
	$(BD)xlangen$(EXE) > $(BD)en.txt


clean:
	-$(RM) $(OBJS)
	-$(RM) $(TARGET)
	-$(RM) $(OD)lib.rsp
	-rmdir $(BINDIR) $(OBJDIR) 

