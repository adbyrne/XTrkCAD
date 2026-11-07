# GUI demo-playback regression tests (RegressionTestAll() / the -T flag, see
# macro.c and GTK3 issue #26). Included from app/bin/CMakeLists.txt, guarded by
# XTRKCAD_REGRESSION_TESTING (top-level CMakeLists.txt).
#
# wGetAppLibDir() (app/wlib/gtk3lib/unix/ixpaths.c) locates demos/params/icons
# by searching relative to the running binary's own directory (../share/<app>)
# among other options. XTRKCAD_TESTING's build tree alone doesn't satisfy that
# search -- compiled resources such as icons16.gresource only exist in the
# *installed* layout, confirmed empirically during the GTK3 issue #26
# investigation -- so a RegressionInstall fixture stages a real `cmake --install`
# tree once per ctest run and every regression test requires it.

set(XTRKCAD_REGRESSION_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/regression-install")

# The option character is / on Windows (win32/getopt.c), and the Windows build
# logs to a file, so a test there can't look for a message in the output.
if(WIN32)
    set(_xtrkcad_test_option "/T")
else()
    set(_xtrkcad_test_option "-T")
endif()

add_test(NAME RegressionInstall
    COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --prefix ${XTRKCAD_REGRESSION_INSTALL_PREFIX}
)
set_tests_properties(RegressionInstall PROPERTIES
    FIXTURES_SETUP regression_install
    LABELS "regression"
    RUN_SERIAL TRUE
)

set(_xtrkcad_regression_exe "${XTRKCAD_REGRESSION_INSTALL_PREFIX}/${XTRKCAD_BIN_INSTALL_DIR}/${XTRKCAD_BIN}")

# Every regression test gets its own scratch HOME. XTrkCAD persists state to
# $HOME/.xtrkcad/xtrkcad.rc (a benchwork-lumber catalog, an MRU color list, ...)
# that mutates across runs -- two regression tests sharing one real $HOME can
# produce a false FAIL from state left over by an earlier run (confirmed
# empirically during the GTK3 issue #22 investigation: a second same-machine
# run against the real $HOME produced a spurious dmdimlin.xtr Color mismatch
# that vanished under an isolated $HOME). This does NOT by itself make these
# tests safe under `ctest -j N`, though -- see RUN_SERIAL below.
set(XTRKCAD_REGRESSION_HOME_ROOT "${CMAKE_BINARY_DIR}/regression-homes")

# Scan the same DEMO list ReadDemo()/PlaybackSetup() build at runtime from
# xtrkcad.xtq, so this per-demo ctest target list can't drift from the actual
# demo menu / what -T's own name-matching accepts. Done here, ahead of
# RegressionCleanHomes below, so that test's recreate list can include every
# demo's home directory.
file(STRINGS "${CMAKE_SOURCE_DIR}/app/lib/xtrkcad.xtq" _xtrkcad_demo_lines REGEX "^DEMO ")
set(_xtrkcad_demo_names "")
foreach(_xtrkcad_demo_line ${_xtrkcad_demo_lines})
    string(REGEX REPLACE "^DEMO \"[^\"]*\" +([^ ]+\\.xtr).*$" "\\1" _xtrkcad_demo_file "${_xtrkcad_demo_line}")
    list(APPEND _xtrkcad_demo_names "${_xtrkcad_demo_file}")
endforeach()

set(_xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/RegressionSuite")
list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/export")
# One round-trip test per example layout (Development Issues #67)
file(GLOB _xtrkcad_example_layouts "${CMAKE_SOURCE_DIR}/app/lib/examples/*.xtc")
foreach(_xtrkcad_example ${_xtrkcad_example_layouts})
    get_filename_component(_xtrkcad_example_name "${_xtrkcad_example}" NAME_WE)
    string(MAKE_C_IDENTIFIER "${_xtrkcad_example_name}" _xtrkcad_example_id)
    list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/roundtrip-${_xtrkcad_example_id}")
endforeach()
list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/roundtrip-empty-text")
list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/roundtrip-objects")
list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/notice-badline")
list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/notice-newer")
list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/archive-badmanifest")
list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/notice-frozen")
list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/notice-diskfull")
foreach(_xtrkcad_compare_case match segment extra)
    list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/compare-${_xtrkcad_compare_case}")
endforeach()
foreach(_xtrkcad_paramcheck_case block wrong undo props longname longkeep)
    list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/paramcheck-${_xtrkcad_paramcheck_case}")
endforeach()
foreach(_xtrkcad_demo ${_xtrkcad_demo_names})
    list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/${_xtrkcad_demo}")
endforeach()

# _xtrkcad_regression_home_dirs is a CMake list (semicolon-separated
# internally) -- embedding it directly in the quoted sh -c string below would
# leave those semicolons literal, e.g. `mkdir -p /a;/b;/c`, which sh parses as
# three separate commands (`mkdir -p /a`, then tries to *run* `/b`, then
# `/c`), not one mkdir with three arguments. Build a properly quoted,
# space-separated argument string instead.
set(_xtrkcad_regression_mkdir_args "")
foreach(_xtrkcad_dir ${_xtrkcad_regression_home_dirs})
    string(APPEND _xtrkcad_regression_mkdir_args " '${_xtrkcad_dir}'")
endforeach()

# Wipe every demo's scratch $HOME at the start of each ctest invocation, not
# just once at CMake configure time. Without this, a process that's killed
# uncleanly mid-run (e.g. by ctest's own TIMEOUT, which sends a hard kill
# signal with no chance for the app to clean up its own state) can leave
# behind a stale checkpoint file. XTrkCAD's own startup logic (OfferCheckpoint,
# misc.c) then treats that as "program was not terminated properly" and pops
# a blocking modal resume/discard dialog, which hangs forever under a headless
# regression run with no one to click it -- and since the scratch HOME persists
# across ctest invocations, that one bad kill poisons every subsequent run of
# the same demo indefinitely, looking exactly like a real, reproducible
# regression. Confirmed as the actual (non-)cause of an apparent hang
# "regression" that briefly looked like it was introduced by the GTK3 issue
# #24 fix (PR #75) -- the fix was fine, a prior timeout's leftover checkpoint
# file was not.
#
# The rm -rf MUST be immediately followed by recreating every directory it
# just removed: wGetAppWorkDir() (app/wlib/gtk3lib/unix/ixpaths.c) creates
# only $HOME/.xtrkcad via a plain, non-recursive g_mkdir() -- it never creates
# $HOME itself. Leaving $HOME missing makes that mkdir fail with ENOENT, which
# XTrkCAD treats as fatal and reports via a blocking modal dialog with no one
# to click it under Xvfb -- indistinguishable from a hang. Confirmed
# empirically (PR #76): this exact gap, introduced by an earlier version of
# this fix that only did the rm -rf, hung RegressionSuite for its full 900s
# TIMEOUT and hung Regression.dmintro.xtr (the first standalone test to run)
# until the CI job was killed. sh -c is used rather than a second ctest test
# because these two steps must run as one atomic unit ahead of every
# regression test -- Linux/Xvfb-only, matching the rest of this file.
add_test(NAME RegressionCleanHomes
    COMMAND sh -c "rm -rf '${XTRKCAD_REGRESSION_HOME_ROOT}' && mkdir -p${_xtrkcad_regression_mkdir_args}"
)
set_tests_properties(RegressionCleanHomes PROPERTIES
    FIXTURES_SETUP regression_clean_homes
    LABELS "regression"
    RUN_SERIAL TRUE
)

# RUN_SERIAL: every regression test below gets this. xvfb-run -a's own
# free-display detection isn't atomic against other concurrent xvfb-run
# instances (confirmed empirically: a batch of the per-demo tests under
# `ctest -j 4` failed 8/48, all cleanly reproducing as passes when rerun
# alone -- a parallel-execution artifact, not real demo bugs, and distinct
# from the $HOME-sharing issue above, which isolated HOMEs already fixed).
# RUN_SERIAL keeps regression tests from ever overlapping *each other* while
# still letting `ctest -j N` freely parallelize the cheap non-display unit
# tests elsewhere in the suite around them.

# Each test wraps itself in its own xvfb-run instance (rather than relying on a
# job-level `xvfb-run ctest ...` wrapper) so per-demo tests don't collide over a
# single display when ctest runs them. Falls back to the inherited environment's
# DISPLAY when xvfb-run isn't available (matches XTRKCAD_REGRESSION_TESTING's own
# auto-detect default).
if(XVFB_RUN_EXECUTABLE)
    set(_xtrkcad_regression_wrapper ${XVFB_RUN_EXECUTABLE} -a --)
else()
    set(_xtrkcad_regression_wrapper "")
endif()

# Shell-quote a CMake list into a single space-separated argument string, e.g.
# ("/usr/bin/xvfb-run" "-a" "--") -> " '/usr/bin/xvfb-run' '-a' '--'". Used to
# fold a variable number of leading wrapper args into one sh -c COMMAND
# string below (see xtrkcad_add_regression_test).
function(_xtrkcad_shell_quote_list out_var)
    set(_result "")
    foreach(_item ${ARGN})
        string(APPEND _result " '${_item}'")
    endforeach()
    set(${out_var} "${_result}" PARENT_SCOPE)
endfunction()

_xtrkcad_shell_quote_list(_xtrkcad_wrapper_sh ${_xtrkcad_regression_wrapper})

# xvfb-run only overrides DISPLAY -- it leaves WAYLAND_DISPLAY untouched
# (confirmed empirically). On a Wayland desktop, GTK3's backend auto-detect
# then prefers Wayland over the X11 DISPLAY xvfb-run just set up, so the
# wrapped xtrkcad connects straight to the *real* compositor instead of the
# isolated Xvfb display -- real windows opening/closing and stealing focus
# on the developer's actual desktop. Forcing GDK_BACKEND=x11 fixes it, but
# only do so when WAYLAND_DISPLAY is actually set at test-run time: this
# whole suite already requires xvfb-run or a real DISPLAY to even be
# enabled (see XTRKCAD_REGRESSION_TESTING above), so assuming the X11 GDK
# backend exists once we're in that branch isn't a new assumption -- and
# a pure-X11 session or CI runner (no WAYLAND_DISPLAY) sees no change at
# all.
set(_xtrkcad_wayland_guard "if [ -n \"$WAYLAND_DISPLAY\" ]; then export GDK_BACKEND=x11; fi; ")
if(WIN32)
    # Keep MSYS from rewriting /T as a path
    string(APPEND _xtrkcad_wayland_guard "export MSYS2_ARG_CONV_EXCL='*'; ")
endif()

# On a REGRESSION FAIL, CheckRegressionResult() (app/bin/track.c) appends the
# full actual-vs-expected track dump to $HOME/.<XTRKCAD_BIN>/xtrkcad.regress
# (confirmed empirically -- e.g. RegressionSuite's own scratch HOME held
# .xtrkcad-5.4.0.<hash>/ after a passing run) -- real diagnostic detail well
# beyond the one-line "FAIL: ..." message that already reaches stdout. That
# file would otherwise just sit in a directory nobody looks at, and once
# RegressionCleanHomes wipes it ahead of the *next* test, it's gone for good.
# Every regression test's COMMAND is wrapped in sh -c so that, on a nonzero
# exit, this dump happens before ctest's own captured output ends -- visible
# directly in `ctest --output-on-failure`/`-VV` and in the CI log.
#
# Windows ignores $HOME here: wGetAppWorkDir() (win32/w32paths.c) takes the work
# directory from xtrkcad0.ini in the install's share directory, otherwise from
# the user's AppData. So each test writes that file first, which is safe because
# they all run serially, and gets the same directory as elsewhere. The log goes
# to a file in that directory too, so a failure shows it as well.
function(xtrkcad_add_regression_test _name _home)
    set(_work_dir "${_home}/.${XTRKCAD_BIN}")
    set(_regress_file "${_work_dir}/xtrkcad.regress")
    set(_work_dir_setup "")
    set(_log_dump "")
    if(WIN32)
        set(_work_dir_ini "${XTRKCAD_REGRESSION_INSTALL_PREFIX}/${XTRKCAD_SHARE_INSTALL_DIR}/xtrkcad0.ini")
        set(_work_dir_setup "mkdir -p '${_work_dir}' && printf '[workdir]\\npath=%s\\n' '${_work_dir}' > '${_work_dir_ini}'; ")
        set(_log_dump " if [ $code -ne 0 ]; then echo \"--- exit code $code ---\"; if [ -f '${_work_dir}/xtclog.txt' ]; then echo '--- xtclog.txt ---'; tail -n 100 '${_work_dir}/xtclog.txt'; fi; fi;")
    endif()
    _xtrkcad_shell_quote_list(_args_sh ${ARGN})
    add_test(NAME "${_name}"
        COMMAND sh -c "${_xtrkcad_wayland_guard}${_work_dir_setup}${_xtrkcad_wrapper_sh} '${_xtrkcad_regression_exe}'${_args_sh}; code=$?; if [ $code -ne 0 ] && [ -f '${_regress_file}' ]; then echo '--- xtrkcad.regress (actual vs expected tracks) ---'; cat '${_regress_file}'; fi;${_log_dump} exit $code"
    )
endfunction()

foreach(_xtrkcad_demo ${_xtrkcad_demo_names})
    set(_xtrkcad_demo_home "${XTRKCAD_REGRESSION_HOME_ROOT}/${_xtrkcad_demo}")
    xtrkcad_add_regression_test("Regression.${_xtrkcad_demo}" "${_xtrkcad_demo_home}" "${_xtrkcad_test_option}${_xtrkcad_demo}")
    # Windows runs these instead of RegressionSuite, see below
    if(WIN32)
        set(_xtrkcad_demo_labels "regression;regression-ci")
        set(_xtrkcad_demo_timeout 300)
    else()
        set(_xtrkcad_demo_labels "regression")
        set(_xtrkcad_demo_timeout 120)
    endif()
    set_tests_properties("Regression.${_xtrkcad_demo}" PROPERTIES
        FIXTURES_REQUIRED "regression_install;regression_clean_homes"
        LABELS "${_xtrkcad_demo_labels}"
        TIMEOUT ${_xtrkcad_demo_timeout}
        ENVIRONMENT "HOME=${_xtrkcad_demo_home}"
        RUN_SERIAL TRUE
    )
endforeach()

# What CI actually gates on: one process, one X session, matching the full -T
# suite's ~535s baseline from the GTK3 issue #26 investigation. The per-demo
# tests above are for local dev targeting (`ctest -R Regression.dmease.xtr`) --
# running all ~47 of them individually would multiply process/X-session startup
# overhead far past the single-process run.
#
# Not on Windows, where the whole suite doesn't finish within its 900 s and a
# timeout doesn't show which demo was running (Development Issues #69). There
# the per-demo tests above are in regression-ci instead.
#
# TEMPORARY exclusions, so CI can check the rest of the suite while these are
# investigated. Remove each one once it's fixed, don't just leave it in place:
# - dmjnss.xtr, arm64: the "join straights cornu" check fails deterministically
#   on Release builds (~0.1 degree cornu-join angle divergence). Confirmed NOT an
#   FMA-contraction codegen issue (forcing -ffp-contract=off on the cornu library
#   had zero effect on the failing job), root cause still unknown (Development
#   Issues #28).
if(NOT WIN32)
    set(_xtrkcad_suite_demos ${_xtrkcad_demo_names})
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        list(REMOVE_ITEM _xtrkcad_suite_demos "dmjnss.xtr")
    endif()
    if(_xtrkcad_suite_demos STREQUAL _xtrkcad_demo_names)
        set(_xtrkcad_regression_suite_command ${_xtrkcad_test_option})
    else()
        list(JOIN _xtrkcad_suite_demos "," _xtrkcad_regression_suite_arg)
        set(_xtrkcad_regression_suite_command ${_xtrkcad_test_option}${_xtrkcad_regression_suite_arg})
    endif()

    set(_xtrkcad_regression_suite_home "${XTRKCAD_REGRESSION_HOME_ROOT}/RegressionSuite")
    xtrkcad_add_regression_test(RegressionSuite "${_xtrkcad_regression_suite_home}" ${_xtrkcad_regression_suite_command})
    set_tests_properties(RegressionSuite PROPERTIES
        FIXTURES_REQUIRED "regression_install;regression_clean_homes"
        LABELS "regression;regression-ci"
        TIMEOUT 900
        ENVIRONMENT "HOME=${_xtrkcad_regression_suite_home}"
        RUN_SERIAL TRUE
    )
endif()

# Tests of the regression check itself (Development Issues #66). It compared
# turnout and structure segments with themselves for years without anything
# noticing, so these make sure a difference really does fail: one case that has
# to match, and two that have to be reported. Those two pass on the expected
# message, not on the exit code, so a crash doesn't count as detecting anything.
set(_xtrkcad_compare_dir "${CMAKE_CURRENT_LIST_DIR}/unittest/testfiles")
set(_xtrkcad_compare_segment_regex "FAIL: Compare T:1 .*SEG:1 - Pos.1.")
set(_xtrkcad_compare_extra_regex "FAIL: Track count: Actual 2, Expected 1")
foreach(_xtrkcad_compare_case match segment extra)
    set(_xtrkcad_compare_home "${XTRKCAD_REGRESSION_HOME_ROOT}/compare-${_xtrkcad_compare_case}")
    xtrkcad_add_regression_test("RegressionCompare.${_xtrkcad_compare_case}" "${_xtrkcad_compare_home}"
        "${_xtrkcad_test_option}${_xtrkcad_compare_dir}/regress-compare-${_xtrkcad_compare_case}.xtr")
    set_tests_properties("RegressionCompare.${_xtrkcad_compare_case}" PROPERTIES
        FIXTURES_REQUIRED "regression_install;regression_clean_homes"
        LABELS "regression;regression-ci"
        TIMEOUT 120
        ENVIRONMENT "HOME=${_xtrkcad_compare_home}"
        RUN_SERIAL TRUE
    )
    if(NOT _xtrkcad_compare_case STREQUAL "match")
        if(WIN32)
            set_tests_properties("RegressionCompare.${_xtrkcad_compare_case}" PROPERTIES WILL_FAIL TRUE)
        else()
            set_tests_properties("RegressionCompare.${_xtrkcad_compare_case}" PROPERTIES
                PASS_REGULAR_EXPRESSION "${_xtrkcad_compare_${_xtrkcad_compare_case}_regex}")
        endif()
    endif()
endforeach()

# Field values in a dialog, through PARAMCHECK. "wrong" has to fail, so a check
# that reports nothing can't pass as a good result. It passes on the message, not
# the exit code, as the comparator tests above do.
foreach(_xtrkcad_paramcheck_case block wrong undo props longname longkeep)
    set(_xtrkcad_paramcheck_home "${XTRKCAD_REGRESSION_HOME_ROOT}/paramcheck-${_xtrkcad_paramcheck_case}")
    xtrkcad_add_regression_test("RegressionParamCheck.${_xtrkcad_paramcheck_case}" "${_xtrkcad_paramcheck_home}"
        "${_xtrkcad_test_option}${_xtrkcad_compare_dir}/regress-paramcheck-${_xtrkcad_paramcheck_case}.xtr")
    set_tests_properties("RegressionParamCheck.${_xtrkcad_paramcheck_case}" PROPERTIES
        FIXTURES_REQUIRED "regression_install;regression_clean_homes"
        LABELS "regression;regression-ci"
        TIMEOUT 120
        ENVIRONMENT "HOME=${_xtrkcad_paramcheck_home}"
        RUN_SERIAL TRUE
    )
endforeach()
if(WIN32)
    set_tests_properties(RegressionParamCheck.wrong PROPERTIES WILL_FAIL TRUE)
else()
    set_tests_properties(RegressionParamCheck.wrong PROPERTIES
        PASS_REGULAR_EXPRESSION "FAIL: PARAMCHECK .*exp: 99.000, act=20.000")
endif()

# Load each example layout, save it, reload it and save again, in both .xtc and
# .xtce (LayoutRoundTripTest(), fileio.c). Nothing else exercises the real load
# and save paths. regression-ci, so the sanitizer job runs these too.
foreach(_xtrkcad_example ${_xtrkcad_example_layouts})
    get_filename_component(_xtrkcad_example_name "${_xtrkcad_example}" NAME_WE)
    string(MAKE_C_IDENTIFIER "${_xtrkcad_example_name}" _xtrkcad_example_id)
    set(_xtrkcad_roundtrip_home "${XTRKCAD_REGRESSION_HOME_ROOT}/roundtrip-${_xtrkcad_example_id}")
    xtrkcad_add_regression_test("RegressionRoundTrip.${_xtrkcad_example_id}" "${_xtrkcad_roundtrip_home}"
        "${_xtrkcad_test_option}${_xtrkcad_example}")
    set_tests_properties("RegressionRoundTrip.${_xtrkcad_example_id}" PROPERTIES
        FIXTURES_REQUIRED "regression_install;regression_clean_homes"
        LABELS "regression;regression-ci"
        TIMEOUT 120
        ENVIRONMENT "HOME=${_xtrkcad_roundtrip_home}"
        RUN_SERIAL TRUE
    )
endforeach()

# Empty text segments, alone and after a converted text in the same object:
# saving them crashed on Windows, and could write the wrong text instead
# (Development Issues #73). The check compares the saved texts with the source.
set(_xtrkcad_empty_text_home "${XTRKCAD_REGRESSION_HOME_ROOT}/roundtrip-empty-text")
set(_xtrkcad_empty_text_source "${CMAKE_CURRENT_LIST_DIR}/unittest/testfiles/roundtrip-empty-text.xtc")
xtrkcad_add_regression_test(RegressionRoundTrip.empty_text "${_xtrkcad_empty_text_home}"
    "${_xtrkcad_test_option}${_xtrkcad_empty_text_source}")
set_tests_properties(RegressionRoundTrip.empty_text PROPERTIES
    FIXTURES_REQUIRED "regression_install;regression_clean_homes"
    FIXTURES_SETUP regression_empty_text
    LABELS "regression;regression-ci"
    TIMEOUT 120
    ENVIRONMENT "HOME=${_xtrkcad_empty_text_home}"
    RUN_SERIAL TRUE
)
add_test(NAME RegressionRoundTrip.empty_text_check
    COMMAND sh "${CMAKE_SOURCE_DIR}/tools/check-roundtrip-text.sh"
        "${_xtrkcad_empty_text_source}"
        "${_xtrkcad_empty_text_home}/.${XTRKCAD_BIN}/roundtrip/roundtrip1.xtc"
)
set_tests_properties(RegressionRoundTrip.empty_text_check PROPERTIES
    FIXTURES_REQUIRED regression_empty_text
    LABELS "regression;regression-ci"
)

# Object types no example or demo contains: Bezier, sensor, control, switchmotor,
# block, signal (Development Issues #75). The check compares them with the source.
set(_xtrkcad_objects_home "${XTRKCAD_REGRESSION_HOME_ROOT}/roundtrip-objects")
set(_xtrkcad_objects_source "${CMAKE_CURRENT_LIST_DIR}/unittest/testfiles/roundtrip-objects.xtc")
xtrkcad_add_regression_test(RegressionRoundTrip.objects "${_xtrkcad_objects_home}"
    "${_xtrkcad_test_option}${_xtrkcad_objects_source}")
set_tests_properties(RegressionRoundTrip.objects PROPERTIES
    FIXTURES_REQUIRED "regression_install;regression_clean_homes"
    FIXTURES_SETUP regression_objects
    LABELS "regression;regression-ci"
    TIMEOUT 120
    ENVIRONMENT "HOME=${_xtrkcad_objects_home}"
    RUN_SERIAL TRUE
)
add_test(NAME RegressionRoundTrip.objects_check
    COMMAND sh "${CMAKE_SOURCE_DIR}/tools/check-roundtrip-objects.sh"
        "${_xtrkcad_objects_source}"
        "${_xtrkcad_objects_home}/.${XTRKCAD_BIN}/roundtrip/roundtrip1.xtc"
)
set_tests_properties(RegressionRoundTrip.objects_check PROPERTIES
    FIXTURES_REQUIRED regression_objects
    LABELS "regression;regression-ci"
)

# What the user is told when a layout has bad lines, and what survives
# (Development Issues #75). Under -T the loader's notices go to the log and are
# answered Continue, so the test passes on the message; the check looks at the
# saved file.
set(_xtrkcad_badline_home "${XTRKCAD_REGRESSION_HOME_ROOT}/notice-badline")
set(_xtrkcad_badline_source "${CMAKE_CURRENT_LIST_DIR}/unittest/testfiles/roundtrip-badline.xtc")
xtrkcad_add_regression_test(RegressionNotice.badline "${_xtrkcad_badline_home}"
    "${_xtrkcad_test_option}${_xtrkcad_badline_source}")
set_tests_properties(RegressionNotice.badline PROPERTIES
    FIXTURES_REQUIRED "regression_install;regression_clean_homes"
    FIXTURES_SETUP regression_badline
    LABELS "regression;regression-ci"
    TIMEOUT 120
    ENVIRONMENT "HOME=${_xtrkcad_badline_home}"
    RUN_SERIAL TRUE
)
if(NOT WIN32)
    set_tests_properties(RegressionNotice.badline PROPERTIES
        PASS_REGULAR_EXPRESSION "NOTICE: While processing Layout file roundtrip-badline.xtc, 4 lines were skipped")
endif()
# A file from a newer version: the user is told to upgrade, and the file must
# not become the current layout, or Save would write the empty layout over it.
set(_xtrkcad_newer_home "${XTRKCAD_REGRESSION_HOME_ROOT}/notice-newer")
xtrkcad_add_regression_test(RegressionNotice.newer "${_xtrkcad_newer_home}"
    "${_xtrkcad_test_option}${CMAKE_CURRENT_LIST_DIR}/unittest/testfiles/roundtrip-newer-version.xtc")
set_tests_properties(RegressionNotice.newer PROPERTIES
    FIXTURES_REQUIRED "regression_install;regression_clean_homes"
    LABELS "regression;regression-ci"
    TIMEOUT 120
    ENVIRONMENT "HOME=${_xtrkcad_newer_home}"
    RUN_SERIAL TRUE
)
if(WIN32)
    set_tests_properties(RegressionNotice.newer PROPERTIES WILL_FAIL TRUE)
else()
    set_tests_properties(RegressionNotice.newer PROPERTIES
        PASS_REGULAR_EXPRESSION "NOTICE: File version 99 is greater than supported version"
        FAIL_REGULAR_EXPRESSION "roundtrip-newer-version.xtc: current layout \"[^\"]+\"")
endif()
# Saving onto a full disk: the user must be told, and the save must count as
# failed. Linux only: the save file is pointed at /dev/full, where every write
# fails with "No space left on device".
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_xtrkcad_diskfull_home "${XTRKCAD_REGRESSION_HOME_ROOT}/notice-diskfull")
    set(_xtrkcad_diskfull_dir "${_xtrkcad_diskfull_home}/.${XTRKCAD_BIN}/roundtrip")
    add_test(NAME RegressionNotice.diskfull_setup
        COMMAND sh -c "mkdir -p '${_xtrkcad_diskfull_dir}' && ln -sf /dev/full '${_xtrkcad_diskfull_dir}/roundtrip1.xtc'")
    set_tests_properties(RegressionNotice.diskfull_setup PROPERTIES
        FIXTURES_REQUIRED regression_clean_homes
        FIXTURES_SETUP regression_diskfull
        LABELS "regression;regression-ci"
    )
    xtrkcad_add_regression_test(RegressionNotice.diskfull "${_xtrkcad_diskfull_home}"
        "${_xtrkcad_test_option}${CMAKE_CURRENT_LIST_DIR}/unittest/testfiles/roundtrip-objects.xtc")
    set_tests_properties(RegressionNotice.diskfull PROPERTIES
        FIXTURES_REQUIRED "regression_install;regression_clean_homes;regression_diskfull"
        LABELS "regression;regression-ci"
        TIMEOUT 60
        ENVIRONMENT "HOME=${_xtrkcad_diskfull_home}"
        RUN_SERIAL TRUE
        PASS_REGULAR_EXPRESSION "NOTICE: Error \"No space left on device\" occurred while writing"
    )
endif()
# An archive whose manifest can't be read: the layout inside, named like the
# archive, still has to open (Development Issues #75).
set(_xtrkcad_badmanifest_home "${XTRKCAD_REGRESSION_HOME_ROOT}/archive-badmanifest")
xtrkcad_add_regression_test(RegressionRoundTrip.badmanifest "${_xtrkcad_badmanifest_home}"
    "${_xtrkcad_test_option}${CMAKE_CURRENT_LIST_DIR}/unittest/testfiles/archive-badmanifest.xtce")
set_tests_properties(RegressionRoundTrip.badmanifest PROPERTIES
    FIXTURES_REQUIRED "regression_install;regression_clean_homes"
    LABELS "regression;regression-ci"
    TIMEOUT 120
    ENVIRONMENT "HOME=${_xtrkcad_badmanifest_home}"
    RUN_SERIAL TRUE
)
set(_xtrkcad_frozen_home "${XTRKCAD_REGRESSION_HOME_ROOT}/notice-frozen")
xtrkcad_add_regression_test(RegressionNotice.frozen "${_xtrkcad_frozen_home}"
    "${_xtrkcad_test_option}${_xtrkcad_compare_dir}/regress-notice-frozen.xtr")
set_tests_properties(RegressionNotice.frozen PROPERTIES
    FIXTURES_REQUIRED "regression_install;regression_clean_homes"
    LABELS "regression;regression-ci"
    TIMEOUT 120
    ENVIRONMENT "HOME=${_xtrkcad_frozen_home}"
    RUN_SERIAL TRUE
)
if(WIN32)
    set_tests_properties(RegressionNotice.frozen PROPERTIES WILL_FAIL TRUE)
else()
    set_tests_properties(RegressionNotice.frozen PROPERTIES
        PASS_REGULAR_EXPRESSION "no matching dialog: Describe Sensor 1 S1"
        FAIL_REGULAR_EXPRESSION "Describe Sensor 4|Describe Sensor-4")
endif()
add_test(NAME RegressionNotice.badline_check
    COMMAND sh "${CMAKE_SOURCE_DIR}/tools/check-roundtrip-badline.sh"
        "${_xtrkcad_badline_home}/.${XTRKCAD_BIN}/roundtrip/roundtrip1.xtc"
)
set_tests_properties(RegressionNotice.badline_check PROPERTIES
    FIXTURES_REQUIRED regression_badline
    LABELS "regression;regression-ci"
)

# Print and export a small layout through the normal code, then check the files
# (Development Issues #68). The check needs poppler-utils, which isn't something
# every package job should have to install, so these aren't in regression-ci;
# regression-gtk3-standalone runs them.
find_program(XTRKCAD_PDFTOHTML_EXECUTABLE pdftohtml)
find_program(XTRKCAD_PDFINFO_EXECUTABLE pdfinfo)
if(XTRKCAD_PDFTOHTML_EXECUTABLE AND XTRKCAD_PDFINFO_EXECUTABLE)
    set(_xtrkcad_export_home "${XTRKCAD_REGRESSION_HOME_ROOT}/export")
    xtrkcad_add_regression_test(RegressionExport.run "${_xtrkcad_export_home}"
        "${_xtrkcad_test_option}export:${CMAKE_CURRENT_LIST_DIR}/unittest/testfiles/export-test.xtc")
    set_tests_properties(RegressionExport.run PROPERTIES
        FIXTURES_REQUIRED "regression_install;regression_clean_homes"
        FIXTURES_SETUP regression_export
        LABELS "regression"
        TIMEOUT 120
        ENVIRONMENT "HOME=${_xtrkcad_export_home}"
        RUN_SERIAL TRUE
    )
    add_test(NAME RegressionExport.check
        COMMAND sh "${CMAKE_SOURCE_DIR}/tools/check-export-test.sh"
            "${_xtrkcad_export_home}/.${XTRKCAD_BIN}"
    )
    set_tests_properties(RegressionExport.check PROPERTIES
        FIXTURES_REQUIRED regression_export
        LABELS "regression"
    )
endif()
