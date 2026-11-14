# Configure the platform specific settings
#
# Setup high-level build options ...

if(XTRKCAD_USE_GTK3)
	# path to GTK resouces
	set(XTRKCAD_RESOURCE_ROOT "/org/xtrackcad")
	set(XTRKCAD_BUILDER_ROOT "${XTRKCAD_RESOURCE_ROOT}/wlib")
	set(XTRKCAD_SYMBOLS_ROOT "${XTRKCAD_RESOURCE_ROOT}/symbols")
	set(XTRKCAD_ICONS_ROOT "${XTRKCAD_RESOURCE_ROOT}/icons")
	message(STATUS "Plattform settings for GTK3")
endif()

# Pointers passed or assigned through incompatible types are an error, as
# GCC 14 and later make them by default; Clang only warns unless told. The
# 333 sites this used to suppress were fixed in Development Issues #52-#54.
# The rules that keep it at zero are in the dev guide (advanced.md, "The
# warning-count ratchet").
set(XTRKCAD_POINTER_TYPES_FLAG -Werror=incompatible-pointer-types)

# Ratchet: Phases 4-6 of the GTK3V2MAIN quality plan brought these
# categories to zero for owned code. They are errors so a future PR can't
# silently reintroduce them. The few remaining instances are all in vendored
# third-party targets (wrapbox), which turn this same list back into
# warnings with XTRKCAD_RATCHET_EXEMPT_OPTIONS.
#
# implicit-fallthrough was initially GCC-only (Phase 10) because Clang
# doesn't recognize this codebase's comment-style fallthrough annotation.
# Phase 12 (SF #663) fixed the ~73 genuine fallthrough sites and
# standardized on __attribute__((fallthrough)) (recognized by both
# compilers), so it ratchets on both now.
set(XTRKCAD_RATCHET_WARNINGS
	sign-compare
	type-limits
	absolute-value
	unused-but-set-parameter
	implicit-fallthrough
)
# cast-function-type stays GCC-only. Under Clang, this same flag name also
# promotes a separate, stricter sub-check (-Wcast-function-type-strict) that
# flags ~106 GTK/GLib generic-callback-cast idiom instances (GCallback,
# GCompareFunc, G_DEFINE_TYPE-generated code) as well as vendored code --
# verified directly (Clang 22, ccache disabled to rule out stale results)
# that even the "plain" flag name pulls -strict in under Clang, so it can't
# be ratcheted there without also flagging that permanently-accepted idiom.
# Phase 12 (SF #662) fixed the ~13 genuine project-typedef cast mismatches
# that WERE real bugs among the findings; the rest -- see SF #662 for the
# full accounting -- are a permanent, un-ratcheted Clang-only false-positive
# class, not tracked as debt.
set(XTRKCAD_RATCHET_WARNINGS_GNU cast-function-type)

list(TRANSFORM XTRKCAD_RATCHET_WARNINGS PREPEND -Werror=
	OUTPUT_VARIABLE _xtrkcad_ratchet)
list(TRANSFORM XTRKCAD_RATCHET_WARNINGS_GNU PREPEND -Werror=
	OUTPUT_VARIABLE _xtrkcad_ratchet_gnu)
# Vendored targets: the same categories, warnings again (all of them on
# every compiler, as architecture-specific findings can surface on
# platforms not tested here).
list(TRANSFORM XTRKCAD_RATCHET_WARNINGS PREPEND -Wno-error=
	OUTPUT_VARIABLE XTRKCAD_RATCHET_EXEMPT_OPTIONS)
list(TRANSFORM XTRKCAD_RATCHET_WARNINGS_GNU PREPEND -Wno-error=
	OUTPUT_VARIABLE _xtrkcad_exempt_gnu)
list(APPEND XTRKCAD_RATCHET_EXEMPT_OPTIONS ${_xtrkcad_exempt_gnu})

# Warning options for GCC and Clang (UNIX, and MinGW on Windows). -Wall is
# added separately: always on UNIX, Debug only on MinGW.
set(XTRKCAD_GNU_WARNING_OPTIONS
	${XTRKCAD_POINTER_TYPES_FLAG}
	# glib 2.0 deprecated GTypeDebugFlags and GTimeVal
	-Wno-deprecated-declarations
	# paramData_t/descData_t positional aggregate literals (dialog control
	# tables throughout app/bin) deliberately set only their leading fields
	# and rely on C's implicit zero-init for the rest -- a longstanding
	# codebase idiom, not latent bugs. Suppress rather than force a large,
	# regression-risk rewrite.
	-Wno-missing-field-initializers
	${_xtrkcad_ratchet}
	"$<$<C_COMPILER_ID:GNU>:${_xtrkcad_ratchet_gnu}>"
)

# Platform and compiler conditions for generator expressions, set once:
# "$<${XTRKCAD_IF_WINDOWS}:file.c>". WINDOWS is CMake's WIN32 (MinGW and
# MSVC), UNIX everything else (macOS too), MACOS is APPLE.
set(XTRKCAD_IF_WINDOWS "$<PLATFORM_ID:Windows>")
set(XTRKCAD_IF_UNIX "$<NOT:$<PLATFORM_ID:Windows>>")
set(XTRKCAD_IF_MACOS "$<PLATFORM_ID:Darwin>")
set(XTRKCAD_IF_MSVC "$<C_COMPILER_ID:MSVC>")

if(UNIX)
    include(FindPkgConfig)
    SET (CMAKE_ENABLE_EXPORTS TRUE)
    set(XTRKCAD_USE_GTK_DEFAULT ON)
	
    # Configure help display and i18n
    if(APPLE)
        set(CMAKE_MACOSX_RPATH 0)
	    set(XTRKCAD_USE_GETTEXT_DEFAULT OFF)
	    # Help opens in the default browser, as in the packages. Apple
	    # Help (osxhelp.c) needs an installed .app bundle with a registered
	    # Help Book, so a plain build had no working Help (Dev Issues #64).
	    set(XTRKCAD_USE_APPLEHELP_DEFAULT OFF)
		set(CMAKE_FIND_APPBUNDLE LAST)
	    set(XTRKCAD_USE_BROWSER_DEFAULT ON)
    else()
        set(XTRKCAD_USE_GETTEXT_DEFAULT ON)
        set(XTRKCAD_USE_BROWSER_DEFAULT ON)
   endif()
endif()

# Set Win64 flag when a 64 bit build is selected
if(WIN32)
	set(XTRKCAD_USE_GETTEXT_DEFAULT ON)
	set(XTRKCAD_USE_GTK_DEFAULT OFF)
	set(XTRKCAD_GTK3_EXPERIMENT OFF)
	set(XTRKCAD_USE_BROWSER ON)

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

	if(MSVC)
		# install(TARGETS ... RUNTIME_DEPENDENCIES) in app/bin
		if(CMAKE_VERSION VERSION_LESS 3.21)
			message(FATAL_ERROR "The MSVC build needs CMake 3.21 or newer")
		endif()
	endif()
endif()

# Compile and link options for every platform and compiler.
set(_xtrkcad_gnu "$<NOT:${XTRKCAD_IF_MSVC}>")
set(_xtrkcad_unix_not_macos "$<NOT:$<PLATFORM_ID:Windows,Darwin>>")
set(_xtrkcad_msvc_c "$<AND:${XTRKCAD_IF_MSVC},$<COMPILE_LANGUAGE:C>>")
add_compile_options(
	"$<${_xtrkcad_unix_not_macos}:-pthread>"
	# GCC and Clang: -Wall always on UNIX, Debug only on MinGW
	"$<$<AND:${_xtrkcad_gnu},$<OR:${XTRKCAD_IF_UNIX},$<CONFIG:DEBUG>>>:-Wall>"
	"$<${_xtrkcad_gnu}:${XTRKCAD_GNU_WARNING_OPTIONS}>"
	"$<$<AND:${XTRKCAD_IF_MSVC},$<CONFIG:DEBUG>>:/W3>"
	# MSVC: sources are UTF-8; the compat header stands in for GCC
	# attributes and POSIX names (C only: rc.exe doesn't know these
	# switches). C4828 (not valid UTF-8) comes from Latin-1 characters in a
	# comment of the vendored app/wlib/include/getline.h, which wlib.h
	# includes everywhere.
	"$<${_xtrkcad_msvc_c}:/utf-8>"
	"$<${_xtrkcad_msvc_c}:/FI${CMAKE_SOURCE_DIR}/app/bin/include/win32/msvccompat.h>"
	"$<${_xtrkcad_msvc_c}:/wd4828>"
)
add_link_options("$<${_xtrkcad_unix_not_macos}:-pthread>")
add_compile_definitions(
	"$<$<AND:${XTRKCAD_IF_WINDOWS},$<CONFIG:DEBUG>>:_DEBUG>"
	"$<${XTRKCAD_IF_WINDOWS}:WINDOWS;_CRT_SECURE_NO_WARNINGS>"
)
