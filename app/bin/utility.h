/** \file utility.h
 * Prototypes for misc utility functions
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

#ifndef UTILITY_H
#define UTILITY_H

#include "common.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define EPSILON (0.000001)

#ifdef small
#undef small
#endif
#define small(r) (r < EPSILON && r > -EPSILON)

extern DEBUGF_T debugIntersection;

#ifndef max
double max( double a, double b );
double min( double a, double b );
#endif
double FindDistance( coOrd p0, coOrd p1 );
double NormalizeAngle( double a );
int IsAligned( double a1, double a2 );
double D2R( double D );
double R2D( double R );
void Rotate( coOrd *p, coOrd orig, double angle );
void Translate( coOrd *res, coOrd orig, double a, double d );
double FindAngle( coOrd p0, coOrd p1 );
int PointOnCircle( coOrd * resP, coOrd center, double radius, double angle );
double ConstrainR( double r );
void FindPos( coOrd * res, double * beyond, coOrd pos, coOrd orig, double angle, double length );
int FindIntersection( coOrd *Pc, coOrd P00, double A0, coOrd P10, double A1 );
double LineDistance( coOrd *p, coOrd p0, coOrd p1 );
double CircleDistance( coOrd *p, coOrd c, double r, double a0, double a1 );
int PickArcEndPt( coOrd, coOrd, coOrd );
int PickLineEndPt( coOrd, double, coOrd );
coOrd AddCoOrd( coOrd, coOrd, double );
int ClipLine( coOrd *, coOrd *, coOrd, double, coOrd );

#endif
