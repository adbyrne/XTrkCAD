
FILE(GLOB HEADERS *.h)

INCLUDE (FindGTKUnixPrint.cmake)

SET(SOURCES
	bitmap.c
	boxes.c
	button.c
# cproto and cppcheck
	color.c
	control.c
	droplist.c	
	filesel.c
	font.c
	help.c
	lines.c
	list.c
	liststore.c
	main.c
	menu.c
	message.c
	misc.c
	notice.c
	pixbuf.c
	png.c
	print.c
	single.c
	splash.c
	text.c
	timer.c
	tooltip.c
	treeview.c
	window.c
	wpref.c
# end of refactored sources	
	gtkdraw-cairo.c
	)

INCLUDE_DIRECTORIES(${XTrkCAD_BINARY_DIR})
INCLUDE_DIRECTORIES(${GTK_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${GTK_UNIX_PRINT_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${GTK_WEBKIT_INCLUDE_DIRS})

ADD_LIBRARY(xtrkcad-wlib ${HEADERS} ${SOURCES})
TARGET_LINK_LIBRARIES(xtrkcad-wlib ${GTK_LIBRARIES})
TARGET_LINK_LIBRARIES(xtrkcad-wlib ${GTK_UNIX_PRINT_LIBRARIES})
TARGET_LINK_LIBRARIES(xtrkcad-wlib ${GTK_WEBKIT_LIBRARIES})
