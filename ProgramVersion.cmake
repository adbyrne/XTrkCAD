# Define program version

set(XTRKCAD_MAJOR_VERSION "5")
set(XTRKCAD_MINOR_VERSION "4")
set(XTRKCAD_RELEASE_VERSION "0")
set(XTRKCAD_VERSION_MODIFIER "")
set(XTRKCAD_VERSION "${XTRKCAD_MAJOR_VERSION}.${XTRKCAD_MINOR_VERSION}.${XTRKCAD_RELEASE_VERSION}${XTRKCAD_VERSION_MODIFIER}")

# Development builds get a fourth, dot-appended component: a short VCS hash
# of the checked-out commit (e.g. "5.4.1Dev.a1b2c3d4e5f6"), so that
# successive dev builds of the *same* nominal version don't collide when
# installed side by side (on top of #673's per-version package/binary
# naming, which only disambiguates by XTRKCAD_VERSION). A formal release
# build — the exact commit a "v<version>" tag points at — ignores this and
# keeps the plain three-part version.
#
# Release flow: tag the release commit (see the "Version" section of
# CLAUDE.md) so it builds with no hash suffix, then immediately bump
# XTRKCAD_RELEASE_VERSION (and XTRKCAD_VERSION_MODIFIER, if used) above for
# the next dev cycle — every untagged build after that automatically gets
# the hash suffix again, no further manual steps needed.
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

if(NOT XTRKCAD_IS_TAGGED_RELEASE AND XTRKCAD_SRC_HASH)
    set(XTRKCAD_VERSION "${XTRKCAD_VERSION}.${XTRKCAD_SRC_HASH}")
endif()
