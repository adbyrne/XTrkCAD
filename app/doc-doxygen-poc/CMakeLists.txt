# Halibut -> Doxygen user-guide proof-of-concept, built side by side with the
# real halibut-based helpsystem target (see ../doc/CMakeLists.txt). Deliberately
# NOT added to the ALL target, NOT installed -- opt-in only via
# `cmake --build <dir> --target help-doxygen-poc` / `help-doxygen-full`, for
# the SF feature-requests #219 investigation described in
# .claude/halibut-doxygen-investigation-plan.md.

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

	find_package(Python3 COMPONENTS Interpreter QUIET)

	if(Python3_Interpreter_FOUND)
		set(doc_full_CONVERTED_DIR ${CMAKE_CURRENT_BINARY_DIR}/converted)
		set(doc_full_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/full)
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile-full.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile-full @ONLY)

		# Full corpus, not the hand-picked slice above: runs
		# tools/but-to-doxygen.py across every app/doc/*.but file into the
		# build tree (never the source tree -- nothing here is committed),
		# then builds the result with Doxygen. Re-runs unconditionally on
		# every invocation (rm -rf + regenerate) since this is a whole-corpus
		# batch conversion, not an incremental per-file build.
		add_custom_target(help-doxygen-full
			COMMAND ${CMAKE_COMMAND} -E rm -rf ${doc_full_CONVERTED_DIR}
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/../../tools/but-to-doxygen.py
				${CMAKE_CURRENT_SOURCE_DIR}/../doc
				${doc_full_CONVERTED_DIR}
			COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile-full
			COMMENT "Converting all app/doc/*.but files and building the full Doxygen user-guide conversion"
		)
	else()
		message(STATUS "Python3 interpreter not found -- help-doxygen-full target not available")
	endif()
else()
	message(STATUS "Doxygen not found -- help-doxygen-poc/help-doxygen-full targets not available")
endif()
