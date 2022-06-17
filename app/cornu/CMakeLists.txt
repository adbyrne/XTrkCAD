project(cornu)
	
add_library(xtrkcad-cornu "")

target_sources(xtrkcad-cornu
	PRIVATE
		bezctx.c
		bezctx_xtrkcad.c
		spiroentrypoints.c
		spiro.c
		bezctx.h
		bezctx_intf.h
		zmisc.h	
)

target_sources(xtrkcad-cornu
	PUBLIC
		bezctx_xtrkcad.h
		spiroentrypoints.h
		spiro.h
)

target_link_libraries(xtrkcad-cornu
	PRIVATE
		xtrkcad-wlib
)

target_include_directories(xtrkcad-cornu PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
