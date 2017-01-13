
FILE(GLOB HEADERS *.h)

INCLUDE (FindGTKUnixPrint.cmake)

SET(SOURCES
	gtkbitmap.c
	gtkbutton.c
	gtkcolor.c
	gtkdraw-cairo.c
	filesel.c
	gtkfont.c
	gtklist.c
	gtkmenu.c
	gtkmisc.c
	gtksimple.c
	gtksingle.c
	gtksplash.c
	gtktext.c
	gtkwindow.c
	gtkxpm.c
	gtkhelp.c
	print.c
	wpref.c
	)

IF(APPLE)
	SET(SOURCES 
		${SOURCES}
		osxhelp.c)
ELSE(APPLE)
# for testing only, has to be ixhelp.c for Unixes
	SET(SOURCES 
		${SOURCES}
		osxhelp.c)
ENDIF(APPLE)

SET_SOURCE_FILES_PROPERTIES(wpref.c PROPERTIES COMPILE_FLAGS -DEXPORT=)

INCLUDE_DIRECTORIES(${XTrkCAD_BINARY_DIR})
INCLUDE_DIRECTORIES(${GTK_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${GTK_UNIX_PRINT_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${GTK_WEBKIT_INCLUDE_DIRS})

ADD_LIBRARY(xtrkcad-wlib ${HEADERS} ${SOURCES})
TARGET_LINK_LIBRARIES(xtrkcad-wlib ${GTK_LIBRARIES})
TARGET_LINK_LIBRARIES(xtrkcad-wlib ${GTK_UNIX_PRINT_LIBRARIES})
TARGET_LINK_LIBRARIES(xtrkcad-wlib ${GTK_WEBKIT_LIBRARIES})
