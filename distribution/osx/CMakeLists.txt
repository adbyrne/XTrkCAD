configure_file("${CMAKE_CURRENT_SOURCE_DIR}/README-INSTALL.txt.in" "${CMAKE_CURRENT_BINARY_DIR}/README-INSTALL.txt" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/README-INSTALL.txt"
    DESTINATION ".")

# Automatically generate most of our external dependencies ...
SET(XTRKCAD_LOCATION $<TARGET_FILE:${XTRKCAD_BIN}>)
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/dependencies.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/dependencies.cmake" @ONLY)
INSTALL(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/dependencies.cmake")


# Generate bundles ...
ADD_SUBDIRECTORY(bundle)

# Add replacement bundle technique ...
FILE(COPY gtk-bundle/ DESTINATION gtk-bundle )

ADD_SUBDIRECTORY(gtk-bundle)

