# Setup GTK3 Parts functions...

add_library(parts
			"")

# GTK 3
find_package (GTK3 REQUIRED)

target_include_directories(parts
	PRIVATE
	${xtrkcad-lib_SOURCE_DIR}
	${xtrkcad-wlib}
	${XTRKCAD_CORE_INCLUDE_DIR}
	${GTK3_INCLUDE_DIRS}
)

target_link_libraries(parts
	PRIVATE
	${GTK3_LIBRARIES}
	xtrkcad-wlib
	dynstring
)

target_sources(parts
	PRIVATE 
    partstore.c
	dpricels.c
)


