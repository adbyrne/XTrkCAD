# pass -DFLATPAK_STATE_DIR=/path/to/.flatpak-builder-xtrkcad/ (created if not present)
# necessary if source and build directory not on same file system
if (NOT DEFINED FLATPAK_STATE_DIR)
    # holds cache of build artifacts so everything doesn't have to be rebuilt
    set(FLATPAK_STATE_DIR $ENV{HOME}/.flatpak-builder-xtrkcad/)
endif()
set(FLATPAK_REPO "temp.repo")
set(FLATPAK_WORK_DIR "temp.work")
set(FLATPAK_BUILD_SCRIPT "${CMAKE_SOURCE_DIR}/distribution/flatpak/buildFlatpak.sh")
string(TIMESTAMP XTRKCAD_BUILD_TIMESTAMP "%a %b %d %H:%M:%S UTC %Y" UTC)
string(TIMESTAMP XTRKCAD_BUILD_DATE "%Y-%m-%d" UTC)
string(REGEX REPLACE "^.*/" "" XTRKCAD_BASE_NAME ${XTRKCAD_SHARE_INSTALL_DIR})
string(REGEX REPLACE ${XTRKCAD_PACKAGE} "" XTRKCAD_SUFFIX_NAME ${XTRKCAD_BASE_NAME})
execute_process(COMMAND ${Hg_EXECUTABLE} "log" "-r" "." "-T" "'{rev}'" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE XTRKCAD_SRC_CHANGESET)
string(REGEX MATCH "[0-9]+" XTRKCAD_SRC_CHANGESET ${XTRKCAD_SRC_CHANGESET})
set(FLATPAK_FILE ${CMAKE_BINARY_DIR}/xtrkcad-${XTRKCAD_VERSION}-${XTRKCAD_SRC_CHANGESET}.${CMAKE_SYSTEM_PROCESSOR}.flatpak)
string(REGEX REPLACE "^.*/" "" FLATPAK_BASE_FILE ${FLATPAK_FILE})

# source .in file prefixes
set(_XTRK_DESKTOP_SRC_FILE "desktop")
set(_XTRK_META_SRC_FILE "metainfo.xml")
set(_XTRK_MANIFEST_SRC_FILE "manifest.yaml")

# flatpak domain build from flathub
set(FLATPAK_DOMAIN "io.sourceforge.xtrkcad_fork")

# flatpak domain local build
#set(FLATPAK_DOMAIN "org.xtrkcad")

set(_XTRK_DESKTOP_FILE "xtrkcad.desktop")
set(_XTRK_META_FILE "xtrkcad.metainfo.xml")
set(_XTRK_MANIFEST_FILE "xtrkcad.manifest.yaml")

# desktop.in
string(CONCAT _XTRK_DESKTOP_IN_FILE
       ${CMAKE_CURRENT_SOURCE_DIR}
       "/"
       ${_XTRK_DESKTOP_SRC_FILE}
       ".in"
      )

#######################################################################
#   Note that some files in the source are modified based on the
#   .in file in order that flathub builds require less patching
#   since these files (the non .in ones) are used in the manifest.
#
#   When a cmake is performed and flatpak tools are available, these
#   will be modified for your checking in pleasure.
#######################################################################

# updates source on purpose for flathub builds
configure_file(${_XTRK_DESKTOP_IN_FILE} "${CMAKE_CURRENT_SOURCE_DIR}/${_XTRK_DESKTOP_FILE}")

# metainfo.xml.in
string(CONCAT _XTRK_META_IN_FILE
       ${CMAKE_CURRENT_SOURCE_DIR}
       "/"
       ${_XTRK_META_SRC_FILE}
       ".in"
      )

# updates source on purpose for flathub builds
configure_file(${_XTRK_META_IN_FILE} "${CMAKE_CURRENT_SOURCE_DIR}/${_XTRK_META_FILE}")

# manifest.yaml.in
string(CONCAT _XTRK_MANIFEST_IN_FILE
       ${CMAKE_CURRENT_SOURCE_DIR}
       "/"
       ${_XTRK_MANIFEST_SRC_FILE}
       ".in"
      )

# build manifest file to use for building in sandbox
# note that the manifest file is not modified in the source tree as it's
# maintained in the git repo (to be documented later)
configure_file(${_XTRK_MANIFEST_IN_FILE} "${_XTRK_MANIFEST_FILE}" @ONLY)

# needed for gtk2
file(COPY
    ${CMAKE_CURRENT_SOURCE_DIR}/shared-modules
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
)

# the flatpak commands to make a local flatpak (official flatpak builds
# maintained by github repo to be documented later)
add_custom_command(
    OUTPUT ${FLATPAK_FILE}
    COMMAND ${Flatpak_EXECUTABLE} 
            remote-add --if-not-exists --user 
            flathub https://flathub.org/repo/flathub.flatpakrepo
    COMMAND ${Flatpak_builder_EXECUTABLE}
            --state-dir=${FLATPAK_STATE_DIR}
            --force-clean --user --install --repo=${FLATPAK_REPO}
            --install-deps-from=flathub
            ${FLATPAK_WORK_DIR} ${_XTRK_MANIFEST_FILE}
    COMMAND rm -rf ${FLATPAK_WORK_DIR}
    COMMAND ${Flatpak_EXECUTABLE} build-bundle
            ${FLATPAK_REPO}
            ${FLATPAK_FILE}
            ${FLATPAK_DOMAIN}.${XTRKCAD_BASE_NAME}
    COMMAND rm -rf ${FLATPAK_REPO}
    COMMENT "Re-compiling xtrkcad within flatpak Sdk; this could take a while ..."
    DEPENDS ${_XTRK_MANIFEST_FILE} ${_XTRK_DESKTOP_FILE} ${_XTRK_META_FILE}
)

# makes for an easy target: "make flatpak" or "ninja flatpak"
add_custom_command(
    OUTPUT flatpak
    DEPENDS ${FLATPAK_FILE}
)

add_custom_target(flatpak
    DEPENDS ${FLATPAK_FILE}
    COMMENT "Generating ${FLATPAK_BASE_FILE}"
)

if( DEFINED DEBUG_FLATPAK )
function(print_all_variables)
    get_cmake_property(_vars VARIABLES)
    list(SORT _vars)
    foreach(_var IN LISTS _vars)
        message(STATUS "${_var}= ${${_var}}")
    endforeach()
endfunction()

print_all_variables()
endif()
