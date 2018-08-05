set(gladefiles 
    "enum.glade"
    "note.glade"
    "rgbcolor.glade"
    )

foreach( glade IN LISTS gladefiles )
    message( STATUS ${glade} )
    configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/${glade} ${CMAKE_BINARY_DIR}/ui/${glade} COPYONLY )
endforeach(glade)
