#
# build the cJSON object library

add_library( cJSON  "")

target_sources(cJSON
	PRIVATE
		cJSON.c
	PUBLIC
		cJSON.h
)

target_include_directories(cJSON PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})	