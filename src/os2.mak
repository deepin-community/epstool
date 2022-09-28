# Copyright (C) 2005 Ghostgum Software Pty Ltd.  All rights reserved.
#
#  This software is provided AS-IS with no warranty, either express or
#  implied.
#
#  This software is distributed under licence and may not be copied,
#  modified or distributed except as expressly authorised under the terms
#  of the licence contained in the file LICENCE in this distribution.
#

# $Id: os2.mak,v 1.1 2005/06/09 23:02:02 ghostgum Exp $
# Make epstool for OS/2

BINDIR=bin
OBJDIR=epsobj
SRCDIR=src
SRCWINDIR=srcwin

XINCLUDE=
PFLAGS=
PLINK=

GTKCFLAGS=
GTKLIBS=

LIBPNGINC=
LIBPNGCFLAGS=
LIBPNGLIBS=

LONGFILEDEF=
LONGFILEMOD=cfile

INSTALL=copy 
INSTALL_EXE=copy

MAKE=nmake
EMXOMF=-Zomf -Zmts
CDEFS=-DOS2 -DNONAG $(LONGFILEDEF)
GSCDEBUG= -g
GSCFLAGS= $(CDEFS) $(EMXOMF) -Wall -Wstrict-prototypes -Wmissing-declarations -Wmissing-prototypes -fno-builtin -fno-common -Wcast-qual -Wwrite-strings $(CDEBUG) $(GSCDEBUG) $(RPM_OPT_FLAGS) $(XINCLUDE) $(PFLAGS) $(LIBPNGCFLAGS) $(GTKCFLAGS)
CCAUX=gcc
CC=gcc
LFLAGS=$(PLINK) $(LIBPNGLIBS) $(GTKLIBS)
CLINK=gcc $(LDFLAGS) $(EMXOMF)
LINK=gcc $(LDFLAGS) $(EMXOMF)

COMP=$(CC) -I$(SRCDIR) -I$(OBJDIR) $(CFLAGS) $(GSCFLAGS)

NUL=
DD=\$(NUL)
SRC=$(SRCDIR)\$(NUL)
SRCWIN=$(SRCWINDIR)\$(NUL)
OD=$(OBJDIR)\$(NUL)
BD=$(BINDIR)\$(NUL)
OBJ=.obj
#OBJ=.o
EXE=.exe
CO=-c

FE=-o $(NUL)
FO=-o $(NUL)
FEO=-o $(OD)
FOO=-o $(OD)

CP=cp -f
RM=rm -f
RMDIR=rm -rf

EPSOBJPLAT=$(OD)xnodll$(OBJ) $(OD)$(LONGFILEMOD)$(OBJ)
EPSLIB=$(LIBPNGLIBS)

BEGIN=$(OD)lib.rsp
TARGET=epstool

!include $(SRCDIR)/common.mak

EPSTOOL_ROOT=/usr/local
EPSTOOL_BASE=$(prefix)$(EPSTOOL_ROOT)
EPSTOOL_DOCDIR=$(EPSTOOL_BASE)/share/doc/epstool-$(EPSTOOL_VERSION)
EPSTOOL_MANDIR=$(EPSTOOL_BASE)/man
EPSTOOL_BINDIR=$(EPSTOOL_BASE)/bin

epstool: $(BD)epstool$(EXE)

epstest: epstool $(BD)epstest$(EXE)
	$(BD)epstest$(EXE)

$(OD)lib.rsp: makefile
	-mkdir $(BINDIR)
	-mkdir $(OBJDIR)
	echo "dummy" > $(OD)lib.rsp

install: $(TARGET)
	-mkdir -p $(EPSTOOL_BASE)
	chmod a+rx $(EPSTOOL_BASE)
	-mkdir -p $(EPSTOOL_BINDIR)
	chmod a+rx $(EPSTOOL_BINDIR)
	$(INSTALL_EXE) $(BD)epstool$(EXE) $(EPSTOOL_BINDIR)$(DD)epstool$(EXE)
	-strip  $(EPSTOOL_BINDIR)$(DD)epstool$(EXE)
	-mkdir -p $(EPSTOOL_MANDIR)
	chmod a+rx $(EPSTOOL_MANDIR)
	-mkdir -p $(EPSTOOL_MANDIR)$(DD)man1
	chmod a+rx $(EPSTOOL_MANDIR)$(DD)man1
	$(INSTALL) doc$(DD)epstool.1 $(EPSTOOL_MANDIR)$(DD)man1$(DD)epstool.1
	-mkdir -p $(EPSTOOL_DOCDIR)
	chmod a+rx $(EPSTOOL_DOCDIR)
	$(INSTALL) doc$(DD)epstool.htm $(EPSTOOL_DOCDIR)$(DD)epstool.htm
	$(INSTALL) doc$(DD)gsview.css $(EPSTOOL_DOCDIR)$(DD)gsview.css
	$(INSTALL) LICENCE $(EPSTOOL_DOCDIR)$(DD)LICENCE



clean:
	-$(RM) $(EPSOBJS)
	-$(RM) $(EPSTESTOBJS)
	-$(RM) $(OD)lib.rsp
	-$(RM) $(BD)epstool$(EXE)
	-$(RM) $(BD)epstest$(EXE)
	-rmdir $(OBJDIR)

