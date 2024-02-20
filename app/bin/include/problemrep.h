/** \file problemrep.h
 * Collect data for a problem report and remove private info
*/

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2024 Martin Fischer
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

#ifndef HAVE_PROBLEMREP_H
#define HAVE_PROBLEMREP_H

static void SaveSystemInfo(char* dir);
static unsigned CollectFiles(char* dest);
static void RemovePrivateData(char* dir);
static void ZipProblemData(char* dest, char* src);
static void ProblemDataCollect();
void DoProblemCollect(void* unused);

#endif // !HAVE_PROBLEMREP_H

