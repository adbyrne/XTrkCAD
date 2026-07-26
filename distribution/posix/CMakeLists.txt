# Setup packaging ...
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "XTrackCAD is a CAD program for designing model railroad layouts.")
set(CPACK_GENERATOR "DEB" "RPM" "STGZ")
set(CPACK_SOURCE_GENERATOR "TGZ" "TBZ2" "ZIP")
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_RPM_PACKAGE_LICENSE "GPL")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Engineering")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}////xtrkcad-desc.txt")
set(CPACK_PACKAGE_FILE_NAME "xtrkcad-setup-${XTRKCAD_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")

if(XTRKCAD_VERSION_MODIFIER MATCHES "^Beta")
	set(CPACK_INSTALL_PREFIX "/usr/local" )
else()
	set(CPACK_INSTALL_PREFIX "/usr" )
endif()
set(CPACK_PACKAGING_INSTALL_PREFIX ${CPACK_INSTALL_PREFIX})

# Generate postinst/prerm from templates so they know this build's actual
# install location instead of a hand-maintained guess (previously hardcoded
# to /usr/local/share/xtrkcad-beta unconditionally, which silently broke
# desktop/mime registration on every non-beta build).
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/postinst.in ${CMAKE_CURRENT_BINARY_DIR}/postinst @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/prerm.in ${CMAKE_CURRENT_BINARY_DIR}/prerm @ONLY)
# configure_file() does not carry over the executable bit the static files
# used to have — dpkg refuses to run non-executable maintainer scripts, so
# this has to be set explicitly.
file(CHMOD ${CMAKE_CURRENT_BINARY_DIR}/postinst PERMISSIONS
	OWNER_READ OWNER_WRITE OWNER_EXECUTE
	GROUP_READ GROUP_EXECUTE
	WORLD_READ WORLD_EXECUTE)
file(CHMOD ${CMAKE_CURRENT_BINARY_DIR}/prerm PERMISSIONS
	OWNER_READ OWNER_WRITE OWNER_EXECUTE
	GROUP_READ GROUP_EXECUTE
	WORLD_READ WORLD_EXECUTE)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_BINARY_DIR}/postinst;${CMAKE_CURRENT_BINARY_DIR}/prerm")

# get the specific configuration for each packager
set(CPACK_PROJECT_CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/package.txt)

set(CPACK_PACKAGE_VENDOR "XTrackCAD Fork Project")

# common dependencies for all Linux builds
set(CPACK_RPM_PACKAGE_REQUIRES "glibc gtk3 zlib libzip mxml librsvg2")

set(CPACK_RPM_BUILDREQUIRES
    "gcc, cmake >= 3.20, pkgconfig, gtk3-devel"
    "libzip-devel, libmxml-devel, librsvg2-tools"
    "gettext, gettext-devel, glibc-devel"
)

# NOTE: no webkitgtk packaging dependency here — the built-in WebKitGTK help
# viewer requires "webkit-1.0", which has no GTK3 equivalent. GTK3 builds use
# the system browser for help (XTRKCAD_USE_BROWSER=ON); see
# app/wlib/gtk3lib/CMakeLists.txt, which now fails the cmake configure step
# if XTRKCAD_USE_BROWSER is turned off on this branch.

# exclude these directories from the rpm
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
	"/usr/local"
	"/usr/local/share"
	"/usr/local/bin" 
	"/usr/share/locale"
	"/usr/share/locale/*"
	"/usr/local/share/locale"
	"/usr/local/share/locale/*"
	"/usr/share/applications"
	"/usr/share/pixmaps"
)

set(CPACK_RPM_DEFAULT_DIR_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)


set(CPACK_RPM_CHANGELOG_FILE "${CMAKE_CURRENT_SOURCE_DIR}////changelog.txt")

set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

# Debian package names must be all-lowercase; guard against a future Dev/Beta
# modifier (today's "5.4.0" is already lowercase-safe).
string(TOLOWER "${XTRKCAD_BIN}" CPACK_DEBIAN_PACKAGE_NAME)
set(CPACK_RPM_PACKAGE_NAME "${XTRKCAD_BIN}")

set(CPACK_DEBIAN_PACKAGE_VERSION "1:${XTRKCAD_VERSION}")

# Map CMake processor name to Debian architecture name
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(XTRKCAD_DEB_ARCH "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7|armhf")
    set(XTRKCAD_DEB_ARCH "armhf")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i686|i386")
    set(XTRKCAD_DEB_ARCH "i386")
else()
    set(XTRKCAD_DEB_ARCH "amd64")
endif()
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${XTRKCAD_DEB_ARCH}")

set(CPACK_DEBIAN_PACKAGE_DEPENDS "xdg-utils")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "xtrkcad@sillub.com")

set(CPACK_PACKAGE_CONTACT "xtrkcad@sillub.com")

set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "XTrkCad supports any scale, has libraries of popular brands of \
turnouts and sectional track (plus you add your own easily), can \
automatically use spiral transition curves when joining track \
XTrkCad lets you manipulate track much like you would with actual \
flex-track to modify, extend and join tracks and turnouts. \
Additional features include tunnels, 'post-it' notes, on-screen \
ruler, parts list, 99 drawing layers, undo/redo commands, \
benchwork, 'Print to BitMap', elevations, train simulation and \
car inventory. \
")


set(CPACK_DEBIAN_PACKAGE_SECTION "contrib/graphics")

set(CPACK_DEBIAN_PACKAGE_COMPRESSION "gzip")

set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "xtrkcad.org")

set(CPACK_PACKAGE_EXECUTABLES "${XTRKCAD_BIN};XTrkCAD ${XTRKCAD_VERSION}")

set(CPACK_CREATE_DESKTOP_LINKS "${XTRKCAD_BIN}")

include(CPack)
