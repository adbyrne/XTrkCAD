# Define program version

set(XTRKCAD_MAJOR_VERSION "5")
set(XTRKCAD_MINOR_VERSION "4")
set(XTRKCAD_RELEASE_VERSION "0")
set(XTRKCAD_VERSION_MODIFIER "")
set(XTRKCAD_VERSION "${XTRKCAD_MAJOR_VERSION}.${XTRKCAD_MINOR_VERSION}.${XTRKCAD_RELEASE_VERSION}${XTRKCAD_VERSION_MODIFIER}")

# Dev builds get a short VCS hash of the checked-out commit detected below
# (e.g. "a1b2c3d4e5f6"). A formal release build — the exact commit a
# "v<version>" tag points at — has no hash: XTRKCAD_SRC_HASH stays empty.
#
# Works from either a git or an Hg checkout, since this file is shared by
# both trees (see CLAUDE.md's Project Layout).
set(XTRKCAD_IS_TAGGED_RELEASE FALSE)
set(XTRKCAD_SRC_HASH "")

if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
    find_program(XTRKCAD_GIT_EXECUTABLE git)
    if(XTRKCAD_GIT_EXECUTABLE)
        execute_process(
            COMMAND ${XTRKCAD_GIT_EXECUTABLE} describe --tags --exact-match --match "v*"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESULT_VARIABLE XTRKCAD_GIT_TAG_RESULT
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(XTRKCAD_GIT_TAG_RESULT EQUAL 0)
            set(XTRKCAD_IS_TAGGED_RELEASE TRUE)
        else()
            execute_process(
                COMMAND ${XTRKCAD_GIT_EXECUTABLE} rev-parse --short=12 HEAD
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE XTRKCAD_SRC_HASH
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
        endif()
    endif()
elseif(EXISTS "${CMAKE_SOURCE_DIR}/.hg")
    find_program(XTRKCAD_HG_EXECUTABLE hg)
    if(XTRKCAD_HG_EXECUTABLE)
        execute_process(
            COMMAND ${XTRKCAD_HG_EXECUTABLE} log -r "." -T "{tags}"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE XTRKCAD_HG_TAGS
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        # {tags} also includes the implicit "tip" tag on the tip revision;
        # look specifically for a "v<digit>..." release tag among them.
        if(XTRKCAD_HG_TAGS MATCHES "(^|[ \t])v[0-9]")
            set(XTRKCAD_IS_TAGGED_RELEASE TRUE)
        else()
            execute_process(
                COMMAND ${XTRKCAD_HG_EXECUTABLE} log -r "." -T "{node|short}"
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE XTRKCAD_SRC_HASH
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
        endif()
    endif()
endif()

# XTRKCAD_VERSION always carries the dev-build hash suffix when one was
# detected above (e.g. "5.4.1Dev.a1b2c3d4e5f6"). It's compiled into the
# binary (xtrkcad-config.h's XTRKCAD_VERSION macro) and used wherever the
# *exact commit* matters as an internal value: the About/--version string,
# and misc.c's stored-preference check that detects "this is a new build
# since last run". A tagged release build keeps the plain three-part
# version, same as XTRKCAD_FILE_VERSION below.
if(NOT XTRKCAD_IS_TAGGED_RELEASE AND XTRKCAD_SRC_HASH)
    set(XTRKCAD_VERSION "${XTRKCAD_VERSION}.${XTRKCAD_SRC_HASH}")
endif()

# XTRKCAD_FILE_VERSION is the version string used anywhere it becomes part
# of a filename or path: the installed binary name, package/installer
# filenames, install directories, desktop file, and (via custom.c's
# sProdNameLower) the runtime work/prefs directory. Unlike XTRKCAD_VERSION
# above, it only gets the hash suffix when XTRKCAD_APPEND_SRC_HASH is on.
#
# With the option off (the default), every untagged dev build resolves to
# the same XTRKCAD_FILE_VERSION, so installing one just overwrites the
# previous dev install/binary in place — matching the historic behavior a
# developer would expect from a plain three-part dev version, and avoiding
# an ever-growing pile of same-version installs side by side on a dev
# machine. With it on, each commit gets its own filename/install path
# (building on #673's per-version package/binary naming, which only
# disambiguates by nominal version), so successive dev builds never
# collide or overwrite each other — the behavior CI wants, since concurrent
# jobs and artifact caches must never collide across commits, and what
# defaults on automatically there.
#
# Release flow: tag the release commit (see the "Version" section of
# CLAUDE.md) so both XTRKCAD_VERSION and XTRKCAD_FILE_VERSION build with no
# hash suffix, then immediately bump XTRKCAD_RELEASE_VERSION (and
# XTRKCAD_VERSION_MODIFIER, if used) above for the next dev cycle — every
# untagged build after that automatically gets the hash suffix again where
# applicable, no further manual steps needed.
if(DEFINED ENV{CI})
    set(_xtrkcad_append_src_hash_default ON)
else()
    set(_xtrkcad_append_src_hash_default OFF)
endif()
option(XTRKCAD_APPEND_SRC_HASH
    "Also give untagged dev builds' filenames/install paths their own short VCS hash, instead of overwriting the previous dev install"
    ${_xtrkcad_append_src_hash_default})

if(XTRKCAD_APPEND_SRC_HASH)
    set(XTRKCAD_FILE_VERSION "${XTRKCAD_VERSION}")
else()
    set(XTRKCAD_FILE_VERSION "${XTRKCAD_MAJOR_VERSION}.${XTRKCAD_MINOR_VERSION}.${XTRKCAD_RELEASE_VERSION}${XTRKCAD_VERSION_MODIFIER}")
endif()
