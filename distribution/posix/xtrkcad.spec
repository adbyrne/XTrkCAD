Summary: XTrkCad
Name: xtrkcad
Version: 4.3.0
Release: 1%{?dist}
License: GPL
Group: Applications/Engineering
Vendor: XTrkCad Fork Project
Source: xtrkcad-source-%{version}.tar.gz
BuildRoot: /var/tmp/%{name}-root
BuildRequires: gcc, cmake >= 2.4.7, pkgconfig, gtk2-devel, webkitgtk-devel
BuildRequires: gettext, gettext-devel, glibc-devel
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
%setup -n xtrkcad-source-%{version} -q

%build
cmake -D CMAKE_INSTALL_PREFIX:PATH=/usr/ .
make

%install
rm -rf $RPM_BUILD_ROOT/*
make DESTDIR=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root)
%defattr(755, root, root) /usr/local/share/xtrkcad/xdg-open
%{_bindir}/xtrkcad
%{_datadir}

