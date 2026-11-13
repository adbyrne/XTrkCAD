add_library(wrapbox)

target_sources(wrapbox
	PRIVATE 
	eggwrapbox.c	
)

target_link_libraries(wrapbox
	PRIVATE
	PkgConfig::GTK3
)

add_executable(testwrapbox)

target_sources(testwrapbox
	PRIVATE
	testwrapbox.c
)

target_link_libraries(testwrapbox
	PRIVATE
	wrapbox
	PkgConfig::GTK3
)

# Vendored code (Gtk+ eggwrapbox): predates the project's warning ratchet
# (GTK3V2MAIN quality plan Phase 10) and isn't ours to fix upstream.
# Blanket-exempt all the ratcheted categories (PlatformSettings.cmake)
# rather than only the ones observed locally -- architecture-specific
# findings (e.g. char-signedness differences between x86_64/ARM64) can
# surface on platforms not tested here.
if(UNIX OR (WIN32 AND NOT MSVC))
	target_compile_options(wrapbox PRIVATE ${XTRKCAD_RATCHET_EXEMPT_OPTIONS})
	target_compile_options(testwrapbox PRIVATE ${XTRKCAD_RATCHET_EXEMPT_OPTIONS})
endif()