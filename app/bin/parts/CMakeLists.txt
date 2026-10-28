# Setup GTK3 Parts functions...

add_library(parts
			"")

target_include_directories(parts
	PRIVATE
	${xtrkcad-lib_SOURCE_DIR}
	${xtrkcad-wlib}
	${XTRKCAD_CORE_INCLUDE_DIR}
)

target_link_libraries(parts
	PRIVATE
	PkgConfig::GTK3
	xtrkcad-wlib
	dynstring
)

target_sources(parts
	PRIVATE 
    partstore.c
	dpricels.c
)


