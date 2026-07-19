find_package(GTK3 REQUIRED)

add_library(wrapbox)

target_sources(wrapbox
	PRIVATE 
	eggwrapbox.c	
)

target_include_directories(wrapbox
	PRIVATE
	${GTK3_INCLUDE_DIRS}
)
	
target_link_libraries(wrapbox 
	PRIVATE 
	${GTK3_LIBRARIES}
)

add_executable(testwrapbox)

target_sources(testwrapbox
	PRIVATE
	testwrapbox.c
)

target_link_libraries(testwrapbox
	PRIVATE
	wrapbox
)

target_include_directories(testwrapbox
	PRIVATE
	${GTK3_INCLUDE_DIRS}
)

# Vendored code (Gtk+ eggwrapbox): predates the project's warning ratchet
# (GTK3V2MAIN quality plan Phase 10) and isn't ours to fix upstream.
if(UNIX OR (WIN32 AND NOT MSVC))
	target_compile_options(wrapbox PRIVATE -Wno-error=sign-compare)
	target_compile_options(testwrapbox PRIVATE -Wno-error=sign-compare)
endif()