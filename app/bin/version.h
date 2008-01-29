/* $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/version.h,v 1.9 2008-01-29 04:10:23 tshead Exp $
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis
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

#ifdef XTRKCAD_CMAKE_BUILD

	#include "xtrkcad-config.h"

	#define VERSION XTRKCAD_VERSION
	#define PARAMVERSION XTRKCAD_PARAMVERSION
	#define PARAMVERSIONVERSION XTRKCAD_PARAMVERSIONVERSION
	#define MINPARAMVERSION XTRKCAD_MINPARAMVERSION

#else

	#define VERSION "4.1.0b1"
	#define PARAMVERSION (10)
	#define PARAMVERSIONVERSION "3.0.0"
	#define MINPARAMVERSION (1)

#endif

