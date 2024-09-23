# Test programs for wlib
add_executable(testapp)

target_sources(testapp
    PRIVATE
        testapp.c
)

target_link_libraries(testapp
    PRIVATE
    xtrkcad-wlib
)