# Test programs for wlib
add_executable(testapp)

target_sources(testapp
    PRIVATE
        testapp.c
        ${messagefile_BINARY_DIR}/tooltiptext.c
)

target_link_libraries(testapp
    PRIVATE
    xtrkcad-wlib
)

if(NOT WIN32)
	target_link_libraries(testapp
        PRIVATE
        m
    )
endif()