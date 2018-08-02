set(gladefiles 
    "note.glade"
    )
message(STATUS ${CMAKE_CURRENT_SOURCE_DIR} )
message(STATUS  ${CMAKE_BINARY_DIR}/ui)




foreach( glade IN LISTS gladefiles )
    message( STATUS ${glade} )
    configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/${glade} ${CMAKE_BINARY_DIR}/ui/${glade} COPYONLY )
endforeach(glade)
