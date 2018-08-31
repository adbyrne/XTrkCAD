set(gladefiles 
    "aspectEdit.glade"
    "blockedit.glade"
    "cmdopt.glade"
    "contmgm.glade"
    "controlEdit.glade"
    "custmgm.glade"
    "display.glade"
    "easement.glade"
    "elev.glade"
    "enum.glade"
    "grid.glade"
    "helix.glade"
    "layer.glade"
    "layout.glade"
    "note.glade"
    "outputbitmap.glade"
    "pref.glade"
    "pricelist.glade"
    "print.glade"
    "prmfile.glade"
    "rescale.glade"
    "rgbcolor.glade"
    "signalEdit.glade"
    "sticky.glade"
    "tip.glade"
    )

foreach( glade IN LISTS gladefiles )
    message( STATUS ${glade} )
    configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/${glade} ${CMAKE_BINARY_DIR}/ui/${glade} COPYONLY )
endforeach(glade)
