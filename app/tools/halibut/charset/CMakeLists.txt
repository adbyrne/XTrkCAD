#
# Character set conversion library as required for halibut

add_library(xtrkcad-charset
	big5enc.c 
	big5set.c 
	charset.h 
	cns11643.c 
	cp949.c
	euc.c 
	fromucs.c 
	gb2312.c
	hz.c 
	internal.h 
	iso2022.c 
	iso2022s.c
	istate.c 
	jisx0208.c 
	jisx0212.c 
	ksx1001.c
	locale.c 
	localenc.c 
	macenc.c 
	mimeenc.c
	sbcs.c 
	sbcsdat.c 
	shiftjis.c 
	slookup.c
	superset.c 
	test.c 
	toucs.c 
	utf16.c 
	utf7.c
	utf8.c 
	xenc.c)

set_target_properties(
	xtrkcad-charset
	PROPERTIES FOLDER HelpDocs
 )

 target_include_directories(xtrkcad-charset PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Vendored code predates the project's warning ratchet (GTK3V2MAIN quality
# plan Phase 10) and isn't ours to fix upstream. Blanket-exempt all six
# ratcheted categories rather than only the ones observed locally --
# architecture-specific findings (e.g. char-signedness differences between
# x86_64/ARM64) can surface in vendored code on platforms not tested here.
if(UNIX OR (WIN32 AND NOT MSVC))
	target_compile_options(xtrkcad-charset PRIVATE
		-Wno-error=sign-compare
		-Wno-error=type-limits
		-Wno-error=absolute-value
		-Wno-error=unused-but-set-parameter
		-Wno-error=cast-function-type
		-Wno-error=implicit-fallthrough
	)
endif()

