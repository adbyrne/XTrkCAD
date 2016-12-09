#!/bin/sh -x

# Script to create Mac DMGs.
# Created 12/1/2016
# Author Adam Richards

version = "Version"
help = false

while getopts v:i:h option
do
	case "${option}"
	in
			v) version=${optarg};;
			i) buildlib=${optarg};;
			h) help = true;;
	esac
done

if (help == true) then
	echo "xtrkcad-bundler -i Cmake_Install_Lib [-v Version]"
	echo " "
	echo " -v Version - will be appended to 'xtrkcad-' in the output image name"
	echo " -i Cmake_Install_Lib where the binary and share files were placed by Make"
	exit -128
fi

if (!buildlib) then
	echo " Cmake_Install_Lib [-i] parameter missing"
	exit -128
fi

#copy in all shares
echo "copying shares from build to share directory"
cp -a "${buildlib}/xtrkcad/share share"

#copy in binary
echo "copying binary from build to bin directory"
cp "${build_lib}/app/bin/xtrkcad bin"

echo "executing gtk-mac-bundler"
EXEC "gtk-mac-bundler xtrkcad.bundle"

#Build dmg using template
echo "making a copy of the tenplate"
unzip "xtrkcad-template.dmg.zip xtrkcad-work.dmg"

echo "mounting template copy"
mkdir -p dmg
hdiutil "attach xtrkcad-work -quiet -noautoopen -mountpoint dmg"

#copy in bundle
echo "copying in bundle"
rm -rf "dmg/xtrkcad.app"
cp "bin/xtrkcad.app dmg"

#convert dmg to RO
echo "closing image"
dev_dmg = 'hdiutil info | grep "dmg" | grep "Apple_HFS"' && \ 
hdiutil detach "${dev_dmg}" -quiet -force
master = "xtrkcad-${version}"
rm -f "${master}"
echo "making image RO and compressed"
hdiutil convert "${template} -quiet -format UDZO -imagekey zlib-level=9 -o ${master}"

rm -rf dmg

exit 0