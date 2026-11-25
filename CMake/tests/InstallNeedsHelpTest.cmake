# Without Doxygen, cmake --install stops before it copies anything
# (Development Issues #98). Configures the source again in a scratch
# directory with Doxygen hidden; the install's check runs first, so nothing
# has to be built. Run by the InstallNeedsHelp ctest (top-level CMakeLists.txt):
#   cmake -DSRC=<source> -DWORK=<scratch dir> -DGENERATOR=<gen> -P InstallNeedsHelpTest.cmake

file(REMOVE_RECURSE "${WORK}")
execute_process(
    COMMAND ${CMAKE_COMMAND} -S "${SRC}" -B "${WORK}/build" -G "${GENERATOR}"
        -DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON -DXTRKCAD_TESTING=OFF
    RESULT_VARIABLE _configure
    OUTPUT_VARIABLE _configure_out
    ERROR_VARIABLE _configure_out)
if(NOT _configure EQUAL 0)
    message(FATAL_ERROR "configure without Doxygen failed:\n${_configure_out}")
endif()
if(NOT _configure_out MATCHES "Doxygen not found: the User Guide")
    message(FATAL_ERROR "configure without Doxygen gave no warning:\n${_configure_out}")
endif()

# A plain install has to stop, with the message, before copying anything
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env --unset=XTRKCAD_INSTALL_WITHOUT_HELP
        ${CMAKE_COMMAND} --install "${WORK}/build" --prefix "${WORK}/install"
    RESULT_VARIABLE _install
    OUTPUT_VARIABLE _install_out
    ERROR_VARIABLE _install_out)
if(_install EQUAL 0)
    message(FATAL_ERROR "install without the User Guide succeeded:\n${_install_out}")
endif()
if(NOT _install_out MATCHES "User Guide \\(help\\) wasn't built")
    message(FATAL_ERROR "install stopped without the User Guide message:\n${_install_out}")
endif()
if(EXISTS "${WORK}/install")
    message(FATAL_ERROR "install copied files before it stopped")
endif()

file(REMOVE_RECURSE "${WORK}")
message(STATUS "install without the User Guide stopped before copying anything")
