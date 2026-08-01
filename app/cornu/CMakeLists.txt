project(cornu C)
	
add_library(xtrkcad-cornu "")

target_sources(xtrkcad-cornu
	PRIVATE
		bezctx.c
		bezctx_xtrkcad.c
		spiroentrypoints.c
		spiro.c
		bezctx.h
		bezctx_intf.h
		zmisc.h	
)

target_sources(xtrkcad-cornu
	PUBLIC
		bezctx_xtrkcad.h
		spiroentrypoints.h
		spiro.h
)

target_link_libraries(xtrkcad-cornu
	PRIVATE
		xtrkcad-wlib
		xtrkcad-lib
)

target_include_directories(xtrkcad-cornu PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
	# spiro.c's Newton-Raphson curve fit (spiro_iter) is sensitive to the
	# rounding of individual multiply/add steps. AArch64's base ISA always
	# has hardware FMA, so GCC/Clang's default -ffp-contract=fast silently
	# fuses multiply-adds there (unlike x86-64 baseline, which lacks FMA
	# unless -mfma is explicit) -- confirmed via GTK3 Issues Tracker #28:
	# GCC 13 on arm64 Release fails RegressionSuite's dmjnss.xtr cornu-join
	# angle check (~0.1 degree off) while GCC 12 on the same CPU arch and
	# GCC 13 on x86-64 both pass. Force strict (non-contracted) FP so the
	# solver's rounding, and therefore the fitted curve, is identical across
	# compilers/architectures/optimization levels.
	target_compile_options(xtrkcad-cornu PRIVATE -ffp-contract=off)
endif()
