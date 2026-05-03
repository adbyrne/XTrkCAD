
# Try to find an SVG rasterizer (rsvg-convert or Inkscape)
# Once done this will define
#
# Inkscape_FOUND
# Inkscape_EXECUTABLE   Where to find the rasterizer
# Inkscape_VERSION      The rasterizer version number
# Inkscape_EXPORT       Option to specify the destination file (Inkscape only)
# Inkscape_GUI          Option to disable the GUI if needed (Inkscape only)
# Inkscape_IS_RSVG      TRUE if rsvg-convert is being used instead of Inkscape
#
# Original module from https://github.com/arx/ArxLibertatis
# Extended to prefer rsvg-convert (no D-Bus session required, works headless/CI)

# Prefer rsvg-convert: lighter weight, no D-Bus session required, works in CI
find_program(RsvgConvert_EXECUTABLE NAMES rsvg-convert DOC "rsvg-convert SVG rasterizer")

if(RsvgConvert_EXECUTABLE)
	set(Inkscape_EXECUTABLE ${RsvgConvert_EXECUTABLE} CACHE FILEPATH "SVG rasterizer (rsvg-convert)" FORCE)
	set(Inkscape_IS_RSVG TRUE CACHE BOOL "SVG conversion uses rsvg-convert instead of Inkscape")
	execute_process(COMMAND ${RsvgConvert_EXECUTABLE} "--version" OUTPUT_VARIABLE _RSVG_VERSION ERROR_QUIET)
	STRING(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" _RSVG_VERSION "${_RSVG_VERSION}")
	set(Inkscape_VERSION ${_RSVG_VERSION} CACHE STRING "SVG converter version")
	message(STATUS "Found rsvg-convert: ${RsvgConvert_EXECUTABLE} (${Inkscape_VERSION}) — using instead of Inkscape")
	include(FindPackageHandleStandardArgs)
	find_package_handle_standard_args(Inkscape REQUIRED_VARS Inkscape_EXECUTABLE)
	return()
endif()

find_program(
	Inkscape_EXECUTABLE
	NAMES inkscape
    HINTS "C:/Program Files/Inkscape/bin"
	DOC "Inkscape command-line SVG rasterizer"
)

execute_process(COMMAND ${Inkscape_EXECUTABLE} "--version" OUTPUT_VARIABLE _Inkscape_VERSION ERROR_QUIET)
STRING(REGEX MATCH "[ \t\r\n][1-9]+\.[0-9]+[ \t\r\n]|[ \t\r\n][1-9]+\.[0-9]+\.[0-9]+[ \t\r\n]" _Inkscape_VERSION ${_Inkscape_VERSION})
STRING(STRIP "${_Inkscape_VERSION}" _Inkscape_VERSION)

set(Inkscape_VERSION ${_Inkscape_VERSION} CACHE STRING "Inkscape Version")

execute_process(COMMAND ${Inkscape_EXECUTABLE} "--help" OUTPUT_VARIABLE _Inkscape_HELP ERROR_QUIET)

if(_Inkscape_HELP MATCHES "--without-gui")
	set(Inkscape_GUI "--without-gui" CACHE STRING "Inkscape option to disable the GUI if needed")
endif()

if(NOT DEFINED Inkscape_EXPORT)
	foreach(option IN ITEMS "--export-filename=" "--export-file=" "--export-png=")
		if(_Inkscape_HELP MATCHES "${option}")
			set(Inkscape_EXPORT "${option}" CACHE STRING "Inkscape option to specify the export filename")
			break()
		endif()
	endforeach()
	if(NOT DEFINED Inkscape_EXPORT)
		message(WARNING "Could not determine Inkscape export file option, assuming -o")
		set(Inkscape_EXPORT "-o " CACHE STRING "Inkscape option to specify the export filename")
	endif()
endif()

# handle the QUIETLY and REQUIRED arguments and set Inkscape_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Inkscape REQUIRED_VARS Inkscape_EXECUTABLE)
