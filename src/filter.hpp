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

#ifndef __sooperlooper_filter_h__
#define __sooperlooper_filter_h__

#include <inttypes.h>

namespace SooperLooper {
	

class OnePoleFilter  {
protected:

  float fSampleRate;
  float fTwoPiOverSampleRate;

  float fLastOutput;
  float fLastCutoff;
  float fCurrCutoff;
  float fAmountOfCurrent;
  float fAmountOfLast;

public:
	OnePoleFilter(float srate);
	virtual ~OnePoleFilter() {}

	void run_lowpass (float * buf, uint32_t nframes);
	void run_highpass (float * buf, uint32_t nframes);

	void set_cutoff (float rate) {
		fCurrCutoff = rate;
	}

	float get_cutoff () { return fCurrCutoff; }

	float get_samplerate () { return fSampleRate; }
	void set_samplerate (float rate)
		{
			fSampleRate = rate;
			fTwoPiOverSampleRate = float(2 * M_PI) / fSampleRate;
		}
};



};


#endif
