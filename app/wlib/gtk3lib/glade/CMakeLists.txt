set(gladefiles 
    "aspectEdit.glade"
    "blockedit.glade"
    "cmdopt.glade"
    "contmgm.glade"
    "custmgm.glade"
    "display.glade"
    "easement.glade"
    "enum.glade"
    "grid.glade"
    "layout.glade"
    "note.glade"
    "outputbitmap.glade"
    "pref.glade"
    "pricelist.glade"
    "print.glade"
    "prmfile.glade"
    "rgbcolor.glade"
    "signalEdit.glade"
    "sticky.glade"
    )

foreach( glade IN LISTS gladefiles )
    message( STATUS ${glade} )
    configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/${glade} ${CMAKE_BINARY_DIR}/ui/${glade} COPYONLY )
endforeach(glade)
