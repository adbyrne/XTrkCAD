
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

# Vendored code: deflate.c predates the project's warning ratchet
# (GTK3V2MAIN quality plan Phase 10) and isn't ours to fix upstream.
if(UNIX OR (WIN32 AND NOT MSVC))
	target_compile_options(halibut PRIVATE -Wno-error=sign-compare)
endif()

add_subdirectory(charset)

