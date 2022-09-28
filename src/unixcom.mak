# Copyright (C) 2002-2005 Ghostgum Software Pty Ltd.  All rights reserved.
#
#  This software is provided AS-IS with no warranty, either express or
#  implied.
#
#  This software is distributed under licence and may not be copied,
#  modified or distributed except as expressly authorised under the terms
#  of the licence contained in the file LICENCE in this distribution.
#

# $Id: unixcom.mak,v 1.2 2005/01/11 11:40:19 ghostgum Exp $
# Unix common makefile


INSTALL=install -m 644
INSTALL_EXE=install -m 755

MAKE=make
CDEFS=-DX11 -DUNIX -DNONAG $(LONGFILEDEF)
GSCDEBUG= -g
GSCFLAGS= $(CDEFS) -Wall -Wstrict-prototypes -Wmissing-declarations -Wmissing-prototypes -fno-builtin -fno-common -Wcast-qual -Wwrite-strings $(CDEBUG) $(GSCDEBUG) $(RPM_OPT_FLAGS) $(XINCLUDE) $(PFLAGS) $(LIBPNGCFLAGS) $(GTKCFLAGS)
CCAUX=gcc
CC=gcc
LFLAGS=$(PLINK) $(LIBPNGLIBS) $(GTKLIBS)
CLINK=gcc $(LDFLAGS)
LINK=gcc $(LDFLAGS)


COMP=$(CC) -I$(SRCDIR) -I$(OBJDIR) $(CFLAGS) $(GSCFLAGS)


NUL=
DD=/$(NUL)
SRC=$(SRCDIR)/$(NUL)
SRCWIN=$(SRCWINDIR)/$(NUL)
OD=$(OBJDIR)/$(NUL)
BD=$(BINDIR)/$(NUL)
OBJ=.o
EXE=
CO=-c

FE=-o $(NUL)
FO=-o $(NUL)
FEO=-o $(OD)
FOO=-o $(OD)

CP=cp -f
RM=rm -f
RMDIR=rm -rf

