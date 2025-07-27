set(FLATPAK_BUILD_SCRIPT "${CMAKE_SOURCE_DIR}/distribution/flatpak/buildFlatpak.sh")
set(FLATPAK_FILE ${CMAKE_BINARY_DIR}/xtrkcad-${XTRKCAD_VERSION}.flatpak)

add_custom_command(
    OUTPUT ${FLATPAK_FILE}
    COMMAND ${FLATPAK_BUILD_SCRIPT} ${CMAKE_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Re-compiling xtrkcad within flatpak Sdk; this could take a while ..."
    DEPENDS ${FLATPAK_BUILD_SCRIPT}
)

# makes for an easy target: "make flatpak" or "ninja flatpak"
add_custom_command(
    OUTPUT flatpak
    COMMAND ${FLATPAK_BUILD_SCRIPT} ${CMAKE_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Re-compiling xtrkcad within flatpak Sdk; this could take a while ..."
    DEPENDS ${FLATPAK_BUILD_SCRIPT}
)

add_custom_target(flatpak
    DEPENDS ${FLATPAK_FILE}
    COMMENT "Generating xtrkcad-${XTRKCAD_VERSION}.flatpak"
)
