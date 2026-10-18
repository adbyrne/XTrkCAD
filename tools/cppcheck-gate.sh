#!/usr/bin/env bash
# The gating cppcheck run: cppcheck's default check set, failing on any
# finding. Shared by ci-gtk3.yml's cppcheck job (the gate, on ubuntu-24.04)
# and ci-gtk3-ubuntu2604.yml's preview of the same check on a newer cppcheck,
# so both always use the same scope and suppression list. See the comment on
# ci-gtk3.yml's cppcheck job for why each scope entry and suppression exists.
#
# Usage: tools/cppcheck-gate.sh   (from the repository root)

set -euo pipefail

cppcheck --version

cppcheck \
	--error-exitcode=1 \
	--inline-suppr \
	-j 4 \
	--suppress=missingInclude \
	--suppress=missingIncludeSystem \
	--suppress=nullPointerOutOfMemory \
	--suppress=nullPointerOutOfResources \
	--suppress=nullPointerArithmeticOutOfMemory \
	--suppress=syntaxError:app/bin/archive.c \
	--suppress=syntaxError:app/bin/cturntbl.c \
	--suppress=syntaxError:app/bin/directory.c \
	--suppress=syntaxError:app/bin/paths.c \
	--suppress=syntaxError:app/bin/utility.c \
	--suppress=unknownMacro:app/bin/custom.c \
	--suppress=unknownMacro:app/wlib/gtk3lib/stickytogglebutton.h \
	--suppress=ctunullpointer:app/bin/track.c \
	--suppress=uninitvar:app/bin/levenshtein.c \
	--suppress=uninitvar:app/bin/problemrep.c \
	--suppress=uninitvar:app/bin/paths.c \
	--suppress=nullPointer:app/bin/partcatalog.c \
	--suppress=missingReturn:app/bin/cturnout.c \
	--suppress=ctuPointerArith:app/bin/cturnout.c \
	-I app/bin -I app/bin/include -I app/wlib/include -I app/wlib/gtk3lib \
	app/bin/*.c app/dynstring/*.c \
	app/help/*.c app/tools/listxtp.c \
	app/wlib/gtk3lib/*.c app/wlib/gtk3lib/unix/*.c
