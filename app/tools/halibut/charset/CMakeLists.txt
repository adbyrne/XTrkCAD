# perl script to create the sbcsdat.c and sbcsdat.h 

add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sbcsdat.c
						  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/sbcsgen.pl
						  DEPENDS sbcs.dat )	

# these are the sources for the charset library

set(charsetsrcs big5enc.c big5set.c charset.h cns11643.c cp949.c)
set(charsetsrcs ${charsetsrcs} euc.c fromucs.c gb2312.c)
set(charsetsrcs ${charsetsrcs} hz.c internal.h iso2022.c iso2022s.c)
set(charsetsrcs ${charsetsrcs} istate.c jisx0208.c jisx0212.c ksx1001.c)
set(charsetsrcs ${charsetsrcs} locale.c localenc.c macenc.c mimeenc.c)
set(charsetsrcs ${charsetsrcs} sbcs.c sbcsdat.c shiftjis.c slookup.c )
set(charsetsrcs ${charsetsrcs} superset.c test.c toucs.c utf16.c utf7.c)
set(charsetsrcs ${charsetsrcs} utf8.c xenc.c)

# build the library

add_library(charset ${charsetsrcs})
