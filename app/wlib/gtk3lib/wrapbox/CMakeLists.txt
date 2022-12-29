
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
