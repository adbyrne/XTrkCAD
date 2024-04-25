# create resource file to be compiled into the program

set (RESOURCE_XML wlib.gresource.xml)
set (RESOURCES_C resources.c)
set (RESOURCES_H resources.h)

set(UI_FILES 
    appwindow.ui
    basicdialog.ui
    note.ui)

# find the tool
find_program(RESOURCE_COMPILER NAMES glib-compile-resources REQUIRED)

#create the XML file
configure_file(${RESOURCE_XML}.in  ${RESOURCE_XML})

# use the XML file to create a include file
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${RESOURCES_H}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${RESOURCE_COMPILER}
    ARGS
        --generate
        --target=${CMAKE_CURRENT_BINARY_DIR}/${RESOURCES_H}
        ${CMAKE_CURRENT_BINARY_DIR}/${RESOURCE_XML}
    VERBATIM
    MAIN_DEPENDENCY ${RESOURCE_XML}   
)

#use the XML file to compile the resources into a source
add_custom_command(
    OUTPUT ${RESOURCES_C}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${RESOURCE_COMPILER}
    ARGS
        --generate
        --target=${CMAKE_CURRENT_BINARY_DIR}/${RESOURCES_C}
        ${CMAKE_CURRENT_BINARY_DIR}/${RESOURCE_XML}
    VERBATIM
    MAIN_DEPENDENCY ${RESOURCE_XML}
    DEPENDS
        ${UI_FILES}
        xtrackcad.css
)

# define target
add_custom_target(
    resource-target
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/${RESOURCE_XML}
        ${CMAKE_CURRENT_BINARY_DIR}/${RESOURCES_C} 
        ${CMAKE_CURRENT_BINARY_DIR}/${RESOURCES_H}
)

set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/${RESOURCES_C}
    PROPERTIES GENERATED TRUE
)

target_sources(xtrkcad-wlib
    PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/${RESOURCES_C}
)

target_include_directories(xtrkcad-wlib
	PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)    