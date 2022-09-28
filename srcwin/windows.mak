# Copyright (C) 2002-2015 Ghostgum Software Pty Ltd.  All rights reserved.
#
#  This software is provided AS-IS with no warranty, either express or
#  implied.
#
#  This software is distributed under licence and may not be copied,
#  modified or distributed except as expressly authorised under the terms
#  of the licence contained in the file LICENCE in this distribution.
#

# $Id: windows.mak,v 1.1.2.25 2005/06/10 09:39:24 ghostgum Exp $
# Windows makefile for MSVC

# Try to detect the compiler version
!if defined(_NMAKE_VER) && !defined(VCVER)
!if "$(_NMAKE_VER)" == "162"
VCVER=5
!endif
!if "$(_NMAKE_VER)" == "6.00.8168.0"
VCVER=6
!endif
!if "$(_NMAKE_VER)" == "7.00.9466"
VCVER=7
!endif
!if "$(_NMAKE_VER)" == "7.10.3077"
VCVER=71
!endif
!if "$(_NMAKE_VER)" == "8.00.40607.16"
VCVER=8
!endif
!if "$(_NMAKE_VER)" == "9.00.21022.08"
VCVER=9
!endif
!if "$(_NMAKE_VER)" == "9.00.30729.01"
VCVER=9
!endif
!if "$(_NMAKE_VER)" == "10.00.30319.01"
VCVER=10
!endif
!endif

# Edit VCVER and DEVBASE as required
!ifndef VCVER
VCVER=71
!endif

!ifndef VCDRIVE
VCDRIVE=C:
!endif

# Win64 requires Microsoft Visual Studio 8 (.NET 2005)
# or Microsoft Visual Studio .NET 2003 with Windows Server 2003 DDK.
# To compile for Win64, use "nmake WIN64=1"
!ifdef WIN64
WIN32=0
WIN64=1
!else
WIN32=1
WIN64=0
!endif


# When compiling on Win64, x86 programs are in different place
!ifndef PROGFILESX86
PROGFILESX86=Program Files
#PROGFILESX86=Program Files (x86)
!endif


!if $(VCVER) <= 5
DEVBASE=$(VCDRIVE)\$(PROGFILESX86)\devstudio
!endif
!if $(VCVER) == 6
DEVBASE=$(VCDRIVE)\$(PROGFILESX86)\Microsoft Visual Studio
!endif
!if $(VCVER) == 7
DEVBASE=$(VCDRIVE)\$(PROGFILESX86)\Microsoft Visual Studio .NET
!endif
!if $(VCVER) == 71
DEVBASE=$(VCDRIVE)\$(PROGFILESX86)\Microsoft Visual Studio .NET 2003
DDKBASE=$(VCDRIVE)\winddk\3790
!endif
!if $(VCVER) == 8
DEVBASE=$(VCDRIVE)\$(PROGFILESX86)\Microsoft Visual Studio 8
!endif
!if $(VCVER) == 9
DEVBASE=$(PROGRAMFILES)\Microsoft Visual Studio 9.0
COMMONBASE=$(PROGRAMFILES)\Microsoft SDKs\Windows\v6.0A
!endif
!if $(VCVER) == 10
DEVBASE=$(PROGRAMFILES)\Microsoft Visual Studio 10.0
COMMONBASE=$(PROGRAMFILES)\Microsoft SDKs\Windows\v6.0A
!endif

# DEBUG=1 for Debugging options
DEBUG=1

BINDIR=.\bin
OBJDIR=.\obj
SRCDIR=.\src
SRCWINDIR=.\srcwin

!ifndef LIBPNGINC
LIBPNGINC=-Ilibpng -Izlib
LIBPNGCFLAGS=-DHAVE_LIBPNG -DPNG_USE_DLL
LIBPNGLIBS=libpng\libpng.lib zlib\zlib.lib
!endif

!if $(VCVER) <= 5
COMPBASE=$(DEVBASE)\vc
!endif
!if $(VCVER) == 6
COMPBASE=$(DEVBASE)\vc98
!endif
!if ($(VCVER) == 7) || ($(VCVER) == 71) 
COMPBASE=$(DEVBASE)\Vc7
PLATLIBDIR=$(COMPBASE)\PlatformSDK\lib
!endif
!if $(VCVER) == 8
COMPBASE = $(DEVBASE)\VC
PLATLIBDIR=$(COMPBASE)\PlatformSDK\lib
!endif
!if ($(VCVER) == 9)
COMPBASE = $(DEVBASE)\VC
PLATLIBDIR=$(COMMONBASE)\lib
INCPLAT=-I"$(COMMONBASE)\Include"
!endif
!if ($(VCVER) == 10)
COMPBASE = $(DEVBASE)\VC
PLATLIBDIR=$(COMMONBASE)\lib
INCPLAT=-I"$(COMMONBASE)\Include"
!endif

# MSVC 8 (2005) warns about deprecated common functions like fopen.
!if $(VCVER) == 8
VC8WARN=/wd4996
!else
VC8WARN=
!endif

COMPDIR=$(COMPBASE)\bin
INCDIR=$(COMPBASE)\include
LIBDIR=$(COMPBASE)\lib
!ifndef PLATLIBDIR
PLATLIBDIR=$(LIBDIR)
!endif

!ifdef UNICODE
UNICODEDEF=-DUNICODE
!endif

CDEFS=-D_Windows -D__WIN32__ $(INCPLAT) -I"$(INCDIR)" $(UNICODEDEF) $(LIBPNGCFLAGS) $(LARGEFILES)
WINEXT=32
CFLAGS=$(CDEFS) /MT /nologo /W4 $(VC8WARN)

!if $(DEBUG)
DEBUGLINK=/DEBUG
CDEBUG=/Zi
!endif

!if $(WIN32)
MODEL=32
CCAUX = "$(COMPDIR)\cl" -I"$(INCDIR)"
CC = "$(COMPDIR)\cl" $(CDEBUG)
CLINK=$(CC)
LINK = "$(COMPDIR)\link"
LINKMACHINE=IX86

!else if $(WIN64)
MODEL=32
CCAUX = "$(COMPDIR)\cl" -I"$(INCDIR)"
!if $(VCVER) == 71
LINKMACHINE=X86
CC="$(DDKBASE)\bin\win64\x86\amd64\cl" $(CDEBUG)
CLINK=$(CC)
LINK="$(DDKBASE)\bin\win64\x86\amd64\link"
LIBDIR=$(DDKBASE)\lib\wnet\amd64
PLATLIBDIR=$(DDKBASE)\lib\wnet\amd64
!else
LINKMACHINE=X64
CC="$(COMPDIR)\x86_amd64\cl" $(CDEBUG)
CLINK=$(CC)
LINK="$(COMPDIR)\x86_amd64\link"
LIBDIR=$(COMPBASE)\lib\amd64
PLATLIBDIR=$(COMPBASE)\PlatformSDK\Lib\AMD64
!endif
!endif


CLFLAG=
RIFLAGS=-i"$(INCDIR)" -i"$(SRCDIR)" -i"$(SRCWINDIR)" -i"$(OBJDIR)"
!if $(VCVER) <= 5
HC="$(COMPDIR)\hcw" /C /E
RCOMP="$(DEVBASE)\sharedide\bin\rc" -D_MSC_VER $(CDEFS) $(RIFLAGS)
!endif
!if $(VCVER) == 6
HC="$(DEVBASE)\common\tools\hcw" /C /E
RCOMP="$(DEVBASE)\common\msdev98\bin\rc" -D_MSC_VER $(CDEFS) $(RIFLAGS)
!endif
!if ($(VCVER) == 7) || ($(VCVER) == 71) 
HC="$(DEVBASE)\Common7\Tools\hcw" /C /E
RCOMP="$(DEVBASE)\Vc7\bin\rc" -D_MSC_VER $(CDEFS) $(RIFLAGS)
!endif
!if $(VCVER) == 8
HC="$(DEVBASE)\VC\bin\hcw" /C /E
RCOMP="$(DEVBASE)\VC\bin\rc" -D_MSC_VER $(CDEFS) $(RIFLAGS)
!endif
!if $(VCVER) == 9
# Help Compiler is no longer included in the SDK.
# Search on the Internet for hcw403_setup.zip
HC="$(PROGRAMFILES)\Help Workshop\hcw" /C /E
HTMLHELP="C:\Program Files\HTML Help Workshop\hhc.exe"
RCOMP="$(COMMONBASE)\bin\rc" -D_MSC_VER $(CDEFS) $(RIFLAGS)
!endif
!if $(VCVER) == 10
# Help Compiler is no longer included in the SDK.
# Search on the Internet for hcw403_setup.zip
HC="$(PROGRAMFILES)\Help Workshop\hcw" /C /E
HTMLHELP="C:\Program Files\HTML Help Workshop\hhc.exe"
RCOMP="$(COMMONBASE)\bin\rc" -D_MSC_VER $(CDEFS) $(RIFLAGS)
!endif

COMP=$(CC) -I$(SRCDIR) -I$(SRCWINDIR) -I$(OBJDIR) $(CFLAGS)


NUL=
DD=\$(NUL)
SRC=$(SRCDIR)\$(NUL)
SRCWIN=$(SRCWINDIR)\$(NUL)
OD=$(OBJDIR)\$(NUL)
BD=$(BINDIR)\$(NUL)
OBJ=.obj
EXE=.exe
CO=-c

FE=-Fe
FO=-Fo
FEO=-Fe$(OD)
FOO=-Fo$(OD)

CP=copy
RM=del
RMDIR=rmdir /s /q

# simple viewer 
OBJPLAT1=$(OD)wapp$(OBJ) $(OD)wdll$(OBJ) $(OD)wdoc$(OBJ) \
 $(OD)wgsimg$(OBJ) $(OD)wgssrv$(OBJ) $(OD)wgsver$(OBJ) $(OD)wimg$(OBJ) \
 $(OD)wfile$(OBJ)
VIEWAPIOBJ=$(OBJPLAT1) $(OBJCOM1) $(OBJCOM2) $(OD)wviewapi$(OBJ)

# complex viewer
OBJPLAT2=$(OD)wview$(OBJ) $(OD)wviewwin$(OBJ) $(OD)main$(OBJ)
OBJPLAT=$(OBJPLAT1) $(OBJPLAT2)

# another complex viewer
VIEWOBJ=$(OBJPLAT1) $(OBJCOM1) $(OBJCOM2) \
 $(OD)wtest2$(OBJ) $(OD)wtest3$(OBJ) $(OD)cvcmd$(OBJ)

# epstool
EPSOBJPLAT=$(OD)wdll$(OBJ) $(OD)wgsver$(OBJ) $(OD)wfile$(OBJ)
EPSLIB="$(PLATLIBDIR)\advapi32.lib" $(LIBPNGLIBS) /link /LIBPATH:"$(LIBDIR)"

HDRSPLAT=$(SRCWIN)wimg.h $(SRCWIN)wgsver.h


OBJSBEGIN=$(OD)lib.rsp
#TARGET=$(BD)main$(EXE)
#TARGET=$(BD)epstool$(EXE)
TARGET=gsview_test1

!include $(SRC)common.mak

$(OD)lib.rsp: makefile
	-mkdir $(BINDIR)
	-mkdir $(OBJDIR)
        echo "$(PLATLIBDIR)\shell32.lib" > $(OD)lib.rsp
        echo "$(PLATLIBDIR)\comdlg32.lib" >> $(OD)lib.rsp
        echo "$(PLATLIBDIR)\gdi32.lib" >> $(OD)lib.rsp
        echo "$(PLATLIBDIR)\user32.lib" >> $(OD)lib.rsp
        echo "$(PLATLIBDIR)\winspool.lib" >> $(OD)lib.rsp
        echo "$(PLATLIBDIR)\advapi32.lib" >> $(OD)lib.rsp
        echo "$(PLATLIBDIR)\ole32.lib" >> $(OD)lib.rsp
        echo "$(PLATLIBDIR)\uuid.lib" >> $(OD)lib.rsp
	echo /NODEFAULTLIB:LIBC.lib >> $(OD)lib.rsp
        echo "$(LIBDIR)\libcmt.lib" >> $(OD)lib.rsp
!if "$(LIBPNGLIBS)" != ""
	echo $(LIBPNGLIBS) >> $(OD)lib.rsp
!endif


# A command line program for testing GSview on Windows.

$(BD)main.exe: $(OBJS) $(OD)lib.rsp
	echo $(OBJS) > $(OD)link.rsp
	$(LINK) $(DEBUGLINK) /OUT:$(BD)main.exe @$(OD)link.rsp @$(OD)lib.rsp

$(OD)main.obj: $(SRCWIN)main.c $(HDRS)
	$(COMP) $(FOO)main$(OBJ) $(CO) $(SRCWIN)main.c

# A DLL with a simplified interface to GSview.  This is intended
# to be used by an ActiveX control.

gsview_test1: $(BD)gsview_test1.dll $(BD)wviewapi_example.exe $(BD)wtest1.exe $(BD)wtest3.exe

$(BD)gsview_test1.dll: $(VIEWAPIOBJ) $(OD)lib.rsp
	echo $(VIEWAPIOBJ) > $(OD)link.rsp
	$(LINK) $(DEBUGLINK) /DLL /OUT:$(BD)gsview_test1.dll @$(OD)link.rsp @$(OD)lib.rsp

$(OD)wviewapi.obj: $(SRCWIN)wviewapi.c $(HDRS)
	$(COMP) $(FOO)wviewapi$(OBJ) $(CO) $(SRCWIN)wviewapi.c

$(BD)wviewapi_example.exe: $(SRCWIN)wviewapi_example.c $(SRCWIN)wviewapi.h $(BD)gsview_test1.dll
	$(COMP) $(FE)$(BD)wviewapi_example.exe $(SRCWIN)wviewapi_example.c $(BD)gsview_test1.lib

$(BD)wtest1$(EXE): $(OD)wtest1$(OBJ) $(OD)wtest2$(OBJ) $(OD)wtest1.res $(SRCWIN)wviewapi.h $(BD)gsview_test1.dll $(OD)lib.rsp
	$(LINK) $(DEBUGLINK) /SUBSYSTEM:WINDOWS /OUT:$(BD)wtest1$(EXE) $(OD)wtest1$(OBJ) $(OD)wtest2$(OBJ) $(BD)gsview_test1.lib @$(OD)lib.rsp $(OD)wtest1.res

$(BD)wtest3$(EXE): $(VIEWOBJ) $(OD)wtest1.res $(OD)lib.rsp
	$(LINK) $(DEBUGLINK) /SUBSYSTEM:WINDOWS /OUT:$(BD)wtest3$(EXE) $(VIEWOBJ) @$(OD)lib.rsp $(OD)wtest1.res

$(OD)wtest1.res: $(SRCWIN)wtest1.h $(SRCWIN)wtest1.rc
	$(RCOMP) -i"srcwin" -fo$(OD)wtest1.res $(SRCWIN)wtest1


# Windows specific

$(OD)wapp$(OBJ): $(SRCWIN)wapp.c $(HDRS)
	$(COMP) $(FOO)wapp$(OBJ) $(CO) $(SRCWIN)wapp.c

$(OD)wdll$(OBJ): $(SRCWIN)wdll.c $(HDRS)
	$(COMP) $(FOO)wdll$(OBJ) $(CO) $(SRCWIN)wdll.c

$(OD)wdoc$(OBJ): $(SRCWIN)wdoc.c $(HDRS)
	$(COMP) $(FOO)wdoc$(OBJ) $(CO) $(SRCWIN)wdoc.c

$(OD)wfile$(OBJ): $(SRCWIN)wfile.c $(cfile_h)
	$(COMP) $(FOO)wfile$(OBJ) $(CO) $(SRCWIN)wfile.c

$(OD)wgsimg$(OBJ): $(SRCWIN)wgsimg.c $(HDRS)
	$(COMP) $(FOO)wgsimg$(OBJ) $(CO) $(SRCWIN)wgsimg.c

$(OD)wgssrv$(OBJ): $(SRCWIN)wgssrv.c $(HDRS)
	$(COMP) $(FOO)wgssrv$(OBJ) $(CO) $(SRCWIN)wgssrv.c

$(OD)wgsver$(OBJ): $(SRCWIN)wgsver.c $(HDRS)
	$(COMP) $(FOO)wgsver$(OBJ) $(CO) $(SRCWIN)wgsver.c

$(OD)wimg$(OBJ): $(SRCWIN)wimg.c $(HDRS)
	$(COMP) $(FOO)wimg$(OBJ) $(CO) $(SRCWIN)wimg.c

$(OD)wview$(OBJ): $(SRCWIN)wview.c $(HDRS)
	$(COMP) $(FOO)wview$(OBJ) $(CO) $(SRCWIN)wview.c

$(OD)wviewwin$(OBJ): $(SRCWIN)wviewwin.c $(HDRS)
	$(COMP) $(FOO)wviewwin$(OBJ) $(CO) $(SRCWIN)wviewwin.c

$(OD)wtest1$(OBJ): $(SRCWIN)wtest1.c $(SRCWIN)wtest1.h $(SRCWIN)wtest2.h $(HDRS)
	$(COMP) $(FOO)wtest1$(OBJ) $(CO) $(SRCWIN)wtest1.c

$(OD)wtest2$(OBJ): $(SRCWIN)wtest2.c $(SRCWIN)wtest1.h $(SRCWIN)wtest2.h $(HDRS)
	$(COMP) $(FOO)wtest2$(OBJ) $(CO) $(SRCWIN)wtest2.c

$(OD)wtest3$(OBJ): $(SRCWIN)wtest3.c $(SRCWIN)wtest1.h $(SRCWIN)wtest2.h $(HDRS)
	$(COMP) $(FOO)wtest3$(OBJ) $(CO) $(SRCWIN)wtest3.c


wtest1: $(BD)wtest1$(EXE)

wtest3: $(BD)wtest3$(EXE)

epstool: $(BD)epstool$(EXE)

epstool_make:
	nmake -f $(SRCWIN)windows.mak VCVER=$(VCVER) VCDRIVE=$(VCDRIVE) LIBPNGINC= LIBPNGCFLAGS= LIBPNGLIBS= $(BD)epstool$(EXE)
 
epstest: epstool_make
	nmake -f $(SRCWIN)windows.mak VCVER=$(VCVER) VCDRIVE=$(VCDRIVE) LIBPNGINC= LIBPNGCFLAGS= LIBPNGLIBS= $(BD)epstest$(EXE)
	$(BD)epstest$(EXE)	
 
clean:
	-del $(OBJS)
	-del $(EPSOBJS)
	-del $(VIEWAPIOBJ)
	-del $(BD)main.exe
	-del $(BD)main.ilk
	-del $(BD)main.pdb
	-del $(BD)epstool.exe
	-del $(BD)epstool.ilk
	-del $(BD)epstool.pdb
	-del $(BD)gsview_test1.dll
	-del $(BD)gsview_test1.exp
	-del $(BD)gsview_test1.lib
	-del $(BD)gsview_test1.ilk
	-del $(BD)gsview_test1.pdb
	-del $(BD)wviewapi_example.exe
	-del $(BD)wviewapi_example.ilk
	-del $(BD)wviewapi_example.pdb
	-del $(OD)wtest1.obj
	-del $(OD)wtest1.res
	-del $(BD)wtest1.exe
	-del $(BD)wtest1.ilk
	-del $(BD)wtest1.pdb
	-del $(OD)wtest3.obj
	-del $(OD)wtest3.res
	-del $(BD)wtest3.exe
	-del $(BD)wtest3.ilk
	-del $(BD)wtest3.pdb
	-del vc70.pdb vc60.pdb vc50.pdb
	-del $(OD)lib.rsp
	-del $(OD)link.rsp


# simple GS interface
SIMPLEOBJ=$(OD)wdll$(OBJ) $(OD)wgsver$(OBJ) \
 $(OD)cgsdll$(OBJ) $(OD)cgsdll2$(OBJ)


$(OD)cgsdll2$(OBJ): $(SRC)cgsdll2.c $(HDRS)
	$(COMP) $(FOO)cgsdll2$(OBJ) $(CO) $(SRC)cgsdll2.c

$(BD)gssimple.dll: $(SIMPLEOBJ) $(OD)lib.rsp
	echo $(SIMPLEOBJ) > $(OD)link.rsp
	$(LINK) $(DEBUGLINK) /DLL /OUT:$(BD)gssimple.dll @$(OD)link.rsp @$(OD)lib.rsp

$(BD)gssimple.exe: $(SIMPLEOBJ) $(OD)lib.rsp
	echo $(SIMPLEOBJ) > $(OD)link.rsp
	$(LINK) $(DEBUGLINK) /SUBSYSTEM:CONSOLE /OUT:$(BD)gssimple.exe @$(OD)link.rsp @$(OD)lib.rsp
