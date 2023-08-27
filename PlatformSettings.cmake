# Configure the platform specific settings
#
# Setup high-level build options ...
if(UNIX)
    include(FindPkgConfig)
    set(XTRKCAD_USE_GTK_DEFAULT ON)

    # Configure help display and i18n
    if(APPLE)
        set(CMAKE_MACOSX_RPATH 0)
	    set(XTRKCAD_USE_GETTEXT_DEFAULT OFF)
	    set(XTRKCAD_USE_APPLEHELP_DEFAULT ON)
		set(CMAKE_FIND_APPBUNDLE LAST)
	    pkg_check_modules(GTK_WEBKIT "webkit-1.0" QUIET)
        if(GTK_WEBKIT_FOUND)
            set(XTRKCAD_USE_BROWSER_DEFAULT OFF)
        else()
            set(XTRKCAD_USE_BROWSER_DEFAULT ON)
        endif()
    else()
        set(XTRKCAD_USE_GETTEXT_DEFAULT ON)
        set(XTRKCAD_USE_BROWSER_DEFAULT ON)
   endif()
endif()

# Set Win64 flag when a 64 bit build is selected
if(WIN32)
	set(XTRKCAD_USE_GETTEXT_DEFAULT ON)
	set(XTRKCAD_USE_GTK_DEFAULT OFF)

	# determine processor target architecture
	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(Win64Bit ON CACHE BOOL "Target Architecture: x64")
	else ()
		set(Win64Bit OFF CACHE BOOL "Target Architecture: x86")
	endif ()

	mark_as_advanced(Win64Bit)

	if (Win64Bit)
        set(XTRKCAD_ARCH_SUBDIR "x64")
		if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  			set(CMAKE_INSTALL_PREFIX "C:/Program Files/XTrkCAD" CACHE PATH "WIN64 Install" FORCE)
		endif()
	else ()
        set( XTRKCAD_ARCH_SUBDIR "x86")
	endif ()

	set(CMAKE_C_FLAGS_DEBUG "/D_DEBUG /MDd /Zi /Ob0 /Od /RTC1" CACHE STRING "Flags used by the compiler during debug builds" FORCE)
	set(CMAKE_C_FLAGS_MINSIZEREL "/MD /O1 /Ob1 /D NDEBUG" CACHE STRING "Flags used by the compiler during release minumum size builds" FORCE)
	set(CMAKE_C_FLAGS_RELEASE "/MD /O2 /Ob2 /D NDEBUG" CACHE STRING "Flags used by the compiler during release builds" FORCE)
	set(CMAKE_C_FLAGS_RELWITHDEBINFO "/MDd /Zi /O2 /Ob1 /D NDEBUG" CACHE STRING "Flags used by the compiler during release with debug info builds" FORCE)

	add_definitions(-DWINDOWS)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()