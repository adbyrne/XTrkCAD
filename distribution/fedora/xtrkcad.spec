Summary: XTrkCad CAD for Model Railroad layout
Name: xtrkcad
Version: 5.2.2
Release: 1%{?dist}
License: GPLv2+
URL: https://sourceforge.net/projects/xtrkcad-fork
Source0: https://sourceforge.net/projects/xtrkcad-fork/files/XTrackCad/Version%20%{version}%20/xtrkcad-source-%{version}.zip
BuildRoot: %{_tmppath}/%{name}-root
BuildRequires: gcc, gcc-c++, cmake >= 2.4.7, pkgconfig, gtk2-devel
BuildRequires: libcmocka, libcmocka-devel, libzip, libzip-devel
BuildRequires: tinyxml, tinyxml-devel, pandoc
BuildRequires: gettext, gettext-devel, glibc-devel
Requires: libcmocka, libzip, tinyxml

%description
XTrkCad is a CAD program for designing Model Railroad layouts.
XTrkCad supports any scale, has libraries of popular brands of x
turnouts and sectional track (plus you add your own easily), can
automatically use spiral transition curves when joining track
XTrkCad lets you manipulate track much like you would with actual
flex-track to modify, extend and join tracks and turnouts.
Additional features include tunnels, 'post-it' notes, on-screen
ruler, parts list, 99 drawing layers, undo/redo commands,
benchwork, 'Print to BitMap', elevations, train simulation and
car inventory.

%prep
%setup -n xtrkcad-source-%{version}/usr/local -q

%build
cmake -D CMAKE_INSTALL_PREFIX:PATH=%{_prefix} -D CMAKE_BUILD_TYPE=Debug  .
make

%install
rm -rf $RPM_BUILD_ROOT/*
make DESTDIR=$RPM_BUILD_ROOT install

%check
make test

%files
%license app/COPYING
%defattr(-, root, root)
%{_bindir}/xtrkcad
%{_datadir}

%changelog
* Tue Dec 14 2021 Phil Cameron
- V5.2.2 GA

