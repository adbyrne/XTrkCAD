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

	set(doc_full_CONVERTED_DIR ${CMAKE_CURRENT_BINARY_DIR}/converted)
	set(doc_full_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/full)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile-full.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile-full @ONLY)

	# messages.in's ~130 auto-generated message-explanation topics, already
	# in Doxygen syntax -- unlike the rest of doc_full_CONVERTED_DIR (a
	# one-time hand-reviewed conversion via tools/but-to-doxygen.py, see
	# that script's own comments), this comes from real, always-regenerated
	# C tooling: app/help/genmessages.c, via app/help/CMakeLists.txt's
	# msgfiles ALL target (which writes messages.h + messages.dox into
	# messagefile_BINARY_DIR on every build -- halibut's \S{}/\H{} output
	# was removed from genmessages.c entirely on this branch, not kept
	# side by side). Copied here so Doxyfile-full's FILE_PATTERNS=*.dox
	# picks it up from the same directory as everything else -- see
	# tools/but-to-doxygen.py's CANONICAL_FILE_ORDER for how its root page
	# ("messageList") is spliced into the sidebar between appendix.but and
	# upgrade.but to match halibut's real book order.
	add_custom_command(
		OUTPUT ${doc_full_CONVERTED_DIR}/messages.dox
		DEPENDS ${messagefile_BINARY_DIR}/messages.dox
		COMMAND ${CMAKE_COMMAND} -E make_directory ${doc_full_CONVERTED_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy
			${messagefile_BINARY_DIR}/messages.dox
			${doc_full_CONVERTED_DIR}/messages.dox
		COMMENT "Copying genmessages' Doxygen-syntax messages.dox into the conversion output directory"
	)

	# tools/but-to-doxygen.py is a one-time, EXTERNAL conversion step (run
	# by hand, or as its own CI step before "Configure" -- see release.yml)
	# -- it is deliberately NOT invoked from here. CMake's job is only to
	# build whatever .dox/.dox.in files already exist in doc_full_CONVERTED_DIR
	# with Doxygen; regular builds must not require Python at all. See
	# .claude/halibut-doxygen-investigation-plan.md.
	#
	# The one exception needing real CMake work rather than a plain file
	# copy: intro.but.in is itself a template (CMake substitutes
	# "@XTRKCAD_VERSION@" via configure_file() to produce the real
	# intro.but that halibut compiles -- see app/doc/CMakeLists.txt). The
	# converter preserves that by emitting intro.dox.in (literal token,
	# not resolved -- see write_dox()'s keep_template) instead of
	# intro.dox. Exactly the same configure_file() mechanism resolves it
	# here, at CMake configure time -- this only requires the external
	# conversion step to have already run at least once (checked via
	# EXISTS; on a fresh checkout that hasn't happened yet, this is simply
	# skipped and help-doxygen-full has nothing to build until it has).
	if(EXISTS ${doc_full_CONVERTED_DIR}/intro.dox.in)
		configure_file(${doc_full_CONVERTED_DIR}/intro.dox.in ${doc_full_CONVERTED_DIR}/intro.dox @ONLY)
	endif()

	add_custom_target(help-doxygen-full
		COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile-full
		DEPENDS ${doc_full_CONVERTED_DIR}/messages.dox
		COMMENT "Building the full Doxygen user-guide conversion (run tools/but-to-doxygen.py first)"
	)

	find_package(Python3 COMPONENTS Interpreter QUIET)

	if(Python3_Interpreter_FOUND)
		# compare-userguide-html.py is a dev-review analysis tool, not part
		# of building the docs -- unlike but-to-doxygen.py above, it's fine
		# for this one to run via CMake, same as any other opt-in,
		# workflow_dispatch-gated investigation target.
		#
		# Compares help-doxygen-full's output against the real halibut-built
		# guide (app/doc/CMakeLists.txt's help-html target, built into
		# ${CMAKE_BINARY_DIR}/app/doc/html) page by page, so dev review can
		# focus on pages that actually changed instead of eyeballing all
		# 234+ pages -- see tools/compare-userguide-html.py.
		add_custom_target(help-doxygen-compare
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/../../tools/compare-userguide-html.py
				${CMAKE_BINARY_DIR}/app/doc/html
				${doc_full_BINARY_DIR}/html
				${CMAKE_CURRENT_BINARY_DIR}/comparison-report.md
			COMMENT "Comparing halibut vs. Doxygen-converted user-guide HTML"
		)
		add_dependencies(help-doxygen-compare help-doxygen-full help-html)
	else()
		message(STATUS "Python3 interpreter not found -- help-doxygen-compare target not available")
	endif()
else()
	message(STATUS "Doxygen not found -- help-doxygen-poc/help-doxygen-full/help-doxygen-compare targets not available")
endif()
