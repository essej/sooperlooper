/*
** Copyright (C) 2004 Jesse Chappell <jesse@essej.net>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**  
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**  
*/

#ifndef __sooperlooper_utils_h__
#define __sooperlooper_utils_h__

#include <stdint.h>
#include <cmath>


namespace SooperLooper
{

// from steve harris's ladspa plugin set

/*
// 32.32 fixpoint
typedef union {
	int64_t all;
	struct {
#ifdef WORDS_BIGENDIAN
		int32_t in;
		uint32_t fr;
#else
		uint32_t fr;
		int32_t in;
#endif
	} part;
} fixp32;
*/

/* 32 bit "pointer cast" union */
typedef union {
        float f;
        int32_t i;
} ls_pcast32;
	
static inline float flush_to_zero(float f)
{
	ls_pcast32 v;

	v.f = f;

	// original: return (v.i & 0x7f800000) == 0 ? 0.0f : f;
	// version from Tim Blechmann
	return (v.i & 0x7f800000) < 0x08000000 ? 0.0f : f;
}



/* A set of branchless clipping operations from Laurent de Soras */

static inline float f_max(float x, float a)
{
	x -= a;
	x += fabsf(x);
	x *= 0.5;
	x += a;

	return x;
}

static inline float f_min(float x, float b)
{
	x = b - x;
	x += fabsf(x);
	x *= 0.5;
	x = b - x;

	return x;
}

static inline float f_clamp(float x, float a, float b)
{
	const float x1 = fabsf(x - a);
	const float x2 = fabsf(x - b);

	x = x1 + a + b;
	x -= x2;
	x *= 0.5;

	return x;
}

};


struct LocaleGuard {
	LocaleGuard (const char*);
	~LocaleGuard ();
	const char* old;
};


#define DB_CO(g) ((g) > -90.0f ? pow(10.0, (g) * 0.05) : 0.0)
#define CO_DB(v) (20.0 * log10(v))


#endif
