# Halibut -> Doxygen user-guide proof-of-concept, built side by side with the
# real halibut-based helpsystem target (see ../doc/CMakeLists.txt). Deliberately
# NOT added to the ALL target, NOT installed -- opt-in only via
# `cmake --build <dir> --target help-doxygen-poc`, for the SF #220-adjacent
# investigation described in .claude/halibut-doxygen-investigation-plan.md.

project(doc-doxygen-poc C)

find_package(Doxygen QUIET)

if(DOXYGEN_FOUND)
	set(doc_poc_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

	add_custom_target(help-doxygen-poc
		COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMENT "Building Halibut->Doxygen user-guide proof-of-concept"
	)
else()
	message(STATUS "Doxygen not found -- help-doxygen-poc target not available")
endif()
