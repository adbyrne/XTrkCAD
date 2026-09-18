# libFuzzer harnesses. Clang-only (libFuzzer is an LLVM sanitizer feature);
# only built when -DXTRKCAD_FUZZ=ON is passed explicitly.
#
# Targets compile getargs.c directly (not linked against xtrkcad-lib) so
# each harness only pulls in the specific parser function under test and
# its few real dependencies, not the whole application's GTK/undo/archive
# dependency graph -- see fuzz_getargs.c's file comment for why.

if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
    message(STATUS "XTRKCAD_FUZZ requested but compiler is not Clang -- skipping fuzz targets")
    return()
endif()

set(FUZZ_SANITIZE_FLAGS -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)

add_executable(fuzz_getargs
    fuzz_getargs.c
    ../getargs.c
)

target_include_directories(fuzz_getargs PRIVATE
    $<TARGET_PROPERTY:xtrkcad-lib,INCLUDE_DIRECTORIES>
)

target_compile_options(fuzz_getargs PRIVATE ${FUZZ_SANITIZE_FLAGS})
target_link_options(fuzz_getargs PRIVATE ${FUZZ_SANITIZE_FLAGS})
