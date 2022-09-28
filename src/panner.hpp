/*
    Copyright (C) 2004 Paul Davis, Jesse Chappell

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

#ifndef __sooperlooper_panner_h__
#define __sooperlooper_panner_h__

#include <cmath>
#include <vector>
#include <string>
#include <iostream>
#include <sigc++/sigc++.h>

#include "audio_driver.hpp"
#include <pbd/xml++.h>

using std::istream;
using std::ostream;

namespace SooperLooper {

typedef float gain_t;       
typedef float pan_t;       

class Panner;

class StreamPanner : public sigc::trackable
{
  public:
	StreamPanner (Panner& p);
	virtual ~StreamPanner ();

	void set_muted (bool yn);
	bool muted() const { return _muted; }

	void set_position (float x, bool link_call = false);
	void set_position (float x, float y, bool link_call = false);
	void set_position (float x, float y, float z, bool link_call = false);

	void get_position (float& xpos) const { xpos = x; }
	void get_position (float& xpos, float& ypos) const { xpos = x; ypos = y; }
	void get_position (float& xpos, float& ypos, float& zpos) const { xpos = x; ypos = y; zpos = z; }

	void get_effective_position (float& xpos) const { xpos = effective_x; }
	void get_effective_position (float& xpos, float& ypos) const { xpos = effective_x; ypos = effective_y; }
	void get_effective_position (float& xpos, float& ypos, float& zpos) const { xpos = effective_x; ypos = effective_y; zpos = effective_z; }

	/* the basic panner API */

	virtual void distribute (sample_t* src, sample_t** obufs, gain_t gain_coeff, nframes_t nframes) = 0;


	sigc::signal0<void> Changed;      /* for position */
	sigc::signal0<void> StateChanged; /* for mute */

	virtual XMLNode& get_state (void) = 0;
	virtual int      set_state (const XMLNode&);

	Panner & get_parent() { return parent; }
	
  protected:
	friend class Panner;
	Panner& parent;

	float x;
	float y;
	float z;
	
	/* these are for automation. they store the last value
	   used by the most recent process() cycle.
	*/

	float effective_x;
	float effective_y;
	float effective_z;

	bool             _muted;

	void add_state (XMLNode&);
	

	virtual void update () = 0;
};

class BaseStereoPanner : public StreamPanner
{
  public:
	BaseStereoPanner (Panner&);
	~BaseStereoPanner ();

	/* this class just leaves the pan law itself to be defined
	   by the update(), distribute_automated() 
	   methods. derived classes also need a factory method
	   and a type name. See EqualPowerStereoPanner as an example.
	*/

	void distribute (sample_t* src, sample_t** obufs, gain_t gain_coeff, nframes_t nframes);

  protected:
	float left;
	float right;
	float desired_left;
	float desired_right;
	float left_interp;
	float right_interp;
    float last_gain;
};

class EqualPowerStereoPanner : public BaseStereoPanner
{
  public:
	EqualPowerStereoPanner (Panner&);
	~EqualPowerStereoPanner ();

	void get_current_coefficients (pan_t*) const;
	void get_desired_coefficients (pan_t*) const;

	static StreamPanner* factory (Panner&);
	static std::string name;

	XMLNode& get_state (void); 
	int      set_state (const XMLNode&);

  private:
	void update ();
};

class Multi2dPanner : public StreamPanner
{
  public:
	Multi2dPanner (Panner& parent);
	~Multi2dPanner ();


	void distribute (sample_t* src, sample_t** obufs, gain_t gain_coeff, nframes_t nframes);

	static StreamPanner* factory (Panner&);
	static std::string name;

	XMLNode& get_state (void);
	int set_state (const XMLNode&);

  private:
	void update ();
};


class Panner : public sigc::trackable
{
  public:
	struct Output {
	    float x;
	    float y;
	    pan_t current_pan;
	    pan_t desired_pan;

	    Output (float xp, float yp) 
		    : x (xp), y (yp), current_pan (0.0f), desired_pan (0.f) {}
		    
	};

	Panner (std::string name);
	virtual ~Panner ();

	void set_name (std::string);

	bool bypassed() const { return _bypassed; }
	void set_bypassed (bool yn);

	StreamPanner* add ();
	void remove (uint32_t which);
	void clear ();
	void reset (uint32_t noutputs, uint32_t npans);

        size_t size() const { return _panners.size(); }
        StreamPanner * operator[](size_t index) const { return _panners[index]; }
        
	XMLNode& get_state (void);
	XMLNode& state (bool full);
	int      set_state (const XMLNode&);

	
	sigc::signal0<void> Changed;
	
	static bool equivalent (pan_t a, pan_t b) {
		return fabsf (a - b) < 0.002; // about 1 degree of arc for a stereo panner
	}

	void move_output (uint32_t, float x, float y);
	uint32_t nouts() const { return outputs.size(); }
	Output& output (uint32_t n) { return outputs[n]; }

	std::vector<Output> outputs;

	enum LinkDirection {
		SameDirection,
		OppositeDirection
	};

	LinkDirection link_direction() const { return _link_direction; }
	void set_link_direction (LinkDirection);
	
	bool linked() const { return _linked; }
	void set_linked (bool yn);

	sigc::signal0<void> LinkStateChanged;
	sigc::signal0<void> StateChanged; /* for bypass */

	/* only StreamPanner should call these */
	
	void set_position (float x, StreamPanner& orig);
	void set_position (float x, float y, StreamPanner& orig);
	void set_position (float x, float y, float z, StreamPanner& orig);

  private:

        std::vector<StreamPanner*> _panners;
    
	uint32_t     current_outs;
	bool             _linked;
	bool             _bypassed;
	LinkDirection    _link_direction;
};

}; /* namespace ARDOUR */

#endif /*__ardour_panner_h__ */
