# Unit test harness for drawcairo.c (cmocka, real Cairo)
#
# Usage (examples, Linux):
#   cmake -S drawcairo_tests -B build-dc -DDRAWCAIRO_SRC=/path/app/wlib/gtk3lib/drawcairo.c
#   cmake -S drawcairo_tests -B build-dc -DXTRKCAD_ROOT=/path/to/xtrkcad
#   cmake --build build-dc
#   ctest --test-dir build-dc --output-on-failure
#
# Path logic:
#   * gtk3lib directory (contains gtkint.h) = directory of drawcairo.c
#   * wlib.h is searched in <gtk3lib>/../include, <root>/app/wlib/include,
#     or set explicitly with -DWLIB_INCLUDE_DIR=...
#
# Principle:
#   * drawcairo.c is compiled unchanged.
#   * Cairo, GLib, gdk-pixbuf and libgdk-3 are linked for real.
#   * libgtk-3 is deliberately NOT linked: the gtk_widget_* functions and the
#     wlib dependencies are stubbed in stubs.c. GTK headers are used for types only.

cmake_minimum_required(VERSION 3.18)
project(drawcairo_tests C)

set(CMAKE_C_STANDARD 11)

set(DRAWCAIRO_SRC "" CACHE FILEPATH
    "Path to drawcairo.c (empty = automatic search)")
set(XTRKCAD_ROOT "" CACHE PATH
    "Root of the XTrkCad source tree (optional, only used for the search)")
set(WLIB_INCLUDE_DIR "" CACHE PATH
    "Directory containing wlib.h (empty = automatic search)")
set(DRAWCAIRO_EXTRA_INCLUDES "" CACHE STRING
    "Additional include paths (e.g. for config.h), semicolon separated")
# Built on its own, the test uses ASan + UBSan by default. In the XTrkCAD
# tree it follows XTRKCAD_SANITIZE, which instruments the whole tree: an ASan
# build can't run under Valgrind (c-tests-valgrind), and needs the sanitizer
# runtimes (libasan, libubsan) that not every machine has.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(_dc_sanitize_default ON)
else()
  set(_dc_sanitize_default ${XTRKCAD_SANITIZE})
endif()
option(DRAWCAIRO_SANITIZE "Enable ASan + UBSan (GCC/Clang, Linux)" ${_dc_sanitize_default})

# ---- locate drawcairo.c ---------------------------------------------------
if(DRAWCAIRO_SRC)
  set(_dc_src "${DRAWCAIRO_SRC}")
else()
  set(_dc_src "")
  foreach(_cand
      "${XTRKCAD_ROOT}/app/wlib/gtk3lib/drawcairo.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/drawcairo.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/../drawcairo.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/../app/wlib/gtk3lib/drawcairo.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/../../drawcairo.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/../../../app/wlib/gtk3lib/drawcairo.c")
    if(NOT _dc_src AND EXISTS "${_cand}")
      get_filename_component(_dc_src "${_cand}" ABSOLUTE)
    endif()
  endforeach()
endif()
if(NOT _dc_src OR NOT EXISTS "${_dc_src}")
  message(FATAL_ERROR
    "drawcairo.c not found. Please pass -DDRAWCAIRO_SRC=/full/path/drawcairo.c")
endif()
get_filename_component(GTK3LIB_DIR "${_dc_src}" DIRECTORY)
if(NOT EXISTS "${GTK3LIB_DIR}/gtkint.h")
  message(FATAL_ERROR "gtkint.h is not next to drawcairo.c (${GTK3LIB_DIR}).")
endif()

# ---- locate wlib.h --------------------------------------------------------
if(NOT WLIB_INCLUDE_DIR OR NOT EXISTS "${WLIB_INCLUDE_DIR}/wlib.h")
  unset(_wlib_dir CACHE)
  find_path(_wlib_dir wlib.h
    HINTS "${GTK3LIB_DIR}/../include"
          "${GTK3LIB_DIR}/../../include"
          "${XTRKCAD_ROOT}/app/wlib/include"
    NO_DEFAULT_PATH)
  if(NOT _wlib_dir)
    message(FATAL_ERROR
      "wlib.h not found. Please pass -DWLIB_INCLUDE_DIR=/path/to/directory")
  endif()
  set(WLIB_INCLUDE_DIR "${_wlib_dir}")
endif()

message(STATUS "drawcairo.c : ${_dc_src}")
message(STATUS "gtkint.h in : ${GTK3LIB_DIR}")
message(STATUS "wlib.h in   : ${WLIB_INCLUDE_DIR}")

# ---- dependencies ---------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(CAIRO      REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(GLIB       REQUIRED IMPORTED_TARGET glib-2.0 gobject-2.0)
pkg_check_modules(PIXBUF     REQUIRED IMPORTED_TARGET gdk-pixbuf-2.0)
pkg_check_modules(GDK3       REQUIRED IMPORTED_TARGET gdk-3.0)
pkg_check_modules(PANGOCAIRO REQUIRED IMPORTED_TARGET pangocairo)
# Headers only (gtkint.h includes gtk/gtk.h). Do NOT link libgtk.
pkg_check_modules(GTK3       REQUIRED gtk+-3.0)

find_package(cmocka CONFIG QUIET)
if(TARGET cmocka::cmocka)
  set(CMOCKA_TARGET cmocka::cmocka)
else()
  find_library(CMOCKA_LIBRARY cmocka REQUIRED)
  find_path(CMOCKA_INCLUDE_DIR cmocka.h REQUIRED)
endif()

add_executable(test_drawcairo
  test_drawcairo.c
  stubs.c
  stubs_basicdraw.c
  "${_dc_src}")

target_include_directories(test_drawcairo PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  "${GTK3LIB_DIR}"
  "${WLIB_INCLUDE_DIR}"
  ${DRAWCAIRO_EXTRA_INCLUDES})
target_include_directories(test_drawcairo SYSTEM PRIVATE ${GTK3_INCLUDE_DIRS})

# Without this, GTK_WIDGET(sentinel) in drawcairo.c fails its runtime type
# check: the tests use a placeholder pointer instead of a real widget.
target_compile_definitions(test_drawcairo PRIVATE G_DISABLE_CAST_CHECKS)

target_compile_options(test_drawcairo PRIVATE -Wall -Wextra -g -O0)

target_link_libraries(test_drawcairo PRIVATE
  PkgConfig::GDK3 PkgConfig::PIXBUF PkgConfig::PANGOCAIRO
  PkgConfig::CAIRO PkgConfig::GLIB m)
if(CMOCKA_TARGET)
  target_link_libraries(test_drawcairo PRIVATE ${CMOCKA_TARGET})
else()
  target_include_directories(test_drawcairo PRIVATE ${CMOCKA_INCLUDE_DIR})
  target_link_libraries(test_drawcairo PRIVATE ${CMOCKA_LIBRARY})
endif()

if(DRAWCAIRO_SANITIZE)
  # Say so at configure time if the runtimes are missing, instead of a
  # link error at the end of the build.
  include(CheckCSourceCompiles)
  set(CMAKE_REQUIRED_FLAGS "-fsanitize=address,undefined")
  set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address,undefined")
  check_c_source_compiles("int main(void) { return 0; }" DRAWCAIRO_SANITIZE_LINKS)
  unset(CMAKE_REQUIRED_FLAGS)
  unset(CMAKE_REQUIRED_LINK_OPTIONS)
  if(NOT DRAWCAIRO_SANITIZE_LINKS)
    message(FATAL_ERROR "DRAWCAIRO_SANITIZE: ${CMAKE_C_COMPILER_ID} can't link "
      "-fsanitize=address,undefined. Install its sanitizer runtimes (GCC: "
      "libasan and libubsan), use clang, or pass -DDRAWCAIRO_SANITIZE=OFF.")
  endif()
  target_compile_options(test_drawcairo PRIVATE
    -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(test_drawcairo PRIVATE -fsanitize=address,undefined)
endif()

enable_testing()

# Run 1: characterization tests. Must be green. KNOWN_BUG tests are skipped.
# detect_leaks=0: the result of wDrawBitMapCreate cannot be freed from the
# test (struct wDrawBitMap_t is private to drawcairo.c).
add_test(NAME drawcairo COMMAND test_drawcairo)
set_tests_properties(drawcairo PROPERTIES
  ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")

# Run 2: one ctest entry per known bug. Each entry runs only that test (via the
# filter argument) and passes only while cmocka reports exactly that test as
# FAILED, i.e. while the bug is still present. A typo in the filter, a crash
# or a linker problem therefore turns the entry red instead of looking "ok".
# Once a bug is fixed its entry turns red: remove the KNOWN_BUG line from the
# test and delete the id from the list below.
set(KNOWN_BUG_IDS 3 1a 1b 6 7e N6)
if(DRAWCAIRO_SANITIZE)
  # 7c is an out-of-bounds read, only detectable with ASan.
#  list(APPEND KNOWN_BUG_IDS 7c)
endif()

foreach(id IN LISTS KNOWN_BUG_IDS)
  add_test(NAME known_bug_${id}
           COMMAND test_drawcairo "*known_bug_${id}_*")
  if(id STREQUAL "7c")
    set(_re "AddressSanitizer|\\[  FAILED  \\] test_known_bug_7c_")
  else()
    set(_re "\\[  FAILED  \\] test_known_bug_${id}_")
  endif()
  # PASS_REGULAR_EXPRESSION replaces the exit-code check. If the regex does not
  # match with your cmocka version, compare it with the real output of
  # "./test_drawcairo '*known_bug_2_*'".
  set_tests_properties(known_bug_${id} PROPERTIES
    ENVIRONMENT "DRAWCAIRO_KNOWN_BUGS=1;ASAN_OPTIONS=detect_leaks=0"
    PASS_REGULAR_EXPRESSION "${_re}")
endforeach()
