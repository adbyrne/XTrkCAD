# Setup global packaging parameters ...
SET(CPACK_PACKAGE_DESCRIPTION "XTrackCad is a CAD program for designing model railroad layouts.")
SET(CPACK_PACKAGE_EXECUTABLES "xtrkcad;XTrkCAD ${XTRKCAD_FILE_VERSION}")
IF(WIN32)
	# Add suffix
	SET(CPACK_PACKAGE_FILE_NAME "xtrkcad-setup-${XTRKCAD_FILE_VERSION}-${XTRKCAD_ARCH_SUBDIR}")
ELSE(WIN32)
	SET(CPACK_PACKAGE_FILE_NAME "xtrkcad-setup-${XTRKCAD_FILE_VERSION}")
ENDIF(WIN32)
SET(CPACK_PACKAGE_INSTALL_DIRECTORY "XTrkCAD ${XTRKCAD_FILE_VERSION}")
SET(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "XTrkCAD ${XTRKCAD_FILE_VERSION}")
SET(CPACK_PACKAGE_NAME "xtrkcad")
SET(CPACK_PACKAGE_VENDOR "XTrackCAD Project")
SET(CPACK_PACKAGE_VERSION ${XTRKCAD_FILE_VERSION})
SET(CPACK_PACKAGE_VERSION_MAJOR ${XTRKCAD_MAJOR_VERSION})
SET(CPACK_PACKAGE_VERSION_MINOR ${XTRKCAD_MINOR_VERSION})
SET(CPACK_PACKAGE_VERSION_PATCH ${XTRKCAD_RELEASE_VERSION})
SET(CPACK_RESOURCE_FILE_LICENSE "${XTrkCAD_SOURCE_DIR}/app/COPYING")
SET(CPACK_SOURCE_GENERATOR "ZIP;TGZ")
SET(CPACK_SOURCE_PACKAGE_FILE_NAME "xtrkcad-source-${XTRKCAD_FILE_VERSION}")

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/downloadpage.wiki.in ${CMAKE_BINARY_DIR}/downloadpage.wiki)

# Second support link shown in the packaged README-INSTALL.txt files (win32/osx), alongside the
# fixed https://xtrkcad.org line -- defaults to the canonical SourceForge project page rather
# than any one contributor's personal git mirror, since that's this project's real, permanent
# home regardless of who built a given package. A packaging job for a specific distribution
# channel (e.g. one that wants to point users back at the exact repo/release a build came from)
# can override this at configure time instead.
if(NOT DEFINED XTRKCAD_PROJECT_URL)
    set(XTRKCAD_PROJECT_URL "https://sourceforge.net/p/xtrkcad-fork/" CACHE STRING
        "Secondary support URL shown in packaged README-INSTALL.txt files")
endif()

# Setup platform-specific packaging ...
IF(APPLE)
	ADD_SUBDIRECTORY(osx)
ENDIF(APPLE)

IF(UNIX AND NOT APPLE)
	ADD_SUBDIRECTORY(posix)
	if(Flatpak_FOUND AND Hg_FOUND AND XTRKCAD_BUILD_FLATPAK)
	    ADD_SUBDIRECTORY(flatpak)
	endif()
ENDIF(UNIX AND NOT APPLE)

IF(WIN32)
	ADD_SUBDIRECTORY(win32)
ENDIF(WIN32)
