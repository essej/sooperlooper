/*
    Copyright (C) 2004 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#include <cmath>
#include <cerrno>
#include <fstream>
#include <cstdlib>
#include <string>
#include <cstdio>
#include <locale.h>
#include <unistd.h>
#include <float.h>

#include <pbd/error.h>
#include <pbd/failed_constructor.h>
#include <pbd/basename.h>

#include "panner.hpp"
//#include <ardour/utils.h>

using namespace std;
using namespace SooperLooper;


string EqualPowerStereoPanner::name = "Equal Power Stereo";
string Multi2dPanner::name = "Multiple (2D)";


StreamPanner::StreamPanner (Panner& p)
	: parent (p)

{
	_muted = false;

	x = 0.5;
	y = 0.5;
	z = 0.5;
}

StreamPanner::~StreamPanner ()
{
}


void
StreamPanner::set_muted (bool yn)
{
	if (yn != _muted) {
		_muted = yn;
		StateChanged ();
	}
}

void
StreamPanner::set_position (float xpos, bool link_call)
{
	if (!link_call && parent.linked()) {
		parent.set_position (xpos, *this);
	}

	if (x != xpos) {
		x = xpos;
		update ();
		Changed ();

	}
}

void
StreamPanner::set_position (float xpos, float ypos, bool link_call)
{
	if (!link_call && parent.linked()) {
		parent.set_position (xpos, ypos, *this);
	}

	if (x != xpos || y != ypos) {
		
		x = xpos;
		y = ypos;
		update ();
		Changed ();
	}
}

void
StreamPanner::set_position (float xpos, float ypos, float zpos, bool link_call)
{
	if (!link_call && parent.linked()) {
		parent.set_position (xpos, ypos, zpos, *this);
	}

	if (x != xpos || y != ypos || z != zpos) {
		x = xpos;
		y = ypos;
		z = zpos;
		update ();
		Changed ();
	}
}


/*---------------------------------------------------------------------- */

BaseStereoPanner::BaseStereoPanner (Panner& p)
	: StreamPanner (p)
{
}

BaseStereoPanner::~BaseStereoPanner ()
{
}

void
BaseStereoPanner::distribute (sample_t* src, sample_t** obufs, gain_t gain_coeff, nframes_t nframes)
{
	pan_t delta;
	sample_t* dst;
	pan_t pan;

	if (_muted) {
		return;
	}

	/* LEFT */

	dst = obufs[0];

	if (fabsf ((delta = (left - desired_left))) > 0.002) { // about 1 degree of arc 
		
		/* interpolate over 64 frames or nframes, whichever is smaller */
		
		nframes_t limit = min ((nframes_t)64, nframes);
		nframes_t n;

		delta = -(delta / (float) (limit));
		
		for (n = 0; n < limit; n++) {
			left_interp = left_interp + delta;
			left = left_interp + 0.9 * (left - left_interp);
			dst[n] += src[n] * left * gain_coeff;
		}
		
		pan = left * gain_coeff;
		
		for (; n < nframes; ++n) {
			dst[n] += src[n] * pan;
		}
		
	} else {
		
		left = desired_left;
		left_interp = left;

		if ((pan = (left * gain_coeff)) != 1.0f) {
			
			if (pan != 0.0f) {
				
				for (nframes_t n = 0; n < nframes; ++n) {
					dst[n] += src[n] * pan;
				}      
				
				/* mark that we wrote into the buffer */

				// obufs[0] = 0;

			} 
			
		} else {
			
			for (nframes_t n = 0; n < nframes; ++n) {
				dst[n] += src[n];
			}      
			/* mark that we wrote into the buffer */
			
			// obufs[0] = 0;
		}
	}

	/* RIGHT */

	dst = obufs[1];
	
	if (fabsf ((delta = (right - desired_right))) > 0.002) { // about 1 degree of arc 
		
		/* interpolate over 64 frames or nframes, whichever is smaller */
		
		nframes_t limit = min ((nframes_t)64, nframes);
		nframes_t n;

		delta = -(delta / (float) (limit));

		for (n = 0; n < limit; n++) {
			right_interp = right_interp + delta;
			right = right_interp + 0.9 * (right - right_interp);
			dst[n] += src[n] * right * gain_coeff;
		}
		
		pan = right * gain_coeff;
		
		for (; n < nframes; ++n) {
			dst[n] += src[n] * pan;
		}
		
		/* XXX it would be nice to mark the buffer as written to */

	} else {

		right = desired_right;
		right_interp = right;
		
		if ((pan = (right * gain_coeff)) != 1.0f) {
			
			if (pan != 0.0f) {
				
				for (nframes_t n = 0; n < nframes; ++n) {
					dst[n] += src[n] * pan;
				}      
				
				/* XXX it would be nice to mark the buffer as written to */
			}
			
		} else {
			
			for (nframes_t n = 0; n < nframes; ++n) {
				dst[n] += src[n];
			}      

			/* XXX it would be nice to mark the buffer as written to */
		}
	}
}

/*---------------------------------------------------------------------- */

EqualPowerStereoPanner::EqualPowerStereoPanner (Panner& p)
	: BaseStereoPanner (p)
{
	update ();

	left = desired_left;
	right = desired_right;
	left_interp = left;
	right_interp = right;
}

EqualPowerStereoPanner::~EqualPowerStereoPanner ()
{
}

void
EqualPowerStereoPanner::update ()
{
	/* it would be very nice to split this out into a virtual function
	   that can be accessed from BaseStereoPanner and used in distribute_automated().
	   
	   but the place where its used in distribute_automated() is a tight inner loop,
	   and making "nframes" virtual function calls to compute values is an absurd
	   overhead.
	*/

	const float pan_law_attenuation = -3.0f;
	const float scale = 2.0f - 4.0f * powf (10.0f,pan_law_attenuation/20.0f);

	/* x == 0 => hard left
	   x == 1 => hard right
	*/

	float panR = x;
	float panL = 1 - panR;
	
	desired_left = panL * (scale * panL + 1.0f - scale);
	desired_right = panR * (scale * panR + 1.0f - scale);

	effective_x = x;
}

StreamPanner*
EqualPowerStereoPanner::factory (Panner& parent)
{
	return new EqualPowerStereoPanner (parent);
}



/*----------------------------------------------------------------------*/

Multi2dPanner::Multi2dPanner (Panner& p)
	: StreamPanner (p)
{
	update ();
}

Multi2dPanner::~Multi2dPanner ()
{
}


void
Multi2dPanner::update ()
{
	static const float BIAS = FLT_MIN;
	uint32_t i;
	uint32_t nouts = parent.outputs.size();
	float dsq[nouts];
	float f, fr;
	vector<pan_t> pans;

	f = 0.0f;

	for (i = 0; i < nouts; i++) {
		dsq[i] = ((x - parent.outputs[i].x) * (x - parent.outputs[i].x) + (y - parent.outputs[i].y) * (y - parent.outputs[i].y) + BIAS);
		if (dsq[i] < 0.0) {
			dsq[i] = 0.0;
		}
		f += dsq[i] * dsq[i];
	}
	fr = 1.0f / sqrtf(f);
	
	for (i = 0; i < nouts; ++i) {
		parent.outputs[i].desired_pan = 1.0f - (dsq[i] * fr);
	}

	effective_x = x;
}

void
Multi2dPanner::distribute (sample_t* src, sample_t** obufs, gain_t gain_coeff, nframes_t nframes)
{
	sample_t* dst;
	pan_t pan;
	vector<Panner::Output>::iterator o;
	uint32_t n;

	if (_muted) {
		return;
	}


	for (n = 0, o = parent.outputs.begin(); o != parent.outputs.end(); ++o, ++n) {

		dst = obufs[n];
	
#ifdef CAN_INTERP
		if (fabsf ((delta = (left_interp - desired_left))) > 0.002) { // about 1 degree of arc 
			
			/* interpolate over 64 frames or nframes, whichever is smaller */
			
			nframes_t limit = min ((nframes_t)64, nframes);
			nframes_t n;
			
			delta = -(delta / (float) (limit));
		
			for (n = 0; n < limit; n++) {
				left_interp = left_interp + delta;
				left = left_interp + 0.9 * (left - left_interp);
				dst[n] += src[n] * left * gain_coeff;
			}
			
			pan = left * gain_coeff;
			
			for (; n < nframes; ++n) {
				dst[n] += src[n] * pan;
			}
			
		} else {

#else			
			pan = (*o).desired_pan;
			
			if ((pan *= gain_coeff) != 1.0f) {
				
				if (pan != 0.0f) {
					
					for (nframes_t n = 0; n < nframes; ++n) {
						dst[n] += src[n] * pan;
					}      
					
				} 

				
			} else {
				
				for (nframes_t n = 0; n < nframes; ++n) {
					dst[n] += src[n];
				}      

			}
#endif
#ifdef CAN_INTERP
		}
#endif
	}
	
	return;
}


StreamPanner*
Multi2dPanner::factory (Panner& p)
{
	return new Multi2dPanner (p);
}



/*---------------------------------------------------------------------- */

Panner::Panner (string name)
{
	set_name (name);
	_linked = false;
	_link_direction = SameDirection;
	_bypassed = false;
}

Panner::~Panner ()
{
}

void
Panner::set_linked (bool yn)
{
	if (yn != _linked) {
		_linked = yn;
		LinkStateChanged (); /* EMIT SIGNAL */
	}
}

void
Panner::set_link_direction (LinkDirection ld)
{
	if (ld != _link_direction) {
		_link_direction = ld;
		LinkStateChanged (); /* EMIT SIGNAL */
	}
}

void
Panner::set_name (string str)
{
}


void
Panner::set_bypassed (bool yn)
{
	if (yn != _bypassed) {
		_bypassed = yn;
		StateChanged ();
	}
}


void
Panner::reset (uint32_t nouts, uint32_t npans)
{
	uint32_t n;
	bool changed = false;

	if (nouts < 2 || (nouts == outputs.size() && npans == size())) {
		return;
	} 

	n = size();
	clear ();

	if (n != npans) {
		changed = true;
	}

	n = outputs.size();
	outputs.clear ();

	if (n != nouts) {
		changed = true;
	}

	switch (nouts) {
	case 0:
		break;

	case 1:
		cerr << "programming error:"
		      << "Panner::reset() called with a single output"
		      << endmsg;
		/*NOTREACHED*/
		break;

	case 2:
		/* line */
		outputs.push_back (Output (0, 0));
		outputs.push_back (Output (1.0, 0));

		for (n = 0; n < npans; ++n) {
			push_back (new EqualPowerStereoPanner (*this));
		}
		break;

	case 3: // triangle
		outputs.push_back (Output  (0.5, 0));
		outputs.push_back (Output  (0, 1.0));
		outputs.push_back (Output  (1.0, 1.0));

		for (n = 0; n < npans; ++n) {
			push_back (new Multi2dPanner (*this));
		}

		break; 

	case 4: // square
		outputs.push_back (Output  (0, 0));
		outputs.push_back (Output  (1.0, 0));
		outputs.push_back (Output  (1.0, 1.0));
		outputs.push_back (Output  (0, 1.0));

		for (n = 0; n < npans; ++n) {
			push_back (new Multi2dPanner (*this));
		}

		break;	

	case 5: //square+offcenter center
		outputs.push_back (Output  (0, 0));
		outputs.push_back (Output  (1.0, 0));
		outputs.push_back (Output  (1.0, 1.0));
		outputs.push_back (Output  (0, 1.0));
		outputs.push_back (Output  (0.5, 0.75));

		for (n = 0; n < npans; ++n) {
			push_back (new Multi2dPanner (*this));
		}

		break;

	default:
		/* XXX horrible placement. FIXME */
		for (n = 0; n < nouts; ++n) {
			outputs.push_back (Output (0.1 * n, 0.1 * n));
		}

		for (n = 0; n < npans; ++n) {
			push_back (new Multi2dPanner (*this));
		}

		break;
	}

	for (iterator x = begin(); x != end(); ++x) {
		(*x)->update ();
	}

	/* force hard left/right panning in a common case: 2in/2out 
	*/
	
	if (npans == 2 && outputs.size() == 2) {

		/* Do this only if we changed configuration, or our configuration
		   appears to be the default set up (center).
		*/

		float left;
		float right;

		front()->get_position (left);
		back()->get_position (right);

		if (changed || ((left == 0.5) && (right == 0.5))) {
		
			front()->set_position (0.0);
			
			back()->set_position (1.0);
			
			changed = true;
		}
	}

	if (changed) {
		Changed (); /* EMIT SIGNAL */
	}

	return;
}

void
Panner::remove (uint32_t which)
{
	vector<StreamPanner*>::iterator i;
	for (i = begin(); i != end() && which; ++i, --which);

	if (i != end()) {
		delete *i;
		erase (i);
	}
}

void
Panner::clear ()
{
	for (vector<StreamPanner*>::iterator i = begin(); i != end(); ++i) {
		delete *i;
	}

	vector<StreamPanner*>::clear ();
}


void
Panner::set_position (float xpos, StreamPanner& orig)
{
	float xnow;
	float xdelta ;
	float xnew;

	orig.get_position (xnow);
	xdelta = xpos - xnow;
	
	if (_link_direction == SameDirection) {

		for (vector<StreamPanner*>::iterator i = begin(); i != end(); ++i) {
			if (*i == &orig) {
				(*i)->set_position (xpos, true);
			} else {
				(*i)->get_position (xnow);
				xnew = min (1.0f, xnow + xdelta);
				xnew = max (0.0f, xnew);
				(*i)->set_position (xnew, true);
			}
		}

	} else {

		for (vector<StreamPanner*>::iterator i = begin(); i != end(); ++i) {
			if (*i == &orig) {
				(*i)->set_position (xpos, true);
			} else {
				(*i)->get_position (xnow);
				xnew = min (1.0f, xnow - xdelta);
				xnew = max (0.0f, xnew);
				(*i)->set_position (xnew, true);
			}
		}
	}
}

void
Panner::set_position (float xpos, float ypos, StreamPanner& orig)
{
	float xnow, ynow;
	float xdelta, ydelta;
	float xnew, ynew;

	orig.get_position (xnow, ynow);
	xdelta = xpos - xnow;
	ydelta = ypos - ynow;
	
	if (_link_direction == SameDirection) {

		for (vector<StreamPanner*>::iterator i = begin(); i != end(); ++i) {
			if (*i == &orig) {
				(*i)->set_position (xpos, ypos, true);
			} else {
				(*i)->get_position (xnow, ynow);

				xnew = min (1.0f, xnow + xdelta);
				xnew = max (0.0f, xnew);

				ynew = min (1.0f, ynow + ydelta);
				ynew = max (0.0f, ynew);

				(*i)->set_position (xnew, ynew, true);
			}
		}

	} else {

		for (vector<StreamPanner*>::iterator i = begin(); i != end(); ++i) {
			if (*i == &orig) {
				(*i)->set_position (xpos, ypos, true);
			} else {
				(*i)->get_position (xnow, ynow);
				
				xnew = min (1.0f, xnow - xdelta);
				xnew = max (0.0f, xnew);

				ynew = min (1.0f, ynow - ydelta);
				ynew = max (0.0f, ynew);

				(*i)->set_position (xnew, ynew, true);
			}
		}
	}
}

void
Panner::set_position (float xpos, float ypos, float zpos, StreamPanner& orig)
{
	float xnow, ynow, znow;
	float xdelta, ydelta, zdelta;
	float xnew, ynew, znew;

	orig.get_position (xnow, ynow, znow);
	xdelta = xpos - xnow;
	ydelta = ypos - ynow;
	zdelta = zpos - znow;

	if (_link_direction == SameDirection) {

		for (vector<StreamPanner*>::iterator i = begin(); i != end(); ++i) {
			if (*i == &orig) {
				(*i)->set_position (xpos, ypos, zpos, true);
			} else {
				(*i)->get_position (xnow, ynow, znow);
				
				xnew = min (1.0f, xnow + xdelta);
				xnew = max (0.0f, xnew);

				ynew = min (1.0f, ynow + ydelta);
				ynew = max (0.0f, ynew);

				znew = min (1.0f, znow + zdelta);
				znew = max (0.0f, znew);

				(*i)->set_position (xnew, ynew, znew, true);
			}
		}

	} else {

		for (vector<StreamPanner*>::iterator i = begin(); i != end(); ++i) {
			if (*i == &orig) {
				(*i)->set_position (xpos, ypos, true);
			} else {
				(*i)->get_position (xnow, ynow, znow);

				xnew = min (1.0f, xnow - xdelta);
				xnew = max (0.0f, xnew);

				ynew = min (1.0f, ynow - ydelta);
				ynew = max (0.0f, ynew);

				znew = min (1.0f, znow + zdelta);
				znew = max (0.0f, znew);

				(*i)->set_position (xnew, ynew, znew, true);
			}
		}
	}
}
