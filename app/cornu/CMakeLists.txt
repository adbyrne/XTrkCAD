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
		spiro.h
		spiroentrypoints.h
)

target_link_libraries(xtrkcad-cornu
	PRIVATE
		xtrkcad-wlib)
