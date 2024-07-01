Name: xtrkcad
Summary: CAD for Model Railroad layout
Version: 5.3.0
Release: 1%{?dist}
License: GPLv2
URL: https://sourceforge.net/projects/xtrkcad-fork
Source0: https://sourceforge.net/projects/xtrkcad-fork/files/XTrackCad/Version%20%{version}/xtrkcad-source-%{version}GA.tar.gz
# fix build to use dynamic libzip
Patch0: xtrkcad-5.3.0GA.libzip.patch

BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: cmake >= 2.4.7
BuildRequires: pkgconfig
BuildRequires: gtk2-devel
BuildRequires: libzip-devel
BuildRequires: desktop-file-utils
BuildRequires: gettext-devel
BuildRequires: glibc-devel
BuildRequires: pandoc
BuildRequires: doxygen
BuildRequires: cjson-devel
BuildRequires: mxml
BuildRequires: mxml-devel
BuildRequires: freeimage
BuildRequires: freeimage-devel
BuildRequires: inkscape
BuildRequires: inkscape-libs

Requires: xdg-utils

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
car inventory. Documents/help is in xtrkcad-doc rpm.

%prep
%setup -n xtrkcad-source-%{version}GA -q
%patch 0 -p1

%package doc
Summary: Documentation for %{name}
BuildArch: noarch

%description doc
This package contains user documentation and help for XTrkCAD,
in HTML format. It also contains demos, and examples.

%build
%cmake -DBUILD_SHARED_LIBS=OFF -DXTRKCAD_USE_DOXYGEN=ON
%cmake_build

%install
%cmake_install

desktop-file-install --dir=%{buildroot}/%{_datadir}/applications \
	%{buildroot}/%{_datadir}/%{name}/applications/xtrkcad.desktop
rm %{buildroot}/%{_datadir}/%{name}/applications/xtrkcad.desktop

mkdir -p %{buildroot}/%{_datadir}/pixmaps
mv %{buildroot}/%{_datadir}/%{name}/pixmaps/xtrkcad.png \
	%{buildroot}/%{_datadir}/pixmaps/xtrkcad.png
rm -rf %{buildroot}/%{_datadir}/%{name}/pixmaps

mkdir -p %{buildroot}/%{_datadir}/mime/packages
mv %{buildroot}/%{_datadir}/%{name}/applications/xtrkcad.xml \
	%{buildroot}/%{_datadir}/mime/packages/xtrkcad.xml

# Tests require a feature in the next release
#%check
#%ctest

%files
%license app/COPYING
%{_bindir}/%{name}
%{_datadir}/applications/xtrkcad.desktop
%{_datadir}/pixmaps/xtrkcad.png
%{_datadir}/mime/packages/xtrkcad.xml
%{_datadir}/%{name}
%exclude %{_datadir}/%{name}/demos
%exclude %{_datadir}/%{name}/examples
%exclude %{_datadir}/%{name}/html

%files doc
%{_datadir}/%{name}/demos
%{_datadir}/%{name}/examples
%{_datadir}/%{name}/html
%{_datadir}/locale/cy_GB/LC_MESSAGES/%{name}.mo
%{_datadir}/locale/de_DE/LC_MESSAGES/%{name}.mo
%{_datadir}/locale/fi/LC_MESSAGES/%{name}.mo
%{_datadir}/locale/fr_FR/LC_MESSAGES/%{name}.mo
%{_datadir}/locale/pt_BR/LC_MESSAGES/%{name}.mo
%{_datadir}/locale/ru/LC_MESSAGES/%{name}.mo

%changelog
* Fri Apr 26 2024 Phil Cameron <pecameron1 -at- gmail.com> 5.3.0-1
- V5.3.0 GA

* Sat Jan 27 2024 Fedora Release Engineering <releng@fedoraproject.org> - 5.2.2-5
- Rebuilt for https://fedoraproject.org/wiki/Fedora_40_Mass_Rebuild

* Sat Jul 22 2023 Fedora Release Engineering <releng@fedoraproject.org> - 5.2.2-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_39_Mass_Rebuild

* Sat Jan 21 2023 Fedora Release Engineering <releng@fedoraproject.org> - 5.2.2-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_38_Mass_Rebuild

* Sat Jul 23 2022 Fedora Release Engineering <releng@fedoraproject.org> - 5.2.2-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_37_Mass_Rebuild

* Mon Feb 21 2022 Phil Cameron <pecameron1 -at- gmail.com> 5.2.2-1
- V5.2.2 GA

