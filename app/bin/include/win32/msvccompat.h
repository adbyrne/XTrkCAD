/** \file msvccompat.h
 * GCC/POSIX compatibility for Windows builds, mainly MSVC (gvsbuild).
 *
 * Under MSVC, PlatformSettings.cmake force-includes it into every C
 * translation unit with /FI. common.h includes it on every Windows build,
 * so a source file only includes it directly if it uses strcasecmp() on
 * Windows without common.h. It is the one place these aliases are defined.
 */

/*  XTrkCad - Model Railroad CAD
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef HAVE_MSVCCOMPAT_H
#define HAVE_MSVCCOMPAT_H

#ifdef _WIN32

/* <strings.h> doesn't exist under MSVC; MinGW has it, but common.h has
 * always used the CRT names on every Windows build */
#define strcasecmp _stricmp
#define strncasecmp _strnicmp

#endif /* _WIN32 */

#ifdef _MSC_VER

/* __attribute__((fallthrough)) and friends are GCC/Clang syntax that cl.exe
 * rejects; clang-cl understands them, so keep them there. */
#ifndef __clang__
#define __attribute__(x)
#endif

/* same arguments, Microsoft's name */
#define strtok_r strtok_s

#endif /* _MSC_VER */

#endif /* HAVE_MSVCCOMPAT_H */
