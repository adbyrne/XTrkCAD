

add_library(wrapbox)

target_sources(wrapbox
	PRIVATE 
	eggwrapbox.c	
)

target_include_directories(wrapbox
	PRIVATE
	${GTK3_INCLUDE_DIRS}
)
	
target_link_libraries(wrapbox 
	PRIVATE 
	${GTK3_LIBRARIES}
)

add_executable(testwrapbox)

target_sources(testwrapbox
	PRIVATE
	testwrapbox.c
)

target_link_libraries(testwrapbox
	PRIVATE
	wrapbox
)

target_include_directories(testwrapbox
	PRIVATE
	${GTK3_INCLUDE_DIRS}
)