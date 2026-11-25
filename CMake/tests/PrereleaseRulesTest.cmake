# Tests for the pre-release rules in CMake/XtrkcadPrerelease.cmake
# (Development Issues #94). Run with cmake -P; the PrereleaseRules ctest
# (top-level CMakeLists.txt) does that.

include("${CMAKE_CURRENT_LIST_DIR}/../XtrkcadPrerelease.cmake")

function(expect_versioned setting tagged prerelease expected)
    xtrkcad_install_versioned("${setting}" "${tagged}" "${prerelease}" _versioned)
    if(NOT _versioned STREQUAL "${expected}")
        message(SEND_ERROR "${setting} tagged=${tagged} prerelease=${prerelease}: ${_versioned}, expected ${expected}")
    endif()
endfunction()

# AUTO: only a tagged final release installs unversioned, over the previous one
expect_versioned(AUTO TRUE FALSE OFF)
expect_versioned(AUTO TRUE TRUE ON)
expect_versioned(AUTO FALSE FALSE ON)
expect_versioned(AUTO FALSE TRUE ON)
# ON and OFF win
expect_versioned(ON TRUE FALSE ON)
expect_versioned(OFF TRUE TRUE OFF)
expect_versioned(OFF FALSE FALSE OFF)
