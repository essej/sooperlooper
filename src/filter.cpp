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

// this code adapted from richard furse's CMT filter plugin

#include <cmath>

#include "filter.hpp"

using namespace SooperLooper;
using namespace std;

OnePoleFilter::OnePoleFilter(float srate)
	: fLastCutoff(0), fAmountOfCurrent(0), fAmountOfLast(0)
{
	set_samplerate (srate);
	
}
void
OnePoleFilter::run_lowpass (float * buf, uint32_t nframes)
{
	float * pbuf = buf;
	
	if (fLastCutoff != fCurrCutoff) {
		fLastCutoff = fCurrCutoff;
		if (fLastCutoff <= 0) {
			/* Reject everything. */
			fAmountOfCurrent = fAmountOfLast = 0;
		}
		else if (fLastCutoff > fSampleRate * 0.5) {
			/* Above Nyquist frequency. Let everything through. */
			fAmountOfCurrent = 1;
			fAmountOfLast = 0;
		}
		else {
			fAmountOfLast = 0;
			float fComp = 2 - cos(fTwoPiOverSampleRate
					      * fLastCutoff);
			fAmountOfLast = fComp - (float)sqrt(fComp * fComp - 1);
			fAmountOfCurrent = 1 - fAmountOfLast;
		}
	}
	
	float tfAmountOfCurrent = fAmountOfCurrent;
	float tfAmountOfLast = fAmountOfLast;
	float tfLastOutput = fLastOutput;
	
	for (uint32_t n = 0; n < nframes; ++n) {
		*(pbuf)	= tfLastOutput
			= (tfAmountOfCurrent * *(pbuf)
			   + tfAmountOfLast * tfLastOutput);

		++pbuf;
	}
	
	fLastOutput = tfLastOutput;
	
}

void
OnePoleFilter::run_highpass (float * buf, uint32_t nframes)
{

}
