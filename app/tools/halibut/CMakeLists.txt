# first create the charset library
add_subdirectory(charset)

include_directories( charset )

set(halibutsrcs biblio.c bk_html.c contents.c deflate.c error.c)
set(halibutsrcs ${halibutsrcs} halibut.h help.c index.c input.c)
set(halibutsrcs ${halibutsrcs} in_pf.c in_afm.c psdata.c bk_paper.c)
set(halibutsrcs ${halibutsrcs} keywords.c licence.c main.c malloc.c)
set(halibutsrcs ${halibutsrcs} misc.c tree234.c tree234.h ustring.c)
set(halibutsrcs ${halibutsrcs} version version.c wcwidth.c)

add_executable(halibut ${halibutsrcs})

target_link_libraries(halibut charset)
