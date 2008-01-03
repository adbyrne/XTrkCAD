SET(SOURCES 
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
	xenc.c
	)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

ADD_LIBRARY(xtrkcad-charset ${SOURCES})

