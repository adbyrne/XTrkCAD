# set(FONTS "xtrackcad-10.bdf" "xtrackcad-15.bdf" "xtrackcad-20.bdf")
foreach( FILE "xtrackcad-10.bdf" "xtrackcad-15.bdf" "xtrackcad-20.bdf")

install(
	FILES "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}"
	DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}
)
endforeach()