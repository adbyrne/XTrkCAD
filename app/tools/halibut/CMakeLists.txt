
add_executable(halibut 	
	biblio.c 
	bk_html.c 
	contents.c 
	deflate.c 
	error.c
	halibut.h 
	help.c 
	index.c 
	input.c
	in_pf.c 
	in_afm.c 
	psdata.c 
	bk_paper.c
	keywords.c 
	licence.c 
	main.c 
	malloc.c
	misc.c 
	tree234.c 
	tree234.h 
	ustring.c
	version.c 
	wcwidth.c
)

set_target_properties(
   halibut
   PROPERTIES FOLDER HelpDocs
)
target_link_libraries(halibut PRIVATE xtrkcad-charset)

# Vendored code predates the project's warning ratchet (GTK3V2MAIN quality
# plan Phase 10) and isn't ours to fix upstream. Blanket-exempt all six
# ratcheted categories rather than only the ones observed locally --
# bk_paper.c's char-signedness-dependent type-limits finding only surfaced
# on ARM64/MinGW GCC, not the x86_64 GCC this was first verified against,
# so per-category overrides risk missing architecture-specific findings.
if(UNIX OR (WIN32 AND NOT MSVC))
	target_compile_options(halibut PRIVATE
		-Wno-error=sign-compare
		-Wno-error=type-limits
		-Wno-error=absolute-value
		-Wno-error=unused-but-set-parameter
		-Wno-error=cast-function-type
		-Wno-error=implicit-fallthrough
	)
endif()

add_subdirectory(charset)

