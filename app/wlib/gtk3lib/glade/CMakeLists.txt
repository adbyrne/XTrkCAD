# copy glade definitions to binary

set (UIDEFS
		main.glade
        map.glade
    )    

foreach(gladefile ${UIDEFS})
    message(STATUS "${CMAKE_CURRENT_SOURCE_DIR}/${gladefile} >> ${xtrkcad-lib_BINARY_DIR}/ui")
    add_custom_command(
					OUTPUT ${xtrkcad-lib_BINARY_DIR}/ui/${gladefile}
					DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${gladefile}
					COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${gladefile} "${xtrkcad-lib_BINARY_DIR}/ui"
    )
    list(APPEND GLADE_RESULTS "${gladefile}")

endforeach()

add_custom_target( uifiles
    DEPENDS ${GLADE_RESULTS}
)

install(DIRECTORY ${CMAKE_XTRKCAD_BINARY_DIR} DESTINATION bin/ui/)