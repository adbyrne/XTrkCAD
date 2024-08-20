# create resource file to be compiled into the program
cmake_minimum_required(VERSION 3.20)

set (RESOURCE_XML wlib.gresource.xml)
set (RESOURCE_XML_IN ${RESOURCE_XML}.in)
set (RESOURCES_C resources.c)
set (RESOURCES_H resources.h)
set (RESOURCE_DEPENDENCIES CACHE INTERNAL "Resource dependencies")

# find the tool
find_program(RESOURCE_COMPILER NAMES glib-compile-resources REQUIRED)

if(NOT EXISTS ${RESOURCE_XML} OR (${RESOURCE_XML_IN} IS_NEWER_THAN ${RESOURCE_XML}))
    #create the XML file
    configure_file(${RESOURCE_XML}.in  ${RESOURCE_XML})

    execute_process(
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${RESOURCE_COMPILER} --generate-dependencies ${RESOURCE_XML}
        OUTPUT_VARIABLE RESOURCE_DEPENDENCIES 
    )

    # OUTPUT_VARIABLE is not a list but a single string value with newlines
    # Convert it to a list and pop out the last newline character
    string(REPLACE "\n" ";" RESOURCE_DEPENDENCIES ${RESOURCE_DEPENDENCIES})
    list(POP_BACK RESOURCE_DEPENDENCIES)
endif()

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
    DEPENDS ${RESOURCE_DEPENDENCIES}
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
    DEPENDS ${RESOURCE_DEPENDENCIES}
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