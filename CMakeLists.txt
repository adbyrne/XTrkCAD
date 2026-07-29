# Main CMake file for XTrackCAD

cmake_minimum_required(VERSION 3.20...4.0)
project(XTrkCAD C)

cmake_policy(SET CMP0076 NEW)
cmake_policy(SET CMP0079 NEW)

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
# additional CMake modules can be found here
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

# Include CMake defaults
include(CMakeDefaults)

include(CheckIncludeFiles)

include(ProgramVersion.cmake)
include(PlatformSettings.cmake)

enable_testing()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find all dependencies
#
# Additional libraries

find_package(Inkscape)
find_package(Libzip)

# Optionally enable SVG export
find_package(MiniXML)
if(MiniXML_FOUND)
    set(XTRKCAD_CREATE_SVG 1)
    message( STATUS "MiniXML found: SVG export is enabled")
else()
    message( STATUS "MiniXML not found: no SVG export")
endif()

# Find unit testing framework
find_package(CMocka)
if(CMOCKA_FOUND)
    include_directories(${CMOCKA_INCLUDE_DIR})
    set(LIBS ${LIBS} ${CMOCKA_LIBRARIES})
    option(XTRKCAD_TESTING "Build unittests" ON)
endif()

option(XTRKCAD_SANITIZE "Build unit tests with AddressSanitizer + UBSan (Linux/GCC or Clang only)" OFF)
option(XTRKCAD_FUZZ "Build libFuzzer harnesses (Clang only, requires -fsanitize=fuzzer support)" OFF)

# Tools
# Find document conversion tool
find_package(Pandoc)

# check for GTK+ 3.0
if(NOT WIN32)
    pkg_check_modules(GTK3 QUIET "gtk+-3.0")
    if(NOT GTK3_FOUND)
        message(FATAL_ERROR "ERROR: GTK3 not found found!")
    endif()
endif()

# Flatpak
find_package(Flatpak)

# Hg
find_package(Hg)


#
# Configure options
#

option(XTRKCAD_USE_GETTEXT "Use gettext for internationalization" ${XTRKCAD_USE_GETTEXT_DEFAULT})
option(XTRKCAD_USE_DOXYGEN "Generate internals documentation using doxygen" OFF)

if(UNIX AND NOT APPLE)
    option(XTRKCAD_USE_BROWSER "Show help in default browser" ${XTRKCAD_USE_BROWSER_DEFAULT})
endif()

if(APPLE)
    option(XTRKCAD_USE_APPLEHELP "Show help in Apple Help" ${XTRKCAD_USE_APPLEHELP_DEFAULT})
    option(XTRKCAD_USE_BROWSER "Show help in default browser" ${XTRKCAD_USE_BROWSER_DEFAULT})
    option(XTRKCAD_USE_PACKAGEMAKER "Generate an OSX PackageMaker package for distribution." OFF)
endif()

# Hide the advanced stuff ...
mark_as_advanced(CMAKE_BACKWARDS_COMPATIBILITY)
mark_as_advanced(CMAKE_EXECUTABLE_FORMAT)
mark_as_advanced(CMAKE_OSX_ARCHITECTURES)
mark_as_advanced(CMAKE_OSX_SYSROOT)
mark_as_advanced(CMAKE_USE_CHRPATH)
mark_as_advanced(EXECUTABLE_OUTPUT_PATH)
mark_as_advanced(LIBRARY_OUTPUT_PATH)

# Test for headers and libraries for portability ...
# check_include_files (malloc.h HAVE_MALLOC_H)
# if(HAVE_MALLOC_H)
#   add_definitions(-DHAVE_MALLOC_H)
# endif()

#packaging related stuff

if(XTRKCAD_USE_PACKAGEMAKER)
    if(NOT CMAKE_INSTALL_PREFIX STREQUAL "/usr")
        message(SEND_ERROR "XTRKCAD_USE_PACKAGEMAKER requires CMAKE_INSTALL_PREFIX set to /usr.")
    endif()
endif()

#
# configure installation directories

#set( CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" )


set(XTRKCAD_BIN_INSTALL_DIR "bin")
set(XTRKCAD_LOCALE_INSTALL_DIR "share/locale")

# XTRKCAD_BIN is the single source of truth for this build's identity: the
# installed binary name, package name, desktop file name, and (via the same
# XTRKCAD_VERSION string reused in app/bin/custom.c) the runtime work/prefs
# directory name. Embedding the version lets multiple GTK3V2MAIN point
# releases install side by side and run simultaneously without colliding.
# XTRKCAD_VERSION itself (ProgramVersion.cmake) also carries a dev-build
# hash suffix on untagged builds, so this disambiguates successive dev
# builds of the same nominal version too, not just distinct point releases.
set(XTRKCAD_BIN "xtrkcad-${XTRKCAD_VERSION}")
set(XTRKCAD_SHARE_INSTALL_DIR "share/${XTRKCAD_BIN}")

# XTRKCAD_PACKAGE doubles as the gettext domain name (app/i18n/CMakeLists.txt,
# app/bin/i18n.c's bindtextdomain/textdomain calls) and therefore the
# installed .mo filename under the *shared* system locale tree
# (XTRKCAD_LOCALE_INSTALL_DIR, deliberately not version-scoped — gettext
# catalogs belong in the standard /usr/share/locale search path). Reusing
# XTRKCAD_BIN here keeps each version's .mo file distinctly named so two
# versions' translations don't overwrite each other in that shared directory.
set(XTRKCAD_PACKAGE "${XTRKCAD_BIN}")

# Enable use of a configuration file ...
add_definitions(-DXTRKCAD_CMAKE_BUILD)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/xtrkcad-config.h.in ${CMAKE_CURRENT_BINARY_DIR}/xtrkcad-config.h)

# find the config.h include file
include_directories(
 ${CMAKE_CURRENT_BINARY_DIR}
)

#
# I18N related dependencies

if(XTRKCAD_USE_GETTEXT)
    #
    # Find the GnuWin32 installation directory, the gettext include should be located in subdir include
    #
    if(WIN32)
        if(MSVC)
            # use supplied gettext library for Visual Studio
            message( STATUS "Use simple_gettext module included with XTrackCAD" )
            add_definitions(-DUSE_SIMPLE_GETTEXT )
        else()
            # for mingw & co. find libintl.h and use it
            find_path ( INTL_PATH libintl.h )
            if(INTL_PATH)
                message( STATUS "Use installed gettext module" )
                include_directories(${INTL_PATH})
            endif()
        endif()
    endif()
    if(APPLE)
        find_path ( INTL_PATH libintl.h )
        if(INTL_PATH)
            message( STATUS "Use installed gettext module" )
            include_directories(${INTL_PATH})
        endif()
    endif()
endif()

# Setup the rest of the build ...
add_subdirectory(app)
add_subdirectory(distribution)
add_subdirectory(docs)
