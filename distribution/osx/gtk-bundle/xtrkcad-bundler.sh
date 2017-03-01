#!/bin/sh -x

echo "Starting Xtrkcad-bundler" "@0"

# Script to create Mac DMGs.
# Created 12/1/2016
# Author Adam Richards

version="Version"

while getopts "v:i:h" option; do
	case "${option}" in
			v) version="${OPTARG}";;
			i) installib="${OPTARG}";;
			h) usage;;
			*) usage;;
	esac
done
shift "$((OPTIND-1))"

usage() {
	echo "xtrkcad-bundler -i Cmake_Install_Lib [-v Version]";
	echo " -v Version - will be appended to 'xtrkcad' in the image name";
	echo " -i Cmake_Install_Lib where the binary and share files were placed by Make";
	exit 1;
	
	}

if [ -z "${installib}" ]; then
	usage;
	exit 1;
fi

#copy in all shares
echo "copying shares from build to share directory"
cp -R "${installib}/share/" "share/"

#copy in binary
echo "copying binaries from build to bin directory"
cp "${installib}/bin/xtrkcad" "bin"
cp "${installib}/bin/helphelper" "bin"

echo "executing gtk-mac-bundler"
gtk-mac-bundler xtrkcad.bundle

#Build dmg using template
echo "making a copy of the tenplate"
rm -f xtrkcad-template.dmg

unzip -o xtrkcad-template.dmg.zip 

echo "mounting template copy"
rm -rf dmg
mkdir -p dmg

hdiutil attach xtrkcad-template.dmg -quiet -noautoopen -mountpoint dmg

#copy in bundle
echo "copying in bundle"
rm -rf "dmg/xtrkcad.app"
cp -R "bin/xtrkcad.app" "dmg/xtrkcad.app"

#convert dmg to RO
echo "closing image"
#dev_dmg='hdiutil info | grep "dmg" | grep "Apple_HFS"' && \ 
hdiutil detach dmg -force
master="xtrkcad-OSX-${version}"
rm -rf "${master}".dmg
echo "making image RO and compressed"
hdiutil convert xtrkcad-template.dmg -format UDZO -imagekey zlib-level=9 -o "${master}"

# compress the output

rm -f "${master}".dmg.tar.gz

tar -cvzf "${master}".dmg.tar.gz "${master}".dmg

mv -f "${master}".dmg.tar.gz "${installib}"/bin

rm -rf dmg

echo "Tarball output image file ${master}.tar.gz is in ${installib}/bin directory"

exit 0