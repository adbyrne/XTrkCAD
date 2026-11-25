# Rules for pre-release builds (Development Issues #94). ProgramVersion.cmake
# uses them; CMake/tests/PrereleaseRulesTest.cmake tests them.
#
# A build is a pre-release when XTRKCAD_VERSION_MODIFIER isn't empty (Beta1,
# RC1, ...). Every pre-release keeps its settings in one directory and one
# preferences file, named XTRKCAD_PRERELEASE_NAME, apart from the release's,
# so a tester's settings carry over from one pre-release to the next
# (dev ML #4735, #4736).

# Whether the install identity carries the version (xtrkcad-5.4.0 rather than
# xtrkcad). setting is XTRKCAD_VERSIONED_INSTALL: ON and OFF win; AUTO is OFF
# only for a tagged final release, so that installing a release replaces the
# previous one but a tagged pre-release installs beside it.
function(xtrkcad_install_versioned setting tagged prerelease out)
    if(setting STREQUAL "AUTO")
        if(tagged AND NOT prerelease)
            set(_versioned OFF)
        else()
            set(_versioned ON)
        endif()
    elseif(setting)
        set(_versioned ON)
    else()
        set(_versioned OFF)
    endif()
    set(${out} ${_versioned} PARENT_SCOPE)
endfunction()
