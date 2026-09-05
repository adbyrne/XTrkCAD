project(sourcedoc C)

#
# create source documentation using Doxygen

find_package(Doxygen
 REQUIRED
)

SET( XTrkCAD_WLIB_DIR "gtk3lib" )

# Doxygen Awesome CSS theme (https://github.com/jothepro/doxygen-awesome-css),
# MIT licensed. Pinned to the commit the v2.4.2 tag pointed to at fetch time
# (d52eafe3e9303399fda15661f3d7bb8fe3d7eabc) rather than the moving tag name,
# for the same reproducibility reason Doxygen/AStyle are version-pinned in CI
# -- a byte-identical fetch regardless of what the tag points to later.
include(FetchContent)
FetchContent_Declare(
 doxygen-awesome-css
 GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css.git
 GIT_TAG d52eafe3e9303399fda15661f3d7bb8fe3d7eabc # v2.4.2
)
FetchContent_MakeAvailable(doxygen-awesome-css)
SET( DOXYGEN_AWESOME_CSS_DIR "${doxygen-awesome-css_SOURCE_DIR}" )

# Where the dev guide's "User Guide" nav tab points -- a relative link that
# resolves correctly for a normal local build (this target plus a sibling
# app/doc help-html build under the same build tree). A CI job that deploys
# this documentation somewhere the two guides aren't laid out this way (e.g.
# release.yml's GitHub Pages job) overrides this at configure time with the
# real absolute URL for that deployment -- computed from that job's own
# context, never a hardcoded site/account.
if(NOT DEFINED XTRKCAD_USERGUIDE_URL)
    set(XTRKCAD_USERGUIDE_URL "../../app/doc/html/" CACHE STRING
        "URL the dev guide's User Guide tab links to")
endif()
configure_file(${sourcedoc_SOURCE_DIR}/DoxygenLayout.xml.in ${sourcedoc_BINARY_DIR}/DoxygenLayout.xml @ONLY)

configure_file(${sourcedoc_SOURCE_DIR}/Doxyfile.in ${sourcedoc_BINARY_DIR}/Doxyfile)

add_custom_target(docs-doxygen
 COMMAND ${DOXYGEN_EXECUTABLE} ${sourcedoc_BINARY_DIR}/Doxyfile
 )
