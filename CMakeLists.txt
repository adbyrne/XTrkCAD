PROJECT(XTrkCAD)
enable_testing()
cmake_minimum_required(VERSION 3.0)

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked

# additional CMake modules can be found here
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

# Setup high-level build options ...
if(UNIX)
    include(FindPkgConfig)
    set(XTRKCAD_USE_GTK_DEFAULT ON)
    if(APPLE)
	SET(XTRKCAD_USE_GETTEXT_DEFAULT OFF)
    else()
	set(XTRKCAD_USE_GETTEXT_DEFAULT ON)    
	PKG_CHECK_MODULES(GTK_WEBKIT "webkit-1.0")
        if(GTK_WEBKIT_FOUND) 
            set(XTRKCAD_USE_BROWSER_DEFAULT OFF)
        else()
            set(XTRKCAD_USE_BROWSER_DEFAULT ON)    
        endif()
    endif(APPLE)   
endif(UNIX)

IF(WIN32)
	SET(XTRKCAD_USE_GTK_DEFAULT OFF)
	SET(XTRKCAD_USE_GETTEXT_DEFAULT ON)

	SET(CMAKE_C_FLAGS_DEBUG "/D_DEBUG /MTd /Zi  /Ob0 /Od /RTC1" CACHE STRING "Flags used by the compiler during debug builds" FORCE)
	SET(CMAKE_C_FLAGS_MINSIZEREL "/MT /O1 /Ob1 /D NDEBUG" CACHE STRING "Flags used by the compiler during release minumum size builds" FORCE)
	SET(CMAKE_C_FLAGS_RELEASE "/MT /O2 /Ob2 /D NDEBUG" CACHE STRING "Flags used by the compiler during release builds" FORCE)
	SET(CMAKE_C_FLAGS_RELWITHDEBINFO "/MT /Zi /O2 /Ob1 /D NDEBUG" CACHE STRING "Flags used by the compiler during release with debug info builds" FORCE)
	
	ADD_DEFINITIONS(-DWINDOWS)
	ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
    if( POLICY CMP0026 )
	cmake_policy(SET CMP0026 OLD)
    endif( POLICY CMP0026 ) 	
endif(COMMAND cmake_policy)

SET(XTRKCAD_USE_DOXYGEN_DEFAULT OFF)

OPTION(XTRKCAD_USE_GTK "Use GTK for the graphical user interface back-end" ${XTRKCAD_USE_GTK_DEFAULT})
OPTION(XTRKCAD_USE_GETTEXT "Use gettext for internationalization" ${XTRKCAD_USE_GETTEXT_DEFAULT})
OPTION(XTRKCAD_USE_DOXYGEN "Generate internals documentation using doxygen" ${XTRKCAD_USE_DOXYGEN_DEFAULT})
if(UNIX AND NOT APPLE)
    option(XTRKCAD_USE_BROWSER "Show help in default browser" ${XTRKCAD_USE_BROWSER_DEFAULT})
endif()

IF(UNIX)
    PKG_CHECK_MODULES(GTK REQUIRED "gtk+-2.0")
ENDIF()

IF(APPLE)
	OPTION(XTRKCAD_USE_PACKAGEMAKER "Generate an OSX PackageMaker package for distribution." OFF)
ENDIF(APPLE)

# Hide the advanced stuff ...
MARK_AS_ADVANCED(CMAKE_BACKWARDS_COMPATIBILITY)
MARK_AS_ADVANCED(CMAKE_EXECUTABLE_FORMAT)
MARK_AS_ADVANCED(CMAKE_OSX_ARCHITECTURES)
MARK_AS_ADVANCED(CMAKE_OSX_SYSROOT)
MARK_AS_ADVANCED(CMAKE_USE_CHRPATH)
MARK_AS_ADVANCED(EXECUTABLE_OUTPUT_PATH)
MARK_AS_ADVANCED(LIBRARY_OUTPUT_PATH)

# Test for dependencies ...
IF(WIN32 AND NOT XTRKCAD_USE_GTK)
	FIND_PACKAGE(HTMLHelp REQUIRED)
ENDIF(WIN32 AND NOT XTRKCAD_USE_GTK)

IF(XTRKCAD_USE_PACKAGEMAKER)
	IF(NOT CMAKE_INSTALL_PREFIX STREQUAL "/usr")
		MESSAGE(SEND_ERROR "XTRKCAD_USE_PACKAGEMAKER requires CMAKE_INSTALL_PREFIX set to /usr.")
	ENDIF(NOT CMAKE_INSTALL_PREFIX STREQUAL "/usr")
ENDIF(XTRKCAD_USE_PACKAGEMAKER)

# Find unit testing framework
find_package(CMocka)
if(CMOCKA_FOUND)
  include_directories(${CMOCKA_INCLUDE_DIR})
  set(LIBS ${LIBS} ${CMOCKA_LIBRARIES})
  option( XTRKCAD_TESTING "Build unittests" ON)
endif()

# Find document conversion tool
find_package(Pandoc)

# Test for headers and libraries for portability ...

INCLUDE (CheckIncludeFiles)
CHECK_INCLUDE_FILES (malloc.h HAVE_MALLOC_H)

# Setup some global options for installation ...
SET(XTRKCAD_MAJOR_VERSION "4")
SET(XTRKCAD_MINOR_VERSION "4")
SET(XTRKCAD_RELEASE_VERSION "0")
SET(XTRKCAD_VERSION_MODIFIER "beta2")
SET(XTRKCAD_VERSION "${XTRKCAD_MAJOR_VERSION}.${XTRKCAD_MINOR_VERSION}.${XTRKCAD_RELEASE_VERSION}${XTRKCAD_VERSION_MODIFIER}")

IF(WIN32)
	SET( CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX} ${XTRKCAD_VERSION}" )
ENDIF(WIN32)
SET(XTRKCAD_BIN_INSTALL_DIR "bin")
SET(XTRKCAD_LOCALE_INSTALL_DIR "share/locale")
SET(XTRKCAD_PACKAGE "xtrkcad")
SET(XTRKCAD_SHARE_INSTALL_DIR "share/xtrkcad")

# Enable use of a configuration file ...
ADD_DEFINITIONS(-DXTRKCAD_CMAKE_BUILD)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/xtrkcad-config.h.in ${CMAKE_CURRENT_BINARY_DIR}/xtrkcad-config.h)

IF(XTRKCAD_USE_GETTEXT)
	SET(GENHELP_OPTS "-bhi")
	#
	# Find the GnuWin32 installation directory, the gettext include should be located in subdir include
	#
	IF(WIN32)
		if(MSVC)
			# use supplied gettext library for Visual Studio
			message( STATUS "Use simple_gettext module included with XTrackCAD" )
			ADD_DEFINITIONS(-DUSE_SIMPLE_GETTEXT )
		else()	
			# for mingw & co. find libintl.h and use it
			find_path ( INTL_PATH libintl.h )
			if(INTL_PATH)
				message( STATUS "Use installed gettext module" )			
				INCLUDE_DIRECTORIES(${INTL_PATH})
			endif(INTL_PATH)		
		endif()		
	ENDIF(WIN32)
ELSE(XTRKCAD_USE_GETTEXT)
	SET(GENHELP_OPTS "-bh")
ENDIF(XTRKCAD_USE_GETTEXT)

# Setup the rest of the build ...
ADD_SUBDIRECTORY(app)
ADD_SUBDIRECTORY(distribution)
ADD_SUBDIRECTORY(docs)

