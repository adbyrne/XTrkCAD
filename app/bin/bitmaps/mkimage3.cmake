
set ( IMG ${CMAKE_ARGV3} )

file( WRITE ${CMAKE_CURRENT_BINARY_DIR}/${IMG}.image3 "#include \"${IMG}16.image1\"\n" )
file( APPEND ${CMAKE_CURRENT_BINARY_DIR}/${IMG}.image3 "#include \"${IMG}24.image1\"\n" )
file( APPEND ${CMAKE_CURRENT_BINARY_DIR}/${IMG}.image3 "#include \"${IMG}32.image1\"\n" )
STRING(MAKE_C_IDENTIFIER ${IMG} NAME )
file( APPEND ${CMAKE_CURRENT_BINARY_DIR}/${IMG}.image3 "static wIconBitMap_t ${NAME}_image3[3] = { ${NAME}16_image1, ${NAME}24_image1, ${NAME}32_image1 };\n" )
	#MESSAGE( "OUTFILE=${CMAKE_CURRENT_BINARY_DIR}/${IMG}.image3" )
