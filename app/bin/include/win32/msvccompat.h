/** \file msvccompat.h
 * GCC/POSIX compatibility for the MSVC (gvsbuild) build.
 *
 * Force-included into every C translation unit with /FI by
 * PlatformSettings.cmake, so no source file includes it directly.
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

#ifdef _MSC_VER

/* __attribute__((fallthrough)) and friends are GCC/Clang syntax that cl.exe
 * rejects; clang-cl understands them, so keep them there. */
#ifndef __clang__
#define __attribute__(x)
#endif

/* <strings.h> doesn't exist; these match the aliases in common.h */
#define strcasecmp _stricmp
#define strncasecmp _strnicmp

/* same arguments, Microsoft's name */
#define strtok_r strtok_s

#endif /* _MSC_VER */

#endif /* HAVE_MSVCCOMPAT_H */
