Name: xtrkcad
Summary: CAD for Model Railroad layout
Version: 5.2.2
Release: 1%{?dist}
License: GPLv2+
URL: https://sourceforge.net/projects/xtrkcad-fork
Source0: https://sourceforge.net/projects/xtrkcad-fork/files/XTrackCad/Version%20%{version}/xtrkcad-source-%{version}GA.tar.gz
# patches removed on next GA release
Patch0: xtrkcad-5.2.2GA-p0.patch
Patch1: xtrkcad-5.2.2GA-p1.patch
BuildRoot: %{_tmppath}/%{name}-root
BuildRequires: gcc, gcc-c++, cmake >= 2.4.7, pkgconfig, gtk2-devel
BuildRequires: libzip, libzip-devel, pandoc, desktop-file-utils
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
%setup -n xtrkcad-source-%{version}GA -q

# removed on next GA release
%patch0 -p1
%patch1 -p1


%build
cmake -D CMAKE_INSTALL_PREFIX:PATH=%{_prefix} -D CMAKE_BUILD_TYPE=Debug  .
%make_build

%install
rm -rf $RPM_BUILD_ROOT/*
make DESTDIR=$RPM_BUILD_ROOT install
desktop-file-install --dir=%{buildroot}/%{_datadir}/applications \
%{buildroot}/%{_datadir}/%{name}/applications/xtrkcad.desktop
rm %{buildroot}/%{_datadir}/%{name}/applications/xtrkcad.desktop
rm %{buildroot}/%{_datadir}/%{name}/xtrkcad.desktop
mkdir -p %{buildroot}/%{_datadir}/icons
mv %{buildroot}/%{_datadir}/%{name}/pixmaps/xtrkcad.png %{buildroot}/%{_datadir}/icons
# Following gets removed on next GA release
mkdir -p %{buildroot}/%{_datadir}/licenses/%{name}
mv %{buildroot}/%{_datadir}/%{name}/COPYING %{buildroot}/%{_datadir}/licenses/%{name}/COPYING

%check
make test

%files
%license app/COPYING
%{_bindir}/xtrkcad
%{_datadir}/applications/xtrkcad.desktop
%{_datadir}/icons/xtrkcad.png
%{_datadir}/xtrkcad

%changelog
* Thu Jan 13 2022 Phil Cameron <pecameron1 -at- gmail.com> 5.2.2-1
- V5.2.2 GA

