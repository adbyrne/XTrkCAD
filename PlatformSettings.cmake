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

if(UNIX)
    include(FindPkgConfig)
    SET (CMAKE_ENABLE_EXPORTS TRUE)
    set(XTRKCAD_USE_GTK_DEFAULT ON)
	
	add_compile_options(-Wno-incompatible-pointer-types)

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
        add_compile_options("-pthread")
        add_link_options("-pthread")
   endif()

    add_compile_options("-Wall")
    # glib 2.0 deprecated GTypeDebugFlags and GTimeVal, gtk2 has not been updated
    add_compile_options("-Wno-deprecated-declarations")
    # paramData_t/descData_t positional aggregate literals (dialog control tables
    # throughout app/bin) deliberately set only their leading fields and rely on
    # C's implicit zero-init for the rest -- a longstanding codebase idiom, not
    # latent bugs. Suppress rather than force a large, regression-risk rewrite.
    add_compile_options("-Wno-missing-field-initializers")
    # Ratchet: Phases 4-6 of the GTK3V2MAIN quality plan brought these
    # categories to zero for owned code. Promote to errors so a future PR
    # can't silently reintroduce them. The few remaining instances are all
    # in vendored third-party targets (halibut/xtrkcad-charset/wrapbox),
    # which carry their own -Wno-error= overrides for exactly these.
    add_compile_options(-Werror=sign-compare)
    add_compile_options(-Werror=type-limits)
    add_compile_options(-Werror=absolute-value)
    add_compile_options(-Werror=unused-but-set-parameter)
    # implicit-fallthrough was initially GCC-only (Phase 10) because Clang
    # doesn't recognize this codebase's comment-style fallthrough annotation
    # -- never audited by the original GCC-only verification. Phase 12
    # (SF #663) fixed the ~73 genuine fallthrough sites and standardized on
    # __attribute__((fallthrough)) (recognized by both compilers), so it
    # ratchets universally now, no GNU-only guard needed.
    add_compile_options(-Werror=implicit-fallthrough)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        # cast-function-type stays GCC-only. Under Clang, this same flag
        # name also promotes a separate, stricter sub-check
        # (-Wcast-function-type-strict) that flags ~106 GTK/GLib generic-
        # callback-cast idiom instances (GCallback, GCompareFunc,
        # G_DEFINE_TYPE-generated code) as well as vendored code -- verified
        # directly (Clang 22, ccache disabled to rule out stale results)
        # that even the "plain" flag name pulls -strict in under Clang, so
        # it can't be ratcheted there without also flagging that permanently-
        # accepted idiom. Phase 12 (SF #662) fixed the ~13 genuine project-
        # typedef cast mismatches that WERE real bugs among the findings;
        # the rest -- see SF #662 for the full accounting -- are a
        # permanent, un-ratcheted Clang-only false-positive class, not
        # tracked as debt.
        add_compile_options(-Werror=cast-function-type)
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
		add_compile_options("$<$<CONFIG:DEBUG>:/W3>")
	else()
		add_compile_options("$<$<CONFIG:DEBUG>:-Wall>")
		# GCC 14 (MSYS2 MinGW) promotes this to a hard error; GTK3 type-system
		# pointer casts throughout the codebase rely on the same suppression
		# that the UNIX build already applies unconditionally.
		add_compile_options(-Wno-incompatible-pointer-types)
		add_compile_options(-Wno-deprecated-declarations)
		# paramData_t/descData_t positional aggregate literals (dialog control tables
		# throughout app/bin) deliberately set only their leading fields and rely on
		# C's implicit zero-init for the rest -- a longstanding codebase idiom, not
		# latent bugs. Suppress rather than force a large, regression-risk rewrite.
		add_compile_options(-Wno-missing-field-initializers)
		# Ratchet: Phases 4-6 of the GTK3V2MAIN quality plan brought these
		# categories to zero for owned code. Promote to errors so a future PR
		# can't silently reintroduce them. The few remaining instances are all
		# in vendored third-party targets (halibut/xtrkcad-charset/wrapbox),
		# which carry their own -Wno-error= overrides for exactly these.
		add_compile_options(-Werror=sign-compare)
		add_compile_options(-Werror=type-limits)
		add_compile_options(-Werror=absolute-value)
		add_compile_options(-Werror=unused-but-set-parameter)
		# See the matching UNIX-branch comment above (Phase 12, SF #662/#663):
		# implicit-fallthrough ratchets universally now; cast-function-type
		# stays GCC-only (Clang's -Wcast-function-type-strict sub-check is
		# not separable from it, and permanently un-ratcheted -- see #662).
		add_compile_options(-Werror=implicit-fallthrough)
		if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
			add_compile_options(-Werror=cast-function-type)
		endif()
	endif()

	add_compile_definitions(
		"$<$<CONFIG:DEBUG>:_DEBUG>"
	)

	add_compile_definitions(WINDOWS)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()
