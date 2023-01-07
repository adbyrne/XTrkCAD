# copy glade definitions to binary's subdir

set (UIDEFS
    about.glade
	main.glade
    map.glade
)    

foreach(gladefile ${UIDEFS})
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

install(DIRECTORY "${xtrkcad-lib_BINARY_DIR}/ui" DESTINATION bin/ui/)