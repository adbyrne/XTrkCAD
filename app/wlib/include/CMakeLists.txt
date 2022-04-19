
target_include_directories(xtrkcad-wlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_sources(xtrkcad-wlib
    PUBLIC
        wlib.h)

if(WIN32)
    target_sources(xtrkcad-wlib
        PRIVATE
            getopt.h)
endif(WIN32)