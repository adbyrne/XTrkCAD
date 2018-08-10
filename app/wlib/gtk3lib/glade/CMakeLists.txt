set(gladefiles 
    "cmdopt.glade"
    "display.glade"
    "easement.glade"
    "enum.glade"
    "layout.glade"
    "note.glade"
    "pref.glade"
    "pricelist.glade"
    "rgbcolor.glade"
    "sticky.glade"
    )

foreach( glade IN LISTS gladefiles )
    message( STATUS ${glade} )
    configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/${glade} ${CMAKE_BINARY_DIR}/ui/${glade} COPYONLY )
endforeach(glade)
