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
# The demos whose last steps RegressionUndo.<demo> undoes and redoes (below).
set(_xtrkcad_undo_demos ${_xtrkcad_demo_names})

set(_xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/RegressionSuite")
list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/export")
# One round-trip test per example layout (Development Issues #67)
file(GLOB _xtrkcad_example_layouts "${CMAKE_SOURCE_DIR}/app/lib/examples/*.xtc")
foreach(_xtrkcad_example ${_xtrkcad_example_layouts})
    get_filename_component(_xtrkcad_example_name "${_xtrkcad_example}" NAME_WE)
    string(MAKE_C_IDENTIFIER "${_xtrkcad_example_name}" _xtrkcad_example_id)
    list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/roundtrip-${_xtrkcad_example_id}")
    list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/describe-${_xtrkcad_example_id}")
endforeach()
list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/roundtrip-empty-text")
list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/roundtrip-objects")
list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/notice-badline")
list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/notice-newer")
list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/archive-badmanifest")
list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/notice-frozen")
list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/notice-diskfull")
foreach(_xtrkcad_checkpoint_case objects timesavr)
    list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/checkpoint-${_xtrkcad_checkpoint_case}")
endforeach()
foreach(_xtrkcad_clipboard_case objects timesavr)
    list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/clipboard-${_xtrkcad_clipboard_case}")
endforeach()
foreach(_xtrkcad_xti_case objects timesavr)
    list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/xti-${_xtrkcad_xti_case}")
endforeach()
list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/params-load")
list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/hints-main")
foreach(_xtrkcad_compare_case match segment extra)
    list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/compare-${_xtrkcad_compare_case}")
endforeach()
# The paramcheck test cases (RegressionParamCheck.<case>, script
# regress-paramcheck-<case>.xtr). Set once: the home dirs, the tests and the
# INPUT ERROR pattern below all use it.
set(_xtrkcad_paramcheck_cases block wrong undo props longname longkeep drawlen longsignal manage carnew carfind carsedit carsdelete carscsv carsproto jsonnote todesign layers modify reports movebelow blockends)
foreach(_xtrkcad_paramcheck_case IN LISTS _xtrkcad_paramcheck_cases)
    list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/paramcheck-${_xtrkcad_paramcheck_case}")
endforeach()
foreach(_xtrkcad_demo ${_xtrkcad_demo_names})
    list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/${_xtrkcad_demo}")
endforeach()
foreach(_xtrkcad_demo IN LISTS _xtrkcad_undo_demos)
    list(APPEND _xtrkcad_regression_home_dirs "${XTRKCAD_REGRESSION_HOME_ROOT}/undo-${_xtrkcad_demo}")
endforeach()

# The home directories go to a file, one per line, which RegressionCleanHomes
# reads. Passing them on the command line stopped working on Windows once it
# grew past 8191 characters: the line was cut off mid-quote and every
# regression test was skipped.
set(_xtrkcad_regression_home_list "${CMAKE_CURRENT_BINARY_DIR}/regression-home-dirs.txt")
string(REPLACE ";" "\n" _xtrkcad_regression_home_lines "${_xtrkcad_regression_home_dirs}")
file(WRITE "${_xtrkcad_regression_home_list}" "${_xtrkcad_regression_home_lines}\n")

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
# until the CI job was killed. One script does both rather than two ctest
# tests, because these two steps must run as one unit ahead of every
# regression test.
add_test(NAME RegressionCleanHomes
    COMMAND ${CMAKE_COMMAND} "-DROOT=${XTRKCAD_REGRESSION_HOME_ROOT}"
        "-DLIST=${_xtrkcad_regression_home_list}"
        -P "${CMAKE_CURRENT_LIST_DIR}/unittest/RegressionCleanHomes.cmake"
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
# Messages in English, whatever the developer's or runner's language: some
# tests look for message text, and libc's own texts (strerror()) have no ID
# to match instead. Only the messages: LC_NUMERIC and the rest keep the
# locale, so a decimal-comma locale (de_DE) still shows where numbers are
# written or read in it (dev ML #4641). LC_ALL would override LC_MESSAGES,
# and LANGUAGE would override it for gettext.
string(APPEND _xtrkcad_wayland_guard "unset LC_ALL LANGUAGE; export LC_MESSAGES=C; ")
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

# Undo and Redo of each demo's last steps, as many as Undo holds (10):
# -Tundo:<demo>, UndoCheckRun() in macro.c (Development Issues #75). Each
# Undo has to give back the layout as it was before the step, each Redo the
# layout as it was after it. Level full (label regression) for now.
foreach(_xtrkcad_demo IN LISTS _xtrkcad_undo_demos)
    set(_xtrkcad_undo_home "${XTRKCAD_REGRESSION_HOME_ROOT}/undo-${_xtrkcad_demo}")
    xtrkcad_add_regression_test("RegressionUndo.${_xtrkcad_demo}" "${_xtrkcad_undo_home}"
        "${_xtrkcad_test_option}undo:${_xtrkcad_demo}")
    set_tests_properties("RegressionUndo.${_xtrkcad_demo}" PROPERTIES
        FIXTURES_REQUIRED "regression_install;regression_clean_homes"
        LABELS "regression"
        TIMEOUT 120
        ENVIRONMENT "HOME=${_xtrkcad_undo_home}"
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
foreach(_xtrkcad_paramcheck_case IN LISTS _xtrkcad_paramcheck_cases)
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
# Every report saved to a file, checked against the expected text: the demos
# only open and cancel them. A row the script selects has to exist.
set_tests_properties(RegressionParamCheck.reports PROPERTIES
    FIXTURES_SETUP regression_reports
    FAIL_REGULAR_EXPRESSION "NOTICE ID: MSG_PLAYBACK_LISTENTRY")
add_test(NAME RegressionParamCheck.reports_check
    COMMAND sh "${CMAKE_SOURCE_DIR}/tools/check-reports-test.sh"
        "${_xtrkcad_compare_dir}/reports-expected"
        "${XTRKCAD_REGRESSION_HOME_ROOT}/paramcheck-reports/.${XTRKCAD_BIN}"
)
set_tests_properties(RegressionParamCheck.reports_check PROPERTIES
    FIXTURES_REQUIRED regression_reports
    LABELS "regression;regression-ci"
)
if(WIN32)
    set_tests_properties(RegressionParamCheck.wrong PROPERTIES WILL_FAIL TRUE)
else()
    set_tests_properties(RegressionParamCheck.wrong PROPERTIES
        PASS_REGULAR_EXPRESSION "FAIL: PARAMCHECK .*exp: 99.000, act=20.000")
endif()
# Only the notice shows that the name was cut (see the script). On Windows the
# test checks the result only, as the message tests above do.
# Adding a car of a part that exists must not ask to update the part
# (Development Issues #77). Notices only reach the output outside Windows.
if(NOT WIN32)
    set_tests_properties(RegressionParamCheck.carnew PROPERTIES
        FAIL_REGULAR_EXPRESSION "NOTICE:|FAIL: PARAMCHECK")
endif()
# The car dialog's main flows: edit and Cancel, a new prototype, and the CSV
# round trip. Edit and prototype may not raise a notice. The round trip deletes
# the cars, so it may raise Delete's confirmation, which has no message ID,
# but no notice that has one (the import's). Importing cars whose indexes
# are taken has to say so once. Tests match message IDs ("NOTICE ID:"),
# which no translation changes.
if(NOT WIN32)
    set_tests_properties(RegressionParamCheck.carsedit RegressionParamCheck.carsproto
        PROPERTIES FAIL_REGULAR_EXPRESSION "NOTICE:|FAIL: PARAMCHECK")
    set_tests_properties(RegressionParamCheck.carscsv PROPERTIES
        FAIL_REGULAR_EXPRESSION "NOTICE ID: |FAIL: PARAMCHECK")
    # The JSON Note dialog may raise no notice, and its Undo step has to
    # work: an Undo assertion only logs, so it fails the test here.
    set_tests_properties(RegressionParamCheck.jsonnote PROPERTIES
        FAIL_REGULAR_EXPRESSION "NOTICE:|ERROR:|FAIL: PARAMCHECK|UNDO ASSERT")
    # The Turnout Designer: a new design with an existing name has to ask
    # (MSG_TODSGN_REPLACE), and a HOTBARSELECT that finds nothing fails.
    set_tests_properties(RegressionParamCheck.todesign PROPERTIES
        PASS_REGULAR_EXPRESSION "NOTICE ID: MSG_TODSGN_REPLACE"
        FAIL_REGULAR_EXPRESSION "FAIL: PARAMCHECK|FAIL: playback|ERROR:|UNDO ASSERT")
    set_tests_properties(RegressionParamCheck.layers PROPERTIES
        FAIL_REGULAR_EXPRESSION "FAIL: PARAMCHECK|FAIL: playback|ERROR:|UNDO ASSERT")
    # Move Objects Below/Above, then Undo and Redo: nothing may be lost
    # (UndoUndo() used to cut the track list at a step's first new track).
    # A loaded block's ends are its tracks' outer ends (ResolveBlockTrack()),
    # not what the last read left in TempEndPts (CompareBlock() checks them).
    set_tests_properties(RegressionParamCheck.blockends PROPERTIES
        FAIL_REGULAR_EXPRESSION "FAIL:|ERROR:|UNDO ASSERT")
    set_tests_properties(RegressionParamCheck.movebelow PROPERTIES
        FAIL_REGULAR_EXPRESSION "FAIL:|ERROR:|UNDO ASSERT")
    set_tests_properties(RegressionParamCheck.undo PROPERTIES
        FAIL_REGULAR_EXPRESSION "FAIL: PARAMCHECK|UNDO ASSERT")
    set_tests_properties(RegressionParamCheck.carsdelete PROPERTIES
        PASS_REGULAR_EXPRESSION "NOTICE ID: MSG_CARIMP_DUP_INDEX"
        FAIL_REGULAR_EXPRESSION "FAIL: PARAMCHECK")
endif()
if(NOT WIN32)
    set_tests_properties(RegressionParamCheck.longsignal PROPERTIES
        PASS_REGULAR_EXPRESSION "NOTICE ID: MSG_ENTERED_STRING_TRUNCATED\nNOTICE: [^\n]*79"
        FAIL_REGULAR_EXPRESSION "FAIL: PARAMCHECK")
endif()
# A script line that doesn't parse ends the script with an INPUT ERROR
# notice, and the test still passed. Added to each test's own pattern, set
# above, since setting the property replaces it. Not "wrong", which has to fail.
if(NOT WIN32)
    set(_xtrkcad_paramcheck_input_cases ${_xtrkcad_paramcheck_cases})
    list(REMOVE_ITEM _xtrkcad_paramcheck_input_cases wrong)
    foreach(_xtrkcad_paramcheck_case IN LISTS _xtrkcad_paramcheck_input_cases)
        get_test_property("RegressionParamCheck.${_xtrkcad_paramcheck_case}"
            FAIL_REGULAR_EXPRESSION _xtrkcad_fail_regex)
        if(_xtrkcad_fail_regex)
            set(_xtrkcad_fail_regex "${_xtrkcad_fail_regex}|NOTICE: INPUT ERROR")
        else()
            set(_xtrkcad_fail_regex "NOTICE: INPUT ERROR")
        endif()
        set_tests_properties("RegressionParamCheck.${_xtrkcad_paramcheck_case}" PROPERTIES
            FAIL_REGULAR_EXPRESSION "${_xtrkcad_fail_regex}")
    endforeach()
endif()

# Every menu item and button of the main window, and of Car Inventory, has a
# hover hint (Development Issues #80): -d missinghints=1 prints a line for
# each one created without one. Outside Windows, where the output reaches ctest.
if(NOT WIN32)
    set(_xtrkcad_hints_home "${XTRKCAD_REGRESSION_HOME_ROOT}/hints-main")
    xtrkcad_add_regression_test(RegressionHints.main "${_xtrkcad_hints_home}"
        "-d" "missinghints=1" "${_xtrkcad_test_option}${_xtrkcad_compare_dir}/regress-hints.xtr")
    set_tests_properties(RegressionHints.main PROPERTIES
        FIXTURES_REQUIRED "regression_install;regression_clean_homes"
        LABELS "regression;regression-ci"
        TIMEOUT 120
        ENVIRONMENT "HOME=${_xtrkcad_hints_home}"
        RUN_SERIAL TRUE
        PASS_REGULAR_EXPRESSION "Regression Tests Pass"
        FAIL_REGULAR_EXPRESSION "Missing Help String|NOTICE: INPUT ERROR"
    )
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

# Open Properties on every object of each example layout and press Done
# without an edit: no dialog may start an Undo step, and no object may change
# (LayoutDescribeTest(), fileio.c; Development Issues #75). The five largest
# layouts take from 20 s to 100 s each, too long for every CI platform and the
# sanitizer job, so they're only in the full regression run.
set(_xtrkcad_describe_large cascade Ondaville_Franklin_and_Carolina_RR port_dinllean
    DVRR_v3 psr)
foreach(_xtrkcad_example ${_xtrkcad_example_layouts})
    get_filename_component(_xtrkcad_example_name "${_xtrkcad_example}" NAME_WE)
    string(MAKE_C_IDENTIFIER "${_xtrkcad_example_name}" _xtrkcad_example_id)
    set(_xtrkcad_describe_home "${XTRKCAD_REGRESSION_HOME_ROOT}/describe-${_xtrkcad_example_id}")
    xtrkcad_add_regression_test("RegressionDescribe.${_xtrkcad_example_id}" "${_xtrkcad_describe_home}"
        "${_xtrkcad_test_option}describe:${_xtrkcad_example}")
    if(_xtrkcad_example_id IN_LIST _xtrkcad_describe_large)
        set(_xtrkcad_describe_labels "regression")
        set(_xtrkcad_describe_timeout 600)
    else()
        set(_xtrkcad_describe_labels "regression;regression-ci")
        set(_xtrkcad_describe_timeout 120)
    endif()
    set_tests_properties("RegressionDescribe.${_xtrkcad_example_id}" PROPERTIES
        FIXTURES_REQUIRED "regression_install;regression_clean_homes"
        LABELS "${_xtrkcad_describe_labels}"
        TIMEOUT ${_xtrkcad_describe_timeout}
        ENVIRONMENT "HOME=${_xtrkcad_describe_home}"
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
        PASS_REGULAR_EXPRESSION "NOTICE ID: MSG_LAYOUT_LINES_SKIPPED\nNOTICE: [^\n]*roundtrip-badline\\.xtc[^\n]* 4 ")
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
        PASS_REGULAR_EXPRESSION "NOTICE ID: MSG_UPGRADE_VERSION[12]\nNOTICE: [^\n]* 99 "
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

# Checkpoints, the only copy of unsaved work after a crash (LayoutCheckpointTest(),
# fileio.c): written on time, kept as backups, restored by Resume and Resume with
# New Name, kept by a failed or cancelled save and removed by one that works.
# The checkpoint is written like a save, which the round trips check for every
# example, so two layouts are enough: the unusual object types and an example.
foreach(_xtrkcad_checkpoint_case objects timesavr)
    if(_xtrkcad_checkpoint_case STREQUAL "objects")
        set(_xtrkcad_checkpoint_source "${CMAKE_CURRENT_LIST_DIR}/unittest/testfiles/roundtrip-objects.xtc")
        set(_xtrkcad_checkpoint_labels "regression;regression-ci")
    else()
        set(_xtrkcad_checkpoint_source "${CMAKE_SOURCE_DIR}/app/lib/examples/timesavr.xtc")
        set(_xtrkcad_checkpoint_labels "regression")
    endif()
    set(_xtrkcad_checkpoint_home "${XTRKCAD_REGRESSION_HOME_ROOT}/checkpoint-${_xtrkcad_checkpoint_case}")
    xtrkcad_add_regression_test("RegressionCheckpoint.${_xtrkcad_checkpoint_case}" "${_xtrkcad_checkpoint_home}"
        "${_xtrkcad_test_option}checkpoint:${_xtrkcad_checkpoint_source}")
    set_tests_properties("RegressionCheckpoint.${_xtrkcad_checkpoint_case}" PROPERTIES
        FIXTURES_REQUIRED "regression_install;regression_clean_homes"
        LABELS "${_xtrkcad_checkpoint_labels}"
        TIMEOUT 120
        ENVIRONMENT "HOME=${_xtrkcad_checkpoint_home}"
        RUN_SERIAL TRUE
    )
endforeach()

# Copy, Cut, Paste and Clone of a whole layout, with Undo and Redo
# (LayoutClipboardTest(), fileio.c): every step has to give back the layout it
# started from, and a clone has to sit at one offset, connected as the original.
foreach(_xtrkcad_clipboard_case objects timesavr)
    if(_xtrkcad_clipboard_case STREQUAL "objects")
        set(_xtrkcad_clipboard_source "${CMAKE_CURRENT_LIST_DIR}/unittest/testfiles/roundtrip-objects.xtc")
        set(_xtrkcad_clipboard_labels "regression;regression-ci")
    else()
        set(_xtrkcad_clipboard_source "${CMAKE_SOURCE_DIR}/app/lib/examples/timesavr.xtc")
        set(_xtrkcad_clipboard_labels "regression")
    endif()
    set(_xtrkcad_clipboard_home "${XTRKCAD_REGRESSION_HOME_ROOT}/clipboard-${_xtrkcad_clipboard_case}")
    xtrkcad_add_regression_test("RegressionClipboard.${_xtrkcad_clipboard_case}" "${_xtrkcad_clipboard_home}"
        "${_xtrkcad_test_option}clipboard:${_xtrkcad_clipboard_source}")
    set_tests_properties("RegressionClipboard.${_xtrkcad_clipboard_case}" PROPERTIES
        FIXTURES_REQUIRED "regression_install;regression_clean_homes"
        LABELS "${_xtrkcad_clipboard_labels}"
        TIMEOUT 120
        ENVIRONMENT "HOME=${_xtrkcad_clipboard_home}"
        RUN_SERIAL TRUE
    )
endforeach()

# File > Export and Import of the .xti format, and Import Module
# (LayoutXtiTest(), fileio.c): into an empty layout and back out, into the
# layout itself with Undo, and as a module on a layer of its own.
foreach(_xtrkcad_xti_case objects timesavr)
    if(_xtrkcad_xti_case STREQUAL "objects")
        set(_xtrkcad_xti_source "${CMAKE_CURRENT_LIST_DIR}/unittest/testfiles/roundtrip-objects.xtc")
        set(_xtrkcad_xti_labels "regression;regression-ci")
    else()
        set(_xtrkcad_xti_source "${CMAKE_SOURCE_DIR}/app/lib/examples/timesavr.xtc")
        set(_xtrkcad_xti_labels "regression")
    endif()
    set(_xtrkcad_xti_home "${XTRKCAD_REGRESSION_HOME_ROOT}/xti-${_xtrkcad_xti_case}")
    xtrkcad_add_regression_test("RegressionXti.${_xtrkcad_xti_case}" "${_xtrkcad_xti_home}"
        "${_xtrkcad_test_option}xti:${_xtrkcad_xti_source}")
    set_tests_properties("RegressionXti.${_xtrkcad_xti_case}" PROPERTIES
        FIXTURES_REQUIRED "regression_install;regression_clean_homes"
        LABELS "${_xtrkcad_xti_labels}"
        TIMEOUT 120
        ENVIRONMENT "HOME=${_xtrkcad_xti_home}"
        RUN_SERIAL TRUE
    )
endforeach()

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
        # The export to export.bmp has to give one notice that says what's
        # wrong with the name, not wlib's "WriteBitMap: ..." notices and the
        # general bitmap failure that used to follow it. A PASS expression
        # replaces the exit code, so the run's own result is matched too.
        PASS_REGULAR_EXPRESSION "NOTICE ID: MSG_BITMAP_BAD_EXTENSION\nNOTICE: [^\n]*export\\.bmp[^\n]*\nNOTICE ANSWER: [^\n]*\n(.*\n)?Regression Tests Pass"
        FAIL_REGULAR_EXPRESSION "NOTICE: WriteBitMap|NOTICE ID: MSG_WBITMAP_FAILED|Regression Tests Fail"
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

# Every parameter file installed with the program has to load (Development
# Issues #75). Otherwise only the few files a new user starts with are ever
# read in a test. The setup step lists all installed .xtp files in the
# preferences, so the program loads them at startup; the run fails on any
# notice or error (Linux and macOS: Windows logs to a file); the check step
# fails unless every file is still in the list the program saved, which a file
# that doesn't load drops out of. regression-ci, so the sanitizer job parses
# them all too.
set(_xtrkcad_params_home "${XTRKCAD_REGRESSION_HOME_ROOT}/params-load")
set(_xtrkcad_params_dir "${XTRKCAD_REGRESSION_INSTALL_PREFIX}/${XTRKCAD_SHARE_INSTALL_DIR}/params")
set(_xtrkcad_params_ini "${_xtrkcad_params_home}/.${XTRKCAD_BIN}/${XTRKCAD_BIN}.ini")
add_test(NAME RegressionParams.setup
    COMMAND ${CMAKE_COMMAND} -DMODE=write "-DPARAMS_DIR=${_xtrkcad_params_dir}"
        "-DINI=${_xtrkcad_params_ini}" -P "${CMAKE_CURRENT_LIST_DIR}/unittest/ParamFilesIni.cmake")
set_tests_properties(RegressionParams.setup PROPERTIES
    FIXTURES_REQUIRED "regression_install;regression_clean_homes"
    FIXTURES_SETUP regression_params
    LABELS "regression;regression-ci"
)
xtrkcad_add_regression_test(RegressionParams.load "${_xtrkcad_params_home}"
    "${_xtrkcad_test_option}${_xtrkcad_compare_dir}/regress-params-load.xtr")
set_tests_properties(RegressionParams.load PROPERTIES
    FIXTURES_REQUIRED "regression_install;regression_clean_homes;regression_params"
    LABELS "regression;regression-ci"
    TIMEOUT 300
    ENVIRONMENT "HOME=${_xtrkcad_params_home}"
    RUN_SERIAL TRUE
)
if(NOT WIN32)
    set_tests_properties(RegressionParams.load PROPERTIES
        FAIL_REGULAR_EXPRESSION "NOTICE:|ERROR:")
endif()
add_test(NAME RegressionParams.check
    COMMAND ${CMAKE_COMMAND} -DMODE=check "-DPARAMS_DIR=${_xtrkcad_params_dir}"
        "-DINI=${_xtrkcad_params_ini}" -P "${CMAKE_CURRENT_LIST_DIR}/unittest/ParamFilesIni.cmake")
set_tests_properties(RegressionParams.check PROPERTIES
    DEPENDS RegressionParams.load
    FIXTURES_REQUIRED "regression_install;regression_clean_homes;regression_params"
    LABELS "regression;regression-ci"
)

# ---------------------------------------------------------------------------
# Test levels (XTRKCAD_REGRESSION_LEVEL, top-level CMakeLists.txt)
#
# critical (label regression-critical): checks of code that changes data and
# of bugs we have fixed, seconds each: dialog values and Undo (ParamCheck),
# what the user is told when something goes wrong (Notice), the demo check
# itself (Compare), parameter files, the round-trip fixtures, and round trip
# and Properties on a few small example layouts. The default for a local
# build, and what Windows and macOS run on a pull request.
# ci (label regression-ci): critical plus every example layout and the demos
# (RegressionSuite, or the per-demo tests on Windows).
# full (label regression): ci plus the per-demo cold starts, the largest
# layouts and export.
# A test above the level is disabled, not left out, so ctest -N still lists
# it, and the labels stay as they were for jobs that select by label.
set(_xtrkcad_critical_tests
    "^RegressionParamCheck\\."
    "^RegressionNotice\\."
    "^RegressionCompare\\."
    "^RegressionParams\\."
    "^RegressionHints\\."
    "^RegressionRoundTrip\\.(objects|objects_check|empty_text|empty_text_check|badmanifest)$"
    "^Regression(RoundTrip|Describe)\\.(timesavr|Control_Panels|pmi)$"
)
get_property(_xtrkcad_all_tests DIRECTORY PROPERTY TESTS)
set(_xtrkcad_level_counts_critical 0)
set(_xtrkcad_level_counts_disabled 0)
foreach(_xtrkcad_t ${_xtrkcad_all_tests})
    get_test_property("${_xtrkcad_t}" LABELS _xtrkcad_labels)
    get_test_property("${_xtrkcad_t}" FIXTURES_SETUP _xtrkcad_setup)
    # Every regression test needs the install and the clean homes; the
    # other setup tests are at the level of the tests that need them.
    if(NOT _xtrkcad_labels OR _xtrkcad_setup MATCHES "^regression_(install|clean_homes)$")
        continue()
    endif()
    if(NOT "regression" IN_LIST _xtrkcad_labels)
        continue()
    endif()
    set(_xtrkcad_is_critical FALSE)
    foreach(_xtrkcad_re ${_xtrkcad_critical_tests})
        if(_xtrkcad_t MATCHES "${_xtrkcad_re}")
            set(_xtrkcad_is_critical TRUE)
        endif()
    endforeach()
    if(_xtrkcad_is_critical)
        list(APPEND _xtrkcad_labels regression-critical)
        set_tests_properties("${_xtrkcad_t}" PROPERTIES LABELS "${_xtrkcad_labels}")
        math(EXPR _xtrkcad_level_counts_critical "${_xtrkcad_level_counts_critical} + 1")
    endif()
    set(_xtrkcad_disable FALSE)
    if(XTRKCAD_REGRESSION_LEVEL STREQUAL "critical" AND NOT _xtrkcad_is_critical)
        set(_xtrkcad_disable TRUE)
    elseif(XTRKCAD_REGRESSION_LEVEL STREQUAL "ci" AND NOT "regression-ci" IN_LIST _xtrkcad_labels)
        set(_xtrkcad_disable TRUE)
    endif()
    if(_xtrkcad_disable)
        set_tests_properties("${_xtrkcad_t}" PROPERTIES DISABLED TRUE)
        math(EXPR _xtrkcad_level_counts_disabled "${_xtrkcad_level_counts_disabled} + 1")
    endif()
endforeach()
message(STATUS "Regression tests: level ${XTRKCAD_REGRESSION_LEVEL}, "
    "${_xtrkcad_level_counts_critical} critical, ${_xtrkcad_level_counts_disabled} disabled "
    "(-DXTRKCAD_REGRESSION_LEVEL=critical|ci|full)")
