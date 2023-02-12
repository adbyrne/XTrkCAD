# Test programs for wlib

add_custom_target(wlibtest
    DEPENDS testapp
)

add_executable(testapp)

target_sources(testapp
    PRIVATE
        testapp.c
)

target_link_libraries(testapp
    PRIVATE
    xtrkcad-wlib
)