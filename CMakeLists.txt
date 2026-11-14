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
# Tests link the CMocka::cmocka imported target, which carries the include
# directory, so nothing is added to every target's include path.
find_package(CMocka)
if(CMOCKA_FOUND)
    option(XTRKCAD_TESTING "Build unittests" ON)
endif()

# GUI demo-playback regression tests (RegressionTestAll(), the -T flag; see GTK3
# issue #26). These drive a real GTK3 window, so they need an X display -- default
# to ON only where one can plausibly be provided (Linux, with xvfb-run found or a
# DISPLAY already set), alongside the unit-test infrastructure they piggyback on.
set(_xtrkcad_regression_default OFF)
if(XTRKCAD_TESTING AND CMOCKA_FOUND AND UNIX AND NOT APPLE)
    find_program(XVFB_RUN_EXECUTABLE xvfb-run)
    if(XVFB_RUN_EXECUTABLE OR DEFINED ENV{DISPLAY})
        set(_xtrkcad_regression_default ON)
    endif()
endif()
option(XTRKCAD_REGRESSION_TESTING
    "Build GUI demo-playback regression tests (requires xvfb-run or an existing X DISPLAY)"
    ${_xtrkcad_regression_default})
# Which regression tests run (app/bin/RegressionTests.cmake): "critical",
# the checks of code that changes data, a few minutes; "ci", what PR CI runs
# on every platform; "full", everything. Tests above the level are added but
# disabled, so ctest lists them as not run. CI builds default to full; a
# workflow picks a lower level where it wants one.
if(DEFINED ENV{CI})
    set(_xtrkcad_regression_level_default full)
else()
    set(_xtrkcad_regression_level_default critical)
endif()
set(XTRKCAD_REGRESSION_LEVEL "${_xtrkcad_regression_level_default}" CACHE STRING
    "Regression tests to run: critical, ci or full")
set_property(CACHE XTRKCAD_REGRESSION_LEVEL PROPERTY STRINGS critical ci full)
if(NOT XTRKCAD_REGRESSION_LEVEL MATCHES "^(critical|ci|full)$")
    message(FATAL_ERROR
        "XTRKCAD_REGRESSION_LEVEL is \"${XTRKCAD_REGRESSION_LEVEL}\": use critical, ci or full")
endif()

option(XTRKCAD_SANITIZE "Build everything, the program included, with AddressSanitizer + UBSan (Linux/GCC or Clang only)" OFF)
# For the whole tree, so the regression tests run an instrumented program.
# Until 2026-11 only the unit-test targets were built with these flags, so
# regression-gtk3-sanitizers ran an uninstrumented xtrkcad.
# The sanitizers, set once: the check below, the flags, and the drawcairo
# test when it's built on its own option (test_drawcairo/CMakeLists.txt).
set(XTRKCAD_SANITIZE_FLAGS -fsanitize=address,undefined)
if(XTRKCAD_SANITIZE AND UNIX)
    # Say so now if the runtimes are missing, not with a link error at the end.
    include(CheckCSourceCompiles)
    string(REPLACE ";" " " CMAKE_REQUIRED_FLAGS "${XTRKCAD_SANITIZE_FLAGS}")
    set(CMAKE_REQUIRED_LINK_OPTIONS ${XTRKCAD_SANITIZE_FLAGS})
    check_c_source_compiles("int main(void) { return 0; }" XTRKCAD_SANITIZE_LINKS)
    unset(CMAKE_REQUIRED_FLAGS)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)
    if(NOT XTRKCAD_SANITIZE_LINKS)
        message(FATAL_ERROR "XTRKCAD_SANITIZE: ${CMAKE_C_COMPILER_ID} can't link "
            "${XTRKCAD_SANITIZE_FLAGS}. Install its sanitizer runtimes (GCC: "
            "libasan and libubsan) or use clang (-DCMAKE_C_COMPILER=clang).")
    endif()
    add_compile_options(${XTRKCAD_SANITIZE_FLAGS} -fno-omit-frame-pointer)
    add_link_options(${XTRKCAD_SANITIZE_FLAGS})
endif()
option(XTRKCAD_FUZZ "Build libFuzzer harnesses (Clang only, requires -fsanitize=fuzzer support)" OFF)

# Tools
# Find document conversion tool
find_package(Pandoc)

# check for GTK+ 3.0
# Imported targets carry full library paths, which MSVC's linker needs (it
# has no default search path for the gvsbuild tree). GLOBAL because static
# libraries in subdirectories pass them on to the executable.
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET GLOBAL "gtk+-3.0")
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET GLOBAL "freetype2")
# GTK's native Unix print dialog; Windows prints through GTK's own dialog
if(UNIX)
    pkg_check_modules(GTK_UNIX_PRINT3 REQUIRED IMPORTED_TARGET GLOBAL "gtk+-unix-print-3.0")
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


# Standard install layout: share/, share/locale, ... (CMAKE_INSTALL_DATADIR
# and friends).
include(GNUInstallDirs)
set(XTRKCAD_BIN_INSTALL_DIR "bin")
set(XTRKCAD_LOCALE_INSTALL_DIR "${CMAKE_INSTALL_LOCALEDIR}")

# XTRKCAD_BIN is the single source of truth for this build's identity: the
# installed binary name, package name, desktop file name, and (via the same
# XTRKCAD_FILE_VERSION string reused in app/bin/custom.c) the runtime
# work/prefs directory name. Embedding the version lets multiple
# point releases install side by side and run simultaneously without
# colliding. XTRKCAD_FILE_VERSION (ProgramVersion.cmake) also carries a
# dev-build hash suffix on untagged builds when XTRKCAD_APPEND_SRC_HASH is
# on, so this disambiguates successive dev builds of the same nominal
# version too, not just distinct point releases — see ProgramVersion.cmake
# for why filenames use XTRKCAD_FILE_VERSION rather than XTRKCAD_VERSION.
# XTRKCAD_VERSIONED_INSTALL (also ProgramVersion.cmake) drops the version
# altogether, which tagged release builds do by default, so that a new
# release replaces the previous install (Development Issues #48).
#
# Note: flatpak builds cannot have periods in the name since periods delimit
#       the namespace.
if (XTRKCAD_BUILD_FLATPAK AND XTRKCAD_FLATPAK_NOVERSION)
	set(XTRKCAD_BIN "xtrkcad")
elseif (XTRKCAD_BUILD_FLATPAK)
	string(REPLACE "." "_" XTRKCAD_VERSION_ID "${XTRKCAD_FILE_VERSION}")
	set(XTRKCAD_BIN "xtrkcad${XTRKCAD_VERSION_ID}")
else()
	set(XTRKCAD_BIN "xtrkcad${XTRKCAD_INSTALL_SUFFIX}")
endif()

set(XTRKCAD_SHARE_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/${XTRKCAD_BIN}")

# Flatpak application ID domain, as on Flathub (distribution/flatpak).
# For a local build under another domain: -DXTRKCAD_FLATPAK_DOMAIN=org.xtrkcad
set(XTRKCAD_FLATPAK_DOMAIN "io.sourceforge.xtrkcad_fork" CACHE STRING
	"Flatpak application ID domain")
mark_as_advanced(XTRKCAD_FLATPAK_DOMAIN)
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
    if(WIN32 OR APPLE)
        # find libintl.h and use it (MinGW, gvsbuild's gettext for MSVC, or
        # Homebrew); on Windows the library itself comes with GLib's link
        # flags in PkgConfig::GTK3
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
