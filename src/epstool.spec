#
# Spec file for epstool 3.08
#
# 3.08 release
#  2005-06-06 Russell Lang <gsview@ghostgum.com.au>
#
Name: epstool
Version: 3.08
Release: 1
Epoch: 0
Summary: Create or extract preview images in EPS files
License: GPL
Group: Applications/Graphics
Source: ftp://mirror.cs.wisc.edu/pub/mirrors/ghost/ghostgum/epstool-3.08.tar.gz
URL: http://www.cs.wisc.edu/~ghost/gsview/
Vendor: Ghostgum Software Pty Ltd
Packager: Russell Lang <gsview@ghostgum.com.au>
Requires: ghostscript
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)


%description
Epstool is a utility to create or extract preview images in EPS files. 
Add Interchange (EPSI), DOS EPS or PICT preview. 
Extract PostScript or preview from DOS EPS or Macintosh EPSF files. 
Create preview using ghostscript. 
Automatically calculate bounding box using ghostscript and update in EPS file. 
Handle DCS 2.0 (Desktop Color Separations). 

%prep
rm -rf $RPM_BUILD_DIR/epstool-%{version}
mkdir $RPM_BUILD_DIR/epstool-%{version}
%setup -n epstool-%{version}

%build
cd $RPM_BUILD_DIR/epstool-%{version}
make RPM_OPT_FLAGS="$RPM_OPT_FLAGS" \
        EPSTOOL_BASE=%{_prefix}          \
        EPSTOOL_BINDIR=%{_bindir}        \
        EPSTOOL_MANDIR=%{_mandir}        \
        EPSTOOL_DOCDIR=%{_docdir}

%install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT%{_bindir}
install -d $RPM_BUILD_ROOT%{_mandir}
install -d $RPM_BUILD_ROOT%{_docdir}
cd $RPM_BUILD_DIR/epstool-%{version}
make install \
        EPSTOOL_BASE=$RPM_BUILD_ROOT%{_prefix}           \
        EPSTOOL_BINDIR=$RPM_BUILD_ROOT%{_bindir}         \
        EPSTOOL_MANDIR=$RPM_BUILD_ROOT%{_mandir}         \
        EPSTOOL_DOCDIR=$RPM_BUILD_ROOT%{_docdir}

%clean
rm -rf $RPM_BUILD_DIR/%{name}-%{version}
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root)
%doc %{_docdir}/*
%{_bindir}/*
%{_mandir}/man*/*

%changelog
* Fri Jun 10 2005 Russell Lang <gsview@ghostgum.com.au>
- epstool 3.08 release
* Fri Jan 07 2005 Russell Lang <gsview@ghostgum.com.au>
- epstool 3.07 release
* Thu Nov 28 2004 Russell Lang <gsview@ghostgum.com.au>
- epstool 3.06 release
* Thu Apr 22 2004 Russell Lang <gsview@ghostgum.com.au>
- epstool 3.05 release
  Initial spec file
  Unfortunately RPM requires that dates be written backwards.
  Release date should be "Thu 22 Apr 2004" or preferably
  2004-04-22.
