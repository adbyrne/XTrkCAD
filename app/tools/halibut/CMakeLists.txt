SET(SOURCES
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

INCLUDE_DIRECTORIES(charset)
ADD_EXECUTABLE(halibut ${SOURCES})
TARGET_LINK_LIBRARIES(halibut xtrkcad-charset)

ADD_SUBDIRECTORY(charset)

