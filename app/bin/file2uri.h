/** \file file2uri.h
 * Include file for file2uri.c
 */

 /*  XTrackCAD - Model Railroad CAD
  *  Copyright (C) 2019 Martin Fischer
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
  *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
  */
  
#ifndef HAVE_FILE2URI_H
unsigned File2URI(char *fileName, unsigned resultLen, char *resultBuffer);
unsigned URI2File(char *encodedFileName, unsigned resultLen, char *resultBuffer);
#endif // !HAVE_FILE2URI_H

